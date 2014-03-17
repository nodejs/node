// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "hydrogen-check-elimination.h"
#include "hydrogen-alias-analysis.h"
#include "hydrogen-flow-engine.h"

#define GLOBAL 1

// Only collect stats in debug mode.
#if DEBUG
#define INC_STAT(x) phase_->x++
#else
#define INC_STAT(x)
#endif

// For code de-uglification.
#define TRACE(x) if (FLAG_trace_check_elimination) PrintF x

namespace v8 {
namespace internal {

typedef UniqueSet<Map>* MapSet;

struct HCheckTableEntry {
  HValue* object_;  // The object being approximated. NULL => invalid entry.
  HValue* check_;   // The last check instruction.
  MapSet maps_;     // The set of known maps for the object.
};


// The main datastructure used during check elimination, which stores a
// set of known maps for each object.
class HCheckTable : public ZoneObject {
 public:
  static const int kMaxTrackedObjects = 10;

  explicit HCheckTable(HCheckEliminationPhase* phase)
    : phase_(phase),
      cursor_(0),
      size_(0) {
  }

  // The main processing of instructions.
  HCheckTable* Process(HInstruction* instr, Zone* zone) {
    switch (instr->opcode()) {
      case HValue::kCheckMaps: {
        ReduceCheckMaps(HCheckMaps::cast(instr));
        break;
      }
      case HValue::kCheckValue: {
        ReduceCheckValue(HCheckValue::cast(instr));
        break;
      }
      case HValue::kLoadNamedField: {
        ReduceLoadNamedField(HLoadNamedField::cast(instr));
        break;
      }
      case HValue::kStoreNamedField: {
        ReduceStoreNamedField(HStoreNamedField::cast(instr));
        break;
      }
      case HValue::kCompareMap: {
        ReduceCompareMap(HCompareMap::cast(instr));
        break;
      }
      case HValue::kTransitionElementsKind: {
        ReduceTransitionElementsKind(
            HTransitionElementsKind::cast(instr));
        break;
      }
      case HValue::kCheckMapValue: {
        ReduceCheckMapValue(HCheckMapValue::cast(instr));
        break;
      }
      case HValue::kCheckHeapObject: {
        ReduceCheckHeapObject(HCheckHeapObject::cast(instr));
        break;
      }
      default: {
        // If the instruction changes maps uncontrollably, drop everything.
        if (instr->CheckGVNFlag(kChangesMaps) ||
            instr->CheckGVNFlag(kChangesOsrEntries)) {
          Kill();
        }
      }
      // Improvements possible:
      // - eliminate redundant HCheckSmi, HCheckInstanceType instructions
      // - track which values have been HCheckHeapObject'd
    }

    return this;
  }

  // Global analysis: Copy state to successor block.
  HCheckTable* Copy(HBasicBlock* succ, HBasicBlock* from_block, Zone* zone) {
    HCheckTable* copy = new(phase_->zone()) HCheckTable(phase_);
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* old_entry = &entries_[i];
      HCheckTableEntry* new_entry = &copy->entries_[i];
      // TODO(titzer): keep the check if this block dominates the successor?
      new_entry->object_ = old_entry->object_;
      new_entry->check_ = NULL;
      new_entry->maps_ = old_entry->maps_->Copy(phase_->zone());
    }
    copy->cursor_ = cursor_;
    copy->size_ = size_;

    // Branch-sensitive analysis for certain comparisons may add more facts
    // to the state for the successor on the true branch.
    bool learned = false;
    HControlInstruction* end = succ->predecessors()->at(0)->end();
    if (succ->predecessors()->length() == 1 && end->SuccessorAt(0) == succ) {
      if (end->IsCompareMap()) {
        // Learn on the true branch of if(CompareMap(x)).
        HCompareMap* cmp = HCompareMap::cast(end);
        HValue* object = cmp->value()->ActualValue();
        HCheckTableEntry* entry = copy->Find(object);
        if (entry == NULL) {
          copy->Insert(object, cmp->map());
        } else {
          MapSet list = new(phase_->zone()) UniqueSet<Map>();
          list->Add(cmp->map(), phase_->zone());
          entry->maps_ = list;
        }
        learned = true;
      } else if (end->IsCompareObjectEqAndBranch()) {
        // Learn on the true branch of if(CmpObjectEq(x, y)).
        HCompareObjectEqAndBranch* cmp =
          HCompareObjectEqAndBranch::cast(end);
        HValue* left = cmp->left()->ActualValue();
        HValue* right = cmp->right()->ActualValue();
        HCheckTableEntry* le = copy->Find(left);
        HCheckTableEntry* re = copy->Find(right);
        if (le == NULL) {
          if (re != NULL) {
            copy->Insert(left, NULL, re->maps_->Copy(zone));
          }
        } else if (re == NULL) {
          copy->Insert(right, NULL, le->maps_->Copy(zone));
        } else {
          MapSet intersect = le->maps_->Intersect(re->maps_, zone);
          le->maps_ = intersect;
          re->maps_ = intersect->Copy(zone);
        }
        learned = true;
      }
      // Learning on false branches requires storing negative facts.
    }

    if (FLAG_trace_check_elimination) {
      PrintF("B%d checkmaps-table %s from B%d:\n",
             succ->block_id(),
             learned ? "learned" : "copied",
             from_block->block_id());
      copy->Print();
    }

    return copy;
  }

  // Global analysis: Merge this state with the other incoming state.
  HCheckTable* Merge(HBasicBlock* succ, HCheckTable* that,
                     HBasicBlock* that_block, Zone* zone) {
    if (that_block->IsReachable()) {
      if (that->size_ == 0) {
        // If the other state is empty, simply reset.
        size_ = 0;
        cursor_ = 0;
      } else {
        bool compact = false;
        for (int i = 0; i < size_; i++) {
          HCheckTableEntry* this_entry = &entries_[i];
          HCheckTableEntry* that_entry = that->Find(this_entry->object_);
          if (that_entry == NULL) {
            this_entry->object_ = NULL;
            compact = true;
          } else {
            this_entry->maps_ =
                this_entry->maps_->Union(that_entry->maps_, phase_->zone());
            if (this_entry->check_ != that_entry->check_) {
              this_entry->check_ = NULL;
            }
            ASSERT(this_entry->maps_->size() > 0);
          }
        }
        if (compact) Compact();
      }
    }
    if (FLAG_trace_check_elimination) {
      PrintF("B%d checkmaps-table merged with B%d table:\n",
             succ->block_id(), that_block->block_id());
      Print();
    }
    return this;
  }

  void ReduceCheckMaps(HCheckMaps* instr) {
    HValue* object = instr->value()->ActualValue();
    HCheckTableEntry* entry = Find(object);
    if (entry != NULL) {
      // entry found;
      MapSet a = entry->maps_;
      MapSet i = instr->map_set().Copy(phase_->zone());
      if (a->IsSubset(i)) {
        // The first check is more strict; the second is redundant.
        if (entry->check_ != NULL) {
          TRACE(("Replacing redundant CheckMaps #%d at B%d with #%d\n",
              instr->id(), instr->block()->block_id(), entry->check_->id()));
          instr->DeleteAndReplaceWith(entry->check_);
          INC_STAT(redundant_);
        } else {
          TRACE(("Marking redundant CheckMaps #%d at B%d as dead\n",
              instr->id(), instr->block()->block_id()));
          // Mark check as dead but leave it in the graph as a checkpoint for
          // subsequent checks.
          instr->SetFlag(HValue::kIsDead);
          entry->check_ = instr;
          INC_STAT(removed_);
        }
        return;
      }
      i = i->Intersect(a, phase_->zone());
      if (i->size() == 0) {
        // Intersection is empty; probably megamorphic, which is likely to
        // deopt anyway, so just leave things as they are.
        INC_STAT(empty_);
      } else {
        // TODO(titzer): replace the first check with a more strict check
        INC_STAT(narrowed_);
      }
    } else {
      // No entry; insert a new one.
      Insert(object, instr, instr->map_set().Copy(phase_->zone()));
    }
  }

  void ReduceCheckValue(HCheckValue* instr) {
    // Canonicalize HCheckValues; they might have their values load-eliminated.
    HValue* value = instr->Canonicalize();
    if (value == NULL) {
      instr->DeleteAndReplaceWith(instr->value());
      INC_STAT(removed_);
    } else if (value != instr) {
      instr->DeleteAndReplaceWith(value);
      INC_STAT(redundant_);
    }
  }

  void ReduceLoadNamedField(HLoadNamedField* instr) {
    // Reduce a load of the map field when it is known to be a constant.
    if (!IsMapAccess(instr->access())) return;

    HValue* object = instr->object()->ActualValue();
    MapSet maps = FindMaps(object);
    if (maps == NULL || maps->size() != 1) return;  // Not a constant.

    Unique<Map> map = maps->at(0);
    HConstant* constant = HConstant::CreateAndInsertBefore(
        instr->block()->graph()->zone(), map, true, instr);
    instr->DeleteAndReplaceWith(constant);
    INC_STAT(loads_);
  }

  void ReduceCheckMapValue(HCheckMapValue* instr) {
    if (!instr->map()->IsConstant()) return;  // Nothing to learn.

    HValue* object = instr->value()->ActualValue();
    // Match a HCheckMapValue(object, HConstant(map))
    Unique<Map> map = MapConstant(instr->map());
    MapSet maps = FindMaps(object);
    if (maps != NULL) {
      if (maps->Contains(map)) {
        if (maps->size() == 1) {
          // Object is known to have exactly this map.
          instr->DeleteAndReplaceWith(NULL);
          INC_STAT(removed_);
        } else {
          // Only one map survives the check.
          maps->Clear();
          maps->Add(map, phase_->zone());
        }
      }
    } else {
      // No prior information.
      Insert(object, map);
    }
  }

  void ReduceCheckHeapObject(HCheckHeapObject* instr) {
    if (FindMaps(instr->value()->ActualValue()) != NULL) {
      // If the object has known maps, it's definitely a heap object.
      instr->DeleteAndReplaceWith(instr->value());
      INC_STAT(removed_cho_);
    }
  }

  void ReduceStoreNamedField(HStoreNamedField* instr) {
    HValue* object = instr->object()->ActualValue();
    if (instr->has_transition()) {
      // This store transitions the object to a new map.
      Kill(object);
      Insert(object, MapConstant(instr->transition()));
    } else if (IsMapAccess(instr->access())) {
      // This is a store directly to the map field of the object.
      Kill(object);
      if (!instr->value()->IsConstant()) return;
      Insert(object, MapConstant(instr->value()));
    } else {
      // If the instruction changes maps, it should be handled above.
      CHECK(!instr->CheckGVNFlag(kChangesMaps));
    }
  }

  void ReduceCompareMap(HCompareMap* instr) {
    MapSet maps = FindMaps(instr->value()->ActualValue());
    if (maps == NULL) return;
    if (maps->Contains(instr->map())) {
      if (maps->size() == 1) {
        TRACE(("Marking redundant CompareMap #%d at B%d as true\n",
            instr->id(), instr->block()->block_id()));
        instr->set_known_successor_index(0);
        INC_STAT(compares_true_);
      }
    } else {
      TRACE(("Marking redundant CompareMap #%d at B%d as false\n",
          instr->id(), instr->block()->block_id()));
      instr->set_known_successor_index(1);
      INC_STAT(compares_false_);
    }
  }

  void ReduceTransitionElementsKind(HTransitionElementsKind* instr) {
    MapSet maps = FindMaps(instr->object()->ActualValue());
    // Can only learn more about an object that already has a known set of maps.
    if (maps == NULL) return;
    if (maps->Contains(instr->original_map())) {
      // If the object has the original map, it will be transitioned.
      maps->Remove(instr->original_map());
      maps->Add(instr->transitioned_map(), phase_->zone());
    } else {
      // Object does not have the given map, thus the transition is redundant.
      instr->DeleteAndReplaceWith(instr->object());
      INC_STAT(transitions_);
    }
  }

  // Kill everything in the table.
  void Kill() {
    size_ = 0;
    cursor_ = 0;
  }

  // Kill everything in the table that may alias {object}.
  void Kill(HValue* object) {
    bool compact = false;
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* entry = &entries_[i];
      ASSERT(entry->object_ != NULL);
      if (phase_->aliasing_->MayAlias(entry->object_, object)) {
        entry->object_ = NULL;
        compact = true;
      }
    }
    if (compact) Compact();
    ASSERT(Find(object) == NULL);
  }

  void Compact() {
    // First, compact the array in place.
    int max = size_, dest = 0, old_cursor = cursor_;
    for (int i = 0; i < max; i++) {
      if (entries_[i].object_ != NULL) {
        if (dest != i) entries_[dest] = entries_[i];
        dest++;
      } else {
        if (i < old_cursor) cursor_--;
        size_--;
      }
    }
    ASSERT(size_ == dest);
    ASSERT(cursor_ <= size_);

    // Preserve the age of the entries by moving the older entries to the end.
    if (cursor_ == size_) return;  // Cursor already points at end.
    if (cursor_ != 0) {
      // | L = oldest |   R = newest   |       |
      //              ^ cursor         ^ size  ^ MAX
      HCheckTableEntry tmp_entries[kMaxTrackedObjects];
      int L = cursor_;
      int R = size_ - cursor_;

      OS::MemMove(&tmp_entries[0], &entries_[0], L * sizeof(HCheckTableEntry));
      OS::MemMove(&entries_[0], &entries_[L], R * sizeof(HCheckTableEntry));
      OS::MemMove(&entries_[R], &tmp_entries[0], L * sizeof(HCheckTableEntry));
    }

    cursor_ = size_;  // Move cursor to end.
  }

  void Print() {
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* entry = &entries_[i];
      ASSERT(entry->object_ != NULL);
      PrintF("  checkmaps-table @%d: object #%d ", i, entry->object_->id());
      if (entry->check_ != NULL) {
        PrintF("check #%d ", entry->check_->id());
      }
      MapSet list = entry->maps_;
      PrintF("%d maps { ", list->size());
      for (int j = 0; j < list->size(); j++) {
        if (j > 0) PrintF(", ");
        PrintF("%" V8PRIxPTR, list->at(j).Hashcode());
      }
      PrintF(" }\n");
    }
  }

 private:
  HCheckTableEntry* Find(HValue* object) {
    for (int i = size_ - 1; i >= 0; i--) {
      // Search from most-recently-inserted to least-recently-inserted.
      HCheckTableEntry* entry = &entries_[i];
      ASSERT(entry->object_ != NULL);
      if (phase_->aliasing_->MustAlias(entry->object_, object)) return entry;
    }
    return NULL;
  }

  MapSet FindMaps(HValue* object) {
    HCheckTableEntry* entry = Find(object);
    return entry == NULL ? NULL : entry->maps_;
  }

  void Insert(HValue* object, Unique<Map> map) {
    MapSet list = new(phase_->zone()) UniqueSet<Map>();
    list->Add(map, phase_->zone());
    Insert(object, NULL, list);
  }

  void Insert(HValue* object, HCheckMaps* check, MapSet maps) {
    HCheckTableEntry* entry = &entries_[cursor_++];
    entry->object_ = object;
    entry->check_ = check;
    entry->maps_ = maps;
    // If the table becomes full, wrap around and overwrite older entries.
    if (cursor_ == kMaxTrackedObjects) cursor_ = 0;
    if (size_ < kMaxTrackedObjects) size_++;
  }

  bool IsMapAccess(HObjectAccess access) {
    return access.IsInobject() && access.offset() == JSObject::kMapOffset;
  }

  Unique<Map> MapConstant(HValue* value) {
    return Unique<Map>::cast(HConstant::cast(value)->GetUnique());
  }

  friend class HCheckMapsEffects;

  HCheckEliminationPhase* phase_;
  HCheckTableEntry entries_[kMaxTrackedObjects];
  int16_t cursor_;  // Must be <= kMaxTrackedObjects
  int16_t size_;    // Must be <= kMaxTrackedObjects
  // TODO(titzer): STATIC_ASSERT kMaxTrackedObjects < max(cursor_)
};


// Collects instructions that can cause effects that invalidate information
// needed for check elimination.
class HCheckMapsEffects : public ZoneObject {
 public:
  explicit HCheckMapsEffects(Zone* zone)
    : maps_stored_(false),
      stores_(5, zone) { }

  inline bool Disabled() {
    return false;  // Effects are _not_ disabled.
  }

  // Process a possibly side-effecting instruction.
  void Process(HInstruction* instr, Zone* zone) {
    switch (instr->opcode()) {
      case HValue::kStoreNamedField: {
        stores_.Add(HStoreNamedField::cast(instr), zone);
        break;
      }
      case HValue::kOsrEntry: {
        // Kill everything. Loads must not be hoisted past the OSR entry.
        maps_stored_ = true;
      }
      default: {
        maps_stored_ |= (instr->CheckGVNFlag(kChangesMaps) |
                         instr->CheckGVNFlag(kChangesElementsKind));
      }
    }
  }

  // Apply these effects to the given check elimination table.
  void Apply(HCheckTable* table) {
    if (maps_stored_) {
      // Uncontrollable map modifications; kill everything.
      table->Kill();
      return;
    }

    // Kill maps for each store contained in these effects.
    for (int i = 0; i < stores_.length(); i++) {
      HStoreNamedField* s = stores_[i];
      if (table->IsMapAccess(s->access()) || s->has_transition()) {
        table->Kill(s->object()->ActualValue());
      }
    }
  }

  // Union these effects with the other effects.
  void Union(HCheckMapsEffects* that, Zone* zone) {
    maps_stored_ |= that->maps_stored_;
    for (int i = 0; i < that->stores_.length(); i++) {
      stores_.Add(that->stores_[i], zone);
    }
  }

 private:
  bool maps_stored_ : 1;
  ZoneList<HStoreNamedField*> stores_;
};


// The main routine of the analysis phase. Use the HFlowEngine for either a
// local or a global analysis.
void HCheckEliminationPhase::Run() {
  HFlowEngine<HCheckTable, HCheckMapsEffects> engine(graph(), zone());
  HCheckTable* table = new(zone()) HCheckTable(this);

  if (GLOBAL) {
    // Perform a global analysis.
    engine.AnalyzeDominatedBlocks(graph()->blocks()->at(0), table);
  } else {
    // Perform only local analysis.
    for (int i = 0; i < graph()->blocks()->length(); i++) {
      table->Kill();
      engine.AnalyzeOneBlock(graph()->blocks()->at(i), table);
    }
  }

  if (FLAG_trace_check_elimination) PrintStats();
}


// Are we eliminated yet?
void HCheckEliminationPhase::PrintStats() {
#if DEBUG
  #define PRINT_STAT(x) if (x##_ > 0) PrintF(" %-16s = %2d\n", #x, x##_)
#else
  #define PRINT_STAT(x)
#endif
  PRINT_STAT(redundant);
  PRINT_STAT(removed);
  PRINT_STAT(removed_cho);
  PRINT_STAT(narrowed);
  PRINT_STAT(loads);
  PRINT_STAT(empty);
  PRINT_STAT(compares_true);
  PRINT_STAT(compares_false);
  PRINT_STAT(transitions);
}

} }  // namespace v8::internal
