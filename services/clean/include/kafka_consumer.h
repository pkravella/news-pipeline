#pragma once
#include <string>
#include <functional>
#include <cstddef>

struct KafkaConsumerCfg {
  std::string bootstrap_servers = "localhost:9092";
  std::string group_id = "news-cleaner";
  std::string topic = "news.raw";
  std::string auto_offset_reset = "latest"; // or "earliest"
};

class KafkaConsumer {
 public:
  explicit KafkaConsumer(const KafkaConsumerCfg& cfg);
  ~KafkaConsumer();
  // Poll one message: returns (ok, key, payload, len). If no message, ok=false.
  bool poll(std::string& key_out, const void*& payload_out, size_t& len_out, int timeout_ms = 100);
 private:
  struct Impl; Impl* impl_;
};
