// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSED_SCOPE_DATA_H_
#define V8_PARSING_PREPARSED_SCOPE_DATA_H_

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

  // Restores the information needed for allocating the Scopes's (and its
  // subscopes') variables.
  void RestoreData(Scope* scope, int* index_ptr) const;
  void RestoreData(DeclarationScope* scope) const;

  FixedUint32Array* Serialize(Isolate* isolate) const;
  void Deserialize(Handle<FixedUint32Array> array);

  bool Consuming() const { return has_data_; }

  bool Producing() const { return !has_data_; }

  PreParseData::FunctionData FindFunction(int start_pos) const;

 private:
  friend class ScopeTestHelper;

  void SaveDataForVariable(Variable* var);
  void RestoreDataForVariable(Variable* var, int* index_ptr) const;
  void SaveDataForInnerScopes(Scope* scope);
  void RestoreDataForInnerScopes(Scope* scope, int* index_ptr) const;
  bool FindFunctionData(int start_pos, int* index) const;

  static bool ScopeNeedsData(Scope* scope);
  static bool IsSkippedFunctionScope(Scope* scope);

  // TODO(marja): Make the backing store more efficient once we know exactly
  // what data is needed.
  std::vector<byte> backing_store_;

  // Start pos -> FunctionData.
  PreParseData function_index_;
  // Start pos -> position in backing_store_.
  std::unordered_map<uint32_t, uint32_t> function_data_positions_;

  bool has_data_ = false;

  DISALLOW_COPY_AND_ASSIGN(PreParsedScopeData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSED_SCOPE_DATA_H_
