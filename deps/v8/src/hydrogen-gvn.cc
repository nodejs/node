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

#include "hydrogen.h"
#include "hydrogen-gvn.h"
#include "v8.h"

namespace v8 {
namespace internal {

class HValueMap: public ZoneObject {
 public:
  explicit HValueMap(Zone* zone)
      : array_size_(0),
        lists_size_(0),
        count_(0),
        present_flags_(0),
        array_(NULL),
        lists_(NULL),
        free_list_head_(kNil) {
    ResizeLists(kInitialSize, zone);
    Resize(kInitialSize, zone);
  }

  void Kill(GVNFlagSet flags);

  void Add(HValue* value, Zone* zone) {
    present_flags_.Add(value->gvn_flags());
    Insert(value, zone);
  }

  HValue* Lookup(HValue* value) const;

  HValueMap* Copy(Zone* zone) const {
    return new(zone) HValueMap(zone, this);
  }

  bool IsEmpty() const { return count_ == 0; }

 private:
  // A linked list of HValue* values.  Stored in arrays.
  struct HValueMapListElement {
    HValue* value;
    int next;  // Index in the array of the next list element.
  };
  static const int kNil = -1;  // The end of a linked list

  // Must be a power of 2.
  static const int kInitialSize = 16;

  HValueMap(Zone* zone, const HValueMap* other);

  void Resize(int new_size, Zone* zone);
  void ResizeLists(int new_size, Zone* zone);
  void Insert(HValue* value, Zone* zone);
  uint32_t Bound(uint32_t value) const { return value & (array_size_ - 1); }

  int array_size_;
  int lists_size_;
  int count_;  // The number of values stored in the HValueMap.
  GVNFlagSet present_flags_;  // All flags that are in any value in the
                              // HValueMap.
  HValueMapListElement* array_;  // Primary store - contains the first value
  // with a given hash.  Colliding elements are stored in linked lists.
  HValueMapListElement* lists_;  // The linked lists containing hash collisions.
  int free_list_head_;  // Unused elements in lists_ are on the free list.
};


class HSideEffectMap BASE_EMBEDDED {
 public:
  HSideEffectMap();
  explicit HSideEffectMap(HSideEffectMap* other);
  HSideEffectMap& operator= (const HSideEffectMap& other);

  void Kill(GVNFlagSet flags);

  void Store(GVNFlagSet flags, HInstruction* instr);

  bool IsEmpty() const { return count_ == 0; }

  inline HInstruction* operator[](int i) const {
    ASSERT(0 <= i);
    ASSERT(i < kNumberOfTrackedSideEffects);
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
  OS::VPrint(msg, arguments);
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


HValueMap::HValueMap(Zone* zone, const HValueMap* other)
    : array_size_(other->array_size_),
      lists_size_(other->lists_size_),
      count_(other->count_),
      present_flags_(other->present_flags_),
      array_(zone->NewArray<HValueMapListElement>(other->array_size_)),
      lists_(zone->NewArray<HValueMapListElement>(other->lists_size_)),
      free_list_head_(other->free_list_head_) {
  OS::MemCopy(
      array_, other->array_, array_size_ * sizeof(HValueMapListElement));
  OS::MemCopy(
      lists_, other->lists_, lists_size_ * sizeof(HValueMapListElement));
}


void HValueMap::Kill(GVNFlagSet flags) {
  GVNFlagSet depends_flags = HValue::ConvertChangesToDependsFlags(flags);
  if (!present_flags_.ContainsAnyOf(depends_flags)) return;
  present_flags_.RemoveAll();
  for (int i = 0; i < array_size_; ++i) {
    HValue* value = array_[i].value;
    if (value != NULL) {
      // Clear list of collisions first, so we know if it becomes empty.
      int kept = kNil;  // List of kept elements.
      int next;
      for (int current = array_[i].next; current != kNil; current = next) {
        next = lists_[current].next;
        HValue* value = lists_[current].value;
        if (value->gvn_flags().ContainsAnyOf(depends_flags)) {
          // Drop it.
          count_--;
          lists_[current].next = free_list_head_;
          free_list_head_ = current;
        } else {
          // Keep it.
          lists_[current].next = kept;
          kept = current;
          present_flags_.Add(value->gvn_flags());
        }
      }
      array_[i].next = kept;

      // Now possibly drop directly indexed element.
      value = array_[i].value;
      if (value->gvn_flags().ContainsAnyOf(depends_flags)) {  // Drop it.
        count_--;
        int head = array_[i].next;
        if (head == kNil) {
          array_[i].value = NULL;
        } else {
          array_[i].value = lists_[head].value;
          array_[i].next = lists_[head].next;
          lists_[head].next = free_list_head_;
          free_list_head_ = head;
        }
      } else {
        present_flags_.Add(value->gvn_flags());  // Keep it.
      }
    }
  }
}


HValue* HValueMap::Lookup(HValue* value) const {
  uint32_t hash = static_cast<uint32_t>(value->Hashcode());
  uint32_t pos = Bound(hash);
  if (array_[pos].value != NULL) {
    if (array_[pos].value->Equals(value)) return array_[pos].value;
    int next = array_[pos].next;
    while (next != kNil) {
      if (lists_[next].value->Equals(value)) return lists_[next].value;
      next = lists_[next].next;
    }
  }
  return NULL;
}


void HValueMap::Resize(int new_size, Zone* zone) {
  ASSERT(new_size > count_);
  // Hashing the values into the new array has no more collisions than in the
  // old hash map, so we can use the existing lists_ array, if we are careful.

  // Make sure we have at least one free element.
  if (free_list_head_ == kNil) {
    ResizeLists(lists_size_ << 1, zone);
  }

  HValueMapListElement* new_array =
      zone->NewArray<HValueMapListElement>(new_size);
  memset(new_array, 0, sizeof(HValueMapListElement) * new_size);

  HValueMapListElement* old_array = array_;
  int old_size = array_size_;

  int old_count = count_;
  count_ = 0;
  // Do not modify present_flags_.  It is currently correct.
  array_size_ = new_size;
  array_ = new_array;

  if (old_array != NULL) {
    // Iterate over all the elements in lists, rehashing them.
    for (int i = 0; i < old_size; ++i) {
      if (old_array[i].value != NULL) {
        int current = old_array[i].next;
        while (current != kNil) {
          Insert(lists_[current].value, zone);
          int next = lists_[current].next;
          lists_[current].next = free_list_head_;
          free_list_head_ = current;
          current = next;
        }
        // Rehash the directly stored value.
        Insert(old_array[i].value, zone);
      }
    }
  }
  USE(old_count);
  ASSERT(count_ == old_count);
}


void HValueMap::ResizeLists(int new_size, Zone* zone) {
  ASSERT(new_size > lists_size_);

  HValueMapListElement* new_lists =
      zone->NewArray<HValueMapListElement>(new_size);
  memset(new_lists, 0, sizeof(HValueMapListElement) * new_size);

  HValueMapListElement* old_lists = lists_;
  int old_size = lists_size_;

  lists_size_ = new_size;
  lists_ = new_lists;

  if (old_lists != NULL) {
    OS::MemCopy(lists_, old_lists, old_size * sizeof(HValueMapListElement));
  }
  for (int i = old_size; i < lists_size_; ++i) {
    lists_[i].next = free_list_head_;
    free_list_head_ = i;
  }
}


void HValueMap::Insert(HValue* value, Zone* zone) {
  ASSERT(value != NULL);
  // Resizing when half of the hashtable is filled up.
  if (count_ >= array_size_ >> 1) Resize(array_size_ << 1, zone);
  ASSERT(count_ < array_size_);
  count_++;
  uint32_t pos = Bound(static_cast<uint32_t>(value->Hashcode()));
  if (array_[pos].value == NULL) {
    array_[pos].value = value;
    array_[pos].next = kNil;
  } else {
    if (free_list_head_ == kNil) {
      ResizeLists(lists_size_ << 1, zone);
    }
    int new_element_pos = free_list_head_;
    ASSERT(new_element_pos != kNil);
    free_list_head_ = lists_[free_list_head_].next;
    lists_[new_element_pos].value = value;
    lists_[new_element_pos].next = array_[pos].next;
    ASSERT(array_[pos].next == kNil || lists_[array_[pos].next].value != NULL);
    array_[pos].next = new_element_pos;
  }
}


HSideEffectMap::HSideEffectMap() : count_(0) {
  memset(data_, 0, kNumberOfTrackedSideEffects * kPointerSize);
}


HSideEffectMap::HSideEffectMap(HSideEffectMap* other) : count_(other->count_) {
  *this = *other;  // Calls operator=.
}


HSideEffectMap& HSideEffectMap::operator= (const HSideEffectMap& other) {
  if (this != &other) {
    OS::MemCopy(data_, other.data_, kNumberOfTrackedSideEffects * kPointerSize);
  }
  return *this;
}

void HSideEffectMap::Kill(GVNFlagSet flags) {
  for (int i = 0; i < kNumberOfTrackedSideEffects; i++) {
    GVNFlag changes_flag = HValue::ChangesFlagFromInt(i);
    if (flags.Contains(changes_flag)) {
      if (data_[i] != NULL) count_--;
      data_[i] = NULL;
    }
  }
}


void HSideEffectMap::Store(GVNFlagSet flags, HInstruction* instr) {
  for (int i = 0; i < kNumberOfTrackedSideEffects; i++) {
    GVNFlag changes_flag = HValue::ChangesFlagFromInt(i);
    if (flags.Contains(changes_flag)) {
      if (data_[i] == NULL) count_++;
      data_[i] = instr;
    }
  }
}


HGlobalValueNumberingPhase::HGlobalValueNumberingPhase(HGraph* graph)
      : HPhase("H_Global value numbering", graph),
        removed_side_effects_(false),
        block_side_effects_(graph->blocks()->length(), zone()),
        loop_side_effects_(graph->blocks()->length(), zone()),
        visited_on_paths_(zone(), graph->blocks()->length()) {
    ASSERT(!AllowHandleAllocation::IsAllowed());
    block_side_effects_.AddBlock(GVNFlagSet(), graph->blocks()->length(),
                                 zone());
    loop_side_effects_.AddBlock(GVNFlagSet(), graph->blocks()->length(),
                                zone());
  }

void HGlobalValueNumberingPhase::Analyze() {
  removed_side_effects_ = false;
  ComputeBlockSideEffects();
  if (FLAG_loop_invariant_code_motion) {
    LoopInvariantCodeMotion();
  }
  AnalyzeGraph();
}


void HGlobalValueNumberingPhase::ComputeBlockSideEffects() {
  // The Analyze phase of GVN can be called multiple times. Clear loop side
  // effects before computing them to erase the contents from previous Analyze
  // passes.
  for (int i = 0; i < loop_side_effects_.length(); ++i) {
    loop_side_effects_[i].RemoveAll();
  }
  for (int i = graph()->blocks()->length() - 1; i >= 0; --i) {
    // Compute side effects for the block.
    HBasicBlock* block = graph()->blocks()->at(i);
    int id = block->block_id();
    GVNFlagSet side_effects;
    for (HInstructionIterator it(block); !it.Done(); it.Advance()) {
      HInstruction* instr = it.Current();
      side_effects.Add(instr->ChangesFlags());
      if (instr->IsSoftDeoptimize()) {
        block_side_effects_[id].RemoveAll();
        side_effects.RemoveAll();
        break;
      }
    }
    block_side_effects_[id].Add(side_effects);

    // Loop headers are part of their loop.
    if (block->IsLoopHeader()) {
      loop_side_effects_[id].Add(side_effects);
    }

    // Propagate loop side effects upwards.
    if (block->HasParentLoopHeader()) {
      int header_id = block->parent_loop_header()->block_id();
      loop_side_effects_[header_id].Add(block->IsLoopHeader()
                                        ? loop_side_effects_[id]
                                        : side_effects);
    }
  }
}


SmartArrayPointer<char> GetGVNFlagsString(GVNFlagSet flags) {
  char underlying_buffer[kLastFlag * 128];
  Vector<char> buffer(underlying_buffer, sizeof(underlying_buffer));
#if DEBUG
  int offset = 0;
  const char* separator = "";
  const char* comma = ", ";
  buffer[0] = 0;
  uint32_t set_depends_on = 0;
  uint32_t set_changes = 0;
  for (int bit = 0; bit < kLastFlag; ++bit) {
    if ((flags.ToIntegral() & (1 << bit)) != 0) {
      if (bit % 2 == 0) {
        set_changes++;
      } else {
        set_depends_on++;
      }
    }
  }
  bool positive_changes = set_changes < (kLastFlag / 2);
  bool positive_depends_on = set_depends_on < (kLastFlag / 2);
  if (set_changes > 0) {
    if (positive_changes) {
      offset += OS::SNPrintF(buffer + offset, "changes [");
    } else {
      offset += OS::SNPrintF(buffer + offset, "changes all except [");
    }
    for (int bit = 0; bit < kLastFlag; ++bit) {
      if (((flags.ToIntegral() & (1 << bit)) != 0) == positive_changes) {
        switch (static_cast<GVNFlag>(bit)) {
#define DECLARE_FLAG(type)                                       \
          case kChanges##type:                                   \
            offset += OS::SNPrintF(buffer + offset, separator);  \
            offset += OS::SNPrintF(buffer + offset, #type);      \
            separator = comma;                                   \
            break;
GVN_TRACKED_FLAG_LIST(DECLARE_FLAG)
GVN_UNTRACKED_FLAG_LIST(DECLARE_FLAG)
#undef DECLARE_FLAG
          default:
              break;
        }
      }
    }
    offset += OS::SNPrintF(buffer + offset, "]");
  }
  if (set_depends_on > 0) {
    separator = "";
    if (set_changes > 0) {
      offset += OS::SNPrintF(buffer + offset, ", ");
    }
    if (positive_depends_on) {
      offset += OS::SNPrintF(buffer + offset, "depends on [");
    } else {
      offset += OS::SNPrintF(buffer + offset, "depends on all except [");
    }
    for (int bit = 0; bit < kLastFlag; ++bit) {
      if (((flags.ToIntegral() & (1 << bit)) != 0) == positive_depends_on) {
        switch (static_cast<GVNFlag>(bit)) {
#define DECLARE_FLAG(type)                                       \
          case kDependsOn##type:                                 \
            offset += OS::SNPrintF(buffer + offset, separator);  \
            offset += OS::SNPrintF(buffer + offset, #type);      \
            separator = comma;                                   \
            break;
GVN_TRACKED_FLAG_LIST(DECLARE_FLAG)
GVN_UNTRACKED_FLAG_LIST(DECLARE_FLAG)
#undef DECLARE_FLAG
          default:
            break;
        }
      }
    }
    offset += OS::SNPrintF(buffer + offset, "]");
  }
#else
  OS::SNPrintF(buffer, "0x%08X", flags.ToIntegral());
#endif
  size_t string_len = strlen(underlying_buffer) + 1;
  ASSERT(string_len <= sizeof(underlying_buffer));
  char* result = new char[strlen(underlying_buffer) + 1];
  OS::MemCopy(result, underlying_buffer, string_len);
  return SmartArrayPointer<char>(result);
}


void HGlobalValueNumberingPhase::LoopInvariantCodeMotion() {
  TRACE_GVN_1("Using optimistic loop invariant code motion: %s\n",
              graph()->use_optimistic_licm() ? "yes" : "no");
  for (int i = graph()->blocks()->length() - 1; i >= 0; --i) {
    HBasicBlock* block = graph()->blocks()->at(i);
    if (block->IsLoopHeader()) {
      GVNFlagSet side_effects = loop_side_effects_[block->block_id()];
      TRACE_GVN_2("Try loop invariant motion for block B%d %s\n",
                  block->block_id(),
                  *GetGVNFlagsString(side_effects));

      GVNFlagSet accumulated_first_time_depends;
      GVNFlagSet accumulated_first_time_changes;
      HBasicBlock* last = block->loop_information()->GetLastBackEdge();
      for (int j = block->block_id(); j <= last->block_id(); ++j) {
        ProcessLoopBlock(graph()->blocks()->at(j), block, side_effects,
                         &accumulated_first_time_depends,
                         &accumulated_first_time_changes);
      }
    }
  }
}


void HGlobalValueNumberingPhase::ProcessLoopBlock(
    HBasicBlock* block,
    HBasicBlock* loop_header,
    GVNFlagSet loop_kills,
    GVNFlagSet* first_time_depends,
    GVNFlagSet* first_time_changes) {
  HBasicBlock* pre_header = loop_header->predecessors()->at(0);
  GVNFlagSet depends_flags = HValue::ConvertChangesToDependsFlags(loop_kills);
  TRACE_GVN_2("Loop invariant motion for B%d %s\n",
              block->block_id(),
              *GetGVNFlagsString(depends_flags));
  HInstruction* instr = block->first();
  while (instr != NULL) {
    HInstruction* next = instr->next();
    bool hoisted = false;
    if (instr->CheckFlag(HValue::kUseGVN)) {
      TRACE_GVN_4("Checking instruction %d (%s) %s. Loop %s\n",
                  instr->id(),
                  instr->Mnemonic(),
                  *GetGVNFlagsString(instr->gvn_flags()),
                  *GetGVNFlagsString(loop_kills));
      bool can_hoist = !instr->gvn_flags().ContainsAnyOf(depends_flags);
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
          TRACE_GVN_1("Hoisting loop invariant instruction %d\n", instr->id());
          // Move the instruction out of the loop.
          instr->Unlink();
          instr->InsertBefore(pre_header->end());
          if (instr->HasSideEffects()) removed_side_effects_ = true;
          hoisted = true;
        }
      }
    }
    if (!hoisted) {
      // If an instruction is not hoisted, we have to account for its side
      // effects when hoisting later HTransitionElementsKind instructions.
      GVNFlagSet previous_depends = *first_time_depends;
      GVNFlagSet previous_changes = *first_time_changes;
      first_time_depends->Add(instr->DependsOnFlags());
      first_time_changes->Add(instr->ChangesFlags());
      if (!(previous_depends == *first_time_depends)) {
        TRACE_GVN_1("Updated first-time accumulated %s\n",
                    *GetGVNFlagsString(*first_time_depends));
      }
      if (!(previous_changes == *first_time_changes)) {
        TRACE_GVN_1("Updated first-time accumulated %s\n",
                    *GetGVNFlagsString(*first_time_changes));
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
  return AllowCodeMotion() && !instr->block()->IsDeoptimizing();
}


GVNFlagSet
HGlobalValueNumberingPhase::CollectSideEffectsOnPathsToDominatedBlock(
    HBasicBlock* dominator, HBasicBlock* dominated) {
  GVNFlagSet side_effects;
  for (int i = 0; i < dominated->predecessors()->length(); ++i) {
    HBasicBlock* block = dominated->predecessors()->at(i);
    if (dominator->block_id() < block->block_id() &&
        block->block_id() < dominated->block_id() &&
        visited_on_paths_.Add(block->block_id())) {
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
                                         HValueMap* entry_map) {
    return new(zone)
        GvnBasicBlockState(NULL, entry_block, entry_map, NULL, zone);
  }

  HBasicBlock* block() { return block_; }
  HValueMap* map() { return map_; }
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
                  HValueMap* map,
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
                     HValueMap* map,
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
      return push(zone,
                  block_->dominated_blocks()->at(dominated_index_),
                  dominators());
    } else {
      return NULL;
    }
  }

  GvnBasicBlockState* push(Zone* zone,
                           HBasicBlock* block,
                           HSideEffectMap* dominators) {
    if (next_ == NULL) {
      next_ =
          new(zone) GvnBasicBlockState(this, block, map(), dominators, zone);
    } else {
      next_->Initialize(block, map(), dominators, true, zone);
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
  HValueMap* map_;
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
  HValueMap* entry_map = new(zone()) HValueMap(zone());
  GvnBasicBlockState* current =
      GvnBasicBlockState::CreateEntry(zone(), entry_block, entry_map);

  while (current != NULL) {
    HBasicBlock* block = current->block();
    HValueMap* map = current->map();
    HSideEffectMap* dominators = current->dominators();

    TRACE_GVN_2("Analyzing block B%d%s\n",
                block->block_id(),
                block->IsLoopHeader() ? " (loop header)" : "");

    // If this is a loop header kill everything killed by the loop.
    if (block->IsLoopHeader()) {
      map->Kill(loop_side_effects_[block->block_id()]);
    }

    // Go through all instructions of the current block.
    HInstruction* instr = block->first();
    while (instr != NULL) {
      HInstruction* next = instr->next();
      GVNFlagSet flags = instr->ChangesFlags();
      if (!flags.IsEmpty()) {
        // Clear all instructions in the map that are affected by side effects.
        // Store instruction as the dominating one for tracked side effects.
        map->Kill(flags);
        dominators->Store(flags, instr);
        TRACE_GVN_2("Instruction %d %s\n", instr->id(),
                    *GetGVNFlagsString(flags));
      }
      if (instr->CheckFlag(HValue::kUseGVN)) {
        ASSERT(!instr->HasObservableSideEffects());
        HValue* other = map->Lookup(instr);
        if (other != NULL) {
          ASSERT(instr->Equals(other) && other->Equals(instr));
          TRACE_GVN_4("Replacing value %d (%s) with value %d (%s)\n",
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
      if (instr->IsLinked() &&
          instr->CheckFlag(HValue::kTrackSideEffectDominators)) {
        for (int i = 0; i < kNumberOfTrackedSideEffects; i++) {
          HValue* other = dominators->at(i);
          GVNFlag changes_flag = HValue::ChangesFlagFromInt(i);
          GVNFlag depends_on_flag = HValue::DependsOnFlagFromInt(i);
          if (instr->DependsOnFlags().Contains(depends_on_flag) &&
              (other != NULL)) {
            TRACE_GVN_5("Side-effect #%d in %d (%s) is dominated by %d (%s)\n",
                        i,
                        instr->id(),
                        instr->Mnemonic(),
                        other->id(),
                        other->Mnemonic());
            instr->SetSideEffectDominator(changes_flag, other);
          }
        }
      }
      instr = next;
    }

    HBasicBlock* dominator_block;
    GvnBasicBlockState* next =
        current->next_in_dominator_tree_traversal(zone(),
                                                  &dominator_block);

    if (next != NULL) {
      HBasicBlock* dominated = next->block();
      HValueMap* successor_map = next->map();
      HSideEffectMap* successor_dominators = next->dominators();

      // Kill everything killed on any path between this block and the
      // dominated block.  We don't have to traverse these paths if the
      // value map and the dominators list is already empty.  If the range
      // of block ids (block_id, dominated_id) is empty there are no such
      // paths.
      if ((!successor_map->IsEmpty() || !successor_dominators->IsEmpty()) &&
          dominator_block->block_id() + 1 < dominated->block_id()) {
        visited_on_paths_.Clear();
        GVNFlagSet side_effects_on_all_paths =
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
