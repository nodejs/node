#
# Copyright 2017 The Abseil Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
absl_cc_library(
  NAME
    any
  HDRS
    "any.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
    absl::core_headers
    absl::utility
  PUBLIC
)

absl_cc_library(
  NAME
    span
  HDRS
    "span.h"
  SRCS
    "internal/span.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::algorithm
    absl::config
    absl::core_headers
    absl::nullability
    absl::throw_delegate
    absl::type_traits
    absl::weakly_mixed_integer
  PUBLIC
)

absl_cc_test(
  NAME
    span_test
  SRCS
    "span_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::span
    absl::base
    absl::config
    absl::core_headers
    absl::exception_testing
    absl::fixed_array
    absl::hash
    absl::inlined_vector
    absl::hash_testing
    absl::strings
    absl::type_traits
    GTest::gmock_main
)

absl_cc_test(
  NAME
    span_test_noexceptions
  SRCS
    "span_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::span
    absl::base
    absl::config
    absl::core_headers
    absl::exception_testing
    absl::fixed_array
    absl::inlined_vector
    absl::hash_testing
    absl::strings
    GTest::gmock_main
)

absl_cc_library(
  NAME
    optional
  HDRS
    "optional.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
    absl::utility
  PUBLIC
)

absl_cc_library(
  NAME
    variant
  HDRS
    "variant.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
    absl::utility
  PUBLIC
)

absl_cc_test(
  NAME
    variant_test
  SRCS
    "variant_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::variant
    GTest::gmock_main
)

absl_cc_library(
  NAME
    compare
  HDRS
    "compare.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
    absl::core_headers
    absl::type_traits
  PUBLIC
)

absl_cc_test(
  NAME
    compare_test
  SRCS
    "compare_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::base
    absl::compare
    GTest::gmock_main
)

# Deprecated empty library.
# Clients should remove this dependency.
absl_cc_library(
  NAME
    bad_any_cast
  PUBLIC
)

# Deprecated empty library.
# Clients should remove this dependency.
absl_cc_library(
  NAME
    bad_optional_access
  PUBLIC
)

# Deprecated empty library.
# Clients should remove this dependency.
absl_cc_library(
  NAME
    bad_variant_access
  PUBLIC
)
