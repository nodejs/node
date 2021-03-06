// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/arm64/decoder-arm64-inl.h"
#include "src/execution/arm64/simulator-arm64.h"

#include "test/cctest/cctest.h"

namespace v8 {
namespace internal {

#ifdef USE_SIMULATOR
TEST(compute_pac) {
  Decoder<DispatchingDecoderVisitor>* decoder =
      new Decoder<DispatchingDecoderVisitor>();
  Simulator simulator(decoder);

  uint64_t data1 = 0xfb623599da6e8127;
  uint64_t data2 = 0x27979fadf7d53cb7;
  uint64_t context = 0x477d469dec0b8762;
  Simulator::PACKey key = {0x84be85ce9804e94b, 0xec2802d4e0a488e9, -1};

  uint64_t pac1 = simulator.ComputePAC(data1, context, key);
  uint64_t pac2 = simulator.ComputePAC(data2, context, key);

  // NOTE: If the PAC implementation is changed, this may fail due to a hash
  // collision.
  CHECK_NE(pac1, pac2);
}

TEST(add_and_auth_pac) {
#ifdef DEBUG
  i::FLAG_sim_abort_on_bad_auth = false;
#endif
  Decoder<DispatchingDecoderVisitor>* decoder =
      new Decoder<DispatchingDecoderVisitor>();
  Simulator simulator(decoder);

  uint64_t ptr = 0x0000000012345678;
  uint64_t context = 0x477d469dec0b8762;
  Simulator::PACKey key_a = {0x84be85ce9804e94b, 0xec2802d4e0a488e9, 0};
  Simulator::PACKey key_b = {0xec1119e288704d13, 0xd7f6b76e1cea585e, 1};

  uint64_t ptr_a =
      simulator.AddPAC(ptr, context, key_a, Simulator::kInstructionPointer);

  // Attempt to authenticate the pointer with PAC using different keys.
  uint64_t success =
      simulator.AuthPAC(ptr_a, context, key_a, Simulator::kInstructionPointer);
  uint64_t fail =
      simulator.AuthPAC(ptr_a, context, key_b, Simulator::kInstructionPointer);

  uint64_t pac_mask =
      simulator.CalculatePACMask(ptr, Simulator::kInstructionPointer, 0);

  // NOTE: If the PAC implementation is changed, this may fail due to a hash
  // collision.
  CHECK_NE((ptr_a & pac_mask), 0);
  CHECK_EQ(success, ptr);
  CHECK_NE(fail, ptr);
}

TEST(add_and_strip_pac) {
  Decoder<DispatchingDecoderVisitor>* decoder =
      new Decoder<DispatchingDecoderVisitor>();
  Simulator simulator(decoder);

  uint64_t ptr = 0xff00000012345678;
  uint64_t pac_mask =
      simulator.CalculatePACMask(ptr, Simulator::kInstructionPointer, 0);
  uint64_t ptr_a = ptr | pac_mask;

  CHECK_EQ(simulator.StripPAC(ptr_a, Simulator::kInstructionPointer), ptr);
}
#endif  // USE_SIMULATOR

}  // namespace internal
}  // namespace v8
