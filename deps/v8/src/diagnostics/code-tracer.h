// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_CODE_TRACER_H_
#define V8_DIAGNOSTICS_CODE_TRACER_H_

#include "src/common/globals.h"
#include "src/flags/flags.h"
#include "src/utils/allocation.h"
#include "src/utils/utils.h"
#include "src/utils/vector.h"

namespace v8 {
namespace internal {

class CodeTracer final : public Malloced {
 public:
  explicit CodeTracer(int isolate_id) : file_(nullptr), scope_depth_(0) {
    if (!ShouldRedirect()) {
      file_ = stdout;
      return;
    }

    if (FLAG_redirect_code_traces_to != nullptr) {
      StrNCpy(filename_, FLAG_redirect_code_traces_to, filename_.length());
    } else if (isolate_id >= 0) {
      SNPrintF(filename_, "code-%d-%d.asm", base::OS::GetCurrentProcessId(),
               isolate_id);
    } else {
      SNPrintF(filename_, "code-%d.asm", base::OS::GetCurrentProcessId());
    }

    WriteChars(filename_.begin(), "", 0, false);
  }

  class Scope {
   public:
    explicit Scope(CodeTracer* tracer) : tracer_(tracer) { tracer->OpenFile(); }
    ~Scope() { tracer_->CloseFile(); }

    FILE* file() const { return tracer_->file(); }

   private:
    CodeTracer* tracer_;
  };

  void OpenFile() {
    if (!ShouldRedirect()) {
      return;
    }

    if (file_ == nullptr) {
      file_ = base::OS::FOpen(filename_.begin(), "ab");
      CHECK_WITH_MSG(file_ != nullptr,
                     "could not open file. If on Android, try passing "
                     "--redirect-code-traces-to=/sdcard/Download/<file-name>");
    }

    scope_depth_++;
  }

  void CloseFile() {
    if (!ShouldRedirect()) {
      return;
    }

    if (--scope_depth_ == 0) {
      DCHECK_NOT_NULL(file_);
      fclose(file_);
      file_ = nullptr;
    }
  }

  FILE* file() const { return file_; }

 private:
  static bool ShouldRedirect() { return FLAG_redirect_code_traces; }

  EmbeddedVector<char, 128> filename_;
  FILE* file_;
  int scope_depth_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_DIAGNOSTICS_CODE_TRACER_H_
