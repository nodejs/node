// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/safepoint-table.h"

#include "src/codegen/macro-assembler.h"
#include "test/unittests/fuzztest.h"
#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

using SafepointTableTest = TestWithZone;

TEST_F(SafepointTableTest, CreatePatch) {
  constexpr int kSize = 200;
  static_assert(kSize >= sizeof(uintptr_t) * 3 * kBitsPerByte);
  GrowableBitVector empty(0, zone());
  GrowableBitVector u(kSize, zone());
  GrowableBitVector v(kSize, zone());
  GrowableBitVector w(kSize, zone());
  u.Add(80, zone());
  v.Add(80, zone());
  w.Add(5, zone());
  uint32_t common_prefix_bits;
  BitVector* patch =
      CompareAndCreateXorPatch(zone(), u, v, &common_prefix_bits);
  EXPECT_EQ(kMaxUInt32, common_prefix_bits);
  EXPECT_EQ(nullptr, patch);

  // Difference to empty vector.
  patch = CompareAndCreateXorPatch(zone(), empty, v, &common_prefix_bits);
  EXPECT_EQ(80u, common_prefix_bits);
  EXPECT_EQ(1, patch->length());
  EXPECT_TRUE(patch->Contains(0));

  // Empty vector's difference to a non-empty vector.
  patch = CompareAndCreateXorPatch(zone(), u, empty, &common_prefix_bits);
  EXPECT_EQ(80u, common_prefix_bits);
  EXPECT_EQ(1, patch->length());
  EXPECT_TRUE(patch->Contains(0));

  // Difference 0->1 near the beginning.
  patch = CompareAndCreateXorPatch(zone(), empty, w, &common_prefix_bits);
  EXPECT_EQ(5u, common_prefix_bits);
  EXPECT_EQ(1, patch->length());
  EXPECT_TRUE(patch->Contains(0));

  // Difference in the middle of the second word.
  v.Add(81, zone());
  v.Add(191, zone());
  patch = CompareAndCreateXorPatch(zone(), u, v, &common_prefix_bits);
  EXPECT_EQ(81u, common_prefix_bits);
  EXPECT_EQ(192 - 81, patch->length());
  EXPECT_TRUE(patch->Contains(81 - 81));
  EXPECT_TRUE(patch->Contains(191 - 81));

  // Now with identical tails and only a small difference in the middle.
  v.Add(83, zone());
  u.Add(191, zone());
  patch = CompareAndCreateXorPatch(zone(), u, v, &common_prefix_bits);
  EXPECT_EQ(81u, common_prefix_bits);
  EXPECT_EQ(3, patch->length());
  EXPECT_TRUE(patch->Contains(81 - 81));
  EXPECT_TRUE(patch->Contains(83 - 81));

  // Difference 1->0 at the beginning of the second word.
  u.Add(64, zone());
  patch = CompareAndCreateXorPatch(zone(), u, v, &common_prefix_bits);
  EXPECT_EQ(64u, common_prefix_bits);
  EXPECT_EQ(84 - 64, patch->length());
  EXPECT_TRUE(patch->Contains(64 - 64));
  EXPECT_FALSE(patch->Contains(80 - 64));  // Both u and v have that bit.
  EXPECT_TRUE(patch->Contains(81 - 64));
  EXPECT_TRUE(patch->Contains(83 - 64));

  // Both contain a very high bit.
  u.Add(1023, zone());
  v.Add(1023, zone());
  patch = CompareAndCreateXorPatch(zone(), u, v, &common_prefix_bits);
  EXPECT_EQ(64u, common_prefix_bits);
  EXPECT_EQ(84 - 64, patch->length());
  EXPECT_TRUE(patch->Contains(64 - 64));
  EXPECT_TRUE(patch->Contains(83 - 64));
}

#ifdef V8_ENABLE_FUZZTEST
class SafepointTableFuzzTest
    : public fuzztest::PerFuzzTestFixtureAdapter<TestWithZone> {
 public:
  void TestXorPatch(const std::set<int>& bits_a, const std::set<int>& bits_b) {
    GrowableBitVector a;
    for (int bit : bits_a) a.Add(bit, zone());
    GrowableBitVector b;
    for (int bit : bits_b) b.Add(bit, zone());

    uint32_t common_prefix_bits;
    BitVector* xor_patch =
        CompareAndCreateXorPatch(zone(), b, a, &common_prefix_bits);
    std::set<int> patched_a = bits_a;
    if (xor_patch) {
      // Apply patch to `patched_a`. That should result in `b`.
      for (int bit : *xor_patch) {
        bit += common_prefix_bits;
        if (!patched_a.erase(bit)) patched_a.insert(bit);
      }
    } else {
      EXPECT_EQ(kMaxUInt32, common_prefix_bits);
    }

    EXPECT_EQ(patched_a, bits_b);
  }
};

V8_FUZZ_TEST_F(SafepointTableFuzzTest, TestXorPatch)
    .WithDomains(fuzztest::SetOf(fuzztest::InRange(0, 1 << 16)),
                 fuzztest::SetOf(fuzztest::InRange(0, 1 << 16)));
#endif  // V8_ENABLE_FUZZTEST

namespace {
bool Contains(base::Vector<const uint8_t> bit_vector, int bit) {
  size_t byte_index = bit / kBitsPerByte;
  size_t bit_index = bit % kBitsPerByte;
  if (byte_index >= bit_vector.size()) return false;
  return (bit_vector[byte_index] & (1 << bit_index)) != 0;
}
}  // namespace

TEST_F(SafepointTableTest, NoTaggedSlots) {
  SafepointTableBuilder builder(zone());
#if V8_TARGET_ARCH_ARM64
  Assembler assembler(zone(), AssemblerOptions{});
#else
  Assembler assembler(AssemblerOptions{});
#endif  // V8_TARGET_ARCH_ARM64

  static constexpr int kStackSlotCount = 8;
  // Define a few safepoints, but without any tagged slots. They can all get
  // deduplicated to an empty entry.
  builder.DefineSafepoint(&assembler, 10);
  builder.DefineSafepoint(&assembler, 20);
  builder.Emit(&assembler, kStackSlotCount);

  std::unique_ptr<AssemblerBuffer> fake_code = assembler.ReleaseBuffer();

  Address instruction_start = reinterpret_cast<Address>(fake_code->start());
  Address safepoint_table_start =
      instruction_start + builder.safepoint_table_offset();
  SafepointTable table =
      SafepointTable::ForTest(instruction_start, safepoint_table_start);
  EXPECT_EQ(kStackSlotCount, table.stack_slots());
  // Make sure the decoder can deal with empty entries.
  SafepointEntry& entry = table.FindEntry(instruction_start + 1);
  EXPECT_EQ(entry.tagged_slots()[0], 0);
}

TEST_F(SafepointTableTest, Basic) {
  SafepointTableBuilder builder(zone());
#if V8_TARGET_ARCH_ARM64
  Assembler assembler(zone(), AssemblerOptions{});
#else
  Assembler assembler(AssemblerOptions{});
#endif  // V8_TARGET_ARCH_ARM64

  static constexpr int kStackSlotCount = 6;
  using Safepoint = SafepointTableBuilder::Safepoint;
  {
    Safepoint sp = builder.DefineSafepoint(&assembler, 0x12f);
    sp.DefineTaggedStackSlot(5);
  }
  {
    builder.DefineSafepoint(&assembler, 0x143);
    // No tagged slots.
  }
  builder.Emit(&assembler, kStackSlotCount);

  std::unique_ptr<AssemblerBuffer> fake_code = assembler.ReleaseBuffer();

  Address instruction_start = reinterpret_cast<Address>(fake_code->start());
  Address safepoint_table_start =
      instruction_start + builder.safepoint_table_offset();

  // Useful for debugging this test.
  if ((false)) {
    SafepointTable table =
        SafepointTable::ForTest(instruction_start, safepoint_table_start);
    table.Print(std::cout);
  }

  auto flip_index = [](int original_index) {
    return kStackSlotCount - 1 - original_index;
  };
  {
    SafepointTable table =
        SafepointTable::ForTest(instruction_start, safepoint_table_start);
    EXPECT_EQ(kStackSlotCount, table.stack_slots());
    SafepointEntry& entry = table.FindEntry(instruction_start + 0x12f);
    EXPECT_EQ(entry.pc(), 0x12f);
    EXPECT_EQ(entry.tagged_slots()[0], 1 << flip_index(5));
  }
  {
    SafepointTable table =
        SafepointTable::ForTest(instruction_start, safepoint_table_start);
    SafepointEntry& entry = table.FindEntry(instruction_start + 0x143);
    EXPECT_EQ(entry.pc(), 0x143);
    EXPECT_EQ(entry.tagged_slots()[0], 0);
  }
}

TEST_F(SafepointTableTest, Bigger) {
  SafepointTableBuilder builder(zone());
#if V8_TARGET_ARCH_ARM64
  Assembler assembler(zone(), AssemblerOptions{});
#else
  Assembler assembler(AssemblerOptions{});
#endif  // V8_TARGET_ARCH_ARM64

  static constexpr int kStackSlotCount = 17;
  using Safepoint = SafepointTableBuilder::Safepoint;
  int last_updated = 0;
  struct Data {
    int pc;
    int deopt;
    int trampoline;
    GrowableBitVector bits;
    Data(int pc, int deopt, int trampoline, std::initializer_list<int> bit_list,
         Zone* zone)
        : pc(pc), deopt(deopt), trampoline(trampoline) {
      for (int bit : bit_list) bits.Add(bit, zone);
    }
  };
  int kNoDeopt = SafepointEntry::kNoDeoptIndex;
  int kNoTrampoline = SafepointEntry::kNoTrampolinePC;
  // Taken from an example somewhere in mjsunit tests that flushed out bugs:
  const Data test_data[] = {
      {0xdc, 3, 0x6bd, {8, 13}, zone()},
      {0x21a, 4, 0x6c1, {13}, zone()},
      {0x2c4, 5, 0x6c5, {13}, zone()},
      {0x2e5, kNoDeopt, kNoTrampoline, {13, 15}, zone()},
      {0x304, 6, 0x6c9, {13, 14, 15}, zone()},
      {0x325, 7, 0x6cd, {13}, zone()},
      {0x369, 8, 0x6d1, {8, 13}, zone()},
      {0x3f8, kNoDeopt, kNoTrampoline, {8, 13}, zone()},
      {0x44b, kNoDeopt, kNoTrampoline, {8, 13, 15}, zone()},
      {0x468, 9, 0x6d5, {8, 13}, zone()},
      {0x490, 10, 0x6d9, {13}, zone()},
      {0x51d, kNoDeopt, kNoTrampoline, {13}, zone()},
      {0x585, kNoDeopt, kNoTrampoline, {13, 14}, zone()},
      {0x5ba, 11, 0x6dd, {13}, zone()},
      {0x5df, 12, 0x6e1, {13}, zone()},
  };
  for (const Data& data : test_data) {
    {
      Safepoint sp = builder.DefineSafepoint(&assembler, data.pc);
      sp.DefineTaggedStackSlots(data.bits);
    }
    if (data.deopt != kNoDeopt) {
      last_updated = builder.UpdateDeoptimizationInfo(data.pc, data.trampoline,
                                                      last_updated, data.deopt);
    }
  }

  builder.Emit(&assembler, kStackSlotCount);

  std::unique_ptr<AssemblerBuffer> fake_code = assembler.ReleaseBuffer();

  Address instruction_start = reinterpret_cast<Address>(fake_code->start());
  Address safepoint_table_start =
      instruction_start + builder.safepoint_table_offset();

  // Useful for debugging this test.
  if ((false)) {
    SafepointTable table =
        SafepointTable::ForTest(instruction_start, safepoint_table_start);
    table.Print(std::cout);
  }

  auto flip_index = [](int original_index) {
    return kStackSlotCount - 1 - original_index;
  };
  EXPECT_EQ(0, flip_index(kStackSlotCount - 1));
  EXPECT_EQ(kStackSlotCount - 1, flip_index(0));

  for (const Data& data : test_data) {
    SCOPED_TRACE(data.pc);
    SafepointTable table =
        SafepointTable::ForTest(instruction_start, safepoint_table_start);
    EXPECT_EQ(kStackSlotCount, table.stack_slots());
    SafepointEntry& entry = table.FindEntry(instruction_start + data.pc);
    EXPECT_EQ(entry.pc(), data.pc);
    if (data.deopt != kNoDeopt) {
      EXPECT_EQ(entry.deoptimization_index(), data.deopt);
      EXPECT_EQ(entry.trampoline_pc(), data.trampoline);
    }
    for (int bit : data.bits) {
      SCOPED_TRACE(bit);
      EXPECT_TRUE(Contains(entry.tagged_slots(), flip_index(bit)));
    }
  }

  // Test the deduplication logic: when looking for a PC between two safepoints
  // or trampoline, return the preceding safepoint/trampoline.
  std::pair<int, int> fallbacks[] = {
      {0xdd, 0xdc},     // Right after an existing PC.
      {0x2c3, 0x21a},   // Right before the next PC.
      {0x600, 0x5df},   // After the last PC.
      {0x6bd, 0xdc},    // Trampoline.
      {0x6c0, 0xdc},    // Between two trampolines.
      {0x6c1, 0x21a},   // Next trampoline.
      {0x6dc, 0x490},   // Must rewind past two entries without trampolines.
      {0x700, 0x5df}};  // Beyond the last trampoline.
  for (auto [lookup, returned] : fallbacks) {
    SafepointTable table =
        SafepointTable::ForTest(instruction_start, safepoint_table_start);
    SafepointEntry& entry = table.FindEntry(instruction_start + lookup);
    EXPECT_EQ(entry.pc(), returned);
  }
}

TEST_F(SafepointTableTest, DecodeSafepointEntry) {
  static constexpr size_t kSize = 16;  // Bytes. Enough for this test.
  base::OwnedVector<uint8_t> safepoint(std::make_unique<uint8_t[]>(kSize),
                                       kSize);
  base::OwnedVector<uint8_t> tagged_slots(std::make_unique<uint8_t[]>(kSize),
                                          kSize);
  base::OwnedVector<uint8_t> expected_vector(std::make_unique<uint8_t[]>(kSize),
                                             kSize);
  auto Test = [&](std::initializer_list<uint8_t> input,
                  std::initializer_list<uint8_t> expected) {
    std::copy(input.begin(), input.end(), safepoint.data());
    // We could zero out the tail of {safepoint}, but we shouldn't need to:
    // the decoder shouldn't read too far.
    // We can't compare against {expected[i]} directly, so write {expected}
    // into {expected_vector}.
    std::copy(expected.begin(), expected.end(), expected_vector.data());
    const uint8_t* ptr = safepoint.data();
    DecodeSafepointEntry<true>(&ptr, tagged_slots);
    DCHECK_EQ(ptr, safepoint.data() + input.size());
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(tagged_slots[i], expected_vector[i]);
    }
    // In {update_tagged_slots=false} mode, we consume the same number of
    // bytes, but don't touch the output vector.
    const uint8_t* no_update = safepoint.data();
    DecodeSafepointEntry<false>(&no_update, tagged_slots);
    DCHECK_EQ(ptr, no_update);
    for (size_t i = 0; i < expected.size(); ++i) {
      EXPECT_EQ(tagged_slots[i], expected_vector[i]);
    }
  };
  // These tests are not independent! In order to test the updating behavior,
  // the {tagged_slots} vector is passed on from each test to the next.

  // Full encoding, setting three bytes.
  Test({0b0'000011'0, 0b00001111, 0b00111100, 0b11110000},
       {0b00001111, 0b00111100, 0b11110000});
  // Zero-byte as input means nothing changes.
  Test({0}, {0b00001111, 0b00111100, 0b11110000});
  // Compact encoding flipping two bits, no skipped bits.
  Test({0b0'00011'01}, {0b00001100, 0b00111100, 0b11110000});
  // Compact encoding flipping two bits, with 11 skipped bits (encoded as 4).
  Test({0b0'00100'11, 0b0'00011'01}, {0b00001100, 0b00100100, 0b11110000});
  // Flipping the same two bits back, but without skipped bits.
  Test({0b1'00000'01, 0b1'1000000, 0b0'0000001},
       {0b00001100, 0b00111100, 0b11110000});
}

TEST_F(SafepointTableTest, EncodeSafepointEntry) {
  auto Test = [](int stack_slot_count, uint32_t common_prefix, int bits_length,
                 std::initializer_list<int> bit_list,
                 std::initializer_list<uint8_t> result_bytes, Zone* zone) {
#if V8_TARGET_ARCH_ARM64
    Assembler assembler(zone, AssemblerOptions{});
#else
    Assembler assembler(AssemblerOptions{});
#endif  // V8_TARGET_ARCH_ARM64
    BitVector bits(bits_length, zone);
    for (int bit : bit_list) bits.Add(bit);
    std::vector<uint8_t> result(result_bytes);

    EncodeSafepointEntry(stack_slot_count, common_prefix, &bits, &assembler);
    size_t num_bytes_generated = assembler.pc_offset();
    std::unique_ptr<AssemblerBuffer> output = assembler.ReleaseBuffer();
    EXPECT_EQ(num_bytes_generated, result.size());
    for (size_t i = 0; i < result.size(); ++i) {
      EXPECT_EQ(output->start()[i], result[i]);
    }
  };
  // Notation: apostrophes separate payload bits from tag bits.
  // One byte xor mask.
  Test(10, 9, 1, {0}, {0b0'00001'01}, zone());
  Test(10, 8, 1, {0}, {0b0'00010'01}, zone());
  Test(10, 5, 3, {0, 2}, {0b0'10100'01}, zone());
  // Two bytes xor mask.
  Test(10, 0, 9, {0, 3, 4, 5, 8}, {0b1'10010'01, 0b0'0010011}, zone());
  // One byte to skip 9 bits (encoded as 2), one byte xor mask.
  Test(10, 0, 1, {0}, {0b0'00010'11, 0b0'00001'01}, zone());
  // Two bytes to skip 53 bits (encoded as 46), two bytes xor mask.
  Test(60, 1, 6, {0, 5}, {0b1'01110'11, 0b0'1, 0b1'00001'01, 0b0'1}, zone());
}

}  // namespace v8::internal
