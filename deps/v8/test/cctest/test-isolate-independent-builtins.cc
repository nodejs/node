// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "src/assembler-inl.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/macro-assembler-inl.h"
#include "src/simulator.h"
#include "src/snapshot/snapshot.h"

// To generate the binary files for the test function, enable this section and
// run GenerateTestFunctionData once on each arch.
#define GENERATE_TEST_FUNCTION_DATA false

namespace v8 {
namespace internal {
namespace test_isolate_independent_builtins {

#ifdef V8_EMBEDDED_BUILTINS
TEST(VerifyBuiltinsIsolateIndependence) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  Snapshot::EnsureAllBuiltinsAreDeserialized(isolate);

  // Build a white-list of all isolate-independent RelocInfo entry kinds.
  constexpr int all_real_modes_mask =
      (1 << (RelocInfo::LAST_REAL_RELOC_MODE + 1)) - 1;
  constexpr int mode_mask =
      all_real_modes_mask & ~RelocInfo::ModeMask(RelocInfo::COMMENT) &
      ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) &
      ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) &
      ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
      ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::LAST_REAL_RELOC_MODE == RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::ModeMask(RelocInfo::COMMENT) ==
                (1 << RelocInfo::COMMENT));
  STATIC_ASSERT(
      mode_mask ==
      (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
       RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
       RelocInfo::ModeMask(RelocInfo::WASM_CONTEXT_REFERENCE) |
       RelocInfo::ModeMask(RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE) |
       RelocInfo::ModeMask(RelocInfo::WASM_GLOBAL_HANDLE) |
       RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
       RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL) |
       RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY) |
       RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE)));

  constexpr bool kVerbose = false;
  bool found_mismatch = false;
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code* code = isolate->builtins()->builtin(i);

    if (kVerbose) {
      printf("%s %s\n", Builtins::KindNameOf(i), isolate->builtins()->name(i));
    }

    bool is_isolate_independent = true;
    for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
      is_isolate_independent = false;

#ifdef ENABLE_DISASSEMBLER
      if (kVerbose) {
        RelocInfo::Mode mode = it.rinfo()->rmode();
        printf("  %s\n", RelocInfo::RelocModeName(mode));
      }
#endif
    }

    const bool expected_result = Builtins::IsIsolateIndependent(i);
    if (is_isolate_independent != expected_result) {
      found_mismatch = true;
      printf("%s %s expected: %d, is: %d\n", Builtins::KindNameOf(i),
             isolate->builtins()->name(i), expected_result,
             is_isolate_independent);
    }
  }

  CHECK(!found_mismatch);
}

TEST(VerifyBuiltinsOffHeapSafety) {
  Isolate* isolate = CcTest::i_isolate();
  HandleScope handle_scope(isolate);

  Snapshot::EnsureAllBuiltinsAreDeserialized(isolate);

  constexpr int all_real_modes_mask =
      (1 << (RelocInfo::LAST_REAL_RELOC_MODE + 1)) - 1;
  constexpr int mode_mask =
      all_real_modes_mask & ~RelocInfo::ModeMask(RelocInfo::COMMENT) &
      ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) &
      ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) &
      ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
      ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL) &
      ~RelocInfo::ModeMask(RelocInfo::EXTERNAL_REFERENCE);
  STATIC_ASSERT(RelocInfo::LAST_REAL_RELOC_MODE == RelocInfo::VENEER_POOL);
  STATIC_ASSERT(RelocInfo::ModeMask(RelocInfo::COMMENT) ==
                (1 << RelocInfo::COMMENT));
  STATIC_ASSERT(
      mode_mask ==
      (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
       RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
       RelocInfo::ModeMask(RelocInfo::WASM_CONTEXT_REFERENCE) |
       RelocInfo::ModeMask(RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE) |
       RelocInfo::ModeMask(RelocInfo::WASM_GLOBAL_HANDLE) |
       RelocInfo::ModeMask(RelocInfo::WASM_CALL) |
       RelocInfo::ModeMask(RelocInfo::JS_TO_WASM_CALL) |
       RelocInfo::ModeMask(RelocInfo::RUNTIME_ENTRY)));

  constexpr bool kVerbose = false;
  bool found_mismatch = false;
  for (int i = 0; i < Builtins::builtin_count; i++) {
    Code* code = isolate->builtins()->builtin(i);

    if (kVerbose) {
      printf("%s %s\n", Builtins::KindNameOf(i), isolate->builtins()->name(i));
    }

    bool is_off_heap_safe = true;
    for (RelocIterator it(code, mode_mask); !it.done(); it.next()) {
      is_off_heap_safe = false;
#ifdef ENABLE_DISASSEMBLER
      if (kVerbose) {
        RelocInfo::Mode mode = it.rinfo()->rmode();
        printf("  %s\n", RelocInfo::RelocModeName(mode));
      }
#endif
    }

    // TODO(jgruber): Remove once we properly set up the on-heap code
    // trampoline.
    if (Builtins::IsTooShortForOffHeapTrampoline(i)) is_off_heap_safe = false;

    const bool expected_result = Builtins::IsOffHeapSafe(i);
    if (is_off_heap_safe != expected_result) {
      found_mismatch = true;
      printf("%s %s expected: %d, is: %d\n", Builtins::KindNameOf(i),
             isolate->builtins()->name(i), expected_result, is_off_heap_safe);
    }
  }

  CHECK(!found_mismatch);
}
#endif  // V8_EMBEDDED_BUILTINS

// V8_CC_MSVC is true for both MSVC and clang on windows. clang can handle
// __asm__-style inline assembly but MSVC cannot, and thus we need a more
// precise compiler detection that can distinguish between the two. clang on
// windows sets both __clang__ and _MSC_VER, MSVC sets only _MSC_VER.
#if defined(_MSC_VER) && !defined(__clang__)
#define V8_COMPILER_IS_MSVC
#endif

#ifndef V8_COMPILER_IS_MSVC
#if GENERATE_TEST_FUNCTION_DATA

// Arch-specific defines.
#if V8_TARGET_ARCH_IA32
#define TEST_FUNCTION_FILE "f-ia32.bin"
#elif V8_TARGET_ARCH_X64 && _WIN64
#define TEST_FUNCTION_FILE "f-x64-win.bin"
#elif V8_TARGET_ARCH_X64
#define TEST_FUNCTION_FILE "f-x64.bin"
#elif V8_TARGET_ARCH_ARM64
#define TEST_FUNCTION_FILE "f-arm64.bin"
#elif V8_TARGET_ARCH_ARM
#define TEST_FUNCTION_FILE "f-arm.bin"
#elif V8_TARGET_ARCH_PPC
#define TEST_FUNCTION_FILE "f-ppc.bin"
#elif V8_TARGET_ARCH_MIPS
#define TEST_FUNCTION_FILE "f-mips.bin"
#elif V8_TARGET_ARCH_MIPS64
#define TEST_FUNCTION_FILE "f-mips64.bin"
#elif V8_TARGET_ARCH_S390
#define TEST_FUNCTION_FILE "f-s390.bin"
#else
#error "Unknown architecture."
#endif

#define __ masm.

TEST(GenerateTestFunctionData) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  HandleScope scope(isolate);

#if V8_TARGET_ARCH_IA32
  v8::internal::byte buffer[256];
  Assembler masm(isolate, buffer, sizeof(buffer));

  __ mov(eax, Operand(esp, 4));
  __ add(eax, Operand(esp, 8));
  __ ret(0);
#elif V8_TARGET_ARCH_X64
  size_t allocated;
  byte* buffer = AllocateAssemblerBuffer(&allocated);
  Assembler masm(isolate, buffer, static_cast<int>(allocated));

#ifdef _WIN64
  static const Register arg1 = rcx;
  static const Register arg2 = rdx;
#else
  static const Register arg1 = rdi;
  static const Register arg2 = rsi;
#endif

  __ movq(rax, arg2);
  __ addq(rax, arg1);
  __ ret(0);
#elif V8_TARGET_ARCH_ARM64
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);

  __ Add(x0, x0, x1);
  __ Ret();
#elif V8_TARGET_ARCH_ARM
  Assembler masm(isolate, nullptr, 0);

  __ add(r0, r0, Operand(r1));
  __ mov(pc, Operand(lr));
#elif V8_TARGET_ARCH_PPC
  Assembler masm(isolate, nullptr, 0);

  __ function_descriptor();
  __ add(r3, r3, r4);
  __ blr();
#elif V8_TARGET_ARCH_MIPS
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);

  __ addu(v0, a0, a1);
  __ jr(ra);
  __ nop();
#elif V8_TARGET_ARCH_MIPS64
  MacroAssembler masm(isolate, nullptr, 0,
                      v8::internal::CodeObjectRequired::kYes);

  __ addu(v0, a0, a1);
  __ jr(ra);
  __ nop();
#elif V8_TARGET_ARCH_S390
  Assembler masm(isolate, nullptr, 0);

  __ agr(r2, r3);
  __ b(r14);
#else  // Unknown architecture.
#error "Unknown architecture."
#endif  // Target architecture.

  CodeDesc desc;
  masm.GetCode(isolate, &desc);

  std::ofstream of(TEST_FUNCTION_FILE, std::ios::out | std::ios::binary);
  of.write(reinterpret_cast<char*>(desc.buffer), desc.instr_size);
}
#endif  // GENERATE_TEST_FUNCTION_DATA

#if V8_TARGET_ARCH_IA32
#define FUNCTION_BYTES \
  ".byte 0x8b, 0x44, 0x24, 0x04, 0x03, 0x44, 0x24, 0x08, 0xc3\n"
#elif V8_TARGET_ARCH_X64 && _WIN64
#define FUNCTION_BYTES ".byte 0x48, 0x8b, 0xc2, 0x48, 0x03, 0xc1, 0xc3\n"
#elif V8_TARGET_ARCH_X64
#define FUNCTION_BYTES ".byte 0x48, 0x8b, 0xc6, 0x48, 0x03, 0xc7, 0xc3\n"
#elif V8_TARGET_ARCH_ARM64
#define FUNCTION_BYTES ".byte 0x00, 0x00, 0x01, 0x8b, 0xc0, 0x03, 0x5f, 0xd6\n"
#elif V8_TARGET_ARCH_ARM
#define FUNCTION_BYTES ".byte 0x01, 0x00, 0x80, 0xe0, 0x0e, 0xf0, 0xa0, 0xe1\n"
#elif V8_TARGET_ARCH_PPC
#define FUNCTION_BYTES ".byte 0x14, 0x22, 0x63, 0x7c, 0x20, 0x00, 0x80, 0x4e\n"
#elif V8_TARGET_ARCH_MIPS
#define FUNCTION_BYTES                               \
  ".byte 0x21, 0x10, 0x85, 0x00, 0x08, 0x00, 0xe0, " \
  "0x03, 0x00, 0x00, 0x00, 0x00\n"
#elif V8_TARGET_ARCH_MIPS64
#define FUNCTION_BYTES                               \
  ".byte 0x21, 0x10, 0x85, 0x00, 0x08, 0x00, 0xe0, " \
  "0x03, 0x00, 0x00, 0x00, 0x00\n"
#elif V8_TARGET_ARCH_S390
#define FUNCTION_BYTES                               \
  ".byte 0xb9, 0x08, 0x00, 0x23, 0x07, 0xfe\n"
#else
#error "Unknown architecture."
#endif

// .byte macros to handle small differences across operating systems.

#if defined(V8_OS_MACOSX)
#define ASM_RODATA_SECTION ".const_data\n"
#define ASM_TEXT_SECTION ".text\n"
#define ASM_MANGLE_LABEL "_"
#define ASM_GLOBAL(NAME) ".globl " ASM_MANGLE_LABEL NAME "\n"
#elif defined(V8_OS_WIN)
#define ASM_RODATA_SECTION ".section .rodata\n"
#define ASM_TEXT_SECTION ".section .text\n"
#if defined(V8_TARGET_ARCH_X64)
#define ASM_MANGLE_LABEL ""
#else
#define ASM_MANGLE_LABEL "_"
#endif
#define ASM_GLOBAL(NAME) ".global " ASM_MANGLE_LABEL NAME "\n"
#else
#define ASM_RODATA_SECTION ".section .rodata\n"
#define ASM_TEXT_SECTION ".section .text\n"
#define ASM_MANGLE_LABEL ""
#define ASM_GLOBAL(NAME) ".global " ASM_MANGLE_LABEL NAME "\n"
#endif

// clang-format off
#define EMBED_IN_RODATA_HEADER(LABEL)    \
  __asm__(ASM_RODATA_SECTION             \
          ASM_GLOBAL(#LABEL)             \
          ".balign 16\n"                 \
          ASM_MANGLE_LABEL #LABEL ":\n");

#define EMBED_IN_TEXT_HEADER(LABEL)        \
    __asm__(ASM_TEXT_SECTION               \
            ASM_GLOBAL(#LABEL)             \
            ".balign 16\n"                 \
            ASM_MANGLE_LABEL #LABEL ":\n");

EMBED_IN_RODATA_HEADER(test_string0_bytes)
__asm__(".byte 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37\n"
        ".byte 0x38, 0x39, 0x0a, 0x00\n");
extern "C" V8_ALIGNED(16) const char test_string0_bytes[];

EMBED_IN_TEXT_HEADER(test_function0_bytes)
__asm__(FUNCTION_BYTES);
extern "C" V8_ALIGNED(16) const char test_function0_bytes[];
// clang-format on

// A historical note: We use .byte over .incbin since the latter leads to
// complications involving generation of build-time dependencies.  Goma parses
// #include statements, and clang has -MD/-MMD. Neither recognize .incbin.

TEST(ByteInRodata) {
  CHECK_EQ(0, std::strcmp("0123456789\n", test_string0_bytes));
}

TEST(ByteInText) {
  CcTest::InitializeVM();
  Isolate* isolate = CcTest::i_isolate();
  auto f = GeneratedCode<int(int, int)>::FromAddress(
      isolate, const_cast<char*>(test_function0_bytes));
  CHECK_EQ(7, f.Call(3, 4));
  CHECK_EQ(11, f.Call(5, 6));
}
#endif  // #ifndef V8_COMPILER_IS_MSVC
#undef V8_COMPILER_IS_MSVC

#undef __
#undef ASM_GLOBAL
#undef ASM_MANGLE_LABEL
#undef ASM_RODATA_SECTION
#undef ASM_TEXT_SECTION
#undef EMBED_IN_RODATA_HEADER
#undef EMBED_IN_TEXT_HEADER
#undef FUNCTION_BYTES
#undef GENERATE_TEST_FUNCTION_DATA
#undef TEST_FUNCTION_FILE

}  // namespace test_isolate_independent_builtins
}  // namespace internal
}  // namespace v8
