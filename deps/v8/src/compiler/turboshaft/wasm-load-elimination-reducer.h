// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_LOAD_ELIMINATION_REDUCER_H_
#define V8_COMPILER_TURBOSHAFT_WASM_LOAD_ELIMINATION_REDUCER_H_

#include <optional>

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/base/doubly-threaded-list.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/turboshaft/snapshot-table-opindex.h"
#include "src/compiler/turboshaft/utils.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/zone/zone.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

// WLE is short for Wasm Load Elimination.
// We need the namespace because we reuse class names below that also exist
// in the LateLoadEliminationReducer, and in the same namespace that'd be
// an ODR violation, i.e. Undefined Behavior.
// TODO(jkummerow): Refactor the two Load Elimination implementations to
// reuse commonalities.
namespace wle {

// We model array length and string canonicalization as fields at negative
// indices.
static constexpr int kArrayLengthFieldIndex = -1;
static constexpr int kStringPrepareForGetCodeunitIndex = -2;
static constexpr int kStringAsWtf16Index = -3;
static constexpr int kAnyConvertExternIndex = -4;
static constexpr int kAssertNotNullIndex = -5;
static constexpr int kGetDescIndex = -6;

// All "load-like" special cases use the same fake size and type. The specific
// values we use don't matter; for accurate alias analysis, the type should
// be "unrelated" to any struct type.
static constexpr wasm::ModuleTypeIndex kLoadLikeType{wasm::kV8MaxWasmTypes + 1};
static constexpr int kLoadLikeSize = 4;  // Chosen by fair dice roll.

struct WasmMemoryAddress {
  OpIndex base;
  int32_t offset;
  wasm::ModuleTypeIndex type_index;
  uint8_t size;
  bool mutability;

  bool operator==(const WasmMemoryAddress& other) const {
    return base == other.base && offset == other.offset &&
           type_index == other.type_index && size == other.size &&
           mutability == other.mutability;
  }
};

inline size_t hash_value(WasmMemoryAddress const& mem) {
  return fast_hash_combine(mem.base, mem.offset, mem.type_index, mem.size,
                           mem.mutability);
}

struct KeyData {
  using Key = SnapshotTableKey<OpIndex, KeyData>;
  WasmMemoryAddress mem = {};
  // Pointers to the previous and the next Keys at the same base.
  Key* prev_same_base = nullptr;
  Key next_same_base = {};
  // Pointers to either the next/previous Keys at the same offset.
  Key* prev_same_offset = nullptr;
  Key next_same_offset = {};
};

struct OffsetListTraits {
  using T = SnapshotTable<OpIndex, KeyData>::Key;
  static T** prev(T t) { return &(t.data().prev_same_offset); }
  static T* next(T t) { return &(t.data().next_same_offset); }
  static bool non_empty(T t) { return t.valid(); }
};

struct BaseListTraits {
  using T = SnapshotTable<OpIndex, KeyData>::Key;
  static T** prev(T t) { return &(t.data().prev_same_base); }
  static T* next(T t) { return &(t.data().next_same_base); }
  static bool non_empty(T t) { return t.valid(); }
};

struct BaseData {
  using Key = SnapshotTable<OpIndex, KeyData>::Key;
  // List of every value at this base that has an offset rather than an index.
  v8::base::DoublyThreadedList<Key, BaseListTraits> with_offsets;
};

class WasmMemoryContentTable
    : public ChangeTrackingSnapshotTable<WasmMemoryContentTable, OpIndex,
                                         KeyData> {
 public:
  using MemoryAddress = WasmMemoryAddress;

  explicit WasmMemoryContentTable(
      PipelineData* data, Zone* zone,
      SparseOpIndexSnapshotTable<bool>& non_aliasing_objects,
      FixedOpIndexSidetable<OpIndex>& replacements, Graph& graph)
      : ChangeTrackingSnapshotTable(zone),
        non_aliasing_objects_(non_aliasing_objects),
        replacements_(replacements),
        data_(data),
        graph_(graph),
        all_keys_(zone),
        base_keys_(zone),
        offset_keys_(zone) {}

  void OnNewKey(Key key, OpIndex value) {
    if (value.valid()) {
      AddKeyInBaseOffsetMaps(key);
    }
  }

  void OnValueChange(Key key, OpIndex old_value, OpIndex new_value) {
    DCHECK_NE(old_value, new_value);
    if (old_value.valid() && !new_value.valid()) {
      RemoveKeyFromBaseOffsetMaps(key);
    } else if (new_value.valid() && !old_value.valid()) {
      AddKeyInBaseOffsetMaps(key);
    } else {
      DCHECK_EQ(new_value.valid(), old_value.valid());
    }
  }

  bool TypesUnrelated(wasm::ModuleTypeIndex type1,
                      wasm::ModuleTypeIndex type2) {
    return wasm::HeapTypesUnrelated(module_->heap_type(type1),
                                    module_->heap_type(type2), module_);
  }

  void Invalidate(int offset, wasm::ModuleTypeIndex type_index) {
    // This is like LateLoadElimination's {InvalidateAtOffset}, but based
    // on Wasm types instead of tracked JS maps.
    auto offset_keys = offset_keys_.find(offset);
    if (offset_keys == offset_keys_.end()) return;
    for (auto it = offset_keys->second.begin();
         it != offset_keys->second.end();) {
      Key key = *it;
      DCHECK_EQ(offset, key.data().mem.offset);
      OpIndex base = key.data().mem.base;

      // If the base is guaranteed non-aliasing, we don't need to clear any
      // other entries. Any existing entry for this base will be overwritten
      // by {Insert(set)}.
      if (non_aliasing_objects_.Get(base)) {
        ++it;
        continue;
      }

      if (TypesUnrelated(type_index, key.data().mem.type_index)) {
        ++it;
        continue;
      }

      it = offset_keys->second.RemoveAt(it);
      Set(key, OpIndex::Invalid());
    }
  }

  void Invalidate(const StructSetOp& set) {
    Invalidate(field_offset(set.type, set.field_index), set.type_index);
  }

  void Invalidate(const StructAtomicRMWOp& rmw_op) {
    Invalidate(field_offset(rmw_op.type, rmw_op.field_index),
               rmw_op.type_index);
  }

  // Invalidates all Keys that are not known as non-aliasing.
  enum class EntriesWithOffsets { kInvalidate, kKeep };
  template <EntriesWithOffsets offsets = EntriesWithOffsets::kInvalidate>
  void InvalidateMaybeAliasing() {
    // We find current active keys through {base_keys_} so that we can bail out
    // for whole buckets non-aliasing buckets (if we had gone through
    // {offset_keys_} instead, then for each key we would've had to check
    // whether it was non-aliasing or not).
    for (auto& base_keys : base_keys_) {
      OpIndex base = base_keys.first;
      if (non_aliasing_objects_.Get(base)) continue;
      if constexpr (offsets == EntriesWithOffsets::kInvalidate) {
        for (auto it = base_keys.second.with_offsets.begin();
             it != base_keys.second.with_offsets.end();) {
          Key key = *it;
          if (key.data().mem.mutability == false) {
            ++it;
            continue;
          }
          // It's important to remove with RemoveAt before Setting the key to
          // invalid, otherwise OnKeyChange will remove {key} from {base_keys},
          // which will invalidate {it}.
          it = base_keys.second.with_offsets.RemoveAt(it);
          Set(key, OpIndex::Invalid());
        }
      }
    }
  }

  // TODO(jkummerow): Move this to the WasmStruct class?
  int field_offset(const wasm::StructType* type, int field_index) {
    return WasmStruct::kHeaderSize + type->field_offset(field_index);
  }

  OpIndex Find(const StructGetOp& get) {
    if (get.is_get_desc()) {
      return FindImpl(ResolveBase(get.object()), kGetDescIndex, get.type_index,
                      kTaggedSize, false);
    }
    int32_t offset = field_offset(get.type, get.field_index);
    uint8_t size = get.type->field(get.field_index).value_kind_size();
    bool mutability = get.type->mutability(get.field_index);
    return FindImpl(ResolveBase(get.object()), offset, get.type_index, size,
                    mutability);
  }

  bool HasValueWithIncorrectMutability(const StructSetOp& set) {
    int32_t offset = field_offset(set.type, set.field_index);
    uint8_t size = set.type->field(set.field_index).value_kind_size();
    bool mutability = set.type->mutability(set.field_index);
    WasmMemoryAddress mem{ResolveBase(set.object()), offset, set.type_index,
                          size, !mutability};
    return all_keys_.find(mem) != all_keys_.end();
  }

  bool LoadLikeMutability(int offset_sentinel) {
    // While strings themselves are immutable, their heap object representation
    // could get rewritten into a ThinString or ExternalString, so we need
    // to consider them mutable (and invalidate such values at calls).
    if (offset_sentinel == kStringPrepareForGetCodeunitIndex) return true;
    return false;
  }

  OpIndex FindLoadLike(OpIndex op_idx, int offset_sentinel) {
    return FindImpl(ResolveBase(op_idx), offset_sentinel, kLoadLikeType,
                    kLoadLikeSize, LoadLikeMutability(offset_sentinel));
  }

  OpIndex FindImpl(OpIndex object, int offset, wasm::ModuleTypeIndex type_index,
                   uint8_t size, bool mutability,
                   OptionalOpIndex index = OptionalOpIndex::Nullopt()) {
    WasmMemoryAddress mem{object, offset, type_index, size, mutability};
    auto key = all_keys_.find(mem);
    if (key == all_keys_.end()) return OpIndex::Invalid();
    return Get(key->second);
  }

  void Insert(const StructSetOp& set) {
    OpIndex base = ResolveBase(set.object());
    int32_t offset = field_offset(set.type, set.field_index);
    uint8_t size = set.type->field(set.field_index).value_kind_size();
    bool mutability = set.type->mutability(set.field_index);
    Insert(base, offset, set.type_index, size, mutability, set.value());
  }

  void Insert(const StructGetOp& get, OpIndex get_idx) {
    if (get.is_get_desc()) {
      Insert(ResolveBase(get.object()), kGetDescIndex, get.type_index,
             kTaggedSize, false, get_idx);
      return;
    }
    OpIndex base = ResolveBase(get.object());
    int32_t offset = field_offset(get.type, get.field_index);
    uint8_t size = get.type->field(get.field_index).value_kind_size();
    bool mutability = get.type->mutability(get.field_index);
    Insert(base, offset, get.type_index, size, mutability, get_idx);
  }

  void InsertLoadLike(OpIndex base_idx, int offset_sentinel,
                      OpIndex value_idx) {
    OpIndex base = ResolveBase(base_idx);
    Insert(base, offset_sentinel, kLoadLikeType, kLoadLikeSize,
           LoadLikeMutability(offset_sentinel), value_idx);
  }

#ifdef DEBUG
  void Print() {
    std::cout << "WasmMemoryContentTable:\n";
    for (const auto& base_keys : base_keys_) {
      for (Key key : base_keys.second.with_offsets) {
        std::cout << "  * " << key.data().mem.base << " - "
                  << key.data().mem.offset << " ==> " << Get(key) << "\n";
      }
    }
  }
#endif  // DEBUG

 private:
  void Insert(OpIndex base, int32_t offset, wasm::ModuleTypeIndex type_index,
              uint8_t size, bool mutability, OpIndex value) {
    DCHECK_EQ(base, ResolveBase(base));

    WasmMemoryAddress mem{base, offset, type_index, size, mutability};
    auto existing_key = all_keys_.find(mem);
    if (existing_key != all_keys_.end()) {
      if (mutability) {
        Set(existing_key->second, value);
      } else {
        SetNoNotify(existing_key->second, value);
      }
      return;
    }

    // Creating a new key.
    Key key = NewKey({mem});
    all_keys_.insert({mem, key});
    if (mutability) {
      Set(key, value);
    } else {
      // Call `SetNoNotify` to avoid calls to `OnNewKey` and `OnValueChanged`.
      SetNoNotify(key, value);
    }
  }

 public:
  OpIndex ResolveBase(OpIndex base) {
    while (true) {
      if (replacements_[base] != OpIndex::Invalid()) {
        base = replacements_[base];
        continue;
      }
      Operation& op = graph_.Get(base);
      if (AssertNotNullOp* check = op.TryCast<AssertNotNullOp>()) {
        base = check->object();
        continue;
      }
      if (WasmTypeCastOp* cast = op.TryCast<WasmTypeCastOp>()) {
        base = cast->object();
        continue;
      }
      break;  // Terminate if nothing happened.
    }
    return base;
  }

  void AddKeyInBaseOffsetMaps(Key key) {
    // Inserting in {base_keys_}.
    OpIndex base = key.data().mem.base;
    auto base_keys = base_keys_.find(base);
    if (base_keys != base_keys_.end()) {
      base_keys->second.with_offsets.PushFront(key);
    } else {
      BaseData data;
      data.with_offsets.PushFront(key);
      base_keys_.insert({base, std::move(data)});
    }

    // Inserting in {offset_keys_}.
    int offset = key.data().mem.offset;
    auto offset_keys = offset_keys_.find(offset);
    if (offset_keys != offset_keys_.end()) {
      offset_keys->second.PushFront(key);
    } else {
      v8::base::DoublyThreadedList<Key, OffsetListTraits> list;
      list.PushFront(key);
      offset_keys_.insert({offset, std::move(list)});
    }
  }

  void RemoveKeyFromBaseOffsetMaps(Key key) {
    // Removing from {base_keys_}.
    v8::base::DoublyThreadedList<Key, BaseListTraits>::Remove(key);
    v8::base::DoublyThreadedList<Key, OffsetListTraits>::Remove(key);
  }

  SparseOpIndexSnapshotTable<bool>& non_aliasing_objects_;
  FixedOpIndexSidetable<OpIndex>& replacements_;

  PipelineData* data_;
  Graph& graph_;

  const wasm::WasmModule* module_ = data_->wasm_module();

  // TODO(dmercadier): consider using a faster datastructure than
  // ZoneUnorderedMap for {all_keys_}, {base_keys_} and {offset_keys_}.

  // A map containing all of the keys, for fast lookup of a specific
  // MemoryAddress.
  ZoneUnorderedMap<WasmMemoryAddress, Key> all_keys_;
  // Map from base OpIndex to keys associated with this base.
  ZoneUnorderedMap<OpIndex, BaseData> base_keys_;
  // Map from offsets to keys associated with this offset.
  ZoneUnorderedMap<int, v8::base::DoublyThreadedList<Key, OffsetListTraits>>
      offset_keys_;
};

}  // namespace wle

class WasmLoadEliminationAnalyzer {
 public:
  using AliasTable = SparseOpIndexSnapshotTable<bool>;
  using AliasKey = AliasTable::Key;
  using AliasSnapshot = AliasTable::Snapshot;

  using MemoryKey = wle::WasmMemoryContentTable::Key;
  using MemorySnapshot = wle::WasmMemoryContentTable::Snapshot;

  WasmLoadEliminationAnalyzer(PipelineData* data, Graph& graph,
                              Zone* phase_zone)
      : graph_(graph),
        phase_zone_(phase_zone),
        replacements_(graph.op_id_count(), phase_zone, &graph),
        non_aliasing_objects_(phase_zone),
        memory_(data, phase_zone, non_aliasing_objects_, replacements_, graph_),
        block_to_snapshot_mapping_(graph.block_count(), phase_zone),
        predecessor_alias_snapshots_(phase_zone),
        predecessor_memory_snapshots_(phase_zone),
        phi_replacements_backups_(phase_zone) {}

  void Run();

  OpIndex Replacement(OpIndex index) { return replacements_[index]; }

 private:
  void ProcessBlock(const Block& block, bool compute_start_snapshot);
  void ProcessStructGet(OpIndex op_idx, const StructGetOp& op);
  void ProcessStructSet(OpIndex op_idx, const StructSetOp& op);
  void ProcessArrayLength(OpIndex op_idx, const ArrayLengthOp& op);
  void ProcessWasmAllocateArray(OpIndex op_idx, const WasmAllocateArrayOp& op);
  void ProcessStringAsWtf16(OpIndex op_idx, const StringAsWtf16Op& op);
  void ProcessStringPrepareForGetCodeUnit(
      OpIndex op_idx, const StringPrepareForGetCodeUnitOp& op);
  void ProcessAnyConvertExtern(OpIndex op_idx, const AnyConvertExternOp& op);
  void ProcessAssertNotNull(OpIndex op_idx, const AssertNotNullOp& op);
  void ProcessAllocate(OpIndex op_idx, const AllocateOp& op);
  void ProcessCall(OpIndex op_idx, const CallOp& op);
  void ProcessPhi(OpIndex op_idx, const PhiOp& op);
  OpIndex MaybeReplacePhi(const PhiOp& phi);

#if V8_ENABLE_WEBASSEMBLY
  void ProcessAtomicRMW(OpIndex op_idx, const StructAtomicRMWOp& op);
#endif

  void DcheckWordBinop(OpIndex op_idx, const WordBinopOp& binop);

  // BeginBlock initializes the various SnapshotTables for {block}, and returns
  // true if {block} is a loop that should be revisited.
  template <bool for_loop_revisit = false>
  bool BeginBlock(const Block* block);
  void FinishBlock(const Block* block);
  // Seals the current snapshot, but discards it. This is used when considering
  // whether a loop should be revisited or not: we recompute the loop header's
  // snapshots, and then revisit the loop if the snapshots contain
  // modifications. If the snapshots are unchanged, we discard them and don't
  // revisit the loop.
  void SealAndDiscard();
  void StoreLoopSnapshotInForwardPredecessor(const Block& loop_header);

  // Returns true if the loop's backedge already has snapshot data (meaning that
  // it was already visited).
  bool BackedgeHasSnapshot(const Block& loop_header) const;

  void InvalidateAllNonAliasingInputs(const Operation& op);
  void InvalidateIfAlias(OpIndex op_idx);

  Graph& graph_;
  Zone* phase_zone_;

  FixedOpIndexSidetable<OpIndex> replacements_;

  AliasTable non_aliasing_objects_;
  wle::WasmMemoryContentTable memory_;

  struct Snapshot {
    AliasSnapshot alias_snapshot;
    MemorySnapshot memory_snapshot;
  };
  FixedBlockSidetable<std::optional<Snapshot>> block_to_snapshot_mapping_;

  // {predecessor_alias_napshots_}, {predecessor_maps_snapshots_} and
  // {predecessor_memory_snapshots_} are used as temporary vectors when starting
  // to process a block. We store them as members to avoid reallocation.
  ZoneVector<AliasSnapshot> predecessor_alias_snapshots_;
  ZoneVector<MemorySnapshot> predecessor_memory_snapshots_;

  // When figuring out whether a Phi change should cause a loop revisit, we
  // need to temporarily store all loop Phis' previous replacements. This
  // ZoneVector thus stores pairs of {phi index, previous phi replacement}.
  ZoneVector<std::pair<OpIndex, OpIndex>> phi_replacements_backups_;
};

template <class Next>
class WasmLoadEliminationReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmLoadElimination)

  void Analyze() {
    if (v8_flags.turboshaft_wasm_load_elimination) {
      DCHECK(AllowHandleDereference::IsAllowed());
      analyzer_.Run();
    }
    Next::Analyze();
  }

#define EMIT_OP(Name)                                                          \
  OpIndex REDUCE_INPUT_GRAPH(Name)(OpIndex ig_index, const Name##Op& op) {     \
    if (v8_flags.turboshaft_wasm_load_elimination) {                           \
      OpIndex ig_replacement_index = analyzer_.Replacement(ig_index);          \
      if (ig_replacement_index.valid()) {                                      \
        OpIndex replacement = Asm().MapToNewGraph(ig_replacement_index);       \
        return replacement;                                                    \
      }                                                                        \
    }                                                                          \
    return Next::ReduceInputGraph##Name(ig_index, op);                         \
  }

  EMIT_OP(ArrayLength)
  EMIT_OP(StringAsWtf16)
  EMIT_OP(StringPrepareForGetCodeUnit)
  EMIT_OP(AnyConvertExtern)

  OpIndex REDUCE_INPUT_GRAPH(StructGet)(OpIndex ig_index,
                                        const StructGetOp& op) {
    // Atomic loads are never eliminated (not even on unshared objects).
    if (v8_flags.turboshaft_wasm_load_elimination && !op.is_atomic()) {
      OpIndex ig_replacement_index = analyzer_.Replacement(ig_index);
      if (ig_replacement_index.valid()) {
        OpIndex replacement = Asm().MapToNewGraph(ig_replacement_index);
        return replacement;
      }
    }
    return Next::ReduceInputGraphStructGet(ig_index, op);
  }

  OpIndex REDUCE_INPUT_GRAPH(StructSet)(OpIndex ig_index,
                                        const StructSetOp& op) {
    if (v8_flags.turboshaft_wasm_load_elimination) {
      OpIndex ig_replacement_index = analyzer_.Replacement(ig_index);
      if (ig_replacement_index.valid()) {
        // For struct.set, "replace with itself" is a sentinel for
        // "unreachable", and those are the only replacements we schedule for
        // this operation.
        DCHECK_EQ(ig_replacement_index, ig_index);
        __ Unreachable();
        return OpIndex::Invalid();
      }
    }
    return Next::ReduceInputGraphStructSet(ig_index, op);
  }

 private:
  WasmLoadEliminationAnalyzer analyzer_{
      Asm().data(), Asm().modifiable_input_graph(), Asm().phase_zone()};
};



#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_LOAD_ELIMINATION_REDUCER_H_
