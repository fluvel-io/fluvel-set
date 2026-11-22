#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

# Verifies the wheels collected in ./wheelhouse before they are published:
#   1. every platform built from the commit of the workflow,
#   2. the checksum of every wheel,
#   3. the commit recorded inside the compiled module of every wheel.

set -euo pipefail

EXPECTED_COMMIT="${GITHUB_SHA}"
EXPECTED_SHORT="${GITHUB_SHA::7}"

cd wheelhouse

echo "Expected workflow commit:"
echo "$EXPECTED_COMMIT"
echo ""

for platform in linux windows macos
do
  file="commit-wheels-${platform}.txt"

  if [ ! -f "$file" ]; then
    echo "ERROR: missing $file"
    exit 1
  fi

  CURRENT_COMMIT=$(tr -d '\r\n' < "$file")

  echo "${platform}: ${CURRENT_COMMIT}"

  if [ "$CURRENT_COMMIT" != "$EXPECTED_COMMIT" ]; then
    echo "ERROR: ${platform} wheels were not built from the current workflow commit"
    echo "Expected: $EXPECTED_COMMIT"
    echo "Found   : $CURRENT_COMMIT"
    exit 1
  fi

  case "$platform" in
    linux)   pattern='./*manylinux*.whl' ;;
    windows) pattern='./*win_amd64*.whl' ;;
    macos)   pattern='./*macosx*.whl' ;;
  esac

  # shellcheck disable=SC2086
  if ! ls $pattern >/dev/null 2>&1; then
    echo "ERROR: no wheel for ${platform}"
    exit 1
  fi
done

echo ""
echo "Commit consistency verified ✔"
echo ""

shopt -s nullglob
wheels=(./*.whl)

if [ "${#wheels[@]}" -eq 0 ]; then
  echo "ERROR: no wheel to verify"
  exit 1
fi

for wheel in "${wheels[@]}"
do
  if [ ! -f "${wheel}.sha256" ]; then
    echo "ERROR: missing checksum for ${wheel}"
    exit 1
  fi

  sha256sum -c "${wheel#./}.sha256"
done

echo ""
echo "Checksums verified ✔"
echo ""

# The commit given to the compiler must be the commit of the workflow.
for wheel in "${wheels[@]}"
do
  tmp=$(mktemp -d)

  unzip -q "$wheel" -d "$tmp"

  modules=("$tmp"/fluvel*.so "$tmp"/fluvel*.pyd)

  if [ "${#modules[@]}" -ne 1 ]; then
    echo "ERROR: expected one compiled module in ${wheel}, found ${#modules[@]}"
    exit 1
  fi

  if ! grep -aq "$EXPECTED_SHORT" "${modules[0]}"; then
    echo "ERROR: ${wheel#./} does not contain the commit ${EXPECTED_SHORT}"
    exit 1
  fi

  echo "${wheel#./}: contains commit ${EXPECTED_SHORT}"

  rm -rf "$tmp"
done

echo ""
echo "Embedded commits verified ✔"
