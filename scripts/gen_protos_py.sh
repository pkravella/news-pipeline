#!/usr/bin/env bash
set -euo pipefail
source .venv/bin/activate
mkdir -p services/scorer/pb services/entity/pb services/consumer/pb
python -m grpc_tools.protoc \
  -I./proto \
  --python_out=services/scorer/pb \
  --grpc_python_out=services/scorer/pb \
  ./proto/scorer.proto
python -m grpc_tools.protoc \
  -I./proto \
  --python_out=services/entity/pb \
  ./proto/news.proto
python -m grpc_tools.protoc \
  -I./proto \
  --python_out=services/consumer/pb \
  ./proto/news.proto
echo "Generated Python protos into services/{scorer,entity,consumer}/pb"
