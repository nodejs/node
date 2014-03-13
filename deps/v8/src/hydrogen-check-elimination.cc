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
  HInstruction* check_;  // The last check instruction.
  MapSet maps_;          // The set of known maps for the object.
  bool is_stable_;
};


// The main data structure used during check elimination, which stores a
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
        if (instr->CheckChangesFlag(kOsrEntries)) {
          Reset();
        } else if (instr->CheckChangesFlag(kMaps)) {
          KillUnstableEntries();
        }
      }
      // Improvements possible:
      // - eliminate redundant HCheckSmi, HCheckInstanceType instructions
      // - track which values have been HCheckHeapObject'd
    }

    return this;
  }

  // Support for global analysis with HFlowEngine: Merge given state with
  // the other incoming state.
  static HCheckTable* Merge(HCheckTable* succ_state, HBasicBlock* succ_block,
                            HCheckTable* pred_state, HBasicBlock* pred_block,
                            Zone* zone) {
    if (pred_state == NULL || pred_block->IsUnreachable()) {
      return succ_state;
    }
    if (succ_state == NULL) {
      return pred_state->Copy(succ_block, pred_block, zone);
    } else {
      return succ_state->Merge(succ_block, pred_state, pred_block, zone);
    }
  }

  // Support for global analysis with HFlowEngine: Given state merged with all
  // the other incoming states, prepare it for use.
  static HCheckTable* Finish(HCheckTable* state, HBasicBlock* block,
                             Zone* zone) {
    if (state == NULL) {
      block->MarkUnreachable();
    }
    return state;
  }

 private:
  // Copy state to successor block.
  HCheckTable* Copy(HBasicBlock* succ, HBasicBlock* from_block, Zone* zone) {
    HCheckTable* copy = new(phase_->zone()) HCheckTable(phase_);
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* old_entry = &entries_[i];
      HCheckTableEntry* new_entry = &copy->entries_[i];
      new_entry->object_ = old_entry->object_;
      new_entry->maps_ = old_entry->maps_->Copy(phase_->zone());
      new_entry->is_stable_ = old_entry->is_stable_;
      // Keep the check if the existing check's block dominates the successor.
      if (old_entry->check_ != NULL &&
          old_entry->check_->block()->Dominates(succ)) {
        new_entry->check_ = old_entry->check_;
      } else {
        // Leave it NULL till we meet a new check instruction for this object
        // in the control flow.
        new_entry->check_ = NULL;
      }
    }
    copy->cursor_ = cursor_;
    copy->size_ = size_;

    // Create entries for succ block's phis.
    if (!succ->IsLoopHeader() && succ->phis()->length() > 0) {
      int pred_index = succ->PredecessorIndexOf(from_block);
      for (int phi_index = 0;
           phi_index < succ->phis()->length();
           ++phi_index) {
        HPhi* phi = succ->phis()->at(phi_index);
        HValue* phi_operand = phi->OperandAt(pred_index);

        HCheckTableEntry* pred_entry = copy->Find(phi_operand);
        if (pred_entry != NULL) {
          // Create an entry for a phi in the table.
          copy->Insert(phi, NULL, pred_entry->maps_->Copy(phase_->zone()),
                       pred_entry->is_stable_);
        }
      }
    }

    // Branch-sensitive analysis for certain comparisons may add more facts
    // to the state for the successor on the true branch.
    bool learned = false;
    if (succ->predecessors()->length() == 1) {
      HControlInstruction* end = succ->predecessors()->at(0)->end();
      bool is_true_branch = end->SuccessorAt(0) == succ;
      if (end->IsCompareMap()) {
        HCompareMap* cmp = HCompareMap::cast(end);
        HValue* object = cmp->value()->ActualValue();
        HCheckTableEntry* entry = copy->Find(object);
        if (is_true_branch) {
          // Learn on the true branch of if(CompareMap(x)).
          if (entry == NULL) {
            copy->Insert(object, cmp, cmp->map(), cmp->is_stable());
          } else {
            MapSet list = new(phase_->zone()) UniqueSet<Map>();
            list->Add(cmp->map(), phase_->zone());
            entry->maps_ = list;
            entry->check_ = cmp;
            entry->is_stable_ = cmp->is_stable();
          }
        } else {
          // Learn on the false branch of if(CompareMap(x)).
          if (entry != NULL) {
            entry->maps_->Remove(cmp->map());
          }
        }
        learned = true;
      } else if (is_true_branch && end->IsCompareObjectEqAndBranch()) {
        // Learn on the true branch of if(CmpObjectEq(x, y)).
        HCompareObjectEqAndBranch* cmp =
          HCompareObjectEqAndBranch::cast(end);
        HValue* left = cmp->left()->ActualValue();
        HValue* right = cmp->right()->ActualValue();
        HCheckTableEntry* le = copy->Find(left);
        HCheckTableEntry* re = copy->Find(right);
        if (le == NULL) {
          if (re != NULL) {
            copy->Insert(left, NULL, re->maps_->Copy(zone), re->is_stable_);
          }
        } else if (re == NULL) {
          copy->Insert(right, NULL, le->maps_->Copy(zone), le->is_stable_);
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

  // Merge this state with the other incoming state.
  HCheckTable* Merge(HBasicBlock* succ, HCheckTable* that,
                     HBasicBlock* pred_block, Zone* zone) {
    if (that->size_ == 0) {
      // If the other state is empty, simply reset.
      Reset();
    } else {
      int pred_index = succ->PredecessorIndexOf(pred_block);
      bool compact = false;
      for (int i = 0; i < size_; i++) {
        HCheckTableEntry* this_entry = &entries_[i];
        HCheckTableEntry* that_entry;
        if (this_entry->object_->IsPhi() &&
            this_entry->object_->block() == succ) {
          HPhi* phi = HPhi::cast(this_entry->object_);
          HValue* phi_operand = phi->OperandAt(pred_index);
          that_entry = that->Find(phi_operand);

        } else {
          that_entry = that->Find(this_entry->object_);
        }

        if (that_entry == NULL) {
          this_entry->object_ = NULL;
          compact = true;
        } else {
          this_entry->maps_ =
              this_entry->maps_->Union(that_entry->maps_, phase_->zone());
          this_entry->is_stable_ =
              this_entry->is_stable_ && that_entry->is_stable_;
          if (this_entry->check_ != that_entry->check_) {
            this_entry->check_ = NULL;
          }
          ASSERT(this_entry->maps_->size() > 0);
        }
      }
      if (compact) Compact();
    }

    if (FLAG_trace_check_elimination) {
      PrintF("B%d checkmaps-table merged with B%d table:\n",
             succ->block_id(), pred_block->block_id());
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
      MapSet intersection = i->Intersect(a, phase_->zone());
      if (intersection->size() == 0) {
        // Intersection is empty; probably megamorphic, which is likely to
        // deopt anyway, so just leave things as they are.
        INC_STAT(empty_);
      } else {
        // Update set of maps in the entry.
        entry->maps_ = intersection;
        if (intersection->size() != i->size()) {
          // Narrow set of maps in the second check maps instruction.
          HGraph* graph = instr->block()->graph();
          if (entry->check_ != NULL &&
              entry->check_->block() == instr->block() &&
              entry->check_->IsCheckMaps()) {
            // There is a check in the same block so replace it with a more
            // strict check and eliminate the second check entirely.
            HCheckMaps* check = HCheckMaps::cast(entry->check_);
            TRACE(("CheckMaps #%d at B%d narrowed\n", check->id(),
                check->block()->block_id()));
            check->set_map_set(intersection, graph->zone());
            TRACE(("Replacing redundant CheckMaps #%d at B%d with #%d\n",
                instr->id(), instr->block()->block_id(), entry->check_->id()));
            instr->DeleteAndReplaceWith(entry->check_);
          } else {
            TRACE(("CheckMaps #%d at B%d narrowed\n", instr->id(),
                instr->block()->block_id()));
            instr->set_map_set(intersection, graph->zone());
            entry->check_ = instr;
          }

          if (FLAG_trace_check_elimination) {
            Print();
          }
          INC_STAT(narrowed_);
        }
      }
    } else {
      // No entry; insert a new one.
      Insert(object, instr, instr->map_set().Copy(phase_->zone()),
             instr->is_stable());
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

    HCheckTableEntry* entry = Find(object);
    if (entry != NULL) {
      MapSet maps = entry->maps_;
      if (maps->Contains(map)) {
        if (maps->size() == 1) {
          // Object is known to have exactly this map.
          if (entry->check_ != NULL) {
            instr->DeleteAndReplaceWith(entry->check_);
          } else {
            // Mark check as dead but leave it in the graph as a checkpoint for
            // subsequent checks.
            instr->SetFlag(HValue::kIsDead);
            entry->check_ = instr;
          }
          INC_STAT(removed_);
        } else {
          // Only one map survives the check.
          maps->Clear();
          maps->Add(map, phase_->zone());
          entry->check_ = instr;
        }
      }
    } else {
      // No prior information.
      // TODO(verwaest): Tag map constants with stability.
      Insert(object, instr, map, false);
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
      Insert(object, NULL, MapConstant(instr->transition()),
             instr->is_stable());
    } else if (IsMapAccess(instr->access())) {
      // This is a store directly to the map field of the object.
      Kill(object);
      if (!instr->value()->IsConstant()) return;
      // TODO(verwaest): Tag with stability.
      Insert(object, NULL, MapConstant(instr->value()), false);
    } else {
      // If the instruction changes maps, it should be handled above.
      CHECK(!instr->CheckChangesFlag(kMaps));
    }
  }

  void ReduceCompareMap(HCompareMap* instr) {
    MapSet maps = FindMaps(instr->value()->ActualValue());
    if (maps == NULL) return;

    int succ;
    if (maps->Contains(instr->map())) {
      if (maps->size() != 1) {
        TRACE(("CompareMap #%d for #%d at B%d can't be eliminated: "
               "ambiguous set of maps\n", instr->id(), instr->value()->id(),
               instr->block()->block_id()));
        return;
      }
      succ = 0;
      INC_STAT(compares_true_);
    } else {
      succ = 1;
      INC_STAT(compares_false_);
    }

    TRACE(("Marking redundant CompareMap #%d for #%d at B%d as %s\n",
        instr->id(), instr->value()->id(), instr->block()->block_id(),
        succ == 0 ? "true" : "false"));
    instr->set_known_successor_index(succ);

    int unreachable_succ = 1 - succ;
    instr->block()->MarkSuccEdgeUnreachable(unreachable_succ);
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

  // Reset the table.
  void Reset() {
    size_ = 0;
    cursor_ = 0;
  }

  // Kill everything in the table.
  void KillUnstableEntries() {
    bool compact = false;
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* entry = &entries_[i];
      ASSERT(entry->object_ != NULL);
      if (!entry->is_stable_) {
        entry->object_ = NULL;
        compact = true;
      }
    }
    if (compact) Compact();
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
      PrintF("  checkmaps-table @%d: %s #%d ", i,
             entry->object_->IsPhi() ? "phi" : "object", entry->object_->id());
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

  void Insert(HValue* object,
              HInstruction* check,
              Unique<Map> map,
              bool is_stable) {
    MapSet list = new(phase_->zone()) UniqueSet<Map>();
    list->Add(map, phase_->zone());
    Insert(object, check, list, is_stable);
  }

  void Insert(HValue* object,
              HInstruction* check,
              MapSet maps,
              bool is_stable) {
    HCheckTableEntry* entry = &entries_[cursor_++];
    entry->object_ = object;
    entry->check_ = check;
    entry->maps_ = maps;
    entry->is_stable_ = is_stable;
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
  friend class HCheckEliminationPhase;

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
    : stores_(5, zone) { }

  inline bool Disabled() {
    return false;  // Effects are _not_ disabled.
  }

  // Process a possibly side-effecting instruction.
  void Process(HInstruction* instr, Zone* zone) {
    if (instr->IsStoreNamedField()) {
      stores_.Add(HStoreNamedField::cast(instr), zone);
    } else {
      flags_.Add(instr->ChangesFlags());
    }
  }

  // Apply these effects to the given check elimination table.
  void Apply(HCheckTable* table) {
    if (flags_.Contains(kOsrEntries)) {
      table->Reset();
      return;
    }
    if (flags_.Contains(kMaps) || flags_.Contains(kElementsKind)) {
      // Uncontrollable map modifications; kill everything.
      table->KillUnstableEntries();
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
    flags_.Add(that->flags_);
    for (int i = 0; i < that->stores_.length(); i++) {
      stores_.Add(that->stores_[i], zone);
    }
  }

 private:
  GVNFlagSet flags_;
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
      table->Reset();
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
