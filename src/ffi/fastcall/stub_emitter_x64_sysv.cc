#include "ffi/fastcall/stub_emitter.h"

#if defined(HAVE_FFI_FASTCALL) && defined(__x86_64__) && \
    (defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__))

#include <cstring>
#include "ffi/fastcall/jit_memory.h"

namespace node::ffi::fastcall {

namespace {
// SysV x86_64 GP arg regs: RDI, RSI, RDX, RCX, R8, R9.
// FP arg regs: XMM0..XMM7. Receiver in RDI.
//
// Strip = shift GP regs down by one. FP regs are NOT shifted — receiver
// is GP. Up to 5 GP args: tail-call. 6 GP args: 7th GP slot is on the
// stack, use call+ret with stack rewrite.

constexpr uint8_t kMoves[5][3] = {
    {0x48, 0x89, 0xF7},  // mov rdi, rsi
    {0x48, 0x89, 0xD6},  // mov rsi, rdx
    {0x48, 0x89, 0xCA},  // mov rdx, rcx
    {0x4C, 0x89, 0xC1},  // mov rcx, r8
    {0x4D, 0x89, 0xC8},  // mov r8, r9
};

}  // namespace

std::optional<EmittedStub> EmitForwarder(
    v8::Isolate* isolate,
    void* target,
    const std::vector<ArgClass>& args,
    ResultClass /*result*/) {
  unsigned gp_count = 0;
  unsigned fp_count = 0;
  for (auto c : args) {
    switch (c) {
      case ArgClass::kGP: ++gp_count; break;
      case ArgClass::kFP: ++fp_count; break;
      case ArgClass::kGPPair: return std::nullopt;  // unsupported on this ABI
    }
  }
  if (gp_count > 6) return std::nullopt;
  if (fp_count > 8) return std::nullopt;

  uint8_t buf[64] = {0};
  size_t off = 0;

  if (gp_count <= 5) {
    // Tail-call path.
    for (unsigned i = 0; i < gp_count; ++i) {
      std::memcpy(buf + off, kMoves[i], 3);
      off += 3;
    }
    // mov r10, imm64; jmp r10
    buf[off++] = 0x49; buf[off++] = 0xBA;
    uint64_t addr = reinterpret_cast<uint64_t>(target);
    std::memcpy(buf + off, &addr, 8); off += 8;
    buf[off++] = 0x41; buf[off++] = 0xFF; buf[off++] = 0xE2;
  } else {
    // gp_count == 6: 7th GP slot on stack. Use call+ret with stack rewrite.
    // sub rsp, 8         -> 48 83 EC 08
    buf[off++] = 0x48; buf[off++] = 0x83; buf[off++] = 0xEC; buf[off++] = 0x08;
    // shift first 5 GP regs
    for (unsigned i = 0; i < 5; ++i) {
      std::memcpy(buf + off, kMoves[i], 3);
      off += 3;
    }
    // mov r9, [rsp+16]    -> 4C 8B 4C 24 10
    buf[off++] = 0x4C; buf[off++] = 0x8B; buf[off++] = 0x4C;
    buf[off++] = 0x24; buf[off++] = 0x10;
    // mov r10, imm64; call r10
    buf[off++] = 0x49; buf[off++] = 0xBA;
    uint64_t addr = reinterpret_cast<uint64_t>(target);
    std::memcpy(buf + off, &addr, 8); off += 8;
    buf[off++] = 0x41; buf[off++] = 0xFF; buf[off++] = 0xD2;
    // add rsp, 8; ret
    buf[off++] = 0x48; buf[off++] = 0x83; buf[off++] = 0xC4; buf[off++] = 0x08;
    buf[off++] = 0xC3;
  }

  return JitMemory::Get(isolate)->EmitStub(buf, off);
}

}  // namespace node::ffi::fastcall

#endif
