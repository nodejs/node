// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/macro-assembler.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

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

  ParameterMode mode = OptimalParameterMode();
  Node* length = TaggedToParameter(LoadFixedArrayBaseLength(source), mode);

  // Check if we can allocate in new space.
  ElementsKind kind = PACKED_ELEMENTS;
  int max_elements = FixedArrayBase::GetMaxLengthForNewSpaceAllocation(kind);
  Label if_newspace(this), if_lospace(this, Label::kDeferred);
  Branch(UintPtrOrSmiLessThan(length, IntPtrOrSmiConstant(max_elements, mode),
                              mode),
         &if_newspace, &if_lospace);

  BIND(&if_newspace);
  {
    Node* target = AllocateFixedArray(kind, length, mode);
    CopyFixedArrayElements(kind, source, target, length, SKIP_WRITE_BARRIER,
                           mode);
    StoreObjectField(object, JSObject::kElementsOffset, target);
    Return(target);
  }

  BIND(&if_lospace);
  {
    Node* target =
        AllocateFixedArray(kind, length, mode, kAllowLargeObjectAllocation);
    CopyFixedArrayElements(kind, source, target, length, UPDATE_WRITE_BARRIER,
                           mode);
    StoreObjectField(object, JSObject::kElementsOffset, target);
    Return(target);
  }
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

TF_BUILTIN(NewUnmappedArgumentsElements, CodeStubAssembler) {
  Node* frame = Parameter(Descriptor::kFrame);
  Node* length = SmiToWord(Parameter(Descriptor::kLength));

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

      // Compute the effective {offset} into the {frame}.
      Node* offset = IntPtrAdd(length, IntPtrConstant(1));

      // Copy the parameters from {frame} (starting at {offset}) to {result}.
      VARIABLE(var_index, MachineType::PointerRepresentation());
      Label loop(this, &var_index), done_loop(this);
      var_index.Bind(IntPtrConstant(0));
      Goto(&loop);
      BIND(&loop);
      {
        // Load the current {index}.
        Node* index = var_index.value();

        // Check if we are done.
        GotoIf(WordEqual(index, length), &done_loop);

        // Load the parameter at the given {index}.
        Node* value = Load(MachineType::AnyTagged(), frame,
                           TimesPointerSize(IntPtrSub(offset, index)));

        // Store the {value} into the {result}.
        StoreFixedArrayElement(result, index, value, SKIP_WRITE_BARRIER);

        // Continue with next {index}.
        var_index.Bind(IntPtrAdd(index, IntPtrConstant(1)));
        Goto(&loop);
      }

      BIND(&done_loop);
      Return(result);
    }
  }

  BIND(&if_oldspace);
  {
    // Allocate in old space (or large object space).
    TailCallRuntime(Runtime::kNewArgumentsElements, NoContextConstant(),
                    BitcastWordToTagged(frame), SmiFromWord(length));
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

    DCHECK(strcmp(Marking::kBlackBitPattern, "11") == 0);

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
    DCHECK(strcmp(Marking::kWhiteBitPattern, "00") == 0);
    Node* cell;
    Node* mask;
    GetMarkBit(object, &cell, &mask);
    // Non-white has 1 for the first bit, so we only need to check for the first
    // bit.
    return WordEqual(WordAnd(Load(MachineType::Pointer(), cell), mask),
                     IntPtrConstant(0));
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

  void InsertToStoreBufferAndGoto(Node* isolate, Node* slot, Label* next) {
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
      CallCFunction1WithCallerSavedRegisters(
          MachineType::Int32(), MachineType::Pointer(), function, isolate);
      Goto(next);
    }
  }
};

TF_BUILTIN(RecordWrite, RecordWriteCodeStubAssembler) {
  Node* object = BitcastTaggedToWord(Parameter(Descriptor::kObject));
  Node* slot = Parameter(Descriptor::kSlot);
  Node* isolate = Parameter(Descriptor::kIsolate);
  Node* value;

  Label test_old_to_new_flags(this);
  Label store_buffer_exit(this), store_buffer_incremental_wb(this);
  Label incremental_wb(this);
  Label exit(this);

  // When incremental marking is not on, we skip cross generation pointer
  // checking here, because there are checks for
  // `kPointersFromHereAreInterestingMask` and
  // `kPointersToHereAreInterestingMask` in
  // `src/compiler/<arch>/code-generator-<arch>.cc` before calling this stub,
  // which serves as the cross generation checking.
  Branch(IsMarking(), &test_old_to_new_flags, &store_buffer_exit);

  BIND(&test_old_to_new_flags);
  {
    value = Load(MachineType::Pointer(), slot);
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
  { InsertToStoreBufferAndGoto(isolate, slot, &exit); }

  BIND(&store_buffer_incremental_wb);
  { InsertToStoreBufferAndGoto(isolate, slot, &incremental_wb); }

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
      CallCFunction3WithCallerSavedRegisters(
          MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
          MachineType::Pointer(), function, object, slot, isolate);
      Goto(&exit);
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
      STATIC_ASSERT(LANGUAGE_END == 2);
      GotoIf(SmiNotEqual(language_mode, SmiConstant(SLOPPY)), &slow);
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

}  // namespace internal
}  // namespace v8
