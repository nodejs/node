// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_H_
#define V8_PARSING_PREPARSE_DATA_H_

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
class PreparseData;
class ZonePreparseData;

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

  ProducedPreparseData implements storing the above mentioned data and
  ConsumedPreparseData implements restoring it (= setting the context
  allocation status of the variables in a Scope (and its subscopes) based on the
  data).

 */

class PreparseDataBuilder : public ZoneObject {
 public:
  class ByteData;

  // Create a PreparseDataBuilder object which will collect data as we
  // parse.
  PreparseDataBuilder(Zone* zone, PreparseDataBuilder* parent);

  PreparseDataBuilder* parent() const { return parent_; }

  // For gathering the inner function data and splitting it up according to the
  // laziness boundaries. Each lazy function gets its own
  // ProducedPreparseData, and so do all lazy functions inside it.
  class DataGatheringScope {
   public:
    explicit DataGatheringScope(PreParser* preparser)
        : preparser_(preparser), builder_(nullptr) {}

    void Start(DeclarationScope* function_scope);
    ~DataGatheringScope();

   private:
    PreParser* preparser_;
    PreparseDataBuilder* builder_;

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
    if (bailed_out_) return true;
    if (parent_ == nullptr) return false;
    return parent_->ThisOrParentBailedOut();
  }
#endif  // DEBUG

  bool ContainsInnerFunctions() const;
  bool HasData() const;

  static bool ScopeNeedsData(Scope* scope);
  static bool ScopeIsSkippableFunctionScope(Scope* scope);
  void AddSkippableFunction(int start_position, int end_position,
                            int num_parameters, int num_inner_functions,
                            LanguageMode language_mode,
                            bool uses_super_property);

 private:
  friend class BuilderProducedPreparseData;

  Handle<PreparseData> Serialize(Isolate* isolate);
  ZonePreparseData* Serialize(Zone* zone);

  void SaveDataForScope(Scope* scope);
  void SaveDataForVariable(Variable* var);
  void SaveDataForInnerScopes(Scope* scope);

  PreparseDataBuilder* parent_;

  ByteData* byte_data_;
  ZoneChunkList<PreparseDataBuilder*> data_for_inner_functions_;

  // Whether we've given up producing the data for this function.
  bool bailed_out_;

  DISALLOW_COPY_AND_ASSIGN(PreparseDataBuilder);
};

class ProducedPreparseData : public ZoneObject {
 public:
  // If there is data (if the Scope contains skippable inner functions), move
  // the data into the heap and return a Handle to it; otherwise return a null
  // MaybeHandle.
  virtual Handle<PreparseData> Serialize(Isolate* isolate) = 0;

  // If there is data (if the Scope contains skippable inner functions), return
  // an off-heap ZonePreparseData representing the data; otherwise
  // return nullptr.
  virtual ZonePreparseData* Serialize(Zone* zone) = 0;

  // Create a ProducedPreparseData which is a proxy for a previous
  // produced PreparseData in zone.
  static ProducedPreparseData* For(PreparseDataBuilder* builder, Zone* zone);

  // Create a ProducedPreparseData which is a proxy for a previous
  // produced PreparseData on the heap.
  static ProducedPreparseData* For(Handle<PreparseData> data, Zone* zone);

  // Create a ProducedPreparseData which is a proxy for a previous
  // produced PreparseData in zone.
  static ProducedPreparseData* For(ZonePreparseData* data, Zone* zone);
};

class ConsumedPreparseData {
 public:
  // Creates a ConsumedPreparseData representing the data of an on-heap
  // PreparseData |data|.
  static std::unique_ptr<ConsumedPreparseData> For(Isolate* isolate,
                                                   Handle<PreparseData> data);

  // Creates a ConsumedPreparseData representing the data of an off-heap
  // ZonePreparseData |data|.
  static std::unique_ptr<ConsumedPreparseData> For(Zone* zone,
                                                   ZonePreparseData* data);

  virtual ~ConsumedPreparseData() = default;

  virtual ProducedPreparseData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode) = 0;

  // Restores the information needed for allocating the Scope's (and its
  // subscopes') variables.
  virtual void RestoreScopeAllocationData(DeclarationScope* scope) = 0;

 protected:
  ConsumedPreparseData() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsumedPreparseData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSE_DATA_H_
