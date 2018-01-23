// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/heap/heap-inl.h"
#include "src/macro-assembler.h"
#include "src/objects/shared-function-info.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

template <typename T>
using TNode = compiler::TNode<T>;

// -----------------------------------------------------------------------------
// Interrupt and stack checks.

void Builtins::Generate_InterruptCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kInterrupt);
}

void Builtins::Generate_StackCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kStackGuard);
}

// -----------------------------------------------------------------------------
// TurboFan support builtins.

TF_BUILTIN(CopyFastSmiOrObjectElements, CodeStubAssembler) {
  Node* object = Parameter(Descriptor::kObject);

  // Load the {object}s elements.
  Node* source = LoadObjectField(object, JSObject::kElementsOffset);
  Node* target = CloneFixedArray(source, ExtractFixedArrayFlag::kFixedArrays);
  StoreObjectField(object, JSObject::kElementsOffset, target);
  Return(target);
}

TF_BUILTIN(GrowFastDoubleElements, CodeStubAssembler) {
  Node* object = Parameter(Descriptor::kObject);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);

  Label runtime(this, Label::kDeferred);
  Node* elements = LoadElements(object);
  elements = TryGrowElementsCapacity(object, elements, PACKED_DOUBLE_ELEMENTS,
                                     key, &runtime);
  Return(elements);

  BIND(&runtime);
  TailCallRuntime(Runtime::kGrowArrayElements, context, object, key);
}

TF_BUILTIN(GrowFastSmiOrObjectElements, CodeStubAssembler) {
  Node* object = Parameter(Descriptor::kObject);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);

  Label runtime(this, Label::kDeferred);
  Node* elements = LoadElements(object);
  elements =
      TryGrowElementsCapacity(object, elements, PACKED_ELEMENTS, key, &runtime);
  Return(elements);

  BIND(&runtime);
  TailCallRuntime(Runtime::kGrowArrayElements, context, object, key);
}

TF_BUILTIN(NewArgumentsElements, CodeStubAssembler) {
  Node* frame = Parameter(Descriptor::kFrame);
  Node* length = SmiToWord(Parameter(Descriptor::kLength));
  Node* mapped_count = SmiToWord(Parameter(Descriptor::kMappedCount));

  // Check if we can allocate in new space.
  ElementsKind kind = PACKED_ELEMENTS;
  int max_elements = FixedArray::GetMaxLengthForNewSpaceAllocation(kind);
  Label if_newspace(this), if_oldspace(this, Label::kDeferred);
  Branch(IntPtrLessThan(length, IntPtrConstant(max_elements)), &if_newspace,
         &if_oldspace);

  BIND(&if_newspace);
  {
    // Prefer EmptyFixedArray in case of non-positive {length} (the {length}
    // can be negative here for rest parameters).
    Label if_empty(this), if_notempty(this);
    Branch(IntPtrLessThanOrEqual(length, IntPtrConstant(0)), &if_empty,
           &if_notempty);

    BIND(&if_empty);
    Return(EmptyFixedArrayConstant());

    BIND(&if_notempty);
    {
      // Allocate a FixedArray in new space.
      Node* result = AllocateFixedArray(kind, length);

      // The elements might be used to back mapped arguments. In that case fill
      // the mapped elements (i.e. the first {mapped_count}) with the hole, but
      // make sure not to overshoot the {length} if some arguments are missing.
      Node* number_of_holes =
          SelectConstant(IntPtrLessThan(mapped_count, length), mapped_count,
                         length, MachineType::PointerRepresentation());
      Node* the_hole = TheHoleConstant();

      // Fill the first elements up to {number_of_holes} with the hole.
      VARIABLE(var_index, MachineType::PointerRepresentation());
      Label loop1(this, &var_index), done_loop1(this);
      var_index.Bind(IntPtrConstant(0));
      Goto(&loop1);
      BIND(&loop1);
      {
        // Load the current {index}.
        Node* index = var_index.value();

        // Check if we are done.
        GotoIf(WordEqual(index, number_of_holes), &done_loop1);

        // Store the hole into the {result}.
        StoreFixedArrayElement(result, index, the_hole, SKIP_WRITE_BARRIER);

        // Continue with next {index}.
        var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
        Goto(&loop1);
      }
      BIND(&done_loop1);

      // Compute the effective {offset} into the {frame}.
      Node* offset = IntPtrAdd(length, IntPtrConstant(1));

      // Copy the parameters from {frame} (starting at {offset}) to {result}.
      Label loop2(this, &var_index), done_loop2(this);
      Goto(&loop2);
      BIND(&loop2);
      {
        // Load the current {index}.
        Node* index = var_index.value();

        // Check if we are done.
        GotoIf(WordEqual(index, length), &done_loop2);

        // Load the parameter at the given {index}.
        Node* value = Load(MachineType::AnyTagged(), frame,
                           TimesPointerSize(IntPtrSub(offset, index)));

        // Store the {value} into the {result}.
        StoreFixedArrayElement(result, index, value, SKIP_WRITE_BARRIER);

        // Continue with next {index}.
        var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
        Goto(&loop2);
      }
      BIND(&done_loop2);

      Return(result);
    }
  }

  BIND(&if_oldspace);
  {
    // Allocate in old space (or large object space).
    TailCallRuntime(Runtime::kNewArgumentsElements, NoContextConstant(),
                    BitcastWordToTagged(frame), SmiFromWord(length),
                    SmiFromWord(mapped_count));
  }
}

TF_BUILTIN(ReturnReceiver, CodeStubAssembler) {
  Return(Parameter(Descriptor::kReceiver));
}

class RecordWriteCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit RecordWriteCodeStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* IsMarking() {
    Node* is_marking_addr = ExternalConstant(
        ExternalReference::heap_is_marking_flag_address(this->isolate()));
    return Load(MachineType::Uint8(), is_marking_addr);
  }

  Node* IsPageFlagSet(Node* object, int mask) {
    Node* page = WordAnd(object, IntPtrConstant(~Page::kPageAlignmentMask));
    Node* flags = Load(MachineType::Pointer(), page,
                       IntPtrConstant(MemoryChunk::kFlagsOffset));
    return WordNotEqual(WordAnd(flags, IntPtrConstant(mask)),
                        IntPtrConstant(0));
  }

  void GotoIfNotBlack(Node* object, Label* not_black) {
    Label exit(this);
    Label* black = &exit;

    DCHECK_EQ(strcmp(Marking::kBlackBitPattern, "11"), 0);

    Node* cell;
    Node* mask;

    GetMarkBit(object, &cell, &mask);
    mask = TruncateWordToWord32(mask);

    Node* bits = Load(MachineType::Int32(), cell);
    Node* bit_0 = Word32And(bits, mask);

    GotoIf(Word32Equal(bit_0, Int32Constant(0)), not_black);

    mask = Word32Shl(mask, Int32Constant(1));

    Label word_boundary(this), in_word(this);

    // If mask becomes zero, we know mask was `1 << 31`, i.e., the bit is on
    // word boundary. Otherwise, the bit is within the word.
    Branch(Word32Equal(mask, Int32Constant(0)), &word_boundary, &in_word);

    BIND(&word_boundary);
    {
      Node* bit_1 = Word32And(
          Load(MachineType::Int32(), IntPtrAdd(cell, IntPtrConstant(4))),
          Int32Constant(1));
      Branch(Word32Equal(bit_1, Int32Constant(0)), not_black, black);
    }

    BIND(&in_word);
    {
      Branch(Word32Equal(Word32And(bits, mask), Int32Constant(0)), not_black,
             black);
    }

    BIND(&exit);
  }

  Node* IsWhite(Node* object) {
    DCHECK_EQ(strcmp(Marking::kWhiteBitPattern, "00"), 0);
    Node* cell;
    Node* mask;
    GetMarkBit(object, &cell, &mask);
    mask = TruncateWordToWord32(mask);
    // Non-white has 1 for the first bit, so we only need to check for the first
    // bit.
    return Word32Equal(Word32And(Load(MachineType::Int32(), cell), mask),
                       Int32Constant(0));
  }

  void GetMarkBit(Node* object, Node** cell, Node** mask) {
    Node* page = WordAnd(object, IntPtrConstant(~Page::kPageAlignmentMask));

    {
      // Temp variable to calculate cell offset in bitmap.
      Node* r0;
      int shift = Bitmap::kBitsPerCellLog2 + kPointerSizeLog2 -
                  Bitmap::kBytesPerCellLog2;
      r0 = WordShr(object, IntPtrConstant(shift));
      r0 = WordAnd(r0, IntPtrConstant((Page::kPageAlignmentMask >> shift) &
                                      ~(Bitmap::kBytesPerCell - 1)));
      *cell = IntPtrAdd(IntPtrAdd(page, r0),
                        IntPtrConstant(MemoryChunk::kHeaderSize));
    }
    {
      // Temp variable to calculate bit offset in cell.
      Node* r1;
      r1 = WordShr(object, IntPtrConstant(kPointerSizeLog2));
      r1 = WordAnd(r1, IntPtrConstant((1 << Bitmap::kBitsPerCellLog2) - 1));
      // It seems that LSB(e.g. cl) is automatically used, so no manual masking
      // is needed. Uncomment the following line otherwise.
      // WordAnd(r1, IntPtrConstant((1 << kBitsPerByte) - 1)));
      *mask = WordShl(IntPtrConstant(1), r1);
    }
  }

  Node* ShouldSkipFPRegs(Node* mode) {
    return WordEqual(mode, SmiConstant(kDontSaveFPRegs));
  }

  Node* ShouldEmitRememberSet(Node* remembered_set) {
    return WordEqual(remembered_set, SmiConstant(EMIT_REMEMBERED_SET));
  }

  void CallCFunction1WithCallerSavedRegistersMode(MachineType return_type,
                                                  MachineType arg0_type,
                                                  Node* function, Node* arg0,
                                                  Node* mode, Label* next) {
    Label dont_save_fp(this), save_fp(this);
    Branch(ShouldSkipFPRegs(mode), &dont_save_fp, &save_fp);
    BIND(&dont_save_fp);
    {
      CallCFunction1WithCallerSavedRegisters(return_type, arg0_type, function,
                                             arg0, kDontSaveFPRegs);
      Goto(next);
    }

    BIND(&save_fp);
    {
      CallCFunction1WithCallerSavedRegisters(return_type, arg0_type, function,
                                             arg0, kSaveFPRegs);
      Goto(next);
    }
  }

  void CallCFunction3WithCallerSavedRegistersMode(
      MachineType return_type, MachineType arg0_type, MachineType arg1_type,
      MachineType arg2_type, Node* function, Node* arg0, Node* arg1, Node* arg2,
      Node* mode, Label* next) {
    Label dont_save_fp(this), save_fp(this);
    Branch(ShouldSkipFPRegs(mode), &dont_save_fp, &save_fp);
    BIND(&dont_save_fp);
    {
      CallCFunction3WithCallerSavedRegisters(return_type, arg0_type, arg1_type,
                                             arg2_type, function, arg0, arg1,
                                             arg2, kDontSaveFPRegs);
      Goto(next);
    }

    BIND(&save_fp);
    {
      CallCFunction3WithCallerSavedRegisters(return_type, arg0_type, arg1_type,
                                             arg2_type, function, arg0, arg1,
                                             arg2, kSaveFPRegs);
      Goto(next);
    }
  }

  void InsertToStoreBufferAndGoto(Node* isolate, Node* slot, Node* mode,
                                  Label* next) {
    Node* store_buffer_top_addr =
        ExternalConstant(ExternalReference::store_buffer_top(this->isolate()));
    Node* store_buffer_top =
        Load(MachineType::Pointer(), store_buffer_top_addr);
    StoreNoWriteBarrier(MachineType::PointerRepresentation(), store_buffer_top,
                        slot);
    Node* new_store_buffer_top =
        IntPtrAdd(store_buffer_top, IntPtrConstant(kPointerSize));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(),
                        store_buffer_top_addr, new_store_buffer_top);

    Node* test = WordAnd(new_store_buffer_top,
                         IntPtrConstant(StoreBuffer::kStoreBufferMask));

    Label overflow(this);
    Branch(WordEqual(test, IntPtrConstant(0)), &overflow, next);

    BIND(&overflow);
    {
      Node* function = ExternalConstant(
          ExternalReference::store_buffer_overflow_function(this->isolate()));
      CallCFunction1WithCallerSavedRegistersMode(MachineType::Int32(),
                                                 MachineType::Pointer(),
                                                 function, isolate, mode, next);
    }
  }
};

TF_BUILTIN(RecordWrite, RecordWriteCodeStubAssembler) {
  Node* object = BitcastTaggedToWord(Parameter(Descriptor::kObject));
  Node* slot = Parameter(Descriptor::kSlot);
  Node* isolate = Parameter(Descriptor::kIsolate);
  Node* remembered_set = Parameter(Descriptor::kRememberedSet);
  Node* fp_mode = Parameter(Descriptor::kFPMode);

  Node* value = Load(MachineType::Pointer(), slot);

  Label generational_wb(this);
  Label incremental_wb(this);
  Label exit(this);

  Branch(ShouldEmitRememberSet(remembered_set), &generational_wb,
         &incremental_wb);

  BIND(&generational_wb);
  {
    Label test_old_to_new_flags(this);
    Label store_buffer_exit(this), store_buffer_incremental_wb(this);
    // When incremental marking is not on, we skip cross generation pointer
    // checking here, because there are checks for
    // `kPointersFromHereAreInterestingMask` and
    // `kPointersToHereAreInterestingMask` in
    // `src/compiler/<arch>/code-generator-<arch>.cc` before calling this stub,
    // which serves as the cross generation checking.
    Branch(IsMarking(), &test_old_to_new_flags, &store_buffer_exit);

    BIND(&test_old_to_new_flags);
    {
      // TODO(albertnetymk): Try to cache the page flag for value and object,
      // instead of calling IsPageFlagSet each time.
      Node* value_in_new_space =
          IsPageFlagSet(value, MemoryChunk::kIsInNewSpaceMask);
      GotoIfNot(value_in_new_space, &incremental_wb);

      Node* object_in_new_space =
          IsPageFlagSet(object, MemoryChunk::kIsInNewSpaceMask);
      GotoIf(object_in_new_space, &incremental_wb);

      Goto(&store_buffer_incremental_wb);
    }

    BIND(&store_buffer_exit);
    { InsertToStoreBufferAndGoto(isolate, slot, fp_mode, &exit); }

    BIND(&store_buffer_incremental_wb);
    { InsertToStoreBufferAndGoto(isolate, slot, fp_mode, &incremental_wb); }
  }

  BIND(&incremental_wb);
  {
    Label call_incremental_wb(this);

#ifndef V8_CONCURRENT_MARKING
    GotoIfNotBlack(object, &exit);
#endif

    // There are two cases we need to call incremental write barrier.
    // 1) value_is_white
    GotoIf(IsWhite(value), &call_incremental_wb);

    // 2) is_compacting && value_in_EC && obj_isnt_skip
    // is_compacting = true when is_marking = true
    GotoIfNot(IsPageFlagSet(value, MemoryChunk::kEvacuationCandidateMask),
              &exit);
    GotoIf(
        IsPageFlagSet(object, MemoryChunk::kSkipEvacuationSlotsRecordingMask),
        &exit);

    Goto(&call_incremental_wb);

    BIND(&call_incremental_wb);
    {
      Node* function = ExternalConstant(
          ExternalReference::incremental_marking_record_write_function(
              this->isolate()));
      CallCFunction3WithCallerSavedRegistersMode(
          MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
          MachineType::Pointer(), function, object, slot, isolate, fp_mode,
          &exit);
    }
  }

  BIND(&exit);
  Return(TrueConstant());
}

class DeletePropertyBaseAssembler : public CodeStubAssembler {
 public:
  explicit DeletePropertyBaseAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void DeleteDictionaryProperty(Node* receiver, Node* properties, Node* name,
                                Node* context, Label* dont_delete,
                                Label* notfound) {
    VARIABLE(var_name_index, MachineType::PointerRepresentation());
    Label dictionary_found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, name, &dictionary_found,
                                         &var_name_index, notfound);

    BIND(&dictionary_found);
    Node* key_index = var_name_index.value();
    Node* details =
        LoadDetailsByKeyIndex<NameDictionary>(properties, key_index);
    GotoIf(IsSetWord32(details, PropertyDetails::kAttributesDontDeleteMask),
           dont_delete);
    // Overwrite the entry itself (see NameDictionary::SetEntry).
    Node* filler = TheHoleConstant();
    DCHECK(Heap::RootIsImmortalImmovable(Heap::kTheHoleValueRootIndex));
    StoreFixedArrayElement(properties, key_index, filler, SKIP_WRITE_BARRIER);
    StoreValueByKeyIndex<NameDictionary>(properties, key_index, filler,
                                         SKIP_WRITE_BARRIER);
    StoreDetailsByKeyIndex<NameDictionary>(properties, key_index,
                                           SmiConstant(0));

    // Update bookkeeping information (see NameDictionary::ElementRemoved).
    Node* nof = GetNumberOfElements<NameDictionary>(properties);
    Node* new_nof = SmiSub(nof, SmiConstant(1));
    SetNumberOfElements<NameDictionary>(properties, new_nof);
    Node* num_deleted = GetNumberOfDeletedElements<NameDictionary>(properties);
    Node* new_deleted = SmiAdd(num_deleted, SmiConstant(1));
    SetNumberOfDeletedElements<NameDictionary>(properties, new_deleted);

    // Shrink the dictionary if necessary (see NameDictionary::Shrink).
    Label shrinking_done(this);
    Node* capacity = GetCapacity<NameDictionary>(properties);
    GotoIf(SmiGreaterThan(new_nof, SmiShr(capacity, 2)), &shrinking_done);
    GotoIf(SmiLessThan(new_nof, SmiConstant(16)), &shrinking_done);
    CallRuntime(Runtime::kShrinkPropertyDictionary, context, receiver);
    Goto(&shrinking_done);
    BIND(&shrinking_done);

    Return(TrueConstant());
  }
};

TF_BUILTIN(DeleteProperty, DeletePropertyBaseAssembler) {
  Node* receiver = Parameter(Descriptor::kObject);
  Node* key = Parameter(Descriptor::kKey);
  Node* language_mode = Parameter(Descriptor::kLanguageMode);
  Node* context = Parameter(Descriptor::kContext);

  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged, key);
  Label if_index(this), if_unique_name(this), if_notunique(this),
      if_notfound(this), slow(this);

  GotoIf(TaggedIsSmi(receiver), &slow);
  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
         &slow);
  TryToName(key, &if_index, &var_index, &if_unique_name, &var_unique, &slow,
            &if_notunique);

  BIND(&if_index);
  {
    Comment("integer index");
    Goto(&slow);  // TODO(jkummerow): Implement more smarts here.
  }

  BIND(&if_unique_name);
  {
    Comment("key is unique name");
    Node* unique = var_unique.value();
    CheckForAssociatedProtector(unique, &slow);

    Label dictionary(this), dont_delete(this);
    GotoIf(IsDictionaryMap(receiver_map), &dictionary);

    // Fast properties need to clear recorded slots, which can only be done
    // in C++.
    Goto(&slow);

    BIND(&dictionary);
    {
      Node* properties = LoadSlowProperties(receiver);
      DeleteDictionaryProperty(receiver, properties, unique, context,
                               &dont_delete, &if_notfound);
    }

    BIND(&dont_delete);
    {
      STATIC_ASSERT(LanguageModeSize == 2);
      GotoIf(SmiNotEqual(language_mode, SmiConstant(LanguageMode::kSloppy)),
             &slow);
      Return(FalseConstant());
    }
  }

  BIND(&if_notunique);
  {
    // If the string was not found in the string table, then no object can
    // have a property with that name.
    TryInternalizeString(key, &if_index, &var_index, &if_unique_name,
                         &var_unique, &if_notfound, &slow);
  }

  BIND(&if_notfound);
  Return(TrueConstant());

  BIND(&slow);
  {
    TailCallRuntime(Runtime::kDeleteProperty, context, receiver, key,
                    language_mode);
  }
}

TF_BUILTIN(ForInEnumerate, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* context = Parameter(Descriptor::kContext);

  Label if_empty(this), if_runtime(this, Label::kDeferred);
  Node* receiver_map = CheckEnumCache(receiver, &if_empty, &if_runtime);
  Return(receiver_map);

  BIND(&if_empty);
  Return(EmptyFixedArrayConstant());

  BIND(&if_runtime);
  TailCallRuntime(Runtime::kForInEnumerate, context, receiver);
}

TF_BUILTIN(ForInFilter, CodeStubAssembler) {
  Node* key = Parameter(Descriptor::kKey);
  Node* object = Parameter(Descriptor::kObject);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsString(key));

  Label if_true(this), if_false(this);
  TNode<Oddball> result = HasProperty(object, key, context, kForInHasProperty);
  Branch(IsTrue(result), &if_true, &if_false);

  BIND(&if_true);
  Return(key);

  BIND(&if_false);
  Return(UndefinedConstant());
}

TF_BUILTIN(SameValue, CodeStubAssembler) {
  Node* lhs = Parameter(Descriptor::kLeft);
  Node* rhs = Parameter(Descriptor::kRight);

  Label if_true(this), if_false(this);
  BranchIfSameValue(lhs, rhs, &if_true, &if_false);

  BIND(&if_true);
  Return(TrueConstant());

  BIND(&if_false);
  Return(FalseConstant());
}

class InternalBuiltinsAssembler : public CodeStubAssembler {
 public:
  explicit InternalBuiltinsAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<IntPtrT> GetPendingMicrotaskCount();
  void SetPendingMicrotaskCount(TNode<IntPtrT> count);

  TNode<FixedArray> GetMicrotaskQueue();
  void SetMicrotaskQueue(TNode<FixedArray> queue);

  TNode<Context> GetCurrentContext();
  void SetCurrentContext(TNode<Context> context);

  void EnterMicrotaskContext(TNode<Context> context);
  void LeaveMicrotaskContext();

  TNode<Object> GetPendingException() {
    auto ref = ExternalReference(kPendingExceptionAddress, isolate());
    return TNode<Object>::UncheckedCast(
        Load(MachineType::AnyTagged(), ExternalConstant(ref)));
  }
  void ClearPendingException() {
    auto ref = ExternalReference(kPendingExceptionAddress, isolate());
    StoreNoWriteBarrier(MachineRepresentation::kTagged, ExternalConstant(ref),
                        TheHoleConstant());
  }

  TNode<Object> GetScheduledException() {
    auto ref = ExternalReference::scheduled_exception_address(isolate());
    return TNode<Object>::UncheckedCast(
        Load(MachineType::AnyTagged(), ExternalConstant(ref)));
  }
  void ClearScheduledException() {
    auto ref = ExternalReference::scheduled_exception_address(isolate());
    StoreNoWriteBarrier(MachineRepresentation::kTagged, ExternalConstant(ref),
                        TheHoleConstant());
  }
};

TNode<IntPtrT> InternalBuiltinsAssembler::GetPendingMicrotaskCount() {
  auto ref = ExternalReference::pending_microtask_count_address(isolate());
  if (kIntSize == 8) {
    return TNode<IntPtrT>::UncheckedCast(
        Load(MachineType::Int64(), ExternalConstant(ref)));
  } else {
    Node* const value = Load(MachineType::Int32(), ExternalConstant(ref));
    return ChangeInt32ToIntPtr(value);
  }
}

void InternalBuiltinsAssembler::SetPendingMicrotaskCount(TNode<IntPtrT> count) {
  auto ref = ExternalReference::pending_microtask_count_address(isolate());
  auto rep = kIntSize == 8 ? MachineRepresentation::kWord64
                           : MachineRepresentation::kWord32;
  if (kIntSize == 4 && kPointerSize == 8) {
    Node* const truncated_count =
        TruncateInt64ToInt32(TNode<Int64T>::UncheckedCast(count));
    StoreNoWriteBarrier(rep, ExternalConstant(ref), truncated_count);
  } else {
    StoreNoWriteBarrier(rep, ExternalConstant(ref), count);
  }
}

TNode<FixedArray> InternalBuiltinsAssembler::GetMicrotaskQueue() {
  return TNode<FixedArray>::UncheckedCast(
      LoadRoot(Heap::kMicrotaskQueueRootIndex));
}

void InternalBuiltinsAssembler::SetMicrotaskQueue(TNode<FixedArray> queue) {
  StoreRoot(Heap::kMicrotaskQueueRootIndex, queue);
}

TNode<Context> InternalBuiltinsAssembler::GetCurrentContext() {
  auto ref = ExternalReference(kContextAddress, isolate());
  return TNode<Context>::UncheckedCast(
      Load(MachineType::AnyTagged(), ExternalConstant(ref)));
}

void InternalBuiltinsAssembler::SetCurrentContext(TNode<Context> context) {
  auto ref = ExternalReference(kContextAddress, isolate());
  StoreNoWriteBarrier(MachineRepresentation::kTagged, ExternalConstant(ref),
                      context);
}

void InternalBuiltinsAssembler::EnterMicrotaskContext(
    TNode<Context> microtask_context) {
  auto ref = ExternalReference::handle_scope_implementer_address(isolate());
  Node* const hsi = Load(MachineType::Pointer(), ExternalConstant(ref));
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), hsi,
      IntPtrConstant(HandleScopeImplementerOffsets::kMicrotaskContext),
      BitcastTaggedToWord(microtask_context));

  // Load mirrored std::vector length from
  // HandleScopeImplementer::entered_contexts_count_
  auto type = kSizetSize == 8 ? MachineType::Uint64() : MachineType::Uint32();
  Node* entered_contexts_length = Load(
      type, hsi,
      IntPtrConstant(HandleScopeImplementerOffsets::kEnteredContextsCount));

  auto rep = kSizetSize == 8 ? MachineRepresentation::kWord64
                             : MachineRepresentation::kWord32;

  StoreNoWriteBarrier(
      rep, hsi,
      IntPtrConstant(
          HandleScopeImplementerOffsets::kEnteredContextCountDuringMicrotasks),
      entered_contexts_length);
}

void InternalBuiltinsAssembler::LeaveMicrotaskContext() {
  auto ref = ExternalReference::handle_scope_implementer_address(isolate());

  Node* const hsi = Load(MachineType::Pointer(), ExternalConstant(ref));
  StoreNoWriteBarrier(
      MachineType::PointerRepresentation(), hsi,
      IntPtrConstant(HandleScopeImplementerOffsets::kMicrotaskContext),
      IntPtrConstant(0));
  if (kSizetSize == 4) {
    StoreNoWriteBarrier(
        MachineRepresentation::kWord32, hsi,
        IntPtrConstant(HandleScopeImplementerOffsets::
                           kEnteredContextCountDuringMicrotasks),
        Int32Constant(0));
  } else {
    StoreNoWriteBarrier(
        MachineRepresentation::kWord64, hsi,
        IntPtrConstant(HandleScopeImplementerOffsets::
                           kEnteredContextCountDuringMicrotasks),
        Int64Constant(0));
  }
}

TF_BUILTIN(EnqueueMicrotask, InternalBuiltinsAssembler) {
  Node* microtask = Parameter(Descriptor::kMicrotask);

  TNode<IntPtrT> num_tasks = GetPendingMicrotaskCount();
  TNode<IntPtrT> new_num_tasks = IntPtrAdd(num_tasks, IntPtrConstant(1));
  TNode<FixedArray> queue = GetMicrotaskQueue();
  TNode<IntPtrT> queue_length = LoadAndUntagFixedArrayBaseLength(queue);

  Label if_append(this), if_grow(this), done(this);
  Branch(WordEqual(num_tasks, queue_length), &if_grow, &if_append);

  BIND(&if_grow);
  {
    // Determine the new queue length and check if we need to allocate
    // in large object space (instead of just going to new space, where
    // we also know that we don't need any write barriers for setting
    // up the new queue object).
    Label if_newspace(this), if_lospace(this, Label::kDeferred);
    TNode<IntPtrT> new_queue_length =
        IntPtrMax(IntPtrConstant(8), IntPtrAdd(num_tasks, num_tasks));
    Branch(IntPtrLessThanOrEqual(new_queue_length,
                                 IntPtrConstant(FixedArray::kMaxRegularLength)),
           &if_newspace, &if_lospace);

    BIND(&if_newspace);
    {
      // This is the likely case where the new queue fits into new space,
      // and thus we don't need any write barriers for initializing it.
      TNode<FixedArray> new_queue =
          CAST(AllocateFixedArray(PACKED_ELEMENTS, new_queue_length));
      CopyFixedArrayElements(PACKED_ELEMENTS, queue, new_queue, num_tasks,
                             SKIP_WRITE_BARRIER);
      StoreFixedArrayElement(new_queue, num_tasks, microtask,
                             SKIP_WRITE_BARRIER);
      FillFixedArrayWithValue(PACKED_ELEMENTS, new_queue, new_num_tasks,
                              new_queue_length, Heap::kUndefinedValueRootIndex);
      SetMicrotaskQueue(new_queue);
      Goto(&done);
    }

    BIND(&if_lospace);
    {
      // The fallback case where the new queue ends up in large object space.
      TNode<FixedArray> new_queue = CAST(AllocateFixedArray(
          PACKED_ELEMENTS, new_queue_length, INTPTR_PARAMETERS,
          AllocationFlag::kAllowLargeObjectAllocation));
      CopyFixedArrayElements(PACKED_ELEMENTS, queue, new_queue, num_tasks);
      StoreFixedArrayElement(new_queue, num_tasks, microtask);
      FillFixedArrayWithValue(PACKED_ELEMENTS, new_queue, new_num_tasks,
                              new_queue_length, Heap::kUndefinedValueRootIndex);
      SetMicrotaskQueue(new_queue);
      Goto(&done);
    }
  }

  BIND(&if_append);
  {
    StoreFixedArrayElement(queue, num_tasks, microtask);
    Goto(&done);
  }

  BIND(&done);
  SetPendingMicrotaskCount(new_num_tasks);
  Return(UndefinedConstant());
}

TF_BUILTIN(RunMicrotasks, InternalBuiltinsAssembler) {
  Label init_queue_loop(this);

  Goto(&init_queue_loop);
  BIND(&init_queue_loop);
  {
    TVARIABLE(IntPtrT, index, IntPtrConstant(0));
    Label loop(this, &index);

    TNode<IntPtrT> num_tasks = GetPendingMicrotaskCount();
    ReturnIf(IntPtrEqual(num_tasks, IntPtrConstant(0)), UndefinedConstant());

    TNode<FixedArray> queue = GetMicrotaskQueue();

    CSA_ASSERT(this, IntPtrGreaterThanOrEqual(
                         LoadAndUntagFixedArrayBaseLength(queue), num_tasks));
    CSA_ASSERT(this, IntPtrGreaterThan(num_tasks, IntPtrConstant(0)));

    SetPendingMicrotaskCount(IntPtrConstant(0));
    SetMicrotaskQueue(
        TNode<FixedArray>::UncheckedCast(EmptyFixedArrayConstant()));

    Goto(&loop);
    BIND(&loop);
    {
      TNode<HeapObject> microtask =
          TNode<HeapObject>::UncheckedCast(LoadFixedArrayElement(queue, index));
      index = IntPtrAdd(index, IntPtrConstant(1));

      CSA_ASSERT(this, TaggedIsNotSmi(microtask));

      TNode<Map> microtask_map = LoadMap(microtask);
      TNode<Int32T> microtask_type = LoadMapInstanceType(microtask_map);

      Label is_call_handler_info(this);
      Label is_function(this);
      Label is_promise_resolve_thenable_job(this);
      Label is_promise_reaction_job(this);
      Label is_unreachable(this);

      int32_t case_values[] = {TUPLE3_TYPE,  // CallHandlerInfo
                               JS_FUNCTION_TYPE,
                               PROMISE_RESOLVE_THENABLE_JOB_INFO_TYPE,
                               PROMISE_REACTION_JOB_INFO_TYPE};

      Label* case_labels[] = {&is_call_handler_info, &is_function,
                              &is_promise_resolve_thenable_job,
                              &is_promise_reaction_job};

      static_assert(arraysize(case_values) == arraysize(case_labels), "");
      Switch(microtask_type, &is_unreachable, case_values, case_labels,
             arraysize(case_labels));

      BIND(&is_call_handler_info);
      {
        // Bailout to C++ slow path for the remainder of the loop.
        auto index_ref =
            ExternalReference(kMicrotaskQueueBailoutIndexAddress, isolate());
        auto count_ref =
            ExternalReference(kMicrotaskQueueBailoutCountAddress, isolate());
        auto rep = kIntSize == 4 ? MachineRepresentation::kWord32
                                 : MachineRepresentation::kWord64;

        // index was pre-incremented, decrement for bailout to C++.
        Node* value = IntPtrSub(index, IntPtrConstant(1));

        if (kPointerSize == 4) {
          DCHECK_EQ(kIntSize, 4);
          StoreNoWriteBarrier(rep, ExternalConstant(index_ref), value);
          StoreNoWriteBarrier(rep, ExternalConstant(count_ref), num_tasks);
        } else {
          Node* count = num_tasks;
          if (kIntSize == 4) {
            value = TruncateInt64ToInt32(value);
            count = TruncateInt64ToInt32(count);
          }
          StoreNoWriteBarrier(rep, ExternalConstant(index_ref), value);
          StoreNoWriteBarrier(rep, ExternalConstant(count_ref), count);
        }

        Return(queue);
      }

      BIND(&is_function);
      {
        Label cont(this);
        VARIABLE(exception, MachineRepresentation::kTagged, TheHoleConstant());
        TNode<Context> old_context = GetCurrentContext();
        TNode<Context> fn_context = TNode<Context>::UncheckedCast(
            LoadObjectField(microtask, JSFunction::kContextOffset));
        TNode<Context> native_context =
            TNode<Context>::UncheckedCast(LoadNativeContext(fn_context));
        SetCurrentContext(native_context);
        EnterMicrotaskContext(fn_context);
        Node* const call = CallJS(CodeFactory::Call(isolate()), native_context,
                                  microtask, UndefinedConstant());
        GotoIfException(call, &cont);
        Goto(&cont);
        BIND(&cont);
        LeaveMicrotaskContext();
        SetCurrentContext(old_context);
        Branch(IntPtrLessThan(index, num_tasks), &loop, &init_queue_loop);
      }

      BIND(&is_promise_resolve_thenable_job);
      {
        VARIABLE(exception, MachineRepresentation::kTagged, TheHoleConstant());
        TNode<Context> old_context = GetCurrentContext();
        TNode<Context> microtask_context =
            TNode<Context>::UncheckedCast(LoadObjectField(
                microtask, PromiseResolveThenableJobInfo::kContextOffset));
        TNode<Context> native_context =
            TNode<Context>::UncheckedCast(LoadNativeContext(microtask_context));
        SetCurrentContext(native_context);
        EnterMicrotaskContext(microtask_context);

        Label if_unhandled_exception(this), done(this);
        Node* const ret = CallBuiltin(Builtins::kPromiseResolveThenableJob,
                                      native_context, microtask);
        GotoIfException(ret, &if_unhandled_exception, &exception);
        Goto(&done);

        BIND(&if_unhandled_exception);
        CallRuntime(Runtime::kReportMessage, native_context, exception.value());
        Goto(&done);

        BIND(&done);
        LeaveMicrotaskContext();
        SetCurrentContext(old_context);

        Branch(IntPtrLessThan(index, num_tasks), &loop, &init_queue_loop);
      }

      BIND(&is_promise_reaction_job);
      {
        Label if_multiple(this);
        Label if_single(this);

        Node* const value =
            LoadObjectField(microtask, PromiseReactionJobInfo::kValueOffset);
        Node* const tasks =
            LoadObjectField(microtask, PromiseReactionJobInfo::kTasksOffset);
        Node* const deferred_promises = LoadObjectField(
            microtask, PromiseReactionJobInfo::kDeferredPromiseOffset);
        Node* const deferred_on_resolves = LoadObjectField(
            microtask, PromiseReactionJobInfo::kDeferredOnResolveOffset);
        Node* const deferred_on_rejects = LoadObjectField(
            microtask, PromiseReactionJobInfo::kDeferredOnRejectOffset);

        TNode<Context> old_context = GetCurrentContext();
        TNode<Context> microtask_context = TNode<Context>::UncheckedCast(
            LoadObjectField(microtask, PromiseReactionJobInfo::kContextOffset));
        TNode<Context> native_context =
            TNode<Context>::UncheckedCast(LoadNativeContext(microtask_context));
        SetCurrentContext(native_context);
        EnterMicrotaskContext(microtask_context);

        Branch(IsFixedArray(deferred_promises), &if_multiple, &if_single);

        BIND(&if_single);
        {
          CallBuiltin(Builtins::kPromiseHandle, native_context, value, tasks,
                      deferred_promises, deferred_on_resolves,
                      deferred_on_rejects);
          LeaveMicrotaskContext();
          SetCurrentContext(old_context);
          Branch(IntPtrLessThan(index, num_tasks), &loop, &init_queue_loop);
        }

        BIND(&if_multiple);
        {
          TVARIABLE(IntPtrT, inner_index, IntPtrConstant(0));
          TNode<IntPtrT> inner_length =
              LoadAndUntagFixedArrayBaseLength(deferred_promises);
          Label inner_loop(this, &inner_index), done(this);

          CSA_ASSERT(this, IntPtrGreaterThan(inner_length, IntPtrConstant(0)));
          Goto(&inner_loop);
          BIND(&inner_loop);
          {
            Node* const task = LoadFixedArrayElement(tasks, inner_index);
            Node* const deferred_promise =
                LoadFixedArrayElement(deferred_promises, inner_index);
            Node* const deferred_on_resolve =
                LoadFixedArrayElement(deferred_on_resolves, inner_index);
            Node* const deferred_on_reject =
                LoadFixedArrayElement(deferred_on_rejects, inner_index);
            CallBuiltin(Builtins::kPromiseHandle, native_context, value, task,
                        deferred_promise, deferred_on_resolve,
                        deferred_on_reject);
            inner_index = IntPtrAdd(inner_index, IntPtrConstant(1));
            Branch(IntPtrLessThan(inner_index, inner_length), &inner_loop,
                   &done);
          }
          BIND(&done);

          LeaveMicrotaskContext();
          SetCurrentContext(old_context);

          Branch(IntPtrLessThan(index, num_tasks), &loop, &init_queue_loop);
        }
      }

      BIND(&is_unreachable);
      Unreachable();
    }
  }
}

TF_BUILTIN(PromiseResolveThenableJob, InternalBuiltinsAssembler) {
  VARIABLE(exception, MachineRepresentation::kTagged, TheHoleConstant());
  Callable call = CodeFactory::Call(isolate());
  Label reject_promise(this, Label::kDeferred);
  TNode<PromiseResolveThenableJobInfo> microtask =
      TNode<PromiseResolveThenableJobInfo>::UncheckedCast(
          Parameter(Descriptor::kMicrotask));
  TNode<Context> context =
      TNode<Context>::UncheckedCast(Parameter(Descriptor::kContext));

  TNode<JSReceiver> thenable = TNode<JSReceiver>::UncheckedCast(LoadObjectField(
      microtask, PromiseResolveThenableJobInfo::kThenableOffset));
  TNode<JSReceiver> then = TNode<JSReceiver>::UncheckedCast(
      LoadObjectField(microtask, PromiseResolveThenableJobInfo::kThenOffset));
  TNode<JSFunction> resolve = TNode<JSFunction>::UncheckedCast(LoadObjectField(
      microtask, PromiseResolveThenableJobInfo::kResolveOffset));
  TNode<JSFunction> reject = TNode<JSFunction>::UncheckedCast(
      LoadObjectField(microtask, PromiseResolveThenableJobInfo::kRejectOffset));

  Node* const result = CallJS(call, context, then, thenable, resolve, reject);
  GotoIfException(result, &reject_promise, &exception);
  Return(UndefinedConstant());

  BIND(&reject_promise);
  CallJS(call, context, reject, UndefinedConstant(), exception.value());
  Return(UndefinedConstant());
}

TF_BUILTIN(AbortJS, CodeStubAssembler) {
  Node* message = Parameter(Descriptor::kObject);
  Node* reason = SmiConstant(0);
  TailCallRuntime(Runtime::kAbortJS, reason, message);
}

}  // namespace internal
}  // namespace v8
