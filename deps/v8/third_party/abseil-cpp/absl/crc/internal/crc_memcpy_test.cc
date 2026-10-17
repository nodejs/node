// Copyright 2022 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/crc/internal/crc_memcpy.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "gtest/gtest.h"
#include "absl/base/attributes.h"
#include "absl/base/config.h"
#include "absl/crc/crc32c.h"
#include "absl/crc/internal/crc.h"
#include "absl/crc/internal/crc32_x86_arm_combined_simd.h"
#include "absl/memory/memory.h"
#include "absl/random/distributions.h"
#include "absl/random/random.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

static int64_t g_num_fences = 0;
static int64_t g_num_unfenced_stores = 0;

#if !defined(NDEBUG) && ABSL_HAVE_ATTRIBUTE_WEAK
namespace absl {
ABSL_NAMESPACE_BEGIN
namespace crc_internal {
// The following definitions are overrides for weak symbols in crc.h.
void DebugReportNonTemporalStore() { ++g_num_unfenced_stores; }
void DebugReportStoreFence() {
  ++g_num_fences;
  g_num_unfenced_stores = 0;
}
}  // namespace crc_internal
ABSL_NAMESPACE_END
}  // namespace absl
#endif  // !defined(NDEBUG) && ABSL_HAVE_ATTRIBUTE_WEAK
// Check whether we can test non-temporal stores and store fences, which are
// only used when AVX is supported. These tests also rely on weak symbols, so we
// skip these tests when the weak attribute is not supported. Since these tests
// rely on bookkeeping that is disabled in optimized builds, we also skip them
// in that case.
#if defined(ABSL_CRC_INTERNAL_HAVE_X86_SIMD) && defined(__AVX__) && \
    ABSL_HAVE_ATTRIBUTE_WEAK && !defined(NDEBUG)
constexpr bool kCanTestNonTemporalStores = true;
#else
constexpr bool kCanTestNonTemporalStores = false;
#endif

namespace {

enum CrcEngine {
  ACCELERATED = 0,
  NONTEMPORAL = 1,
  FALLBACK = 2,
};

// Correctness tests:
// - Every source/destination byte alignment 0-15, every size 0-511 bytes
// - Arbitrarily aligned source, large size
template <size_t max_size>
class CrcMemcpyTest : public testing::Test {
 protected:
  CrcMemcpyTest() {
    source_ = std::make_unique<char[]>(kSize);
    destination_ = std::make_unique<char[]>(kSize);
  }
  static constexpr size_t kAlignment = 16;
  static constexpr size_t kMaxCopySize = max_size;
  static constexpr size_t kSize = kAlignment + kMaxCopySize;
  std::unique_ptr<char[]> source_;
  std::unique_ptr<char[]> destination_;

  absl::BitGen gen_;
};

// Small test is slightly larger 4096 bytes to allow coverage of the "large"
// copy function.  The minimum size to exercise all code paths in that function
// would be around 256 consecutive tests (getting every possible tail value
// and 0-2 small copy loops after the main block), so testing from 4096-4500
// will cover all of those code paths multiple times.
typedef CrcMemcpyTest<4500> CrcSmallTest;
typedef CrcMemcpyTest<(1 << 24)> CrcLargeTest;
// Parametrize the small test so that it can be done with all configurations.
template <typename ParamsT>
class EngineParamTestTemplate : public CrcSmallTest,
                                public ::testing::WithParamInterface<ParamsT> {
 protected:
  EngineParamTestTemplate() {
    if (GetParam().crc_engine_selector == FALLBACK) {
      engine_ = std::make_unique<absl::crc_internal::FallbackCrcMemcpyEngine>();
    } else if (GetParam().crc_engine_selector == NONTEMPORAL) {
      engine_ =
          std::make_unique<absl::crc_internal::CrcNonTemporalMemcpyEngine>();
    } else {
      engine_ = absl::crc_internal::CrcMemcpy::GetTestEngine(
          GetParam().vector_lanes, GetParam().integer_lanes);
    }
  }

  // Convenience method.
  ParamsT GetParam() const {
    return ::testing::WithParamInterface<ParamsT>::GetParam();
  }

  std::unique_ptr<absl::crc_internal::CrcMemcpyEngine> engine_;
};
struct TestParams {
  CrcEngine crc_engine_selector = ACCELERATED;
  int vector_lanes = 0;
  int integer_lanes = 0;
};
using EngineParamTest = EngineParamTestTemplate<TestParams>;
// SmallCorrectness is designed to exercise every possible set of code paths
// in the memcpy code, not including the loop.
TEST_P(EngineParamTest, SmallCorrectnessCheckSourceAlignment) {
  constexpr size_t kTestSizes[] = {0, 100, 255, 512, 1024, 4000, kMaxCopySize};

  for (size_t source_alignment = 0; source_alignment < kAlignment;
       source_alignment++) {
    for (auto size : kTestSizes) {
      char* base_data = static_cast<char*>(source_.get()) + source_alignment;
      for (size_t i = 0; i < size; i++) {
        *(base_data + i) =
            static_cast<char>(absl::Uniform<unsigned char>(gen_));
      }
      SCOPED_TRACE(absl::StrCat("engine=<", GetParam().vector_lanes, ",",
                                GetParam().integer_lanes, ">, ", "size=", size,
                                ", source_alignment=", source_alignment));
      absl::crc32c_t initial_crc =
          absl::crc32c_t{absl::Uniform<uint32_t>(gen_)};
      absl::crc32c_t experiment_crc =
          engine_->Compute(destination_.get(), source_.get() + source_alignment,
                           size, initial_crc);
      // Check the memory region to make sure it is the same
      int mem_comparison =
          memcmp(destination_.get(), source_.get() + source_alignment, size);
      SCOPED_TRACE(absl::StrCat("Error in memcpy of size: ", size,
                                " with source alignment: ", source_alignment));
      ASSERT_EQ(mem_comparison, 0);
      absl::crc32c_t baseline_crc = absl::ExtendCrc32c(
          initial_crc,
          absl::string_view(
              static_cast<char*>(source_.get()) + source_alignment, size));
      ASSERT_EQ(baseline_crc, experiment_crc);
    }
  }
}

TEST_P(EngineParamTest, SmallCorrectnessCheckDestAlignment) {
  constexpr size_t kTestSizes[] = {0, 100, 255, 512, 1024, 4000, kMaxCopySize};

  for (size_t dest_alignment = 0; dest_alignment < kAlignment;
       dest_alignment++) {
    for (auto size : kTestSizes) {
      char* base_data = static_cast<char*>(source_.get());
      for (size_t i = 0; i < size; i++) {
        *(base_data + i) =
            static_cast<char>(absl::Uniform<unsigned char>(gen_));
      }
      SCOPED_TRACE(absl::StrCat("engine=<", GetParam().vector_lanes, ",",
                                GetParam().integer_lanes, ">, ", "size=", size,
                                ", destination_alignment=", dest_alignment));
      absl::crc32c_t initial_crc =
          absl::crc32c_t{absl::Uniform<uint32_t>(gen_)};
      absl::crc32c_t experiment_crc =
          engine_->Compute(destination_.get() + dest_alignment, source_.get(),
                           size, initial_crc);
      // Check the memory region to make sure it is the same
      int mem_comparison =
          memcmp(destination_.get() + dest_alignment, source_.get(), size);
      SCOPED_TRACE(absl::StrCat("Error in memcpy of size: ", size,
                                " with dest alignment: ", dest_alignment));
      ASSERT_EQ(mem_comparison, 0);
      absl::crc32c_t baseline_crc = absl::ExtendCrc32c(
          initial_crc,
          absl::string_view(static_cast<char*>(source_.get()), size));
      ASSERT_EQ(baseline_crc, experiment_crc);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(EngineParamTest, EngineParamTest,
                         ::testing::Values(
                             // Tests for configurations that may occur in prod.
                             TestParams{ACCELERATED, 3, 0},
                             TestParams{ACCELERATED, 1, 2},
                             TestParams{ACCELERATED, 1, 0},
                             // Fallback test.
                             TestParams{FALLBACK, 0, 0},
                             // Non Temporal
                             TestParams{NONTEMPORAL, 0, 0}));

// AVX2 loads and stores need 32-byte alignment.
template <size_t N>
struct alignas(32) SimdTestBuffer : std::array<char, N> {};

template <size_t N>
SimdTestBuffer<N> MakeSimdTestBuffer(char first, char last) {
  SimdTestBuffer<N> source = {};
  std::fill(source.begin(), source.end(), 0);
  source[0] = first;
  source[N - 1] = last;
  return source;
}

// Test that bookkeeping for non-temporal stores is correct.
TEST(CrcMemcpyNonTemporalFenceTest, V256_StoreNonTemporal) {
  if (!kCanTestNonTemporalStores) {
    GTEST_SKIP() << "Test does not support this build configuration.";
  }
  const auto source = MakeSimdTestBuffer<32>(
      /*first=*/0x12, /*last=*/0x45);
  auto destination = MakeSimdTestBuffer<32>(
      /*first=*/0, /*last=*/0);
  g_num_fences = 0;
  g_num_unfenced_stores = 0;
  absl::crc_internal::V256_StoreNonTemporal(
      destination.data(), absl::crc_internal::V256_LoadU(source.data()));
  EXPECT_EQ(g_num_fences, 0);
  EXPECT_EQ(g_num_unfenced_stores, 1);
  absl::crc_internal::NonTemporalStoreFence();
  EXPECT_EQ(g_num_fences, 1);
  EXPECT_EQ(g_num_unfenced_stores, 0);
  EXPECT_EQ(destination, source);
}

TEST(CrcMemcpyNonTemporalFenceTest, V256_StorePairNonTemporal) {
  if (!kCanTestNonTemporalStores) {
    GTEST_SKIP() << "Test does not support this build configuration.";
  }
  const auto source = MakeSimdTestBuffer<64>(
      /*first=*/0x12, /*last=*/0x45);
  auto destination = MakeSimdTestBuffer<64>(
      /*first=*/0, /*last=*/0);
  g_num_fences = 0;
  g_num_unfenced_stores = 0;
  absl::crc_internal::V256_StorePairNonTemporal(
      destination.data(), absl::crc_internal::V256_LoadU(source.data()),
      absl::crc_internal::V256_LoadU(source.data() + 32));
  EXPECT_EQ(g_num_fences, 0);
  EXPECT_EQ(g_num_unfenced_stores, 2);
  absl::crc_internal::NonTemporalStoreFence();
  EXPECT_EQ(g_num_fences, 1);
  EXPECT_EQ(g_num_unfenced_stores, 0);
  EXPECT_EQ(destination, source);
}

TEST(CrcMemcpyNonTemporalFenceTest, V256_CopyPairNonTemporal) {
  if (!kCanTestNonTemporalStores) {
    GTEST_SKIP() << "Test does not support this build configuration.";
  }
  const auto source = MakeSimdTestBuffer<64>(
      /*first=*/0x12, /*last=*/0x45);
  auto destination = MakeSimdTestBuffer<64>(
      /*first=*/0, /*last=*/0);
  g_num_fences = 0;
  g_num_unfenced_stores = 0;
  absl::crc_internal::V256_CopyPairNonTemporal(destination.data(),
                                               source.data());
  EXPECT_EQ(g_num_fences, 0);
  EXPECT_EQ(g_num_unfenced_stores, 2);
  absl::crc_internal::NonTemporalStoreFence();
  EXPECT_EQ(g_num_fences, 1);
  EXPECT_EQ(g_num_unfenced_stores, 0);
  EXPECT_EQ(destination, source);
}

TEST(CrcMemcpyNonTemporalFenceTest, V256_StoreU) {
  if (!kCanTestNonTemporalStores) {
    GTEST_SKIP() << "Test does not support this build configuration.";
  }
  const auto source = MakeSimdTestBuffer<32>(
      /*first=*/0x12, /*last=*/0x45);
  auto destination = MakeSimdTestBuffer<32>(
      /*first=*/0, /*last=*/0);
  g_num_fences = 0;
  g_num_unfenced_stores = 0;
  // Regular (temporal) stores should not change the counts.
  absl::crc_internal::V256_StoreU(
      destination.data(), absl::crc_internal::V256_LoadU(source.data()));
  EXPECT_EQ(g_num_fences, 0);
  EXPECT_EQ(g_num_unfenced_stores, 0);
  EXPECT_EQ(destination, source);
}

TEST(CrcMemcpyNonTemporalFenceTest, NonTemporalStoreFence) {
  if (!kCanTestNonTemporalStores) {
    GTEST_SKIP() << "Test does not support this build configuration.";
  }
  g_num_unfenced_stores = 1;
  g_num_fences = 0;
  absl::crc_internal::NonTemporalStoreFence();
  EXPECT_EQ(g_num_fences, 1);
  EXPECT_EQ(g_num_unfenced_stores, 0);
}

// Non-temporal stores are weakly ordered, so Crc32CAndCopy() has to fence them
// before it returns, or the copy is not guaranteed to be visible to the thread
// that reads it next. This checks that it always does, across the sizes and
// alignments that select between the implementation's code paths.
//
// Only the accelerated implementation issues non-temporal stores, and only for
// copies large enough to reach its PCLMUL path, so a fence is not expected
// after every call; what is checked is that no call ever leaves a non-temporal
// store unfenced.
TEST(CrcMemcpyNonTemporalFenceTest, Crc32CAndCopyMemoryFence) {
  if (!kCanTestNonTemporalStores) {
    GTEST_SKIP() << "Test does not support this build configuration.";
  }
  // Enough room for the largest copy below, plus the largest alignment
  // offset.
  constexpr size_t kMaxCopySize = size_t{1} << 16;
  constexpr size_t kMaxAlignment = 64;
  const std::vector<char> source(kMaxCopySize + kMaxAlignment, 0x45);
  auto destination = std::make_unique<char[]>(kMaxCopySize + kMaxAlignment);

  // Sizes straddling the cutoffs between the memcpy-based path and the PCLMUL
  // path, which is the one that stores non-temporally.
  constexpr size_t kSizes[] = {0,    1,    63,   64,   255,         256,
                               1024, 2047, 2048, 4096, kMaxCopySize};
  // Offsets that leave the destination variously cache-line aligned and
  // misaligned, since the implementation peels a differently sized head off
  // the copy depending on the alignment.
  constexpr size_t kOffsets[] = {0, 1, 8, 63};

  g_num_unfenced_stores = 0;
  for (size_t size : kSizes) {
    for (size_t destination_offset : kOffsets) {
      for (size_t source_offset : kOffsets) {
        absl::crc_internal::Crc32CAndCopy(
            destination.get() + destination_offset,
            source.data() + source_offset, size, absl::crc32c_t{0},
            /*non_temporal=*/true);
        EXPECT_EQ(g_num_unfenced_stores, 0)
            << "Crc32CAndCopy() returned with non-temporal stores that were "
               "never fenced, so the copy may not be visible to other "
               "threads.";
      }
    }
  }
}
}  // namespace
