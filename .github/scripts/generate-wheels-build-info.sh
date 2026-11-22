#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

# Generates BUILD_INFO-python.txt, the bill of the wheels collected in ./wheelhouse.

set -euo pipefail

if [[ "${GITHUB_REF}" == refs/tags/* ]]; then
  VERSION="${GITHUB_REF_NAME#v}"
else
  VERSION="dev-${GITHUB_SHA::7}"
fi

BUILD_DATE=$(date -u +"%d %B %Y at %H:%M:%S UTC")

# Version of the package, read from the file name of the first wheel.
FIRST_WHEEL=$(cd wheelhouse && ls ./*.whl | head -n 1)
FIRST_WHEEL=${FIRST_WHEEL#./}
PACKAGE_VERSION=$(echo "$FIRST_WHEEL" | cut -d- -f2)

{
  echo "Fluvel Set Python Build Information"
  echo "==================================="
  echo ""
  echo "Version : $VERSION"
  echo "Package : fluvel-set $PACKAGE_VERSION"
  echo "Branch  : ${GITHUB_REF_NAME}"
  echo "Commit  : ${GITHUB_SHA}"
  echo "Date    : $BUILD_DATE"
  echo ""
  echo "Artifacts"
  echo "========="
  echo ""
  (cd wheelhouse && ls ./*.whl | sed 's|^\./||')
  echo ""
  echo "Checksums"
  echo "========="
  echo ""
  (cd wheelhouse && cat ./*.sha256)
  echo ""
  echo "--"
  echo ""
  echo "Generated automatically by the Fluvel Set Python wheels workflow."
} > BUILD_INFO-python.txt
