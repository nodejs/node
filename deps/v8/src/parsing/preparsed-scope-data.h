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
#include "src/zone/zone-chunk-list.h"

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
  class ByteData : public ZoneObject {
   public:
    explicit ByteData(Zone* zone)
        : backing_store_(zone), free_quarters_in_last_byte_(0) {}

    void WriteUint32(uint32_t data);
    void WriteUint8(uint8_t data);
    void WriteQuarter(uint8_t data);

    // For overwriting previously written data at position 0.
    void OverwriteFirstUint32(uint32_t data);

    Handle<PodArray<uint8_t>> Serialize(Isolate* isolate);

    size_t size() const { return backing_store_.size(); }

   private:
    ZoneChunkList<uint8_t> backing_store_;
    uint8_t free_quarters_in_last_byte_;
  };

  // Create a ProducedPreParsedScopeData object which will collect data as we
  // parse.
  ProducedPreParsedScopeData(Zone* zone, ProducedPreParsedScopeData* parent);

  // Create a ProducedPreParsedScopeData which is just a proxy for a previous
  // produced PreParsedScopeData.
  ProducedPreParsedScopeData(Handle<PreParsedScopeData> data, Zone* zone);

  ProducedPreParsedScopeData* parent() const { return parent_; }

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
    ProducedPreParsedScopeData* produced_preparsed_scope_data_;

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

  // If there is data (if the Scope contains skippable inner functions), move
  // the data into the heap and return a Handle to it; otherwise return a null
  // MaybeHandle.
  MaybeHandle<PreParsedScopeData> Serialize(Isolate* isolate);

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

  ProducedPreParsedScopeData* parent_;

  ByteData* byte_data_;
  ZoneChunkList<ProducedPreParsedScopeData*> data_for_inner_functions_;

  // Whether we've given up producing the data for this function.
  bool bailed_out_;

  // ProducedPreParsedScopeData can also mask a Handle<PreParsedScopeData>
  // which was produced already earlier. This happens for deeper lazy functions.
  Handle<PreParsedScopeData> previously_produced_preparsed_scope_data_;

  DISALLOW_COPY_AND_ASSIGN(ProducedPreParsedScopeData);
};

class ConsumedPreParsedScopeData {
 public:
  class ByteData {
   public:
    ByteData()
        : data_(nullptr), index_(0), stored_quarters_(0), stored_byte_(0) {}

    // Reading from the ByteData is only allowed when a ReadingScope is on the
    // stack. This ensures that we have a DisallowHeapAllocation in place
    // whenever ByteData holds a raw pointer into the heap.
    class ReadingScope {
     public:
      ReadingScope(ByteData* consumed_data, PodArray<uint8_t>* data)
          : consumed_data_(consumed_data) {
        consumed_data->data_ = data;
      }
      explicit ReadingScope(ConsumedPreParsedScopeData* parent);
      ~ReadingScope() { consumed_data_->data_ = nullptr; }

     private:
      ByteData* consumed_data_;
      DisallowHeapAllocation no_gc;
    };

    void SetPosition(int position) { index_ = position; }

    int32_t ReadUint32();
    uint8_t ReadUint8();
    uint8_t ReadQuarter();

    size_t RemainingBytes() const {
      DCHECK_NOT_NULL(data_);
      return data_->length() - index_;
    }

    // private:
    PodArray<uint8_t>* data_;
    int index_;
    uint8_t stored_quarters_;
    uint8_t stored_byte_;
  };

  ConsumedPreParsedScopeData();
  ~ConsumedPreParsedScopeData();

  void SetData(Handle<PreParsedScopeData> data);

  bool HasData() const { return !data_.is_null(); }

  ProducedPreParsedScopeData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode);

  // Restores the information needed for allocating the Scope's (and its
  // subscopes') variables.
  void RestoreScopeAllocationData(DeclarationScope* scope);

 private:
  void RestoreData(Scope* scope);
  void RestoreDataForVariable(Variable* var);
  void RestoreDataForInnerScopes(Scope* scope);

  Handle<PreParsedScopeData> data_;
  std::unique_ptr<ByteData> scope_data_;
  // When consuming the data, these indexes point to the data we're going to
  // consume next.
  int child_index_;

  DISALLOW_COPY_AND_ASSIGN(ConsumedPreParsedScopeData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSED_SCOPE_DATA_H_
