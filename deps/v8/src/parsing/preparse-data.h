// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_H_
#define V8_PARSING_PREPARSE_DATA_H_

#include <unordered_map>

#include "src/allocation.h"
#include "src/base/hashmap.h"
#include "src/collector.h"
#include "src/messages.h"
namespace v8 {
namespace internal {

class PreParserLogger final {
 public:
  PreParserLogger()
      : end_(-1),
        num_parameters_(-1),
        num_inner_functions_(-1) {}

  void LogFunction(int end, int num_parameters, int num_inner_functions) {
    end_ = end;
    num_parameters_ = num_parameters;
    num_inner_functions_ = num_inner_functions;
  }

  int end() const { return end_; }
  int num_parameters() const {
    return num_parameters_;
  }
  int num_inner_functions() const { return num_inner_functions_; }

 private:
  int end_;
  // For function entries.
  int num_parameters_;
  int num_inner_functions_;
};

class PreParseData final {
 public:
  struct FunctionData {
    int end;
    int num_parameters;
    int num_inner_functions;
    LanguageMode language_mode;
    bool uses_super_property : 1;

    FunctionData() : end(kNoSourcePosition) {}

    FunctionData(int end, int num_parameters, int num_inner_functions,
                 LanguageMode language_mode, bool uses_super_property)
        : end(end),
          num_parameters(num_parameters),
          num_inner_functions(num_inner_functions),
          language_mode(language_mode),
          uses_super_property(uses_super_property) {}

    bool is_valid() const {
      DCHECK_IMPLIES(end < 0, end == kNoSourcePosition);
      return end != kNoSourcePosition;
    }
  };

  FunctionData GetFunctionData(int start) const;
  void AddFunctionData(int start, FunctionData&& data);
  void AddFunctionData(int start, const FunctionData& data);
  size_t size() const;

  typedef std::unordered_map<int, FunctionData>::const_iterator const_iterator;
  const_iterator begin() const;
  const_iterator end() const;

 private:
  std::unordered_map<int, FunctionData> functions_;
};

}  // namespace internal
}  // namespace v8.

#endif  // V8_PARSING_PREPARSE_DATA_H_
