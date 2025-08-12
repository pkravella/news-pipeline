#pragma once
#include <string>
#include <vector>
#include <optional>

struct KafkaConfig {
  std::string bootstrap_servers = "localhost:9092";
  std::string topic = "news.raw";
  std::string acks = "all";
  int linger_ms = 5;
  int batch_num_messages = 10000;
};

class KafkaProducer {
 public:
  explicit KafkaProducer(const KafkaConfig& cfg);
  ~KafkaProducer();
  bool produce(const std::string& key, const void* payload, size_t len);
  void flush(int timeout_ms);

 private:
  struct Impl;
  Impl* impl_;
};
