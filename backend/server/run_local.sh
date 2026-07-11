#!/usr/bin/env bash
# ============================================================
# run_local.sh - Start the voting server for local development
#
# Reads credentials from a .env file in this directory if present,
# then launches the server with settings appropriate for plain HTTP.
#
# Usage:
#   chmod +x run_local.sh
#   ./run_local.sh
#
# Minimum .env required:
#   SUPABASE_URL=https://your-project.supabase.co
#   SUPABASE_KEY=your-supabase-anon-key
#   EMBEDDING_ENCRYPTION_KEY=<output of: openssl rand -hex 32>
# ============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ENV_FILE="$SCRIPT_DIR/.env"

# Load .env if it exists
if [[ -f "$ENV_FILE" ]]; then
  echo "[run_local] Loading env from $ENV_FILE"
  set -a
  # shellcheck disable=SC1090
  source "$ENV_FILE"
  set +a
else
  echo "[run_local] No .env file found at $ENV_FILE"
  echo "            Copy .env.example -> .env and fill in your values."
  exit 1
fi

# ── Local-specific overrides ─────────────────────────────────────────────────
# Disable the Secure cookie flag — required for plain http://localhost.
export SESSION_COOKIE_SECURE=0

# Accept requests from the origins a local frontend dev server uses.
# Override by setting ALLOWED_ORIGINS in your .env file.
if [[ -z "${ALLOWED_ORIGINS:-}" ]]; then
  export ALLOWED_ORIGINS="http://localhost,http://localhost:3000,http://127.0.0.1,http://127.0.0.1:3000"
fi 

echo "[run_local] SESSION_COOKIE_SECURE=0  (plain HTTP mode)"
echo "[run_local] ALLOWED_ORIGINS=$ALLOWED_ORIGINS"
echo "[run_local] PORT=${PORT:-8080}"
echo ""

# Build first if binary is missing or sources are newer
BINARY="$SCRIPT_DIR/bin/voting_server"
if [[ ! -f "$BINARY" ]] || \
   find "$SCRIPT_DIR/src" -name "*.cpp" -newer "$BINARY" | grep -q .; then
  echo "[run_local] Building..."
  make -C "$SCRIPT_DIR" all
fi

exec "$BINARY"
