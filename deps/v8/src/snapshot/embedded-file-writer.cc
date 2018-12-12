// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded-file-writer.h"

#include <cinttypes>

namespace v8 {
namespace internal {

// V8_CC_MSVC is true for both MSVC and clang on windows. clang can handle
// __asm__-style inline assembly but MSVC cannot, and thus we need a more
// precise compiler detection that can distinguish between the two. clang on
// windows sets both __clang__ and _MSC_VER, MSVC sets only _MSC_VER.
#if defined(_MSC_VER) && !defined(__clang__)
#define V8_COMPILER_IS_MSVC
#endif

// Name mangling.
// Symbols are prefixed with an underscore on 32-bit architectures.
#if defined(V8_OS_WIN) && !defined(V8_TARGET_ARCH_X64) && \
    !defined(V8_TARGET_ARCH_ARM64)
#define SYMBOL_PREFIX "_"
#else
#define SYMBOL_PREFIX ""
#endif

// Platform-independent bits.
// -----------------------------------------------------------------------------

namespace {

DataDirective PointerSizeDirective() {
  if (kPointerSize == 8) {
    return kQuad;
  } else {
    CHECK_EQ(4, kPointerSize);
    return kLong;
  }
}

}  // namespace

const char* DirectiveAsString(DataDirective directive) {
#if defined(V8_OS_WIN) && defined(V8_COMPILER_IS_MSVC)
  switch (directive) {
    case kByte:
      return "BYTE";
    case kLong:
      return "DWORD";
    case kQuad:
      return "QWORD";
    default:
      UNREACHABLE();
  }
#elif defined(V8_OS_AIX)
  switch (directive) {
    case kByte:
      return ".byte";
    case kLong:
      return ".long";
    case kQuad:
      return ".llong";
    default:
      UNREACHABLE();
  }
#else
  switch (directive) {
    case kByte:
      return ".byte";
    case kLong:
      return ".long";
    case kQuad:
      return ".quad";
    case kOcta:
      return ".octa";
  }
  UNREACHABLE();
#endif
}

// V8_OS_MACOSX
// -----------------------------------------------------------------------------

#if defined(V8_OS_MACOSX)

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".text\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".data\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".const_data\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s _%s\n", DirectiveAsString(PointerSizeDirective()), target);
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  // TODO(jgruber): Investigate switching to .globl. Using .private_extern
  // prevents something along the compilation chain from messing with the
  // embedded blob. Using .global here causes embedded blob hash verification
  // failures at runtime.
  fprintf(fp_, ".private_extern _%s\n", name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "_%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  DeclareLabel(name);

  // TODO(mvstanton): Investigate the proper incantations to mark the label as
  // a function on OSX.
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

// V8_OS_AIX
// -----------------------------------------------------------------------------

#elif defined(V8_OS_AIX)

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".csect .text[PR]\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".csect .data[RW]\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".csect[RO]\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, ".align 2\n");
  fprintf(fp_, "%s:\n", name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d\n", value);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  AlignToCodeAlignment();
  DeclareLabel(name);
  fprintf(fp_, "  %s %s\n", DirectiveAsString(PointerSizeDirective()), target);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, ".globl %s\n", name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".align 5\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  Newline();
  DeclareSymbolGlobal(name);
  fprintf(fp_, ".csect %s[DS]\n", name); // function descriptor
  fprintf(fp_, "%s:\n", name);
  fprintf(fp_, ".llong .%s, 0, 0\n", name);
  SectionText();
  fprintf(fp_, ".%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

// V8_OS_WIN (MSVC)
// -----------------------------------------------------------------------------

#elif defined(V8_OS_WIN) && defined(V8_COMPILER_IS_MSVC)

// For MSVC builds we emit assembly in MASM syntax.
// See https://docs.microsoft.com/en-us/cpp/assembler/masm/directives-reference.

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".CODE\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".DATA\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".CONST\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s%s %s %d\n", SYMBOL_PREFIX, name, DirectiveAsString(kLong),
          value);
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s%s %s %s%s\n", SYMBOL_PREFIX, name,
          DirectiveAsString(PointerSizeDirective()), SYMBOL_PREFIX, target);
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, "PUBLIC %s%s\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  // Diverges from other platforms due to compile error
  // 'invalid combination with segment alignment'.
  fprintf(fp_, "ALIGN 4\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "; %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s LABEL %s\n", SYMBOL_PREFIX, name,
          DirectiveAsString(kByte));
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  fprintf(fp_, "%s%s PROC\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
  fprintf(fp_, "%s%s ENDP\n", SYMBOL_PREFIX, name);
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0%" PRIx64 "h", value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {
#if !defined(V8_TARGET_ARCH_X64)
  fprintf(fp_, ".MODEL FLAT\n");
#endif
}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {
  fprintf(fp_, "END\n");
}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

// Everything but AIX, Windows with MSVC, or OSX.
// -----------------------------------------------------------------------------

#else

void PlatformDependentEmbeddedFileWriter::SectionText() {
#ifdef OS_CHROMEOS
  fprintf(fp_, ".section .text.hot.embedded\n");
#else
  fprintf(fp_, ".section .text\n");
#endif
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  fprintf(fp_, ".section .data\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".section .rodata\n");
}

void PlatformDependentEmbeddedFileWriter::DeclareUint32(const char* name,
                                                        uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s %s%s\n", DirectiveAsString(PointerSizeDirective()),
          SYMBOL_PREFIX, target);
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, ".global %s%s\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformDependentEmbeddedFileWriter::AlignToDataAlignment() {
#if defined(V8_OS_WIN) && defined(V8_TARGET_ARCH_ARM64)
  // On Windows ARM64, instruction "ldr xt,[xn,v8_Default_embedded_blob_]" is
  // generated by clang-cl to load elements in v8_Default_embedded_blob_.
  // The generated instruction has scale 3 which requires the load target to be
  // aligned at 8 bytes (2^3).
  fprintf(fp_, ".balign 8\n");
#endif
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s:\n", SYMBOL_PREFIX, name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  DeclareLabel(name);

#if defined(V8_OS_WIN)
#if defined(V8_TARGET_ARCH_ARM64)
  // Windows ARM64 assembly is in GAS syntax, but ".type" is invalid directive
  // in PE/COFF for Windows.
#else
  // The directives for inserting debugging information on Windows come
  // from the PE (Portable Executable) and COFF (Common Object File Format)
  // standards. Documented here:
  // https://docs.microsoft.com/en-us/windows/desktop/debug/pe-format
  //
  // .scl 2 means StorageClass external.
  // .type 32 means Type Representation Function.
  fprintf(fp_, ".def %s%s; .scl 2; .type 32; .endef;\n", SYMBOL_PREFIX, name);
#endif
#elif defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_ARM64)
  // ELF format binaries on ARM use ".type <function name>, %function"
  // to create a DWARF subprogram entry.
  fprintf(fp_, ".type %s, %%function\n", name);
#else
  // Other ELF Format binaries use ".type <function name>, @function"
  // to create a DWARF subprogram entry.
  fprintf(fp_, ".type %s, @function\n", name);
#endif
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {}

void PlatformDependentEmbeddedFileWriter::FileEpilogue() {}

int PlatformDependentEmbeddedFileWriter::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

#endif

#undef SYMBOL_PREFIX
#undef V8_COMPILER_IS_MSVC

}  // namespace internal
}  // namespace v8
