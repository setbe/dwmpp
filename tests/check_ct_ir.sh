#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."

CXX=${CXX:-c++}
$CXX -std=c++20 -O2 -S -emit-llvm tests/ct_literal_dispatch_test.cpp -o /tmp/ct_literal_dispatch_test.ll
$CXX -std=c++20 -O2 -S tests/ct_literal_dispatch_test.cpp -o /tmp/ct_literal_dispatch_test.s

if grep -E "strcmp|memcmp|strlen|hash|registry|lookup" /tmp/ct_literal_dispatch_test.ll; then
  echo "Found forbidden runtime lookup symbols in LLVM IR" >&2
  exit 1
fi

echo "OK: no runtime lookup symbols in LLVM IR"
