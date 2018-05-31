// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"
#include "src/heap/heap-inl.h"
#include "src/ic/accessor-assembler.h"
#include "src/macro-assembler.h"
#include "src/objects/debug-objects.h"
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
      Node* result = AllocateFixedArray(kind, length);

      // The elements might be used to back mapped arguments. In that case fill
      // the mapped elements (i.e. the first {mapped_count}) with the hole, but
      // make sure not to overshoot the {length} if some arguments are missing.
      TNode<IntPtrT> number_of_holes = IntPtrMin(mapped_count, length);
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
                    BitcastWordToTagged(frame), SmiFromIntPtr(length),
                    SmiFromIntPtr(mapped_count));
  }
}

TF_BUILTIN(ReturnReceiver, CodeStubAssembler) {
  Return(Parameter(Descriptor::kReceiver));
}

TF_BUILTIN(DebugBreakTrampoline, CodeStubAssembler) {
  Label tailcall_to_shared(this);
  TNode<Context> context = CAST(Parameter(BuiltinDescriptor::kContext));
  TNode<Object> new_target = CAST(Parameter(BuiltinDescriptor::kNewTarget));
  TNode<Int32T> arg_count =
      UncheckedCast<Int32T>(Parameter(BuiltinDescriptor::kArgumentsCount));
  TNode<JSFunction> function = CAST(LoadFromFrame(
      StandardFrameConstants::kFunctionOffset, MachineType::TaggedPointer()));

  // Check break-at-entry flag on the debug info.
  TNode<SharedFunctionInfo> shared =
      CAST(LoadObjectField(function, JSFunction::kSharedFunctionInfoOffset));
  TNode<Object> maybe_debug_info =
      LoadObjectField(shared, SharedFunctionInfo::kDebugInfoOffset);
  GotoIf(TaggedIsSmi(maybe_debug_info), &tailcall_to_shared);

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
  // Use the ConstructTrampolineDescriptor because it passes new.target too in
  // case this is called during construct.
  CSA_ASSERT(this, IsCode(code));
  ConstructTrampolineDescriptor descriptor(isolate());
  TailCallStub(descriptor, code, context, function, new_target, arg_count);
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
    mask = TruncateIntPtrToInt32(mask);

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
    mask = TruncateIntPtrToInt32(mask);
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

class DeletePropertyBaseAssembler : public AccessorAssembler {
 public:
  explicit DeletePropertyBaseAssembler(compiler::CodeAssemblerState* state)
      : AccessorAssembler(state) {}

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
      InvalidateValidityCellIfPrototype(receiver_map);

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

  void RunPromiseHook(Runtime::FunctionId id, TNode<Context> context,
                      SloppyTNode<HeapObject> payload);

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

void InternalBuiltinsAssembler::RunPromiseHook(
    Runtime::FunctionId id, TNode<Context> context,
    SloppyTNode<HeapObject> payload) {
  Label hook(this, Label::kDeferred), done_hook(this);
  Branch(IsPromiseHookEnabledOrDebugIsActive(), &hook, &done_hook);
  BIND(&hook);
  {
    CallRuntime(id, context, payload);
    Goto(&done_hook);
  }
  BIND(&done_hook);
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
  // Load the current context from the isolate.
  TNode<Context> current_context = GetCurrentContext();

  Label init_queue_loop(this);
  Goto(&init_queue_loop);
  BIND(&init_queue_loop);
  {
    TVARIABLE(IntPtrT, index, IntPtrConstant(0));
    Label loop(this, &index), loop_next(this);

    TNode<IntPtrT> num_tasks = GetPendingMicrotaskCount();
    ReturnIf(IntPtrEqual(num_tasks, IntPtrConstant(0)), UndefinedConstant());

    TNode<FixedArray> queue = GetMicrotaskQueue();

    CSA_ASSERT(this, IntPtrGreaterThanOrEqual(
                         LoadAndUntagFixedArrayBaseLength(queue), num_tasks));
    CSA_ASSERT(this, IntPtrGreaterThan(num_tasks, IntPtrConstant(0)));

    SetPendingMicrotaskCount(IntPtrConstant(0));
    SetMicrotaskQueue(EmptyFixedArrayConstant());

    Goto(&loop);
    BIND(&loop);
    {
      TNode<HeapObject> microtask =
          CAST(LoadFixedArrayElement(queue, index.value()));
      index = IntPtrAdd(index.value(), IntPtrConstant(1));

      CSA_ASSERT(this, TaggedIsNotSmi(microtask));

      TNode<Map> microtask_map = LoadMap(microtask);
      TNode<Int32T> microtask_type = LoadMapInstanceType(microtask_map);

      VARIABLE(var_exception, MachineRepresentation::kTagged,
               TheHoleConstant());
      Label if_exception(this, Label::kDeferred);
      Label is_callable(this), is_callback(this),
          is_promise_fulfill_reaction_job(this),
          is_promise_reject_reaction_job(this),
          is_promise_resolve_thenable_job(this),
          is_unreachable(this, Label::kDeferred);

      int32_t case_values[] = {CALLABLE_TASK_TYPE, CALLBACK_TASK_TYPE,
                               PROMISE_FULFILL_REACTION_JOB_TASK_TYPE,
                               PROMISE_REJECT_REACTION_JOB_TASK_TYPE,
                               PROMISE_RESOLVE_THENABLE_JOB_TASK_TYPE};
      Label* case_labels[] = {
          &is_callable, &is_callback, &is_promise_fulfill_reaction_job,
          &is_promise_reject_reaction_job, &is_promise_resolve_thenable_job};
      static_assert(arraysize(case_values) == arraysize(case_labels), "");
      Switch(microtask_type, &is_unreachable, case_values, case_labels,
             arraysize(case_labels));

      BIND(&is_callable);
      {
        // Enter the context of the {microtask}.
        TNode<Context> microtask_context =
            LoadObjectField<Context>(microtask, CallableTask::kContextOffset);
        TNode<Context> native_context = LoadNativeContext(microtask_context);

        CSA_ASSERT(this, IsNativeContext(native_context));
        EnterMicrotaskContext(microtask_context);
        SetCurrentContext(native_context);

        TNode<JSReceiver> callable = LoadObjectField<JSReceiver>(
            microtask, CallableTask::kCallableOffset);
        Node* const result = CallJS(
            CodeFactory::Call(isolate(), ConvertReceiverMode::kNullOrUndefined),
            microtask_context, callable, UndefinedConstant());
        GotoIfException(result, &if_exception, &var_exception);
        LeaveMicrotaskContext();
        SetCurrentContext(current_context);
        Goto(&loop_next);
      }

      BIND(&is_callback);
      {
        Node* const microtask_callback =
            LoadObjectField(microtask, CallbackTask::kCallbackOffset);
        Node* const microtask_data =
            LoadObjectField(microtask, CallbackTask::kDataOffset);

        // If this turns out to become a bottleneck because of the calls
        // to C++ via CEntryStub, we can choose to speed them up using a
        // similar mechanism that we use for the CallApiFunction stub,
        // except that calling the MicrotaskCallback is even easier, since
        // it doesn't accept any tagged parameters, doesn't return a value
        // and ignores exceptions.
        //
        // But from our current measurements it doesn't seem to be a
        // serious performance problem, even if the microtask is full
        // of CallHandlerTasks (which is not a realistic use case anyways).
        Node* const result =
            CallRuntime(Runtime::kRunMicrotaskCallback, current_context,
                        microtask_callback, microtask_data);
        GotoIfException(result, &if_exception, &var_exception);
        Goto(&loop_next);
      }

      BIND(&is_promise_resolve_thenable_job);
      {
        // Enter the context of the {microtask}.
        TNode<Context> microtask_context = LoadObjectField<Context>(
            microtask, PromiseResolveThenableJobTask::kContextOffset);
        TNode<Context> native_context = LoadNativeContext(microtask_context);
        CSA_ASSERT(this, IsNativeContext(native_context));
        EnterMicrotaskContext(microtask_context);
        SetCurrentContext(native_context);

        Node* const promise_to_resolve = LoadObjectField(
            microtask, PromiseResolveThenableJobTask::kPromiseToResolveOffset);
        Node* const then = LoadObjectField(
            microtask, PromiseResolveThenableJobTask::kThenOffset);
        Node* const thenable = LoadObjectField(
            microtask, PromiseResolveThenableJobTask::kThenableOffset);

        Node* const result =
            CallBuiltin(Builtins::kPromiseResolveThenableJob, native_context,
                        promise_to_resolve, thenable, then);
        GotoIfException(result, &if_exception, &var_exception);
        LeaveMicrotaskContext();
        SetCurrentContext(current_context);
        Goto(&loop_next);
      }

      BIND(&is_promise_fulfill_reaction_job);
      {
        // Enter the context of the {microtask}.
        TNode<Context> microtask_context = LoadObjectField<Context>(
            microtask, PromiseReactionJobTask::kContextOffset);
        TNode<Context> native_context = LoadNativeContext(microtask_context);
        CSA_ASSERT(this, IsNativeContext(native_context));
        EnterMicrotaskContext(microtask_context);
        SetCurrentContext(native_context);

        Node* const argument =
            LoadObjectField(microtask, PromiseReactionJobTask::kArgumentOffset);
        Node* const handler =
            LoadObjectField(microtask, PromiseReactionJobTask::kHandlerOffset);
        Node* const payload =
            LoadObjectField(microtask, PromiseReactionJobTask::kPayloadOffset);

        // Run the promise before/debug hook if enabled.
        RunPromiseHook(Runtime::kPromiseHookBefore, microtask_context, payload);

        Node* const result =
            CallBuiltin(Builtins::kPromiseFulfillReactionJob, microtask_context,
                        argument, handler, payload);
        GotoIfException(result, &if_exception, &var_exception);

        // Run the promise after/debug hook if enabled.
        RunPromiseHook(Runtime::kPromiseHookAfter, microtask_context, payload);

        LeaveMicrotaskContext();
        SetCurrentContext(current_context);
        Goto(&loop_next);
      }

      BIND(&is_promise_reject_reaction_job);
      {
        // Enter the context of the {microtask}.
        TNode<Context> microtask_context = LoadObjectField<Context>(
            microtask, PromiseReactionJobTask::kContextOffset);
        TNode<Context> native_context = LoadNativeContext(microtask_context);
        CSA_ASSERT(this, IsNativeContext(native_context));
        EnterMicrotaskContext(microtask_context);
        SetCurrentContext(native_context);

        Node* const argument =
            LoadObjectField(microtask, PromiseReactionJobTask::kArgumentOffset);
        Node* const handler =
            LoadObjectField(microtask, PromiseReactionJobTask::kHandlerOffset);
        Node* const payload =
            LoadObjectField(microtask, PromiseReactionJobTask::kPayloadOffset);

        // Run the promise before/debug hook if enabled.
        RunPromiseHook(Runtime::kPromiseHookBefore, microtask_context, payload);

        Node* const result =
            CallBuiltin(Builtins::kPromiseRejectReactionJob, microtask_context,
                        argument, handler, payload);
        GotoIfException(result, &if_exception, &var_exception);

        // Run the promise after/debug hook if enabled.
        RunPromiseHook(Runtime::kPromiseHookAfter, microtask_context, payload);

        LeaveMicrotaskContext();
        SetCurrentContext(current_context);
        Goto(&loop_next);
      }

      BIND(&is_unreachable);
      Unreachable();

      BIND(&if_exception);
      {
        // Report unhandled exceptions from microtasks.
        CallRuntime(Runtime::kReportMessage, current_context,
                    var_exception.value());
        LeaveMicrotaskContext();
        SetCurrentContext(current_context);
        Goto(&loop_next);
      }

      BIND(&loop_next);
      Branch(IntPtrLessThan(index.value(), num_tasks), &loop, &init_queue_loop);
    }
  }
}

TF_BUILTIN(AbortJS, CodeStubAssembler) {
  Node* message = Parameter(Descriptor::kObject);
  Node* reason = SmiConstant(0);
  TailCallRuntime(Runtime::kAbortJS, reason, message);
}

}  // namespace internal
}  // namespace v8
