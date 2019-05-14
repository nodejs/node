// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <stdlib.h>
#include "test/cctest/cctest.h"

#include "src/arm64/decoder-arm64.h"
#include "src/arm64/decoder-arm64-inl.h"
#include "src/arm64/disasm-arm64.h"

#if defined(V8_OS_WIN)
#define RANDGEN() rand()
#else
#define RANDGEN() mrand48()
#endif

namespace v8 {
namespace internal {

TEST(FUZZ_decoder) {
  // Feed noise into the decoder to check that it doesn't crash.
  // 43 million = ~1% of the instruction space.
  static const int instruction_count = 43 * 1024 * 1024;

#if defined(V8_OS_WIN)
  srand(1);
#else
  uint16_t seed[3] = {1, 2, 3};
  seed48(seed);
#endif

  Decoder<DispatchingDecoderVisitor> decoder;
  Instruction buffer[kInstrSize];

  for (int i = 0; i < instruction_count; i++) {
    uint32_t instr = static_cast<uint32_t>(RANDGEN());
    buffer->SetInstructionBits(instr);
    decoder.Decode(buffer);
  }
}


TEST(FUZZ_disasm) {
  // Feed noise into the disassembler to check that it doesn't crash.
  // 9 million = ~0.2% of the instruction space.
  static const int instruction_count = 9 * 1024 * 1024;

#if defined(V8_OS_WIN)
  srand(42);
#else
  uint16_t seed[3] = {42, 43, 44};
  seed48(seed);
#endif

  Decoder<DispatchingDecoderVisitor> decoder;
  DisassemblingDecoder disasm;
  Instruction buffer[kInstrSize];

  decoder.AppendVisitor(&disasm);
  for (int i = 0; i < instruction_count; i++) {
    uint32_t instr = static_cast<uint32_t>(RANDGEN());
    buffer->SetInstructionBits(instr);
    decoder.Decode(buffer);
  }
}

}  // namespace internal
}  // namespace v8

#undef RANDGEN
