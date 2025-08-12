#include "kafka_consumer.h"
#include <rdkafka.h>
#include <iostream>
#include <stdexcept>

struct KafkaConsumer::Impl {
  rd_kafka_conf_t* conf = nullptr;
  rd_kafka_t* rk = nullptr;
  rd_kafka_topic_partition_list_t* topics = nullptr;
};

KafkaConsumer::KafkaConsumer(const KafkaConsumerCfg& cfg) : impl_(new Impl()) {
  char errstr[512];

  impl_->conf = rd_kafka_conf_new();
  if (rd_kafka_conf_set(impl_->conf, "bootstrap.servers", cfg.bootstrap_servers.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    throw std::runtime_error(errstr);
  if (rd_kafka_conf_set(impl_->conf, "group.id", cfg.group_id.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    throw std::runtime_error(errstr);
  if (rd_kafka_conf_set(impl_->conf, "enable.partition.eof", "false", errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    throw std::runtime_error(errstr);
  if (rd_kafka_conf_set(impl_->conf, "auto.offset.reset", cfg.auto_offset_reset.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    throw std::runtime_error(errstr);

  impl_->rk = rd_kafka_new(RD_KAFKA_CONSUMER, impl_->conf, errstr, sizeof(errstr));
  if (!impl_->rk) throw std::runtime_error(std::string("rd_kafka_new consumer failed: ") + errstr);
  rd_kafka_poll_set_consumer(impl_->rk);

  impl_->topics = rd_kafka_topic_partition_list_new(1);
  rd_kafka_topic_partition_list_add(impl_->topics, cfg.topic.c_str(), -1);

  rd_kafka_resp_err_t err = rd_kafka_subscribe(impl_->rk, impl_->topics);
  if (err) throw std::runtime_error(std::string("subscribe failed: ") + rd_kafka_err2str(err));
}

KafkaConsumer::~KafkaConsumer() {
  if (impl_) {
    if (impl_->rk) {
      rd_kafka_consumer_close(impl_->rk);
      rd_kafka_destroy(impl_->rk);
    }
    if (impl_->topics) rd_kafka_topic_partition_list_destroy(impl_->topics);
    delete impl_;
  }
}

bool KafkaConsumer::poll(std::string& key_out, const void*& payload_out, size_t& len_out, int timeout_ms) {
  rd_kafka_message_t* rkmessage = rd_kafka_consumer_poll(impl_->rk, timeout_ms);
  if (!rkmessage) return false;

  bool ok = true;
  if (rkmessage->err) {
    if (rkmessage->err != RD_KAFKA_RESP_ERR__PARTITION_EOF) {
      std::cerr << "[kafka] consumer error: " << rd_kafka_message_errstr(rkmessage) << "\n";
    }
    ok = false;
  } else {
    // key
    if (rkmessage->key && rkmessage->key_len > 0)
      key_out.assign((const char*)rkmessage->key, rkmessage->key_len);
    else
      key_out.clear();
    payload_out = rkmessage->payload;
    len_out = rkmessage->len;
  }

  rd_kafka_message_destroy(rkmessage);
  return ok;
}
