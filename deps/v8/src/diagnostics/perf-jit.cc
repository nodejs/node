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

#include "src/diagnostics/perf-jit.h"

#include "src/common/assert-scope.h"

// Only compile the {LinuxPerfJitLogger} on Linux.
#if V8_OS_LINUX

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>

#include "src/base/platform/wrappers.h"
#include "src/codegen/assembler.h"
#include "src/codegen/source-position-table.h"
#include "src/diagnostics/eh-frame.h"
#include "src/objects/objects-inl.h"
#include "src/objects/shared-function-info.h"
#include "src/snapshot/embedded/embedded-data.h"
#include "src/utils/ostreams.h"

#if V8_ENABLE_WEBASSEMBLY
#include "src/wasm/wasm-code-manager.h"
#endif  // V8_ENABLE_WEBASSEMBLY

namespace v8 {
namespace internal {

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

const char LinuxPerfJitLogger::kFilenameFormatString[] = "./jit-%d.dump";

// Extra padding for the PID in the filename
const int LinuxPerfJitLogger::kFilenameBufferPadding = 16;

static const char kStringTerminator[] = {'\0'};
static const char kRepeatedNameMarker[] = {'\xff', '\0'};

base::LazyRecursiveMutex LinuxPerfJitLogger::file_mutex_;
// The following static variables are protected by
// LinuxPerfJitLogger::file_mutex_.
int LinuxPerfJitLogger::process_id_ = 0;
uint64_t LinuxPerfJitLogger::reference_count_ = 0;
void* LinuxPerfJitLogger::marker_address_ = nullptr;
uint64_t LinuxPerfJitLogger::code_index_ = 0;
FILE* LinuxPerfJitLogger::perf_output_handle_ = nullptr;

void LinuxPerfJitLogger::OpenJitDumpFile() {
  // Open the perf JIT dump file.
  perf_output_handle_ = nullptr;

  int bufferSize = sizeof(kFilenameFormatString) + kFilenameBufferPadding;
  base::ScopedVector<char> perf_dump_name(bufferSize);
  int size = SNPrintF(perf_dump_name, kFilenameFormatString, process_id_);
  CHECK_NE(size, -1);

  int fd = open(perf_dump_name.begin(), O_CREAT | O_TRUNC | O_RDWR, 0666);
  if (fd == -1) return;

  // If --perf-prof-delete-file is given, unlink the file right after opening
  // it. This keeps the file handle to the file valid. This only works on Linux,
  // which is the only platform supported for --perf-prof anyway.
  if (FLAG_perf_prof_delete_file) CHECK_EQ(0, unlink(perf_dump_name.begin()));

  marker_address_ = OpenMarkerFile(fd);
  if (marker_address_ == nullptr) return;

  perf_output_handle_ = fdopen(fd, "w+");
  if (perf_output_handle_ == nullptr) return;

  setvbuf(perf_output_handle_, nullptr, _IOFBF, kLogBufferSize);
}

void LinuxPerfJitLogger::CloseJitDumpFile() {
  if (perf_output_handle_ == nullptr) return;
  base::Fclose(perf_output_handle_);
  perf_output_handle_ = nullptr;
}

void* LinuxPerfJitLogger::OpenMarkerFile(int fd) {
  long page_size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  if (page_size == -1) return nullptr;

  // Mmap the file so that there is a mmap record in the perf_data file.
  //
  // The map must be PROT_EXEC to ensure it is not ignored by perf record.
  void* marker_address =
      mmap(nullptr, page_size, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
  return (marker_address == MAP_FAILED) ? nullptr : marker_address;
}

void LinuxPerfJitLogger::CloseMarkerFile(void* marker_address) {
  if (marker_address == nullptr) return;
  long page_size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  if (page_size == -1) return;
  munmap(marker_address, page_size);
}

LinuxPerfJitLogger::LinuxPerfJitLogger(Isolate* isolate)
    : CodeEventLogger(isolate) {
  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());
  process_id_ = base::OS::GetCurrentProcessId();

  reference_count_++;
  // If this is the first logger, open the file and write the header.
  if (reference_count_ == 1) {
    OpenJitDumpFile();
    if (perf_output_handle_ == nullptr) return;
    LogWriteHeader();
  }
}

LinuxPerfJitLogger::~LinuxPerfJitLogger() {
  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  reference_count_--;
  // If this was the last logger, close the file.
  if (reference_count_ == 0) {
    CloseJitDumpFile();
  }
}

uint64_t LinuxPerfJitLogger::GetTimestamp() {
  struct timespec ts;
  int result = clock_gettime(CLOCK_MONOTONIC, &ts);
  DCHECK_EQ(0, result);
  USE(result);
  static const uint64_t kNsecPerSec = 1000000000;
  return (ts.tv_sec * kNsecPerSec) + ts.tv_nsec;
}

void LinuxPerfJitLogger::LogRecordedBuffer(
    Handle<AbstractCode> abstract_code,
    MaybeHandle<SharedFunctionInfo> maybe_shared, const char* name,
    int length) {
  if (FLAG_perf_basic_prof_only_functions &&
      (abstract_code->kind() != CodeKind::INTERPRETED_FUNCTION &&
       abstract_code->kind() != CodeKind::TURBOFAN &&
       abstract_code->kind() != CodeKind::MAGLEV &&
       abstract_code->kind() != CodeKind::BASELINE)) {
    return;
  }

  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  if (perf_output_handle_ == nullptr) return;

  // We only support non-interpreted functions.
  if (!abstract_code->IsCode()) return;
  Handle<Code> code = Handle<Code>::cast(abstract_code);
  DCHECK(code->raw_instruction_start() == code->address() + Code::kHeaderSize);

  // Debug info has to be emitted first.
  Handle<SharedFunctionInfo> shared;
  if (FLAG_perf_prof && maybe_shared.ToHandle(&shared)) {
    // TODO(herhut): This currently breaks for js2wasm/wasm2js functions.
    if (code->kind() != CodeKind::JS_TO_WASM_FUNCTION &&
        code->kind() != CodeKind::WASM_TO_JS_FUNCTION) {
      LogWriteDebugInfo(code, shared);
    }
  }

  const char* code_name = name;
  uint8_t* code_pointer = reinterpret_cast<uint8_t*>(code->InstructionStart());

  // Unwinding info comes right after debug info.
  if (FLAG_perf_prof_unwinding_info) LogWriteUnwindingInfo(*code);

  WriteJitCodeLoadEntry(code_pointer, code->InstructionSize(), code_name,
                        length);
}

#if V8_ENABLE_WEBASSEMBLY
void LinuxPerfJitLogger::LogRecordedBuffer(const wasm::WasmCode* code,
                                           const char* name, int length) {
  base::LockGuard<base::RecursiveMutex> guard_file(file_mutex_.Pointer());

  if (perf_output_handle_ == nullptr) return;

  if (FLAG_perf_prof_annotate_wasm) LogWriteDebugInfo(code);

  WriteJitCodeLoadEntry(code->instructions().begin(),
                        code->instructions().length(), name, length);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void LinuxPerfJitLogger::WriteJitCodeLoadEntry(const uint8_t* code_pointer,
                                               uint32_t code_size,
                                               const char* name,
                                               int name_length) {
  PerfJitCodeLoad code_load;
  code_load.event_ = PerfJitCodeLoad::kLoad;
  code_load.size_ = sizeof(code_load) + name_length + 1 + code_size;
  code_load.time_stamp_ = GetTimestamp();
  code_load.process_id_ = static_cast<uint32_t>(process_id_);
  code_load.thread_id_ = static_cast<uint32_t>(base::OS::GetCurrentThreadId());
  code_load.vma_ = reinterpret_cast<uint64_t>(code_pointer);
  code_load.code_address_ = reinterpret_cast<uint64_t>(code_pointer);
  code_load.code_size_ = code_size;
  code_load.code_id_ = code_index_;

  code_index_++;

  LogWriteBytes(reinterpret_cast<const char*>(&code_load), sizeof(code_load));
  LogWriteBytes(name, name_length);
  LogWriteBytes(kStringTerminator, sizeof(kStringTerminator));
  LogWriteBytes(reinterpret_cast<const char*>(code_pointer), code_size);
}

namespace {

constexpr char kUnknownScriptNameString[] = "<unknown>";
constexpr size_t kUnknownScriptNameStringLen =
    arraysize(kUnknownScriptNameString) - 1;

namespace {
base::Vector<const char> GetScriptName(Object maybeScript,
                                       std::unique_ptr<char[]>* storage,
                                       const DisallowGarbageCollection& no_gc) {
  if (maybeScript.IsScript()) {
    Object name_or_url = Script::cast(maybeScript).GetNameOrSourceURL();
    if (name_or_url.IsSeqOneByteString()) {
      SeqOneByteString str = SeqOneByteString::cast(name_or_url);
      return {reinterpret_cast<char*>(str.GetChars(no_gc)),
              static_cast<size_t>(str.length())};
    } else if (name_or_url.IsString()) {
      int length;
      *storage = String::cast(name_or_url)
                     .ToCString(DISALLOW_NULLS, FAST_STRING_TRAVERSAL, &length);
      return {storage->get(), static_cast<size_t>(length)};
    }
  }
  return {kUnknownScriptNameString, kUnknownScriptNameStringLen};
}

}  // namespace

SourcePositionInfo GetSourcePositionInfo(Handle<Code> code,
                                         Handle<SharedFunctionInfo> function,
                                         SourcePosition pos) {
  DisallowGarbageCollection disallow;
  if (code->is_turbofanned()) {
    return pos.FirstInfo(code);
  } else {
    return SourcePositionInfo(pos, function);
  }
}

}  // namespace

void LinuxPerfJitLogger::LogWriteDebugInfo(Handle<Code> code,
                                           Handle<SharedFunctionInfo> shared) {
  // Line ends of all scripts have been initialized prior to this.
  DisallowGarbageCollection no_gc;
  // The WasmToJS wrapper stubs have source position entries.
  if (!shared->HasSourceCode()) return;

  PerfJitCodeDebugInfo debug_info;
  uint32_t size = sizeof(debug_info);

  ByteArray source_position_table = code->SourcePositionTable(*shared);
  // Compute the entry count and get the names of all scripts.
  // Avoid additional work if the script name is repeated. Multiple script
  // names only occur for cross-script inlining.
  uint32_t entry_count = 0;
  Object last_script = Smi::zero();
  std::vector<base::Vector<const char>> script_names;
  for (SourcePositionTableIterator iterator(source_position_table);
       !iterator.done(); iterator.Advance()) {
    SourcePositionInfo info(
        GetSourcePositionInfo(code, shared, iterator.source_position()));
    Object current_script = *info.script;
    if (current_script != last_script) {
      std::unique_ptr<char[]> name_storage;
      auto name = GetScriptName(shared->script(), &name_storage, no_gc);
      script_names.push_back(name);
      // Add the size of the name after each entry.
      size += name.size() + sizeof(kStringTerminator);
      last_script = current_script;
    } else {
      size += sizeof(kRepeatedNameMarker);
    }
    entry_count++;
  }
  if (entry_count == 0) return;

  debug_info.event_ = PerfJitCodeLoad::kDebugInfo;
  debug_info.time_stamp_ = GetTimestamp();
  debug_info.address_ = code->InstructionStart();
  debug_info.entry_count_ = entry_count;

  // Add the sizes of fixed parts of entries.
  size += entry_count * sizeof(PerfJitDebugEntry);

  int padding = ((size + 7) & (~7)) - size;
  debug_info.size_ = size + padding;
  LogWriteBytes(reinterpret_cast<const char*>(&debug_info), sizeof(debug_info));

  Address code_start = code->InstructionStart();

  last_script = Smi::zero();
  int script_names_index = 0;
  for (SourcePositionTableIterator iterator(source_position_table);
       !iterator.done(); iterator.Advance()) {
    SourcePositionInfo info(
        GetSourcePositionInfo(code, shared, iterator.source_position()));
    PerfJitDebugEntry entry;
    // The entry point of the function will be placed straight after the ELF
    // header when processed by "perf inject". Adjust the position addresses
    // accordingly.
    entry.address_ = code_start + iterator.code_offset() + kElfHeaderSize;
    entry.line_number_ = info.line + 1;
    entry.column_ = info.column + 1;
    LogWriteBytes(reinterpret_cast<const char*>(&entry), sizeof(entry));
    Object current_script = *info.script;
    if (current_script != last_script) {
      auto name_string = script_names[script_names_index];
      LogWriteBytes(name_string.begin(),
                    static_cast<uint32_t>(name_string.size()));
      LogWriteBytes(kStringTerminator, sizeof(kStringTerminator));
      script_names_index++;
      last_script = current_script;
    } else {
      // Use the much shorter kRepeatedNameMarker for repeated names.
      LogWriteBytes(kRepeatedNameMarker, sizeof(kRepeatedNameMarker));
    }
  }
  char padding_bytes[8] = {0};
  LogWriteBytes(padding_bytes, padding);
}

#if V8_ENABLE_WEBASSEMBLY
void LinuxPerfJitLogger::LogWriteDebugInfo(const wasm::WasmCode* code) {
  wasm::WasmModuleSourceMap* source_map =
      code->native_module()->GetWasmSourceMap();
  wasm::WireBytesRef code_ref =
      code->native_module()->module()->functions[code->index()].code;
  uint32_t code_offset = code_ref.offset();
  uint32_t code_end_offset = code_ref.end_offset();

  uint32_t entry_count = 0;
  uint32_t size = 0;

  if (!source_map || !source_map->IsValid() ||
      !source_map->HasSource(code_offset, code_end_offset)) {
    return;
  }

  for (SourcePositionTableIterator iterator(code->source_positions());
       !iterator.done(); iterator.Advance()) {
    uint32_t offset = iterator.source_position().ScriptOffset() + code_offset;
    if (!source_map->HasValidEntry(code_offset, offset)) continue;
    entry_count++;
    size += source_map->GetFilename(offset).size() + 1;
  }

  if (entry_count == 0) return;

  PerfJitCodeDebugInfo debug_info;

  debug_info.event_ = PerfJitCodeLoad::kDebugInfo;
  debug_info.time_stamp_ = GetTimestamp();
  debug_info.address_ =
      reinterpret_cast<uintptr_t>(code->instructions().begin());
  debug_info.entry_count_ = entry_count;

  size += sizeof(debug_info);
  // Add the sizes of fixed parts of entries.
  size += entry_count * sizeof(PerfJitDebugEntry);

  int padding = ((size + 7) & (~7)) - size;
  debug_info.size_ = size + padding;
  LogWriteBytes(reinterpret_cast<const char*>(&debug_info), sizeof(debug_info));

  uintptr_t code_begin =
      reinterpret_cast<uintptr_t>(code->instructions().begin());

  for (SourcePositionTableIterator iterator(code->source_positions());
       !iterator.done(); iterator.Advance()) {
    uint32_t offset = iterator.source_position().ScriptOffset() + code_offset;
    if (!source_map->HasValidEntry(code_offset, offset)) continue;
    PerfJitDebugEntry entry;
    // The entry point of the function will be placed straight after the ELF
    // header when processed by "perf inject". Adjust the position addresses
    // accordingly.
    entry.address_ = code_begin + iterator.code_offset() + kElfHeaderSize;
    entry.line_number_ =
        static_cast<int>(source_map->GetSourceLine(offset)) + 1;
    entry.column_ = 1;
    LogWriteBytes(reinterpret_cast<const char*>(&entry), sizeof(entry));
    std::string name_string = source_map->GetFilename(offset);
    LogWriteBytes(name_string.c_str(), static_cast<int>(name_string.size()));
    LogWriteBytes(kStringTerminator, sizeof(kStringTerminator));
  }

  char padding_bytes[8] = {0};
  LogWriteBytes(padding_bytes, padding);
}
#endif  // V8_ENABLE_WEBASSEMBLY

void LinuxPerfJitLogger::LogWriteUnwindingInfo(Code code) {
  PerfJitCodeUnwindingInfo unwinding_info_header;
  unwinding_info_header.event_ = PerfJitCodeLoad::kUnwindingInfo;
  unwinding_info_header.time_stamp_ = GetTimestamp();
  unwinding_info_header.eh_frame_hdr_size_ = EhFrameConstants::kEhFrameHdrSize;

  if (code.has_unwinding_info()) {
    unwinding_info_header.unwinding_size_ = code.unwinding_info_size();
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

  if (code.has_unwinding_info()) {
    LogWriteBytes(reinterpret_cast<const char*>(code.unwinding_info_start()),
                  code.unwinding_info_size());
  } else {
    OFStream perf_output_stream(perf_output_handle_);
    EhFrameWriter::WriteEmptyEhFrame(perf_output_stream);
  }

  char padding_bytes[] = "\0\0\0\0\0\0\0\0";
  DCHECK_LT(padding_size, static_cast<int>(sizeof(padding_bytes)));
  LogWriteBytes(padding_bytes, static_cast<int>(padding_size));
}

void LinuxPerfJitLogger::CodeMoveEvent(AbstractCode from, AbstractCode to) {
  // We may receive a CodeMove event if a BytecodeArray object moves. Otherwise
  // code relocation is not supported.
  CHECK(from.IsBytecodeArray());
}

void LinuxPerfJitLogger::LogWriteBytes(const char* bytes, int size) {
  size_t rv = fwrite(bytes, 1, size, perf_output_handle_);
  DCHECK(static_cast<size_t>(size) == rv);
  USE(rv);
}

void LinuxPerfJitLogger::LogWriteHeader() {
  DCHECK_NOT_NULL(perf_output_handle_);
  PerfJitHeader header;

  header.magic_ = PerfJitHeader::kMagic;
  header.version_ = PerfJitHeader::kVersion;
  header.size_ = sizeof(header);
  header.elf_mach_target_ = GetElfMach();
  header.reserved_ = 0xDEADBEEF;
  header.process_id_ = process_id_;
  header.time_stamp_ =
      static_cast<uint64_t>(V8::GetCurrentPlatform()->CurrentClockTimeMillis() *
                            base::Time::kMicrosecondsPerMillisecond);
  header.flags_ = 0;

  LogWriteBytes(reinterpret_cast<const char*>(&header), sizeof(header));
}

}  // namespace internal
}  // namespace v8

#endif  // V8_OS_LINUX
