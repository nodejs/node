// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_H_
#define V8_PARSING_PREPARSE_DATA_H_

#include "src/allocation.h"
#include "src/base/hashmap.h"
#include "src/collector.h"
#include "src/messages.h"
#include "src/parsing/preparse-data-format.h"

namespace v8 {
namespace internal {

class ScriptData {
 public:
  ScriptData(const byte* data, int length);
  ~ScriptData() {
    if (owns_data_) DeleteArray(data_);
  }

  const byte* data() const { return data_; }
  int length() const { return length_; }
  bool rejected() const { return rejected_; }

  void Reject() { rejected_ = true; }

  void AcquireDataOwnership() {
    DCHECK(!owns_data_);
    owns_data_ = true;
  }

  void ReleaseDataOwnership() {
    DCHECK(owns_data_);
    owns_data_ = false;
  }

 private:
  bool owns_data_ : 1;
  bool rejected_ : 1;
  const byte* data_;
  int length_;

  DISALLOW_COPY_AND_ASSIGN(ScriptData);
};

class PreParserLogger final {
 public:
  PreParserLogger()
      : end_(-1),
        num_parameters_(-1),
        function_length_(-1),
        has_duplicate_parameters_(false),
        num_inner_functions_(-1) {}

  void LogFunction(int end, int num_parameters, int function_length,
                   bool has_duplicate_parameters, int literals, int properties,
                   int num_inner_functions) {
    end_ = end;
    num_parameters_ = num_parameters;
    function_length_ = function_length;
    has_duplicate_parameters_ = has_duplicate_parameters;
    literals_ = literals;
    properties_ = properties;
    num_inner_functions_ = num_inner_functions;
  }

  int end() const { return end_; }
  int num_parameters() const {
    return num_parameters_;
  }
  int function_length() const {
    return function_length_;
  }
  bool has_duplicate_parameters() const {
    return has_duplicate_parameters_;
  }
  int literals() const {
    return literals_;
  }
  int properties() const {
    return properties_;
  }
  int num_inner_functions() const { return num_inner_functions_; }

 private:
  int end_;
  // For function entries.
  int num_parameters_;
  int function_length_;
  bool has_duplicate_parameters_;
  int literals_;
  int properties_;
  int num_inner_functions_;
};

class ParserLogger final {
 public:
  ParserLogger();

  void LogFunction(int start, int end, int num_parameters, int function_length,
                   bool has_duplicate_parameters, int literals, int properties,
                   LanguageMode language_mode, bool uses_super_property,
                   bool calls_eval, int num_inner_functions);

  ScriptData* GetScriptData();

 private:
  Collector<unsigned> function_store_;
  unsigned preamble_[PreparseDataConstants::kHeaderSize];

#ifdef DEBUG
  int prev_start_;
#endif
};


}  // namespace internal
}  // namespace v8.

#endif  // V8_PARSING_PREPARSE_DATA_H_
