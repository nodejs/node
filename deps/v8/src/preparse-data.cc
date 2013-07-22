// Copyright 2010 the V8 project authors. All rights reserved.
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

#include "../include/v8stdint.h"

#include "preparse-data-format.h"
#include "preparse-data.h"

#include "checks.h"
#include "globals.h"
#include "hashmap.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// FunctionLoggingParserRecorder

FunctionLoggingParserRecorder::FunctionLoggingParserRecorder()
    : function_store_(0),
      is_recording_(true),
      pause_count_(0) {
  preamble_[PreparseDataConstants::kMagicOffset] =
      PreparseDataConstants::kMagicNumber;
  preamble_[PreparseDataConstants::kVersionOffset] =
      PreparseDataConstants::kCurrentVersion;
  preamble_[PreparseDataConstants::kHasErrorOffset] = false;
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = 0;
  preamble_[PreparseDataConstants::kSymbolCountOffset] = 0;
  preamble_[PreparseDataConstants::kSizeOffset] = 0;
  ASSERT_EQ(6, PreparseDataConstants::kHeaderSize);
#ifdef DEBUG
  prev_start_ = -1;
#endif
}


void FunctionLoggingParserRecorder::LogMessage(int start_pos,
                                               int end_pos,
                                               const char* message,
                                               const char* arg_opt) {
  if (has_error()) return;
  preamble_[PreparseDataConstants::kHasErrorOffset] = true;
  function_store_.Reset();
  STATIC_ASSERT(PreparseDataConstants::kMessageStartPos == 0);
  function_store_.Add(start_pos);
  STATIC_ASSERT(PreparseDataConstants::kMessageEndPos == 1);
  function_store_.Add(end_pos);
  STATIC_ASSERT(PreparseDataConstants::kMessageArgCountPos == 2);
  function_store_.Add((arg_opt == NULL) ? 0 : 1);
  STATIC_ASSERT(PreparseDataConstants::kMessageTextPos == 3);
  WriteString(CStrVector(message));
  if (arg_opt != NULL) WriteString(CStrVector(arg_opt));
  is_recording_ = false;
}


void FunctionLoggingParserRecorder::WriteString(Vector<const char> str) {
  function_store_.Add(str.length());
  for (int i = 0; i < str.length(); i++) {
    function_store_.Add(str[i]);
  }
}


// ----------------------------------------------------------------------------
// PartialParserRecorder -  Record both function entries and symbols.

Vector<unsigned> PartialParserRecorder::ExtractData() {
  int function_size = function_store_.size();
  int total_size = PreparseDataConstants::kHeaderSize + function_size;
  Vector<unsigned> data = Vector<unsigned>::New(total_size);
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = function_size;
  preamble_[PreparseDataConstants::kSymbolCountOffset] = 0;
  OS::MemCopy(data.start(), preamble_, sizeof(preamble_));
  int symbol_start = PreparseDataConstants::kHeaderSize + function_size;
  if (function_size > 0) {
    function_store_.WriteTo(data.SubVector(PreparseDataConstants::kHeaderSize,
                                           symbol_start));
  }
  return data;
}


// ----------------------------------------------------------------------------
// CompleteParserRecorder -  Record both function entries and symbols.

CompleteParserRecorder::CompleteParserRecorder()
    : FunctionLoggingParserRecorder(),
      literal_chars_(0),
      symbol_store_(0),
      symbol_keys_(0),
      string_table_(vector_compare),
      symbol_id_(0) {
}


void CompleteParserRecorder::LogSymbol(int start,
                                       int hash,
                                       bool is_ascii,
                                       Vector<const byte> literal_bytes) {
  Key key = { is_ascii, literal_bytes };
  HashMap::Entry* entry = string_table_.Lookup(&key, hash, true);
  int id = static_cast<int>(reinterpret_cast<intptr_t>(entry->value));
  if (id == 0) {
    // Copy literal contents for later comparison.
    key.literal_bytes =
        Vector<const byte>::cast(literal_chars_.AddBlock(literal_bytes));
    // Put (symbol_id_ + 1) into entry and increment it.
    id = ++symbol_id_;
    entry->value = reinterpret_cast<void*>(id);
    Vector<Key> symbol = symbol_keys_.AddBlock(1, key);
    entry->key = &symbol[0];
  }
  WriteNumber(id - 1);
}


Vector<unsigned> CompleteParserRecorder::ExtractData() {
  int function_size = function_store_.size();
  // Add terminator to symbols, then pad to unsigned size.
  int symbol_size = symbol_store_.size();
  int padding = sizeof(unsigned) - (symbol_size % sizeof(unsigned));
  symbol_store_.AddBlock(padding, PreparseDataConstants::kNumberTerminator);
  symbol_size += padding;
  int total_size = PreparseDataConstants::kHeaderSize + function_size
      + (symbol_size / sizeof(unsigned));
  Vector<unsigned> data = Vector<unsigned>::New(total_size);
  preamble_[PreparseDataConstants::kFunctionsSizeOffset] = function_size;
  preamble_[PreparseDataConstants::kSymbolCountOffset] = symbol_id_;
  OS::MemCopy(data.start(), preamble_, sizeof(preamble_));
  int symbol_start = PreparseDataConstants::kHeaderSize + function_size;
  if (function_size > 0) {
    function_store_.WriteTo(data.SubVector(PreparseDataConstants::kHeaderSize,
                                           symbol_start));
  }
  if (!has_error()) {
    symbol_store_.WriteTo(
        Vector<byte>::cast(data.SubVector(symbol_start, total_size)));
  }
  return data;
}


void CompleteParserRecorder::WriteNumber(int number) {
  ASSERT(number >= 0);

  int mask = (1 << 28) - 1;
  for (int i = 28; i > 0; i -= 7) {
    if (number > mask) {
      symbol_store_.Add(static_cast<byte>(number >> i) | 0x80u);
      number &= mask;
    }
    mask >>= 7;
  }
  symbol_store_.Add(static_cast<byte>(number));
}


} }  // namespace v8::internal.
