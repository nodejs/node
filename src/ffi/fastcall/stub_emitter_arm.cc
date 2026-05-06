#include "ffi/fastcall/stub_emitter.h"

#if defined(HAVE_FFI_FASTCALL) && defined(__arm__)

#include <cstring>
#include "ffi/fastcall/jit_memory.h"

namespace node::ffi::fastcall {

namespace {

constexpr unsigned kMaxGPArgs = 3;  // r1..r3 after stripping receiver in r0

uint32_t MovReg(unsigned dst, unsigned src) {
  // mov rd, rm   -> 0xE1A00000 | (dst<<12) | src
  return 0xE1A00000u | (dst << 12) | src;
}

uint32_t LdrLitR12() {
  // ldr r12, [pc, #0]   -> 0xE59FC000
  // pc-relative offset 0 means: literal at pc, where pc = ldr_pos + 8.
  return 0xE59FC000u;
}

uint32_t BxReg(unsigned reg) {
  // bx rn   -> 0xE12FFF10 | rn
  return 0xE12FFF10u | reg;
}

}  // namespace

std::optional<EmittedStub> EmitForwarder(
    v8::Isolate* isolate,
    void* target,
    const std::vector<ArgClass>& args,
    ResultClass /*result*/) {
  unsigned gp = 0, fp = 0;
  for (auto c : args) {
    switch (c) {
      case ArgClass::kGP: ++gp; break;
      case ArgClass::kFP: ++fp; break;
      case ArgClass::kGPPair: return std::nullopt;  // i64/u64 unsupported
    }
  }
  if (gp > kMaxGPArgs) return std::nullopt;
  if (fp > 16) return std::nullopt;

  uint32_t code[8] = {0};
  size_t off = 0;

  // Shift r1..rN -> r0..r(N-1).
  for (unsigned i = 0; i < gp; ++i) {
    code[off++] = MovReg(/*dst=*/i, /*src=*/i + 1);
  }

  // Layout: ldr r12, [pc, #0] ; bx r12 ; .word target ; .word 0
  // After ldr executes, pc = ldr_addr + 8. With imm12 = 0, the ldr loads
  // from pc + 0 = ldr_addr + 8. That's the address of the .word target.
  code[off++] = LdrLitR12();
  code[off++] = BxReg(12);
  code[off++] = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target));
  code[off++] = 0;  // padding to keep stub size a multiple of 8

  size_t bytes = off * 4;
  return JitMemory::Get(isolate)->EmitStub(
      reinterpret_cast<const uint8_t*>(code), bytes);
}

}  // namespace node::ffi::fastcall

#endif
