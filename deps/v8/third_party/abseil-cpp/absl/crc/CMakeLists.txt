# Copyright 2022 The Abseil Authors.
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

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    crc_internal
  HDRS
    "internal/crc.h"
    "internal/crc32_x86_arm_combined_simd.h"
  SRCS
    "internal/crc.cc"
    "internal/crc_internal.h"
    "internal/crc_x86_arm_combined.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::base_cpu_detect
    absl::bits
    absl::config
    absl::core_headers
    absl::endian
    absl::memory
    absl::prefetch
    absl::raw_logging_internal
)

absl_cc_library(
  NAME
    crc32c
  HDRS
    "crc32c.h"
    "internal/crc32c.h"
    "internal/crc_memcpy.h"
  SRCS
    "crc32c.cc"
    "internal/crc32c_inline.h"
    "internal/crc_memcpy_fallback.cc"
    "internal/crc_memcpy_x86_arm_combined.cc"
    "internal/crc_non_temporal_memcpy.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::base_cpu_detect
    absl::config
    absl::core_headers
    absl::crc_internal
    absl::endian
    absl::non_temporal_memcpy
    absl::prefetch
    absl::str_format
    absl::string_view
)

absl_cc_test(
  NAME
    crc32c_test
  SRCS
    "crc32c_test.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::crc32c
    absl::str_format
    absl::string_view
    absl::strings
    GTest::gtest_main
)

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    non_temporal_arm_intrinsics
  HDRS
    "internal/non_temporal_arm_intrinsics.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
)

# Internal-only target, do not depend on directly.
absl_cc_library(
  NAME
    non_temporal_memcpy
  HDRS
    "internal/non_temporal_memcpy.h"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
    absl::core_headers
    absl::non_temporal_arm_intrinsics
)

absl_cc_test(
  NAME
    crc_memcpy_test
  SRCS
    "internal/crc_memcpy_test.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::crc32c
    absl::memory
    absl::random_distributions
    absl::random_random
    absl::string_view
    absl::strings
    GTest::gtest_main
)

absl_cc_test(
  NAME
    non_temporal_memcpy_test
  SRCS
    "internal/non_temporal_memcpy_test.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::non_temporal_memcpy
    GTest::gtest_main
)

absl_cc_library(
  NAME
    crc_cord_state
  HDRS
    "internal/crc_cord_state.h"
  SRCS
    "internal/crc_cord_state.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::config
    absl::crc32c
    absl::no_destructor
    absl::strings
)

absl_cc_test(
  NAME
    crc_cord_state_test
  SRCS
    "internal/crc_cord_state_test.cc"
  COPTS
    ${ABSL_DEFAULT_COPTS}
  DEPS
    absl::crc32c
    absl::crc_cord_state
    GTest::gtest_main
)
