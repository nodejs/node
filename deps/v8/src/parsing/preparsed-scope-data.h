// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSED_SCOPE_DATA_H_
#define V8_PARSING_PREPARSED_SCOPE_DATA_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "src/globals.h"
#include "src/objects.h"
#include "src/parsing/preparse-data.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

/*

  Skipping inner functions.

  Consider the following code:
  (function eager_outer() {
    function lazy_inner() {
      let a;
      function skip_me() { a; }
    }

    return lazy_inner;
  })();

  ... lazy_inner(); ...

  When parsing the code the first time, eager_outer is parsed and lazy_inner
  (and everything inside it) is preparsed. When lazy_inner is called, we don't
  want to parse or preparse skip_me again. Instead, we want to skip over it,
  since it has already been preparsed once.

  In order to be able to do this, we need to store the information needed for
  allocating the variables in lazy_inner when we preparse it, and then later do
  scope allocation based on that data.

  We need the following data for each scope in lazy_inner's scope tree:
  For each Variable:
  - is_used
  - maybe_assigned
  - has_forced_context_allocation

  For each Scope:
  - inner_scope_calls_eval_.

  PreParsedScopeData implements storing and restoring the above mentioned data.

 */

class PreParsedScopeData {
 public:
  PreParsedScopeData() {}
  ~PreParsedScopeData() {}

  // Saves the information needed for allocating the Scope's (and its
  // subscopes') variables.
  void SaveData(Scope* scope);

  // Save data for a function we might skip later. The data is used later for
  // creating a FunctionLiteral.
  void AddSkippableFunction(int start_position,
                            const PreParseData::FunctionData& function_data);

  // Save variable allocation data for function which contains skippable
  // functions.
  void AddFunction(int start_position,
                   const PreParseData::FunctionData& function_data);

  // FIXME(marja): We need different kinds of data for the two types of
  // functions. For a skippable function we need the end position + the data
  // needed for creating a FunctionLiteral. For a function which contains
  // skippable functions, we need the data affecting context allocation status
  // of the variables (but e.g., no end position). Currently we just save the
  // same data for both. Here we can save less data.

  // Restores the information needed for allocating the Scopes's (and its
  // subscopes') variables.
  void RestoreData(Scope* scope, uint32_t* index_ptr) const;
  void RestoreData(DeclarationScope* scope) const;

  Handle<PodArray<uint32_t>> Serialize(Isolate* isolate) const;
  void Deserialize(PodArray<uint32_t>* array);

  bool Consuming() const { return has_data_; }

  bool Producing() const { return !has_data_; }

  PreParseData::FunctionData FindSkippableFunction(int start_pos) const;

 private:
  friend class ScopeTestHelper;

  void SaveDataForVariable(Variable* var);
  void RestoreDataForVariable(Variable* var, uint32_t* index_ptr) const;
  void SaveDataForInnerScopes(Scope* scope);
  void RestoreDataForInnerScopes(Scope* scope, uint32_t* index_ptr) const;
  bool FindFunctionData(int start_pos, uint32_t* index) const;

  static bool ScopeNeedsData(Scope* scope);
  static bool IsSkippedFunctionScope(Scope* scope);

  // TODO(marja): Make the backing store more efficient once we know exactly
  // what data is needed.
  std::vector<uint32_t> backing_store_;

  // Start pos -> FunctionData. Used for creating FunctionLiterals for skipped
  // functions (when they're actually skipped).
  PreParseData function_index_;
  // Start pos -> position in backing_store_.
  std::unordered_map<uint32_t, uint32_t> function_data_positions_;
  // Start positions of skippable functions.
  std::set<uint32_t> skippable_functions_;

  bool has_data_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreParsedScopeData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSED_SCOPE_DATA_H_
