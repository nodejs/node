// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded-file-writer.h"

#include <cinttypes>

namespace v8 {
namespace internal {

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

// static
const char* PlatformDependentEmbeddedFileWriter::DirectiveAsString(
    DataDirective directive) {
#ifndef V8_OS_WIN
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
#else
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

// TODO(aix): Update custom logic previously contained in section header macros.
// See
// https://cs.chromium.org/chromium/src/v8/src/snapshot/macros.h?l=81&rcl=31b2546b348e864539ade15897eac971b3c0e402

void PlatformDependentEmbeddedFileWriter::SectionText() {
  fprintf(fp_, ".csect .text[PR]\n");
}

void PlatformDependentEmbeddedFileWriter::SectionData() {
  // TODO(aix): Confirm and update if needed.
  fprintf(fp_, ".csect .data[RW]\n");
}

void PlatformDependentEmbeddedFileWriter::SectionRoData() {
  fprintf(fp_, ".csect[RO]\n");
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
  fprintf(fp_, "  %s %s\n", DirectiveAsString(PointerSizeDirective()), target);
  Newline();
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, ".globl %s\n", name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  DeclareLabel(name);
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

// V8_OS_WIN
// -----------------------------------------------------------------------------

#elif defined(V8_OS_WIN)

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
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  fprintf(fp_, "%s %s %d\n", name, DirectiveAsString(kLong), value);
#else
  fprintf(fp_, "_%s %s %d\n", name, DirectiveAsString(kLong), value);
#endif
}

void PlatformDependentEmbeddedFileWriter::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  fprintf(fp_, "%s %s %s\n", name, DirectiveAsString(PointerSizeDirective()),
          target);
#else
  fprintf(fp_, "_%s %s _%s\n", name, DirectiveAsString(PointerSizeDirective()),
          target);
#endif
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  fprintf(fp_, "PUBLIC %s\n", name);
#else
  fprintf(fp_, "PUBLIC _%s\n", name);
#endif
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  // Diverges from other platforms due to compile error
  // 'invalid combination with segment alignment'.
  fprintf(fp_, "ALIGN 4\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "; %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  fprintf(fp_, "%s LABEL %s\n", name, DirectiveAsString(kByte));
#else
  fprintf(fp_, "_%s LABEL %s\n", name, DirectiveAsString(kByte));
#endif
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  fprintf(fp_, "%s PROC\n", name);
#else
  fprintf(fp_, "_%s PROC\n", name);
#endif
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionEnd(const char* name) {
#if defined(V8_TARGET_ARCH_X64) || defined(V8_TARGET_ARCH_ARM64)
  fprintf(fp_, "%s ENDP\n", name);
#else
  fprintf(fp_, "_%s ENDP\n", name);
#endif
}

int PlatformDependentEmbeddedFileWriter::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0%" PRIx64 "h", value);
}

void PlatformDependentEmbeddedFileWriter::FilePrologue() {
#if !defined(V8_TARGET_ARCH_X64) && !defined(V8_TARGET_ARCH_ARM64)
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

// Everything but AIX, Windows, or OSX.
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
  fprintf(fp_, "  %s %s\n", DirectiveAsString(PointerSizeDirective()), target);
}

void PlatformDependentEmbeddedFileWriter::DeclareSymbolGlobal(
    const char* name) {
  fprintf(fp_, ".global %s\n", name);
}

void PlatformDependentEmbeddedFileWriter::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformDependentEmbeddedFileWriter::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformDependentEmbeddedFileWriter::DeclareLabel(const char* name) {
  fprintf(fp_, "%s:\n", name);
}

void PlatformDependentEmbeddedFileWriter::DeclareFunctionBegin(
    const char* name) {
  DeclareLabel(name);

#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_ARM64)
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

}  // namespace internal
}  // namespace v8
