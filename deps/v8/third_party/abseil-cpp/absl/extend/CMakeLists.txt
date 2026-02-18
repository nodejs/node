# Copyright 2026 The Abseil Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

absl_cc_library(
  NAME
    dependencies_internal
  HDRS
    "internal/dependencies.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
)

absl_cc_test(
  NAME
    dependencies_internal_test
  SRCS
    "internal/dependencies_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::dependencies_internal
    GTest::gmock_main
)

absl_cc_library(
  NAME
    num_initializers_internal
  HDRS
    "internal/num_initializers.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::core_headers
)

absl_cc_test(
  NAME
    num_initializers_internal_test
  SRCS
    "internal/num_initializers_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::num_initializers_internal
    GTest::gmock_main
)

absl_cc_library(
  NAME
    num_bases_internal
  HDRS
    "internal/num_bases.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::num_initializers_internal
    absl::type_traits
    absl::optional
)

absl_cc_test(
  NAME
    num_bases_internal_test
  SRCS
    "internal/num_bases_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::num_bases_internal
    GTest::gmock_main
)

absl_cc_library(
  NAME
    aggregate_internal
  HDRS
    "internal/aggregate.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::num_bases_internal
    absl::num_initializers_internal
)

absl_cc_test(
  NAME
    aggregate_internal_test
  SRCS
    "internal/aggregate_test.cc"
  COPTS
    ${ABSL_TEST_COPTS}
  DEPS
    absl::aggregate_internal
    GTest::gmock_main
)

absl_cc_library(
  NAME
    tuple_internal
  HDRS
    "internal/tuple.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
)

absl_cc_library(
  NAME
    is_tuple_hashable_internal
  HDRS
    "internal/is_tuple_hashable.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::hash
)

absl_cc_library(
  NAME
    reflection_internal
  HDRS
    "internal/reflection.h"
  SRCS
    "internal/reflection.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::strings
    absl::span
)
