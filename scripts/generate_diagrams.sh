#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy
#
# Regenerate every docs/mermaid/*.svg from its .mmd source with mermaid-cli.
#
# Usage:
#   scripts/generate_diagrams.sh              # all diagrams
#   scripts/generate_diagrams.sh <name>...    # only some (name without extension)
#
# Environment:
#   MMDC_PUPPETEER_CONFIG  Optional puppeteer JSON config, e.g. a file with
#                          {"args": ["--no-sandbox"]} when Chromium refuses to
#                          start (containers, recent Ubuntu).

set -euo pipefail

# =========================
# Resolve project root
# =========================
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
MERMAID_DIR="$ROOT_DIR/docs/mermaid"
CONFIG="$MERMAID_DIR/mermaid.config.json"

# =========================
# Find mermaid-cli
# =========================
MMDC=$(command -v mmdc || true)

if [ -z "$MMDC" ]; then
    echo "❌ mmdc not found in PATH"
    echo "👉 You can install it with:"
    echo "   npm install -g @mermaid-js/mermaid-cli"
    exit 1
fi

# =========================
# Select diagrams
# =========================
SOURCES=()
if [ "$#" -gt 0 ]; then
    for name in "$@"; do
        SOURCES+=("$MERMAID_DIR/${name%.mmd}.mmd")
    done
else
    for f in "$MERMAID_DIR"/*.mmd; do
        SOURCES+=("$f")
    done
fi

ARGS=(-b transparent -c "$CONFIG")
if [ -n "${MMDC_PUPPETEER_CONFIG:-}" ]; then
    ARGS+=(-p "$MMDC_PUPPETEER_CONFIG")
fi

# =========================
# Generate
# =========================
for src in "${SOURCES[@]}"; do
    if [ ! -f "$src" ]; then
        echo "❌ Not found: $src"
        exit 1
    fi

    out="${src%.mmd}.svg"
    echo "🔄 $(basename "$src") -> $(basename "$out")"
    "$MMDC" -i "$src" -o "$out" "${ARGS[@]}" --quiet
done

echo "✅ Diagrams generated successfully"
