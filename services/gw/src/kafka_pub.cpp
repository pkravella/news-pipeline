#include "kafka_pub.h"
#include <rdkafka.h>
#include <iostream>
#include <cstring>

struct KafkaProducer::Impl {
  rd_kafka_conf_t* conf = nullptr;
  rd_kafka_t* rk = nullptr;
  rd_kafka_topic_t* rkt = nullptr;
  std::string topic;

  static void dr_cb(rd_kafka_t*, const rd_kafka_message_t* rkmessage, void*) {
    if (rkmessage->err) {
      std::cerr << "[kafka] delivery failed: " << rd_kafka_err2str(rkmessage->err) << "\n";
    }
  }
};

KafkaProducer::KafkaProducer(const KafkaConfig& cfg) : impl_(new Impl()) {
  char errstr[512];

  impl_->conf = rd_kafka_conf_new();
  rd_kafka_conf_set_dr_msg_cb(impl_->conf, Impl::dr_cb);

  if (rd_kafka_conf_set(impl_->conf, "bootstrap.servers", cfg.bootstrap_servers.c_str(), errstr, sizeof(errstr)) != RD_KAFKA_CONF_OK)
    throw std::runtime_error(errstr);
  rd_kafka_conf_set(impl_->conf, "acks", cfg.acks.c_str(), errstr, sizeof(errstr));
  {
    std::string v = std::to_string(cfg.linger_ms);
    rd_kafka_conf_set(impl_->conf, "linger.ms", v.c_str(), errstr, sizeof(errstr));
  }
  {
    std::string v = std::to_string(cfg.batch_num_messages);
    rd_kafka_conf_set(impl_->conf, "batch.num.messages", v.c_str(), errstr, sizeof(errstr));
  }

  impl_->rk = rd_kafka_new(RD_KAFKA_PRODUCER, impl_->conf, errstr, sizeof(errstr));
  if (!impl_->rk) throw std::runtime_error(std::string("rd_kafka_new failed: ") + errstr);

  impl_->topic = cfg.topic;
}

KafkaProducer::~KafkaProducer() {
  if (impl_) {
    if (impl_->rk) {
      rd_kafka_flush(impl_->rk, 2000);
      rd_kafka_destroy(impl_->rk);
    }
    delete impl_;
  }
}

bool KafkaProducer::produce(const std::string& key, const void* payload, size_t len) {
  rd_kafka_resp_err_t err = rd_kafka_producev(
      impl_->rk,
      RD_KAFKA_V_TOPIC(impl_->topic.c_str()),
      RD_KAFKA_V_MSGFLAGS(RD_KAFKA_MSG_F_COPY),
      RD_KAFKA_V_VALUE(const_cast<void*>(payload), len),
      RD_KAFKA_V_KEY(const_cast<char*>(key.data()), key.size()),
      RD_KAFKA_V_END);

  if (err) {
    std::cerr << "[kafka] produce failed: " << rd_kafka_err2str(err) << "\n";
    return false;
  }
  return true;
}

void KafkaProducer::flush(int timeout_ms) {
  rd_kafka_flush(impl_->rk, timeout_ms);
}
