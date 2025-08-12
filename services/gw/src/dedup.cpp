#include "dedup.h"
#include <hiredis/hiredis.h>
#include <iostream>

bool dedup_setnx(const DedupConfig& cfg, const std::string& key) {
  redisContext* c = redisConnect(cfg.host.c_str(), cfg.port);
  if (!c || c->err) {
    if (c) { std::cerr << "[redis] connect error: " << c->errstr << "\n"; redisFree(c); }
    else { std::cerr << "[redis] connect error (null context)\n"; }
    return true; // fail-open to avoid dropping data
  }

  // optional: SELECT db
  if (cfg.db != 0) {
    redisReply* sel = (redisReply*)redisCommand(c, "SELECT %d", cfg.db);
    if (sel) freeReplyObject(sel);
  }

  redisReply* reply = (redisReply*)redisCommand(
    c, "SET %s 1 NX EX %d", key.c_str(), cfg.ttl_seconds);

  bool ok_new = false;
  if (reply) {
    if (reply->type == REDIS_REPLY_STATUS && reply->str && std::string(reply->str) == "OK") {
      ok_new = true; // set occurred
    } else {
      // nil reply == already exists -> duplicate
      ok_new = false;
    }
    freeReplyObject(reply);
  } else {
    std::cerr << "[redis] command error\n";
    ok_new = true; // fail-open
  }

  redisFree(c);
  return ok_new;
}
