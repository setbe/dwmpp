#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-c++}
fail_ok() {
  local file="$1"
  if $CXX -std=c++20 -c "$file" -o /tmp/ctneg.o 2>/tmp/ctneg.err; then
    echo "Expected compile failure, but succeeded: $file" >&2
    exit 1
  fi
}

fail_ok tests/ct_literal_dispatch_negative_bad_format.cpp
fail_ok tests/ct_literal_dispatch_negative_unknown_ns.cpp
fail_ok tests/ct_literal_dispatch_negative_unknown_cmd.cpp

echo "OK: negative compile-time tests failed as expected"
