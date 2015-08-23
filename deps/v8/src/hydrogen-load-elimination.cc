// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen-alias-analysis.h"
#include "src/hydrogen-flow-engine.h"
#include "src/hydrogen-instructions.h"
#include "src/hydrogen-load-elimination.h"

namespace v8 {
namespace internal {

#define GLOBAL true
#define TRACE(x) if (FLAG_trace_load_elimination) PrintF x

static const int kMaxTrackedFields = 16;
static const int kMaxTrackedObjects = 5;

// An element in the field approximation list.
class HFieldApproximation : public ZoneObject {
 public:  // Just a data blob.
  HValue* object_;
  HValue* last_value_;
  HFieldApproximation* next_;

  // Recursively copy the entire linked list of field approximations.
  HFieldApproximation* Copy(Zone* zone) {
    HFieldApproximation* copy = new(zone) HFieldApproximation();
    copy->object_ = this->object_;
    copy->last_value_ = this->last_value_;
    copy->next_ = this->next_ == NULL ? NULL : this->next_->Copy(zone);
    return copy;
  }
};


// The main datastructure used during load/store elimination. Each in-object
// field is tracked separately. For each field, store a list of known field
// values for known objects.
class HLoadEliminationTable : public ZoneObject {
 public:
  HLoadEliminationTable(Zone* zone, HAliasAnalyzer* aliasing)
    : zone_(zone), fields_(kMaxTrackedFields, zone), aliasing_(aliasing) { }

  // The main processing of instructions.
  HLoadEliminationTable* Process(HInstruction* instr, Zone* zone) {
    switch (instr->opcode()) {
      case HValue::kLoadNamedField: {
        HLoadNamedField* l = HLoadNamedField::cast(instr);
        TRACE((" process L%d field %d (o%d)\n",
               instr->id(),
               FieldOf(l->access()),
               l->object()->ActualValue()->id()));
        HValue* result = load(l);
        if (result != instr && l->CanBeReplacedWith(result)) {
          // The load can be replaced with a previous load or a value.
          TRACE(("  replace L%d -> v%d\n", instr->id(), result->id()));
          instr->DeleteAndReplaceWith(result);
        }
        break;
      }
      case HValue::kStoreNamedField: {
        HStoreNamedField* s = HStoreNamedField::cast(instr);
        TRACE((" process S%d field %d (o%d) = v%d\n",
               instr->id(),
               FieldOf(s->access()),
               s->object()->ActualValue()->id(),
               s->value()->id()));
        HValue* result = store(s);
        if (result == NULL) {
          // The store is redundant. Remove it.
          TRACE(("  remove S%d\n", instr->id()));
          instr->DeleteAndReplaceWith(NULL);
        }
        break;
      }
      case HValue::kTransitionElementsKind: {
        HTransitionElementsKind* t = HTransitionElementsKind::cast(instr);
        HValue* object = t->object()->ActualValue();
        KillFieldInternal(object, FieldOf(JSArray::kElementsOffset), NULL);
        KillFieldInternal(object, FieldOf(JSObject::kMapOffset), NULL);
        break;
      }
      default: {
        if (instr->CheckChangesFlag(kInobjectFields)) {
          TRACE((" kill-all i%d\n", instr->id()));
          Kill();
          break;
        }
        if (instr->CheckChangesFlag(kMaps)) {
          TRACE((" kill-maps i%d\n", instr->id()));
          KillOffset(JSObject::kMapOffset);
        }
        if (instr->CheckChangesFlag(kElementsKind)) {
          TRACE((" kill-elements-kind i%d\n", instr->id()));
          KillOffset(JSObject::kMapOffset);
          KillOffset(JSObject::kElementsOffset);
        }
        if (instr->CheckChangesFlag(kElementsPointer)) {
          TRACE((" kill-elements i%d\n", instr->id()));
          KillOffset(JSObject::kElementsOffset);
        }
        if (instr->CheckChangesFlag(kOsrEntries)) {
          TRACE((" kill-osr i%d\n", instr->id()));
          Kill();
        }
      }
      // Improvements possible:
      // - learn from HCheckMaps for field 0
      // - remove unobservable stores (write-after-write)
      // - track cells
      // - track globals
      // - track roots
    }
    return this;
  }

  // Support for global analysis with HFlowEngine: Merge given state with
  // the other incoming state.
  static HLoadEliminationTable* Merge(HLoadEliminationTable* succ_state,
                                      HBasicBlock* succ_block,
                                      HLoadEliminationTable* pred_state,
                                      HBasicBlock* pred_block,
                                      Zone* zone) {
    DCHECK(pred_state != NULL);
    if (succ_state == NULL) {
      return pred_state->Copy(succ_block, pred_block, zone);
    } else {
      return succ_state->Merge(succ_block, pred_state, pred_block, zone);
    }
  }

  // Support for global analysis with HFlowEngine: Given state merged with all
  // the other incoming states, prepare it for use.
  static HLoadEliminationTable* Finish(HLoadEliminationTable* state,
                                       HBasicBlock* block,
                                       Zone* zone) {
    DCHECK(state != NULL);
    return state;
  }

 private:
  // Copy state to successor block.
  HLoadEliminationTable* Copy(HBasicBlock* succ, HBasicBlock* from_block,
                              Zone* zone) {
    HLoadEliminationTable* copy =
        new(zone) HLoadEliminationTable(zone, aliasing_);
    copy->EnsureFields(fields_.length());
    for (int i = 0; i < fields_.length(); i++) {
      copy->fields_[i] = fields_[i] == NULL ? NULL : fields_[i]->Copy(zone);
    }
    if (FLAG_trace_load_elimination) {
      TRACE((" copy-to B%d\n", succ->block_id()));
      copy->Print();
    }
    return copy;
  }

  // Merge this state with the other incoming state.
  HLoadEliminationTable* Merge(HBasicBlock* succ, HLoadEliminationTable* that,
                               HBasicBlock* that_block, Zone* zone) {
    if (that->fields_.length() < fields_.length()) {
      // Drop fields not in the other table.
      fields_.Rewind(that->fields_.length());
    }
    for (int i = 0; i < fields_.length(); i++) {
      // Merge the field approximations for like fields.
      HFieldApproximation* approx = fields_[i];
      HFieldApproximation* prev = NULL;
      while (approx != NULL) {
        // TODO(titzer): Merging is O(N * M); sort?
        HFieldApproximation* other = that->Find(approx->object_, i);
        if (other == NULL || !Equal(approx->last_value_, other->last_value_)) {
          // Kill an entry that doesn't agree with the other value.
          if (prev != NULL) {
            prev->next_ = approx->next_;
          } else {
            fields_[i] = approx->next_;
          }
          approx = approx->next_;
          continue;
        }
        prev = approx;
        approx = approx->next_;
      }
    }
    if (FLAG_trace_load_elimination) {
      TRACE((" merge-to B%d\n", succ->block_id()));
      Print();
    }
    return this;
  }

  friend class HLoadEliminationEffects;  // Calls Kill() and others.
  friend class HLoadEliminationPhase;

 private:
  // Process a load instruction, updating internal table state. If a previous
  // load or store for this object and field exists, return the new value with
  // which the load should be replaced. Otherwise, return {instr}.
  HValue* load(HLoadNamedField* instr) {
    // There must be no loads from non observable in-object properties.
    DCHECK(!instr->access().IsInobject() ||
           instr->access().existing_inobject_property());

    int field = FieldOf(instr->access());
    if (field < 0) return instr;

    HValue* object = instr->object()->ActualValue();
    HFieldApproximation* approx = FindOrCreate(object, field);

    if (approx->last_value_ == NULL) {
      // Load is not redundant. Fill out a new entry.
      approx->last_value_ = instr;
      return instr;
    } else if (approx->last_value_->block()->EqualToOrDominates(
        instr->block())) {
      // Eliminate the load. Reuse previously stored value or load instruction.
      return approx->last_value_;
    } else {
      return instr;
    }
  }

  // Process a store instruction, updating internal table state. If a previous
  // store to the same object and field makes this store redundant (e.g. because
  // the stored values are the same), return NULL indicating that this store
  // instruction is redundant. Otherwise, return {instr}.
  HValue* store(HStoreNamedField* instr) {
    if (instr->access().IsInobject() &&
        !instr->access().existing_inobject_property()) {
      TRACE(("  skipping non existing property initialization store\n"));
      return instr;
    }

    int field = FieldOf(instr->access());
    if (field < 0) return KillIfMisaligned(instr);

    HValue* object = instr->object()->ActualValue();
    HValue* value = instr->value();

    if (instr->has_transition()) {
      // A transition introduces a new field and alters the map of the object.
      // Since the field in the object is new, it cannot alias existing entries.
      // TODO(titzer): introduce a constant for the new map and remember it.
      KillFieldInternal(object, FieldOf(JSObject::kMapOffset), NULL);
    } else {
      // Kill non-equivalent may-alias entries.
      KillFieldInternal(object, field, value);
    }
    HFieldApproximation* approx = FindOrCreate(object, field);

    if (Equal(approx->last_value_, value)) {
      // The store is redundant because the field already has this value.
      return NULL;
    } else {
      // The store is not redundant. Update the entry.
      approx->last_value_ = value;
      return instr;
    }
  }

  // Kill everything in this table.
  void Kill() {
    fields_.Rewind(0);
  }

  // Kill all entries matching the given offset.
  void KillOffset(int offset) {
    int field = FieldOf(offset);
    if (field >= 0 && field < fields_.length()) {
      fields_[field] = NULL;
    }
  }

  // Kill all entries aliasing the given store.
  void KillStore(HStoreNamedField* s) {
    int field = FieldOf(s->access());
    if (field >= 0) {
      KillFieldInternal(s->object()->ActualValue(), field, s->value());
    } else {
      KillIfMisaligned(s);
    }
  }

  // Kill multiple entries in the case of a misaligned store.
  HValue* KillIfMisaligned(HStoreNamedField* instr) {
    HObjectAccess access = instr->access();
    if (access.IsInobject()) {
      int offset = access.offset();
      if ((offset % kPointerSize) != 0) {
        // Kill the field containing the first word of the access.
        HValue* object = instr->object()->ActualValue();
        int field = offset / kPointerSize;
        KillFieldInternal(object, field, NULL);

        // Kill the next field in case of overlap.
        int size = access.representation().size();
        int next_field = (offset + size - 1) / kPointerSize;
        if (next_field != field) KillFieldInternal(object, next_field, NULL);
      }
    }
    return instr;
  }

  // Find an entry for the given object and field pair.
  HFieldApproximation* Find(HValue* object, int field) {
    // Search for a field approximation for this object.
    HFieldApproximation* approx = fields_[field];
    while (approx != NULL) {
      if (aliasing_->MustAlias(object, approx->object_)) return approx;
      approx = approx->next_;
    }
    return NULL;
  }

  // Find or create an entry for the given object and field pair.
  HFieldApproximation* FindOrCreate(HValue* object, int field) {
    EnsureFields(field + 1);

    // Search for a field approximation for this object.
    HFieldApproximation* approx = fields_[field];
    int count = 0;
    while (approx != NULL) {
      if (aliasing_->MustAlias(object, approx->object_)) return approx;
      count++;
      approx = approx->next_;
    }

    if (count >= kMaxTrackedObjects) {
      // Pull the last entry off the end and repurpose it for this object.
      approx = ReuseLastApproximation(field);
    } else {
      // Allocate a new entry.
      approx = new(zone_) HFieldApproximation();
    }

    // Insert the entry at the head of the list.
    approx->object_ = object;
    approx->last_value_ = NULL;
    approx->next_ = fields_[field];
    fields_[field] = approx;

    return approx;
  }

  // Kill all entries for a given field that _may_ alias the given object
  // and do _not_ have the given value.
  void KillFieldInternal(HValue* object, int field, HValue* value) {
    if (field >= fields_.length()) return;  // Nothing to do.

    HFieldApproximation* approx = fields_[field];
    HFieldApproximation* prev = NULL;
    while (approx != NULL) {
      if (aliasing_->MayAlias(object, approx->object_)) {
        if (!Equal(approx->last_value_, value)) {
          // Kill an aliasing entry that doesn't agree on the value.
          if (prev != NULL) {
            prev->next_ = approx->next_;
          } else {
            fields_[field] = approx->next_;
          }
          approx = approx->next_;
          continue;
        }
      }
      prev = approx;
      approx = approx->next_;
    }
  }

  bool Equal(HValue* a, HValue* b) {
    if (a == b) return true;
    if (a != NULL && b != NULL && a->CheckFlag(HValue::kUseGVN)) {
      return a->Equals(b);
    }
    return false;
  }

  // Remove the last approximation for a field so that it can be reused.
  // We reuse the last entry because it was the first inserted and is thus
  // farthest away from the current instruction.
  HFieldApproximation* ReuseLastApproximation(int field) {
    HFieldApproximation* approx = fields_[field];
    DCHECK(approx != NULL);

    HFieldApproximation* prev = NULL;
    while (approx->next_ != NULL) {
      prev = approx;
      approx = approx->next_;
    }
    if (prev != NULL) prev->next_ = NULL;
    return approx;
  }

  // Compute the field index for the given object access; -1 if not tracked.
  int FieldOf(HObjectAccess access) {
    return access.IsInobject() ? FieldOf(access.offset()) : -1;
  }

  // Compute the field index for the given in-object offset; -1 if not tracked.
  int FieldOf(int offset) {
    if (offset >= kMaxTrackedFields * kPointerSize) return -1;
    // TODO(titzer): track misaligned loads in a separate list?
    if ((offset % kPointerSize) != 0) return -1;  // Ignore misaligned accesses.
    return offset / kPointerSize;
  }

  // Ensure internal storage for the given number of fields.
  void EnsureFields(int num_fields) {
    if (fields_.length() < num_fields) {
      fields_.AddBlock(NULL, num_fields - fields_.length(), zone_);
    }
  }

  // Print this table to stdout.
  void Print() {
    for (int i = 0; i < fields_.length(); i++) {
      PrintF("  field %d: ", i);
      for (HFieldApproximation* a = fields_[i]; a != NULL; a = a->next_) {
        PrintF("[o%d =", a->object_->id());
        if (a->last_value_ != NULL) PrintF(" v%d", a->last_value_->id());
        PrintF("] ");
      }
      PrintF("\n");
    }
  }

  Zone* zone_;
  ZoneList<HFieldApproximation*> fields_;
  HAliasAnalyzer* aliasing_;
};


// Support for HFlowEngine: collect store effects within loops.
class HLoadEliminationEffects : public ZoneObject {
 public:
  explicit HLoadEliminationEffects(Zone* zone)
    : zone_(zone), stores_(5, zone) { }

  inline bool Disabled() {
    return false;  // Effects are _not_ disabled.
  }

  // Process a possibly side-effecting instruction.
  void Process(HInstruction* instr, Zone* zone) {
    if (instr->IsStoreNamedField()) {
      stores_.Add(HStoreNamedField::cast(instr), zone_);
    } else {
      flags_.Add(instr->ChangesFlags());
    }
  }

  // Apply these effects to the given load elimination table.
  void Apply(HLoadEliminationTable* table) {
    // Loads must not be hoisted past the OSR entry, therefore we kill
    // everything if we see an OSR entry.
    if (flags_.Contains(kInobjectFields) || flags_.Contains(kOsrEntries)) {
      table->Kill();
      return;
    }
    if (flags_.Contains(kElementsKind) || flags_.Contains(kMaps)) {
      table->KillOffset(JSObject::kMapOffset);
    }
    if (flags_.Contains(kElementsKind) || flags_.Contains(kElementsPointer)) {
      table->KillOffset(JSObject::kElementsOffset);
    }

    // Kill non-agreeing fields for each store contained in these effects.
    for (int i = 0; i < stores_.length(); i++) {
      table->KillStore(stores_[i]);
    }
  }

  // Union these effects with the other effects.
  void Union(HLoadEliminationEffects* that, Zone* zone) {
    flags_.Add(that->flags_);
    for (int i = 0; i < that->stores_.length(); i++) {
      stores_.Add(that->stores_[i], zone);
    }
  }

 private:
  Zone* zone_;
  GVNFlagSet flags_;
  ZoneList<HStoreNamedField*> stores_;
};


// The main routine of the analysis phase. Use the HFlowEngine for either a
// local or a global analysis.
void HLoadEliminationPhase::Run() {
  HFlowEngine<HLoadEliminationTable, HLoadEliminationEffects>
    engine(graph(), zone());
  HAliasAnalyzer aliasing;
  HLoadEliminationTable* table =
      new(zone()) HLoadEliminationTable(zone(), &aliasing);

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
}

}  // namespace internal
}  // namespace v8
