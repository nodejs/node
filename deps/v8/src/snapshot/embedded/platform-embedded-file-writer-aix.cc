// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/platform-embedded-file-writer-aix.h"

#include "src/objects/code.h"

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
  // These symbols are not visible outside of the final binary, this allows for
  // reduced binary size, and less work for the dynamic linker.
  fprintf(fp_, ".globl %s, hidden\n", name);
}

void PlatformEmbeddedFileWriterAIX::AlignToCodeAlignment() {
  STATIC_ASSERT((1 << 5) >= kCodeAlignment);
  fprintf(fp_, ".align 5\n");
}

void PlatformEmbeddedFileWriterAIX::AlignToDataAlignment() {
  STATIC_ASSERT((1 << 3) >= Code::kMetadataAlignment);
  fprintf(fp_, ".align 3\n");
}

void PlatformEmbeddedFileWriterAIX::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformEmbeddedFileWriterAIX::DeclareLabel(const char* name) {
  // .global is required on AIX, if the label is used/referenced in another file
  // later to be linked.
  fprintf(fp_, ".globl %s\n", name);
  fprintf(fp_, "%s:\n", name);
}

void PlatformEmbeddedFileWriterAIX::SourceInfo(int fileid, const char* filename,
                                               int line) {
  fprintf(fp_, ".xline %d, \"%s\"\n", line, filename);
}

// TODO(mmarchini): investigate emitting size annotations for AIX
void PlatformEmbeddedFileWriterAIX::DeclareFunctionBegin(const char* name,
                                                         uint32_t size) {
  Newline();
  if (ENABLE_CONTROL_FLOW_INTEGRITY_BOOL) {
    DeclareSymbolGlobal(name);
  }
  fprintf(fp_, ".csect %s[DS]\n", name);  // function descriptor
  fprintf(fp_, "%s:\n", name);
  fprintf(fp_, ".llong .%s, 0, 0\n", name);
  SectionText();
  fprintf(fp_, ".%s:\n", name);
}

void PlatformEmbeddedFileWriterAIX::DeclareFunctionEnd(const char* name) {}

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

#undef SYMBOL_PREFIX

}  // namespace internal
}  // namespace v8
