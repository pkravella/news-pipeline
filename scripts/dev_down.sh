#!/usr/bin/env bash
set -euo pipefail
docker compose -f infra/docker-compose.yml down -v
echo "Services stopped and volumes removed."
