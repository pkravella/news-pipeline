#!/usr/bin/env bash
set -euo pipefail
CH_HOST=${CH_HOST:-localhost}
CH_PORT=${CH_PORT:-8123}

echo "Applying ClickHouse sinks to http://$CH_HOST:$CH_PORT ..."
curl -sS "http://$CH_HOST:$CH_PORT" --data-binary @infra/clickhouse/002_news_kafka.sql
echo
echo "Done."
