#!/usr/bin/env bash
# Launch the TempControl FastAPI server on the Pi.
# Usage: ./run.sh
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"
exec .venv/bin/uvicorn app.main:app --host 0.0.0.0 --port 8000
