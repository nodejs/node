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

namespace v8 {
namespace internal {

static const int kMaxTrackedObjects = 10;
typedef UniqueSet<Map>* MapSet;

// The main datastructure used during check elimination, which stores a
// set of known maps for each object.
class HCheckTable {
 public:
  explicit HCheckTable(Zone* zone) : zone_(zone) {
    Kill();
    redundant_ = 0;
    narrowed_ = 0;
    empty_ = 0;
    removed_ = 0;
    compares_true_ = 0;
    compares_false_ = 0;
    transitions_ = 0;
    loads_ = 0;
  }

  void ReduceCheckMaps(HCheckMaps* instr) {
    HValue* object = instr->value()->ActualValue();
    int index = Find(object);
    if (index >= 0) {
      // entry found;
      MapSet a = known_maps_[index];
      MapSet i = instr->map_set().Copy(zone_);
      if (a->IsSubset(i)) {
        // The first check is more strict; the second is redundant.
        if (checks_[index] != NULL) {
          instr->DeleteAndReplaceWith(checks_[index]);
          redundant_++;
        } else {
          instr->DeleteAndReplaceWith(instr->value());
          removed_++;
        }
        return;
      }
      i = i->Intersect(a, zone_);
      if (i->size() == 0) {
        // Intersection is empty; probably megamorphic, which is likely to
        // deopt anyway, so just leave things as they are.
        empty_++;
      } else {
        // TODO(titzer): replace the first check with a more strict check.
        narrowed_++;
      }
    } else {
      // No entry; insert a new one.
      Insert(object, instr, instr->map_set().Copy(zone_));
    }
  }

  void ReduceCheckValue(HCheckValue* instr) {
    // Canonicalize HCheckValues; they might have their values load-eliminated.
    HValue* value = instr->Canonicalize();
    if (value == NULL) {
      instr->DeleteAndReplaceWith(instr->value());
      removed_++;
    } else if (value != instr) {
      instr->DeleteAndReplaceWith(value);
      redundant_++;
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
    loads_++;
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
          removed_++;
        } else {
          // Only one map survives the check.
          maps->Clear();
          maps->Add(map, zone_);
        }
      }
    } else {
      // No prior information.
      Insert(object, map);
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
    } else if (instr->CheckGVNFlag(kChangesMaps)) {
      // This store indirectly changes the map of the object.
      Kill(instr->object());
      UNREACHABLE();
    }
  }

  void ReduceCompareMap(HCompareMap* instr) {
    MapSet maps = FindMaps(instr->value()->ActualValue());
    if (maps == NULL) return;
    if (maps->Contains(instr->map())) {
      // TODO(titzer): replace with goto true branch
      if (maps->size() == 1) compares_true_++;
    } else {
      // TODO(titzer): replace with goto false branch
      compares_false_++;
    }
  }

  void ReduceTransitionElementsKind(HTransitionElementsKind* instr) {
    MapSet maps = FindMaps(instr->object()->ActualValue());
    // Can only learn more about an object that already has a known set of maps.
    if (maps == NULL) return;
    if (maps->Contains(instr->original_map())) {
      // If the object has the original map, it will be transitioned.
      maps->Remove(instr->original_map());
      maps->Add(instr->transitioned_map(), zone_);
    } else {
      // Object does not have the given map, thus the transition is redundant.
      instr->DeleteAndReplaceWith(instr->object());
      transitions_++;
    }
  }

  // Kill everything in the table.
  void Kill() {
    memset(objects_, 0, sizeof(objects_));
  }

  // Kill everything in the table that may alias {object}.
  void Kill(HValue* object) {
    for (int i = 0; i < kMaxTrackedObjects; i++) {
      if (objects_[i] == NULL) continue;
      if (aliasing_.MayAlias(objects_[i], object)) objects_[i] = NULL;
    }
    ASSERT(Find(object) < 0);
  }

  void Print() {
    for (int i = 0; i < kMaxTrackedObjects; i++) {
      if (objects_[i] == NULL) continue;
      PrintF("  checkmaps-table @%d: object #%d ", i, objects_[i]->id());
      if (checks_[i] != NULL) {
        PrintF("check #%d ", checks_[i]->id());
      }
      MapSet list = known_maps_[i];
      PrintF("%d maps { ", list->size());
      for (int j = 0; j < list->size(); j++) {
        if (j > 0) PrintF(", ");
        PrintF("%" V8PRIxPTR, list->at(j).Hashcode());
      }
      PrintF(" }\n");
    }
  }

  void PrintStats() {
    if (redundant_ > 0)      PrintF("  redundant   = %2d\n", redundant_);
    if (removed_ > 0)        PrintF("  removed     = %2d\n", removed_);
    if (narrowed_ > 0)       PrintF("  narrowed    = %2d\n", narrowed_);
    if (loads_ > 0)          PrintF("  loads       = %2d\n", loads_);
    if (empty_ > 0)          PrintF("  empty       = %2d\n", empty_);
    if (compares_true_ > 0)  PrintF("  cmp_true    = %2d\n", compares_true_);
    if (compares_false_ > 0) PrintF("  cmp_false   = %2d\n", compares_false_);
    if (transitions_ > 0)    PrintF("  transitions = %2d\n", transitions_);
  }

 private:
  int Find(HValue* object) {
    for (int i = 0; i < kMaxTrackedObjects; i++) {
      if (objects_[i] == NULL) continue;
      if (aliasing_.MustAlias(objects_[i], object)) return i;
    }
    return -1;
  }

  MapSet FindMaps(HValue* object) {
    int index = Find(object);
    return index < 0 ? NULL : known_maps_[index];
  }

  void Insert(HValue* object, Unique<Map> map) {
    MapSet list = new(zone_) UniqueSet<Map>();
    list->Add(map, zone_);
    Insert(object, NULL, list);
  }

  void Insert(HValue* object, HCheckMaps* check, MapSet maps) {
    for (int i = 0; i < kMaxTrackedObjects; i++) {
      // TODO(titzer): drop old entries instead of disallowing new ones.
      if (objects_[i] == NULL) {
        objects_[i] = object;
        checks_[i] = check;
        known_maps_[i] = maps;
        return;
      }
    }
  }

  bool IsMapAccess(HObjectAccess access) {
    return access.IsInobject() && access.offset() == JSObject::kMapOffset;
  }

  Unique<Map> MapConstant(HValue* value) {
    return Unique<Map>::cast(HConstant::cast(value)->GetUnique());
  }

  Zone* zone_;
  HValue* objects_[kMaxTrackedObjects];
  HValue* checks_[kMaxTrackedObjects];
  MapSet known_maps_[kMaxTrackedObjects];
  HAliasAnalyzer aliasing_;
  int redundant_;
  int removed_;
  int narrowed_;
  int loads_;
  int empty_;
  int compares_true_;
  int compares_false_;
  int transitions_;
};


void HCheckEliminationPhase::Run() {
  for (int i = 0; i < graph()->blocks()->length(); i++) {
    EliminateLocalChecks(graph()->blocks()->at(i));
  }
}


// For code de-uglification.
#define TRACE(x) if (FLAG_trace_check_elimination) PrintF x


// Eliminate checks local to a block.
void HCheckEliminationPhase::EliminateLocalChecks(HBasicBlock* block) {
  HCheckTable table(zone());
  TRACE(("-- check-elim B%d ------------------------------------------------\n",
         block->block_id()));

  for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
    bool changed = false;
    HInstruction* instr = it.Current();

    switch (instr->opcode()) {
      case HValue::kCheckMaps: {
        table.ReduceCheckMaps(HCheckMaps::cast(instr));
        changed = true;
        break;
      }
      case HValue::kCheckValue: {
        table.ReduceCheckValue(HCheckValue::cast(instr));
        changed = true;
        break;
      }
      case HValue::kLoadNamedField: {
        table.ReduceLoadNamedField(HLoadNamedField::cast(instr));
        changed = true;
        break;
      }
      case HValue::kStoreNamedField: {
        table.ReduceStoreNamedField(HStoreNamedField::cast(instr));
        changed = true;
        break;
      }
      case HValue::kCompareMap: {
        table.ReduceCompareMap(HCompareMap::cast(instr));
        changed = true;
        break;
      }
      case HValue::kTransitionElementsKind: {
        table.ReduceTransitionElementsKind(
            HTransitionElementsKind::cast(instr));
        changed = true;
        break;
      }
      case HValue::kCheckMapValue: {
        table.ReduceCheckMapValue(HCheckMapValue::cast(instr));
        changed = true;
        break;
      }
      default: {
        // If the instruction changes maps uncontrollably, kill the whole town.
        if (instr->CheckGVNFlag(kChangesMaps)) {
          table.Kill();
          changed = true;
        }
      }
      // Improvements possible:
      // - eliminate HCheckSmi and HCheckHeapObject
    }

    if (changed && FLAG_trace_check_elimination) table.Print();
  }

  if (FLAG_trace_check_elimination) table.PrintStats();
}


} }  // namespace v8::internal
