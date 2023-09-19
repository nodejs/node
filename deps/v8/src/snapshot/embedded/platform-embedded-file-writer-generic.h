// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_EMBEDDED_PLATFORM_EMBEDDED_FILE_WRITER_GENERIC_H_
#define V8_SNAPSHOT_EMBEDDED_PLATFORM_EMBEDDED_FILE_WRITER_GENERIC_H_

#include "src/common/globals.h"  // For V8_OS_WIN_X64
#include "src/snapshot/embedded/platform-embedded-file-writer-base.h"

namespace v8 {
namespace internal {

class PlatformEmbeddedFileWriterGeneric
    : public PlatformEmbeddedFileWriterBase {
 public:
  PlatformEmbeddedFileWriterGeneric(EmbeddedTargetArch target_arch,
                                    EmbeddedTargetOs target_os)
      : target_arch_(target_arch), target_os_(target_os) {
    DCHECK(target_os_ == EmbeddedTargetOs::kChromeOS ||
           target_os_ == EmbeddedTargetOs::kFuchsia ||
           target_os_ == EmbeddedTargetOs::kGeneric);
  }

  void SectionText() override;
  void SectionRoData() override;

  void AlignToCodeAlignment() override;
  void AlignToPageSizeIfNeeded() override;
  void AlignToDataAlignment() override;

  void DeclareUint32(const char* name, uint32_t value) override;

  void DeclareSymbolGlobal(const char* name) override;
  void DeclareLabel(const char* name) override;

  void SourceInfo(int fileid, const char* filename, int line) override;
  void DeclareFunctionBegin(const char* name, uint32_t size) override;
  void DeclareFunctionEnd(const char* name) override;

  void Comment(const char* string) override;

  void FilePrologue() override;
  void DeclareExternalFilename(int fileid, const char* filename) override;
  void FileEpilogue() override;

  int IndentedDataDirective(DataDirective directive) override;

  DataDirective ByteChunkDataDirective() const override;

 private:
  const EmbeddedTargetArch target_arch_;
  const EmbeddedTargetOs target_os_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_EMBEDDED_PLATFORM_EMBEDDED_FILE_WRITER_GENERIC_H_
