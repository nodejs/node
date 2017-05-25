// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSED_SCOPE_DATA_H_
#define V8_PARSING_PREPARSED_SCOPE_DATA_H_

#include <vector>

#include "src/globals.h"

namespace v8 {
namespace internal {

class PreParsedScopeData {
 public:
  PreParsedScopeData() {}
  ~PreParsedScopeData() {}

  // Whether the scope has variables whose context allocation or
  // maybeassignedness we need to decide based on preparsed scope data.
  static bool HasVariablesWhichNeedAllocationData(Scope* scope);

  class ScopeScope {
   public:
    ScopeScope(PreParsedScopeData* data, ScopeType scope_type,
               int start_position, int end_position);
    ~ScopeScope();

    void MaybeAddVariable(Variable* var);

   private:
    PreParsedScopeData* data_;
    size_t index_in_data_;
    ScopeScope* previous_scope_;

    int inner_scope_count_ = 0;
    int variable_count_ = 0;
    bool got_data_ = false;
    DISALLOW_COPY_AND_ASSIGN(ScopeScope);
  };

 private:
  friend class ScopeTestHelper;

  // TODO(marja): Make the backing store more efficient once we know exactly
  // what data is needed.
  std::vector<int> backing_store_;
  ScopeScope* current_scope_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PreParsedScopeData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSED_SCOPE_DATA_H_
