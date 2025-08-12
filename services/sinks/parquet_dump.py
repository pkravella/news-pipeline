#!/usr/bin/env python3
import os, sys, json, time, pathlib, datetime
from typing import List, Dict, Any
import yaml
from confluent_kafka import Consumer
import pyarrow as pa
import pyarrow.parquet as pq

def load_cfg():
    with open(os.path.join("config","sinks.yml"), "r") as f:
        return yaml.safe_load(f)

def hour_bucket(ts_ms: int) -> datetime.datetime:
    dt = datetime.datetime.utcfromtimestamp(ts_ms / 1000.0)
    return dt.replace(minute=0, second=0, microsecond=0)

def out_dir(root: str, dt: datetime.datetime) -> str:
    return os.path.join(root, f"{dt.year:04d}", f"{dt.month:02d}", f"{dt.day:02d}", f"{dt.hour:02d}")

def ensure_dir(p: str):
    pathlib.Path(p).mkdir(parents=True, exist_ok=True)

def to_record(row: Dict[str, Any]) -> Dict[str, Any]:
    # Normalize record to a fixed schema
    return {
        "id": row.get("id"),
        "source": row.get("source"),
        "url": row.get("url"),
        "title": row.get("title"),
        "tickers": row.get("tickers") or [],
        "sentiment": float(row.get("sentiment") or 0.0),
        "topics": row.get("topics") or [],
        "scored_ts": int(row.get("scored_ts") or 0)
    }

def schema():
    return pa.schema([
        pa.field("id", pa.string()),
        pa.field("source", pa.string()),
        pa.field("url", pa.string()),
        pa.field("title", pa.string()),
        pa.field("tickers", pa.list_(pa.string())),
        pa.field("sentiment", pa.float32()),
        pa.field("topics", pa.list_(pa.string())),
        pa.field("scored_ts", pa.uint64()),
    ])

class HourWriter:
    def __init__(self, root_dir: str, flush_every_n: int):
        self.root_dir = root_dir
        self.flush_every_n = flush_every_n
        self.cur_hour = None
        self.buffer: List[Dict[str, Any]] = []

    def add(self, rec: Dict[str, Any]):
        self.buffer.append(rec)
        if len(self.buffer) >= self.flush_every_n:
            self.flush()

    def set_hour(self, dt: datetime.datetime):
        if self.cur_hour is None:
            self.cur_hour = dt
            return
        if dt != self.cur_hour:
            # roll to new hour
            self.flush()
            self.cur_hour = dt

    def flush(self):
        if not self.buffer or self.cur_hour is None:
            return
        outdir = out_dir(self.root_dir, self.cur_hour)
        ensure_dir(outdir)
        # unique-ish filename
        fname = f"part-{int(time.time()*1000)}.parquet"
        path = os.path.join(outdir, fname)
        table = pa.Table.from_pylist(self.buffer, schema=schema())
        pq.write_table(table, path, compression="zstd")
        print(f"[parquet-sink] wrote {len(self.buffer)} rows -> {path}")
        self.buffer.clear()

def main():
    cfg = load_cfg()
    kc = cfg["kafka"]
    pcfg = cfg["parquet"]

    root = pcfg["root_dir"]
    flush_every_n = int(pcfg.get("flush_every_n", 500))

    consumer = Consumer({
        "bootstrap.servers": kc["bootstrap_servers"],
        "group.id": kc["group_id_parquet"],
        "auto.offset.reset": "latest",
        "enable.auto.commit": True
    })
    consumer.subscribe([kc["topic_scored"]])

    writer = HourWriter(root, flush_every_n)
    print(f"[parquet-sink] Consuming '{kc['topic_scored']}', dumping to {root}/YYYY/MM/DD/HH/*.parquet")

    last_flush_timer = time.time()
    while True:
        msg = consumer.poll(0.2)
        if msg is None:
            # periodic flush every ~60s to ensure files appear even in low volume
            if time.time() - last_flush_timer > 60:
                writer.flush(); last_flush_timer = time.time()
            continue
        if msg.error():
            print(f"[parquet-sink] Kafka error: {msg.error()}", flush=True)
            continue

        try:
            row = json.loads(msg.value())
        except Exception as e:
            print(f"[parquet-sink] Bad JSON: {e}", flush=True)
            continue

        rec = to_record(row)
        hb = hour_bucket(rec["scored_ts"])
        writer.set_hour(hb)
        writer.add(rec)

if __name__ == "__main__":
    main()
