// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/embedded/platform-embedded-file-writer-mac.h"

namespace v8 {
namespace internal {

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

void PlatformEmbeddedFileWriterMac::SectionText() { fprintf(fp_, ".text\n"); }

void PlatformEmbeddedFileWriterMac::SectionData() { fprintf(fp_, ".data\n"); }

void PlatformEmbeddedFileWriterMac::SectionRoData() {
  fprintf(fp_, ".const_data\n");
}

void PlatformEmbeddedFileWriterMac::DeclareUint32(const char* name,
                                                  uint32_t value) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  IndentedDataDirective(kLong);
  fprintf(fp_, "%d", value);
  Newline();
}

void PlatformEmbeddedFileWriterMac::DeclarePointerToSymbol(const char* name,
                                                           const char* target) {
  DeclareSymbolGlobal(name);
  DeclareLabel(name);
  fprintf(fp_, "  %s _%s\n", DirectiveAsString(PointerSizeDirective()), target);
}

void PlatformEmbeddedFileWriterMac::DeclareSymbolGlobal(const char* name) {
  // TODO(jgruber): Investigate switching to .globl. Using .private_extern
  // prevents something along the compilation chain from messing with the
  // embedded blob. Using .global here causes embedded blob hash verification
  // failures at runtime.
  fprintf(fp_, ".private_extern _%s\n", name);
}

void PlatformEmbeddedFileWriterMac::AlignToCodeAlignment() {
  fprintf(fp_, ".balign 32\n");
}

void PlatformEmbeddedFileWriterMac::AlignToDataAlignment() {
  fprintf(fp_, ".balign 8\n");
}

void PlatformEmbeddedFileWriterMac::Comment(const char* string) {
  fprintf(fp_, "// %s\n", string);
}

void PlatformEmbeddedFileWriterMac::DeclareLabel(const char* name) {
  fprintf(fp_, "_%s:\n", name);
}

void PlatformEmbeddedFileWriterMac::SourceInfo(int fileid, const char* filename,
                                               int line) {
  fprintf(fp_, ".loc %d %d\n", fileid, line);
}

// TODO(mmarchini): investigate emitting size annotations for OS X
void PlatformEmbeddedFileWriterMac::DeclareFunctionBegin(const char* name,
                                                         uint32_t size) {
  DeclareLabel(name);

  // TODO(mvstanton): Investigate the proper incantations to mark the label as
  // a function on OSX.
}

void PlatformEmbeddedFileWriterMac::DeclareFunctionEnd(const char* name) {}

int PlatformEmbeddedFileWriterMac::HexLiteral(uint64_t value) {
  return fprintf(fp_, "0x%" PRIx64, value);
}

void PlatformEmbeddedFileWriterMac::FilePrologue() {}

void PlatformEmbeddedFileWriterMac::DeclareExternalFilename(
    int fileid, const char* filename) {
  fprintf(fp_, ".file %d \"%s\"\n", fileid, filename);
}

void PlatformEmbeddedFileWriterMac::FileEpilogue() {}

int PlatformEmbeddedFileWriterMac::IndentedDataDirective(
    DataDirective directive) {
  return fprintf(fp_, "  %s ", DirectiveAsString(directive));
}

}  // namespace internal
}  // namespace v8
