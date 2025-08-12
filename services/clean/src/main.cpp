#include <fmt/core.h>
#include <xxhash.h>
#include <csignal>
#include <string>
#include <vector>

#include "html_clean.h"
#include "kafka_consumer.h"
#include "kafka_pub.h"
#include "config.h"
#include "news.pb.h"

static volatile std::sig_atomic_t g_stop = 0;
static void handle_sigint(int) { g_stop = 1; }

int main(int argc, char** argv) {
  std::signal(SIGINT, handle_sigint);
  std::signal(SIGTERM, handle_sigint);

  std::string app_cfg_path = "config/app.yml";
  if (argc >= 2) app_cfg_path = argv[1];

  fmt::print("[news_clean] Loading config: app='{}'\n", app_cfg_path);
  AppConfig app = load_app_config(app_cfg_path);

  KafkaConsumerCfg ccfg;
  ccfg.bootstrap_servers = app.kafka.bootstrap_servers;
  ccfg.group_id = "news-cleaner";
  ccfg.topic = app.kafka.topic_raw;
  ccfg.auto_offset_reset = "latest";

  KafkaConsumer consumer(ccfg);

  KafkaConfig pcfg;
  pcfg.bootstrap_servers = app.kafka.bootstrap_servers;
  pcfg.topic = app.kafka.topic_clean;
  pcfg.acks = app.kafka.acks;
  pcfg.linger_ms = app.kafka.linger_ms;
  pcfg.batch_num_messages = app.kafka.batch_num_messages;

  KafkaProducer producer(pcfg);

  HtmlFetchOptions fopt;
  fopt.user_agent = app.cleaner.user_agent;
  fopt.timeout_secs = app.cleaner.http_timeout_secs;

  fmt::print("[news_clean] Ready. Consuming '{}' -> producing '{}'\n", app.kafka.topic_raw, app.kafka.topic_clean);

  std::string key;
  const void* payload = nullptr; size_t len = 0;
  int processed = 0;

  while (!g_stop) {
    if (!consumer.poll(key, payload, len, 200)) continue;
    if (!payload || len == 0) continue;

    finnews::ArticleRaw raw;
    if (!raw.ParseFromArray(payload, (int)len)) {
      fmt::print("[news_clean] WARN: failed to parse ArticleRaw\n");
      continue;
    }

    auto html_opt = fetch_html(raw.url(), fopt);
    if (!html_opt) {
      fmt::print("[news_clean] WARN: failed HTML fetch url={}\n", raw.url());
      continue;
    }

    auto r = clean_html_to_text(raw.url(), *html_opt);

    if (app.cleaner.require_english && r.language != "en") {
      // Skip if not English
      continue;
    }
    if ((int)r.body.size() < app.cleaner.min_body_chars) {
      continue;
    }

    finnews::ArticleClean c;
    c.set_id(raw.id());
    c.set_source(raw.source());
    c.set_url(raw.url());
    c.set_title(!r.title.empty() ? r.title : raw.title());
    c.set_body(r.body);
    c.set_published_ts(raw.published_ts());
    c.set_language(r.language);
    for (auto& h : r.hints) c.add_hints(h);

    std::string bytes;
    if (!c.SerializeToString(&bytes)) {
      fmt::print("[news_clean] ERROR: failed to serialize ArticleClean id={}\n", raw.id());
      continue;
    }
    producer.produce(raw.id(), bytes.data(), bytes.size());

    if (++processed % 10 == 0) producer.flush(10);
  }

  fmt::print("[news_clean] Stopping.\n");
  return 0;
}
