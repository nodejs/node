// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DIAGNOSTICS_UNWINDING_INFO_WIN64_H_
#define V8_DIAGNOSTICS_UNWINDING_INFO_WIN64_H_

#include "include/v8.h"
#include "include/v8config.h"
#include "src/common/globals.h"

#if defined(V8_OS_WIN64)
#include "src/base/win32-headers.h"

namespace v8 {
namespace internal {

namespace win64_unwindinfo {

#define CRASH_HANDLER_FUNCTION_NAME CrashForExceptionInNonABICompliantCodeRange
#define CRASH_HANDLER_FUNCTION_NAME_STRING \
  "CrashForExceptionInNonABICompliantCodeRange"

static const int kOSPageSize = 4096;

/**
 * Returns true if V8 is configured to emit unwinding data for embedded in the
 * pdata/xdata sections of the executable. Currently, this happens when V8 is
 * built with "v8_win64_unwinding_info = true".
 */
bool CanEmitUnwindInfoForBuiltins();

/**
 * Returns true if V8 if we can register unwinding data for the whole code range
 * of an isolate or Wasm module. The first page of the code range is reserved
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

void RegisterNonABICompliantCodeRange(void* start, size_t size_in_bytes);
void UnregisterNonABICompliantCodeRange(void* start);

/**
 * Default count of RUNTIME_FUNCTION needed. For Windows X64, 1 RUNTIME_FUNCTION
 * covers 4GB range which is sufficient to cover the whole code range of an
 * isolate or Wasm module. For Windows ARM64, 1 RUNTIME_FUNCTION covers
 * kMaxFunctionLength bytes so multiple RUNTIME_FUNCTION structs could be needed
 * to cover the whole code range of an isolate or Wasm module. The extra
 * RUNTIME_FUNCTIONs are assumed following the first one in the reserved page.
 */
static const uint32_t kDefaultRuntimeFunctionCount = 1;

#if defined(V8_OS_WIN_X64)

static const int kPushRbpInstructionLength = 1;
static const int kMovRbpRspInstructionLength = 3;
static const int kRbpPrefixCodes = 2;
static const int kRbpPrefixLength =
    kPushRbpInstructionLength + kMovRbpRspInstructionLength;

/**
 * Returns a vector of bytes that contains the Win X64 unwind data used for all
 * V8 builtin functions.
 */
std::vector<uint8_t> GetUnwindInfoForBuiltinFunctions();

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
      : assembler_(assembler), current_frame_code_offset_(-1) {}

  void onPushRbp();
  void onMovRbpRsp();

  BuiltinUnwindInfo unwinding_info() const {
    return BuiltinUnwindInfo(fp_offsets_);
  }

 private:
  const Assembler& assembler_;
  std::vector<int> fp_offsets_;
  int current_frame_code_offset_;
};

#elif defined(V8_OS_WIN_ARM64)

/**
 * Base on below doc, unwind record has 18 bits (unsigned) to encode function
 * length, besides 2 LSB which are always 0.
 * https://docs.microsoft.com/en-us/cpp/build/arm64-exception-handling#xdata-records
 */
static const int kMaxFunctionLength = ((1 << 18) - 1) << 2;

struct FrameOffsets {
  FrameOffsets();
  bool IsDefault() const;
  int fp_to_saved_caller_fp;
  int fp_to_caller_sp;
};

/**
 * Returns a vector of bytes that contains the Win ARM64 unwind data used for
 * all V8 builtin functions.
 *
 * func_len: length in bytes of current function/region to unwind.
 * fp_adjustment: offset of the saved caller's fp based on fp in current frame.
 *                this is necessary to encode unwind data for Windows stack
 *                unwinder to find correct caller's fp.
 */
std::vector<uint8_t> GetUnwindInfoForBuiltinFunction(
    uint32_t func_len, FrameOffsets fp_adjustment);
class BuiltinUnwindInfo {
 public:
  BuiltinUnwindInfo() : is_leaf_function_(true) {}
  explicit BuiltinUnwindInfo(const std::vector<int>& fp_offsets,
                             const std::vector<FrameOffsets>& fp_adjustments)
      : is_leaf_function_(false),
        fp_offsets_(fp_offsets),
        fp_adjustments_(fp_adjustments) {}

  const std::vector<FrameOffsets>& fp_adjustments() const {
    return fp_adjustments_;
  }

  bool is_leaf_function() const { return is_leaf_function_; }
  const std::vector<int>& fp_offsets() const { return fp_offsets_; }

 private:
  bool is_leaf_function_;
  std::vector<int> fp_offsets_;
  std::vector<FrameOffsets> fp_adjustments_;
};

class XdataEncoder {
 public:
  explicit XdataEncoder(const Assembler& assembler)
      : assembler_(assembler), current_frame_code_offset_(-1) {}

  void onSaveFpLr();
  void onFramePointerAdjustment(int fp_to_saved_caller_fp, int fp_to_caller_sp);

  BuiltinUnwindInfo unwinding_info() const {
    return BuiltinUnwindInfo(fp_offsets_, fp_adjustments_);
  }

 private:
  const Assembler& assembler_;
  std::vector<int> fp_offsets_;
  int current_frame_code_offset_;
  FrameOffsets current_frame_adjustment_;
  std::vector<FrameOffsets> fp_adjustments_;
};

#endif

}  // namespace win64_unwindinfo
}  // namespace internal
}  // namespace v8

#endif  // V8_OS_WIN64

#endif  // V8_DIAGNOSTICS_UNWINDING_INFO_WIN64_H_
