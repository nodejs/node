#include "ffi/fastcall/stub_emitter.h"

#if defined(HAVE_FFI_FASTCALL) && (defined(__aarch64__) || defined(_M_ARM64))

#include <cstring>

#include "ffi/fastcall/jit_memory.h"

namespace node::ffi::fastcall {

namespace {

constexpr unsigned kMaxGPArgs = 7;  // x0..x7 minus receiver

// AArch64 instruction helpers. All instructions are 32 bits, little-endian.

// MOV Xd, Xm (encoded as ORR Xd, XZR, Xm)
uint32_t MovReg(unsigned dst, unsigned src) {
  return 0xAA0003E0u | (src << 16) | dst;
}

// MOVZ Xd, #imm16, LSL #shift (shift in {0,16,32,48})
uint32_t Movz(unsigned dst, uint16_t imm, unsigned shift) {
  unsigned hw = shift / 16;
  return 0xD2800000u | (hw << 21) | (uint32_t(imm) << 5) | dst;
}

// MOVK Xd, #imm16, LSL #shift
uint32_t Movk(unsigned dst, uint16_t imm, unsigned shift) {
  unsigned hw = shift / 16;
  return 0xF2800000u | (hw << 21) | (uint32_t(imm) << 5) | dst;
}

// BR Xn  (unconditional branch to register; tail-call)
uint32_t BrReg(unsigned reg) {
  return 0xD61F0000u | (reg << 5);
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
      case ArgClass::kGPPair: return std::nullopt;  // unsupported on this ABI
    }
  }
  if (gp > kMaxGPArgs) return std::nullopt;
  if (fp > 8) return std::nullopt;

  uint32_t code[16] = {0};
  size_t off = 0;

  // Shift x1..xN -> x0..x(N-1). Forward order is safe: at iteration i
  // we emit `MOV xi, x(i+1)`, writing only xi. At iteration i+1 we
  // read x(i+2), which has not been written yet. Each register is
  // written exactly once, and the read of register k always happens
  // before any write to k.
  for (unsigned i = 0; i < gp; ++i) {
    code[off++] = MovReg(/*dst=*/i, /*src=*/i + 1);
  }

  // Load target into x16 (intra-procedure-call scratch reg). Build the
  // 64-bit address with MOVZ + up to 3 MOVKs. Always emit MOVZ for the
  // bottom 16 bits even if zero; only emit higher MOVKs when their
  // bits are non-zero, to keep stubs tight.
  uint64_t addr = reinterpret_cast<uint64_t>(target);
  code[off++] = Movz(16, static_cast<uint16_t>(addr & 0xFFFF), 0);
  if (((addr >> 16) & 0xFFFF) != 0) {
    code[off++] = Movk(16, static_cast<uint16_t>((addr >> 16) & 0xFFFF), 16);
  }
  if (((addr >> 32) & 0xFFFF) != 0) {
    code[off++] = Movk(16, static_cast<uint16_t>((addr >> 32) & 0xFFFF), 32);
  }
  if (((addr >> 48) & 0xFFFF) != 0) {
    code[off++] = Movk(16, static_cast<uint16_t>((addr >> 48) & 0xFFFF), 48);
  }

  // BR x16: tail-call.
  code[off++] = BrReg(16);

  size_t bytes = off * 4;
  return JitMemory::Get(isolate)->EmitStub(
      reinterpret_cast<const uint8_t*>(code), bytes);
}

}  // namespace node::ffi::fastcall

#endif
