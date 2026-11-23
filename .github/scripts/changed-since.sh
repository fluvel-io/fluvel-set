#!/usr/bin/env bash
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

# Tells whether files changed since a reference commit.
#
#   changed-since.sh <ref> <path>...
#
# Prints "true" if at least one <path> (file or directory) differs between <ref>
# and HEAD, "false" otherwise. It also prints "true" when the comparison is not
# possible (empty or unknown <ref>): building too much is better than
# publishing nothing.
#
# If RELEASE is set, the release of that name must exist on GitHub, otherwise
# the answer is "true" (nothing was published yet, or it was deleted).
# This needs the full history and the tags: checkout with fetch-depth: 0.

set -uo pipefail

REF="${1:-}"
shift || true

if [ -n "${RELEASE:-}" ] && ! gh release view "$RELEASE" --repo "$GITHUB_REPOSITORY" >/dev/null 2>&1; then
  echo "Release '$RELEASE' not found: everything has to be built." >&2
  echo true
  exit 0
fi

if [ -z "$REF" ] || ! git rev-parse -q --verify "${REF}^{commit}" >/dev/null; then
  echo "Reference '$REF' not found: everything has to be built." >&2
  echo true
  exit 0
fi

if git diff --quiet "$REF" HEAD -- "$@"; then
  echo "No change since $REF." >&2
  echo false
else
  echo "Changes since $REF:" >&2
  git diff --name-only "$REF" HEAD -- "$@" | head -20 >&2
  echo true
fi
