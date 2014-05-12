// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "checks.h"

#if V8_LIBC_GLIBC || V8_OS_BSD
# include <cxxabi.h>
# include <execinfo.h>
#elif V8_OS_QNX
# include <backtrace.h>
#endif  // V8_LIBC_GLIBC || V8_OS_BSD
#include <stdio.h>

#include "platform.h"
#include "v8.h"

namespace v8 {
namespace internal {

intptr_t HeapObjectTagMask() { return kHeapObjectTagMask; }

// Attempts to dump a backtrace (if supported).
void DumpBacktrace() {
#if V8_LIBC_GLIBC || V8_OS_BSD
  void* trace[100];
  int size = backtrace(trace, ARRAY_SIZE(trace));
  char** symbols = backtrace_symbols(trace, size);
  OS::PrintError("\n==== C stack trace ===============================\n\n");
  if (size == 0) {
    OS::PrintError("(empty)\n");
  } else if (symbols == NULL) {
    OS::PrintError("(no symbols)\n");
  } else {
    for (int i = 1; i < size; ++i) {
      OS::PrintError("%2d: ", i);
      char mangled[201];
      if (sscanf(symbols[i], "%*[^(]%*[(]%200[^)+]", mangled) == 1) {  // NOLINT
        int status;
        size_t length;
        char* demangled = abi::__cxa_demangle(mangled, NULL, &length, &status);
        OS::PrintError("%s\n", demangled != NULL ? demangled : mangled);
        free(demangled);
      } else {
        OS::PrintError("??\n");
      }
    }
  }
  free(symbols);
#elif V8_OS_QNX
  char out[1024];
  bt_accessor_t acc;
  bt_memmap_t memmap;
  bt_init_accessor(&acc, BT_SELF);
  bt_load_memmap(&acc, &memmap);
  bt_sprn_memmap(&memmap, out, sizeof(out));
  OS::PrintError(out);
  bt_addr_t trace[100];
  int size = bt_get_backtrace(&acc, trace, ARRAY_SIZE(trace));
  OS::PrintError("\n==== C stack trace ===============================\n\n");
  if (size == 0) {
    OS::PrintError("(empty)\n");
  } else {
    bt_sprnf_addrs(&memmap, trace, size, const_cast<char*>("%a\n"),
                   out, sizeof(out), NULL);
    OS::PrintError(out);
  }
  bt_unload_memmap(&memmap);
  bt_release_accessor(&acc);
#endif  // V8_LIBC_GLIBC || V8_OS_BSD
}

} }  // namespace v8::internal


// Contains protection against recursive calls (faults while handling faults).
extern "C" void V8_Fatal(const char* file, int line, const char* format, ...) {
  fflush(stdout);
  fflush(stderr);
  i::OS::PrintError("\n\n#\n# Fatal error in %s, line %d\n# ", file, line);
  va_list arguments;
  va_start(arguments, format);
  i::OS::VPrintError(format, arguments);
  va_end(arguments);
  i::OS::PrintError("\n#\n");
  v8::internal::DumpBacktrace();
  fflush(stderr);
  i::OS::Abort();
}


void CheckEqualsHelper(const char* file,
                       int line,
                       const char* expected_source,
                       v8::Handle<v8::Value> expected,
                       const char* value_source,
                       v8::Handle<v8::Value> value) {
  if (!expected->Equals(value)) {
    v8::String::Utf8Value value_str(value);
    v8::String::Utf8Value expected_str(expected);
    V8_Fatal(file, line,
             "CHECK_EQ(%s, %s) failed\n#   Expected: %s\n#   Found: %s",
             expected_source, value_source, *expected_str, *value_str);
  }
}


void CheckNonEqualsHelper(const char* file,
                          int line,
                          const char* unexpected_source,
                          v8::Handle<v8::Value> unexpected,
                          const char* value_source,
                          v8::Handle<v8::Value> value) {
  if (unexpected->Equals(value)) {
    v8::String::Utf8Value value_str(value);
    V8_Fatal(file, line, "CHECK_NE(%s, %s) failed\n#   Value: %s",
             unexpected_source, value_source, *value_str);
  }
}
