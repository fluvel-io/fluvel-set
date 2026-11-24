#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

# Build the Python bindings in Debug (asserts enabled) and run the tests.
#
# Usage: scripts/test_bindings.sh [pytest arguments]
# Example: scripts/test_bindings.sh -k active_contour -x

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

"${SCRIPT_DIR}/build_bindings.sh"

PYTHONPATH="${PROJECT_ROOT}/build-bindings/python" \
    python3 -m pytest "${PROJECT_ROOT}/tests/python" -q "$@"
