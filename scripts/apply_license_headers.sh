#!/usr/bin/env bash
# Mostly vibe coded using Claude 4.6 Opus, then corrected manually
set -euo pipefail

OWNER="David Paul"
YEARS="2025-2026"

HEADER_CPP="// Prime Differences
// Copyright (c) ${YEARS} ${OWNER}
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details."

HEADER_CMAKE="# Prime Differences
# BSD-2-Clause License
# Copyright (c) ${YEARS} ${OWNER}
# This file is part of Prime Differences. See LICENSE for details."

# Prepend license header to all .cpp and .hpp files that don't already have a BSD-2-Clause tag
for f in $(git ls-files '*.cpp' '*.hpp'); do
  if ! head -n 1 "$f" | grep -q "BSD-2-Clause"; then
    tmp="$(mktemp)"
    printf "%s\n" "$HEADER_CPP" > "$tmp"
    cat "$f" >> "$tmp"
    mv "$tmp" "$f"
  fi
done

# Prepend license header to CMakeLists.txt if missing
if [ -f "CMakeLists.txt" ]; then
  if ! head -n 1 "CMakeLists.txt" | grep -q "BSD-2-Clause"; then
    tmp="$(mktemp)"
    printf "%s\n" "$HEADER_CMAKE" > "$tmp"
    cat "CMakeLists.txt" >> "$tmp"
    mv "$tmp" "CMakeLists.txt"
  fi
fi

echo "License headers applied."
