// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "src/v8.h"

#include "src/interpreter/bytecode-decoder.h"
#include "test/unittests/interpreter/bytecode-utils.h"
#include "test/unittests/test-utils.h"

namespace v8 {
namespace internal {
namespace interpreter {

TEST(BytecodeDecoder, DecodeBytecodeAndOperands) {
  struct BytecodesAndResult {
    const uint8_t bytecode[32];
    const size_t length;
    int parameter_count;
    const char* output;
  };

  const BytecodesAndResult cases[] = {
      {{B(LdaSmi), U8(1)}, 2, 0, "            LdaSmi [1]"},
      {{B(Wide), B(LdaSmi), U16(1000)}, 4, 0, "      LdaSmi.Wide [1000]"},
      {{B(ExtraWide), B(LdaSmi), U32(100000)},
       6,
       0,
       "LdaSmi.ExtraWide [100000]"},
      {{B(LdaSmi), U8(-1)}, 2, 0, "            LdaSmi [-1]"},
      {{B(Wide), B(LdaSmi), U16(-1000)}, 4, 0, "      LdaSmi.Wide [-1000]"},
      {{B(ExtraWide), B(LdaSmi), U32(-100000)},
       6,
       0,
       "LdaSmi.ExtraWide [-100000]"},
      {{B(Star), R8(5)}, 2, 0, "            Star r5"},
      {{B(Wide), B(Star), R16(136)}, 4, 0, "      Star.Wide r136"},
      {{B(Wide), B(Call), R16(134), R16(135), U16(10), U16(177)},
       10,
       0,
       "Call.Wide r134, r135-r144, [177]"},
      {{B(ForInPrepare), R8(10), R8(11)},
       3,
       0,
       "         ForInPrepare r10, r11-r13"},
      {{B(CallRuntime), U16(134), R8(0), U8(0)},
       5,
       0,
       "   CallRuntime [134], r0-r0"},
      {{B(Ldar),
        static_cast<uint8_t>(Register::FromParameterIndex(2, 3).ToOperand())},
       2,
       3,
       "            Ldar a1"},
      {{B(Wide), B(CreateObjectLiteral), U16(513), U16(1027), U8(165),
        R16(137)},
       9,
       0,
       "CreateObjectLiteral.Wide [513], [1027], #165, r137"},
      {{B(ExtraWide), B(JumpIfNull), U32(123456789)},
       6,
       0,
       "JumpIfNull.ExtraWide [123456789]"},
  };

  for (size_t i = 0; i < arraysize(cases); ++i) {
    // Generate reference string by prepending formatted bytes.
    std::stringstream expected_ss;
    std::ios default_format(nullptr);
    default_format.copyfmt(expected_ss);
    // Match format of BytecodeDecoder::Decode() for byte representations.
    expected_ss.fill('0');
    expected_ss.flags(std::ios::right | std::ios::hex);
    for (size_t b = 0; b < cases[i].length; b++) {
      expected_ss << std::setw(2) << static_cast<uint32_t>(cases[i].bytecode[b])
                  << ' ';
    }
    expected_ss.copyfmt(default_format);
    expected_ss << cases[i].output;

    // Generate decoded byte output.
    std::stringstream actual_ss;
    BytecodeDecoder::Decode(actual_ss, cases[i].bytecode,
                            cases[i].parameter_count);

    // Compare.
    CHECK_EQ(actual_ss.str(), expected_ss.str());
  }
}

}  // namespace interpreter
}  // namespace internal
}  // namespace v8
