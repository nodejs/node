// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/unwinding-info-win64.h"

#if defined(V8_OS_WIN_X64)

#include "src/allocation.h"
#include "src/macro-assembler.h"
#include "src/x64/assembler-x64.h"

namespace v8 {
namespace internal {
namespace win64_unwindinfo {

bool CanEmitUnwindInfoForBuiltins() { return FLAG_win64_unwinding_info; }

bool CanRegisterUnwindInfoForNonABICompliantCodeRange() {
  return !FLAG_jitless;
}

bool RegisterUnwindInfoForExceptionHandlingOnly() {
  DCHECK(CanRegisterUnwindInfoForNonABICompliantCodeRange());
  return !IsWindows8OrGreater() || !FLAG_win64_unwinding_info;
}

#pragma pack(push, 1)

/*
 * From Windows SDK ehdata.h, which does not compile with Clang.
 * See https://msdn.microsoft.com/en-us/library/ddssxxy8.aspx.
 */
typedef union _UNWIND_CODE {
  struct {
    unsigned char CodeOffset;
    unsigned char UnwindOp : 4;
    unsigned char OpInfo : 4;
  };
  uint16_t FrameOffset;
} UNWIND_CODE, *PUNWIND_CODE;

typedef struct _UNWIND_INFO {
  unsigned char Version : 3;
  unsigned char Flags : 5;
  unsigned char SizeOfProlog;
  unsigned char CountOfCodes;
  unsigned char FrameRegister : 4;
  unsigned char FrameOffset : 4;
} UNWIND_INFO, *PUNWIND_INFO;

struct V8UnwindData {
  UNWIND_INFO unwind_info;
  UNWIND_CODE unwind_codes[2];

  V8UnwindData() {
    static constexpr int kOpPushNonvol = 0;
    static constexpr int kOpSetFPReg = 3;

    unwind_info.Version = 1;
    unwind_info.Flags = UNW_FLAG_EHANDLER;
    unwind_info.SizeOfProlog = kRbpPrefixLength;
    unwind_info.CountOfCodes = kRbpPrefixCodes;
    unwind_info.FrameRegister = rbp.code();
    unwind_info.FrameOffset = 0;

    unwind_codes[0].CodeOffset = kRbpPrefixLength;  // movq rbp, rsp
    unwind_codes[0].UnwindOp = kOpSetFPReg;
    unwind_codes[0].OpInfo = 0;

    unwind_codes[1].CodeOffset = kPushRbpInstructionLength;  // push rbp
    unwind_codes[1].UnwindOp = kOpPushNonvol;
    unwind_codes[1].OpInfo = rbp.code();
  }
};

struct ExceptionHandlerUnwindData {
  UNWIND_INFO unwind_info;

  ExceptionHandlerUnwindData() {
    unwind_info.Version = 1;
    unwind_info.Flags = UNW_FLAG_EHANDLER;
    unwind_info.SizeOfProlog = 0;
    unwind_info.CountOfCodes = 0;
    unwind_info.FrameRegister = 0;
    unwind_info.FrameOffset = 0;
  }
};

#pragma pack(pop)

v8::UnhandledExceptionCallback unhandled_exception_callback_g = nullptr;

void SetUnhandledExceptionCallback(
    v8::UnhandledExceptionCallback unhandled_exception_callback) {
  unhandled_exception_callback_g = unhandled_exception_callback;
}

// This function is registered as exception handler for V8-generated code as
// part of the registration of unwinding info. It is referenced by
// RegisterNonABICompliantCodeRange(), below, and by the unwinding info for
// builtins declared in the embedded blob.
extern "C" int CRASH_HANDLER_FUNCTION_NAME(
    PEXCEPTION_RECORD ExceptionRecord, ULONG64 EstablisherFrame,
    PCONTEXT ContextRecord, PDISPATCHER_CONTEXT DispatcherContext) {
  if (unhandled_exception_callback_g != nullptr) {
    EXCEPTION_POINTERS info = {ExceptionRecord, ContextRecord};
    return unhandled_exception_callback_g(&info);
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

static constexpr int kMaxExceptionThunkSize = 12;

struct CodeRangeUnwindingRecord {
  RUNTIME_FUNCTION runtime_function;
  V8UnwindData unwind_info;
  uint32_t exception_handler;
  uint8_t exception_thunk[kMaxExceptionThunkSize];
  void* dynamic_table;
};

struct ExceptionHandlerRecord {
  RUNTIME_FUNCTION runtime_function;
  ExceptionHandlerUnwindData unwind_info;
  uint32_t exception_handler;
  uint8_t exception_thunk[kMaxExceptionThunkSize];
};

static decltype(
    &::RtlAddGrowableFunctionTable) add_growable_function_table_func = nullptr;
static decltype(
    &::RtlDeleteGrowableFunctionTable) delete_growable_function_table_func =
    nullptr;

namespace {

void LoadNtdllUnwindingFunctions() {
  static bool loaded = false;
  if (loaded) {
    return;
  }
  loaded = true;

  // Load functions from the ntdll.dll module.
  HMODULE ntdll_module =
      LoadLibraryEx(L"ntdll.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  DCHECK_NOT_NULL(ntdll_module);

  // This fails on Windows 7.
  add_growable_function_table_func =
      reinterpret_cast<decltype(&::RtlAddGrowableFunctionTable)>(
          ::GetProcAddress(ntdll_module, "RtlAddGrowableFunctionTable"));
  DCHECK_IMPLIES(IsWindows8OrGreater(), add_growable_function_table_func);

  delete_growable_function_table_func =
      reinterpret_cast<decltype(&::RtlDeleteGrowableFunctionTable)>(
          ::GetProcAddress(ntdll_module, "RtlDeleteGrowableFunctionTable"));
  DCHECK_IMPLIES(IsWindows8OrGreater(), delete_growable_function_table_func);
}

bool AddGrowableFunctionTable(PVOID* DynamicTable,
                              PRUNTIME_FUNCTION FunctionTable, DWORD EntryCount,
                              DWORD MaximumEntryCount, ULONG_PTR RangeBase,
                              ULONG_PTR RangeEnd) {
  DCHECK(::IsWindows8OrGreater());

  LoadNtdllUnwindingFunctions();
  DCHECK_NOT_NULL(add_growable_function_table_func);

  *DynamicTable = nullptr;
  DWORD status =
      add_growable_function_table_func(DynamicTable, FunctionTable, EntryCount,
                                       MaximumEntryCount, RangeBase, RangeEnd);
  DCHECK((status == 0 && *DynamicTable != nullptr) ||
         status == 0xC000009A);  // STATUS_INSUFFICIENT_RESOURCES
  return (status == 0);
}

void DeleteGrowableFunctionTable(PVOID dynamic_table) {
  DCHECK(::IsWindows8OrGreater());

  LoadNtdllUnwindingFunctions();
  DCHECK_NOT_NULL(delete_growable_function_table_func);

  delete_growable_function_table_func(dynamic_table);
}

}  // namespace

std::vector<uint8_t> GetUnwindInfoForBuiltinFunctions() {
  V8UnwindData xdata;
  return std::vector<uint8_t>(
      reinterpret_cast<uint8_t*>(&xdata),
      reinterpret_cast<uint8_t*>(&xdata) + sizeof(xdata));
}

template <typename Record>
void InitUnwindingRecord(Record* record, size_t code_size_in_bytes) {
  // We assume that the first page of the code range is executable and
  // committed and reserved to contain PDATA/XDATA.

  // All addresses are 32bit relative offsets to start.
  record->runtime_function.BeginAddress = 0;
  record->runtime_function.EndAddress = static_cast<DWORD>(code_size_in_bytes);
  record->runtime_function.UnwindData = offsetof(Record, unwind_info);

  record->exception_handler = offsetof(Record, exception_thunk);

  // Hardcoded thunk.
  MacroAssembler masm(AssemblerOptions{}, NewAssemblerBuffer(64));
  masm.movq(rax, reinterpret_cast<uint64_t>(&CRASH_HANDLER_FUNCTION_NAME));
  masm.jmp(rax);
  DCHECK_GE(masm.buffer_size(), sizeof(record->exception_thunk));
  memcpy(&record->exception_thunk[0], masm.buffer_start(), masm.buffer_size());
}

void RegisterNonABICompliantCodeRange(void* start, size_t size_in_bytes) {
  DCHECK(CanRegisterUnwindInfoForNonABICompliantCodeRange());

  // When the --win64-unwinding-info flag is set, we call
  // RtlAddGrowableFunctionTable to register unwinding info for the whole code
  // range of an isolate or WASM module. This enables the Windows OS stack
  // unwinder to work correctly with V8-generated code, enabling stack walking
  // in Windows debuggers and performance tools. However, the
  // RtlAddGrowableFunctionTable API is only supported on Windows 8 and above.
  //
  // On Windows 7, or when --win64-unwinding-info is not set, we may still need
  // to call RtlAddFunctionTable to register a custom exception handler passed
  // by the embedder (like Crashpad).

  if (RegisterUnwindInfoForExceptionHandlingOnly()) {
    if (unhandled_exception_callback_g) {
      ExceptionHandlerRecord* record = new (start) ExceptionHandlerRecord();
      InitUnwindingRecord(record, size_in_bytes);

      CHECK(::RtlAddFunctionTable(&record->runtime_function, 1,
                                  reinterpret_cast<DWORD64>(start)));

      // Protect reserved page against modifications.
      DWORD old_protect;
      CHECK(VirtualProtect(start, sizeof(CodeRangeUnwindingRecord),
                           PAGE_EXECUTE_READ, &old_protect));
    }
  } else {
    CodeRangeUnwindingRecord* record = new (start) CodeRangeUnwindingRecord();
    InitUnwindingRecord(record, size_in_bytes);

    CHECK(AddGrowableFunctionTable(
        &record->dynamic_table, &record->runtime_function, 1, 1,
        reinterpret_cast<DWORD64>(start),
        reinterpret_cast<DWORD64>(reinterpret_cast<uint8_t*>(start) +
                                  size_in_bytes)));

    // Protect reserved page against modifications.
    DWORD old_protect;
    CHECK(VirtualProtect(start, sizeof(CodeRangeUnwindingRecord),
                         PAGE_EXECUTE_READ, &old_protect));
  }
}

void UnregisterNonABICompliantCodeRange(void* start) {
  DCHECK(CanRegisterUnwindInfoForNonABICompliantCodeRange());

  if (RegisterUnwindInfoForExceptionHandlingOnly()) {
    if (unhandled_exception_callback_g) {
      ExceptionHandlerRecord* record =
          reinterpret_cast<ExceptionHandlerRecord*>(start);
      CHECK(::RtlDeleteFunctionTable(&record->runtime_function));
    }
  } else {
    CodeRangeUnwindingRecord* record =
        reinterpret_cast<CodeRangeUnwindingRecord*>(start);
    if (record->dynamic_table) {
      DeleteGrowableFunctionTable(record->dynamic_table);
    }
  }
}

void XdataEncoder::onPushRbp() {
  current_push_rbp_offset_ = assembler_.pc_offset() - kPushRbpInstructionLength;
}

void XdataEncoder::onMovRbpRsp() {
  if (current_push_rbp_offset_ >= 0 &&
      current_push_rbp_offset_ == assembler_.pc_offset() - kRbpPrefixLength) {
    fp_offsets_.push_back(current_push_rbp_offset_);
  }
}

}  // namespace win64_unwindinfo
}  // namespace internal
}  // namespace v8

#endif  // defined(V8_OS_WIN_X64)
