// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_PLATFORM_EMBEDDED_FILE_WRITER_WIN_H_
#define V8_SNAPSHOT_EMBEDDED_PLATFORM_EMBEDDED_FILE_WRITER_WIN_H_

#include "src/base/macros.h"
#include "src/snapshot/embedded/platform-embedded-file-writer-base.h"

namespace v8 {
namespace internal {

class PlatformEmbeddedFileWriterWin : public PlatformEmbeddedFileWriterBase {
 public:
  PlatformEmbeddedFileWriterWin(EmbeddedTargetArch target_arch,
                                EmbeddedTargetOs target_os)
      : target_arch_(target_arch), target_os_(target_os) {
    USE(target_os_);
    DCHECK_EQ(target_os_, EmbeddedTargetOs::kWin);
  }

  void SectionText() override;
  void SectionData() override;
  void SectionRoData() override;

  void AlignToCodeAlignment() override;
  void AlignToDataAlignment() override;

  void DeclareUint32(const char* name, uint32_t value) override;
  void DeclarePointerToSymbol(const char* name, const char* target) override;

  void DeclareSymbolGlobal(const char* name) override;
  void DeclareLabel(const char* name) override;

  void SourceInfo(int fileid, const char* filename, int line) override;
  void DeclareFunctionBegin(const char* name, uint32_t size) override;
  void DeclareFunctionEnd(const char* name) override;

  int HexLiteral(uint64_t value) override;

  void Comment(const char* string) override;

  void FilePrologue() override;
  void DeclareExternalFilename(int fileid, const char* filename) override;
  void FileEpilogue() override;

  int IndentedDataDirective(DataDirective directive) override;

  DataDirective ByteChunkDataDirective() const override;
  int WriteByteChunk(const uint8_t* data) override;

  void StartPdataSection();
  void EndPdataSection();
  void StartXdataSection();
  void EndXdataSection();
  void DeclareExternalFunction(const char* name);

  // Emits an RVA (address relative to the module load address) specified as an
  // offset from a given symbol.
  void DeclareRvaToSymbol(const char* name, uint64_t offset = 0);

  void MaybeEmitUnwindData(const char* unwind_info_symbol,
                           const char* embedded_blob_data_symbol,
                           const EmbeddedData* blob,
                           const void* unwind_infos) override;

 private:
  const char* DirectiveAsString(DataDirective directive);

 private:
  const EmbeddedTargetArch target_arch_;
  const EmbeddedTargetOs target_os_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_PLATFORM_EMBEDDED_FILE_WRITER_WIN_H_
