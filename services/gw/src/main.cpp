#include <absl/strings/str_format.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <xxhash.h>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>
#include <string>
#include <unordered_map>

#include "http_fetch.h"
#include "rss_parser.h"
#include "url_norm.h"
#include "dedup.h"
#include "kafka_pub.h"
#include "config.h"
#include "yahoo_html.h"

// Protobuf
#include "news.pb.h"

static volatile std::sig_atomic_t g_stop = 0;
static void handle_sigint(int) { g_stop = 1; }

static long long NowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

static std::string stable_id(const std::string& source, const std::string& normalized_url) {
  std::string key = source;
  key.append("|").append(normalized_url);
  unsigned long long h = XXH64(key.data(), key.size(), 0);
  char buf[32];
  snprintf(buf, sizeof(buf), "%016llx", (unsigned long long)h);
  return std::string(buf);
}

static std::string replace_all(std::string s, const std::string& from, const std::string& to) {
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from.length(), to);
    pos += to.length();
  }
  return s;
}

int main(int argc, char** argv) {
  std::signal(SIGINT, handle_sigint);
  std::signal(SIGTERM, handle_sigint);

  std::string app_cfg_path = "config/app.yml";
  std::string rss_cfg_path = "config/rss.yml";
  if (argc >= 2) app_cfg_path = argv[1];
  if (argc >= 3) rss_cfg_path = argv[2];

  fmt::print("[news_gw] Loading config: app='{}' rss='{}'\n", app_cfg_path, rss_cfg_path);
  AppConfig app = load_app_config(app_cfg_path);
  RssConfig rss = load_rss_config(rss_cfg_path);

  // Optionally append Yahoo Finance RSS feeds per ticker
  std::vector<Feed> yahooFeeds;
  if (app.yahoo.enable_rss && !app.yahoo.rss_url_template.empty() && !app.yahoo.tickers.empty()) {
    for (const auto& t : app.yahoo.tickers) {
      std::string url = replace_all(app.yahoo.rss_url_template, "{TICKER}", t);
      yahooFeeds.push_back(Feed{ "YahooFinanceRSS:" + t, url });
    }
    rss.feeds.insert(rss.feeds.end(), yahooFeeds.begin(), yahooFeeds.end());
    fmt::print("[news_gw] Yahoo RSS enabled: +{} feeds from tickers list\n", yahooFeeds.size());
  }

  // HTTP options
  HttpOptions httpopt;
  httpopt.user_agent  = app.ingest.user_agent.empty() ? rss.user_agent : app.ingest.user_agent;
  httpopt.timeout_secs = app.ingest.http_timeout_secs;

  // Dedup config
  DedupConfig dcfg;
  dcfg.host = app.redis.host;
  dcfg.port = app.redis.port;
  dcfg.db   = app.redis.db;
  dcfg.ttl_seconds = app.redis.dedup_ttl_seconds;

  // Kafka
  KafkaConfig kcfg;
  kcfg.bootstrap_servers   = app.kafka.bootstrap_servers;
  kcfg.topic               = app.kafka.topic_raw;
  kcfg.acks                = app.kafka.acks;
  kcfg.linger_ms           = app.kafka.linger_ms;
  kcfg.batch_num_messages  = app.kafka.batch_num_messages;

  KafkaProducer producer(kcfg);

  // Prepare Yahoo HTML config
  YahooHtmlConfig yhcfg;
  yhcfg.url_template = app.yahoo.html_url_template;
  yhcfg.robots_url = app.yahoo.robots_url;
  yhcfg.user_agent = httpopt.user_agent;
  yhcfg.http_timeout_secs = httpopt.timeout_secs;
  yhcfg.min_seconds_between_requests = app.yahoo.min_seconds_between_requests;
  yhcfg.max_links_per_page = app.yahoo.max_links_per_page;

  bool yahoo_allowed = true;
  if (app.yahoo.enable_html) {
    yahoo_allowed = yahoo_html_robots_allows(yhcfg);
    if (!yahoo_allowed) {
      fmt::print("[news_gw] Yahoo HTML adapter disabled by robots.txt\n");
    } else {
      fmt::print("[news_gw] Yahoo HTML adapter enabled for {} tickers\n", app.yahoo.tickers.size());
    }
  }

  // Track last fetch time so we don't hammer the host
  std::unordered_map<std::string, long long> last_fetch_ms;

  fmt::print("[news_gw] Starting poll loop (every {}s) with {} RSS feeds\n",
             rss.interval_secs, rss.feeds.size());

  while (!g_stop) {
    auto t_start = std::chrono::steady_clock::now();

    // --- RSS path ---
    for (const auto& f : rss.feeds) {
      auto resp = http_get(f.url, httpopt);
      if (!resp || resp->status < 200 || resp->status >= 300) {
        fmt::print("[news_gw] WARN fetch failed source={} url={} status={}\n",
                   f.source, f.url, (resp ? resp->status : -1));
        continue;
      }

      auto items = parse_feed_xml(resp->body);
      fmt::print("[news_gw] {}: parsed {} RSS items\n", f.source, items.size());

      for (auto& it : items) {
        std::string norm = normalize_url(it.link);
        std::string id = stable_id(f.source, norm);
        std::string dedup_key = "dedup:" + id;

        if (!dedup_setnx(dcfg, dedup_key)) continue;

        finnews::ArticleRaw raw;
        raw.set_id(id);
        raw.set_source(f.source);
        raw.set_url(norm);
        raw.set_title(it.title);
        raw.set_body("");
        raw.set_published_ts(it.published_ts_ms);
        raw.set_ingested_ts(NowMs());

        std::string bytes;
        if (!raw.SerializeToString(&bytes)) continue;
        producer.produce(id, bytes.data(), bytes.size());
      }
    }

    // --- Yahoo HTML path ---
    if (app.yahoo.enable_html && yahoo_allowed && !app.yahoo.tickers.empty() &&
        !app.yahoo.html_url_template.empty()) {

      const long long now = NowMs();
      for (const auto& tkr : app.yahoo.tickers) {
        // simple per-ticker rate limit (polite)
        long long last = last_fetch_ms[tkr];
        if (last > 0 && (now - last) < (long long)yhcfg.min_seconds_between_requests * 1000LL) {
          continue;
        }

        const std::string url = yahoo_html_url_for(tkr, yhcfg);
        HttpOptions yopt = httpopt; // reuse UA/timeout
        auto resp = http_get(url, yopt);
        last_fetch_ms[tkr] = NowMs();

        if (!resp || resp->status < 200 || resp->status >= 300) {
          fmt::print("[news_gw] WARN Yahoo HTML fetch failed ticker={} url={} status={}\n",
                     tkr, url, (resp ? resp->status : -1));
          continue;
        }

        auto items = yahoo_html_extract_items(resp->body, yhcfg.max_links_per_page);
        fmt::print("[news_gw] YahooHTML:{}: extracted {} links\n", tkr, items.size());

        const std::string source = "YahooFinanceHTML:" + tkr;
        for (auto& it : items) {
          std::string norm = normalize_url(it.url);
          std::string id = stable_id(source, norm);
          std::string dedup_key = "dedup:" + id;
          if (!dedup_setnx(dcfg, dedup_key)) continue;

          finnews::ArticleRaw raw;
          raw.set_id(id);
          raw.set_source(source);
          raw.set_url(norm);
          raw.set_title(it.title);
          raw.set_body("");
          raw.set_published_ts(it.published_ts_ms > 0 ? it.published_ts_ms : NowMs());
          raw.set_ingested_ts(NowMs());

          std::string bytes;
          if (!raw.SerializeToString(&bytes)) continue;
          producer.produce(id, bytes.data(), bytes.size());
        }
      }
    }

    producer.flush(100);

    // sleep for remainder of interval
    auto t_end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(t_end - t_start).count();
    int to_sleep = std::max(1, rss.interval_secs - (int)elapsed);
    for (int i = 0; i < to_sleep && !g_stop; ++i) std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  fmt::print("[news_gw] Stopping.\n");
  return 0;
}
