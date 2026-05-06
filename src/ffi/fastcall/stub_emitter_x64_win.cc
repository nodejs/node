#include "ffi/fastcall/stub_emitter.h"

#if defined(HAVE_FFI_FASTCALL) && defined(__x86_64__) && defined(_WIN32)

#include <cstring>
#include "ffi/fastcall/jit_memory.h"

namespace node::ffi::fastcall {

namespace {
// Win64 ABI: GP regs rcx, rdx, r8, r9 (4); FP regs xmm0-3 (4).
// FP/GP slots are POSITIONAL — slot N is one of {rcx,rdx,r8,r9} or one of
// {xmm0..xmm3} depending on the type at position N. Strip = shift the Nth
// arg from slot N+1 to slot N, where slot K maps to gp_reg[K] for kGP and
// xmm[K] for kFP.
//
// Encoding tables, indexed by destination slot 0..2:
//   GP from slot+1 to slot:
//     slot 0: mov rcx, rdx        48 89 D1
//     slot 1: mov rdx, r8         4C 89 C2
//     slot 2: mov r8,  r9         4D 89 C8
//   FP from slot+1 to slot:
//     slot 0: movaps xmm0, xmm1   0F 28 C1
//     slot 1: movaps xmm1, xmm2   0F 28 CA
//     slot 2: movaps xmm2, xmm3   0F 28 D3
//
// Cap: args.size() <= 3 (4 register slots minus the receiver). Stack-arg
// shuffling is not implemented; signatures with more args fall back to libffi.

const uint8_t kGPMov[3][3] = {
  {0x48, 0x89, 0xD1},  // mov rcx, rdx
  {0x4C, 0x89, 0xC2},  // mov rdx, r8
  {0x4D, 0x89, 0xC8},  // mov r8, r9
};

const uint8_t kFPMov[3][3] = {
  {0x0F, 0x28, 0xC1},  // movaps xmm0, xmm1
  {0x0F, 0x28, 0xCA},  // movaps xmm1, xmm2
  {0x0F, 0x28, 0xD3},  // movaps xmm2, xmm3
};

}  // namespace

std::optional<EmittedStub> EmitForwarder(
    v8::Isolate* isolate,
    void* target,
    const std::vector<ArgClass>& args,
    ResultClass /*result*/) {
  if (args.size() > 3) return std::nullopt;  // 4 reg slots minus receiver
  for (auto c : args) {
    switch (c) {
      case ArgClass::kGP:
      case ArgClass::kFP:
        break;
      case ArgClass::kGPPair:
        return std::nullopt;  // unsupported on Win64
    }
  }

  uint8_t buf[64] = {0};
  size_t off = 0;

  for (size_t i = 0; i < args.size(); ++i) {
    const uint8_t* enc = (args[i] == ArgClass::kGP) ? kGPMov[i] : kFPMov[i];
    std::memcpy(buf + off, enc, 3);
    off += 3;
  }

  // mov r10, imm64; jmp r10
  buf[off++] = 0x49; buf[off++] = 0xBA;
  uint64_t addr = reinterpret_cast<uint64_t>(target);
  std::memcpy(buf + off, &addr, 8); off += 8;
  buf[off++] = 0x41; buf[off++] = 0xFF; buf[off++] = 0xE2;

  return JitMemory::Get(isolate)->EmitStub(buf, off);
}

}  // namespace node::ffi::fastcall

#endif
