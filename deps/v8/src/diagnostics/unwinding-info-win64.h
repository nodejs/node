// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_UNWINDING_INFO_WIN64_H_
#define V8_DIAGNOSTICS_UNWINDING_INFO_WIN64_H_

#include "include/v8.h"
#include "include/v8config.h"
#include "src/common/globals.h"

#if defined(V8_OS_WIN_X64)
#include "src/base/win32-headers.h"

namespace v8 {
namespace internal {

namespace win64_unwindinfo {

#define CRASH_HANDLER_FUNCTION_NAME CrashForExceptionInNonABICompliantCodeRange
#define CRASH_HANDLER_FUNCTION_NAME_STRING \
  "CrashForExceptionInNonABICompliantCodeRange"

static const int kPushRbpInstructionLength = 1;
static const int kMovRbpRspInstructionLength = 3;
static const int kRbpPrefixCodes = 2;
static const int kRbpPrefixLength =
    kPushRbpInstructionLength + kMovRbpRspInstructionLength;

/**
 * Returns true if V8 is configured to emit unwinding data for embedded in the
 * pdata/xdata sections of the executable. Currently, this happens when V8 is
 * built with "v8_win64_unwinding_info = true".
 */
bool CanEmitUnwindInfoForBuiltins();

/**
 * Returns true if V8 if we can register unwinding data for the whole code range
 * of an isolate or WASM module. The first page of the code range is reserved
 * and writable, to be used to store unwind data, as documented in:
 * https://docs.microsoft.com/en-us/cpp/build/exception-handling-x64.
 * In jitless mode V8 does not allocate any executable memory itself so the only
 * non-abi-compliant code range is in the embedded blob.
 */
bool CanRegisterUnwindInfoForNonABICompliantCodeRange();

/**
 * Registers a custom exception handler for exceptions in V8-generated code.
 */
void SetUnhandledExceptionCallback(
    v8::UnhandledExceptionCallback unhandled_exception_callback);

/**
 * Returns a vector of bytes that contains the Win64 unwind data used for all
 * V8 builtin functions.
 */
std::vector<uint8_t> GetUnwindInfoForBuiltinFunctions();

void RegisterNonABICompliantCodeRange(void* start, size_t size_in_bytes);
void UnregisterNonABICompliantCodeRange(void* start);

class BuiltinUnwindInfo {
 public:
  BuiltinUnwindInfo() : is_leaf_function_(true) {}
  explicit BuiltinUnwindInfo(const std::vector<int>& fp_offsets)
      : is_leaf_function_(false), fp_offsets_(fp_offsets) {}

  bool is_leaf_function() const { return is_leaf_function_; }
  const std::vector<int>& fp_offsets() const { return fp_offsets_; }

 private:
  bool is_leaf_function_;
  std::vector<int> fp_offsets_;
};

class XdataEncoder {
 public:
  explicit XdataEncoder(const Assembler& assembler)
      : assembler_(assembler), current_push_rbp_offset_(-1) {}

  void onPushRbp();
  void onMovRbpRsp();

  BuiltinUnwindInfo unwinding_info() const {
    return BuiltinUnwindInfo(fp_offsets_);
  }

 private:
  const Assembler& assembler_;
  std::vector<int> fp_offsets_;
  int current_push_rbp_offset_;
};

}  // namespace win64_unwindinfo

}  // namespace internal
}  // namespace v8

#endif  // defined(V8_OS_WIN_X64)

#endif  // V8_DIAGNOSTICS_UNWINDING_INFO_WIN64_H_
