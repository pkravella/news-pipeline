#!/usr/bin/env bash
set -euo pipefail
python3 -m venv .venv
source .venv/bin/activate
pip install --upgrade pip
pip install grpcio grpcio-tools onnxruntime torch transformers spacy confluent-kafka clickhouse-connect rapidfuzz prometheus-client
python -m spacy download en_core_web_sm
echo "Python venv ready."
