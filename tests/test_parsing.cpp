// Prime Differences
// Copyright (c) 2025-2026 David Paul
// SPDX-License-Identifier: BSD-2-Clause
// This file is part of Prime Differences. See LICENSE for details.
#include "doctest.h"
#include "parsing.hpp"

// Tests that ensure parsing works correctly.

TEST_CASE("parse_value_type") {
  CHECK(parse_value_type("0") == 0);
  CHECK(parse_value_type("12345") == 12345);
  CHECK(parse_value_type("18446744073709551615") == UINT64_MAX);

  SUBCASE("Invalid inputs should throw") {
    CHECK_THROWS_AS([&]() { (void)parse_value_type(""); }(), std::runtime_error);
    CHECK_THROWS_AS([&]() { (void)parse_value_type("-5"); }(), std::runtime_error);
    CHECK_THROWS_AS([&]() { (void)parse_value_type("123abc"); }(), std::runtime_error);
    CHECK_THROWS_AS([&]() { (void)parse_value_type("abc123"); }(), std::runtime_error);
    CHECK_THROWS_AS([&]() { (void)parse_value_type("99999999999999999999"); }(),
                    std::runtime_error);
  }
}

TEST_CASE("parse_vector_type") {
  CHECK(parse_vector_type("1,2,3") == vector_type{1, 2, 3});
  CHECK(parse_vector_type("42") == vector_type{42});

  SUBCASE("Whitespace should be allowed") {
    // Based on your trim logic, this might or might not work
    CHECK(parse_vector_type("1, 2, 3") == vector_type{1, 2, 3});
  }
}

TEST_CASE("vector_type_to_string") {
  CHECK(vector_type_to_string({1, 2, 3}) == "1,2,3");
  CHECK(vector_type_to_string({42}) == "42");
  CHECK(vector_type_to_string({}) == "");
}
