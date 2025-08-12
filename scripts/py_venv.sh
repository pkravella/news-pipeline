#!/usr/bin/env bash
set -euo pipefail
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip

# Core pipeline deps
pip install grpcio grpcio-tools onnxruntime torch transformers spacy confluent-kafka clickhouse-connect rapidfuzz prometheus-client

# NLP model
python -m spacy download en_core_web_sm

# Sinks & backtesting deps
pip install redis pyarrow pandas

echo "Python venv ready."
