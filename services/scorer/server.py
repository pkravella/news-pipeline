#!/usr/bin/env python3
import os, sys, time, threading
from concurrent import futures
from typing import List, Tuple
import yaml
import grpc

# Protos
sys.path.append(os.path.join(os.path.dirname(__file__), "pb"))
import scorer_pb2, scorer_pb2_grpc

from model import SentimentModel, rule_topics

def load_cfg(path: str):
    with open(path, "r") as f:
        return yaml.safe_load(f)

class ScorerServicer(scorer_pb2_grpc.ScorerServicer):
    def __init__(self, cfg):
        mcfg = cfg["model"]
        self.sm = SentimentModel(
            model_name=mcfg["sentiment_model_name"],
            onnx_path=mcfg["onnx_sentiment_path"],
            use_onnx_if_available=bool(mcfg.get("use_onnx_if_available", True))
        )
        self.enable_rule_topics = bool(cfg.get("topics", {}).get("enable_rules", True))

    def BatchScore(self, request, context):
        # Merge title + first ~2000 body chars for sentiment
        texts = [f"{t or ''}\n{(b or '')[:2000]}" for t,b in zip(request.title, request.body)]
        score, conf = self.sm.sentiment_batch(texts)

        if self.enable_rule_topics:
            rule_t = rule_topics(texts)  # List[List[str]]
            # Represent topics as pipe-joined strings to match our planned consumer
            topics_str = ["|".join(ts) if ts else "" for ts in rule_t]
        else:
            topics_str = [""] * len(texts)

        # Convert numpy arrays to plain lists
        sentiment = [float(x) for x in score]
        confidence = [float(x) for x in conf]

        return scorer_pb2.ScoreResponse(
            sentiment=sentiment,
            confidence=confidence,
            topics=topics_str  # one string per item (pipe-separated topic labels)
        )

def serve(cfg_path: str):
    cfg = load_cfg(cfg_path)
    host = cfg["server"]["host"]
    port = cfg["server"]["port"]

    server = grpc.server(futures.ThreadPoolExecutor(max_workers=8))
    scorer_pb2_grpc.add_ScorerServicer_to_server(ScorerServicer(cfg), server)
    server.add_insecure_port(f"{host}:{port}")
    server.start()
    print(f"[scorer] gRPC server listening on {host}:{port}")
    server.wait_for_termination()

if __name__ == "__main__":
    cfg_path = sys.argv[1] if len(sys.argv)>1 else "config/scorer.yml"
    serve(cfg_path)
