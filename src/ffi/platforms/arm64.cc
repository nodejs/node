#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__aarch64__) || defined(_M_ARM64)

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

#include <stdint.h>

#include <limits>

#if defined(__APPLE__)
#include <libkern/OSCacheControl.h>
#endif

namespace {

using node::ffi::FastFFIType;

// Fixed instruction encodings used verbatim in every trampoline. These are the
// prologue/epilogue and indirect-call instructions that do not depend on a
// register number or immediate value.
constexpr uint32_t kBlrX16 = 0xd63f0200;
constexpr uint32_t kStpFpLrPreIndex = 0xa9bf7bfd;
constexpr uint32_t kLdpFpLrPostIndex = 0xa8c17bfd;
constexpr uint32_t kRet = 0xd65f03c0;

// Floating-point values use vN registers in the AArch64 ABI. Everything else
// uses general-purpose xN registers and follows the GP shuffle path below.
bool IsFloatType(FastFFIType type) {
  return type == FastFFIType::kFloat32 || type == FastFFIType::kFloat64;
}

// The trampoline only needs to distinguish FP from non-FP ABI classes for the
// current scalar implementation.
bool IsGPType(FastFFIType type) {
  return !IsFloatType(type);
}

// kBuffer means V8 passes the original JS value; the trampoline must call back
// into C++ to convert it to a native pointer before invoking the target.
bool IsPointerValueType(FastFFIType type) {
  return type == FastFFIType::kBuffer;
}

// The helpers below encode the small set of AArch64 instructions needed by the
// generated trampoline. Keeping them as functions makes the emitted sequence
// readable without pulling in a full assembler dependency.
uint32_t MovX(unsigned dst, unsigned src) {
  // mov Xdst, Xsrc is encoded as ORR with xzr. It is used for register
  // shuffles between V8's calling convention and the target ABI.
  return 0xaa0003e0 | (src << 16) | dst;
}

uint32_t SubSp(unsigned imm) {
  // Reserve stack spill space with `sub sp, sp, #imm`. The caller guarantees
  // 16-byte alignment and keeps the immediate within the 12-bit encoding.
  return 0xd10003ff | ((imm & 0xfff) << 10);
}

uint32_t AddSp(unsigned imm) {
  // Release stack spill space with `add sp, sp, #imm`, matching SubSp().
  return 0x910003ff | ((imm & 0xfff) << 10);
}

uint32_t StrXSp(unsigned reg, unsigned offset) {
  // Store a 64-bit GP register at [sp + offset]. Offsets are slot-based in the
  // instruction encoding, so byte offsets are divided by 8.
  return 0xf90003e0 | ((offset / 8) << 10) | reg;
}

uint32_t LdrXSp(unsigned reg, unsigned offset) {
  // Reload a 64-bit GP register from [sp + offset], using the same spill-slot
  // layout as StrXSp().
  return 0xf94003e0 | ((offset / 8) << 10) | reg;
}

uint32_t MovzW(unsigned dst, uint16_t value) {
  // Load a small immediate into a W register. The buffer helper's argument
  // index is uint32_t, but current Fast API signatures are capped well below
  // the 16-bit immediate range.
  return 0x52800000 | (value << 5) | dst;
}

uint32_t CmpX(unsigned lhs, unsigned rhs) {
  // Compare two X registers by encoding SUBS xzr, lhs, rhs. Used to test the
  // helper result against the invalid-pointer sentinel loaded into x16.
  return 0xeb00001f | (rhs << 16) | (lhs << 5);
}

uint32_t BCond(unsigned instruction_offset, unsigned cond) {
  // Emit a conditional branch with an instruction-count offset. The buffer
  // helper path uses cond=1 (`ne`) to skip the early-return sequence when the
  // converted pointer is valid.
  return 0x54000000 | ((instruction_offset & 0x7ffff) << 5) | cond;
}

uint32_t MovzX16(uint16_t value, unsigned shift) {
  // Start materializing a 64-bit absolute address in x16, 16 bits at a time.
  return 0xd2800000 | ((shift / 16) << 21) | (value << 5) | 16;
}

uint32_t MovkX16(uint16_t value, unsigned shift) {
  // Patch the next 16-bit lane of x16 without disturbing the lanes already
  // written by MovzX16()/previous MovkX16() instructions.
  return 0xf2800000 | ((shift / 16) << 21) | (value << 5) | 16;
}

uint32_t SxtbW(unsigned reg) {
  // Sign-extend the low 8 bits in-place for int8 arguments/returns.
  return 0x13001c00 | (reg << 5) | reg;
}

uint32_t UxtbW(unsigned reg) {
  // Zero-extend the low 8 bits in-place for bool/uint8 arguments/returns.
  return 0x53001c00 | (reg << 5) | reg;
}

uint32_t SxthW(unsigned reg) {
  // Sign-extend the low 16 bits in-place for int16 arguments/returns.
  return 0x13003c00 | (reg << 5) | reg;
}

uint32_t UxthW(unsigned reg) {
  // Zero-extend the low 16 bits in-place for uint16 arguments/returns.
  return 0x53003c00 | (reg << 5) | reg;
}

// Narrow integer parameters arrive widened by V8. Sign/zero extend them back
// to the ABI width expected by the native target before the final call.
bool EmitNarrow(uint32_t** cursor, FastFFIType type, unsigned reg) {
  switch (type) {
    case FastFFIType::kBool:
    case FastFFIType::kUint8:
      *(*cursor)++ = UxtbW(reg);
      return true;
    case FastFFIType::kInt8:
      *(*cursor)++ = SxtbW(reg);
      return true;
    case FastFFIType::kUint16:
      *(*cursor)++ = UxthW(reg);
      return true;
    case FastFFIType::kInt16:
      *(*cursor)++ = SxthW(reg);
      return true;
    default:
      return false;
  }
}

// Materialize an absolute address into x16, the scratch register used for BLR.
void EmitLoadX16(uint32_t** cursor, uintptr_t address) {
  *(*cursor)++ = MovzX16((address >> 0) & 0xffff, 0);
  *(*cursor)++ = MovkX16((address >> 16) & 0xffff, 16);
  *(*cursor)++ = MovkX16((address >> 32) & 0xffff, 32);
  *(*cursor)++ = MovkX16((address >> 48) & 0xffff, 48);
}

// Stack adjustment must preserve the platform's 16-byte SP alignment rule.
unsigned Align16(unsigned value) {
  return (value + 15) & ~15;
}

void* AllocateCode(size_t code_size) {
#if defined(_WIN32)
  return VirtualAlloc(
      nullptr, code_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
  void* code = mmap(nullptr,
                    code_size,
                    PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANON,
                    -1,
                    0);
  return code == MAP_FAILED ? nullptr : code;
#endif
}

void FreeCode(void* code, size_t code_size) {
#if defined(_WIN32)
  VirtualFree(code, 0, MEM_RELEASE);
#else
  munmap(code, code_size);
#endif
}

void FlushCode(void* code, size_t written) {
#if defined(_WIN32)
  FlushInstructionCache(GetCurrentProcess(), code, written);
#elif defined(__APPLE__)
  // Make the just-written instructions visible to the CPU's instruction cache.
  sys_icache_invalidate(code, written);
#else
  __builtin___clear_cache(static_cast<char*>(code),
                          static_cast<char*>(code) + written);
#endif
}

bool ProtectCode(void* code, size_t code_size) {
#if defined(_WIN32)
  DWORD old_protect;
  return VirtualProtect(code, code_size, PAGE_EXECUTE_READ, &old_protect) != 0;
#else
  return mprotect(code, code_size, PROT_READ | PROT_EXEC) == 0;
#endif
}

}  // namespace

extern "C" bool node_ffi_create_fast_trampoline(
    void* target,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  // Null inputs mean the caller cannot safely create executable code for this
  // signature. Report rejection so the generic FFI path can be used instead.
  if (target == nullptr || out == nullptr) {
    return false;
  }

  size_t gp_count = 0;
  size_t fp_count = 0;
  bool has_buffer_args = false;
  // Count the incoming register classes before emitting code. This keeps all
  // unsupported cases out of the code generator instead of relying on partial
  // instruction streams to fail later.
  for (size_t i = 0; i < argc; i++) {
    if (IsFloatType(args[i])) {
      fp_count++;
    } else {
      gp_count++;
      has_buffer_args = has_buffer_args || IsPointerValueType(args[i]);
    }
  }

  // x0 is occupied by the V8 receiver, leaving x1..x7 for incoming GP args.
  // Keep the first implementation register-only rather than generating stack
  // shuffles incorrectly.
  // Buffer conversion calls into C++ before invoking the target. Keep the
  // first implementation scalar-only so no FP argument registers need to be
  // preserved across helper calls.
  if (has_buffer_args && fp_count != 0) {
    return false;
  }

  const size_t incoming_gp_count = gp_count + (has_buffer_args ? 1 : 0);
  // When V8 passes FastApiCallbackOptions, it consumes one additional GP
  // register after the declared user arguments. This implementation refuses
  // signatures that would require loading incoming GP args from the stack.
  if (incoming_gp_count > 7 || fp_count > 8) {
    return false;
  }

  constexpr size_t kMaxInstructions = 160;
  const size_t code_size = kMaxInstructions * sizeof(uint32_t);
  // The fixed allocation is intentionally larger than any currently emitted
  // path. It keeps the generator simple while the supported signature set is
  // small and bounded.
  // Generate into writable anonymous memory first; the page is made executable
  // only after the instruction stream is complete and the instruction cache is
  // synchronized.
  void* code = AllocateCode(code_size);
  if (code == nullptr) {
    return false;
  }

  uint32_t* cursor = static_cast<uint32_t*>(code);
  // Save frame pointer and link register so helper calls and the final target
  // call can return through this generated trampoline safely.
  *cursor++ = kStpFpLrPreIndex;

  if (has_buffer_args) {
    // Buffer conversion calls a C++ helper before the target call, so spill all
    // incoming GP registers that may be clobbered by that helper.
    const unsigned frame_size = Align16(incoming_gp_count * 8);
    if (frame_size != 0) {
      *cursor++ = SubSp(frame_size);
    }

    for (unsigned i = 0; i < incoming_gp_count; i++) {
      // V8 presents user GP args in x1.. and the callback options pointer after
      // them. Spill that whole incoming register window at stack slot i.
      *cursor++ = StrXSp(i + 1, i * 8);
    }

    unsigned incoming_gp_index = 0;
    unsigned target_gp_index = 0;
    const unsigned options_offset = gp_count * 8;
    // `incoming_gp_index` walks spilled V8 argument slots; `target_gp_index`
    // walks the ABI registers expected by the native target. They differ only
    // because V8 has an implicit receiver in x0 that the target does not see.
    // Reload each original argument in target ABI order. Buffer arguments are
    // converted to raw pointers; scalar GP arguments are moved/narrowed.
    for (size_t i = 0; i < argc; i++) {
      if (!IsGPType(args[i])) {
        continue;
      }

      const unsigned arg_offset = incoming_gp_index * 8;
      if (IsPointerValueType(args[i])) {
        // Call node_ffi_fast_buffer_data(value, options, index). A sentinel
        // return means the helper already threw, so return nullptr/zero without
        // invoking the native target.
        *cursor++ = LdrXSp(0, arg_offset);
        *cursor++ = LdrXSp(1, options_offset);
        *cursor++ = MovzW(2, static_cast<uint16_t>(i));
        EmitLoadX16(&cursor,
                    reinterpret_cast<uintptr_t>(node_ffi_fast_buffer_data));
        // blr x16 clobbers caller-saved registers. The original arguments were
        // spilled above specifically so subsequent target args can be restored.
        *cursor++ = kBlrX16;
        EmitLoadX16(&cursor, std::numeric_limits<uintptr_t>::max());
        *cursor++ = CmpX(0, 16);
        // If x0 != sentinel, skip over the early-return sequence and continue
        // assembling the target call state. The offset differs by one
        // instruction depending on whether stack cleanup is needed.
        *cursor++ = BCond(frame_size != 0 ? 5 : 4, 1);  // b.ne valid_buffer
        // Conversion failed. Return zero/null directly; the helper has already
        // scheduled the JS exception through FastApiCallbackOptions.
        *cursor++ = MovX(0, 31);
        if (frame_size != 0) {
          *cursor++ = AddSp(frame_size);
        }
        *cursor++ = kLdpFpLrPostIndex;
        *cursor++ = kRet;
        if (target_gp_index != 0) {
          // Helper returns the converted pointer in x0; move it into the ABI
          // register assigned to this target argument if needed.
          *cursor++ = MovX(target_gp_index, 0);
        }
      } else {
        // Non-buffer GP arguments can be restored directly from the spill area.
        *cursor++ = LdrXSp(target_gp_index, arg_offset);
        EmitNarrow(&cursor, args[i], target_gp_index);
      }

      incoming_gp_index++;
      target_gp_index++;
    }
  } else {
    // Scalar-only signatures need no spills: V8's receiver occupies x0, so
    // user GP arguments are shifted down from x1..x7 to x0..x6.
    unsigned gp_index = 0;
    for (size_t i = 0; i < argc; i++) {
      if (!IsGPType(args[i])) {
        // FP arguments are already in v0..v7 and are not shifted by the x0
        // receiver slot, so the scalar path does not need to touch them.
        continue;
      }
      *cursor++ = MovX(gp_index, gp_index + 1);
      EmitNarrow(&cursor, args[i], gp_index);
      gp_index++;
    }
  }

  // Tail of the trampoline: load the actual library symbol address and call it
  // with arguments now arranged according to the native ABI.
  EmitLoadX16(&cursor, reinterpret_cast<uintptr_t>(target));
  *cursor++ = kBlrX16;

  if (has_buffer_args) {
    // Restore the spill frame allocated for buffer conversion before returning
    // through the saved frame pointer/link-register pair.
    const unsigned frame_size = Align16(incoming_gp_count * 8);
    if (frame_size != 0) {
      *cursor++ = AddSp(frame_size);
    }
  }

  *cursor++ = kLdpFpLrPostIndex;
  // Native small integer returns may leave upper bits unspecified. Normalize
  // them before V8 observes the result.
  EmitNarrow(&cursor, result, 0);
  *cursor++ = kRet;

  const size_t written = reinterpret_cast<uint8_t*>(cursor) -
                         static_cast<uint8_t*>(code);

  FlushCode(code, written);

  // Enforce W^X after code generation: the trampoline is executable but no
  // longer writable once published through FastFFITrampoline.
  if (!ProtectCode(code, code_size)) {
    FreeCode(code, code_size);
    return false;
  }

  out->code = code;
  out->size = code_size;
  return true;
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  // Trampoline cleanup is idempotent so metadata destruction can safely run on
  // partially initialized or already-released objects.
  if (trampoline == nullptr || trampoline->code == nullptr) {
    return;
  }
  FreeCode(trampoline->code, trampoline->size);
  trampoline->code = nullptr;
  trampoline->size = 0;
}

#elif !defined(__x86_64__) && !defined(_M_X64) && \
    !defined(__powerpc64__) && !defined(__ppc64__) && \
    !defined(__loongarch64) && \
    !(defined(__riscv) && __riscv_xlen == 64) && !defined(__s390x__)

extern "C" bool node_ffi_create_fast_trampoline(
    void* target,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  // Non-AArch64 platforms have no generated trampoline yet. Returning false
  // lets CreateFastFFIMetadata() fall back cleanly.
  return false;
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  // No code is allocated in the non-AArch64 stub.
}

#endif  // defined(__aarch64__) || defined(_M_ARM64)

#endif  // HAVE_FFI
