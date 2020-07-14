// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_PREPARSE_DATA_H_
#define V8_PARSING_PREPARSE_DATA_H_

#include <memory>

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/utils/vector.h"
#include "src/zone/zone-chunk-list.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

template <typename T>
class PodArray;

class Parser;
class PreParser;
class PreparseData;
class ZonePreparseData;
class AstValueFactory;

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

struct PreparseByteDataConstants {
#ifdef DEBUG
  static constexpr int kMagicValue = 0xC0DE0DE;

  static constexpr size_t kUint32Size = 5;
  static constexpr size_t kVarint32MinSize = 3;
  static constexpr size_t kVarint32MaxSize = 7;
  static constexpr size_t kVarint32EndMarker = 0xF1;
  static constexpr size_t kUint8Size = 2;
  static constexpr size_t kQuarterMarker = 0xF2;
  static constexpr size_t kPlaceholderSize = kUint32Size;
#else
  static constexpr size_t kUint32Size = 4;
  static constexpr size_t kVarint32MinSize = 1;
  static constexpr size_t kVarint32MaxSize = 5;
  static constexpr size_t kUint8Size = 1;
  static constexpr size_t kPlaceholderSize = 0;
#endif

  static const size_t kSkippableFunctionMinDataSize =
      4 * kVarint32MinSize + 1 * kUint8Size;
  static const size_t kSkippableFunctionMaxDataSize =
      4 * kVarint32MaxSize + 1 * kUint8Size;
};

class V8_EXPORT_PRIVATE PreparseDataBuilder : public ZoneObject,
                                              public PreparseByteDataConstants {
 public:
  // Create a PreparseDataBuilder object which will collect data as we
  // parse.
  explicit PreparseDataBuilder(Zone* zone, PreparseDataBuilder* parent_builder,
                               std::vector<void*>* children_buffer);
  ~PreparseDataBuilder() {}

  PreparseDataBuilder* parent() const { return parent_; }

  // For gathering the inner function data and splitting it up according to the
  // laziness boundaries. Each lazy function gets its own
  // ProducedPreparseData, and so do all lazy functions inside it.
  class DataGatheringScope {
   public:
    explicit DataGatheringScope(PreParser* preparser)
        : preparser_(preparser), builder_(nullptr) {}

    void Start(DeclarationScope* function_scope);
    void SetSkippableFunction(DeclarationScope* function_scope,
                              int function_length, int num_inner_functions);
    inline ~DataGatheringScope() {
      if (builder_ == nullptr) return;
      Close();
    }

   private:
    void Close();

    PreParser* preparser_;
    PreparseDataBuilder* builder_;

    DISALLOW_COPY_AND_ASSIGN(DataGatheringScope);
  };

  class V8_EXPORT_PRIVATE ByteData : public ZoneObject,
                                     public PreparseByteDataConstants {
   public:
    ByteData()
        : byte_data_(nullptr), index_(0), free_quarters_in_last_byte_(0) {}

    void Start(std::vector<uint8_t>* buffer);
    void Finalize(Zone* zone);

    Handle<PreparseData> CopyToHeap(Isolate* isolate, int children_length);
    Handle<PreparseData> CopyToOffThreadHeap(OffThreadIsolate* isolate,
                                             int children_length);
    inline ZonePreparseData* CopyToZone(Zone* zone, int children_length);

    void Reserve(size_t bytes);
    void Add(uint8_t byte);
    int length() const;

    void WriteVarint32(uint32_t data);
    void WriteUint8(uint8_t data);
    void WriteQuarter(uint8_t data);

#ifdef DEBUG
    void WriteUint32(uint32_t data);
    // For overwriting previously written data at position 0.
    void SaveCurrentSizeAtFirstUint32();
#endif

   private:
    union {
      struct {
        // Only used during construction (is_finalized_ == false).
        std::vector<uint8_t>* byte_data_;
        int index_;
      };
      // Once the data is finalized, it lives in a Zone, this implies
      // is_finalized_ == true.
      Vector<uint8_t> zone_byte_data_;
    };
    uint8_t free_quarters_in_last_byte_;

#ifdef DEBUG
    bool is_finalized_ = false;
#endif
  };

  // Saves the information needed for allocating the Scope's (and its
  // subscopes') variables.
  void SaveScopeAllocationData(DeclarationScope* scope, Parser* parser);

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

  bool HasInnerFunctions() const;
  bool HasData() const;
  bool HasDataForParent() const;

  static bool ScopeNeedsData(Scope* scope);

 private:
  friend class BuilderProducedPreparseData;

  Handle<PreparseData> Serialize(Isolate* isolate);
  Handle<PreparseData> Serialize(OffThreadIsolate* isolate);
  ZonePreparseData* Serialize(Zone* zone);

  void FinalizeChildren(Zone* zone);
  void AddChild(PreparseDataBuilder* child);

  void SaveDataForScope(Scope* scope);
  void SaveDataForVariable(Variable* var);
  void SaveDataForInnerScopes(Scope* scope);
  bool SaveDataForSkippableFunction(PreparseDataBuilder* builder);

  void CopyByteData(Zone* zone);

  PreparseDataBuilder* parent_;
  ByteData byte_data_;
  union {
    ScopedPtrList<PreparseDataBuilder> children_buffer_;
    Vector<PreparseDataBuilder*> children_;
  };

  DeclarationScope* function_scope_;
  int function_length_;
  int num_inner_functions_;
  int num_inner_with_data_;

  // Whether we've given up producing the data for this function.
  bool bailed_out_ : 1;
  bool has_data_ : 1;

#ifdef DEBUG
  bool finalized_children_ = false;
#endif

  DISALLOW_COPY_AND_ASSIGN(PreparseDataBuilder);
};

class ProducedPreparseData : public ZoneObject {
 public:
  // If there is data (if the Scope contains skippable inner functions), move
  // the data into the heap and return a Handle to it; otherwise return a null
  // MaybeHandle.
  virtual Handle<PreparseData> Serialize(Isolate* isolate) = 0;

  // If there is data (if the Scope contains skippable inner functions), move
  // the data into the heap and return a Handle to it; otherwise return a null
  // MaybeHandle.
  virtual Handle<PreparseData> Serialize(OffThreadIsolate* isolate) = 0;

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
  V8_EXPORT_PRIVATE static std::unique_ptr<ConsumedPreparseData> For(
      Isolate* isolate, Handle<PreparseData> data);

  // Creates a ConsumedPreparseData representing the data of an off-heap
  // ZonePreparseData |data|.
  static std::unique_ptr<ConsumedPreparseData> For(Zone* zone,
                                                   ZonePreparseData* data);

  virtual ~ConsumedPreparseData() = default;

  virtual ProducedPreparseData* GetDataForSkippableFunction(
      Zone* zone, int start_position, int* end_position, int* num_parameters,
      int* function_length, int* num_inner_functions, bool* uses_super_property,
      LanguageMode* language_mode) = 0;

  // Restores the information needed for allocating the Scope's (and its
  // subscopes') variables.
  virtual void RestoreScopeAllocationData(DeclarationScope* scope,
                                          AstValueFactory* ast_value_factory,
                                          Zone* zone) = 0;

 protected:
  ConsumedPreparseData() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConsumedPreparseData);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_PREPARSE_DATA_H_
