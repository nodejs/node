// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/execution/arm64/simulator-arm64.h"

#include "test/unittests/test-utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8::internal {

using SimulatorArm64Test = TestWithIsolate;

#ifdef USE_SIMULATOR

namespace {
class SimulatorForTest : public Simulator {
 public:
  using Simulator::MemoryRead;
  using Simulator::MemoryWrite;
  using Simulator::ProbeMemory;
  using Simulator::Simulator;
};
}  // namespace

TEST_F(SimulatorArm64Test, AddressUntag) {
  v8_flags.sim_arm64_tbi = true;
  uintptr_t addr = 0x0000111122223333;
  uintptr_t tagged_addr = 0xaa00111122223333;
  EXPECT_EQ(addr, SimMemory::AddressUntag(tagged_addr));

  v8_flags.sim_arm64_tbi = false;
  EXPECT_EQ(tagged_addr, SimMemory::AddressUntag(tagged_addr));
}

TEST_F(SimulatorArm64Test, MemoryReadWriteTBI) {
  uint64_t data = 0x123456789abcdef0;
  uintptr_t addr = reinterpret_cast<uintptr_t>(&data);
  uintptr_t tagged_addr = addr | (static_cast<uintptr_t>(0xaa) << 56);

  auto decoder = new Decoder<DispatchingDecoderVisitor>();
  SimulatorForTest simulator(decoder, i_isolate());

  v8_flags.sim_arm64_tbi = true;

  simulator.MemoryWrite(tagged_addr, 0x1122334455667788ULL);
  EXPECT_EQ(0x1122334455667788ULL, data);
  EXPECT_EQ(0x1122334455667788ULL, simulator.MemoryRead<uint64_t>(tagged_addr));
}

TEST_F(SimulatorArm64Test, ProbeMemoryTBI) {
#if V8_ENABLE_WEBASSEMBLY && V8_TRAP_HANDLER_SUPPORTED
  uint64_t data = 0;
  uintptr_t addr = reinterpret_cast<uintptr_t>(&data);
  uintptr_t tagged_addr = addr | (static_cast<uintptr_t>(0xaa) << 56);

  auto decoder = new Decoder<DispatchingDecoderVisitor>();
  SimulatorForTest simulator(decoder, i_isolate());

  v8_flags.sim_arm64_tbi = true;

  // ProbeMemory should return true for a valid (but tagged) address.
  EXPECT_TRUE(simulator.ProbeMemory(tagged_addr, 1));
#endif
}

#endif  // USE_SIMULATOR

}  // namespace v8::internal
