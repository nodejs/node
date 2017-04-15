// Copyright 2016 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_PERF_JIT_H_
#define V8_PERF_JIT_H_

#include "src/log.h"

namespace v8 {
namespace internal {

#if V8_OS_LINUX

// Linux perf tool logging support
class PerfJitLogger : public CodeEventLogger {
 public:
  PerfJitLogger();
  virtual ~PerfJitLogger();

  void CodeMoveEvent(AbstractCode* from, Address to) override;
  void CodeDisableOptEvent(AbstractCode* code,
                           SharedFunctionInfo* shared) override {}

 private:
  void OpenJitDumpFile();
  void CloseJitDumpFile();
  void* OpenMarkerFile(int fd);
  void CloseMarkerFile(void* marker_address);

  uint64_t GetTimestamp();
  void LogRecordedBuffer(AbstractCode* code, SharedFunctionInfo* shared,
                         const char* name, int length) override;

  // Extension added to V8 log file name to get the low-level log name.
  static const char kFilenameFormatString[];
  static const int kFilenameBufferPadding;

  // File buffer size of the low-level log. We don't use the default to
  // minimize the associated overhead.
  static const int kLogBufferSize = 2 * MB;

  void LogWriteBytes(const char* bytes, int size);
  void LogWriteHeader();
  void LogWriteDebugInfo(Code* code, SharedFunctionInfo* shared);
  void LogWriteUnwindingInfo(Code* code);

  static const uint32_t kElfMachIA32 = 3;
  static const uint32_t kElfMachX64 = 62;
  static const uint32_t kElfMachARM = 40;
  static const uint32_t kElfMachMIPS = 10;
  static const uint32_t kElfMachARM64 = 183;

  uint32_t GetElfMach() {
#if V8_TARGET_ARCH_IA32
    return kElfMachIA32;
#elif V8_TARGET_ARCH_X64
    return kElfMachX64;
#elif V8_TARGET_ARCH_ARM
    return kElfMachARM;
#elif V8_TARGET_ARCH_MIPS
    return kElfMachMIPS;
#elif V8_TARGET_ARCH_ARM64
    return kElfMachARM64;
#else
    UNIMPLEMENTED();
    return 0;
#endif
  }

  // Per-process singleton file. We assume that there is one main isolate;
  // to determine when it goes away, we keep reference count.
  static base::LazyRecursiveMutex file_mutex_;
  static FILE* perf_output_handle_;
  static uint64_t reference_count_;
  static void* marker_address_;
  static uint64_t code_index_;
};

#else

// PerfJitLogger is only implemented on Linux
class PerfJitLogger : public CodeEventLogger {
 public:
  void CodeMoveEvent(AbstractCode* from, Address to) override {
    UNIMPLEMENTED();
  }

  void CodeDisableOptEvent(AbstractCode* code,
                           SharedFunctionInfo* shared) override {
    UNIMPLEMENTED();
  }

  void LogRecordedBuffer(AbstractCode* code, SharedFunctionInfo* shared,
                         const char* name, int length) override {
    UNIMPLEMENTED();
  }
};

#endif  // V8_OS_LINUX
}  // namespace internal
}  // namespace v8
#endif
