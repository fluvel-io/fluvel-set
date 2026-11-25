#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy
#
# Run clang-tidy on every .cpp file of src/ with the rules of .clang-tidy.
#
# Usage:
#   scripts/run_clang_tidy.sh                 # all files
#   scripts/run_clang_tidy.sh path/to/a.cpp   # only some files
#
# Environment (all optional):
#   QT_PATH      Qt installation (default: $HOME/Qt/6.11.2/gcc_64)
#   CLANG_TIDY   clang-tidy executable (default: the one found in PATH)
#   JOBS         number of parallel jobs (default: number of CPU cores)
#
# The full report is written to build-clang-tidy/clang-tidy.log.
# The exit code is 0 when nothing is reported, 1 otherwise.

set -euo pipefail

#######################################
# PATHS
#######################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${PROJECT_ROOT}/build-clang-tidy"
QT_PATH="${QT_PATH:-$HOME/Qt/6.11.2/gcc_64}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"
JOBS="${JOBS:-$(nproc)}"
LOG_FILE="${BUILD_DIR}/clang-tidy.log"

if ! command -v "${CLANG_TIDY}" >/dev/null 2>&1; then
    echo "❌ clang-tidy not found: ${CLANG_TIDY}"
    echo "👉 Install it, or point CLANG_TIDY to an executable, for example:"
    echo "   CLANG_TIDY=/path/to/clang-tidy scripts/run_clang_tidy.sh"
    exit 1
fi

echo "🔧 Using $(command -v "${CLANG_TIDY}")"

#######################################
# CONFIGURE (idempotent)
#######################################

echo "🧹 Cleaning ${BUILD_DIR}..."
rm -rf "${BUILD_DIR}"

cmake \
    -S "${PROJECT_ROOT}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="${QT_PATH}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DFLUVEL_BUILD_APP=ON \
    -DFLUVEL_BUILD_BIND=ON

#######################################
# SELECT FILES
#######################################

# Only .cpp files: headers are checked through the files that include them
# (see HeaderFilterRegex in .clang-tidy).
FILES=()
if [ "$#" -gt 0 ]; then
    FILES=("$@")
else
    while IFS= read -r -d '' file; do
        FILES+=("${file}")
    done < <(find "${PROJECT_ROOT}/src" -name "*.cpp" -print0 | sort -z)
fi

#######################################
# RUN CLANG-TIDY
#######################################

echo "🔍 Running clang-tidy on ${#FILES[@]} file(s) with ${JOBS} job(s)..."

OUT_DIR="$(mktemp -d)"
trap 'rm -rf "${OUT_DIR}"' EXIT

export CLANG_TIDY BUILD_DIR OUT_DIR

# One output file per source, so parallel runs never mix their messages.
printf '%s\0' "${FILES[@]}" |
    xargs -0 -P "${JOBS}" -I {} bash -c '
        file="$1"
        out="${OUT_DIR}/$(printf "%s" "${file}" | md5sum | cut -d" " -f1).txt"
        "${CLANG_TIDY}" --quiet -p "${BUILD_DIR}" "${file}" > "${out}" 2>&1 || true
    ' _ {}

#######################################
# REPORT
#######################################

cat "${OUT_DIR}"/*.txt > "${LOG_FILE}" 2>/dev/null || : > "${LOG_FILE}"

# grep -c prints 0 and exits with 1 when nothing matches.
WARNINGS=$(grep -c ': warning:' "${LOG_FILE}" || true)
ERRORS=$(grep -c ': error:' "${LOG_FILE}" || true)

echo ""
echo "📄 Report: ${LOG_FILE}"
echo "   ${WARNINGS} warning(s), ${ERRORS} error(s)"

if [ "${WARNINGS}" -gt 0 ] || [ "${ERRORS}" -gt 0 ]; then
    echo ""
    echo "Most frequent checks:"
    grep -o '\[[a-zA-Z0-9.,-]*\]$' "${LOG_FILE}" | sort | uniq -c | sort -rn | head -10
    exit 1
fi

echo "✅ clang-tidy: nothing to report"
