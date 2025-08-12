#include <absl/strings/str_format.h>
#include <fmt/core.h>
#include <nlohmann/json.hpp>
#include <xxhash.h>
#include <chrono>
#include <iostream>

// Protobuf headers generated into build/generated
#include "news.pb.h"
#include "scorer.pb.h"

static inline long long NowMs() {
  using namespace std::chrono;
  return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

int main(int argc, char** argv) {
  fmt::print("[news_gw] GW stub starting...\n");

  // Create a dummy ArticleRaw just to prove proto compiles/links.
  finnews::ArticleRaw raw;
  raw.set_id("demo-id");
  raw.set_source("DemoSource");
  raw.set_url("https://example.com/news");
  raw.set_title("Hello, FinNews");
  raw.set_body("");
  raw.set_published_ts(NowMs());
  raw.set_ingested_ts(NowMs());

  std::string serialized;
  raw.SerializeToString(&serialized);
  fmt::print("[news_gw] Serialized ArticleRaw size={} bytes\n", serialized.size());

  fmt::print("[news_gw] OK. (Real ingestion will be added in Phase 1)\n");
  return 0;
}
