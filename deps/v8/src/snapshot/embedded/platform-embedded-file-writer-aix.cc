// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/platform-embedded-file-writer-aix.h"

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
      return ".llong";
    default:
      UNREACHABLE();
  }
}

}  // namespace

void PlatformEmbeddedFileWriterAIX::SectionText() {
  fprintf(fp_, ".csect .text[PR]\n");
}

void PlatformEmbeddedFileWriterAIX::SectionData() {
  fprintf(fp_, ".csect .data[RW]\n");
}

void PlatformEmbeddedFileWriterAIX::SectionRoData() {
  fprintf(fp_, ".csect[RO]\n");
}

void PlatformEmbeddedFileWriterAIX::DeclareUint32(const char* name,
                                                  uint32_t value) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, ".align 2\n");
  fprintf(fp_, "%s:\n", name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d\n", value);
  Newline();
}

void PlatformEmbeddedFileWriterAIX::DeclarePointerToSymbol(const char* name,
                                                           const char* target) {
  AlignToCodeAlignment();
  DeclareLabel(name);
  fprintf(fp_, "  %s %s\n", DirectiveAsString(PointerSizeDirective()), target);
  Newline();
}

void PlatformEmbeddedFileWriterAIX::DeclareSymbolGlobal(const char* name) {
  fprintf(fp_, ".globl %s\n", name);
}

void PlatformEmbeddedFileWriterAIX::AlignToCodeAlignment() {
  fprintf(fp_, ".align 5\n");
}

void PlatformEmbeddedFileWriterAIX::AlignToDataAlignment() {
  fprintf(fp_, ".align 3\n");
}

void PlatformEmbeddedFileWriterAIX::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformEmbeddedFileWriterAIX::DeclareLabel(const char* name) {
  DeclareSymbolGlobal(name);
  fprintf(fp_, "%s:\n", name);
}

void PlatformEmbeddedFileWriterAIX::SourceInfo(int fileid, const char* filename,
                                               int line) {
  fprintf(fp_, ".xline %d, \"%s\"\n", line, filename);
}

void PlatformEmbeddedFileWriterAIX::DeclareFunctionBegin(const char* name) {
  Newline();
  DeclareSymbolGlobal(name);
  fprintf(fp_, ".csect %s[DS]\n", name);  // function descriptor
  fprintf(fp_, "%s:\n", name);
  fprintf(fp_, ".llong .%s, 0, 0\n", name);
  SectionText();
  fprintf(fp_, ".%s:\n", name);
}

void PlatformEmbeddedFileWriterAIX::DeclareFunctionEnd(const char* name) {}

int PlatformEmbeddedFileWriterAIX::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformEmbeddedFileWriterAIX::FilePrologue() {}

void PlatformEmbeddedFileWriterAIX::DeclareExternalFilename(
    int fileid, const char* filename) {
  // File name cannot be declared with an identifier on AIX.
  // We use the SourceInfo method to emit debug info in
  //.xline <line-number> <file-name> format.
}

void PlatformEmbeddedFileWriterAIX::FileEpilogue() {}

int PlatformEmbeddedFileWriterAIX::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

DataDirective PlatformEmbeddedFileWriterAIX::ByteChunkDataDirective() const {
  // PPC uses a fixed 4 byte instruction set, using .long
  // to prevent any unnecessary padding.
  return kLong;
}

int PlatformEmbeddedFileWriterAIX::WriteByteChunk(const uint8_t* data) {
  DCHECK_EQ(ByteChunkDataDirective(), kLong);
  const uint32_t* long_ptr = reinterpret_cast<const uint32_t*>(data);
  return HexLiteral(*long_ptr);
}

#undef SYMBOL_PREFIX

}  // namespace internal
}  // namespace v8
