#pragma once
#include <string>

struct DedupConfig {
  std::string host = "localhost";
  int port = 6379;
  int db = 0;
  int ttl_seconds = 172800;
};

// returns true if key was newly set (i.e., NOT a duplicate)
bool dedup_setnx(const DedupConfig& cfg, const std::string& key);
