// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/v8stdint.h"
#include "src/base/logging.h"
#include "src/compiler.h"
#include "src/globals.h"
#include "src/hashmap.h"
#include "src/preparse-data.h"
#include "src/preparse-data-format.h"

namespace v8 {
namespace internal {


CompleteParserRecorder::CompleteParserRecorder()
    : function_store_(0) {
  preamble_[PreparseDataConstants::kMagicOffset] =
      PreparseDataConstants::kMagicNumber;
  preamble_[PreparseDataConstants::kVersionOffset] =
      PreparseDataConstants::kCurrentVersion;
  preamble_[PreparseDataConstants::kHasErrorOffset] = false;
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = 0;
  preamble_[PreparseDataConstants::kSizeOffset] = 0;
  DCHECK_EQ(5, PreparseDataConstants::kHeaderSize);
#ifdef DEBUG
  prev_start_ = -1;
#endif
}


void CompleteParserRecorder::LogMessage(int start_pos,
                                        int end_pos,
                                        const char* message,
                                        const char* arg_opt,
                                        bool is_reference_error) {
  if (HasError()) return;
  preamble_[PreparseDataConstants::kHasErrorOffset] = true;
  function_store_.Reset();
  STATIC_ASSERT(PreparseDataConstants::kMessageStartPos == 0);
  function_store_.Add(start_pos);
  STATIC_ASSERT(PreparseDataConstants::kMessageEndPos == 1);
  function_store_.Add(end_pos);
  STATIC_ASSERT(PreparseDataConstants::kMessageArgCountPos == 2);
  function_store_.Add((arg_opt == NULL) ? 0 : 1);
  STATIC_ASSERT(PreparseDataConstants::kIsReferenceErrorPos == 3);
  function_store_.Add(is_reference_error ? 1 : 0);
  STATIC_ASSERT(PreparseDataConstants::kMessageTextPos == 4);
  WriteString(CStrVector(message));
  if (arg_opt != NULL) WriteString(CStrVector(arg_opt));
}


void CompleteParserRecorder::WriteString(Vector<const char> str) {
  function_store_.Add(str.length());
  for (int i = 0; i < str.length(); i++) {
    function_store_.Add(str[i]);
  }
}


ScriptData* CompleteParserRecorder::GetScriptData() {
  int function_size = function_store_.size();
  int total_size = PreparseDataConstants::kHeaderSize + function_size;
  unsigned* data = NewArray<unsigned>(total_size);
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = function_size;
  MemCopy(data, preamble_, sizeof(preamble_));
  if (function_size > 0) {
    function_store_.WriteTo(Vector<unsigned>(
        data + PreparseDataConstants::kHeaderSize, function_size));
  }
  DCHECK(IsAligned(reinterpret_cast<intptr_t>(data), kPointerAlignment));
  ScriptData* result = new ScriptData(reinterpret_cast<byte*>(data),
                                      total_size * sizeof(unsigned));
  result->AcquireDataOwnership();
  return result;
}


} }  // namespace v8::internal.
