#!/usr/bin/env python3
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

"""Write a <wheel>.sha256 file (sha256sum format) next to each wheel of a directory.

Python is used instead of sha256sum/shasum so that the same step runs on the
Linux, Windows and macOS runners.
"""

import hashlib
import pathlib
import sys

directory = pathlib.Path(sys.argv[1])
wheels = sorted(directory.glob("*.whl"))

if not wheels:
    sys.exit(f"ERROR: no wheel found in {directory}")

for wheel in wheels:
    digest = hashlib.sha256(wheel.read_bytes()).hexdigest()
    (directory / f"{wheel.name}.sha256").write_text(f"{digest}  {wheel.name}\n")
    print(f"{digest}  {wheel.name}")
