#!/usr/bin/env python3
import os, sys, time, json, queue
from typing import List, Dict
import yaml
from confluent_kafka import Consumer, Producer
import grpc

# Protos
sys.path.append(os.path.join(os.path.dirname(__file__), "pb"))
import news_pb2
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "scorer", "pb"))
import scorer_pb2, scorer_pb2_grpc

def load_cfg(path: str) -> dict:
    with open(path, "r") as f:
        return yaml.safe_load(f)

class Batch:
    def __init__(self, max_items: int, max_wait_ms: int):
        self.max_items = max_items
        self.max_wait_ms = max_wait_ms
        self.items = []
        self.first_ts = None

    def add(self, item):
        now = time.time()
        if not self.items:
            self.first_ts = now
        self.items.append(item)
        return len(self.items) >= self.max_items

    def should_flush(self):
        if not self.items:
            return False
        return (time.time() - self.first_ts) * 1000.0 >= self.max_wait_ms

    def take(self):
        out = self.items
        self.items = []
        self.first_ts = None
        return out

def main():
    cfg = load_cfg(os.path.join("config","scoring.yml"))

    # Kafka
    kc = cfg["kafka"]
    consumer = Consumer({
        "bootstrap.servers": kc["bootstrap_servers"],
        "group.id": kc["group_id"],
        "auto.offset.reset": "latest",
        "enable.auto.commit": True
    })
    consumer.subscribe([kc["topic_in"]])
    producer = Producer({"bootstrap.servers": kc["bootstrap_servers"]})

    # gRPC
    target = f"{cfg['grpc']['host']}:{cfg['grpc']['port']}"
    channel = grpc.insecure_channel(target)
    stub = scorer_pb2_grpc.ScorerStub(channel)

    # Batch
    bc = Batch(cfg["batch"]["max_items"], cfg["batch"]["max_wait_ms"])

    print(f"[scoring] Ready. Consuming '{kc['topic_in']}' -> producing '{kc['topic_out']}' via gRPC {target}")

    while True:
        msg = consumer.poll(0.01)
        if msg is None:
            # timer flush
            if bc.should_flush():
                flush_batch(bc, stub, producer, kc["topic_out"])
            continue
        if msg.error():
            print(f"[scoring] Kafka error: {msg.error()}", flush=True)
            continue

        enriched = news_pb2.ArticleEnriched()
        try:
            enriched.ParseFromString(msg.value())
        except Exception as e:
            print(f"[scoring] parse error: {e}", flush=True)
            continue

        item = enriched
        if bc.add(item):
            flush_batch(bc, stub, producer, kc["topic_out"])

def flush_batch(bc: Batch, stub, producer, topic_out: str):
    items = bc.take()
    if not items:
        return

    req = scorer_pb2.ScoreRequest(
        id=[x.id for x in items],
        title=[x.title for x in items],
        body=[x.body for x in items]
    )
    resp = stub.BatchScore(req)

    # resp.topics is pipe-joined string per item (from our server)
    now_ms = int(time.time() * 1000)
    for x, s, conf, tpipe in zip(items, resp.sentiment, resp.confidence, resp.topics):
        # merge topics: enriched + model
        base_topics = list(x.topics)
        model_topics = [t for t in (tpipe.split("|") if tpipe else []) if t]
        final_topics = sorted(list({*base_topics, *model_topics}))

        out = {
            "id": x.id,
            "source": x.source,
            "url": x.url,
            "title": x.title,
            "tickers": list(x.tickers),
            "sentiment": float(s),
            "topics": final_topics,
            "scored_ts": now_ms
        }
        producer.produce(topic_out, key=x.id.encode("utf-8"), value=json.dumps(out))
    producer.poll(0)

if __name__ == "__main__":
    main()
