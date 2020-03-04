// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/codegen/code-stub-assembler.h"
#include "src/codegen/macro-assembler.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/ic/accessor-assembler.h"
#include "src/ic/keyed-store-generic.h"
#include "src/logging/counters.h"
#include "src/objects/debug-objects.h"
#include "src/objects/shared-function-info.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Stack checks.

void Builtins::Generate_StackCheck(MacroAssembler* masm) {
  masm->TailCallRuntime(Runtime::kStackGuard);
}

// -----------------------------------------------------------------------------
// TurboFan support builtins.

TF_BUILTIN(CopyFastSmiOrObjectElements, CodeStubAssembler) {
  TNode<JSObject> js_object = CAST(Parameter(Descriptor::kObject));

  // Load the {object}s elements.
  TNode<FixedArrayBase> source =
      CAST(LoadObjectField(js_object, JSObject::kElementsOffset));
  TNode<FixedArrayBase> target =
      CloneFixedArray(source, ExtractFixedArrayFlag::kFixedArrays);
  StoreObjectField(js_object, JSObject::kElementsOffset, target);
  Return(target);
}

TF_BUILTIN(GrowFastDoubleElements, CodeStubAssembler) {
  TNode<JSObject> object = CAST(Parameter(Descriptor::kObject));
  TNode<Number> key = CAST(Parameter(Descriptor::kKey));

  Label runtime(this, Label::kDeferred);
  TNode<FixedArrayBase> elements = LoadElements(object);
  elements = TryGrowElementsCapacity(object, elements, PACKED_DOUBLE_ELEMENTS,
                                     key, &runtime);
  Return(elements);

  BIND(&runtime);
  TailCallRuntime(Runtime::kGrowArrayElements, NoContextConstant(), object,
                  key);
}

TF_BUILTIN(GrowFastSmiOrObjectElements, CodeStubAssembler) {
  TNode<JSObject> object = CAST(Parameter(Descriptor::kObject));
  TNode<Number> key = CAST(Parameter(Descriptor::kKey));

  Label runtime(this, Label::kDeferred);
  TNode<FixedArrayBase> elements = LoadElements(object);
  elements =
      TryGrowElementsCapacity(object, elements, PACKED_ELEMENTS, key, &runtime);
  Return(elements);

  BIND(&runtime);
  TailCallRuntime(Runtime::kGrowArrayElements, NoContextConstant(), object,
                  key);
}

TF_BUILTIN(NewArgumentsElements, CodeStubAssembler) {
  TNode<IntPtrT> frame = UncheckedCast<IntPtrT>(Parameter(Descriptor::kFrame));
  TNode<IntPtrT> length = SmiToIntPtr(Parameter(Descriptor::kLength));
  TNode<IntPtrT> mapped_count =
      SmiToIntPtr(Parameter(Descriptor::kMappedCount));

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
      TNode<FixedArray> result = CAST(AllocateFixedArray(kind, length));

      // The elements might be used to back mapped arguments. In that case fill
      // the mapped elements (i.e. the first {mapped_count}) with the hole, but
      // make sure not to overshoot the {length} if some arguments are missing.
      TNode<IntPtrT> number_of_holes = IntPtrMin(mapped_count, length);
      TNode<Oddball> the_hole = TheHoleConstant();

      // Fill the first elements up to {number_of_holes} with the hole.
      TVARIABLE(IntPtrT, var_index, IntPtrConstant(0));
      Label loop1(this, &var_index), done_loop1(this);
      Goto(&loop1);
      BIND(&loop1);
      {
        // Load the current {index}.
        TNode<IntPtrT> index = var_index.value();

        // Check if we are done.
        GotoIf(IntPtrEqual(index, number_of_holes), &done_loop1);

        // Store the hole into the {result}.
        StoreFixedArrayElement(result, index, the_hole, SKIP_WRITE_BARRIER);

        // Continue with next {index}.
        var_index = IntPtrAdd(index, IntPtrConstant(1));
        Goto(&loop1);
      }
      BIND(&done_loop1);

      // Compute the effective {offset} into the {frame}.
      TNode<IntPtrT> offset = IntPtrAdd(length, IntPtrConstant(1));

      // Copy the parameters from {frame} (starting at {offset}) to {result}.
      Label loop2(this, &var_index), done_loop2(this);
      Goto(&loop2);
      BIND(&loop2);
      {
        // Load the current {index}.
        TNode<IntPtrT> index = var_index.value();

        // Check if we are done.
        GotoIf(IntPtrEqual(index, length), &done_loop2);

        // Load the parameter at the given {index}.
        TNode<Object> value = BitcastWordToTagged(
            Load(MachineType::Pointer(), frame,
                 TimesSystemPointerSize(IntPtrSub(offset, index))));

        // Store the {value} into the {result}.
        StoreFixedArrayElement(result, index, value, SKIP_WRITE_BARRIER);

        // Continue with next {index}.
        var_index = IntPtrAdd(index, IntPtrConstant(1));
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
                    BitcastWordToTagged(frame), SmiFromIntPtr(length),
                    SmiFromIntPtr(mapped_count));
  }
}

TF_BUILTIN(ReturnReceiver, CodeStubAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  Return(receiver);
}

TF_BUILTIN(DebugBreakTrampoline, CodeStubAssembler) {
  Label tailcall_to_shared(this);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kJSNewTarget));
  TNode<Int32T> arg_count =
      UncheckedCast<Int32T>(Parameter(Descriptor::kJSActualArgumentsCount));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kJSTarget));

  // Check break-at-entry flag on the debug info.
  TNode<SharedFunctionInfo> shared =
      CAST(LoadObjectField(function, JSFunction::kSharedFunctionInfoOffset));
  TNode<Object> maybe_heap_object_or_smi =
      LoadObjectField(shared, SharedFunctionInfo::kScriptOrDebugInfoOffset);
  TNode<HeapObject> maybe_debug_info =
      TaggedToHeapObject(maybe_heap_object_or_smi, &tailcall_to_shared);
  GotoIfNot(HasInstanceType(maybe_debug_info, InstanceType::DEBUG_INFO_TYPE),
            &tailcall_to_shared);

  {
    TNode<DebugInfo> debug_info = CAST(maybe_debug_info);
    TNode<Smi> flags =
        CAST(LoadObjectField(debug_info, DebugInfo::kFlagsOffset));
    GotoIfNot(SmiToInt32(SmiAnd(flags, SmiConstant(DebugInfo::kBreakAtEntry))),
              &tailcall_to_shared);

    CallRuntime(Runtime::kDebugBreakAtEntry, context, function);
    Goto(&tailcall_to_shared);
  }

  BIND(&tailcall_to_shared);
  // Tail call into code object on the SharedFunctionInfo.
  TNode<Code> code = GetSharedFunctionInfoCode(shared);
  TailCallJSCode(code, context, function, new_target, arg_count);
}

class RecordWriteCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit RecordWriteCodeStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  TNode<BoolT> IsMarking() {
    TNode<ExternalReference> is_marking_addr = ExternalConstant(
        ExternalReference::heap_is_marking_flag_address(this->isolate()));
    return Word32NotEqual(Load<Uint8T>(is_marking_addr), Int32Constant(0));
  }

  TNode<BoolT> IsPageFlagSet(TNode<IntPtrT> object, int mask) {
    TNode<IntPtrT> page = PageFromAddress(object);
    TNode<IntPtrT> flags =
        UncheckedCast<IntPtrT>(Load(MachineType::Pointer(), page,
                                    IntPtrConstant(MemoryChunk::kFlagsOffset)));
    return WordNotEqual(WordAnd(flags, IntPtrConstant(mask)),
                        IntPtrConstant(0));
  }

  TNode<BoolT> IsWhite(TNode<IntPtrT> object) {
    DCHECK_EQ(strcmp(Marking::kWhiteBitPattern, "00"), 0);
    TNode<IntPtrT> cell;
    TNode<IntPtrT> mask;
    GetMarkBit(object, &cell, &mask);
    TNode<Int32T> mask32 = TruncateIntPtrToInt32(mask);
    // Non-white has 1 for the first bit, so we only need to check for the first
    // bit.
    return Word32Equal(Word32And(Load<Int32T>(cell), mask32), Int32Constant(0));
  }

  void GetMarkBit(TNode<IntPtrT> object, TNode<IntPtrT>* cell,
                  TNode<IntPtrT>* mask) {
    TNode<IntPtrT> page = PageFromAddress(object);
    TNode<IntPtrT> bitmap =
        Load<IntPtrT>(page, IntPtrConstant(MemoryChunk::kMarkBitmapOffset));

    {
      // Temp variable to calculate cell offset in bitmap.
      TNode<WordT> r0;
      int shift = Bitmap::kBitsPerCellLog2 + kTaggedSizeLog2 -
                  Bitmap::kBytesPerCellLog2;
      r0 = WordShr(object, IntPtrConstant(shift));
      r0 = WordAnd(r0, IntPtrConstant((kPageAlignmentMask >> shift) &
                                      ~(Bitmap::kBytesPerCell - 1)));
      *cell = IntPtrAdd(bitmap, Signed(r0));
    }
    {
      // Temp variable to calculate bit offset in cell.
      TNode<WordT> r1;
      r1 = WordShr(object, IntPtrConstant(kTaggedSizeLog2));
      r1 = WordAnd(r1, IntPtrConstant((1 << Bitmap::kBitsPerCellLog2) - 1));
      // It seems that LSB(e.g. cl) is automatically used, so no manual masking
      // is needed. Uncomment the following line otherwise.
      // WordAnd(r1, IntPtrConstant((1 << kBitsPerByte) - 1)));
      *mask = WordShl(IntPtrConstant(1), r1);
    }
  }

  TNode<BoolT> ShouldSkipFPRegs(SloppyTNode<Smi> mode) {
    return TaggedEqual(mode, SmiConstant(kDontSaveFPRegs));
  }

  TNode<BoolT> ShouldEmitRememberSet(SloppyTNode<Smi> remembered_set) {
    return TaggedEqual(remembered_set, SmiConstant(EMIT_REMEMBERED_SET));
  }

  template <typename Ret, typename Arg0, typename Arg1>
  void CallCFunction2WithCallerSavedRegistersMode(
      TNode<ExternalReference> function, TNode<Arg0> arg0, TNode<Arg1> arg1,
      TNode<Smi> mode, Label* next) {
    Label dont_save_fp(this), save_fp(this);
    Branch(ShouldSkipFPRegs(mode), &dont_save_fp, &save_fp);
    BIND(&dont_save_fp);
    {
      CallCFunctionWithCallerSavedRegisters(
          function, MachineTypeOf<Ret>::value, kDontSaveFPRegs,
          std::make_pair(MachineTypeOf<Arg0>::value, arg0),
          std::make_pair(MachineTypeOf<Arg1>::value, arg1));
      Goto(next);
    }

    BIND(&save_fp);
    {
      CallCFunctionWithCallerSavedRegisters(
          function, MachineTypeOf<Ret>::value, kSaveFPRegs,
          std::make_pair(MachineTypeOf<Arg0>::value, arg0),
          std::make_pair(MachineTypeOf<Arg1>::value, arg1));
      Goto(next);
    }
  }

  template <typename Ret, typename Arg0, typename Arg1, typename Arg2>
  void CallCFunction3WithCallerSavedRegistersMode(
      TNode<ExternalReference> function, TNode<Arg0> arg0, TNode<Arg1> arg1,
      TNode<Arg2> arg2, TNode<Smi> mode, Label* next) {
    Label dont_save_fp(this), save_fp(this);
    Branch(ShouldSkipFPRegs(mode), &dont_save_fp, &save_fp);
    BIND(&dont_save_fp);
    {
      CallCFunctionWithCallerSavedRegisters(
          function, MachineTypeOf<Ret>::value, kDontSaveFPRegs,
          std::make_pair(MachineTypeOf<Arg0>::value, arg0),
          std::make_pair(MachineTypeOf<Arg1>::value, arg1),
          std::make_pair(MachineTypeOf<Arg2>::value, arg2));
      Goto(next);
    }

    BIND(&save_fp);
    {
      CallCFunctionWithCallerSavedRegisters(
          function, MachineTypeOf<Ret>::value, kSaveFPRegs,
          std::make_pair(MachineTypeOf<Arg0>::value, arg0),
          std::make_pair(MachineTypeOf<Arg1>::value, arg1),
          std::make_pair(MachineTypeOf<Arg2>::value, arg2));
      Goto(next);
    }
  }

  void InsertIntoRememberedSetAndGotoSlow(TNode<IntPtrT> object,
                                          TNode<IntPtrT> slot, TNode<Smi> mode,
                                          Label* next) {
    TNode<IntPtrT> page = PageFromAddress(object);
    TNode<ExternalReference> function =
        ExternalConstant(ExternalReference::insert_remembered_set_function());
    CallCFunction2WithCallerSavedRegistersMode<Int32T, IntPtrT, IntPtrT>(
        function, page, slot, mode, next);
  }

  void InsertIntoRememberedSetAndGoto(TNode<IntPtrT> object,
                                      TNode<IntPtrT> slot, TNode<Smi> mode,
                                      Label* next) {
    Label slow_path(this);
    TNode<IntPtrT> page = PageFromAddress(object);

    // Load address of SlotSet
    TNode<IntPtrT> slot_set = LoadSlotSet(page, &slow_path);
    TNode<IntPtrT> slot_offset = IntPtrSub(slot, page);

    // Load bucket
    TNode<IntPtrT> bucket = LoadBucket(slot_set, slot_offset, &slow_path);

    // Update cell
    SetBitInCell(bucket, slot_offset);

    Goto(next);

    BIND(&slow_path);
    InsertIntoRememberedSetAndGotoSlow(object, slot, mode, next);
  }

  TNode<IntPtrT> LoadSlotSet(TNode<IntPtrT> page, Label* slow_path) {
    TNode<IntPtrT> slot_set = UncheckedCast<IntPtrT>(
        Load(MachineType::Pointer(), page,
             IntPtrConstant(MemoryChunk::kOldToNewSlotSetOffset)));
    GotoIf(WordEqual(slot_set, IntPtrConstant(0)), slow_path);

    return slot_set;
  }

  TNode<IntPtrT> LoadBucket(TNode<IntPtrT> slot_set, TNode<WordT> slot_offset,
                            Label* slow_path) {
    TNode<WordT> bucket_index =
        WordShr(slot_offset, SlotSet::kBitsPerBucketLog2 + kTaggedSizeLog2);
    TNode<IntPtrT> bucket = UncheckedCast<IntPtrT>(
        Load(MachineType::Pointer(), slot_set,
             WordShl(bucket_index, kSystemPointerSizeLog2)));
    GotoIf(WordEqual(bucket, IntPtrConstant(0)), slow_path);
    return bucket;
  }

  void SetBitInCell(TNode<IntPtrT> bucket, TNode<WordT> slot_offset) {
    // Load cell value
    TNode<WordT> cell_offset = WordAnd(
        WordShr(slot_offset, SlotSet::kBitsPerCellLog2 + kTaggedSizeLog2 -
                                 SlotSet::kCellSizeBytesLog2),
        IntPtrConstant((SlotSet::kCellsPerBucket - 1)
                       << SlotSet::kCellSizeBytesLog2));
    TNode<IntPtrT> cell_address =
        UncheckedCast<IntPtrT>(IntPtrAdd(bucket, cell_offset));
    TNode<IntPtrT> old_cell_value =
        ChangeInt32ToIntPtr(Load<Int32T>(cell_address));

    // Calculate new cell value
    TNode<WordT> bit_index = WordAnd(WordShr(slot_offset, kTaggedSizeLog2),
                                     IntPtrConstant(SlotSet::kBitsPerCell - 1));
    TNode<IntPtrT> new_cell_value = UncheckedCast<IntPtrT>(
        WordOr(old_cell_value, WordShl(IntPtrConstant(1), bit_index)));

    // Update cell value
    StoreNoWriteBarrier(MachineRepresentation::kWord32, cell_address,
                        TruncateIntPtrToInt32(new_cell_value));
  }
};

TF_BUILTIN(RecordWrite, RecordWriteCodeStubAssembler) {
  Label generational_wb(this);
  Label incremental_wb(this);
  Label exit(this);

  TNode<Smi> remembered_set =
      UncheckedCast<Smi>(Parameter(Descriptor::kRememberedSet));
  Branch(ShouldEmitRememberSet(remembered_set), &generational_wb,
         &incremental_wb);

  BIND(&generational_wb);
  {
    Label test_old_to_young_flags(this);
    Label store_buffer_exit(this), store_buffer_incremental_wb(this);

    // When incremental marking is not on, we skip cross generation pointer
    // checking here, because there are checks for
    // `kPointersFromHereAreInterestingMask` and
    // `kPointersToHereAreInterestingMask` in
    // `src/compiler/<arch>/code-generator-<arch>.cc` before calling this stub,
    // which serves as the cross generation checking.
    TNode<IntPtrT> slot = UncheckedCast<IntPtrT>(Parameter(Descriptor::kSlot));
    Branch(IsMarking(), &test_old_to_young_flags, &store_buffer_exit);

    BIND(&test_old_to_young_flags);
    {
      // TODO(ishell): do a new-space range check instead.
      TNode<IntPtrT> value =
          BitcastTaggedToWord(Load(MachineType::TaggedPointer(), slot));

      // TODO(albertnetymk): Try to cache the page flag for value and object,
      // instead of calling IsPageFlagSet each time.
      TNode<BoolT> value_is_young =
          IsPageFlagSet(value, MemoryChunk::kIsInYoungGenerationMask);
      GotoIfNot(value_is_young, &incremental_wb);

      TNode<IntPtrT> object =
          BitcastTaggedToWord(Parameter(Descriptor::kObject));
      TNode<BoolT> object_is_young =
          IsPageFlagSet(object, MemoryChunk::kIsInYoungGenerationMask);
      Branch(object_is_young, &incremental_wb, &store_buffer_incremental_wb);
    }

    BIND(&store_buffer_exit);
    {
      TNode<Smi> fp_mode = UncheckedCast<Smi>(Parameter(Descriptor::kFPMode));
      TNode<IntPtrT> object =
          BitcastTaggedToWord(Parameter(Descriptor::kObject));
      InsertIntoRememberedSetAndGoto(object, slot, fp_mode, &exit);
    }

    BIND(&store_buffer_incremental_wb);
    {
      TNode<Smi> fp_mode = UncheckedCast<Smi>(Parameter(Descriptor::kFPMode));
      TNode<IntPtrT> object =
          BitcastTaggedToWord(Parameter(Descriptor::kObject));
      InsertIntoRememberedSetAndGoto(object, slot, fp_mode, &incremental_wb);
    }
  }

  BIND(&incremental_wb);
  {
    Label call_incremental_wb(this);

    TNode<IntPtrT> slot = UncheckedCast<IntPtrT>(Parameter(Descriptor::kSlot));
    TNode<IntPtrT> value =
        BitcastTaggedToWord(Load(MachineType::TaggedPointer(), slot));

    // There are two cases we need to call incremental write barrier.
    // 1) value_is_white
    GotoIf(IsWhite(value), &call_incremental_wb);

    // 2) is_compacting && value_in_EC && obj_isnt_skip
    // is_compacting = true when is_marking = true
    GotoIfNot(IsPageFlagSet(value, MemoryChunk::kEvacuationCandidateMask),
              &exit);

    TNode<IntPtrT> object = BitcastTaggedToWord(Parameter(Descriptor::kObject));
    Branch(
        IsPageFlagSet(object, MemoryChunk::kSkipEvacuationSlotsRecordingMask),
        &exit, &call_incremental_wb);

    BIND(&call_incremental_wb);
    {
      TNode<ExternalReference> function = ExternalConstant(
          ExternalReference::incremental_marking_record_write_function());
      TNode<ExternalReference> isolate_constant =
          ExternalConstant(ExternalReference::isolate_address(isolate()));
      TNode<Smi> fp_mode = UncheckedCast<Smi>(Parameter(Descriptor::kFPMode));
      TNode<IntPtrT> object =
          BitcastTaggedToWord(Parameter(Descriptor::kObject));
      CallCFunction3WithCallerSavedRegistersMode<Int32T, IntPtrT, IntPtrT,
                                                 ExternalReference>(
          function, object, slot, isolate_constant, fp_mode, &exit);
    }
  }

  BIND(&exit);
  IncrementCounter(isolate()->counters()->write_barriers(), 1);
  Return(TrueConstant());
}

TF_BUILTIN(EphemeronKeyBarrier, RecordWriteCodeStubAssembler) {
  Label exit(this);

  TNode<ExternalReference> function = ExternalConstant(
      ExternalReference::ephemeron_key_write_barrier_function());
  TNode<ExternalReference> isolate_constant =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  TNode<IntPtrT> address =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kSlotAddress));
  TNode<IntPtrT> object = BitcastTaggedToWord(Parameter(Descriptor::kObject));
  TNode<Smi> fp_mode = UncheckedCast<Smi>(Parameter(Descriptor::kFPMode));
  CallCFunction3WithCallerSavedRegistersMode<Int32T, IntPtrT, IntPtrT,
                                             ExternalReference>(
      function, object, address, isolate_constant, fp_mode, &exit);

  BIND(&exit);
  IncrementCounter(isolate()->counters()->write_barriers(), 1);
  Return(TrueConstant());
}

class DeletePropertyBaseAssembler : public AccessorAssembler {
 public:
  explicit DeletePropertyBaseAssembler(compiler::CodeAssemblerState* state)
      : AccessorAssembler(state) {}

  void DeleteDictionaryProperty(TNode<Object> receiver,
                                TNode<NameDictionary> properties,
                                TNode<Name> name, TNode<Context> context,
                                Label* dont_delete, Label* notfound) {
    TVARIABLE(IntPtrT, var_name_index);
    Label dictionary_found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, name, &dictionary_found,
                                         &var_name_index, notfound);

    BIND(&dictionary_found);
    TNode<IntPtrT> key_index = var_name_index.value();
    TNode<Uint32T> details =
        LoadDetailsByKeyIndex<NameDictionary>(properties, key_index);
    GotoIf(IsSetWord32(details, PropertyDetails::kAttributesDontDeleteMask),
           dont_delete);
    // Overwrite the entry itself (see NameDictionary::SetEntry).
    TNode<Oddball> filler = TheHoleConstant();
    DCHECK(RootsTable::IsImmortalImmovable(RootIndex::kTheHoleValue));
    StoreFixedArrayElement(properties, key_index, filler, SKIP_WRITE_BARRIER);
    StoreValueByKeyIndex<NameDictionary>(properties, key_index, filler,
                                         SKIP_WRITE_BARRIER);
    StoreDetailsByKeyIndex<NameDictionary>(properties, key_index,
                                           SmiConstant(0));

    // Update bookkeeping information (see NameDictionary::ElementRemoved).
    TNode<Smi> nof = GetNumberOfElements<NameDictionary>(properties);
    TNode<Smi> new_nof = SmiSub(nof, SmiConstant(1));
    SetNumberOfElements<NameDictionary>(properties, new_nof);
    TNode<Smi> num_deleted =
        GetNumberOfDeletedElements<NameDictionary>(properties);
    TNode<Smi> new_deleted = SmiAdd(num_deleted, SmiConstant(1));
    SetNumberOfDeletedElements<NameDictionary>(properties, new_deleted);

    // Shrink the dictionary if necessary (see NameDictionary::Shrink).
    Label shrinking_done(this);
    TNode<Smi> capacity = GetCapacity<NameDictionary>(properties);
    GotoIf(SmiGreaterThan(new_nof, SmiShr(capacity, 2)), &shrinking_done);
    GotoIf(SmiLessThan(new_nof, SmiConstant(16)), &shrinking_done);
    CallRuntime(Runtime::kShrinkPropertyDictionary, context, receiver);
    Goto(&shrinking_done);
    BIND(&shrinking_done);

    Return(TrueConstant());
  }
};

TF_BUILTIN(DeleteProperty, DeletePropertyBaseAssembler) {
  TNode<Object> receiver = CAST(Parameter(Descriptor::kObject));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Smi> language_mode = CAST(Parameter(Descriptor::kLanguageMode));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  TVARIABLE(IntPtrT, var_index);
  TVARIABLE(Name, var_unique);
  Label if_index(this, &var_index), if_unique_name(this), if_notunique(this),
      if_notfound(this), slow(this), if_proxy(this);

  GotoIf(TaggedIsSmi(receiver), &slow);
  TNode<Map> receiver_map = LoadMap(CAST(receiver));
  TNode<Uint16T> instance_type = LoadMapInstanceType(receiver_map);
  GotoIf(InstanceTypeEqual(instance_type, JS_PROXY_TYPE), &if_proxy);
  GotoIf(IsCustomElementsReceiverInstanceType(instance_type), &slow);
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
    CheckForAssociatedProtector(var_unique.value(), &slow);

    Label dictionary(this), dont_delete(this);
    GotoIf(IsDictionaryMap(receiver_map), &dictionary);

    // Fast properties need to clear recorded slots, which can only be done
    // in C++.
    Goto(&slow);

    BIND(&dictionary);
    {
      InvalidateValidityCellIfPrototype(receiver_map);

      TNode<NameDictionary> properties =
          CAST(LoadSlowProperties(CAST(receiver)));
      DeleteDictionaryProperty(receiver, properties, var_unique.value(),
                               context, &dont_delete, &if_notfound);
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
    TryInternalizeString(CAST(key), &if_index, &var_index, &if_unique_name,
                         &var_unique, &if_notfound, &slow);
  }

  BIND(&if_notfound);
  Return(TrueConstant());

  BIND(&if_proxy);
  {
    TNode<Name> name = CAST(CallBuiltin(Builtins::kToName, context, key));
    GotoIf(IsPrivateSymbol(name), &slow);
    TailCallBuiltin(Builtins::kProxyDeleteProperty, context, receiver, name,
                    language_mode);
  }

  BIND(&slow);
  {
    TailCallRuntime(Runtime::kDeleteProperty, context, receiver, key,
                    language_mode);
  }
}

namespace {

class SetOrCopyDataPropertiesAssembler : public CodeStubAssembler {
 public:
  explicit SetOrCopyDataPropertiesAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

 protected:
  TNode<Object> SetOrCopyDataProperties(TNode<Context> context,
                                        TNode<JSReceiver> target,
                                        TNode<Object> source, Label* if_runtime,
                                        bool use_set = true) {
    Label if_done(this), if_noelements(this),
        if_sourcenotjsobject(this, Label::kDeferred);

    // JSPrimitiveWrapper wrappers for numbers don't have any enumerable own
    // properties, so we can immediately skip the whole operation if {source} is
    // a Smi.
    GotoIf(TaggedIsSmi(source), &if_done);

    // Otherwise check if {source} is a proper JSObject, and if not, defer
    // to testing for non-empty strings below.
    TNode<Map> source_map = LoadMap(CAST(source));
    TNode<Uint16T> source_instance_type = LoadMapInstanceType(source_map);
    GotoIfNot(IsJSObjectInstanceType(source_instance_type),
              &if_sourcenotjsobject);

    TNode<FixedArrayBase> source_elements = LoadElements(CAST(source));
    GotoIf(IsEmptyFixedArray(source_elements), &if_noelements);
    Branch(IsEmptySlowElementDictionary(source_elements), &if_noelements,
           if_runtime);

    BIND(&if_noelements);
    {
      // If the target is deprecated, the object will be updated on first store.
      // If the source for that store equals the target, this will invalidate
      // the cached representation of the source. Handle this case in runtime.
      TNode<Map> target_map = LoadMap(target);
      GotoIf(IsDeprecatedMap(target_map), if_runtime);

      if (use_set) {
        TNode<BoolT> target_is_simple_receiver = IsSimpleObjectMap(target_map);
        ForEachEnumerableOwnProperty(
            context, source_map, CAST(source), kEnumerationOrder,
            [=](TNode<Name> key, TNode<Object> value) {
              KeyedStoreGenericGenerator::SetProperty(
                  state(), context, target, target_is_simple_receiver, key,
                  value, LanguageMode::kStrict);
            },
            if_runtime);
      } else {
        ForEachEnumerableOwnProperty(
            context, source_map, CAST(source), kEnumerationOrder,
            [=](TNode<Name> key, TNode<Object> value) {
              CallBuiltin(Builtins::kSetPropertyInLiteral, context, target, key,
                          value);
            },
            if_runtime);
      }
      Goto(&if_done);
    }

    BIND(&if_sourcenotjsobject);
    {
      // Handle other JSReceivers in the runtime.
      GotoIf(IsJSReceiverInstanceType(source_instance_type), if_runtime);

      // Non-empty strings are the only non-JSReceivers that need to be
      // handled explicitly by Object.assign() and CopyDataProperties.
      GotoIfNot(IsStringInstanceType(source_instance_type), &if_done);
      TNode<IntPtrT> source_length = LoadStringLengthAsWord(CAST(source));
      Branch(IntPtrEqual(source_length, IntPtrConstant(0)), &if_done,
             if_runtime);
    }

    BIND(&if_done);
    return UndefinedConstant();
  }
};

}  // namespace

// ES #sec-copydataproperties
TF_BUILTIN(CopyDataProperties, SetOrCopyDataPropertiesAssembler) {
  TNode<JSObject> target = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> source = CAST(Parameter(Descriptor::kSource));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  CSA_ASSERT(this, TaggedNotEqual(target, source));

  Label if_runtime(this, Label::kDeferred);
  Return(SetOrCopyDataProperties(context, target, source, &if_runtime, false));

  BIND(&if_runtime);
  TailCallRuntime(Runtime::kCopyDataProperties, context, target, source);
}

TF_BUILTIN(SetDataProperties, SetOrCopyDataPropertiesAssembler) {
  TNode<JSReceiver> target = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> source = CAST(Parameter(Descriptor::kSource));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label if_runtime(this, Label::kDeferred);
  Return(SetOrCopyDataProperties(context, target, source, &if_runtime, true));

  BIND(&if_runtime);
  TailCallRuntime(Runtime::kSetDataProperties, context, target, source);
}

TF_BUILTIN(ForInEnumerate, CodeStubAssembler) {
  TNode<HeapObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label if_empty(this), if_runtime(this, Label::kDeferred);
  TNode<Map> receiver_map = CheckEnumCache(receiver, &if_empty, &if_runtime);
  Return(receiver_map);

  BIND(&if_empty);
  Return(EmptyFixedArrayConstant());

  BIND(&if_runtime);
  TailCallRuntime(Runtime::kForInEnumerate, context, receiver);
}

TF_BUILTIN(ForInFilter, CodeStubAssembler) {
  TNode<String> key = CAST(Parameter(Descriptor::kKey));
  TNode<HeapObject> object = CAST(Parameter(Descriptor::kObject));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));

  Label if_true(this), if_false(this);
  TNode<Oddball> result = HasProperty(context, object, key, kForInHasProperty);
  Branch(IsTrue(result), &if_true, &if_false);

  BIND(&if_true);
  Return(key);

  BIND(&if_false);
  Return(UndefinedConstant());
}

TF_BUILTIN(SameValue, CodeStubAssembler) {
  TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
  TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));

  Label if_true(this), if_false(this);
  BranchIfSameValue(lhs, rhs, &if_true, &if_false);

  BIND(&if_true);
  Return(TrueConstant());

  BIND(&if_false);
  Return(FalseConstant());
}

TF_BUILTIN(SameValueNumbersOnly, CodeStubAssembler) {
  TNode<Object> lhs = CAST(Parameter(Descriptor::kLeft));
  TNode<Object> rhs = CAST(Parameter(Descriptor::kRight));

  Label if_true(this), if_false(this);
  BranchIfSameValue(lhs, rhs, &if_true, &if_false, SameValueMode::kNumbersOnly);

  BIND(&if_true);
  Return(TrueConstant());

  BIND(&if_false);
  Return(FalseConstant());
}

TF_BUILTIN(AdaptorWithBuiltinExitFrame, CodeStubAssembler) {
  TNode<JSFunction> target = CAST(Parameter(Descriptor::kTarget));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<WordT> c_function =
      UncheckedCast<WordT>(Parameter(Descriptor::kCFunction));

  // The logic contained here is mirrored for TurboFan inlining in
  // JSTypedLowering::ReduceJSCall{Function,Construct}. Keep these in sync.

  // Make sure we operate in the context of the called function (for example
  // ConstructStubs implemented in C++ will be run in the context of the caller
  // instead of the callee, due to the way that [[Construct]] is defined for
  // ordinary functions).
  TNode<Context> context =
      CAST(LoadObjectField(target, JSFunction::kContextOffset));

  // Update arguments count for CEntry to contain the number of arguments
  // including the receiver and the extra arguments.
  TNode<Int32T> argc =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  argc = Int32Add(
      argc,
      Int32Constant(BuiltinExitFrameConstants::kNumExtraArgsWithReceiver));

  const bool builtin_exit_frame = true;
  TNode<Code> code = HeapConstant(CodeFactory::CEntry(
      isolate(), 1, kDontSaveFPRegs, kArgvOnStack, builtin_exit_frame));

  // Unconditionally push argc, target and new target as extra stack arguments.
  // They will be used by stack frame iterators when constructing stack trace.
  TailCallStub(CEntry1ArgvOnStackDescriptor{},  // descriptor
               code, context,       // standard arguments for TailCallStub
               argc, c_function,    // register arguments
               TheHoleConstant(),   // additional stack argument 1 (padding)
               SmiFromInt32(argc),  // additional stack argument 2
               target,              // additional stack argument 3
               new_target);         // additional stack argument 4
}

TF_BUILTIN(AllocateInYoungGeneration, CodeStubAssembler) {
  TNode<IntPtrT> requested_size =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kRequestedSize));
  CSA_CHECK(this, IsValidPositiveSmi(requested_size));

  TNode<Smi> allocation_flags =
      SmiConstant(Smi::FromInt(AllocateDoubleAlignFlag::encode(false) |
                               AllowLargeObjectAllocationFlag::encode(true)));
  TailCallRuntime(Runtime::kAllocateInYoungGeneration, NoContextConstant(),
                  SmiFromIntPtr(requested_size), allocation_flags);
}

TF_BUILTIN(AllocateRegularInYoungGeneration, CodeStubAssembler) {
  TNode<IntPtrT> requested_size =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kRequestedSize));
  CSA_CHECK(this, IsValidPositiveSmi(requested_size));

  TNode<Smi> allocation_flags =
      SmiConstant(Smi::FromInt(AllocateDoubleAlignFlag::encode(false) |
                               AllowLargeObjectAllocationFlag::encode(false)));
  TailCallRuntime(Runtime::kAllocateInYoungGeneration, NoContextConstant(),
                  SmiFromIntPtr(requested_size), allocation_flags);
}

TF_BUILTIN(AllocateInOldGeneration, CodeStubAssembler) {
  TNode<IntPtrT> requested_size =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kRequestedSize));
  CSA_CHECK(this, IsValidPositiveSmi(requested_size));

  TNode<Smi> runtime_flags =
      SmiConstant(Smi::FromInt(AllocateDoubleAlignFlag::encode(false) |
                               AllowLargeObjectAllocationFlag::encode(true)));
  TailCallRuntime(Runtime::kAllocateInOldGeneration, NoContextConstant(),
                  SmiFromIntPtr(requested_size), runtime_flags);
}

TF_BUILTIN(AllocateRegularInOldGeneration, CodeStubAssembler) {
  TNode<IntPtrT> requested_size =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kRequestedSize));
  CSA_CHECK(this, IsValidPositiveSmi(requested_size));

  TNode<Smi> runtime_flags =
      SmiConstant(Smi::FromInt(AllocateDoubleAlignFlag::encode(false) |
                               AllowLargeObjectAllocationFlag::encode(false)));
  TailCallRuntime(Runtime::kAllocateInOldGeneration, NoContextConstant(),
                  SmiFromIntPtr(requested_size), runtime_flags);
}

TF_BUILTIN(Abort, CodeStubAssembler) {
  TNode<Smi> message_id = CAST(Parameter(Descriptor::kMessageOrMessageId));
  TailCallRuntime(Runtime::kAbort, NoContextConstant(), message_id);
}

TF_BUILTIN(AbortCSAAssert, CodeStubAssembler) {
  TNode<String> message = CAST(Parameter(Descriptor::kMessageOrMessageId));
  TailCallRuntime(Runtime::kAbortCSAAssert, NoContextConstant(), message);
}

void Builtins::Generate_CEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 1, kDontSaveFPRegs, kArgvOnStack, false);
}

void Builtins::Generate_CEntry_Return1_DontSaveFPRegs_ArgvOnStack_BuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 1, kDontSaveFPRegs, kArgvOnStack, true);
}

void Builtins::
    Generate_CEntry_Return1_DontSaveFPRegs_ArgvInRegister_NoBuiltinExit(
        MacroAssembler* masm) {
  Generate_CEntry(masm, 1, kDontSaveFPRegs, kArgvInRegister, false);
}

void Builtins::Generate_CEntry_Return1_SaveFPRegs_ArgvOnStack_NoBuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 1, kSaveFPRegs, kArgvOnStack, false);
}

void Builtins::Generate_CEntry_Return1_SaveFPRegs_ArgvOnStack_BuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 1, kSaveFPRegs, kArgvOnStack, true);
}

void Builtins::Generate_CEntry_Return2_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 2, kDontSaveFPRegs, kArgvOnStack, false);
}

void Builtins::Generate_CEntry_Return2_DontSaveFPRegs_ArgvOnStack_BuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 2, kDontSaveFPRegs, kArgvOnStack, true);
}

void Builtins::
    Generate_CEntry_Return2_DontSaveFPRegs_ArgvInRegister_NoBuiltinExit(
        MacroAssembler* masm) {
  Generate_CEntry(masm, 2, kDontSaveFPRegs, kArgvInRegister, false);
}

void Builtins::Generate_CEntry_Return2_SaveFPRegs_ArgvOnStack_NoBuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 2, kSaveFPRegs, kArgvOnStack, false);
}

void Builtins::Generate_CEntry_Return2_SaveFPRegs_ArgvOnStack_BuiltinExit(
    MacroAssembler* masm) {
  Generate_CEntry(masm, 2, kSaveFPRegs, kArgvOnStack, true);
}

#if !defined(V8_TARGET_ARCH_ARM) && !defined(V8_TARGET_ARCH_MIPS)
void Builtins::Generate_MemCopyUint8Uint8(MacroAssembler* masm) {
  masm->Call(BUILTIN_CODE(masm->isolate(), Illegal), RelocInfo::CODE_TARGET);
}
#endif  // !defined(V8_TARGET_ARCH_ARM) && !defined(V8_TARGET_ARCH_MIPS)

#ifndef V8_TARGET_ARCH_IA32
void Builtins::Generate_MemMove(MacroAssembler* masm) {
  masm->Call(BUILTIN_CODE(masm->isolate(), Illegal), RelocInfo::CODE_TARGET);
}
#endif  // V8_TARGET_ARCH_IA32

// ES6 [[Get]] operation.
TF_BUILTIN(GetProperty, CodeStubAssembler) {
  TNode<Object> object = CAST(Parameter(Descriptor::kObject));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  // TODO(duongn): consider tailcalling to GetPropertyWithReceiver(object,
  // object, key, OnNonExistent::kReturnUndefined).
  Label if_notfound(this), if_proxy(this, Label::kDeferred),
      if_slow(this, Label::kDeferred);

  CodeStubAssembler::LookupPropertyInHolder lookup_property_in_holder =
      [=](TNode<HeapObject> receiver, TNode<HeapObject> holder,
          TNode<Map> holder_map, TNode<Int32T> holder_instance_type,
          TNode<Name> unique_name, Label* next_holder, Label* if_bailout) {
        TVARIABLE(Object, var_value);
        Label if_found(this);
        TryGetOwnProperty(context, receiver, holder, holder_map,
                          holder_instance_type, unique_name, &if_found,
                          &var_value, next_holder, if_bailout);
        BIND(&if_found);
        Return(var_value.value());
      };

  CodeStubAssembler::LookupElementInHolder lookup_element_in_holder =
      [=](TNode<HeapObject> receiver, TNode<HeapObject> holder,
          TNode<Map> holder_map, TNode<Int32T> holder_instance_type,
          TNode<IntPtrT> index, Label* next_holder, Label* if_bailout) {
        // Not supported yet.
        Use(next_holder);
        Goto(if_bailout);
      };

  TryPrototypeChainLookup(object, object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &if_notfound, &if_slow,
                          &if_proxy);

  BIND(&if_notfound);
  Return(UndefinedConstant());

  BIND(&if_slow);
  TailCallRuntime(Runtime::kGetProperty, context, object, key);

  BIND(&if_proxy);
  {
    // Convert the {key} to a Name first.
    TNode<Object> name = CallBuiltin(Builtins::kToName, context, key);

    // The {object} is a JSProxy instance, look up the {name} on it, passing
    // {object} both as receiver and holder. If {name} is absent we can safely
    // return undefined from here.
    TailCallBuiltin(Builtins::kProxyGetProperty, context, object, name, object,
                    SmiConstant(OnNonExistent::kReturnUndefined));
  }
}

// ES6 [[Get]] operation with Receiver.
TF_BUILTIN(GetPropertyWithReceiver, CodeStubAssembler) {
  TNode<Object> object = CAST(Parameter(Descriptor::kObject));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> on_non_existent = CAST(Parameter(Descriptor::kOnNonExistent));
  Label if_notfound(this), if_proxy(this, Label::kDeferred),
      if_slow(this, Label::kDeferred);

  CodeStubAssembler::LookupPropertyInHolder lookup_property_in_holder =
      [=](TNode<HeapObject> receiver, TNode<HeapObject> holder,
          TNode<Map> holder_map, TNode<Int32T> holder_instance_type,
          TNode<Name> unique_name, Label* next_holder, Label* if_bailout) {
        TVARIABLE(Object, var_value);
        Label if_found(this);
        TryGetOwnProperty(context, receiver, holder, holder_map,
                          holder_instance_type, unique_name, &if_found,
                          &var_value, next_holder, if_bailout);
        BIND(&if_found);
        Return(var_value.value());
      };

  CodeStubAssembler::LookupElementInHolder lookup_element_in_holder =
      [=](TNode<HeapObject> receiver, TNode<HeapObject> holder,
          TNode<Map> holder_map, TNode<Int32T> holder_instance_type,
          TNode<IntPtrT> index, Label* next_holder, Label* if_bailout) {
        // Not supported yet.
        Use(next_holder);
        Goto(if_bailout);
      };

  TryPrototypeChainLookup(receiver, object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &if_notfound, &if_slow,
                          &if_proxy);

  BIND(&if_notfound);
  Label throw_reference_error(this);
  GotoIf(TaggedEqual(on_non_existent,
                     SmiConstant(OnNonExistent::kThrowReferenceError)),
         &throw_reference_error);
  CSA_ASSERT(this, TaggedEqual(on_non_existent,
                               SmiConstant(OnNonExistent::kReturnUndefined)));
  Return(UndefinedConstant());

  BIND(&throw_reference_error);
  Return(CallRuntime(Runtime::kThrowReferenceError, context, key));

  BIND(&if_slow);
  TailCallRuntime(Runtime::kGetPropertyWithReceiver, context, object, key,
                  receiver, on_non_existent);

  BIND(&if_proxy);
  {
    // Convert the {key} to a Name first.
    TNode<Name> name = CAST(CallBuiltin(Builtins::kToName, context, key));

    // Proxy cannot handle private symbol so bailout.
    GotoIf(IsPrivateSymbol(name), &if_slow);

    // The {object} is a JSProxy instance, look up the {name} on it, passing
    // {object} both as receiver and holder. If {name} is absent we can safely
    // return undefined from here.
    TailCallBuiltin(Builtins::kProxyGetProperty, context, object, name,
                    receiver, on_non_existent);
  }
}

// ES6 [[Set]] operation.
TF_BUILTIN(SetProperty, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));

  KeyedStoreGenericGenerator::SetProperty(state(), context, receiver, key,
                                          value, LanguageMode::kStrict);
}

// ES6 CreateDataProperty(), specialized for the case where objects are still
// being initialized, and have not yet been made accessible to the user. Thus,
// any operation here should be unobservable until after the object has been
// returned.
TF_BUILTIN(SetPropertyInLiteral, CodeStubAssembler) {
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<JSObject> receiver = CAST(Parameter(Descriptor::kReceiver));
  TNode<Object> key = CAST(Parameter(Descriptor::kKey));
  TNode<Object> value = CAST(Parameter(Descriptor::kValue));

  KeyedStoreGenericGenerator::SetPropertyInLiteral(state(), context, receiver,
                                                   key, value);
}

TF_BUILTIN(InstantiateAsmJs, CodeStubAssembler) {
  Label tailcall_to_function(this);
  TNode<Context> context = CAST(Parameter(Descriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(Descriptor::kNewTarget));
  TNode<Int32T> arg_count =
      UncheckedCast<Int32T>(Parameter(Descriptor::kActualArgumentsCount));
  TNode<JSFunction> function = CAST(Parameter(Descriptor::kTarget));

  // Retrieve arguments from caller (stdlib, foreign, heap).
  CodeStubArguments args(this, arg_count);
  TNode<Object> stdlib = args.GetOptionalArgumentValue(0);
  TNode<Object> foreign = args.GetOptionalArgumentValue(1);
  TNode<Object> heap = args.GetOptionalArgumentValue(2);

  // Call runtime, on success just pass the result to the caller and pop all
  // arguments. A smi 0 is returned on failure, an object on success.
  TNode<Object> maybe_result_or_smi_zero = CallRuntime(
      Runtime::kInstantiateAsmJs, context, function, stdlib, foreign, heap);
  GotoIf(TaggedIsSmi(maybe_result_or_smi_zero), &tailcall_to_function);
  args.PopAndReturn(maybe_result_or_smi_zero);

  BIND(&tailcall_to_function);
  // On failure, tail call back to regular JavaScript by re-calling the given
  // function which has been reset to the compile lazy builtin.
  TNode<Code> code = CAST(LoadObjectField(function, JSFunction::kCodeOffset));
  TailCallJSCode(code, context, function, new_target, arg_count);
}

}  // namespace internal
}  // namespace v8
