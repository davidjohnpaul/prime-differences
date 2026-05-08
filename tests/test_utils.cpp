// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "doctest.h"
#include "utils.hpp"

// Tests that ensure utility functions work correctly.

TEST_CASE("trim") {
  std::string s = "  hello  ";
  trim(s);
  CHECK(s == "hello");

  s = "  ";
  trim(s);
  CHECK(s == "");

  s = "nospaces";
  trim(s);
  CHECK(s == "nospaces");
}
