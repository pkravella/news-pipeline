#pragma once
#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct CleanResult {
  std::string title;
  std::string body;
  std::string language;        // "en" or "unknown"
  std::vector<std::string> hints; // cashtags, host/path tokens
};

struct HtmlFetchOptions {
  std::string user_agent = "FinNewsBot/1.0";
  int timeout_secs = 10;
};

// Fetches HTML and returns the raw string (std::nullopt on failure)
std::optional<std::string> fetch_html(const std::string& url, const HtmlFetchOptions& opt);

// Extracts a main title/body from HTML using heuristics (no network)
CleanResult clean_html_to_text(const std::string& url, const std::string& html);

// Heuristic English detector: returns "en" if the body is mostly ASCII alphabetic/space/punct
std::string detect_language_en_heuristic(const std::string& text);

// Extract hints: cashtags ($AAPL), host + path tokens (lowercased, simple stopword filter)
std::vector<std::string> extract_hints(const std::string& url, const std::string& text);
