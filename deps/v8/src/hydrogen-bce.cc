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

#include "hydrogen-bce.h"

namespace v8 {
namespace internal {

// We try to "factor up" HBoundsCheck instructions towards the root of the
// dominator tree.
// For now we handle checks where the index is like "exp + int32value".
// If in the dominator tree we check "exp + v1" and later (dominated)
// "exp + v2", if v2 <= v1 we can safely remove the second check, and if
// v2 > v1 we can use v2 in the 1st check and again remove the second.
// To do so we keep a dictionary of all checks where the key if the pair
// "exp, length".
// The class BoundsCheckKey represents this key.
class BoundsCheckKey : public ZoneObject {
 public:
  HValue* IndexBase() const { return index_base_; }
  HValue* Length() const { return length_; }

  uint32_t Hash() {
    return static_cast<uint32_t>(index_base_->Hashcode() ^ length_->Hashcode());
  }

  static BoundsCheckKey* Create(Zone* zone,
                                HBoundsCheck* check,
                                int32_t* offset) {
    if (!check->index()->representation().IsSmiOrInteger32()) return NULL;

    HValue* index_base = NULL;
    HConstant* constant = NULL;
    bool is_sub = false;

    if (check->index()->IsAdd()) {
      HAdd* index = HAdd::cast(check->index());
      if (index->left()->IsConstant()) {
        constant = HConstant::cast(index->left());
        index_base = index->right();
      } else if (index->right()->IsConstant()) {
        constant = HConstant::cast(index->right());
        index_base = index->left();
      }
    } else if (check->index()->IsSub()) {
      HSub* index = HSub::cast(check->index());
      is_sub = true;
      if (index->left()->IsConstant()) {
        constant = HConstant::cast(index->left());
        index_base = index->right();
      } else if (index->right()->IsConstant()) {
        constant = HConstant::cast(index->right());
        index_base = index->left();
      }
    }

    if (constant != NULL && constant->HasInteger32Value()) {
      *offset = is_sub ? - constant->Integer32Value()
                       : constant->Integer32Value();
    } else {
      *offset = 0;
      index_base = check->index();
    }

    return new(zone) BoundsCheckKey(index_base, check->length());
  }

 private:
  BoundsCheckKey(HValue* index_base, HValue* length)
    : index_base_(index_base),
      length_(length) { }

  HValue* index_base_;
  HValue* length_;

  DISALLOW_COPY_AND_ASSIGN(BoundsCheckKey);
};


// Data about each HBoundsCheck that can be eliminated or moved.
// It is the "value" in the dictionary indexed by "base-index, length"
// (the key is BoundsCheckKey).
// We scan the code with a dominator tree traversal.
// Traversing the dominator tree we keep a stack (implemented as a singly
// linked list) of "data" for each basic block that contains a relevant check
// with the same key (the dictionary holds the head of the list).
// We also keep all the "data" created for a given basic block in a list, and
// use it to "clean up" the dictionary when backtracking in the dominator tree
// traversal.
// Doing this each dictionary entry always directly points to the check that
// is dominating the code being examined now.
// We also track the current "offset" of the index expression and use it to
// decide if any check is already "covered" (so it can be removed) or not.
class BoundsCheckBbData: public ZoneObject {
 public:
  BoundsCheckKey* Key() const { return key_; }
  int32_t LowerOffset() const { return lower_offset_; }
  int32_t UpperOffset() const { return upper_offset_; }
  HBasicBlock* BasicBlock() const { return basic_block_; }
  HBoundsCheck* LowerCheck() const { return lower_check_; }
  HBoundsCheck* UpperCheck() const { return upper_check_; }
  BoundsCheckBbData* NextInBasicBlock() const { return next_in_bb_; }
  BoundsCheckBbData* FatherInDominatorTree() const { return father_in_dt_; }

  bool OffsetIsCovered(int32_t offset) const {
    return offset >= LowerOffset() && offset <= UpperOffset();
  }

  bool HasSingleCheck() { return lower_check_ == upper_check_; }

  // The goal of this method is to modify either upper_offset_ or
  // lower_offset_ so that also new_offset is covered (the covered
  // range grows).
  //
  // The precondition is that new_check follows UpperCheck() and
  // LowerCheck() in the same basic block, and that new_offset is not
  // covered (otherwise we could simply remove new_check).
  //
  // If HasSingleCheck() is true then new_check is added as "second check"
  // (either upper or lower; note that HasSingleCheck() becomes false).
  // Otherwise one of the current checks is modified so that it also covers
  // new_offset, and new_check is removed.
  //
  // If the check cannot be modified because the context is unknown it
  // returns false, otherwise it returns true.
  bool CoverCheck(HBoundsCheck* new_check,
                  int32_t new_offset) {
    ASSERT(new_check->index()->representation().IsSmiOrInteger32());
    bool keep_new_check = false;

    if (new_offset > upper_offset_) {
      upper_offset_ = new_offset;
      if (HasSingleCheck()) {
        keep_new_check = true;
        upper_check_ = new_check;
      } else {
        bool result = BuildOffsetAdd(upper_check_,
                                     &added_upper_index_,
                                     &added_upper_offset_,
                                     Key()->IndexBase(),
                                     new_check->index()->representation(),
                                     new_offset);
        if (!result) return false;
        upper_check_->ReplaceAllUsesWith(upper_check_->index());
        upper_check_->SetOperandAt(0, added_upper_index_);
      }
    } else if (new_offset < lower_offset_) {
      lower_offset_ = new_offset;
      if (HasSingleCheck()) {
        keep_new_check = true;
        lower_check_ = new_check;
      } else {
        bool result = BuildOffsetAdd(lower_check_,
                                     &added_lower_index_,
                                     &added_lower_offset_,
                                     Key()->IndexBase(),
                                     new_check->index()->representation(),
                                     new_offset);
        if (!result) return false;
        lower_check_->ReplaceAllUsesWith(lower_check_->index());
        lower_check_->SetOperandAt(0, added_lower_index_);
      }
    } else {
      ASSERT(false);
    }

    if (!keep_new_check) {
      new_check->block()->graph()->isolate()->counters()->
          bounds_checks_eliminated()->Increment();
      new_check->DeleteAndReplaceWith(new_check->ActualValue());
    }

    return true;
  }

  void RemoveZeroOperations() {
    RemoveZeroAdd(&added_lower_index_, &added_lower_offset_);
    RemoveZeroAdd(&added_upper_index_, &added_upper_offset_);
  }

  BoundsCheckBbData(BoundsCheckKey* key,
                    int32_t lower_offset,
                    int32_t upper_offset,
                    HBasicBlock* bb,
                    HBoundsCheck* lower_check,
                    HBoundsCheck* upper_check,
                    BoundsCheckBbData* next_in_bb,
                    BoundsCheckBbData* father_in_dt)
  : key_(key),
    lower_offset_(lower_offset),
    upper_offset_(upper_offset),
    basic_block_(bb),
    lower_check_(lower_check),
    upper_check_(upper_check),
    added_lower_index_(NULL),
    added_lower_offset_(NULL),
    added_upper_index_(NULL),
    added_upper_offset_(NULL),
    next_in_bb_(next_in_bb),
    father_in_dt_(father_in_dt) { }

 private:
  BoundsCheckKey* key_;
  int32_t lower_offset_;
  int32_t upper_offset_;
  HBasicBlock* basic_block_;
  HBoundsCheck* lower_check_;
  HBoundsCheck* upper_check_;
  HInstruction* added_lower_index_;
  HConstant* added_lower_offset_;
  HInstruction* added_upper_index_;
  HConstant* added_upper_offset_;
  BoundsCheckBbData* next_in_bb_;
  BoundsCheckBbData* father_in_dt_;

  // Given an existing add instruction and a bounds check it tries to
  // find the current context (either of the add or of the check index).
  HValue* IndexContext(HInstruction* add, HBoundsCheck* check) {
    if (add != NULL && add->IsAdd()) {
      return HAdd::cast(add)->context();
    }
    if (check->index()->IsBinaryOperation()) {
      return HBinaryOperation::cast(check->index())->context();
    }
    return NULL;
  }

  // This function returns false if it cannot build the add because the
  // current context cannot be determined.
  bool BuildOffsetAdd(HBoundsCheck* check,
                      HInstruction** add,
                      HConstant** constant,
                      HValue* original_value,
                      Representation representation,
                      int32_t new_offset) {
    HValue* index_context = IndexContext(*add, check);
    if (index_context == NULL) return false;

    Zone* zone = BasicBlock()->zone();
    HConstant* new_constant = HConstant::New(zone, index_context,
                                             new_offset, representation);
    if (*add == NULL) {
      new_constant->InsertBefore(check);
      (*add) = HAdd::New(zone, index_context, original_value, new_constant);
      (*add)->AssumeRepresentation(representation);
      (*add)->InsertBefore(check);
    } else {
      new_constant->InsertBefore(*add);
      (*constant)->DeleteAndReplaceWith(new_constant);
    }
    *constant = new_constant;
    return true;
  }

  void RemoveZeroAdd(HInstruction** add, HConstant** constant) {
    if (*add != NULL && (*add)->IsAdd() && (*constant)->Integer32Value() == 0) {
      (*add)->DeleteAndReplaceWith(HAdd::cast(*add)->left());
      (*constant)->DeleteAndReplaceWith(NULL);
    }
  }

  DISALLOW_COPY_AND_ASSIGN(BoundsCheckBbData);
};


static bool BoundsCheckKeyMatch(void* key1, void* key2) {
  BoundsCheckKey* k1 = static_cast<BoundsCheckKey*>(key1);
  BoundsCheckKey* k2 = static_cast<BoundsCheckKey*>(key2);
  return k1->IndexBase() == k2->IndexBase() && k1->Length() == k2->Length();
}


BoundsCheckTable::BoundsCheckTable(Zone* zone)
    : ZoneHashMap(BoundsCheckKeyMatch, ZoneHashMap::kDefaultHashMapCapacity,
                  ZoneAllocationPolicy(zone)) { }


BoundsCheckBbData** BoundsCheckTable::LookupOrInsert(BoundsCheckKey* key,
                                                     Zone* zone) {
  return reinterpret_cast<BoundsCheckBbData**>(
      &(Lookup(key, key->Hash(), true, ZoneAllocationPolicy(zone))->value));
}


void BoundsCheckTable::Insert(BoundsCheckKey* key,
                              BoundsCheckBbData* data,
                              Zone* zone) {
  Lookup(key, key->Hash(), true, ZoneAllocationPolicy(zone))->value = data;
}


void BoundsCheckTable::Delete(BoundsCheckKey* key) {
  Remove(key, key->Hash());
}


class HBoundsCheckEliminationState {
 public:
  HBasicBlock* block_;
  BoundsCheckBbData* bb_data_list_;
  int index_;
};


// Eliminates checks in bb and recursively in the dominated blocks.
// Also replace the results of check instructions with the original value, if
// the result is used. This is safe now, since we don't do code motion after
// this point. It enables better register allocation since the value produced
// by check instructions is really a copy of the original value.
void HBoundsCheckEliminationPhase::EliminateRedundantBoundsChecks(
    HBasicBlock* entry) {
  // Allocate the stack.
  HBoundsCheckEliminationState* stack =
    zone()->NewArray<HBoundsCheckEliminationState>(graph()->blocks()->length());

  // Explicitly push the entry block.
  stack[0].block_ = entry;
  stack[0].bb_data_list_ = PreProcessBlock(entry);
  stack[0].index_ = 0;
  int stack_depth = 1;

  // Implement depth-first traversal with a stack.
  while (stack_depth > 0) {
    int current = stack_depth - 1;
    HBoundsCheckEliminationState* state = &stack[current];
    const ZoneList<HBasicBlock*>* children = state->block_->dominated_blocks();

    if (state->index_ < children->length()) {
      // Recursively visit children blocks.
      HBasicBlock* child = children->at(state->index_++);
      int next = stack_depth++;
      stack[next].block_ = child;
      stack[next].bb_data_list_ = PreProcessBlock(child);
      stack[next].index_ = 0;
    } else {
      // Finished with all children; post process the block.
      PostProcessBlock(state->block_, state->bb_data_list_);
      stack_depth--;
    }
  }
}


BoundsCheckBbData* HBoundsCheckEliminationPhase::PreProcessBlock(
    HBasicBlock* bb) {
  BoundsCheckBbData* bb_data_list = NULL;

  for (HInstructionIterator it(bb); !it.Done(); it.Advance()) {
    HInstruction* i = it.Current();
    if (!i->IsBoundsCheck()) continue;

    HBoundsCheck* check = HBoundsCheck::cast(i);
    int32_t offset;
    BoundsCheckKey* key =
        BoundsCheckKey::Create(zone(), check, &offset);
    if (key == NULL) continue;
    BoundsCheckBbData** data_p = table_.LookupOrInsert(key, zone());
    BoundsCheckBbData* data = *data_p;
    if (data == NULL) {
      bb_data_list = new(zone()) BoundsCheckBbData(key,
                                                   offset,
                                                   offset,
                                                   bb,
                                                   check,
                                                   check,
                                                   bb_data_list,
                                                   NULL);
      *data_p = bb_data_list;
    } else if (data->OffsetIsCovered(offset)) {
      bb->graph()->isolate()->counters()->
          bounds_checks_eliminated()->Increment();
      check->DeleteAndReplaceWith(check->ActualValue());
    } else if (data->BasicBlock() != bb ||
               !data->CoverCheck(check, offset)) {
      // If the check is in the current BB we try to modify it by calling
      // "CoverCheck", but if also that fails we record the current offsets
      // in a new data instance because from now on they are covered.
      int32_t new_lower_offset = offset < data->LowerOffset()
          ? offset
          : data->LowerOffset();
      int32_t new_upper_offset = offset > data->UpperOffset()
          ? offset
          : data->UpperOffset();
      bb_data_list = new(zone()) BoundsCheckBbData(key,
                                                   new_lower_offset,
                                                   new_upper_offset,
                                                   bb,
                                                   data->LowerCheck(),
                                                   data->UpperCheck(),
                                                   bb_data_list,
                                                   data);
      table_.Insert(key, bb_data_list, zone());
    }
  }

  return bb_data_list;
}


void HBoundsCheckEliminationPhase::PostProcessBlock(
    HBasicBlock* block, BoundsCheckBbData* data) {
  while (data != NULL) {
    data->RemoveZeroOperations();
    if (data->FatherInDominatorTree()) {
      table_.Insert(data->Key(), data->FatherInDominatorTree(), zone());
    } else {
      table_.Delete(data->Key());
    }
    data = data->NextInBasicBlock();
  }
}

} }  // namespace v8::internal
