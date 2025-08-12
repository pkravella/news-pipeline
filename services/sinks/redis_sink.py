#!/usr/bin/env python3
import os, sys, json, time
import yaml
import redis
from confluent_kafka import Consumer

def load_cfg():
    with open(os.path.join("config","sinks.yml"), "r") as f:
        return yaml.safe_load(f)

def redis_client(cfg):
    r = redis.Redis(host=cfg["redis"]["host"],
                    port=int(cfg["redis"]["port"]),
                    db=int(cfg["redis"]["db"]))
    # simple ping test
    r.ping()
    return r

def main():
    cfg = load_cfg()
    kc = cfg["kafka"]

    consumer = Consumer({
        "bootstrap.servers": kc["bootstrap_servers"],
        "group.id": kc["group_id_redis"],
        "auto.offset.reset": "latest",
        "enable.auto.commit": True
    })
    consumer.subscribe([kc["topic_scored"]])

    r = redis_client(cfg)
    max_n = int(cfg["redis"]["max_per_ticker"])
    ttl  = int(cfg["redis"]["ttl_seconds"])

    print(f"[redis-sink] Consuming '{kc['topic_scored']}', keeping last {max_n} per ticker...")
    while True:
        msg = consumer.poll(0.2)
        if msg is None:
            continue
        if msg.error():
            print(f"[redis-sink] Kafka error: {msg.error()}", flush=True)
            continue
        try:
            row = json.loads(msg.value())
        except Exception as e:
            print(f"[redis-sink] Bad JSON: {e}", flush=True)
            continue

        tickers = row.get("tickers") or []
        if not isinstance(tickers, list):
            continue

        payload = json.dumps(row, separators=(",",":"))
        for t in tickers:
            key = f"feed:{t}"
            # newest first
            r.lpush(key, payload)
            r.ltrim(key, 0, max_n-1)
            if ttl > 0:
                r.expire(key, ttl)

if __name__ == "__main__":
    main()
