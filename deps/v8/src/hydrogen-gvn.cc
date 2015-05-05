// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/hydrogen.h"
#include "src/hydrogen-gvn.h"
#include "src/v8.h"

namespace v8 {
namespace internal {

class HInstructionMap FINAL : public ZoneObject {
 public:
  HInstructionMap(Zone* zone, SideEffectsTracker* side_effects_tracker)
      : array_size_(0),
        lists_size_(0),
        count_(0),
        array_(NULL),
        lists_(NULL),
        free_list_head_(kNil),
        side_effects_tracker_(side_effects_tracker) {
    ResizeLists(kInitialSize, zone);
    Resize(kInitialSize, zone);
  }

  void Kill(SideEffects side_effects);

  void Add(HInstruction* instr, Zone* zone) {
    present_depends_on_.Add(side_effects_tracker_->ComputeDependsOn(instr));
    Insert(instr, zone);
  }

  HInstruction* Lookup(HInstruction* instr) const;

  HInstructionMap* Copy(Zone* zone) const {
    return new(zone) HInstructionMap(zone, this);
  }

  bool IsEmpty() const { return count_ == 0; }

 private:
  // A linked list of HInstruction* values.  Stored in arrays.
  struct HInstructionMapListElement {
    HInstruction* instr;
    int next;  // Index in the array of the next list element.
  };
  static const int kNil = -1;  // The end of a linked list

  // Must be a power of 2.
  static const int kInitialSize = 16;

  HInstructionMap(Zone* zone, const HInstructionMap* other);

  void Resize(int new_size, Zone* zone);
  void ResizeLists(int new_size, Zone* zone);
  void Insert(HInstruction* instr, Zone* zone);
  uint32_t Bound(uint32_t value) const { return value & (array_size_ - 1); }

  int array_size_;
  int lists_size_;
  int count_;  // The number of values stored in the HInstructionMap.
  SideEffects present_depends_on_;
  HInstructionMapListElement* array_;
  // Primary store - contains the first value
  // with a given hash.  Colliding elements are stored in linked lists.
  HInstructionMapListElement* lists_;
  // The linked lists containing hash collisions.
  int free_list_head_;  // Unused elements in lists_ are on the free list.
  SideEffectsTracker* side_effects_tracker_;
};


class HSideEffectMap FINAL BASE_EMBEDDED {
 public:
  HSideEffectMap();
  explicit HSideEffectMap(HSideEffectMap* other);
  HSideEffectMap& operator= (const HSideEffectMap& other);

  void Kill(SideEffects side_effects);

  void Store(SideEffects side_effects, HInstruction* instr);

  bool IsEmpty() const { return count_ == 0; }

  inline HInstruction* operator[](int i) const {
    DCHECK(0 <= i);
    DCHECK(i < kNumberOfTrackedSideEffects);
    return data_[i];
  }
  inline HInstruction* at(int i) const { return operator[](i); }

 private:
  int count_;
  HInstruction* data_[kNumberOfTrackedSideEffects];
};


void TraceGVN(const char* msg, ...) {
  va_list arguments;
  va_start(arguments, msg);
  base::OS::VPrint(msg, arguments);
  va_end(arguments);
}


// Wrap TraceGVN in macros to avoid the expense of evaluating its arguments when
// --trace-gvn is off.
#define TRACE_GVN_1(msg, a1)                    \
  if (FLAG_trace_gvn) {                         \
    TraceGVN(msg, a1);                          \
  }

#define TRACE_GVN_2(msg, a1, a2)                \
  if (FLAG_trace_gvn) {                         \
    TraceGVN(msg, a1, a2);                      \
  }

#define TRACE_GVN_3(msg, a1, a2, a3)            \
  if (FLAG_trace_gvn) {                         \
    TraceGVN(msg, a1, a2, a3);                  \
  }

#define TRACE_GVN_4(msg, a1, a2, a3, a4)        \
  if (FLAG_trace_gvn) {                         \
    TraceGVN(msg, a1, a2, a3, a4);              \
  }

#define TRACE_GVN_5(msg, a1, a2, a3, a4, a5)    \
  if (FLAG_trace_gvn) {                         \
    TraceGVN(msg, a1, a2, a3, a4, a5);          \
  }


HInstructionMap::HInstructionMap(Zone* zone, const HInstructionMap* other)
    : array_size_(other->array_size_),
      lists_size_(other->lists_size_),
      count_(other->count_),
      present_depends_on_(other->present_depends_on_),
      array_(zone->NewArray<HInstructionMapListElement>(other->array_size_)),
      lists_(zone->NewArray<HInstructionMapListElement>(other->lists_size_)),
      free_list_head_(other->free_list_head_),
      side_effects_tracker_(other->side_effects_tracker_) {
  MemCopy(array_, other->array_,
          array_size_ * sizeof(HInstructionMapListElement));
  MemCopy(lists_, other->lists_,
          lists_size_ * sizeof(HInstructionMapListElement));
}


void HInstructionMap::Kill(SideEffects changes) {
  if (!present_depends_on_.ContainsAnyOf(changes)) return;
  present_depends_on_.RemoveAll();
  for (int i = 0; i < array_size_; ++i) {
    HInstruction* instr = array_[i].instr;
    if (instr != NULL) {
      // Clear list of collisions first, so we know if it becomes empty.
      int kept = kNil;  // List of kept elements.
      int next;
      for (int current = array_[i].next; current != kNil; current = next) {
        next = lists_[current].next;
        HInstruction* instr = lists_[current].instr;
        SideEffects depends_on = side_effects_tracker_->ComputeDependsOn(instr);
        if (depends_on.ContainsAnyOf(changes)) {
          // Drop it.
          count_--;
          lists_[current].next = free_list_head_;
          free_list_head_ = current;
        } else {
          // Keep it.
          lists_[current].next = kept;
          kept = current;
          present_depends_on_.Add(depends_on);
        }
      }
      array_[i].next = kept;

      // Now possibly drop directly indexed element.
      instr = array_[i].instr;
      SideEffects depends_on = side_effects_tracker_->ComputeDependsOn(instr);
      if (depends_on.ContainsAnyOf(changes)) {  // Drop it.
        count_--;
        int head = array_[i].next;
        if (head == kNil) {
          array_[i].instr = NULL;
        } else {
          array_[i].instr = lists_[head].instr;
          array_[i].next = lists_[head].next;
          lists_[head].next = free_list_head_;
          free_list_head_ = head;
        }
      } else {
        present_depends_on_.Add(depends_on);  // Keep it.
      }
    }
  }
}


HInstruction* HInstructionMap::Lookup(HInstruction* instr) const {
  uint32_t hash = static_cast<uint32_t>(instr->Hashcode());
  uint32_t pos = Bound(hash);
  if (array_[pos].instr != NULL) {
    if (array_[pos].instr->Equals(instr)) return array_[pos].instr;
    int next = array_[pos].next;
    while (next != kNil) {
      if (lists_[next].instr->Equals(instr)) return lists_[next].instr;
      next = lists_[next].next;
    }
  }
  return NULL;
}


void HInstructionMap::Resize(int new_size, Zone* zone) {
  DCHECK(new_size > count_);
  // Hashing the values into the new array has no more collisions than in the
  // old hash map, so we can use the existing lists_ array, if we are careful.

  // Make sure we have at least one free element.
  if (free_list_head_ == kNil) {
    ResizeLists(lists_size_ << 1, zone);
  }

  HInstructionMapListElement* new_array =
      zone->NewArray<HInstructionMapListElement>(new_size);
  memset(new_array, 0, sizeof(HInstructionMapListElement) * new_size);

  HInstructionMapListElement* old_array = array_;
  int old_size = array_size_;

  int old_count = count_;
  count_ = 0;
  // Do not modify present_depends_on_.  It is currently correct.
  array_size_ = new_size;
  array_ = new_array;

  if (old_array != NULL) {
    // Iterate over all the elements in lists, rehashing them.
    for (int i = 0; i < old_size; ++i) {
      if (old_array[i].instr != NULL) {
        int current = old_array[i].next;
        while (current != kNil) {
          Insert(lists_[current].instr, zone);
          int next = lists_[current].next;
          lists_[current].next = free_list_head_;
          free_list_head_ = current;
          current = next;
        }
        // Rehash the directly stored instruction.
        Insert(old_array[i].instr, zone);
      }
    }
  }
  USE(old_count);
  DCHECK(count_ == old_count);
}


void HInstructionMap::ResizeLists(int new_size, Zone* zone) {
  DCHECK(new_size > lists_size_);

  HInstructionMapListElement* new_lists =
      zone->NewArray<HInstructionMapListElement>(new_size);
  memset(new_lists, 0, sizeof(HInstructionMapListElement) * new_size);

  HInstructionMapListElement* old_lists = lists_;
  int old_size = lists_size_;

  lists_size_ = new_size;
  lists_ = new_lists;

  if (old_lists != NULL) {
    MemCopy(lists_, old_lists, old_size * sizeof(HInstructionMapListElement));
  }
  for (int i = old_size; i < lists_size_; ++i) {
    lists_[i].next = free_list_head_;
    free_list_head_ = i;
  }
}


void HInstructionMap::Insert(HInstruction* instr, Zone* zone) {
  DCHECK(instr != NULL);
  // Resizing when half of the hashtable is filled up.
  if (count_ >= array_size_ >> 1) Resize(array_size_ << 1, zone);
  DCHECK(count_ < array_size_);
  count_++;
  uint32_t pos = Bound(static_cast<uint32_t>(instr->Hashcode()));
  if (array_[pos].instr == NULL) {
    array_[pos].instr = instr;
    array_[pos].next = kNil;
  } else {
    if (free_list_head_ == kNil) {
      ResizeLists(lists_size_ << 1, zone);
    }
    int new_element_pos = free_list_head_;
    DCHECK(new_element_pos != kNil);
    free_list_head_ = lists_[free_list_head_].next;
    lists_[new_element_pos].instr = instr;
    lists_[new_element_pos].next = array_[pos].next;
    DCHECK(array_[pos].next == kNil || lists_[array_[pos].next].instr != NULL);
    array_[pos].next = new_element_pos;
  }
}


HSideEffectMap::HSideEffectMap() : count_(0) {
  memset(data_, 0, kNumberOfTrackedSideEffects * kPointerSize);
}


HSideEffectMap::HSideEffectMap(HSideEffectMap* other) : count_(other->count_) {
  *this = *other;  // Calls operator=.
}


HSideEffectMap& HSideEffectMap::operator=(const HSideEffectMap& other) {
  if (this != &other) {
    MemCopy(data_, other.data_, kNumberOfTrackedSideEffects * kPointerSize);
  }
  return *this;
}


void HSideEffectMap::Kill(SideEffects side_effects) {
  for (int i = 0; i < kNumberOfTrackedSideEffects; i++) {
    if (side_effects.ContainsFlag(GVNFlagFromInt(i))) {
      if (data_[i] != NULL) count_--;
      data_[i] = NULL;
    }
  }
}


void HSideEffectMap::Store(SideEffects side_effects, HInstruction* instr) {
  for (int i = 0; i < kNumberOfTrackedSideEffects; i++) {
    if (side_effects.ContainsFlag(GVNFlagFromInt(i))) {
      if (data_[i] == NULL) count_++;
      data_[i] = instr;
    }
  }
}


SideEffects SideEffectsTracker::ComputeChanges(HInstruction* instr) {
  int index;
  SideEffects result(instr->ChangesFlags());
  if (result.ContainsFlag(kGlobalVars)) {
    if (instr->IsStoreNamedField()) {
      HStoreNamedField* store = HStoreNamedField::cast(instr);
      HConstant* target = HConstant::cast(store->object());
      if (ComputeGlobalVar(Unique<PropertyCell>::cast(target->GetUnique()),
                           &index)) {
        result.RemoveFlag(kGlobalVars);
        result.AddSpecial(GlobalVar(index));
        return result;
      }
    }
    for (index = 0; index < kNumberOfGlobalVars; ++index) {
      result.AddSpecial(GlobalVar(index));
    }
  } else if (result.ContainsFlag(kInobjectFields)) {
    if (instr->IsStoreNamedField() &&
        ComputeInobjectField(HStoreNamedField::cast(instr)->access(), &index)) {
      result.RemoveFlag(kInobjectFields);
      result.AddSpecial(InobjectField(index));
    } else {
      for (index = 0; index < kNumberOfInobjectFields; ++index) {
        result.AddSpecial(InobjectField(index));
      }
    }
  }
  return result;
}


SideEffects SideEffectsTracker::ComputeDependsOn(HInstruction* instr) {
  int index;
  SideEffects result(instr->DependsOnFlags());
  if (result.ContainsFlag(kGlobalVars)) {
    if (instr->IsLoadNamedField()) {
      HLoadNamedField* load = HLoadNamedField::cast(instr);
      HConstant* target = HConstant::cast(load->object());
      if (ComputeGlobalVar(Unique<PropertyCell>::cast(target->GetUnique()),
                           &index)) {
        result.RemoveFlag(kGlobalVars);
        result.AddSpecial(GlobalVar(index));
        return result;
      }
    }
    for (index = 0; index < kNumberOfGlobalVars; ++index) {
      result.AddSpecial(GlobalVar(index));
    }
  } else if (result.ContainsFlag(kInobjectFields)) {
    if (instr->IsLoadNamedField() &&
        ComputeInobjectField(HLoadNamedField::cast(instr)->access(), &index)) {
      result.RemoveFlag(kInobjectFields);
      result.AddSpecial(InobjectField(index));
    } else {
      for (index = 0; index < kNumberOfInobjectFields; ++index) {
        result.AddSpecial(InobjectField(index));
      }
    }
  }
  return result;
}


std::ostream& operator<<(std::ostream& os, const TrackedEffects& te) {
  SideEffectsTracker* t = te.tracker;
  const char* separator = "";
  os << "[";
  for (int bit = 0; bit < kNumberOfFlags; ++bit) {
    GVNFlag flag = GVNFlagFromInt(bit);
    if (te.effects.ContainsFlag(flag)) {
      os << separator;
      separator = ", ";
      switch (flag) {
#define DECLARE_FLAG(Type) \
  case k##Type:            \
    os << #Type;           \
    break;
GVN_TRACKED_FLAG_LIST(DECLARE_FLAG)
GVN_UNTRACKED_FLAG_LIST(DECLARE_FLAG)
#undef DECLARE_FLAG
        default:
            break;
      }
    }
  }
  for (int index = 0; index < t->num_global_vars_; ++index) {
    if (te.effects.ContainsSpecial(t->GlobalVar(index))) {
      os << separator << "[" << *t->global_vars_[index].handle() << "]";
      separator = ", ";
    }
  }
  for (int index = 0; index < t->num_inobject_fields_; ++index) {
    if (te.effects.ContainsSpecial(t->InobjectField(index))) {
      os << separator << t->inobject_fields_[index];
      separator = ", ";
    }
  }
  os << "]";
  return os;
}


bool SideEffectsTracker::ComputeGlobalVar(Unique<PropertyCell> cell,
                                          int* index) {
  for (int i = 0; i < num_global_vars_; ++i) {
    if (cell == global_vars_[i]) {
      *index = i;
      return true;
    }
  }
  if (num_global_vars_ < kNumberOfGlobalVars) {
    if (FLAG_trace_gvn) {
      OFStream os(stdout);
      os << "Tracking global var [" << *cell.handle() << "] "
         << "(mapped to index " << num_global_vars_ << ")" << std::endl;
    }
    *index = num_global_vars_;
    global_vars_[num_global_vars_++] = cell;
    return true;
  }
  return false;
}


bool SideEffectsTracker::ComputeInobjectField(HObjectAccess access,
                                              int* index) {
  for (int i = 0; i < num_inobject_fields_; ++i) {
    if (access.Equals(inobject_fields_[i])) {
      *index = i;
      return true;
    }
  }
  if (num_inobject_fields_ < kNumberOfInobjectFields) {
    if (FLAG_trace_gvn) {
      OFStream os(stdout);
      os << "Tracking inobject field access " << access << " (mapped to index "
         << num_inobject_fields_ << ")" << std::endl;
    }
    *index = num_inobject_fields_;
    inobject_fields_[num_inobject_fields_++] = access;
    return true;
  }
  return false;
}


HGlobalValueNumberingPhase::HGlobalValueNumberingPhase(HGraph* graph)
    : HPhase("H_Global value numbering", graph),
      removed_side_effects_(false),
      block_side_effects_(graph->blocks()->length(), zone()),
      loop_side_effects_(graph->blocks()->length(), zone()),
      visited_on_paths_(graph->blocks()->length(), zone()) {
  DCHECK(!AllowHandleAllocation::IsAllowed());
  block_side_effects_.AddBlock(
      SideEffects(), graph->blocks()->length(), zone());
  loop_side_effects_.AddBlock(
      SideEffects(), graph->blocks()->length(), zone());
}


void HGlobalValueNumberingPhase::Run() {
  DCHECK(!removed_side_effects_);
  for (int i = FLAG_gvn_iterations; i > 0; --i) {
    // Compute the side effects.
    ComputeBlockSideEffects();

    // Perform loop invariant code motion if requested.
    if (FLAG_loop_invariant_code_motion) LoopInvariantCodeMotion();

    // Perform the actual value numbering.
    AnalyzeGraph();

    // Continue GVN if we removed any side effects.
    if (!removed_side_effects_) break;
    removed_side_effects_ = false;

    // Clear all side effects.
    DCHECK_EQ(block_side_effects_.length(), graph()->blocks()->length());
    DCHECK_EQ(loop_side_effects_.length(), graph()->blocks()->length());
    for (int i = 0; i < graph()->blocks()->length(); ++i) {
      block_side_effects_[i].RemoveAll();
      loop_side_effects_[i].RemoveAll();
    }
    visited_on_paths_.Clear();
  }
}


void HGlobalValueNumberingPhase::ComputeBlockSideEffects() {
  for (int i = graph()->blocks()->length() - 1; i >= 0; --i) {
    // Compute side effects for the block.
    HBasicBlock* block = graph()->blocks()->at(i);
    SideEffects side_effects;
    if (block->IsReachable() && !block->IsDeoptimizing()) {
      int id = block->block_id();
      for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
        HInstruction* instr = it.Current();
        side_effects.Add(side_effects_tracker_.ComputeChanges(instr));
      }
      block_side_effects_[id].Add(side_effects);

      // Loop headers are part of their loop.
      if (block->IsLoopHeader()) {
        loop_side_effects_[id].Add(side_effects);
      }

      // Propagate loop side effects upwards.
      if (block->HasParentLoopHeader()) {
        HBasicBlock* with_parent = block;
        if (block->IsLoopHeader()) side_effects = loop_side_effects_[id];
        do {
          HBasicBlock* parent_block = with_parent->parent_loop_header();
          loop_side_effects_[parent_block->block_id()].Add(side_effects);
          with_parent = parent_block;
        } while (with_parent->HasParentLoopHeader());
      }
    }
  }
}


void HGlobalValueNumberingPhase::LoopInvariantCodeMotion() {
  TRACE_GVN_1("Using optimistic loop invariant code motion: %s\n",
              graph()->use_optimistic_licm() ? "yes" : "no");
  for (int i = graph()->blocks()->length() - 1; i >= 0; --i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    if (block->IsLoopHeader()) {
      SideEffects side_effects = loop_side_effects_[block->block_id()];
      if (FLAG_trace_gvn) {
        OFStream os(stdout);
        os << "Try loop invariant motion for " << *block << " changes "
           << Print(side_effects) << std::endl;
      }
      HBasicBlock* last = block->loop_information()->GetLastBackEdge();
      for (int j = block->block_id(); j <= last->block_id(); ++j) {
        ProcessLoopBlock(graph()->blocks()->at(j), block, side_effects);
      }
    }
  }
}


void HGlobalValueNumberingPhase::ProcessLoopBlock(
    HBasicBlock* block,
    HBasicBlock* loop_header,
    SideEffects loop_kills) {
  HBasicBlock* pre_header = loop_header->predecessors()->at(0);
  if (FLAG_trace_gvn) {
    OFStream os(stdout);
    os << "Loop invariant code motion for " << *block << " depends on "
       << Print(loop_kills) << std::endl;
  }
  HInstruction* instr = block->first();
  while (instr != NULL) {
    HInstruction* next = instr->next();
    if (instr->CheckFlag(HValue::kUseGVN)) {
      SideEffects changes = side_effects_tracker_.ComputeChanges(instr);
      SideEffects depends_on = side_effects_tracker_.ComputeDependsOn(instr);
      if (FLAG_trace_gvn) {
        OFStream os(stdout);
        os << "Checking instruction i" << instr->id() << " ("
           << instr->Mnemonic() << ") changes " << Print(changes)
           << ", depends on " << Print(depends_on) << ". Loop changes "
           << Print(loop_kills) << std::endl;
      }
      bool can_hoist = !depends_on.ContainsAnyOf(loop_kills);
      if (can_hoist && !graph()->use_optimistic_licm()) {
        can_hoist = block->IsLoopSuccessorDominator();
      }

      if (can_hoist) {
        bool inputs_loop_invariant = true;
        for (int i = 0; i < instr->OperandCount(); ++i) {
          if (instr->OperandAt(i)->IsDefinedAfter(pre_header)) {
            inputs_loop_invariant = false;
          }
        }

        if (inputs_loop_invariant && ShouldMove(instr, loop_header)) {
          TRACE_GVN_2("Hoisting loop invariant instruction i%d to block B%d\n",
                      instr->id(), pre_header->block_id());
          // Move the instruction out of the loop.
          instr->Unlink();
          instr->InsertBefore(pre_header->end());
          if (instr->HasSideEffects()) removed_side_effects_ = true;
        }
      }
    }
    instr = next;
  }
}


bool HGlobalValueNumberingPhase::AllowCodeMotion() {
  return info()->IsStub() || info()->opt_count() + 1 < FLAG_max_opt_count;
}


bool HGlobalValueNumberingPhase::ShouldMove(HInstruction* instr,
                                            HBasicBlock* loop_header) {
  // If we've disabled code motion or we're in a block that unconditionally
  // deoptimizes, don't move any instructions.
  return AllowCodeMotion() && !instr->block()->IsDeoptimizing() &&
      instr->block()->IsReachable();
}


SideEffects
HGlobalValueNumberingPhase::CollectSideEffectsOnPathsToDominatedBlock(
    HBasicBlock* dominator, HBasicBlock* dominated) {
  SideEffects side_effects;
  for (int i = 0; i < dominated->predecessors()->length(); ++i) {
    HBasicBlock* block = dominated->predecessors()->at(i);
    if (dominator->block_id() < block->block_id() &&
        block->block_id() < dominated->block_id() &&
        !visited_on_paths_.Contains(block->block_id())) {
      visited_on_paths_.Add(block->block_id());
      side_effects.Add(block_side_effects_[block->block_id()]);
      if (block->IsLoopHeader()) {
        side_effects.Add(loop_side_effects_[block->block_id()]);
      }
      side_effects.Add(CollectSideEffectsOnPathsToDominatedBlock(
          dominator, block));
    }
  }
  return side_effects;
}


// Each instance of this class is like a "stack frame" for the recursive
// traversal of the dominator tree done during GVN (the stack is handled
// as a double linked list).
// We reuse frames when possible so the list length is limited by the depth
// of the dominator tree but this forces us to initialize each frame calling
// an explicit "Initialize" method instead of a using constructor.
class GvnBasicBlockState: public ZoneObject {
 public:
  static GvnBasicBlockState* CreateEntry(Zone* zone,
                                         HBasicBlock* entry_block,
                                         HInstructionMap* entry_map) {
    return new(zone)
        GvnBasicBlockState(NULL, entry_block, entry_map, NULL, zone);
  }

  HBasicBlock* block() { return block_; }
  HInstructionMap* map() { return map_; }
  HSideEffectMap* dominators() { return &dominators_; }

  GvnBasicBlockState* next_in_dominator_tree_traversal(
      Zone* zone,
      HBasicBlock** dominator) {
    // This assignment needs to happen before calling next_dominated() because
    // that call can reuse "this" if we are at the last dominated block.
    *dominator = block();
    GvnBasicBlockState* result = next_dominated(zone);
    if (result == NULL) {
      GvnBasicBlockState* dominator_state = pop();
      if (dominator_state != NULL) {
        // This branch is guaranteed not to return NULL because pop() never
        // returns a state where "is_done() == true".
        *dominator = dominator_state->block();
        result = dominator_state->next_dominated(zone);
      } else {
        // Unnecessary (we are returning NULL) but done for cleanness.
        *dominator = NULL;
      }
    }
    return result;
  }

 private:
  void Initialize(HBasicBlock* block,
                  HInstructionMap* map,
                  HSideEffectMap* dominators,
                  bool copy_map,
                  Zone* zone) {
    block_ = block;
    map_ = copy_map ? map->Copy(zone) : map;
    dominated_index_ = -1;
    length_ = block->dominated_blocks()->length();
    if (dominators != NULL) {
      dominators_ = *dominators;
    }
  }
  bool is_done() { return dominated_index_ >= length_; }

  GvnBasicBlockState(GvnBasicBlockState* previous,
                     HBasicBlock* block,
                     HInstructionMap* map,
                     HSideEffectMap* dominators,
                     Zone* zone)
      : previous_(previous), next_(NULL) {
    Initialize(block, map, dominators, true, zone);
  }

  GvnBasicBlockState* next_dominated(Zone* zone) {
    dominated_index_++;
    if (dominated_index_ == length_ - 1) {
      // No need to copy the map for the last child in the dominator tree.
      Initialize(block_->dominated_blocks()->at(dominated_index_),
                 map(),
                 dominators(),
                 false,
                 zone);
      return this;
    } else if (dominated_index_ < length_) {
      return push(zone, block_->dominated_blocks()->at(dominated_index_));
    } else {
      return NULL;
    }
  }

  GvnBasicBlockState* push(Zone* zone, HBasicBlock* block) {
    if (next_ == NULL) {
      next_ =
          new(zone) GvnBasicBlockState(this, block, map(), dominators(), zone);
    } else {
      next_->Initialize(block, map(), dominators(), true, zone);
    }
    return next_;
  }
  GvnBasicBlockState* pop() {
    GvnBasicBlockState* result = previous_;
    while (result != NULL && result->is_done()) {
      TRACE_GVN_2("Backtracking from block B%d to block b%d\n",
                  block()->block_id(),
                  previous_->block()->block_id())
      result = result->previous_;
    }
    return result;
  }

  GvnBasicBlockState* previous_;
  GvnBasicBlockState* next_;
  HBasicBlock* block_;
  HInstructionMap* map_;
  HSideEffectMap dominators_;
  int dominated_index_;
  int length_;
};


// This is a recursive traversal of the dominator tree but it has been turned
// into a loop to avoid stack overflows.
// The logical "stack frames" of the recursion are kept in a list of
// GvnBasicBlockState instances.
void HGlobalValueNumberingPhase::AnalyzeGraph() {
  HBasicBlock* entry_block = graph()->entry_block();
  HInstructionMap* entry_map =
      new(zone()) HInstructionMap(zone(), &side_effects_tracker_);
  GvnBasicBlockState* current =
      GvnBasicBlockState::CreateEntry(zone(), entry_block, entry_map);

  while (current != NULL) {
    HBasicBlock* block = current->block();
    HInstructionMap* map = current->map();
    HSideEffectMap* dominators = current->dominators();

    TRACE_GVN_2("Analyzing block B%d%s\n",
                block->block_id(),
                block->IsLoopHeader() ? " (loop header)" : "");

    // If this is a loop header kill everything killed by the loop.
    if (block->IsLoopHeader()) {
      map->Kill(loop_side_effects_[block->block_id()]);
      dominators->Kill(loop_side_effects_[block->block_id()]);
    }

    // Go through all instructions of the current block.
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      if (instr->CheckFlag(HValue::kTrackSideEffectDominators)) {
        for (int i = 0; i < kNumberOfTrackedSideEffects; i++) {
          HValue* other = dominators->at(i);
          GVNFlag flag = GVNFlagFromInt(i);
          if (instr->DependsOnFlags().Contains(flag) && other != NULL) {
            TRACE_GVN_5("Side-effect #%d in %d (%s) is dominated by %d (%s)\n",
                        i,
                        instr->id(),
                        instr->Mnemonic(),
                        other->id(),
                        other->Mnemonic());
            if (instr->HandleSideEffectDominator(flag, other)) {
              removed_side_effects_ = true;
            }
          }
        }
      }
      // Instruction was unlinked during graph traversal.
      if (!instr->IsLinked()) continue;

      SideEffects changes = side_effects_tracker_.ComputeChanges(instr);
      if (!changes.IsEmpty()) {
        // Clear all instructions in the map that are affected by side effects.
        // Store instruction as the dominating one for tracked side effects.
        map->Kill(changes);
        dominators->Store(changes, instr);
        if (FLAG_trace_gvn) {
          OFStream os(stdout);
          os << "Instruction i" << instr->id() << " changes " << Print(changes)
             << std::endl;
        }
      }
      if (instr->CheckFlag(HValue::kUseGVN) &&
          !instr->CheckFlag(HValue::kCantBeReplaced)) {
        DCHECK(!instr->HasObservableSideEffects());
        HInstruction* other = map->Lookup(instr);
        if (other != NULL) {
          DCHECK(instr->Equals(other) && other->Equals(instr));
          TRACE_GVN_4("Replacing instruction i%d (%s) with i%d (%s)\n",
                      instr->id(),
                      instr->Mnemonic(),
                      other->id(),
                      other->Mnemonic());
          if (instr->HasSideEffects()) removed_side_effects_ = true;
          instr->DeleteAndReplaceWith(other);
        } else {
          map->Add(instr, zone());
        }
      }
    }

    HBasicBlock* dominator_block;
    GvnBasicBlockState* next =
        current->next_in_dominator_tree_traversal(zone(),
                                                  &dominator_block);

    if (next != NULL) {
      HBasicBlock* dominated = next->block();
      HInstructionMap* successor_map = next->map();
      HSideEffectMap* successor_dominators = next->dominators();

      // Kill everything killed on any path between this block and the
      // dominated block.  We don't have to traverse these paths if the
      // value map and the dominators list is already empty.  If the range
      // of block ids (block_id, dominated_id) is empty there are no such
      // paths.
      if ((!successor_map->IsEmpty() || !successor_dominators->IsEmpty()) &&
          dominator_block->block_id() + 1 < dominated->block_id()) {
        visited_on_paths_.Clear();
        SideEffects side_effects_on_all_paths =
            CollectSideEffectsOnPathsToDominatedBlock(dominator_block,
                                                      dominated);
        successor_map->Kill(side_effects_on_all_paths);
        successor_dominators->Kill(side_effects_on_all_paths);
      }
    }
    current = next;
  }
}

} }  // namespace v8::internal
