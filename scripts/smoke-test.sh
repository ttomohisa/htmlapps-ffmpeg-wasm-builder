#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PROFILE="${1:-video-compressor}"
if ! command -v pwsh >/dev/null 2>&1; then
  echo "pwsh is required for the cross-platform browser smoke test." >&2
  exit 1
fi
pwsh -NoLogo -NoProfile -File "$ROOT/scripts/smoke-test.ps1" -Profile "$PROFILE"
