// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Slightly adapted for inclusion in V8.
// Copyright 2016 the V8 project authors. All rights reserved.

#include "src/base/debug/stack_trace.h"

#include <windows.h>
#include <dbghelp.h>
#include <Shlwapi.h>
#include <stddef.h>

#include <iostream>
#include <memory>

#include "src/base/logging.h"
#include "src/base/macros.h"

namespace v8 {
namespace base {
namespace debug {

namespace {

// Previous unhandled filter. Will be called if not NULL when we intercept an
// exception. Only used in unit tests.
LPTOP_LEVEL_EXCEPTION_FILTER g_previous_filter = NULL;

bool g_dump_stack_in_signal_handler = true;
bool g_initialized_symbols = false;
DWORD g_init_error = ERROR_SUCCESS;

// Prints the exception call stack.
// This is the unit tests exception filter.
long WINAPI StackDumpExceptionFilter(EXCEPTION_POINTERS* info) {  // NOLINT
  if (g_dump_stack_in_signal_handler) {
    debug::StackTrace(info).Print();
  }
  if (g_previous_filter) return g_previous_filter(info);
  return EXCEPTION_CONTINUE_SEARCH;
}

void GetExePath(wchar_t* path_out) {
  GetModuleFileName(NULL, path_out, MAX_PATH);
  path_out[MAX_PATH - 1] = L'\0';
  PathRemoveFileSpec(path_out);
}

bool InitializeSymbols() {
  if (g_initialized_symbols) return g_init_error == ERROR_SUCCESS;
  g_initialized_symbols = true;
  // Defer symbol load until they're needed, use undecorated names, and get line
  // numbers.
  SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME | SYMOPT_LOAD_LINES);
  if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
    g_init_error = GetLastError();
    // TODO(awong): Handle error: SymInitialize can fail with
    // ERROR_INVALID_PARAMETER.
    // When it fails, we should not call debugbreak since it kills the current
    // process (prevents future tests from running or kills the browser
    // process).
    return false;
  }

  // When transferring the binaries e.g. between bots, path put
  // into the executable will get off. To still retrieve symbols correctly,
  // add the directory of the executable to symbol search path.
  // All following errors are non-fatal.
  const size_t kSymbolsArraySize = 1024;
  std::unique_ptr<wchar_t[]> symbols_path(new wchar_t[kSymbolsArraySize]);

  // Note: The below function takes buffer size as number of characters,
  // not number of bytes!
  if (!SymGetSearchPathW(GetCurrentProcess(), symbols_path.get(),
                         kSymbolsArraySize)) {
    g_init_error = GetLastError();
    return false;
  }

  wchar_t exe_path[MAX_PATH];
  GetExePath(exe_path);
  std::wstring new_path(std::wstring(symbols_path.get()) + L";" +
                        std::wstring(exe_path));
  if (!SymSetSearchPathW(GetCurrentProcess(), new_path.c_str())) {
    g_init_error = GetLastError();
    return false;
  }

  g_init_error = ERROR_SUCCESS;
  return true;
}

// For the given trace, attempts to resolve the symbols, and output a trace
// to the ostream os.  The format for each line of the backtrace is:
//
//    <tab>SymbolName[0xAddress+Offset] (FileName:LineNo)
//
// This function should only be called if Init() has been called.  We do not
// LOG(FATAL) here because this code is called might be triggered by a
// LOG(FATAL) itself. Also, it should not be calling complex code that is
// extensible like PathService since that can in turn fire CHECKs.
void OutputTraceToStream(const void* const* trace, size_t count,
                         std::ostream* os) {
  for (size_t i = 0; (i < count) && os->good(); ++i) {
    const int kMaxNameLength = 256;
    DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

    // Code adapted from MSDN example:
    // http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
    ULONG64 buffer[(sizeof(SYMBOL_INFO) + kMaxNameLength * sizeof(wchar_t) +
                    sizeof(ULONG64) - 1) /
                   sizeof(ULONG64)];
    memset(buffer, 0, sizeof(buffer));

    // Initialize symbol information retrieval structures.
    DWORD64 sym_displacement = 0;
    PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = kMaxNameLength - 1;
    BOOL has_symbol =
        SymFromAddr(GetCurrentProcess(), frame, &sym_displacement, symbol);

    // Attempt to retrieve line number information.
    DWORD line_displacement = 0;
    IMAGEHLP_LINE64 line = {};
    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    BOOL has_line = SymGetLineFromAddr64(GetCurrentProcess(), frame,
                                         &line_displacement, &line);

    // Output the backtrace line.
    (*os) << "\t";
    if (has_symbol) {
      (*os) << symbol->Name << " [0x" << trace[i] << "+" << sym_displacement
            << "]";
    } else {
      // If there is no symbol information, add a spacer.
      (*os) << "(No symbol) [0x" << trace[i] << "]";
    }
    if (has_line) {
      (*os) << " (" << line.FileName << ":" << line.LineNumber << ")";
    }
    (*os) << "\n";
  }
}

}  // namespace

bool EnableInProcessStackDumping() {
  // Add stack dumping support on exception on windows. Similar to OS_POSIX
  // signal() handling in process_util_posix.cc.
  g_previous_filter = SetUnhandledExceptionFilter(&StackDumpExceptionFilter);
  g_dump_stack_in_signal_handler = true;

  // Need to initialize symbols early in the process or else this fails on
  // swarming (since symbols are in different directory than in the exes) and
  // also release x64.
  return InitializeSymbols();
}

void DisableSignalStackDump() {
  g_dump_stack_in_signal_handler = false;
}

// Disable optimizations for the StackTrace::StackTrace function. It is
// important to disable at least frame pointer optimization ("y"), since
// that breaks CaptureStackBackTrace() and prevents StackTrace from working
// in Release builds (it may still be janky if other frames are using FPO,
// but at least it will make it further).
#if defined(V8_CC_MSVC)
#pragma optimize("", off)
#endif

StackTrace::StackTrace() {
  // When walking our own stack, use CaptureStackBackTrace().
  count_ = CaptureStackBackTrace(0, arraysize(trace_), trace_, NULL);
}

#if defined(V8_CC_MSVC)
#pragma optimize("", on)
#endif

StackTrace::StackTrace(EXCEPTION_POINTERS* exception_pointers) {
  InitTrace(exception_pointers->ContextRecord);
}

StackTrace::StackTrace(const CONTEXT* context) { InitTrace(context); }

void StackTrace::InitTrace(const CONTEXT* context_record) {
  // StackWalk64 modifies the register context in place, so we have to copy it
  // so that downstream exception handlers get the right context.  The incoming
  // context may have had more register state (YMM, etc) than we need to unwind
  // the stack. Typically StackWalk64 only needs integer and control registers.
  CONTEXT context_copy;
  memcpy(&context_copy, context_record, sizeof(context_copy));
  context_copy.ContextFlags = CONTEXT_INTEGER | CONTEXT_CONTROL;

  // When walking an exception stack, we need to use StackWalk64().
  count_ = 0;
  // Initialize stack walking.
  STACKFRAME64 stack_frame;
  memset(&stack_frame, 0, sizeof(stack_frame));
#if defined(_WIN64)
  int machine_type = IMAGE_FILE_MACHINE_AMD64;
  stack_frame.AddrPC.Offset = context_record->Rip;
  stack_frame.AddrFrame.Offset = context_record->Rbp;
  stack_frame.AddrStack.Offset = context_record->Rsp;
#else
  int machine_type = IMAGE_FILE_MACHINE_I386;
  stack_frame.AddrPC.Offset = context_record->Eip;
  stack_frame.AddrFrame.Offset = context_record->Ebp;
  stack_frame.AddrStack.Offset = context_record->Esp;
#endif
  stack_frame.AddrPC.Mode = AddrModeFlat;
  stack_frame.AddrFrame.Mode = AddrModeFlat;
  stack_frame.AddrStack.Mode = AddrModeFlat;
  while (StackWalk64(machine_type, GetCurrentProcess(), GetCurrentThread(),
                     &stack_frame, &context_copy, NULL,
                     &SymFunctionTableAccess64, &SymGetModuleBase64, NULL) &&
         count_ < arraysize(trace_)) {
    trace_[count_++] = reinterpret_cast<void*>(stack_frame.AddrPC.Offset);
  }

  for (size_t i = count_; i < arraysize(trace_); ++i) trace_[i] = NULL;
}

void StackTrace::Print() const { OutputToStream(&std::cerr); }

void StackTrace::OutputToStream(std::ostream* os) const {
  InitializeSymbols();
  if (g_init_error != ERROR_SUCCESS) {
    (*os) << "Error initializing symbols (" << g_init_error
          << ").  Dumping unresolved backtrace:\n";
    for (size_t i = 0; (i < count_) && os->good(); ++i) {
      (*os) << "\t" << trace_[i] << "\n";
    }
  } else {
    (*os) << "\n";
    (*os) << "==== C stack trace ===============================\n";
    (*os) << "\n";
    OutputTraceToStream(trace_, count_, os);
  }
}

}  // namespace debug
}  // namespace base
}  // namespace v8
