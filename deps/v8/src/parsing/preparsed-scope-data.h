// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSED_SCOPE_DATA_H_
#define V8_PARSING_PREPARSED_SCOPE_DATA_H_

#include "src/globals.h"
#include "src/handles.h"
#include "src/maybe-handles.h"
#include "src/zone/zone-chunk-list.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

template <typename T>
class PodArray;

class PreParser;
class PreParsedScopeData;
class ZonePreParsedScopeData;

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

class PreParsedScopeDataBuilder : public ZoneObject {
 public:
  class ByteData;

  // Create a PreParsedScopeDataBuilder object which will collect data as we
  // parse.
  PreParsedScopeDataBuilder(Zone* zone, PreParsedScopeDataBuilder* parent);

  PreParsedScopeDataBuilder* parent() const { return parent_; }

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
    PreParsedScopeDataBuilder* builder_;

    DISALLOW_COPY_AND_ASSIGN(DataGatheringScope);
  };

  // Saves the information needed for allocating the Scope's (and its
  // subscopes') variables.
  void SaveScopeAllocationData(DeclarationScope* scope);

  // In some cases, PreParser cannot produce the same Scope structure as
  // Parser. If it happens, we're unable to produce the data that would enable
  // skipping the inner functions of that function.
  void Bailout() {
    bailed_out_ = true;

    // We don't need to call Bailout on existing / future children: the only way
    // to try to retrieve their data is through calling Serialize on the parent,
    // and if the parent is bailed out, it won't call Serialize on its children.
  }

  bool bailed_out() const { return bailed_out_; }

#ifdef DEBUG
  bool ThisOrParentBailedOut() const {
    if (bailed_out_) {
      return true;
    }
    if (parent_ == nullptr) {
      return false;
    }
    return parent_->ThisOrParentBailedOut();
  }
#endif  // DEBUG

  bool ContainsInnerFunctions() const;

  static bool ScopeNeedsData(Scope* scope);
  static bool ScopeIsSkippableFunctionScope(Scope* scope);

 private:
  friend class BuilderProducedPreParsedScopeData;

  virtual MaybeHandle<PreParsedScopeData> Serialize(Isolate* isolate);
  virtual ZonePreParsedScopeData* Serialize(Zone* zone);

  void AddSkippableFunction(int start_position, int end_position,
                            int num_parameters, int num_inner_functions,
                            LanguageMode language_mode,
                            bool uses_super_property);

  void SaveDataForScope(Scope* scope);
  void SaveDataForVariable(Variable* var);
  void SaveDataForInnerScopes(Scope* scope);

  PreParsedScopeDataBuilder* parent_;

  ByteData* byte_data_;
  ZoneChunkList<PreParsedScopeDataBuilder*> data_for_inner_functions_;

  // Whether we've given up producing the data for this function.
  bool bailed_out_;

  DISALLOW_COPY_AND_ASSIGN(PreParsedScopeDataBuilder);
};

class ProducedPreParsedScopeData : public ZoneObject {
 public:
  // If there is data (if the Scope contains skippable inner functions), move
  // the data into the heap and return a Handle to it; otherwise return a null
  // MaybeHandle.
  virtual MaybeHandle<PreParsedScopeData> Serialize(Isolate* isolate) = 0;

  // If there is data (if the Scope contains skippable inner functions), return
  // an off-heap ZonePreParsedScopeData representing the data; otherwise
  // return nullptr.
  virtual ZonePreParsedScopeData* Serialize(Zone* zone) = 0;

  // Create a ProducedPreParsedScopeData which is a proxy for a previous
  // produced PreParsedScopeData in zone.
  static ProducedPreParsedScopeData* For(PreParsedScopeDataBuilder* builder,
                                         Zone* zone);

  // Create a ProducedPreParsedScopeData which is a proxy for a previous
  // produced PreParsedScopeData on the heap.
  static ProducedPreParsedScopeData* For(Handle<PreParsedScopeData> data,
                                         Zone* zone);

  // Create a ProducedPreParsedScopeData which is a proxy for a previous
  // produced PreParsedScopeData in zone.
  static ProducedPreParsedScopeData* For(ZonePreParsedScopeData* data,
                                         Zone* zone);
};

class ConsumedPreParsedScopeData {
 public:
  // Creates a ConsumedPreParsedScopeData representing the data of an on-heap
  // PreParsedScopeData |data|.
  static std::unique_ptr<ConsumedPreParsedScopeData> For(
      Isolate* isolate, Handle<PreParsedScopeData> data);

  // Creates a ConsumedPreParsedScopeData representing the data of an off-heap
  // ZonePreParsedScopeData |data|.
  static std::unique_ptr<ConsumedPreParsedScopeData> For(
      Zone* zone, ZonePreParsedScopeData* data);

  virtual ~ConsumedPreParsedScopeData() = default;

  virtual ProducedPreParsedScopeData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode) = 0;

  // Restores the information needed for allocating the Scope's (and its
  // subscopes') variables.
  virtual void RestoreScopeAllocationData(DeclarationScope* scope) = 0;

 protected:
  ConsumedPreParsedScopeData() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsumedPreParsedScopeData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSED_SCOPE_DATA_H_
