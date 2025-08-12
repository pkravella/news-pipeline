#include "config.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

AppConfig load_app_config(const std::string& path) {
  YAML::Node root = YAML::LoadFile(path);
  AppConfig c;

  auto k = root["kafka"];
  auto r = root["redis"];
  auto i = root["ingest"];
  auto y = root["yahoo"];

  // kafka
  c.kafka.bootstrap_servers   = k["bootstrap_servers"].as<std::string>();
  c.kafka.topic_raw           = k["topic_raw"].as<std::string>();
  c.kafka.acks                = k["acks"].as<std::string>();
  c.kafka.linger_ms           = k["linger_ms"].as<int>();
  c.kafka.batch_num_messages  = k["batch_num_messages"].as<int>();

  // redis
  c.redis.host               = r["host"].as<std::string>();
  c.redis.port               = r["port"].as<int>();
  c.redis.db                 = r["db"].as<int>();
  c.redis.dedup_ttl_seconds  = r["dedup_ttl_seconds"].as<int>();

  // ingest
  c.ingest.interval_secs     = i["interval_secs"].as<int>();
  c.ingest.user_agent        = i["user_agent"].as<std::string>();
  c.ingest.http_timeout_secs = i["http_timeout_secs"].as<int>();

  // yahoo (optional)
  if (y) {
    c.yahoo.enable_rss = y["enable_rss"] ? y["enable_rss"].as<bool>() : false;
    if (y["rss_url_template"]) c.yahoo.rss_url_template = y["rss_url_template"].as<std::string>();
    if (y["tickers"])         c.yahoo.tickers = y["tickers"].as<std::vector<std::string>>();

    c.yahoo.enable_html = y["enable_html"] ? y["enable_html"].as<bool>() : false;
    if (y["html_url_template"]) c.yahoo.html_url_template = y["html_url_template"].as<std::string>();
    if (y["robots_url"])        c.yahoo.robots_url = y["robots_url"].as<std::string>();
    if (y["min_seconds_between_requests"]) c.yahoo.min_seconds_between_requests = y["min_seconds_between_requests"].as<int>();
    if (y["max_links_per_page"])           c.yahoo.max_links_per_page = y["max_links_per_page"].as<int>();
  }

  return c;
}

RssConfig load_rss_config(const std::string& path) {
  YAML::Node root = YAML::LoadFile(path);
  RssConfig rc;
  rc.interval_secs = root["interval_secs"].as<int>();
  rc.user_agent    = root["user_agent"].as<std::string>();
  for (auto& f : root["feeds"]) {
    Feed feed;
    feed.source = f["source"].as<std::string>();
    feed.url    = f["url"].as<std::string>();
    rc.feeds.push_back(std::move(feed));
  }
  return rc;
}
