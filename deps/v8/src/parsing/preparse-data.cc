// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/preparse-data.h"
#include "src/base/hashmap.h"
#include "src/base/logging.h"
#include "src/globals.h"
#include "src/objects-inl.h"
#include "src/parsing/parser.h"
#include "src/parsing/preparse-data-format.h"

namespace v8 {
namespace internal {

void ParserLogger::LogFunction(int start, int end, int num_parameters,
                               int function_length,
                               bool has_duplicate_parameters, int literals,
                               int properties, LanguageMode language_mode,
                               bool uses_super_property, bool calls_eval,
                               int num_inner_functions) {
  function_store_.Add(start);
  function_store_.Add(end);
  function_store_.Add(num_parameters);
  function_store_.Add(function_length);
  function_store_.Add(literals);
  function_store_.Add(properties);
  function_store_.Add(
      FunctionEntry::EncodeFlags(language_mode, uses_super_property, calls_eval,
                                 has_duplicate_parameters));
  function_store_.Add(num_inner_functions);
}

ParserLogger::ParserLogger() {
  preamble_[PreparseDataConstants::kMagicOffset] =
      PreparseDataConstants::kMagicNumber;
  preamble_[PreparseDataConstants::kVersionOffset] =
      PreparseDataConstants::kCurrentVersion;
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = 0;
  preamble_[PreparseDataConstants::kSizeOffset] = 0;
  DCHECK_EQ(4, PreparseDataConstants::kHeaderSize);
#ifdef DEBUG
  prev_start_ = -1;
#endif
}

ScriptData* ParserLogger::GetScriptData() {
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


}  // namespace internal
}  // namespace v8.
