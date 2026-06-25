#if HAVE_FFI

#include "ffi/fast.h"

#if defined(__x86_64__) && !defined(_WIN32)

#include <sys/mman.h>
#include <unistd.h>

#include <stdint.h>

#include <limits>

namespace {

using node::ffi::FastFFIType;

// System V x86_64 argument registers in ABI order. V8 Fast API calls enter the
// trampoline with the JS receiver in the first GP slot (rdi), so user GP
// arguments arrive shifted by one register: rsi, rdx, rcx, r8, r9.
constexpr unsigned kRax = 0;
constexpr unsigned kRcx = 1;
constexpr unsigned kRdx = 2;
constexpr unsigned kRsp = 4;
constexpr unsigned kRsi = 6;
constexpr unsigned kRdi = 7;
constexpr unsigned kR8 = 8;
constexpr unsigned kR9 = 9;
constexpr unsigned kR11 = 11;

constexpr unsigned kIncomingGPRegisters[] = {kRsi, kRdx, kRcx, kR8, kR9};
constexpr unsigned kTargetGPRegisters[] = {kRdi, kRsi, kRdx, kRcx, kR8, kR9};

// Floating-point values use xmmN registers in the SysV x86_64 ABI. The receiver
// consumes only a GP register, so FP arguments are already in the target ABI
// locations and the trampoline only needs to shuffle GP arguments.
bool IsFloatType(FastFFIType type) {
  return type == FastFFIType::kFloat32 || type == FastFFIType::kFloat64;
}

// The current scalar implementation only distinguishes FP from non-FP ABI
// classes; non-FP values are carried in GP registers.
bool IsGPType(FastFFIType type) {
  return !IsFloatType(type);
}

// kBuffer means V8 passes the original JS value; the trampoline must call back
// into C++ to convert it to a native pointer before invoking the target.
bool IsPointerValueType(FastFFIType type) {
  return type == FastFFIType::kBuffer;
}

void Emit8(uint8_t** cursor, uint8_t value) {
  *(*cursor)++ = value;
}

void Emit32(uint8_t** cursor, uint32_t value) {
  for (unsigned i = 0; i < 4; i++) {
    Emit8(cursor, (value >> (i * 8)) & 0xff);
  }
}

void Emit64(uint8_t** cursor, uint64_t value) {
  for (unsigned i = 0; i < 8; i++) {
    Emit8(cursor, (value >> (i * 8)) & 0xff);
  }
}

void EmitRex(uint8_t** cursor, bool wide, unsigned reg, unsigned rm) {
  // REX encodes 64-bit operand size and the high bit of the ModRM reg/rm
  // fields. Emit it unconditionally for byte-register operations too so rsi,
  // rdi, and r8+ name their low-byte forms instead of legacy high-byte regs.
  Emit8(cursor, 0x40 | (wide ? 0x08 : 0) | ((reg >> 3) << 2) | (rm >> 3));
}

void EmitModRM(uint8_t** cursor, unsigned reg, unsigned rm) {
  Emit8(cursor, 0xc0 | ((reg & 7) << 3) | (rm & 7));
}

void EmitMov(uint8_t** cursor, unsigned dst, unsigned src) {
  // mov dst, src. Used for the receiver-slot shuffle from V8's GP layout to
  // the target ABI layout, and for placing converted buffer pointers.
  EmitRex(cursor, true, src, dst);
  Emit8(cursor, 0x89);
  EmitModRM(cursor, src, dst);
}

void EmitMovImm64(uint8_t** cursor, unsigned reg, uintptr_t value) {
  // Materialize an absolute address into a scratch register. The generated code
  // uses r11 for helper/target calls and sentinels, avoiding ABI argument regs.
  EmitRex(cursor, true, 0, reg);
  Emit8(cursor, 0xb8 | (reg & 7));
  Emit64(cursor, value);
}

void EmitMovImm32(uint8_t** cursor, unsigned reg, uint32_t value) {
  // Load the buffer helper's uint32_t argument index into edx.
  if (reg >= 8) {
    EmitRex(cursor, false, 0, reg);
  }
  Emit8(cursor, 0xb8 | (reg & 7));
  Emit32(cursor, value);
}

void EmitCall(uint8_t** cursor, unsigned reg) {
  // call *reg. All targets are absolute addresses first loaded into r11.
  EmitRex(cursor, true, 0, reg);
  Emit8(cursor, 0xff);
  EmitModRM(cursor, 2, reg);
}

void EmitJmp(uint8_t** cursor, unsigned reg) {
  // jmp *reg. Scalar signatures that do not need return normalization can tail
  // jump to the native target, avoiding the trampoline's call/return overhead.
  EmitRex(cursor, true, 0, reg);
  Emit8(cursor, 0xff);
  EmitModRM(cursor, 4, reg);
}

bool EmitJmpRel32(uint8_t** cursor, uintptr_t target) {
  // jmp rel32 is shorter than movabs+jmp and avoids an indirect branch. It is
  // only usable when the native target is within the signed 32-bit displacement
  // range from the end of the instruction. mmap() placement is not guaranteed,
  // so callers must keep the absolute-jump fallback.
  constexpr uintptr_t kInstructionSize = 5;
  const uintptr_t next_instruction = reinterpret_cast<uintptr_t>(*cursor) +
                                     kInstructionSize;
  const intptr_t displacement = static_cast<intptr_t>(target) -
                                static_cast<intptr_t>(next_instruction);
  if (displacement < std::numeric_limits<int32_t>::min() ||
      displacement > std::numeric_limits<int32_t>::max()) {
    return false;
  }

  Emit8(cursor, 0xe9);
  Emit32(cursor, static_cast<uint32_t>(static_cast<int32_t>(displacement)));
  return true;
}

void EmitPushRbp(uint8_t** cursor) {
  // At function entry rsp is 8 mod 16. Pushing rbp makes rsp 16-byte aligned so
  // the trampoline can safely call C++ helpers and the native target.
  Emit8(cursor, 0x55);
}

void EmitPopRbp(uint8_t** cursor) {
  Emit8(cursor, 0x5d);
}

void EmitRet(uint8_t** cursor) {
  Emit8(cursor, 0xc3);
}

void EmitSubRsp(uint8_t** cursor, unsigned value) {
  // Reserve stack spill space with `sub rsp, imm32`. The caller keeps the value
  // 16-byte aligned so nested calls preserve the SysV stack alignment rule.
  EmitRex(cursor, true, 5, kRsp);
  Emit8(cursor, 0x81);
  Emit8(cursor, 0xec);
  Emit32(cursor, value);
}

void EmitAddRsp(uint8_t** cursor, unsigned value) {
  // Release stack spill space with `add rsp, imm32`, matching EmitSubRsp().
  EmitRex(cursor, true, 0, kRsp);
  Emit8(cursor, 0x81);
  Emit8(cursor, 0xc4);
  Emit32(cursor, value);
}

void EmitStackMemory(uint8_t** cursor, unsigned reg, unsigned offset) {
  // Address [rsp + offset]. x86_64 requires a SIB byte whenever rsp is the base
  // register; offsets fit in the small fixed spill frame used by this emitter.
  if (offset == 0) {
    Emit8(cursor, ((reg & 7) << 3) | 0x04);
    Emit8(cursor, 0x24);
  } else {
    Emit8(cursor, 0x44 | ((reg & 7) << 3));
    Emit8(cursor, 0x24);
    Emit8(cursor, offset);
  }
}

void EmitStoreRsp(uint8_t** cursor, unsigned reg, unsigned offset) {
  // Store a 64-bit incoming GP register into its spill slot.
  EmitRex(cursor, true, reg, kRsp);
  Emit8(cursor, 0x89);
  EmitStackMemory(cursor, reg, offset);
}

void EmitLoadRsp(uint8_t** cursor, unsigned reg, unsigned offset) {
  // Reload a 64-bit GP register from the spill area.
  EmitRex(cursor, true, reg, kRsp);
  Emit8(cursor, 0x8b);
  EmitStackMemory(cursor, reg, offset);
}

void EmitLoadCallerStack(uint8_t** cursor, unsigned reg, unsigned offset) {
  // Reload a stack-passed incoming argument from the caller's frame. With no
  // trampoline prologue, the first stack argument is at [rsp + 8] because
  // [rsp] holds the return address. If the trampoline pushed rbp first, callers
  // pass offset 16 instead.
  EmitLoadRsp(cursor, reg, offset);
}

void EmitCmp(uint8_t** cursor, unsigned lhs, unsigned rhs) {
  // cmp lhs, rhs. Used to compare the helper result in rax with the invalid
  // pointer sentinel loaded into r11.
  EmitRex(cursor, true, rhs, lhs);
  Emit8(cursor, 0x39);
  EmitModRM(cursor, rhs, lhs);
}

void EmitXorEax(uint8_t** cursor) {
  // Return nullptr/zero after the helper has already scheduled the JS exception.
  Emit8(cursor, 0x31);
  Emit8(cursor, 0xc0);
}

void EmitNarrowInstruction(uint8_t** cursor, uint8_t opcode, unsigned reg) {
  // movsx/movzx reg32, reg8/reg16. Writing the 32-bit destination also clears
  // the upper 32 bits, which is the ABI-normal form V8 observes.
  EmitRex(cursor, false, reg, reg);
  Emit8(cursor, 0x0f);
  Emit8(cursor, opcode);
  EmitModRM(cursor, reg, reg);
}

// Native small integer returns may leave upper bits unspecified. Normalize them
// before V8 observes the result. Arguments do not need the same treatment: the
// C ABI passes narrow integer arguments in integer registers and the callee
// consumes the declared low 8/16 bits.
bool EmitNarrowReturn(uint8_t** cursor, FastFFIType type, unsigned reg) {
  switch (type) {
    case FastFFIType::kBool:
    case FastFFIType::kUint8:
      EmitNarrowInstruction(cursor, 0xb6, reg);
      return true;
    case FastFFIType::kInt8:
      EmitNarrowInstruction(cursor, 0xbe, reg);
      return true;
    case FastFFIType::kUint16:
      EmitNarrowInstruction(cursor, 0xb7, reg);
      return true;
    case FastFFIType::kInt16:
      EmitNarrowInstruction(cursor, 0xbf, reg);
      return true;
    default:
      return false;
  }
}

bool NeedsNarrow(FastFFIType type) {
  switch (type) {
    case FastFFIType::kBool:
    case FastFFIType::kUint8:
    case FastFFIType::kInt8:
    case FastFFIType::kUint16:
    case FastFFIType::kInt16:
      return true;
    default:
      return false;
  }
}

// Stack adjustment must preserve the platform's 16-byte SP alignment rule.
unsigned Align16(unsigned value) {
  return (value + 15) & ~15;
}

void* AllocationHintNear(uintptr_t address) {
  // A plain mmap hint is advisory: the kernel may ignore it, but unlike
  // MAP_FIXED it will not replace an existing mapping. Keeping the trampoline
  // close to the native target lets the scalar tail path use jmp rel32 instead
  // of an absolute indirect jump on common Linux layouts.
  const uintptr_t page_size = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
  if (page_size == 0) {
    return nullptr;
  }
  return reinterpret_cast<void*>(address & ~(page_size - 1));
}

void* AllocateCodeNear(uintptr_t target_address, size_t code_size) {
#if defined(__linux__) && defined(MAP_FIXED_NOREPLACE)
  const uintptr_t page_size = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));
  if (page_size != 0) {
    const uintptr_t base = target_address & ~(page_size - 1);
    // Search a small window around the target first. Shared libraries usually
    // leave nearby holes, and keeping the trampoline close enables jmp rel32.
    constexpr uintptr_t kMaxPages = 1024;
    for (uintptr_t i = 1; i <= kMaxPages; i++) {
      const uintptr_t delta = i * page_size;
      const uintptr_t candidates[] = {
          base + delta,
          base >= delta ? base - delta : 0,
      };
      for (uintptr_t candidate : candidates) {
        if (candidate == 0) {
          continue;
        }
        const intptr_t displacement = static_cast<intptr_t>(target_address) -
                                      static_cast<intptr_t>(candidate + 5);
        if (displacement < std::numeric_limits<int32_t>::min() ||
            displacement > std::numeric_limits<int32_t>::max()) {
          continue;
        }
        void* code = mmap(reinterpret_cast<void*>(candidate),
                          code_size,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANON | MAP_FIXED_NOREPLACE,
                          -1,
                          0);
        if (code != MAP_FAILED) {
          return code;
        }
      }
    }
  }
#endif

  return mmap(AllocationHintNear(target_address),
              code_size,
              PROT_READ | PROT_WRITE,
              MAP_PRIVATE | MAP_ANON,
              -1,
              0);
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

  // Buffer conversion calls into C++ before invoking the target. Keep the first
  // implementation scalar-only so no XMM argument registers need to be preserved
  // across helper calls.
  if (has_buffer_args && fp_count != 0) {
    return false;
  }

  const size_t incoming_gp_count = gp_count + (has_buffer_args ? 1 : 0);
  // rdi is occupied by the V8 receiver, leaving rsi, rdx, rcx, r8, and r9 for
  // incoming GP args/options. Scalar signatures can load one more user argument
  // from the caller stack into the target's sixth GP slot; buffer signatures
  // still stay register-only because the helper path spills the incoming window.
  const size_t max_incoming_gp_count = has_buffer_args ? 5 : 6;
  if (incoming_gp_count > max_incoming_gp_count || fp_count > 8) {
    return false;
  }

  constexpr size_t kCodeSize = 1024;
  // The fixed allocation is intentionally larger than any currently emitted
  // path. It keeps the generator simple while the supported signature set is
  // small and bounded.
  // Generate into writable anonymous memory first; the page is made executable
  // only after the instruction stream is complete and the instruction cache is
  // synchronized.
  const uintptr_t target_address = reinterpret_cast<uintptr_t>(target);
  void* code = AllocateCodeNear(target_address, kCodeSize);
  if (code == MAP_FAILED) {
    return false;
  }

  uint8_t* cursor = static_cast<uint8_t*>(code);
  const bool tail_call = !has_buffer_args && !NeedsNarrow(result);
  if (!tail_call) {
    EmitPushRbp(&cursor);
  }

  if (has_buffer_args) {
    // Buffer conversion calls a C++ helper before the target call, so spill all
    // incoming GP registers that may be clobbered by that helper.
    const unsigned frame_size = Align16(incoming_gp_count * 8);
    if (frame_size != 0) {
      EmitSubRsp(&cursor, frame_size);
    }

    for (unsigned i = 0; i < incoming_gp_count; i++) {
      // V8 presents user GP args in rsi.. and the callback options pointer
      // after them. Spill that whole incoming register window at stack slot i.
      EmitStoreRsp(&cursor, kIncomingGPRegisters[i], i * 8);
    }

    unsigned incoming_gp_index = 0;
    unsigned target_gp_index = 0;
    const unsigned options_offset = gp_count * 8;
    // `incoming_gp_index` walks spilled V8 argument slots; `target_gp_index`
    // walks the ABI registers expected by the native target. They differ only
    // because V8 has an implicit receiver in rdi that the target does not see.
    // Reload each original argument in target ABI order. Buffer arguments are
    // converted to raw pointers; scalar GP arguments are moved/narrowed.
    for (size_t i = 0; i < argc; i++) {
      if (!IsGPType(args[i])) {
        continue;
      }

      const unsigned arg_offset = incoming_gp_index * 8;
      const unsigned target_reg = kTargetGPRegisters[target_gp_index];
      if (IsPointerValueType(args[i])) {
        // Call node_ffi_fast_buffer_data(value, options, index). A sentinel
        // return means the helper already threw, so return nullptr/zero without
        // invoking the native target.
        EmitLoadRsp(&cursor, kRdi, arg_offset);
        EmitLoadRsp(&cursor, kRsi, options_offset);
        EmitMovImm32(&cursor, kRdx, static_cast<uint32_t>(i));
        EmitMovImm64(&cursor,
                     kR11,
                     reinterpret_cast<uintptr_t>(node_ffi_fast_buffer_data));
        // call *r11 clobbers caller-saved registers. The original arguments were
        // spilled above specifically so subsequent target args can be restored.
        EmitCall(&cursor, kR11);
        EmitMovImm64(&cursor, kR11, std::numeric_limits<uintptr_t>::max());
        EmitCmp(&cursor, kRax, kR11);
        // If rax != sentinel, skip over the early-return sequence and continue
        // assembling the target call state.
        uint8_t* jne = cursor;
        Emit8(&cursor, 0x75);  // jne valid_buffer
        Emit8(&cursor, 0);
        // Conversion failed. Return zero/null directly; the helper has already
        // scheduled the JS exception through FastApiCallbackOptions.
        EmitXorEax(&cursor);
        if (frame_size != 0) {
          EmitAddRsp(&cursor, frame_size);
        }
        EmitPopRbp(&cursor);
        EmitRet(&cursor);
        jne[1] = static_cast<uint8_t>(cursor - (jne + 2));
        if (target_reg != kRax) {
          // Helper returns the converted pointer in rax; move it into the ABI
          // register assigned to this target argument if needed.
          EmitMov(&cursor, target_reg, kRax);
        }
      } else {
        // Non-buffer GP arguments can be restored directly from the spill area.
        EmitLoadRsp(&cursor, target_reg, arg_offset);
      }

      incoming_gp_index++;
      target_gp_index++;
    }
  } else {
    // Scalar-only signatures need no spills: V8's receiver occupies rdi, so
    // user GP arguments are shifted down from rsi..r9 to rdi..r8.
    unsigned gp_index = 0;
    for (size_t i = 0; i < argc; i++) {
      if (!IsGPType(args[i])) {
        // FP arguments are already in xmm0..xmm7 and are not shifted by the rdi
        // receiver slot, so the scalar path does not need to touch them.
        continue;
      }
      if (gp_index < 5) {
        EmitMov(&cursor,
                kTargetGPRegisters[gp_index],
                kIncomingGPRegisters[gp_index]);
      } else {
        // The sixth user GP argument is stack-passed by V8 because the receiver
        // consumed rdi. Load it into r9, the target ABI's sixth GP register.
        EmitLoadCallerStack(&cursor,
                            kTargetGPRegisters[gp_index],
                            tail_call ? 8 : 16);
      }
      gp_index++;
    }
  }

  // Tail of the trampoline: load the actual library symbol address and call it
  // with arguments now arranged according to the native ABI.
  if (tail_call) {
    // The trampoline has not touched rsp in this path, so a tail jump preserves
    // the exact stack shape the native target expects after a normal call.
    if (!EmitJmpRel32(&cursor, target_address)) {
      EmitMovImm64(&cursor, kR11, target_address);
      EmitJmp(&cursor, kR11);
    }
  } else {
    EmitMovImm64(&cursor, kR11, target_address);
    EmitCall(&cursor, kR11);
  }

  if (has_buffer_args) {
    // Restore the spill frame allocated for buffer conversion before returning.
    const unsigned frame_size = Align16(incoming_gp_count * 8);
    if (frame_size != 0) {
      EmitAddRsp(&cursor, frame_size);
    }
  }

  EmitPopRbp(&cursor);
  // Native small integer returns may leave upper bits unspecified. Normalize
  // them before V8 observes the result.
  EmitNarrowReturn(&cursor, result, kRax);
  EmitRet(&cursor);

  const size_t written = cursor - static_cast<uint8_t*>(code);
  __builtin___clear_cache(static_cast<char*>(code),
                          static_cast<char*>(code) + written);

  // Enforce W^X after code generation: the trampoline is executable but no
  // longer writable once published through FastFFITrampoline.
  if (mprotect(code, kCodeSize, PROT_READ | PROT_EXEC) != 0) {
    munmap(code, kCodeSize);
    return false;
  }

  out->code = code;
  out->size = kCodeSize;
  return true;
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  // Trampoline cleanup is idempotent so metadata destruction can safely run on
  // partially initialized or already-released objects.
  if (trampoline == nullptr || trampoline->code == nullptr) {
    return;
  }
  munmap(trampoline->code, trampoline->size);
  trampoline->code = nullptr;
  trampoline->size = 0;
}

#elif defined(_M_X64)

#include <windows.h>

#include <stdint.h>

namespace {

using node::ffi::FastFFIType;

constexpr unsigned kRax = 0;
constexpr unsigned kRcx = 1;
constexpr unsigned kRdx = 2;
constexpr unsigned kRsp = 4;
constexpr unsigned kR8 = 8;
constexpr unsigned kR9 = 9;
constexpr unsigned kR11 = 11;

constexpr unsigned kWin64GPRegisters[] = {kRcx, kRdx, kR8, kR9};

bool IsFloatType(FastFFIType type) {
  return type == FastFFIType::kFloat32 || type == FastFFIType::kFloat64;
}

bool IsBufferType(FastFFIType type) {
  return type == FastFFIType::kBuffer;
}

void Emit8(uint8_t** cursor, uint8_t value) {
  *(*cursor)++ = value;
}

void Emit32(uint8_t** cursor, uint32_t value) {
  for (unsigned i = 0; i < 4; i++) {
    Emit8(cursor, (value >> (i * 8)) & 0xff);
  }
}

void Emit64(uint8_t** cursor, uint64_t value) {
  for (unsigned i = 0; i < 8; i++) {
    Emit8(cursor, (value >> (i * 8)) & 0xff);
  }
}

void EmitRex(uint8_t** cursor, bool wide, unsigned reg, unsigned rm) {
  Emit8(cursor, 0x40 | (wide ? 0x08 : 0) | ((reg >> 3) << 2) | (rm >> 3));
}

void EmitModRM(uint8_t** cursor, unsigned reg, unsigned rm) {
  Emit8(cursor, 0xc0 | ((reg & 7) << 3) | (rm & 7));
}

void EmitMov(uint8_t** cursor, unsigned dst, unsigned src) {
  EmitRex(cursor, true, src, dst);
  Emit8(cursor, 0x89);
  EmitModRM(cursor, src, dst);
}

void EmitMovaps(uint8_t** cursor, unsigned dst, unsigned src) {
  // movaps xmm_dst, xmm_src. Used only for register-to-register argument
  // shuffles; it preserves the payload bits for both f32 and f64 arguments.
  EmitRex(cursor, false, dst, src);
  Emit8(cursor, 0x0f);
  Emit8(cursor, 0x28);
  EmitModRM(cursor, dst, src);
}

void EmitMovImm64(uint8_t** cursor, unsigned reg, uintptr_t value) {
  EmitRex(cursor, true, 0, reg);
  Emit8(cursor, 0xb8 | (reg & 7));
  Emit64(cursor, value);
}

void EmitCall(uint8_t** cursor, unsigned reg) {
  EmitRex(cursor, true, 0, reg);
  Emit8(cursor, 0xff);
  EmitModRM(cursor, 2, reg);
}

void EmitJmp(uint8_t** cursor, unsigned reg) {
  EmitRex(cursor, true, 0, reg);
  Emit8(cursor, 0xff);
  EmitModRM(cursor, 4, reg);
}

void EmitSubRsp(uint8_t** cursor, unsigned value) {
  EmitRex(cursor, true, 5, kRsp);
  Emit8(cursor, 0x81);
  Emit8(cursor, 0xec);
  Emit32(cursor, value);
}

void EmitAddRsp(uint8_t** cursor, unsigned value) {
  EmitRex(cursor, true, 0, kRsp);
  Emit8(cursor, 0x81);
  Emit8(cursor, 0xc4);
  Emit32(cursor, value);
}

void EmitRet(uint8_t** cursor) {
  Emit8(cursor, 0xc3);
}

void EmitNarrowInstruction(uint8_t** cursor, uint8_t opcode, unsigned reg) {
  EmitRex(cursor, false, reg, reg);
  Emit8(cursor, 0x0f);
  Emit8(cursor, opcode);
  EmitModRM(cursor, reg, reg);
}

bool EmitNarrowReturn(uint8_t** cursor, FastFFIType type, unsigned reg) {
  switch (type) {
    case FastFFIType::kBool:
    case FastFFIType::kUint8:
      EmitNarrowInstruction(cursor, 0xb6, reg);
      return true;
    case FastFFIType::kInt8:
      EmitNarrowInstruction(cursor, 0xbe, reg);
      return true;
    case FastFFIType::kUint16:
      EmitNarrowInstruction(cursor, 0xb7, reg);
      return true;
    case FastFFIType::kInt16:
      EmitNarrowInstruction(cursor, 0xbf, reg);
      return true;
    default:
      return false;
  }
}

bool NeedsNarrow(FastFFIType type) {
  switch (type) {
    case FastFFIType::kBool:
    case FastFFIType::kUint8:
    case FastFFIType::kInt8:
    case FastFFIType::kUint16:
    case FastFFIType::kInt16:
      return true;
    default:
      return false;
  }
}

void FreeCode(void* code, size_t code_size) {
  VirtualFree(code, 0, MEM_RELEASE);
}

}  // namespace

extern "C" bool node_ffi_create_fast_trampoline(
    void* target,
    const node::ffi::FastFFIType* args,
    size_t argc,
    node::ffi::FastFFIType result,
    node::ffi::FastFFITrampoline* out) {
  if (target == nullptr || out == nullptr || argc > 3) {
    return false;
  }

  for (size_t i = 0; i < argc; i++) {
    if (IsBufferType(args[i])) {
      return false;
    }
  }

  constexpr size_t kCodeSize = 512;
  void* code = VirtualAlloc(
      nullptr, kCodeSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (code == nullptr) {
    return false;
  }

  uint8_t* cursor = static_cast<uint8_t*>(code);
  const bool tail_call = !NeedsNarrow(result);

  // Win64 uses positional registers. The V8 receiver occupies position 0, so
  // public arguments arrive in positions 1..3 and must be shifted down.
  for (size_t i = 0; i < argc; i++) {
    if (IsFloatType(args[i])) {
      EmitMovaps(&cursor, static_cast<unsigned>(i), static_cast<unsigned>(i + 1));
    } else {
      EmitMov(&cursor, kWin64GPRegisters[i], kWin64GPRegisters[i + 1]);
    }
  }

  EmitMovImm64(&cursor, kR11, reinterpret_cast<uintptr_t>(target));
  if (tail_call) {
    // The caller already provided Win64 shadow space for the trampoline; after
    // the receiver-slot shuffle, the target can reuse the same stack shape.
    EmitJmp(&cursor, kR11);
  } else {
    // Reserve 32 bytes of shadow space plus 8 bytes for 16-byte stack alignment
    // before making a nested call from inside the trampoline.
    EmitSubRsp(&cursor, 40);
    EmitCall(&cursor, kR11);
    EmitAddRsp(&cursor, 40);
    EmitNarrowReturn(&cursor, result, kRax);
    EmitRet(&cursor);
  }

  const size_t written = cursor - static_cast<uint8_t*>(code);
  FlushInstructionCache(GetCurrentProcess(), code, written);

  DWORD old_protect;
  if (VirtualProtect(code, kCodeSize, PAGE_EXECUTE_READ, &old_protect) == 0) {
    FreeCode(code, kCodeSize);
    return false;
  }

  out->code = code;
  out->size = kCodeSize;
  return true;
}

extern "C" void node_ffi_free_fast_trampoline(
    node::ffi::FastFFITrampoline* trampoline) {
  if (trampoline == nullptr || trampoline->code == nullptr) {
    return;
  }
  FreeCode(trampoline->code, trampoline->size);
  trampoline->code = nullptr;
  trampoline->size = 0;
}

#endif  // defined(_M_X64)

#endif  // HAVE_FFI
