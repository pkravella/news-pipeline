#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct YahooHtmlItem {
  std::string title;
  std::string url;
  int64_t published_ts_ms = 0; // may be approx if not found on page
};

struct YahooHtmlConfig {
  std::string url_template;           // e.g., https://finance.yahoo.com/quote/{TICKER}/news
  std::string robots_url;             // e.g., https://finance.yahoo.com/robots.txt
  std::string user_agent = "FinNewsBot/1.0";
  int http_timeout_secs = 10;
  int min_seconds_between_requests = 10;
  int max_links_per_page = 30;
};

// Returns false if robots.txt indicates the path should not be crawled (basic check)
bool yahoo_html_robots_allows(const YahooHtmlConfig& cfg);

// Expand {TICKER} into actual URL
std::string yahoo_html_url_for(const std::string& ticker, const YahooHtmlConfig& cfg);

// Parse an HTML page and extract candidate news links/titles
std::vector<YahooHtmlItem> yahoo_html_extract_items(const std::string& html, int max_links);

// A simple clock helper
long long now_ms();
