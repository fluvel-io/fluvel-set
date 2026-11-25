#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy
#
# Run Clazy (Qt specific static analysis) on every .cpp file of src/.
# The checks are chosen in scripts/clazy.conf.
#
# Usage:
#   scripts/run_clazy.sh                 # all files
#   scripts/run_clazy.sh path/to/a.cpp   # only some files
#
# Environment (all optional):
#   QT_PATH         Qt installation (default: $HOME/Qt/6.11.2/gcc_64)
#   CLAZY           clazy-standalone executable (default: the one of Qt Creator)
#   JOBS            number of parallel jobs (default: number of CPU cores)
#   CLAZY_CHECKS    overrides the checks of scripts/clazy.conf
#
# The compilation database is shared with scripts/run_clang_tidy.sh
# (build-clang-tidy/). It is created here only if it does not exist yet.
# The full report is written to build-clang-tidy/clazy.log.
# The exit code is 0 when nothing is reported, 1 otherwise.

set -euo pipefail

#######################################
# PATHS AND SETTINGS
#######################################

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="${PROJECT_ROOT}/build-clang-tidy"
QT_PATH="${QT_PATH:-$HOME/Qt/6.11.2/gcc_64}"
CLAZY="${CLAZY:-$HOME/Qt/Tools/QtCreator/libexec/qtcreator/clang/bin/clazy-standalone}"
JOBS="${JOBS:-$(nproc)}"
LOG_FILE="${BUILD_DIR}/clazy.log"

# Settings of the project (the environment has priority over the file).
USER_CLAZY_CHECKS="${CLAZY_CHECKS:-}"
# shellcheck source=scripts/clazy.conf
source "${SCRIPT_DIR}/clazy.conf"
CLAZY_CHECKS="${USER_CLAZY_CHECKS:-${CLAZY_CHECKS}}"

if ! command -v "${CLAZY}" >/dev/null 2>&1; then
    echo "❌ clazy-standalone not found: ${CLAZY}"
    echo "👉 Install it (for example: sudo apt install clazy), or point CLAZY to an executable:"
    echo "   CLAZY=/path/to/clazy-standalone scripts/run_clazy.sh"
    exit 1
fi

echo "🔧 Using $(command -v "${CLAZY}")"
echo "🧪 Checks: ${CLAZY_CHECKS}"

#######################################
# CONFIGURE (only if needed)
#######################################

if [ ! -f "${BUILD_DIR}/compile_commands.json" ]; then
    echo "⚙️  Configuring ${BUILD_DIR}..."
    cmake \
        -S "${PROJECT_ROOT}" \
        -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_PREFIX_PATH="${QT_PATH}" \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DFLUVEL_BUILD_APP=ON \
        -DFLUVEL_BUILD_BIND=ON
fi

#######################################
# SELECT FILES
#######################################

# Only .cpp files: headers are checked through the files that include them.
FILES=()
if [ "$#" -gt 0 ]; then
    FILES=("$@")
else
    while IFS= read -r -d '' file; do
        FILES+=("${file}")
    done < <(find "${PROJECT_ROOT}/src" -name "*.cpp" -print0 | sort -z)
fi

#######################################
# RUN CLAZY
#######################################

echo "🔍 Running clazy on ${#FILES[@]} file(s) with ${JOBS} job(s)..."

OUT_DIR="$(mktemp -d)"
trap 'rm -rf "${OUT_DIR}"' EXIT

export CLAZY BUILD_DIR OUT_DIR CLAZY_CHECKS CLAZY_IGNORE_DIRS PROJECT_ROOT

# One output file per source, so parallel runs never mix their messages.
printf '%s\0' "${FILES[@]}" |
    xargs -0 -P "${JOBS}" -I {} bash -c '
        file="$1"
        out="${OUT_DIR}/$(printf "%s" "${file}" | md5sum | cut -d" " -f1).txt"
        "${CLAZY}" -p "${BUILD_DIR}" --checks="${CLAZY_CHECKS}" \
            --ignore-dirs="${CLAZY_IGNORE_DIRS}" \
            --header-filter="${PROJECT_ROOT}/src/" "${file}" > "${out}" 2>&1 || true
    ' _ {}

#######################################
# REPORT
#######################################

# A header included by several .cpp files is reported once for each of them:
# keep one copy of each warning.
cat "${OUT_DIR}"/*.txt 2>/dev/null | grep -E ': (warning|error):' | sort -u > "${LOG_FILE}" || : > "${LOG_FILE}"

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

echo "✅ clazy: nothing to report"
