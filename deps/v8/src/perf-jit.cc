// Copyright 2014 the V8 project authors. All rights reserved.
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

#include "src/perf-jit.h"

#if V8_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#include "src/third_party/kernel/tools/perf/util/jitdump.h"
#endif  // V8_OS_LINUX

namespace v8 {
namespace internal {

#if V8_OS_LINUX

const char PerfJitLogger::kFilenameFormatString[] = "perfjit-%d.dump";

// Extra padding for the PID in the filename
const int PerfJitLogger::kFilenameBufferPadding = 16;


PerfJitLogger::PerfJitLogger() : perf_output_handle_(NULL), code_index_(0) {
  if (!base::TimeTicks::KernelTimestampAvailable()) {
    FATAL("Cannot profile with perf JIT - kernel timestamps not available.");
  }

  // Open the perf JIT dump file.
  int bufferSize = sizeof(kFilenameFormatString) + kFilenameBufferPadding;
  ScopedVector<char> perf_dump_name(bufferSize);
  int size = SNPrintF(perf_dump_name, kFilenameFormatString,
                      base::OS::GetCurrentProcessId());
  CHECK_NE(size, -1);
  perf_output_handle_ =
      base::OS::FOpen(perf_dump_name.start(), base::OS::LogFileOpenMode);
  CHECK_NE(perf_output_handle_, NULL);
  setvbuf(perf_output_handle_, NULL, _IOFBF, kLogBufferSize);

  LogWriteHeader();
}


PerfJitLogger::~PerfJitLogger() {
  fclose(perf_output_handle_);
  perf_output_handle_ = NULL;
}


uint64_t PerfJitLogger::GetTimestamp() {
  return static_cast<int64_t>(
      base::TimeTicks::KernelTimestampNow().ToInternalValue());
}


void PerfJitLogger::LogRecordedBuffer(Code* code, SharedFunctionInfo*,
                                      const char* name, int length) {
  DCHECK(code->instruction_start() == code->address() + Code::kHeaderSize);
  DCHECK(perf_output_handle_ != NULL);

  const char* code_name = name;
  uint8_t* code_pointer = reinterpret_cast<uint8_t*>(code->instruction_start());
  uint32_t code_size = code->is_crankshafted() ? code->safepoint_table_offset()
                                               : code->instruction_size();

  static const char string_terminator[] = "\0";

  jr_code_load code_load;
  code_load.p.id = JIT_CODE_LOAD;
  code_load.p.total_size = sizeof(code_load) + length + 1 + code_size;
  code_load.p.timestamp = GetTimestamp();
  code_load.pid = static_cast<uint32_t>(base::OS::GetCurrentProcessId());
  code_load.tid = static_cast<uint32_t>(base::OS::GetCurrentThreadId());
  code_load.vma = 0x0;  //  Our addresses are absolute.
  code_load.code_addr = reinterpret_cast<uint64_t>(code_pointer);
  code_load.code_size = code_size;
  code_load.code_index = code_index_;

  code_index_++;

  LogWriteBytes(reinterpret_cast<const char*>(&code_load), sizeof(code_load));
  LogWriteBytes(code_name, length);
  LogWriteBytes(string_terminator, 1);
  LogWriteBytes(reinterpret_cast<const char*>(code_pointer), code_size);
}


void PerfJitLogger::CodeMoveEvent(Address from, Address to) {
  // Code relocation not supported.
  UNREACHABLE();
}


void PerfJitLogger::CodeDeleteEvent(Address from) {
  // V8 does not send notification on code unload
}


void PerfJitLogger::SnapshotPositionEvent(Address addr, int pos) {}


void PerfJitLogger::LogWriteBytes(const char* bytes, int size) {
  size_t rv = fwrite(bytes, 1, size, perf_output_handle_);
  DCHECK(static_cast<size_t>(size) == rv);
  USE(rv);
}


void PerfJitLogger::LogWriteHeader() {
  DCHECK(perf_output_handle_ != NULL);
  jitheader header;
  header.magic = JITHEADER_MAGIC;
  header.version = JITHEADER_VERSION;
  header.total_size = sizeof(jitheader);
  header.pad1 = 0xdeadbeef;
  header.elf_mach = GetElfMach();
  header.pid = base::OS::GetCurrentProcessId();
  header.timestamp =
      static_cast<uint64_t>(base::OS::TimeCurrentMillis() * 1000.0);
  LogWriteBytes(reinterpret_cast<const char*>(&header), sizeof(header));
}

#endif  // V8_OS_LINUX
}
}  // namespace v8::internal
