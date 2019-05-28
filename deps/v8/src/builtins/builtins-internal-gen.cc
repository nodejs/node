// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/counters.h"
#include "src/heap/heap-inl.h"  // crbug.com/v8/8499
#include "src/ic/accessor-assembler.h"
#include "src/ic/keyed-store-generic.h"
#include "src/macro-assembler.h"
#include "src/objects/debug-objects.h"
#include "src/objects/shared-function-info.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

template <typename T>
using TNode = compiler::TNode<T>;

// -----------------------------------------------------------------------------
// Stack checks.

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
      Node* the_hole = TheHoleConstant();

      // Fill the first elements up to {number_of_holes} with the hole.
      TVARIABLE(IntPtrT, var_index, IntPtrConstant(0));
      Label loop1(this, &var_index), done_loop1(this);
      Goto(&loop1);
      BIND(&loop1);
      {
        // Load the current {index}.
        TNode<IntPtrT> index = var_index.value();

        // Check if we are done.
        GotoIf(WordEqual(index, number_of_holes), &done_loop1);

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
        GotoIf(WordEqual(index, length), &done_loop2);

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
  Return(Parameter(Descriptor::kReceiver));
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

  Node* IsMarking() {
    Node* is_marking_addr = ExternalConstant(
        ExternalReference::heap_is_marking_flag_address(this->isolate()));
    return Load(MachineType::Uint8(), is_marking_addr);
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
    Node* cell;
    Node* mask;
    GetMarkBit(object, &cell, &mask);
    mask = TruncateIntPtrToInt32(mask);
    // Non-white has 1 for the first bit, so we only need to check for the first
    // bit.
    return Word32Equal(Word32And(Load(MachineType::Int32(), cell), mask),
                       Int32Constant(0));
  }

  void GetMarkBit(TNode<IntPtrT> object, Node** cell, Node** mask) {
    TNode<IntPtrT> page = PageFromAddress(object);
    Node* bitmap = Load(MachineType::Pointer(), page,
                        IntPtrConstant(MemoryChunk::kMarkBitmapOffset));

    {
      // Temp variable to calculate cell offset in bitmap.
      Node* r0;
      int shift = Bitmap::kBitsPerCellLog2 + kTaggedSizeLog2 -
                  Bitmap::kBytesPerCellLog2;
      r0 = WordShr(object, IntPtrConstant(shift));
      r0 = WordAnd(r0, IntPtrConstant((kPageAlignmentMask >> shift) &
                                      ~(Bitmap::kBytesPerCell - 1)));
      *cell = IntPtrAdd(bitmap, r0);
    }
    {
      // Temp variable to calculate bit offset in cell.
      Node* r1;
      r1 = WordShr(object, IntPtrConstant(kTaggedSizeLog2));
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
      CallCFunctionWithCallerSavedRegisters(function, return_type,
                                            kDontSaveFPRegs,
                                            std::make_pair(arg0_type, arg0));
      Goto(next);
    }

    BIND(&save_fp);
    {
      CallCFunctionWithCallerSavedRegisters(function, return_type,
                                            kSaveFPRegs,
                                            std::make_pair(arg0_type, arg0));
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
      CallCFunctionWithCallerSavedRegisters(
          function, return_type, kDontSaveFPRegs,
          std::make_pair(arg0_type, arg0), std::make_pair(arg1_type, arg1),
          std::make_pair(arg2_type, arg2));
      Goto(next);
    }

    BIND(&save_fp);
    {
      CallCFunctionWithCallerSavedRegisters(
          function, return_type, kSaveFPRegs, std::make_pair(arg0_type, arg0),
          std::make_pair(arg1_type, arg1), std::make_pair(arg2_type, arg2));
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
        IntPtrAdd(store_buffer_top, IntPtrConstant(kSystemPointerSize));
    StoreNoWriteBarrier(MachineType::PointerRepresentation(),
                        store_buffer_top_addr, new_store_buffer_top);

    Node* test = WordAnd(new_store_buffer_top,
                         IntPtrConstant(Heap::store_buffer_mask_constant()));

    Label overflow(this);
    Branch(WordEqual(test, IntPtrConstant(0)), &overflow, next);

    BIND(&overflow);
    {
      Node* function =
          ExternalConstant(ExternalReference::store_buffer_overflow_function());
      CallCFunction1WithCallerSavedRegistersMode(MachineType::Int32(),
                                                 MachineType::Pointer(),
                                                 function, isolate, mode, next);
    }
  }
};

TF_BUILTIN(RecordWrite, RecordWriteCodeStubAssembler) {
  Label generational_wb(this);
  Label incremental_wb(this);
  Label exit(this);

  Node* remembered_set = Parameter(Descriptor::kRememberedSet);
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
      Node* isolate_constant =
          ExternalConstant(ExternalReference::isolate_address(isolate()));
      Node* fp_mode = Parameter(Descriptor::kFPMode);
      InsertToStoreBufferAndGoto(isolate_constant, slot, fp_mode, &exit);
    }

    BIND(&store_buffer_incremental_wb);
    {
      Node* isolate_constant =
          ExternalConstant(ExternalReference::isolate_address(isolate()));
      Node* fp_mode = Parameter(Descriptor::kFPMode);
      InsertToStoreBufferAndGoto(isolate_constant, slot, fp_mode,
                                 &incremental_wb);
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
      Node* function = ExternalConstant(
          ExternalReference::incremental_marking_record_write_function());
      Node* isolate_constant =
          ExternalConstant(ExternalReference::isolate_address(isolate()));
      Node* fp_mode = Parameter(Descriptor::kFPMode);
      TNode<IntPtrT> object =
          BitcastTaggedToWord(Parameter(Descriptor::kObject));
      CallCFunction3WithCallerSavedRegistersMode(
          MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
          MachineType::Pointer(), function, object, slot, isolate_constant,
          fp_mode, &exit);
    }
  }

  BIND(&exit);
  IncrementCounter(isolate()->counters()->write_barriers(), 1);
  Return(TrueConstant());
}

TF_BUILTIN(EphemeronKeyBarrier, RecordWriteCodeStubAssembler) {
  Label exit(this);

  Node* function = ExternalConstant(
      ExternalReference::ephemeron_key_write_barrier_function());
  Node* isolate_constant =
      ExternalConstant(ExternalReference::isolate_address(isolate()));
  Node* address = Parameter(Descriptor::kSlotAddress);
  Node* object = BitcastTaggedToWord(Parameter(Descriptor::kObject));
  Node* fp_mode = Parameter(Descriptor::kFPMode);
  CallCFunction3WithCallerSavedRegistersMode(
      MachineType::Int32(), MachineType::Pointer(), MachineType::Pointer(),
      MachineType::Pointer(), function, object, address, isolate_constant,
      fp_mode, &exit);

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
    TNode<HeapObject> filler = TheHoleConstant();
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

  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged, key);
  Label if_index(this), if_unique_name(this), if_notunique(this),
      if_notfound(this), slow(this);

  GotoIf(TaggedIsSmi(receiver), &slow);
  TNode<Map> receiver_map = LoadMap(CAST(receiver));
  TNode<Int32T> instance_type = LoadMapInstanceType(receiver_map);
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
    TNode<Name> unique = CAST(var_unique.value());
    CheckForAssociatedProtector(unique, &slow);

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
  TNode<Oddball> result = HasProperty(context, object, key, kForInHasProperty);
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

  template <typename Descriptor>
  void GenerateAdaptorWithExitFrameType(
      Builtins::ExitFrameType exit_frame_type);
};

template <typename Descriptor>
void InternalBuiltinsAssembler::GenerateAdaptorWithExitFrameType(
    Builtins::ExitFrameType exit_frame_type) {
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

  TNode<Code> code = HeapConstant(
      CodeFactory::CEntry(isolate(), 1, kDontSaveFPRegs, kArgvOnStack,
                          exit_frame_type == Builtins::BUILTIN_EXIT));

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

TF_BUILTIN(AdaptorWithExitFrame, InternalBuiltinsAssembler) {
  GenerateAdaptorWithExitFrameType<Descriptor>(Builtins::EXIT);
}

TF_BUILTIN(AdaptorWithBuiltinExitFrame, InternalBuiltinsAssembler) {
  GenerateAdaptorWithExitFrameType<Descriptor>(Builtins::BUILTIN_EXIT);
}

TF_BUILTIN(AllocateInYoungGeneration, CodeStubAssembler) {
  TNode<IntPtrT> requested_size =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kRequestedSize));

  TailCallRuntime(Runtime::kAllocateInYoungGeneration, NoContextConstant(),
                  SmiFromIntPtr(requested_size));
}

TF_BUILTIN(AllocateInOldGeneration, CodeStubAssembler) {
  TNode<IntPtrT> requested_size =
      UncheckedCast<IntPtrT>(Parameter(Descriptor::kRequestedSize));

  TailCallRuntime(Runtime::kAllocateInOldGeneration, NoContextConstant(),
                  SmiFromIntPtr(requested_size), SmiConstant(0));
}

TF_BUILTIN(Abort, CodeStubAssembler) {
  TNode<Smi> message_id = CAST(Parameter(Descriptor::kMessageOrMessageId));
  TailCallRuntime(Runtime::kAbort, NoContextConstant(), message_id);
}

TF_BUILTIN(AbortJS, CodeStubAssembler) {
  TNode<String> message = CAST(Parameter(Descriptor::kMessageOrMessageId));
  TailCallRuntime(Runtime::kAbortJS, NoContextConstant(), message);
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

#ifndef V8_TARGET_ARCH_ARM
void Builtins::Generate_MemCopyUint16Uint8(MacroAssembler* masm) {
  masm->Call(BUILTIN_CODE(masm->isolate(), Illegal), RelocInfo::CODE_TARGET);
}
#endif  // V8_TARGET_ARCH_ARM

#ifndef V8_TARGET_ARCH_IA32
void Builtins::Generate_MemMove(MacroAssembler* masm) {
  masm->Call(BUILTIN_CODE(masm->isolate(), Illegal), RelocInfo::CODE_TARGET);
}
#endif  // V8_TARGET_ARCH_IA32

// ES6 [[Get]] operation.
TF_BUILTIN(GetProperty, CodeStubAssembler) {
  Node* object = Parameter(Descriptor::kObject);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);
  Label if_notfound(this), if_proxy(this, Label::kDeferred),
      if_slow(this, Label::kDeferred);

  CodeStubAssembler::LookupInHolder lookup_property_in_holder =
      [=](Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* unique_name, Label* next_holder,
          Label* if_bailout) {
        VARIABLE(var_value, MachineRepresentation::kTagged);
        Label if_found(this);
        TryGetOwnProperty(context, receiver, holder, holder_map,
                          holder_instance_type, unique_name, &if_found,
                          &var_value, next_holder, if_bailout);
        BIND(&if_found);
        Return(var_value.value());
      };

  CodeStubAssembler::LookupInHolder lookup_element_in_holder =
      [=](Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* index, Label* next_holder,
          Label* if_bailout) {
        // Not supported yet.
        Use(next_holder);
        Goto(if_bailout);
      };

  TryPrototypeChainLookup(object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &if_notfound, &if_slow,
                          &if_proxy);

  BIND(&if_notfound);
  Return(UndefinedConstant());

  BIND(&if_slow);
  TailCallRuntime(Runtime::kGetProperty, context, object, key);

  BIND(&if_proxy);
  {
    // Convert the {key} to a Name first.
    Node* name = CallBuiltin(Builtins::kToName, context, key);

    // The {object} is a JSProxy instance, look up the {name} on it, passing
    // {object} both as receiver and holder. If {name} is absent we can safely
    // return undefined from here.
    TailCallBuiltin(Builtins::kProxyGetProperty, context, object, name, object,
                    SmiConstant(OnNonExistent::kReturnUndefined));
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

}  // namespace internal
}  // namespace v8
