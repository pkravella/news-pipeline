#pragma once
#include <string>
#include <vector>
#include <cstdint>

struct AppKafka {
  std::string bootstrap_servers;
  std::string topic_raw;
  std::string acks;
  int linger_ms;
  int batch_num_messages;
};

struct AppRedis {
  std::string host;
  int port;
  int db;
  int dedup_ttl_seconds;
};

struct AppIngest {
  int interval_secs;
  std::string user_agent;
  int http_timeout_secs;
};

struct Feed {
  std::string source;
  std::string url;
};

struct RssConfig {
  int interval_secs;
  std::string user_agent;
  std::vector<Feed> feeds;
};

struct YahooConfig {
  bool enable_rss = false;
  std::string rss_url_template;
  std::vector<std::string> tickers;

  bool enable_html = false;
  std::string html_url_template;
  std::string robots_url;
  int min_seconds_between_requests = 10;
  int max_links_per_page = 30;
};

struct AppConfig {
  AppKafka kafka;
  AppRedis redis;
  AppIngest ingest;
  YahooConfig yahoo;
};

AppConfig load_app_config(const std::string& path);
RssConfig load_rss_config(const std::string& path);
