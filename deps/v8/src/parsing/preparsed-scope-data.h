// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSED_SCOPE_DATA_H_
#define V8_PARSING_PREPARSED_SCOPE_DATA_H_

#include <set>
#include <unordered_map>
#include <vector>

#include "src/globals.h"
#include "src/handles.h"
#include "src/objects/shared-function-info.h"
#include "src/parsing/preparse-data.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class PreParser;
class PreParsedScopeData;

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

  ProducedPreParsedScopeData implements storing the above mentioned data and
  ConsumedPreParsedScopeData implements restoring it (= setting the context
  allocation status of the variables in a Scope (and its subscopes) based on the
  data).

 */

class ProducedPreParsedScopeData : public ZoneObject {
 public:
  // Create a ProducedPreParsedScopeData object which will collect data as we
  // parse.
  explicit ProducedPreParsedScopeData(Zone* zone)
      : backing_store_(zone),
        data_for_inner_functions_(zone),
        scope_data_start_(-1) {}

  // Create a ProducedPreParsedScopeData which is just a proxy for a previous
  // produced PreParsedScopeData.
  ProducedPreParsedScopeData(Handle<PreParsedScopeData> data, Zone* zone)
      : backing_store_(zone),
        data_for_inner_functions_(zone),
        scope_data_start_(-1),
        previously_produced_preparsed_scope_data_(data) {}

  // For gathering the inner function data and splitting it up according to the
  // laziness boundaries. Each lazy function gets its own
  // ProducedPreParsedScopeData, and so do all lazy functions inside it.
  class DataGatheringScope {
   public:
    DataGatheringScope(DeclarationScope* function_scope, PreParser* preparser);
    ~DataGatheringScope();

    void MarkFunctionAsSkippable(int end_position, int num_inner_functions);

   private:
    DeclarationScope* function_scope_;
    PreParser* preparser_;
    ProducedPreParsedScopeData* parent_data_;

    DISALLOW_COPY_AND_ASSIGN(DataGatheringScope);
  };

  // Saves the information needed for allocating the Scope's (and its
  // subscopes') variables.
  void SaveScopeAllocationData(DeclarationScope* scope);

  // If there is data (if the Scope contains skippable inner functions), move
  // the data into the heap and return a Handle to it; otherwise return a null
  // MaybeHandle.
  MaybeHandle<PreParsedScopeData> Serialize(Isolate* isolate) const;

  static bool ScopeNeedsData(Scope* scope);
  static bool ScopeIsSkippableFunctionScope(Scope* scope);

 private:
  void AddSkippableFunction(int start_position, int end_position,
                            int num_parameters, int num_inner_functions,
                            LanguageMode language_mode,
                            bool uses_super_property);

  void SaveDataForScope(Scope* scope);
  void SaveDataForVariable(Variable* var);
  void SaveDataForInnerScopes(Scope* scope);

  // TODO(marja): Make the backing store more efficient once we know exactly
  // what data is needed.
  ZoneDeque<uint32_t> backing_store_;
  ZoneDeque<ProducedPreParsedScopeData*> data_for_inner_functions_;
  // The backing store contains data about inner functions and then data about
  // this scope's (and its subscopes') variables. scope_data_start_ marks where
  // the latter starts.
  int scope_data_start_;

  // ProducedPreParsedScopeData can also mask a Handle<PreParsedScopeData>
  // which was produced already earlier. This happens for deeper lazy functions.
  Handle<PreParsedScopeData> previously_produced_preparsed_scope_data_;

  DISALLOW_COPY_AND_ASSIGN(ProducedPreParsedScopeData);
};

class ConsumedPreParsedScopeData {
 public:
  // Real data starts from index 1 (see data format description in the .cc
  // file).
  ConsumedPreParsedScopeData() : index_(1), child_index_(0) {}
  ~ConsumedPreParsedScopeData() {}

  void SetData(Handle<PreParsedScopeData> data);

  bool HasData() const { return !data_.is_null(); }

  ProducedPreParsedScopeData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode);

  // Restores the information needed for allocating the Scope's (and its
  // subscopes') variables.
  void RestoreScopeAllocationData(DeclarationScope* scope);

  // Skips the data about skippable functions, moves straight to the scope
  // allocation data. Useful for tests which don't want to verify only the scope
  // allocation data.
  void SkipFunctionDataForTesting();

 private:
  void RestoreData(Scope* scope, PodArray<uint32_t>* scope_data);
  void RestoreDataForVariable(Variable* var, PodArray<uint32_t>* scope_data);
  void RestoreDataForInnerScopes(Scope* scope, PodArray<uint32_t>* scope_data);

  Handle<PreParsedScopeData> data_;
  // When consuming the data, these indexes point to the data we're going to
  // consume next.
  int index_;
  int child_index_;

  DISALLOW_COPY_AND_ASSIGN(ConsumedPreParsedScopeData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSED_SCOPE_DATA_H_
