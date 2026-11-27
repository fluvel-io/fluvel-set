#!/usr/bin/env python3
# SPDX-License-Identifier: CeCILL-2.1
# Copyright (C) 2010-2026 Fabien Bessy

"""Check the sanitizer logs written by the ASan workflow of the Python bindings.

Usage: check-asan-logs.py <log directory> [exit status of pytest]

Python and NumPy are not built with the sanitizers: at exit, CPython alone
leaves hundreds of blocks that LeakSanitizer reports as leaks. A LeakSanitizer
suppression file cannot filter them safely: it matches *any* frame of a stack,
so a pattern hiding the interpreter would also hide a real leak of Fluvel made
from a Python call (the interpreter is in its stack too).

The leak records are therefore filtered here instead. A leak is kept when its
allocation stack goes through the code of Fluvel (src/image_processing or
src/bindings), except when it comes from the initialization of the module: the
strings that pybind11 sets on its types at import are kept until the process
exits, on purpose.

Any other report of AddressSanitizer is always an error.

Exit status: 0 when nothing is found, 1 for a finding, 2 when the logs show
that the leak detection did not run (nothing could have been checked). A
process stopped by a sanitizer or by a failing run does not reach the leak
check at exit: when pytest ended with an error, its absence is not reported.
"""

import re
import sys
from pathlib import Path

LEAK_RECORD_START = re.compile(r"\n(?=(?:Direct|Indirect) leak of )")

# The allocation goes through the code of Fluvel.
FLUVEL_FRAME = re.compile(r"/src/(?:image_processing|bindings)/|fluvel_ip::")

# One-time initialization of the module at import.
MODULE_INIT_FRAME = re.compile(r"pybind11_(?:init|exec)_fluvel|PyInit_fluvel")

MAX_REPORT_LINES = 300

# "#3 0x7f... in function (file:line)": the part after the address.
FRAME_ADDRESS = re.compile(r"^\s*#\d+ 0x[0-9a-f]+ in ")


def allocation_site(record):
    """First frame of the record that belongs to the code of Fluvel."""
    for line in record.splitlines():
        if FLUVEL_FRAME.search(line):
            return FRAME_ADDRESS.sub("", line).strip()

    return "unknown"


def print_report(title, text, limit):
    print(f"::group::{title}")
    lines = text.splitlines()
    print("\n".join(lines[:limit]))
    if len(lines) > limit:
        print(f"... ({len(lines) - limit} more lines, see the uploaded logs)")
    print("::endgroup::")


def main():
    if len(sys.argv) not in (2, 3):
        sys.exit(__doc__)

    pytest_failed = len(sys.argv) == 3 and sys.argv[2] != "0"

    logs = sorted(path for path in Path(sys.argv[1]).glob("asan*") if path.is_file())

    errors = 0
    leak_check_ran = False
    leaks_total = 0
    leaks_from_init = 0
    leaks_of_fluvel = {}  # allocation site -> [records]

    for log in logs:
        text = log.read_text(errors="replace")

        if "ERROR: AddressSanitizer" in text:
            errors += 1
            print(f"::error::AddressSanitizer reported an error ({log.name})")
            print_report(f"AddressSanitizer report ({log.name})", text, MAX_REPORT_LINES)

        if "ERROR: LeakSanitizer" not in text:
            continue

        leak_check_ran = True

        for record in LEAK_RECORD_START.split(text):
            if not record.startswith(("Direct leak", "Indirect leak")):
                continue

            leaks_total += 1

            if not FLUVEL_FRAME.search(record):
                continue

            if MODULE_INIT_FRAME.search(record):
                leaks_from_init += 1
                continue

            record = record.split("\nObjects leaked above:")[0]
            leaks_of_fluvel.setdefault(allocation_site(record), []).append(record)

    print(f"Sanitizer logs: {len(logs)} file(s)")
    leaks_count = sum(len(records) for records in leaks_of_fluvel.values())

    print(
        f"Leak records: {leaks_total} in total, "
        f"{leaks_total - leaks_from_init - leaks_count} outside of Fluvel "
        f"(Python, NumPy: ignored), {leaks_from_init} from the module initialization "
        f"(ignored), {leaks_count} from Fluvel"
    )

    # One report per allocation site: the same leak is often seen from many
    # different callers.
    for site, records in leaks_of_fluvel.items():
        print(f"::error::Memory leak allocated at {site} ({len(records)} record(s))")
        print_report(f"Leak allocated at {site} (first of {len(records)})", records[0], MAX_REPORT_LINES)

    if errors or leaks_of_fluvel:
        return 1

    if not leak_check_ran and not pytest_failed:
        # The interpreter itself always leaves some blocks: no leak report at
        # all means that the leak detection was not active.
        print("::error::No LeakSanitizer report found: the leak detection did not run")
        return 2

    if pytest_failed:
        print("No leak in the code of Fluvel (the tests failed or were stopped, see above).")
        return 0

    print("No error and no leak in the code of Fluvel.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
