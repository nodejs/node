// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "src/assembler-inl.h"
#include "src/handles-inl.h"
#include "src/isolate.h"
#include "src/macro-assembler-inl.h"
#include "src/simulator.h"
#include "src/snapshot/macros.h"
#include "src/snapshot/snapshot.h"

// To generate the binary files for the test function, enable this section and
// run GenerateTestFunctionData once on each arch.
#define GENERATE_TEST_FUNCTION_DATA false

namespace v8 {
namespace internal {
namespace test_isolate_independent_builtins {

#ifdef V8_EMBEDDED_BUILTINS
UNINITIALIZED_TEST(VerifyBuiltinsIsolateIndependence) {
  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator = CcTest::array_buffer_allocator();
  v8::Isolate* v8_isolate = v8::Isolate::New(create_params);

  {
    v8::Isolate::Scope isolate_scope(v8_isolate);
    v8::internal::Isolate* isolate =
        reinterpret_cast<v8::internal::Isolate*>(v8_isolate);
    HandleScope handle_scope(isolate);

    Snapshot::EnsureAllBuiltinsAreDeserialized(isolate);

    // TODO(jgruber,v8:6666): Investigate CONST_POOL and VENEER_POOL kinds.
    // CONST_POOL is currently relevant on {arm,arm64,mips,mips64,ppc,s390}.
    //            Rumors are it will also become relevant on x64. My
    //            understanding is that we should be fine if we ensure it
    //            doesn't contain heap constants and we use pc-relative
    //            addressing.
    // VENEER_POOL is arm64-only. From what I've seen, jumps are pc-relative
    //             and stay within the same code object and thus should be
    //             isolate-independent.

    // Build a white-list of all isolate-independent RelocInfo entry kinds.
    constexpr int all_real_modes_mask =
        (1 << (RelocInfo::LAST_REAL_RELOC_MODE + 1)) - 1;
    constexpr int mode_mask =
        all_real_modes_mask & ~RelocInfo::ModeMask(RelocInfo::COMMENT) &
        ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE) &
        ~RelocInfo::ModeMask(RelocInfo::INTERNAL_REFERENCE_ENCODED) &
        ~RelocInfo::ModeMask(RelocInfo::OFF_HEAP_TARGET) &
        ~RelocInfo::ModeMask(RelocInfo::CONST_POOL) &
        ~RelocInfo::ModeMask(RelocInfo::VENEER_POOL);
    STATIC_ASSERT(RelocInfo::LAST_REAL_RELOC_MODE == RelocInfo::VENEER_POOL);
    STATIC_ASSERT(RelocInfo::ModeMask(RelocInfo::COMMENT) ==
                  (1 << RelocInfo::COMMENT));
    STATIC_ASSERT(
        mode_mask ==
        (RelocInfo::ModeMask(RelocInfo::CODE_TARGET) |
         RelocInfo::ModeMask(RelocInfo::EMBEDDED_OBJECT) |
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
        printf("%s %s\n", Builtins::KindNameOf(i),
               isolate->builtins()->name(i));
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

      // Relaxed condition only checks whether the isolate-independent list is
      // valid, not whether it is complete. This is to avoid constant work
      // updating the list.
      bool should_be_isolate_independent = Builtins::IsIsolateIndependent(i);
      if (should_be_isolate_independent && !is_isolate_independent) {
        found_mismatch = true;
        printf("%s %s expected: %d, is: %d\n", Builtins::KindNameOf(i),
               isolate->builtins()->name(i), should_be_isolate_independent,
               is_isolate_independent);
      }
    }

    CHECK(!found_mismatch);
  }

  v8_isolate->Dispose();
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
#undef __
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
#if defined(V8_OS_AIX)
#define FUNCTION_BYTES ".byte 0x7c, 0x64, 0x1a, 0x14, 0x4e, 0x80, 0x00, 0x20\n"
#else
#define FUNCTION_BYTES ".byte 0x14, 0x22, 0x63, 0x7c, 0x20, 0x00, 0x80, 0x4e\n"
#endif
#elif defined(V8_TARGET_ARCH_MIPS) || defined(V8_TARGET_ARCH_MIPS64)
#if defined(V8_TARGET_BIG_ENDIAN)
#define FUNCTION_BYTES                               \
  ".byte 0x00, 0x85, 0x10, 0x21, 0x03, 0xe0, 0x00, " \
  "0x08, 0x00, 0x00, 0x00, 0x00\n"
#else
#define FUNCTION_BYTES                               \
  ".byte 0x21, 0x10, 0x85, 0x00, 0x08, 0x00, 0xe0, " \
  "0x03, 0x00, 0x00, 0x00, 0x00\n"
#endif
#elif V8_TARGET_ARCH_S390
#define FUNCTION_BYTES                               \
  ".byte 0xb9, 0x08, 0x00, 0x23, 0x07, 0xfe\n"
#else
#error "Unknown architecture."
#endif

V8_EMBEDDED_RODATA_HEADER(test_string0_bytes)
__asm__(".byte 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37\n"
        ".byte 0x38, 0x39, 0x0a, 0x00\n");
extern "C" V8_ALIGNED(16) const char test_string0_bytes[];

V8_EMBEDDED_TEXT_HEADER(test_function0_bytes)
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

#undef FUNCTION_BYTES
#undef GENERATE_TEST_FUNCTION_DATA
#undef TEST_FUNCTION_FILE

}  // namespace test_isolate_independent_builtins
}  // namespace internal
}  // namespace v8
