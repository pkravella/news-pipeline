#!/usr/bin/env bash
set -euo pipefail
docker compose -f infra/docker-compose.yml up -d
echo "Services starting:
- Kafka (Redpanda): localhost:9092
- ClickHouse: http://localhost:8123
- Redis: localhost:6379
- Prometheus: http://localhost:9090
- Grafana: http://localhost:3000  (admin/admin)"
