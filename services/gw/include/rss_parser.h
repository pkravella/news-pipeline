#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct FeedItem {
  std::string title;
  std::string link;
  int64_t published_ts_ms = 0; // epoch ms
};

std::vector<FeedItem> parse_feed_xml(const std::string& xml);
