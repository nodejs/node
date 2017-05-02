// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/crankshaft/hydrogen-check-elimination.h"

#include "src/crankshaft/hydrogen-alias-analysis.h"
#include "src/crankshaft/hydrogen-flow-engine.h"
#include "src/objects-inl.h"

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

typedef const UniqueSet<Map>* MapSet;

struct HCheckTableEntry {
  enum State {
    // We have seen a map check (i.e. an HCheckMaps) for these maps, so we can
    // use this information to eliminate further map checks, elements kind
    // transitions, etc.
    CHECKED,
    // Same as CHECKED, but we also know that these maps are stable.
    CHECKED_STABLE,
    // These maps are stable, but not checked (i.e. we learned this via field
    // type tracking or from a constant, or they were initially CHECKED_STABLE,
    // but became UNCHECKED_STABLE because of an instruction that changes maps
    // or elements kind), and we need a stability check for them in order to use
    // this information for check elimination (which turns them back to
    // CHECKED_STABLE).
    UNCHECKED_STABLE
  };

  static const char* State2String(State state) {
    switch (state) {
      case CHECKED: return "checked";
      case CHECKED_STABLE: return "checked stable";
      case UNCHECKED_STABLE: return "unchecked stable";
    }
    UNREACHABLE();
    return NULL;
  }

  static State StateMerge(State state1, State state2) {
    if (state1 == state2) return state1;
    if ((state1 == CHECKED && state2 == CHECKED_STABLE) ||
        (state2 == CHECKED && state1 == CHECKED_STABLE)) {
      return CHECKED;
    }
    DCHECK((state1 == CHECKED_STABLE && state2 == UNCHECKED_STABLE) ||
           (state2 == CHECKED_STABLE && state1 == UNCHECKED_STABLE));
    return UNCHECKED_STABLE;
  }

  HValue* object_;  // The object being approximated. NULL => invalid entry.
  HInstruction* check_;  // The last check instruction.
  MapSet maps_;          // The set of known maps for the object.
  State state_;          // The state of this entry.
};


// The main data structure used during check elimination, which stores a
// set of known maps for each object.
class HCheckTable : public ZoneObject {
 public:
  static const int kMaxTrackedObjects = 16;

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
      case HValue::kCompareObjectEqAndBranch: {
        ReduceCompareObjectEqAndBranch(HCompareObjectEqAndBranch::cast(instr));
        break;
      }
      case HValue::kIsStringAndBranch: {
        ReduceIsStringAndBranch(HIsStringAndBranch::cast(instr));
        break;
      }
      case HValue::kTransitionElementsKind: {
        ReduceTransitionElementsKind(
            HTransitionElementsKind::cast(instr));
        break;
      }
      case HValue::kCheckHeapObject: {
        ReduceCheckHeapObject(HCheckHeapObject::cast(instr));
        break;
      }
      case HValue::kCheckInstanceType: {
        ReduceCheckInstanceType(HCheckInstanceType::cast(instr));
        break;
      }
      default: {
        // If the instruction changes maps uncontrollably, drop everything.
        if (instr->CheckChangesFlag(kOsrEntries)) {
          Kill();
          break;
        }
        if (instr->CheckChangesFlag(kElementsKind) ||
            instr->CheckChangesFlag(kMaps)) {
          KillUnstableEntries();
        }
      }
      // Improvements possible:
      // - eliminate redundant HCheckSmi instructions
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
    } else if (block->IsUnreachable()) {
      state = NULL;
    }
    if (FLAG_trace_check_elimination) {
      PrintF("Processing B%d, checkmaps-table:\n", block->block_id());
      Print(state);
    }
    return state;
  }

 private:
  // Copy state to successor block.
  HCheckTable* Copy(HBasicBlock* succ, HBasicBlock* from_block, Zone* zone) {
    HCheckTable* copy = new(zone) HCheckTable(phase_);
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* old_entry = &entries_[i];
      DCHECK(old_entry->maps_->size() > 0);
      HCheckTableEntry* new_entry = &copy->entries_[i];
      new_entry->object_ = old_entry->object_;
      new_entry->maps_ = old_entry->maps_;
      new_entry->state_ = old_entry->state_;
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
          copy->Insert(phi, NULL, pred_entry->maps_, pred_entry->state_);
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
          HCheckTableEntry::State state = cmp->map_is_stable()
              ? HCheckTableEntry::CHECKED_STABLE
              : HCheckTableEntry::CHECKED;
          // Learn on the true branch of if(CompareMap(x)).
          if (entry == NULL) {
            copy->Insert(object, cmp, cmp->map(), state);
          } else {
            entry->maps_ = new(zone) UniqueSet<Map>(cmp->map(), zone);
            entry->check_ = cmp;
            entry->state_ = state;
          }
        } else {
          // Learn on the false branch of if(CompareMap(x)).
          if (entry != NULL) {
            EnsureChecked(entry, object, cmp);
            UniqueSet<Map>* maps = entry->maps_->Copy(zone);
            maps->Remove(cmp->map());
            entry->maps_ = maps;
            DCHECK_NE(HCheckTableEntry::UNCHECKED_STABLE, entry->state_);
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
            copy->Insert(left, NULL, re->maps_, re->state_);
          }
        } else if (re == NULL) {
          copy->Insert(right, NULL, le->maps_, le->state_);
        } else {
          EnsureChecked(le, cmp->left(), cmp);
          EnsureChecked(re, cmp->right(), cmp);
          le->maps_ = re->maps_ = le->maps_->Intersect(re->maps_, zone);
          le->state_ = re->state_ = HCheckTableEntry::StateMerge(
              le->state_, re->state_);
          DCHECK_NE(HCheckTableEntry::UNCHECKED_STABLE, le->state_);
          DCHECK_NE(HCheckTableEntry::UNCHECKED_STABLE, re->state_);
        }
        learned = true;
      } else if (end->IsIsStringAndBranch()) {
        HIsStringAndBranch* cmp = HIsStringAndBranch::cast(end);
        HValue* object = cmp->value()->ActualValue();
        HCheckTableEntry* entry = copy->Find(object);
        if (is_true_branch) {
          // Learn on the true branch of if(IsString(x)).
          if (entry == NULL) {
            copy->Insert(object, NULL, string_maps(),
                         HCheckTableEntry::CHECKED);
          } else {
            EnsureChecked(entry, object, cmp);
            entry->maps_ = entry->maps_->Intersect(string_maps(), zone);
            DCHECK_NE(HCheckTableEntry::UNCHECKED_STABLE, entry->state_);
          }
        } else {
          // Learn on the false branch of if(IsString(x)).
          if (entry != NULL) {
            EnsureChecked(entry, object, cmp);
            entry->maps_ = entry->maps_->Subtract(string_maps(), zone);
            DCHECK_NE(HCheckTableEntry::UNCHECKED_STABLE, entry->state_);
          }
        }
      }
      // Learning on false branches requires storing negative facts.
    }

    if (FLAG_trace_check_elimination) {
      PrintF("B%d checkmaps-table %s from B%d:\n",
             succ->block_id(),
             learned ? "learned" : "copied",
             from_block->block_id());
      Print(copy);
    }

    return copy;
  }

  // Merge this state with the other incoming state.
  HCheckTable* Merge(HBasicBlock* succ, HCheckTable* that,
                     HBasicBlock* pred_block, Zone* zone) {
    if (that->size_ == 0) {
      // If the other state is empty, simply reset.
      size_ = 0;
      cursor_ = 0;
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

        if (that_entry == NULL ||
            (that_entry->state_ == HCheckTableEntry::CHECKED &&
             this_entry->state_ == HCheckTableEntry::UNCHECKED_STABLE) ||
            (this_entry->state_ == HCheckTableEntry::CHECKED &&
             that_entry->state_ == HCheckTableEntry::UNCHECKED_STABLE)) {
          this_entry->object_ = NULL;
          compact = true;
        } else {
          this_entry->maps_ =
              this_entry->maps_->Union(that_entry->maps_, zone);
          this_entry->state_ = HCheckTableEntry::StateMerge(
              this_entry->state_, that_entry->state_);
          if (this_entry->check_ != that_entry->check_) {
            this_entry->check_ = NULL;
          }
          DCHECK(this_entry->maps_->size() > 0);
        }
      }
      if (compact) Compact();
    }

    if (FLAG_trace_check_elimination) {
      PrintF("B%d checkmaps-table merged with B%d table:\n",
             succ->block_id(), pred_block->block_id());
      Print(this);
    }
    return this;
  }

  void ReduceCheckMaps(HCheckMaps* instr) {
    HValue* object = instr->value()->ActualValue();
    HCheckTableEntry* entry = Find(object);
    if (entry != NULL) {
      // entry found;
      HGraph* graph = instr->block()->graph();
      if (entry->maps_->IsSubset(instr->maps())) {
        // The first check is more strict; the second is redundant.
        if (entry->check_ != NULL) {
          DCHECK_NE(HCheckTableEntry::UNCHECKED_STABLE, entry->state_);
          TRACE(("Replacing redundant CheckMaps #%d at B%d with #%d\n",
              instr->id(), instr->block()->block_id(), entry->check_->id()));
          instr->DeleteAndReplaceWith(entry->check_);
          INC_STAT(redundant_);
        } else if (entry->state_ == HCheckTableEntry::UNCHECKED_STABLE) {
          DCHECK_NULL(entry->check_);
          TRACE(("Marking redundant CheckMaps #%d at B%d as stability check\n",
                 instr->id(), instr->block()->block_id()));
          instr->set_maps(entry->maps_->Copy(graph->zone()));
          instr->MarkAsStabilityCheck();
          entry->state_ = HCheckTableEntry::CHECKED_STABLE;
        } else if (!instr->IsStabilityCheck()) {
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
      MapSet intersection = instr->maps()->Intersect(
          entry->maps_, graph->zone());
      if (intersection->size() == 0) {
        // Intersection is empty; probably megamorphic.
        INC_STAT(empty_);
        entry->object_ = NULL;
        Compact();
      } else {
        // Update set of maps in the entry.
        entry->maps_ = intersection;
        // Update state of the entry.
        if (instr->maps_are_stable() ||
            entry->state_ == HCheckTableEntry::UNCHECKED_STABLE) {
          entry->state_ = HCheckTableEntry::CHECKED_STABLE;
        }
        if (intersection->size() != instr->maps()->size()) {
          // Narrow set of maps in the second check maps instruction.
          if (entry->check_ != NULL &&
              entry->check_->block() == instr->block() &&
              entry->check_->IsCheckMaps()) {
            // There is a check in the same block so replace it with a more
            // strict check and eliminate the second check entirely.
            HCheckMaps* check = HCheckMaps::cast(entry->check_);
            DCHECK(!check->IsStabilityCheck());
            TRACE(("CheckMaps #%d at B%d narrowed\n", check->id(),
                check->block()->block_id()));
            // Update map set and ensure that the check is alive.
            check->set_maps(intersection);
            check->ClearFlag(HValue::kIsDead);
            TRACE(("Replacing redundant CheckMaps #%d at B%d with #%d\n",
                instr->id(), instr->block()->block_id(), entry->check_->id()));
            instr->DeleteAndReplaceWith(entry->check_);
          } else {
            TRACE(("CheckMaps #%d at B%d narrowed\n", instr->id(),
                instr->block()->block_id()));
            instr->set_maps(intersection);
            entry->check_ = instr->IsStabilityCheck() ? NULL : instr;
          }

          if (FLAG_trace_check_elimination) {
            Print(this);
          }
          INC_STAT(narrowed_);
        }
      }
    } else {
      // No entry; insert a new one.
      HCheckTableEntry::State state = instr->maps_are_stable()
          ? HCheckTableEntry::CHECKED_STABLE
          : HCheckTableEntry::CHECKED;
      HCheckMaps* check = instr->IsStabilityCheck() ? NULL : instr;
      Insert(object, check, instr->maps(), state);
    }
  }

  void ReduceCheckInstanceType(HCheckInstanceType* instr) {
    HValue* value = instr->value()->ActualValue();
    HCheckTableEntry* entry = Find(value);
    if (entry == NULL) {
      if (instr->check() == HCheckInstanceType::IS_STRING) {
        Insert(value, NULL, string_maps(), HCheckTableEntry::CHECKED);
      }
      return;
    }
    UniqueSet<Map>* maps = new(zone()) UniqueSet<Map>(
        entry->maps_->size(), zone());
    for (int i = 0; i < entry->maps_->size(); ++i) {
      InstanceType type;
      Unique<Map> map = entry->maps_->at(i);
      {
        // This is safe, because maps don't move and their instance type does
        // not change.
        AllowHandleDereference allow_deref;
        type = map.handle()->instance_type();
      }
      if (instr->is_interval_check()) {
        InstanceType first_type, last_type;
        instr->GetCheckInterval(&first_type, &last_type);
        if (first_type <= type && type <= last_type) maps->Add(map, zone());
      } else {
        uint8_t mask, tag;
        instr->GetCheckMaskAndTag(&mask, &tag);
        if ((type & mask) == tag) maps->Add(map, zone());
      }
    }
    if (maps->size() == entry->maps_->size()) {
      TRACE(("Removing redundant CheckInstanceType #%d at B%d\n",
              instr->id(), instr->block()->block_id()));
      EnsureChecked(entry, value, instr);
      instr->DeleteAndReplaceWith(value);
      INC_STAT(removed_cit_);
    } else if (maps->size() != 0) {
      entry->maps_ = maps;
      if (entry->state_ == HCheckTableEntry::UNCHECKED_STABLE) {
        entry->state_ = HCheckTableEntry::CHECKED_STABLE;
      }
    }
  }

  void ReduceLoadNamedField(HLoadNamedField* instr) {
    // Reduce a load of the map field when it is known to be a constant.
    if (!instr->access().IsMap()) {
      // Check if we introduce field maps here.
      MapSet maps = instr->maps();
      if (maps != NULL) {
        DCHECK_NE(0, maps->size());
        Insert(instr, NULL, maps, HCheckTableEntry::UNCHECKED_STABLE);
      }
      return;
    }

    HValue* object = instr->object()->ActualValue();
    HCheckTableEntry* entry = Find(object);
    if (entry == NULL || entry->maps_->size() != 1) return;  // Not a constant.

    EnsureChecked(entry, object, instr);
    Unique<Map> map = entry->maps_->at(0);
    bool map_is_stable = (entry->state_ != HCheckTableEntry::CHECKED);
    HConstant* constant = HConstant::CreateAndInsertBefore(
        instr->block()->graph()->zone(), map, map_is_stable, instr);
    instr->DeleteAndReplaceWith(constant);
    INC_STAT(loads_);
  }

  void ReduceCheckHeapObject(HCheckHeapObject* instr) {
    HValue* value = instr->value()->ActualValue();
    if (Find(value) != NULL) {
      // If the object has known maps, it's definitely a heap object.
      instr->DeleteAndReplaceWith(value);
      INC_STAT(removed_cho_);
    }
  }

  void ReduceStoreNamedField(HStoreNamedField* instr) {
    HValue* object = instr->object()->ActualValue();
    if (instr->has_transition()) {
      // This store transitions the object to a new map.
      Kill(object);
      HConstant* c_transition = HConstant::cast(instr->transition());
      HCheckTableEntry::State state = c_transition->HasStableMapValue()
          ? HCheckTableEntry::CHECKED_STABLE
          : HCheckTableEntry::CHECKED;
      Insert(object, NULL, c_transition->MapValue(), state);
    } else if (instr->access().IsMap()) {
      // This is a store directly to the map field of the object.
      Kill(object);
      if (!instr->value()->IsConstant()) return;
      HConstant* c_value = HConstant::cast(instr->value());
      HCheckTableEntry::State state = c_value->HasStableMapValue()
          ? HCheckTableEntry::CHECKED_STABLE
          : HCheckTableEntry::CHECKED;
      Insert(object, NULL, c_value->MapValue(), state);
    } else {
      // If the instruction changes maps, it should be handled above.
      CHECK(!instr->CheckChangesFlag(kMaps));
    }
  }

  void ReduceCompareMap(HCompareMap* instr) {
    HCheckTableEntry* entry = Find(instr->value()->ActualValue());
    if (entry == NULL) return;

    EnsureChecked(entry, instr->value(), instr);

    int succ;
    if (entry->maps_->Contains(instr->map())) {
      if (entry->maps_->size() != 1) {
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

  void ReduceCompareObjectEqAndBranch(HCompareObjectEqAndBranch* instr) {
    HValue* left = instr->left()->ActualValue();
    HCheckTableEntry* le = Find(left);
    if (le == NULL) return;
    HValue* right = instr->right()->ActualValue();
    HCheckTableEntry* re = Find(right);
    if (re == NULL) return;

    EnsureChecked(le, left, instr);
    EnsureChecked(re, right, instr);

    // TODO(bmeurer): Add a predicate here instead of computing the intersection
    MapSet intersection = le->maps_->Intersect(re->maps_, zone());
    if (intersection->size() > 0) return;

    TRACE(("Marking redundant CompareObjectEqAndBranch #%d at B%d as false\n",
        instr->id(), instr->block()->block_id()));
    int succ = 1;
    instr->set_known_successor_index(succ);

    int unreachable_succ = 1 - succ;
    instr->block()->MarkSuccEdgeUnreachable(unreachable_succ);
  }

  void ReduceIsStringAndBranch(HIsStringAndBranch* instr) {
    HValue* value = instr->value()->ActualValue();
    HCheckTableEntry* entry = Find(value);
    if (entry == NULL) return;
    EnsureChecked(entry, value, instr);
    int succ;
    if (entry->maps_->IsSubset(string_maps())) {
      TRACE(("Marking redundant IsStringAndBranch #%d at B%d as true\n",
             instr->id(), instr->block()->block_id()));
      succ = 0;
    } else {
      MapSet intersection = entry->maps_->Intersect(string_maps(), zone());
      if (intersection->size() > 0) return;
      TRACE(("Marking redundant IsStringAndBranch #%d at B%d as false\n",
            instr->id(), instr->block()->block_id()));
      succ = 1;
    }
    instr->set_known_successor_index(succ);
    int unreachable_succ = 1 - succ;
    instr->block()->MarkSuccEdgeUnreachable(unreachable_succ);
  }

  void ReduceTransitionElementsKind(HTransitionElementsKind* instr) {
    HValue* object = instr->object()->ActualValue();
    HCheckTableEntry* entry = Find(object);
    // Can only learn more about an object that already has a known set of maps.
    if (entry == NULL) {
      Kill(object);
      return;
    }
    EnsureChecked(entry, object, instr);
    if (entry->maps_->Contains(instr->original_map())) {
      // If the object has the original map, it will be transitioned.
      UniqueSet<Map>* maps = entry->maps_->Copy(zone());
      maps->Remove(instr->original_map());
      maps->Add(instr->transitioned_map(), zone());
      HCheckTableEntry::State state =
          (entry->state_ == HCheckTableEntry::CHECKED_STABLE &&
           instr->map_is_stable())
              ? HCheckTableEntry::CHECKED_STABLE
              : HCheckTableEntry::CHECKED;
      Kill(object);
      Insert(object, NULL, maps, state);
    } else {
      // Object does not have the given map, thus the transition is redundant.
      instr->DeleteAndReplaceWith(object);
      INC_STAT(transitions_);
    }
  }

  void EnsureChecked(HCheckTableEntry* entry,
                     HValue* value,
                     HInstruction* instr) {
    if (entry->state_ != HCheckTableEntry::UNCHECKED_STABLE) return;
    HGraph* graph = instr->block()->graph();
    HCheckMaps* check = HCheckMaps::CreateAndInsertBefore(
        graph->zone(), value, entry->maps_->Copy(graph->zone()), true, instr);
    check->MarkAsStabilityCheck();
    entry->state_ = HCheckTableEntry::CHECKED_STABLE;
    entry->check_ = NULL;
  }

  // Kill everything in the table.
  void Kill() {
    size_ = 0;
    cursor_ = 0;
  }

  // Kill all unstable entries in the table.
  void KillUnstableEntries() {
    bool compact = false;
    for (int i = 0; i < size_; ++i) {
      HCheckTableEntry* entry = &entries_[i];
      DCHECK_NOT_NULL(entry->object_);
      if (entry->state_ == HCheckTableEntry::CHECKED) {
        entry->object_ = NULL;
        compact = true;
      } else {
        // All checked stable entries become unchecked stable.
        entry->state_ = HCheckTableEntry::UNCHECKED_STABLE;
        entry->check_ = NULL;
      }
    }
    if (compact) Compact();
  }

  // Kill everything in the table that may alias {object}.
  void Kill(HValue* object) {
    bool compact = false;
    for (int i = 0; i < size_; i++) {
      HCheckTableEntry* entry = &entries_[i];
      DCHECK_NOT_NULL(entry->object_);
      if (phase_->aliasing_->MayAlias(entry->object_, object)) {
        entry->object_ = NULL;
        compact = true;
      }
    }
    if (compact) Compact();
    DCHECK_NULL(Find(object));
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
    DCHECK(size_ == dest);
    DCHECK(cursor_ <= size_);

    // Preserve the age of the entries by moving the older entries to the end.
    if (cursor_ == size_) return;  // Cursor already points at end.
    if (cursor_ != 0) {
      // | L = oldest |   R = newest   |       |
      //              ^ cursor         ^ size  ^ MAX
      HCheckTableEntry tmp_entries[kMaxTrackedObjects];
      int L = cursor_;
      int R = size_ - cursor_;

      MemMove(&tmp_entries[0], &entries_[0], L * sizeof(HCheckTableEntry));
      MemMove(&entries_[0], &entries_[L], R * sizeof(HCheckTableEntry));
      MemMove(&entries_[R], &tmp_entries[0], L * sizeof(HCheckTableEntry));
    }

    cursor_ = size_;  // Move cursor to end.
  }

  static void Print(HCheckTable* table) {
    if (table == NULL) {
      PrintF("  unreachable\n");
      return;
    }

    for (int i = 0; i < table->size_; i++) {
      HCheckTableEntry* entry = &table->entries_[i];
      DCHECK(entry->object_ != NULL);
      PrintF("  checkmaps-table @%d: %s #%d ", i,
             entry->object_->IsPhi() ? "phi" : "object", entry->object_->id());
      if (entry->check_ != NULL) {
        PrintF("check #%d ", entry->check_->id());
      }
      MapSet list = entry->maps_;
      PrintF("%d %s maps { ", list->size(),
             HCheckTableEntry::State2String(entry->state_));
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
      DCHECK(entry->object_ != NULL);
      if (phase_->aliasing_->MustAlias(entry->object_, object)) return entry;
    }
    return NULL;
  }

  void Insert(HValue* object,
              HInstruction* check,
              Unique<Map> map,
              HCheckTableEntry::State state) {
    Insert(object, check, new(zone()) UniqueSet<Map>(map, zone()), state);
  }

  void Insert(HValue* object,
              HInstruction* check,
              MapSet maps,
              HCheckTableEntry::State state) {
    DCHECK(state != HCheckTableEntry::UNCHECKED_STABLE || check == NULL);
    HCheckTableEntry* entry = &entries_[cursor_++];
    entry->object_ = object;
    entry->check_ = check;
    entry->maps_ = maps;
    entry->state_ = state;
    // If the table becomes full, wrap around and overwrite older entries.
    if (cursor_ == kMaxTrackedObjects) cursor_ = 0;
    if (size_ < kMaxTrackedObjects) size_++;
  }

  Zone* zone() const { return phase_->zone(); }
  MapSet string_maps() const { return phase_->string_maps(); }

  friend class HCheckMapsEffects;
  friend class HCheckEliminationPhase;

  HCheckEliminationPhase* phase_;
  HCheckTableEntry entries_[kMaxTrackedObjects];
  int16_t cursor_;  // Must be <= kMaxTrackedObjects
  int16_t size_;    // Must be <= kMaxTrackedObjects
  STATIC_ASSERT(kMaxTrackedObjects < (1 << 15));
};


// Collects instructions that can cause effects that invalidate information
// needed for check elimination.
class HCheckMapsEffects : public ZoneObject {
 public:
  explicit HCheckMapsEffects(Zone* zone) : objects_(0, zone) { }

  // Effects are _not_ disabled.
  inline bool Disabled() const { return false; }

  // Process a possibly side-effecting instruction.
  void Process(HInstruction* instr, Zone* zone) {
    switch (instr->opcode()) {
      case HValue::kStoreNamedField: {
        HStoreNamedField* store = HStoreNamedField::cast(instr);
        if (store->access().IsMap() || store->has_transition()) {
          objects_.Add(store->object(), zone);
        }
        break;
      }
      case HValue::kTransitionElementsKind: {
        objects_.Add(HTransitionElementsKind::cast(instr)->object(), zone);
        break;
      }
      default: {
        flags_.Add(instr->ChangesFlags());
        break;
      }
    }
  }

  // Apply these effects to the given check elimination table.
  void Apply(HCheckTable* table) {
    if (flags_.Contains(kOsrEntries)) {
      // Uncontrollable map modifications; kill everything.
      table->Kill();
      return;
    }

    // Kill all unstable entries.
    if (flags_.Contains(kElementsKind) || flags_.Contains(kMaps)) {
      table->KillUnstableEntries();
    }

    // Kill maps for each object contained in these effects.
    for (int i = 0; i < objects_.length(); ++i) {
      table->Kill(objects_[i]->ActualValue());
    }
  }

  // Union these effects with the other effects.
  void Union(HCheckMapsEffects* that, Zone* zone) {
    flags_.Add(that->flags_);
    for (int i = 0; i < that->objects_.length(); ++i) {
      objects_.Add(that->objects_[i], zone);
    }
  }

 private:
  ZoneList<HValue*> objects_;
  GVNFlagSet flags_;
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
  PRINT_STAT(removed_cit);
  PRINT_STAT(narrowed);
  PRINT_STAT(loads);
  PRINT_STAT(empty);
  PRINT_STAT(compares_true);
  PRINT_STAT(compares_false);
  PRINT_STAT(transitions);
}

}  // namespace internal
}  // namespace v8
