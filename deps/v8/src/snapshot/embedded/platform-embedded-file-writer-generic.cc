// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/platform-embedded-file-writer-generic.h"

#include <algorithm>
#include <cinttypes>

#include "src/common/globals.h"

namespace v8 {
namespace internal {

#define SYMBOL_PREFIX ""

namespace {

const char* DirectiveAsString(DataDirective directive) {
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
}

}  // namespace

void PlatformEmbeddedFileWriterGeneric::SectionText() {
  if (target_os_ == EmbeddedTargetOs::kChromeOS) {
    fprintf(fp_, ".section .text.hot.embedded\n");
  } else {
    fprintf(fp_, ".section .text\n");
  }
}

void PlatformEmbeddedFileWriterGeneric::SectionData() {
  fprintf(fp_, ".section .data\n");
}

void PlatformEmbeddedFileWriterGeneric::SectionRoData() {
  fprintf(fp_, ".section .rodata\n");
}

void PlatformEmbeddedFileWriterGeneric::DeclareUint32(const char* name,
                                                      uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformEmbeddedFileWriterGeneric::DeclarePointerToSymbol(
    const char* name, const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s %s%s\n", DirectiveAsString(PointerSizeDirective()),
          SYMBOL_PREFIX, target);
}

void PlatformEmbeddedFileWriterGeneric::DeclareSymbolGlobal(const char* name) {
  fprintf(fp_, ".global %s%s\n", SYMBOL_PREFIX, name);
}

void PlatformEmbeddedFileWriterGeneric::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformEmbeddedFileWriterGeneric::AlignToDataAlignment() {
  // On Windows ARM64, s390, PPC and possibly more platforms, aligned load
  // instructions are used to retrieve v8_Default_embedded_blob_ and/or
  // v8_Default_embedded_blob_size_. The generated instructions require the
  // load target to be aligned at 8 bytes (2^3).
  fprintf(fp_, ".balign 8\n");
}

void PlatformEmbeddedFileWriterGeneric::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformEmbeddedFileWriterGeneric::DeclareLabel(const char* name) {
  fprintf(fp_, "%s%s:\n", SYMBOL_PREFIX, name);
}

void PlatformEmbeddedFileWriterGeneric::SourceInfo(int fileid,
                                                   const char* filename,
                                                   int line) {
  fprintf(fp_, ".loc %d %d\n", fileid, line);
}

void PlatformEmbeddedFileWriterGeneric::DeclareFunctionBegin(const char* name,
                                                             uint32_t size) {
  DeclareLabel(name);

  if (target_arch_ == EmbeddedTargetArch::kArm ||
      target_arch_ == EmbeddedTargetArch::kArm64) {
    // ELF format binaries on ARM use ".type <function name>, %function"
    // to create a DWARF subprogram entry.
    fprintf(fp_, ".type %s, %%function\n", name);
  } else {
    // Other ELF Format binaries use ".type <function name>, @function"
    // to create a DWARF subprogram entry.
    fprintf(fp_, ".type %s, @function\n", name);
  }
  fprintf(fp_, ".size %s, %u\n", name, size);
}

void PlatformEmbeddedFileWriterGeneric::DeclareFunctionEnd(const char* name) {}

int PlatformEmbeddedFileWriterGeneric::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformEmbeddedFileWriterGeneric::FilePrologue() {}

void PlatformEmbeddedFileWriterGeneric::DeclareExternalFilename(
    int fileid, const char* filename) {
  // Replace any Windows style paths (backslashes) with forward
  // slashes.
  std::string fixed_filename(filename);
  std::replace(fixed_filename.begin(), fixed_filename.end(), '\\', '/');
  fprintf(fp_, ".file %d \"%s\"\n", fileid, fixed_filename.c_str());
}

void PlatformEmbeddedFileWriterGeneric::FileEpilogue() {
  // Omitting this section can imply an executable stack, which is usually
  // a linker warning/error. C++ compilers add these automatically, but
  // compiling assembly requires the .note.GNU-stack section to be inserted
  // manually.
  // Additional documentation:
  // https://wiki.gentoo.org/wiki/Hardened/GNU_stack_quickstart
  fprintf(fp_, ".section .note.GNU-stack,\"\",%%progbits\n");
}

int PlatformEmbeddedFileWriterGeneric::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

#undef SYMBOL_PREFIX

}  // namespace internal
}  // namespace v8
