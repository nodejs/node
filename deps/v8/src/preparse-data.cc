// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "../include/v8stdint.h"

#include "preparse-data-format.h"
#include "preparse-data.h"

#include "checks.h"
#include "globals.h"
#include "hashmap.h"

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
  ASSERT_EQ(5, PreparseDataConstants::kHeaderSize);
#ifdef DEBUG
  prev_start_ = -1;
#endif
}


void CompleteParserRecorder::LogMessage(int start_pos,
                                        int end_pos,
                                        const char* message,
                                        const char* arg_opt,
                                        bool is_reference_error) {
  if (has_error()) return;
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


Vector<unsigned> CompleteParserRecorder::ExtractData() {
  int function_size = function_store_.size();
  int total_size = PreparseDataConstants::kHeaderSize + function_size;
  Vector<unsigned> data = Vector<unsigned>::New(total_size);
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = function_size;
  OS::MemCopy(data.start(), preamble_, sizeof(preamble_));
  if (function_size > 0) {
    function_store_.WriteTo(data.SubVector(PreparseDataConstants::kHeaderSize,
                                           total_size));
  }
  return data;
}


} }  // namespace v8::internal.
