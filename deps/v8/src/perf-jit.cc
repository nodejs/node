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

#include "src/perf-jit.h"

#include <memory>

#include "src/assembler.h"
#include "src/eh-frame.h"
#include "src/instruction-stream.h"
#include "src/objects-inl.h"
#include "src/source-position-table.h"
#include "src/wasm/wasm-code-manager.h"

#if V8_OS_LINUX
#include <fcntl.h>
#include <sys/mman.h>
#undef MAP_TYPE  // jumbo: conflicts with v8::internal::InstanceType::MAP_TYPE
#include <unistd.h>
#endif  // V8_OS_LINUX

namespace v8 {
namespace internal {

#if V8_OS_LINUX

struct PerfJitHeader {
  uint32_t magic_;
  uint32_t version_;
  uint32_t size_;
  uint32_t elf_mach_target_;
  uint32_t reserved_;
  uint32_t process_id_;
  uint64_t time_stamp_;
  uint64_t flags_;

  static const uint32_t kMagic = 0x4A695444;
  static const uint32_t kVersion = 1;
};

struct PerfJitBase {
  enum PerfJitEvent {
    kLoad = 0,
    kMove = 1,
    kDebugInfo = 2,
    kClose = 3,
    kUnwindingInfo = 4
  };

  uint32_t event_;
  uint32_t size_;
  uint64_t time_stamp_;
};

struct PerfJitCodeLoad : PerfJitBase {
  uint32_t process_id_;
  uint32_t thread_id_;
  uint64_t vma_;
  uint64_t code_address_;
  uint64_t code_size_;
  uint64_t code_id_;
};

struct PerfJitDebugEntry {
  uint64_t address_;
  int line_number_;
  int column_;
  // Followed by null-terminated name or \0xFF\0 if same as previous.
};

struct PerfJitCodeDebugInfo : PerfJitBase {
  uint64_t address_;
  uint64_t entry_count_;
  // Followed by entry_count_ instances of PerfJitDebugEntry.
};

struct PerfJitCodeUnwindingInfo : PerfJitBase {
  uint64_t unwinding_size_;
  uint64_t eh_frame_hdr_size_;
  uint64_t mapped_size_;
  // Followed by size_ - sizeof(PerfJitCodeUnwindingInfo) bytes of data.
};

const char PerfJitLogger::kFilenameFormatString[] = "./jit-%d.dump";

// Extra padding for the PID in the filename
const int PerfJitLogger::kFilenameBufferPadding = 16;

base::LazyRecursiveMutex PerfJitLogger::file_mutex_;
// The following static variables are protected by PerfJitLogger::file_mutex_.
uint64_t PerfJitLogger::reference_count_ = 0;
void* PerfJitLogger::marker_address_ = nullptr;
uint64_t PerfJitLogger::code_index_ = 0;
FILE* PerfJitLogger::perf_output_handle_ = nullptr;

void PerfJitLogger::OpenJitDumpFile() {
  // Open the perf JIT dump file.
  perf_output_handle_ = nullptr;

  int bufferSize = sizeof(kFilenameFormatString) + kFilenameBufferPadding;
  ScopedVector<char> perf_dump_name(bufferSize);
  int size = SNPrintF(perf_dump_name, kFilenameFormatString,
                      base::OS::GetCurrentProcessId());
  CHECK_NE(size, -1);

  int fd = open(perf_dump_name.start(), O_CREAT | O_TRUNC | O_RDWR, 0666);
  if (fd == -1) return;

  marker_address_ = OpenMarkerFile(fd);
  if (marker_address_ == nullptr) return;

  perf_output_handle_ = fdopen(fd, "w+");
  if (perf_output_handle_ == nullptr) return;

  setvbuf(perf_output_handle_, nullptr, _IOFBF, kLogBufferSize);
}

void PerfJitLogger::CloseJitDumpFile() {
  if (perf_output_handle_ == nullptr) return;
  fclose(perf_output_handle_);
  perf_output_handle_ = nullptr;
}

void* PerfJitLogger::OpenMarkerFile(int fd) {
  long page_size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  if (page_size == -1) return nullptr;

  // Mmap the file so that there is a mmap record in the perf_data file.
  //
  // The map must be PROT_EXEC to ensure it is not ignored by perf record.
  void* marker_address =
      mmap(nullptr, page_size, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
  return (marker_address == MAP_FAILED) ? nullptr : marker_address;
}

void PerfJitLogger::CloseMarkerFile(void* marker_address) {
  if (marker_address == nullptr) return;
  long page_size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  if (page_size == -1) return;
  munmap(marker_address, page_size);
}

PerfJitLogger::PerfJitLogger(Isolate* isolate) : CodeEventLogger(isolate) {
  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  reference_count_++;
  // If this is the first logger, open the file and write the header.
  if (reference_count_ == 1) {
    OpenJitDumpFile();
    if (perf_output_handle_ == nullptr) return;
    LogWriteHeader();
  }
}

PerfJitLogger::~PerfJitLogger() {
  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  reference_count_--;
  // If this was the last logger, close the file.
  if (reference_count_ == 0) {
    CloseJitDumpFile();
  }
}

uint64_t PerfJitLogger::GetTimestamp() {
  struct timespec ts;
  int result = clock_gettime(CLOCK_MONOTONIC, &ts);
  DCHECK_EQ(0, result);
  USE(result);
  static const uint64_t kNsecPerSec = 1000000000;
  return (ts.tv_sec * kNsecPerSec) + ts.tv_nsec;
}

void PerfJitLogger::LogRecordedBuffer(AbstractCode* abstract_code,
                                      SharedFunctionInfo* shared,
                                      const char* name, int length) {
  if (FLAG_perf_basic_prof_only_functions &&
      (abstract_code->kind() != AbstractCode::INTERPRETED_FUNCTION &&
       abstract_code->kind() != AbstractCode::OPTIMIZED_FUNCTION)) {
    return;
  }

  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  if (perf_output_handle_ == nullptr) return;

  // We only support non-interpreted functions.
  if (!abstract_code->IsCode()) return;
  Code* code = abstract_code->GetCode();
  DCHECK(code->raw_instruction_start() == code->address() + Code::kHeaderSize);

  // Debug info has to be emitted first.
  if (FLAG_perf_prof && shared != nullptr) {
    // TODO(herhut): This currently breaks for js2wasm/wasm2js functions.
    if (code->kind() != Code::JS_TO_WASM_FUNCTION &&
        code->kind() != Code::WASM_TO_JS_FUNCTION) {
      LogWriteDebugInfo(code, shared);
    }
  }

  const char* code_name = name;
  uint8_t* code_pointer = reinterpret_cast<uint8_t*>(code->InstructionStart());
  // Code generated by Turbofan will have the safepoint table directly after
  // instructions. There is no need to record the safepoint table itself.
  uint32_t code_size = code->is_turbofanned() ? code->safepoint_table_offset()
                                              : code->InstructionSize();

  // Unwinding info comes right after debug info.
  if (FLAG_perf_prof_unwinding_info) LogWriteUnwindingInfo(code);

  WriteJitCodeLoadEntry(code_pointer, code_size, code_name, length);
}

void PerfJitLogger::LogRecordedBuffer(const wasm::WasmCode* code,
                                      const char* name, int length) {
  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  if (perf_output_handle_ == nullptr) return;

  WriteJitCodeLoadEntry(code->instructions().start(),
                        code->instructions().length(), name, length);
}

void PerfJitLogger::WriteJitCodeLoadEntry(const uint8_t* code_pointer,
                                          uint32_t code_size, const char* name,
                                          int name_length) {
  static const char string_terminator[] = "\0";

  PerfJitCodeLoad code_load;
  code_load.event_ = PerfJitCodeLoad::kLoad;
  code_load.size_ = sizeof(code_load) + name_length + 1 + code_size;
  code_load.time_stamp_ = GetTimestamp();
  code_load.process_id_ =
      static_cast<uint32_t>(base::OS::GetCurrentProcessId());
  code_load.thread_id_ = static_cast<uint32_t>(base::OS::GetCurrentThreadId());
  code_load.vma_ = reinterpret_cast<uint64_t>(code_pointer);
  code_load.code_address_ = reinterpret_cast<uint64_t>(code_pointer);
  code_load.code_size_ = code_size;
  code_load.code_id_ = code_index_;

  code_index_++;

  LogWriteBytes(reinterpret_cast<const char*>(&code_load), sizeof(code_load));
  LogWriteBytes(name, name_length);
  LogWriteBytes(string_terminator, 1);
  LogWriteBytes(reinterpret_cast<const char*>(code_pointer), code_size);
}

namespace {

constexpr char kUnknownScriptNameString[] = "<unknown>";
constexpr size_t kUnknownScriptNameStringLen =
    arraysize(kUnknownScriptNameString) - 1;

size_t GetScriptNameLength(const SourcePositionInfo& info) {
  if (!info.script.is_null()) {
    Object* name_or_url = info.script->GetNameOrSourceURL();
    if (name_or_url->IsString()) {
      String* str = String::cast(name_or_url);
      if (str->IsOneByteRepresentation()) return str->length();
      int length;
      str->ToCString(DISALLOW_NULLS, FAST_STRING_TRAVERSAL, &length);
      return static_cast<size_t>(length);
    }
  }
  return kUnknownScriptNameStringLen;
}

Vector<const char> GetScriptName(const SourcePositionInfo& info,
                                 std::unique_ptr<char[]>* storage) {
  if (!info.script.is_null()) {
    Object* name_or_url = info.script->GetNameOrSourceURL();
    if (name_or_url->IsSeqOneByteString()) {
      SeqOneByteString* str = SeqOneByteString::cast(name_or_url);
      return {reinterpret_cast<char*>(str->GetChars()),
              static_cast<size_t>(str->length())};
    } else if (name_or_url->IsString()) {
      int length;
      *storage =
          String::cast(name_or_url)
              ->ToCString(DISALLOW_NULLS, FAST_STRING_TRAVERSAL, &length);
      return {storage->get(), static_cast<size_t>(length)};
    }
  }
  return {kUnknownScriptNameString, kUnknownScriptNameStringLen};
}

SourcePositionInfo GetSourcePositionInfo(Handle<Code> code,
                                         Handle<SharedFunctionInfo> function,
                                         SourcePosition pos) {
  if (code->is_turbofanned()) {
    DisallowHeapAllocation disallow;
    return pos.InliningStack(code)[0];
  } else {
    return SourcePositionInfo(pos, function);
  }
}

}  // namespace

void PerfJitLogger::LogWriteDebugInfo(Code* code, SharedFunctionInfo* shared) {
  // Compute the entry count and get the name of the script.
  uint32_t entry_count = 0;
  for (SourcePositionTableIterator iterator(code->SourcePositionTable());
       !iterator.done(); iterator.Advance()) {
    entry_count++;
  }
  if (entry_count == 0) return;
  // The WasmToJS wrapper stubs have source position entries.
  if (!shared->HasSourceCode()) return;
  Isolate* isolate = shared->GetIsolate();
  Handle<Script> script(Script::cast(shared->script()), isolate);

  PerfJitCodeDebugInfo debug_info;

  debug_info.event_ = PerfJitCodeLoad::kDebugInfo;
  debug_info.time_stamp_ = GetTimestamp();
  debug_info.address_ = code->InstructionStart();
  debug_info.entry_count_ = entry_count;

  uint32_t size = sizeof(debug_info);
  // Add the sizes of fixed parts of entries.
  size += entry_count * sizeof(PerfJitDebugEntry);
  // Add the size of the name after each entry.

  Handle<Code> code_handle(code, isolate);
  Handle<SharedFunctionInfo> function_handle(shared, isolate);
  for (SourcePositionTableIterator iterator(code->SourcePositionTable());
       !iterator.done(); iterator.Advance()) {
    SourcePositionInfo info(GetSourcePositionInfo(code_handle, function_handle,
                                                  iterator.source_position()));
    size += GetScriptNameLength(info) + 1;
  }

  int padding = ((size + 7) & (~7)) - size;
  debug_info.size_ = size + padding;
  LogWriteBytes(reinterpret_cast<const char*>(&debug_info), sizeof(debug_info));

  Address code_start = code->InstructionStart();

  for (SourcePositionTableIterator iterator(code->SourcePositionTable());
       !iterator.done(); iterator.Advance()) {
    SourcePositionInfo info(GetSourcePositionInfo(code_handle, function_handle,
                                                  iterator.source_position()));
    PerfJitDebugEntry entry;
    // The entry point of the function will be placed straight after the ELF
    // header when processed by "perf inject". Adjust the position addresses
    // accordingly.
    entry.address_ = code_start + iterator.code_offset() + kElfHeaderSize;
    entry.line_number_ = info.line + 1;
    entry.column_ = info.column + 1;
    LogWriteBytes(reinterpret_cast<const char*>(&entry), sizeof(entry));
    // The extracted name may point into heap-objects, thus disallow GC.
    DisallowHeapAllocation no_gc;
    std::unique_ptr<char[]> name_storage;
    Vector<const char> name_string = GetScriptName(info, &name_storage);
    LogWriteBytes(name_string.start(),
                  static_cast<uint32_t>(name_string.size()) + 1);
  }
  char padding_bytes[8] = {0};
  LogWriteBytes(padding_bytes, padding);
}

void PerfJitLogger::LogWriteUnwindingInfo(Code* code) {
  PerfJitCodeUnwindingInfo unwinding_info_header;
  unwinding_info_header.event_ = PerfJitCodeLoad::kUnwindingInfo;
  unwinding_info_header.time_stamp_ = GetTimestamp();
  unwinding_info_header.eh_frame_hdr_size_ = EhFrameConstants::kEhFrameHdrSize;

  if (code->has_unwinding_info()) {
    unwinding_info_header.unwinding_size_ = code->unwinding_info_size();
    unwinding_info_header.mapped_size_ = unwinding_info_header.unwinding_size_;
  } else {
    unwinding_info_header.unwinding_size_ = EhFrameConstants::kEhFrameHdrSize;
    unwinding_info_header.mapped_size_ = 0;
  }

  int content_size = static_cast<int>(sizeof(unwinding_info_header) +
                                      unwinding_info_header.unwinding_size_);
  int padding_size = RoundUp(content_size, 8) - content_size;
  unwinding_info_header.size_ = content_size + padding_size;

  LogWriteBytes(reinterpret_cast<const char*>(&unwinding_info_header),
                sizeof(unwinding_info_header));

  if (code->has_unwinding_info()) {
    LogWriteBytes(reinterpret_cast<const char*>(code->unwinding_info_start()),
                  code->unwinding_info_size());
  } else {
    OFStream perf_output_stream(perf_output_handle_);
    EhFrameWriter::WriteEmptyEhFrame(perf_output_stream);
  }

  char padding_bytes[] = "\0\0\0\0\0\0\0\0";
  DCHECK_LT(padding_size, static_cast<int>(sizeof(padding_bytes)));
  LogWriteBytes(padding_bytes, static_cast<int>(padding_size));
}

void PerfJitLogger::CodeMoveEvent(AbstractCode* from, AbstractCode* to) {
  // We may receive a CodeMove event if a BytecodeArray object moves. Otherwise
  // code relocation is not supported.
  CHECK(from->IsBytecodeArray());
}

void PerfJitLogger::LogWriteBytes(const char* bytes, int size) {
  size_t rv = fwrite(bytes, 1, size, perf_output_handle_);
  DCHECK(static_cast<size_t>(size) == rv);
  USE(rv);
}

void PerfJitLogger::LogWriteHeader() {
  DCHECK_NOT_NULL(perf_output_handle_);
  PerfJitHeader header;

  header.magic_ = PerfJitHeader::kMagic;
  header.version_ = PerfJitHeader::kVersion;
  header.size_ = sizeof(header);
  header.elf_mach_target_ = GetElfMach();
  header.reserved_ = 0xDEADBEEF;
  header.process_id_ = base::OS::GetCurrentProcessId();
  header.time_stamp_ =
      static_cast<uint64_t>(V8::GetCurrentPlatform()->CurrentClockTimeMillis() *
                            base::Time::kMicrosecondsPerMillisecond);
  header.flags_ = 0;

  LogWriteBytes(reinterpret_cast<const char*>(&header), sizeof(header));
}

#endif  // V8_OS_LINUX
}  // namespace internal
}  // namespace v8
