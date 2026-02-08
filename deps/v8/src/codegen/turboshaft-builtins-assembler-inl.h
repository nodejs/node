// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_TURBOSHAFT_BUILTINS_ASSEMBLER_INL_H_
#define V8_CODEGEN_TURBOSHAFT_BUILTINS_ASSEMBLER_INL_H_

#include <iterator>

#include "src/common/globals.h"
#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/access-builder.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/machine-lowering-reducer-inl.h"
#include "src/compiler/turboshaft/operation-matcher.h"
#include "src/compiler/turboshaft/runtime-call-descriptors.h"
#include "src/compiler/turboshaft/sidetable.h"
#include "src/compiler/turboshaft/string-view.h"
#include "src/compiler/turboshaft/typeswitch.h"
#include "src/interpreter/bytecode-register.h"
#include "src/objects/elements-kind.h"
#include "src/objects/objects-inl.h"

#define DEFINE_TURBOSHAFT_ALIASES()                                            \
  template <typename T>                                                        \
  using V = compiler::turboshaft::V<T>;                                        \
  template <typename T>                                                        \
  using ConstOrV = compiler::turboshaft::ConstOrV<T>;                          \
  template <typename T>                                                        \
  using OptionalV = compiler::turboshaft::OptionalV<T>;                        \
  template <typename... Ts>                                                    \
  using Label = compiler::turboshaft::Label<Ts...>;                            \
  template <typename... Ts>                                                    \
  using LoopLabel = compiler::turboshaft::LoopLabel<Ts...>;                    \
  template <typename... Args>                                                  \
  using Tuple = compiler::turboshaft::Tuple<Args...>;                          \
  using Block = compiler::turboshaft::Block;                                   \
  using OpIndex = compiler::turboshaft::OpIndex;                               \
  using OptionalOpIndex = compiler::turboshaft::OptionalOpIndex;               \
  using None = compiler::turboshaft::None;                                     \
  using Word32 = compiler::turboshaft::Word32;                                 \
  using Word64 = compiler::turboshaft::Word64;                                 \
  using WordPtr = compiler::turboshaft::WordPtr;                               \
  using Float32 = compiler::turboshaft::Float32;                               \
  using Float64 = compiler::turboshaft::Float64;                               \
  using Any = compiler::turboshaft::Any;                                       \
  using RegisterRepresentation = compiler::turboshaft::RegisterRepresentation; \
  using MemoryRepresentation = compiler::turboshaft::MemoryRepresentation;     \
  using BuiltinCallDescriptor =                                                \
      compiler::turboshaft::deprecated::BuiltinCallDescriptor;                 \
  using AccessBuilderTS = compiler::turboshaft::AccessBuilderTS;               \
  template <typename T>                                                        \
  using Uninitialized = compiler::turboshaft::Uninitialized<T>;                \
  using StringView = compiler::turboshaft::StringView;                         \
  template <typename T>                                                        \
  using Sequence = compiler::turboshaft::Sequence<T>;                          \
  template <typename... Iterables>                                             \
  using Zip = compiler::turboshaft::Zip<Iterables...>;                         \
  using StoreOp = compiler::turboshaft::StoreOp;                               \
  template <typename A>                                                        \
  using TypeswitchBuilder = compiler::turboshaft::TypeswitchBuilder<A>;        \
  template <typename C, typename F>                                            \
  using HeapObjectField = compiler::turboshaft::HeapObjectField<C, F>;         \
  using builtin = compiler::turboshaft::builtin;                               \
  using runtime = compiler::turboshaft::runtime;

#define BUILTIN_REDUCER(name)          \
  TURBOSHAFT_REDUCER_BOILERPLATE(name) \
  DEFINE_TURBOSHAFT_ALIASES()

namespace v8::internal {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

enum IsKnownTaggedPointer { kNo, kYes };

namespace detail {

#ifdef DEBUG
inline bool IsValidArgumentCountFor(const CallInterfaceDescriptor& descriptor,
                                    size_t argument_count) {
  size_t parameter_count = descriptor.GetParameterCount();
  if (descriptor.AllowVarArgs()) {
    return argument_count >= parameter_count;
  } else {
    return argument_count == parameter_count;
  }
}
#endif  // DEBUG

// TODO(nicohartmann): Rename once CSA is (mostly) gone.
template <typename Assembler>
class BuiltinArgumentsTS {
 public:
  DEFINE_TURBOSHAFT_ALIASES()

  auto& Asm() const { return *assembler_; }

  // |argc| specifies the number of arguments passed to the builtin.
  template <typename T>
  BuiltinArgumentsTS(Assembler* assembler, V<T> argc,
                     OptionalV<WordPtr> fp = {})
      : assembler_(assembler) {
    if constexpr (std::is_same_v<T, WordPtr>) {
      argc_ = argc;
    } else {
      if constexpr (std::is_same_v<T, Word32>) {
        DCHECK((std::is_same_v<WordPtr, Word64>));
        argc_ = assembler_->ChangeInt32ToInt64(argc);
      } else {
        static_assert(std::is_same_v<T, Word64>);
        DCHECK((std::is_same_v<WordPtr, Word32>));
        argc_ = assembler_->TruncateWord64ToWord32(argc);
      }
    }
    if (fp.has_value()) {
      fp_ = fp.value();
    } else {
      fp_ = __ FramePointer();
    }
    const intptr_t offset =
        (StandardFrameConstants::kFixedSlotCountAboveFp + 1) *
        kSystemPointerSize;
    // base_ points to the first argument, not the receiver
    // whether present or not.
    base_ = __ WordPtrAdd(fp_, offset);
  }

  V<WordPtr> GetLengthWithReceiver() const { return argc_; }

  V<WordPtr> GetLengthWithoutReceiver() const {
    return __ WordPtrSub(argc_, kJSArgcReceiverSlots);
  }

  V<Object> AtIndex(ConstOrV<WordPtr> index) const {
    TSA_DCHECK(this, __ UintPtrLessThan(index, GetLengthWithoutReceiver()));
    return V<Object>::Cast(
        __ LoadOffHeap(AtIndexPtr(index), MemoryRepresentation::AnyTagged()));
  }

  class Iterator {
   public:
    using iterator_type = V<WordPtr>;
    using value_type = V<Object>;

    // {end} is the iterator-typical exclusive one past the last element.
    Iterator(const BuiltinArgumentsTS* args, ConstOrV<WordPtr> begin_index,
             ConstOrV<WordPtr> end_index)
        : args_(args), begin_index_(begin_index), end_index_(end_index) {}

    template <typename A>
    iterator_type Begin(A& assembler) {
      DCHECK(!end_offset_.valid());
      // Pre-compute the end offset.
      end_offset_ = args_->AtIndexPtr(assembler.resolve(end_index_));
      return args_->AtIndexPtr(assembler.resolve(begin_index_));
    }

    template <typename A>
    OptionalV<Word32> IsEnd(A& assembler,
                            iterator_type current_iterator) const {
      return assembler.UintPtrLessThanOrEqual(end_offset_, current_iterator);
    }

    template <typename A>
    iterator_type Advance(A& assembler, iterator_type current_iterator) const {
      return assembler.WordPtrAdd(
          current_iterator, ElementsKindToByteSize(SYSTEM_POINTER_ELEMENTS));
    }

    template <typename A>
    value_type Dereference(A& assembler, iterator_type current_iterator) const {
      return V<Object>::Cast(assembler.LoadOffHeap(
          current_iterator, MemoryRepresentation::AnyTagged()));
    }

   private:
    const BuiltinArgumentsTS* args_;
    ConstOrV<WordPtr> begin_index_;
    ConstOrV<WordPtr> end_index_;
    V<WordPtr> end_offset_;
  };

  Iterator Range(ConstOrV<WordPtr> begin, ConstOrV<WordPtr> end) const {
    return Iterator(this, begin, end);
  }

  Iterator Range(ConstOrV<WordPtr> begin) const {
    return Iterator(this, begin, GetLengthWithoutReceiver());
  }

  Iterator Range() const {
    return Iterator(this, 0, GetLengthWithoutReceiver());
  }

 private:
  V<WordPtr> AtIndexPtr(ConstOrV<WordPtr> index) const {
    V<WordPtr> offset =
        assembler_->ElementOffsetFromIndex(index, SYSTEM_POINTER_ELEMENTS, 0);
    return __ WordPtrAdd(base_, offset);
  }

  Assembler* assembler_;
  V<WordPtr> argc_;
  V<WordPtr> fp_;
  V<WordPtr> base_;
};

// Deduction guide.
template <typename A, typename T>
BuiltinArgumentsTS(
    A*, compiler::turboshaft::V<T>,
    compiler::turboshaft::OptionalV<compiler::turboshaft::WordPtr>)
    -> BuiltinArgumentsTS<A>;

}  // namespace detail

template <typename Next>
class FeedbackCollectorReducer : public Next {
 public:
  BUILTIN_REDUCER(FeedbackCollector)

  using FeedbackVectorOrUndefined = Union<FeedbackVector, Undefined>;
  static constexpr bool HasFeedbackCollector() { return true; }

  void CombineFeedback(int additional_feedback) {
    __ CodeComment("CombineFeedback");
    feedback_ = __ SmiBitwiseOr(
        feedback_, __ SmiConstant(Smi::FromInt(additional_feedback)));
  }

  void OverwriteFeedback(int new_feedback) {
    __ CodeComment("OverwriteFeedback");
    feedback_ = __ SmiConstant(Smi::FromInt(new_feedback));
  }

  // TODO(nicohartmann): Generalize this to allow nested scopes to set this
  // feedback.
  void CombineFeedbackOnException(int additional_feedback) {
    feedback_on_exception_ = __ SmiConstant(Smi::FromInt(additional_feedback));
  }

  void CombineExceptionFeedback() {
    feedback_ = __ SmiBitwiseOr(feedback_, feedback_on_exception_);
  }

  V<Word32> FeedbackIs(int checked_feedback) {
    return __ SmiEqual(feedback_,
                       __ SmiConstant(Smi::FromInt(checked_feedback)));
  }

  V<Word32> FeedbackHas(int checked_feedback) {
    V<Smi> mask = __ SmiConstant(Smi::FromInt(checked_feedback));
    return __ SmiEqual(__ SmiBitwiseAnd(feedback_, mask), mask);
  }

  V<FeedbackVectorOrUndefined> LoadFeedbackVector() {
    return V<FeedbackVectorOrUndefined>::Cast(
        __ LoadRegister(interpreter::Register::feedback_vector()));
  }

  V<WordPtr> LoadFeedbackVectorLength(V<FeedbackVector> feedback_vector) {
    V<Word32> length = __ LoadField(feedback_vector,
                                    AccessBuilderTS::ForFeedbackVectorLength());
    return ChangePositiveInt32ToIntPtr(length);
  }

  V<MaybeObject> LoadFeedbackVectorSlot(V<FeedbackVector> feedback_vector,
                                        V<WordPtr> slot,
                                        int additional_offset = 0) {
    __ CodeComment("LoadFeedbackVectorSlot");
    int32_t header_size =
        FeedbackVector::kRawFeedbackSlotsOffset + additional_offset;
    V<WordPtr> offset =
        __ ElementOffsetFromIndex(slot, HOLEY_ELEMENTS, header_size);
    TSA_SLOW_DCHECK(this, IsOffsetInBounds(
                              offset, LoadFeedbackVectorLength(feedback_vector),
                              FeedbackVector::kHeaderSize));
    return V<MaybeObject>::Cast(
        __ Load(feedback_vector, offset,
                compiler::turboshaft::LoadOp::Kind::TaggedBase(),
                MemoryRepresentation::AnyTagged()));
  }

  void StoreFeedbackVectorSlot(
      V<FeedbackVector> feedback_vector, V<WordPtr> slot, V<Object> value,
      WriteBarrierMode barrier_mode = UPDATE_WRITE_BARRIER,
      int additional_offset = 0) {
    __ CodeComment("StoreFeedbackVectorSlot");
    DCHECK(IsAligned(additional_offset, kTaggedSize));
    int header_size =
        FeedbackVector::kRawFeedbackSlotsOffset + additional_offset;
    V<WordPtr> offset =
        __ ElementOffsetFromIndex(slot, HOLEY_ELEMENTS, header_size);
    TSA_DCHECK(this, IsOffsetInBounds(offset,
                                      LoadFeedbackVectorLength(feedback_vector),
                                      FeedbackVector::kHeaderSize));
    switch (barrier_mode) {
      case SKIP_WRITE_BARRIER: {
        __ Store(feedback_vector, offset, value,
                 compiler::turboshaft::StoreOp::Kind::TaggedBase(),
                 MemoryRepresentation::AnyTagged(),
                 compiler::WriteBarrierKind::kNoWriteBarrier);
        return;
      }
      case SKIP_WRITE_BARRIER_SCOPE:
      case SKIP_WRITE_BARRIER_FOR_GC:
      case UNSAFE_SKIP_WRITE_BARRIER:
      case UPDATE_WRITE_BARRIER:
        UNIMPLEMENTED();
      case UPDATE_EPHEMERON_KEY_WRITE_BARRIER:
        UNREACHABLE();
    }
  }

  void SetFeedbackSlot(V<WordPtr> slot_id) { slot_id_ = slot_id; }
  void SetFeedbackVector(V<FeedbackVector> feedback_vector) {
    TSA_DCHECK(this, IsFeedbackVector(feedback_vector));
    maybe_feedback_vector_ = feedback_vector;
    feedback_ = __ SmiConstant(Smi::FromInt(0));
    feedback_on_exception_ = feedback_.Get();
  }

  void LoadFeedbackVectorOrUndefinedIfJitless() {
#ifndef V8_JITLESS
    maybe_feedback_vector_ = LoadFeedbackVector();
#else
    maybe_feedback_vector_ = __ UndefinedConstant();
#endif
    feedback_ = __ SmiConstant(Smi::FromInt(0));
    feedback_on_exception_ = feedback_.Get();
  }

  static constexpr UpdateFeedbackMode DefaultUpdateFeedbackMode() {
#ifndef V8_JITLESS
    return UpdateFeedbackMode::kOptionalFeedback;
#else
    return UpdateFeedbackMode::kNoFeedback;
#endif  // !V8_JITLESS
  }

  void UpdateFeedback() {
    __ CodeComment("UpdateFeedback");
    if (mode_ == UpdateFeedbackMode::kNoFeedback) {
#ifdef V8_JITLESS
      TSA_DCHECK(this, __ IsUndefined(maybe_feedback_vector_));
      return;
#else
      UNREACHABLE();
#endif  // !V8_JITLESS
    }

    Label<> done(this);
    if (mode_ == UpdateFeedbackMode::kOptionalFeedback) {
      GOTO_IF(__ IsUndefined(maybe_feedback_vector_), done);
    } else {
      DCHECK_EQ(mode_, UpdateFeedbackMode::kGuaranteedFeedback);
    }

    V<FeedbackVector> feedback_vector =
        V<FeedbackVector>::Cast(maybe_feedback_vector_);

    V<MaybeObject> feedback_element =
        LoadFeedbackVectorSlot(feedback_vector, slot_id_);
    V<Smi> previous_feedback = V<Smi>::Cast(feedback_element);
    V<Smi> combined_feedback = __ SmiBitwiseOr(previous_feedback, feedback_);
    IF_NOT (__ SmiEqual(previous_feedback, combined_feedback)) {
      StoreFeedbackVectorSlot(feedback_vector, slot_id_, combined_feedback,
                              SKIP_WRITE_BARRIER);
      // TODO(nicohartmann):
      // ReportFeedbackUpdate(maybe_feedback_vector_, slot_id_,
      // "UpdateFeedback");
    }
    GOTO(done);

    BIND(done);
  }

  V<Smi> SmiBitwiseOr(V<Smi> a, V<Smi> b) {
    return __ BitcastWordPtrToSmi(__ WordPtrBitwiseOr(
        __ BitcastSmiToWordPtr(a), __ BitcastSmiToWordPtr(b)));
  }

  V<Smi> SmiBitwiseAnd(V<Smi> a, V<Smi> b) {
    return __ BitcastWordPtrToSmi(__ WordPtrBitwiseAnd(
        __ BitcastSmiToWordPtr(a), __ BitcastSmiToWordPtr(b)));
  }

  V<WordPtr> ChangePositiveInt32ToIntPtr(V<Word32> input) {
    TSA_DCHECK(this, __ Int32LessThanOrEqual(0, input));
    return __ ChangeUint32ToUintPtr(input);
  }

  V<Word32> IsFeedbackVector(V<HeapObject> heap_object) {
    V<Map> map = __ LoadMapField(heap_object);
    return __ IsFeedbackVectorMap(map);
  }

  V<Word32> IsOffsetInBounds(V<WordPtr> offset, V<WordPtr> length,
                             int header_size,
                             ElementsKind kind = HOLEY_ELEMENTS) {
    // Make sure we point to the last field.
    int element_size = 1 << ElementsKindToShiftSize(kind);
    int correction = header_size - element_size;
    V<WordPtr> last_offset =
        __ ElementOffsetFromIndex(length, kind, correction);
    return __ IntPtrLessThanOrEqual(offset, last_offset);
  }

  using ReturnOp = compiler::turboshaft::ReturnOp;
  using OperationStorageSlot = compiler::turboshaft::OperationStorageSlot;

  V<None> REDUCE(Return)(V<Word32> pop_count,
                         base::Vector<const OpIndex> return_values,
                         bool spill_caller_frame_slots) {
    // Store feedback first, before we return from the function.
    UpdateFeedback();
    return Next::ReduceReturn(pop_count, return_values,
                              spill_caller_frame_slots);
  }

 private:
  V<FeedbackVectorOrUndefined> maybe_feedback_vector_;
  V<WordPtr> slot_id_;
  compiler::turboshaft::Var<Smi, assembler_t> feedback_{this};
  compiler::turboshaft::Var<Smi, assembler_t> feedback_on_exception_{this};
  UpdateFeedbackMode mode_ = DefaultUpdateFeedbackMode();
};

template <typename Next>
class NoFeedbackCollectorReducer : public Next {
 public:
  BUILTIN_REDUCER(NoFeedbackCollector)

  static constexpr bool HasFeedbackCollector() { return false; }

  void CombineFeedback(int additional_feedback) {}

  void OverwriteFeedback(int new_feedback) {}

  V<Word32> FeedbackIs(int checked_feedback) { UNREACHABLE(); }
  V<Word32> FeedbackHas(int checked_feedbac) { UNREACHABLE(); }

  void UpdateFeedback() {}
  void CombineExceptionFeedback() {}
};

template <typename Next>
class BuiltinsReducer : public Next {
 public:
  BUILTIN_REDUCER(Builtins)

  using BuiltinArgumentsTS = detail::BuiltinArgumentsTS<assembler_t>;

  void EmitBuiltinProlog(Builtin builtin_id) {
    // Bind the entry block.
    __ Bind(__ NewBlock());
    // Eagerly emit all parameters such that they are guaranteed to be in the
    // entry block (assembler will cache them).
    const compiler::CallDescriptor* desc =
        __ data() -> builtin_call_descriptor();
    for (int i = 0; i < static_cast<int>(desc->ParameterCount()); ++i) {
      __ Parameter(i, RegisterRepresentation::FromMachineType(
                          desc->GetParameterType(i)));
    }
    // TODO(nicohartmann): CSA tracks some debug information here.
    // Emit stack check.
    if (Builtins::KindOf(builtin_id) == Builtins::TFJ_TSA) {
      __ PerformStackCheck(__ JSContextParameter());
    }
  }

  void EmitEpilog(Block* catch_block) {
    DCHECK_EQ(__ HasFeedbackCollector(), catch_block != nullptr);
    if (catch_block) {
      // If the handler can potentially throw, we catch the exception here and
      // update the feedback vector before we rethrow the exception.
      if (__ Bind(catch_block)) {
        V<Object> exception = __ CatchBlockBegin();
        __ CombineExceptionFeedback();
        __ UpdateFeedback();
        __ template CallRuntime<runtime::ReThrow>(__ NoContextConstant(),
                                                  {.exception = exception});
        __ Unreachable();
      }
    }
  }

  V<Context> JSContextParameter() {
    return __ template Parameter<Context>(
        compiler::Linkage::GetJSCallContextParamIndex(static_cast<int>(
            __ data()->builtin_call_descriptor()->JSParameterCount())));
  }

  void PerformStackCheck(V<Context> context) {
    __ JSStackCheck(context,
                    OptionalV<compiler::turboshaft::FrameState>::Nullopt(),
                    compiler::turboshaft::JSStackCheckOp::Kind::kBuiltinEntry);
  }

  void PopAndReturn(BuiltinArgumentsTS& arguments,
                    compiler::turboshaft::V<Object> return_value) {
    // PopAndReturn is supposed to be using ONLY in CSA/Torque builtins for
    // dropping ALL JS arguments that are currently located on the stack.
    // The check below ensures that there are no directly accessible stack
    // parameters from current builtin, which implies that the builtin with
    // JS calling convention (TFJ) was created with kDontAdaptArgumentsSentinel.
    // This simplifies semantics of this instruction because in case of presence
    // of directly accessible stack parameters it's impossible to distinguish
    // the following cases:
    // 1) stack parameter is included in JS arguments (and therefore it will be
    //    dropped as a part of 'pop' number of arguments),
    // 2) stack parameter is NOT included in JS arguments (and therefore it
    // should
    //    be dropped in ADDITION to the 'pop' number of arguments).
    // Additionally, in order to simplify assembly code, PopAndReturn is also
    // not allowed in builtins with stub linkage and parameters on stack.
    CHECK_EQ(__ data()->builtin_call_descriptor()->ParameterSlotCount(), 0);
    V<WordPtr> pop_count = arguments.GetLengthWithReceiver();
    std::initializer_list<const OpIndex> temp{return_value};
    __ Return(__ TruncateWordPtrToWord32(pop_count), base::VectorOf(temp));
  }

  V<Word32> TruncateTaggedToWord32(V<Context> context, V<Object> value) {
    Label<Word32> is_number(this);
    TaggedToWord32OrBigIntImpl<Object::Conversion::kToNumber>(
        context, value, IsKnownTaggedPointer::kNo, is_number);

    BIND(is_number, number);
    return number;
  }

  V<Word32> IsBigIntInstanceType(ConstOrV<Word32> instance_type) {
    return InstanceTypeEqual(instance_type, BIGINT_TYPE);
  }
  V<Word32> IsSmallBigInt(V<BigInt> value) { UNIMPLEMENTED(); }
  V<Word32> InstanceTypeEqual(ConstOrV<Word32> instance_type,
                              ConstOrV<Word32> other_instance_type) {
    return __ Word32Equal(instance_type, other_instance_type);
  }

  V<WordPtr> AlignTagged(V<WordPtr> size) {
    return __ WordPtrBitwiseAnd(__ WordPtrAdd(size, kObjectAlignmentMask),
                                ~kObjectAlignmentMask);
  }

  V<WordPtr> ElementOffsetFromIndex(ConstOrV<WordPtr> index, ElementsKind kind,
                                    intptr_t base_size) {
    const int element_size_shift = ElementsKindToShiftSize(kind);
    if (std::optional<intptr_t> constant_index = TryToIntPtrConstant(index)) {
      return __ WordPtrConstant(base_size +
                                (1 << element_size_shift) * (*constant_index));
    }
    if (element_size_shift == 0) {
      return __ WordPtrAdd(base_size, index);
    } else {
      DCHECK_LT(0, element_size_shift);
      return __ WordPtrAdd(base_size,
                           __ WordPtrShiftLeft(index, element_size_shift));
    }
  }

  std::optional<intptr_t> TryToIntPtrConstant(ConstOrV<WordPtr> index) {
    if (index.is_constant()) return index.constant_value();
    intptr_t value;
    if (matcher_.MatchIntegralWordPtrConstant(index.value(), &value)) {
      return value;
    }
    return std::nullopt;
  }

  template <Object::Conversion Conversion>
  void TaggedToWord32OrBigIntImpl(V<Context> context, V<Object> value,
                                  IsKnownTaggedPointer is_known_tagged_pointer,
                                  Label<Word32>& if_number,
                                  Label<BigInt>* if_bigint = nullptr,
                                  Label<BigInt>* if_bigint64 = nullptr) {
    DCHECK_EQ(Conversion == Object::Conversion::kToNumeric,
              if_bigint != nullptr);

    if (is_known_tagged_pointer == IsKnownTaggedPointer::kNo) {
      IF (__ IsSmi(value)) {
        __ CombineFeedback(BinaryOperationFeedback::kSignedSmall);
        GOTO(if_number, __ UntagSmi(V<Smi>::Cast(value)));
      }
    }

    ScopedVar<HeapObject> value_heap_object(this, V<HeapObject>::Cast(value));
    WHILE(1) {
      V<Map> map = __ LoadMapField(value_heap_object);

      IF (__ IsHeapNumberMap(map)) {
        __ CombineFeedback(BinaryOperationFeedback::kNumber);
        V<Float64> value_float64 =
            __ LoadHeapNumberValue(V<HeapNumber>::Cast(value_heap_object));
        GOTO(if_number, __ JSTruncateFloat64ToWord32(value_float64));
      }

      V<Word32> instance_type = __ LoadInstanceTypeField(map);
      if (Conversion == Object::Conversion::kToNumeric) {
        IF (IsBigIntInstanceType(instance_type)) {
          V<BigInt> value_bigint = V<BigInt>::Cast(value_heap_object);
          if (Is64() && if_bigint64) {
            IF (IsSmallBigInt(value_bigint)) {
              __ CombineFeedback(BinaryOperationFeedback::kBigInt64);
              GOTO(*if_bigint64, value_bigint);
            }
          }
          __ CombineFeedback(BinaryOperationFeedback::kBigInt);
          GOTO(*if_bigint, value_bigint);
        }
      }

      // Not HeapNumber (or BigInt if conversion == kToNumeric).
      if (__ HasFeedbackCollector()) {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a Numeric, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        TSA_DCHECK(this, __ FeedbackIs(BinaryOperationFeedback::kNone));
      }
      IF (InstanceTypeEqual(instance_type, ODDBALL_TYPE)) {
        __ OverwriteFeedback(BinaryOperationFeedback::kNumberOrOddball);
        V<Float64> oddball_value =
            __ LoadField(V<Oddball>::Cast(value_heap_object),
                         AccessBuilderTS::ForHeapNumberOrOddballValue());
        GOTO(if_number, __ JSTruncateFloat64ToWord32(oddball_value));
      }

      // Not an oddball either -> convert.
      V<Object> converted_value;
      // TODO(nicohartmann): We have to make sure that we store the feedback if
      // any of those calls throws an exception.
      __ OverwriteFeedback(BinaryOperationFeedback::kAny);

      using Builtin =
          std::conditional_t<Conversion == Object::Conversion::kToNumeric,
                             builtin::NonNumberToNumeric,
                             builtin::NonNumberToNumber>;
      converted_value = __ template CallBuiltin<Builtin>(
          context, {.input = V<JSAnyNotNumber>::Cast(value_heap_object)});

      GOTO_IF(__ IsSmi(converted_value), if_number,
              __ UntagSmi(V<Smi>::Cast(converted_value)));
      value_heap_object = V<HeapObject>::Cast(converted_value);
    }

    __ Unreachable();
  }

  V<Word32> IsFastElementsKind(ConstOrV<Word32> elements_kind) {
    static_assert(FIRST_ELEMENTS_KIND == FIRST_FAST_ELEMENTS_KIND);
    if (elements_kind.is_constant()) {
      return __ Word32Constant(
          elements_kind.constant_value() <= LAST_FAST_ELEMENTS_KIND ? 1 : 0);
    } else {
      return __ Uint32LessThanOrEqual(elements_kind.value(),
                                      LAST_FAST_ELEMENTS_KIND);
    }
  }

  V<Object> LoadContextElementNoCell(V<Context> context,
                                     ConstOrV<WordPtr> index) {
    V<Object> val;
    if (index.is_constant()) {
      val = __ template LoadField<Object>(
          context, AccessBuilderTS::ForContextSlot(index.constant_value()));
    } else {
      val = V<Object>::Cast(__ Load(context, index.value(),
                                    compiler::turboshaft::LoadOp::Kind::Aligned(
                                        compiler::BaseTaggedness::kTaggedBase),
                                    MemoryRepresentation::AnyTagged(),
                                    RegisterRepresentation::Tagged(),
                                    Context::kElementsOffset, kTaggedSizeLog2));
    }
    TSA_DCHECK(this,
               __ Word32BitwiseOr(__ IsTheHole(val),
                                  __ Word32Equal(__ IsContextCell(val), 0)));
    return val;
  }

  V<Map> LoadJSArrayElementsMap(ConstOrV<Word32> elements_kind,
                                V<NativeContext> native_context) {
    ConstOrV<WordPtr> offset;
    if (elements_kind.is_constant()) {
      offset = Context::ArrayMapIndex(
          static_cast<ElementsKind>(elements_kind.constant_value()));
    } else {
      TSA_DCHECK(this, IsFastElementsKind(elements_kind));
      offset = __ WordPtrAdd(Context::FIRST_JS_ARRAY_MAP_SLOT,
                             __ ChangeInt32ToIntPtr(elements_kind));
    }
    return V<Map>::Cast(LoadContextElementNoCell(native_context, offset));
  }

  V<Word32> IsContextCell(V<Object> object) {
    Label<Word32> done(this);
    GOTO_IF(__ IsSmi(object), done, 0);
    V<Map> map = __ LoadMapField(V<HeapObject>::Cast(object));
    GOTO(done, __ IsContextCellMap(map));

    BIND(done, result);
    return result;
  }

  V<Word32> IsJSArrayMap(V<Map> map) {
    return IsJSArrayInstanceType(__ LoadInstanceTypeField(map));
  }

  V<Word32> IsJSArrayInstanceType(V<Word32> instance_type) {
    return __ InstanceTypeEqual(instance_type, JS_ARRAY_TYPE);
  }

  V<MaybeObject> ClearedValue() {
    return V<MaybeObject>(
        __ BitcastWordPtrToTagged(kClearedWeakHeapObjectLower32));
  }
  V<MaybeObject> PrototypeChainInvalidConstant() {
    static_assert(Map::kPrototypeChainInvalid.IsCleared());
    return ClearedValue();
  }

  template <typename T, typename U>
  V<T> Cast(V<U> value, Label<>* otherwise) {
    // TODO(nicohartmann): Implement debug checks.
    Label<T> done(this);

    // TODO(nicohartmann): Maybe we should not emit a full typeswitch here.
    TYPESWITCH(value) {
      CASE_(V<T>, v): {
        GOTO(done, v);
      }
    }

    if (otherwise) {
      GOTO(*otherwise);
    } else {
      __ Unreachable();
    }

    BIND(done, result);
    return result;
  }

  template <typename Desc>
    requires(!Desc::kCanTriggerLazyDeopt)
  auto CallRuntime(compiler::turboshaft::V<Context> context,
                   const Desc::Arguments& args) {
    return Next::template CallRuntime<Desc>(context, args);
  }

  template <typename Desc>
    requires(Desc::kCanTriggerLazyDeopt)
  auto CallRuntime(compiler::turboshaft::V<Context> context,
                   const Desc::Arguments& args) {
    return Next::template CallRuntime<Desc>(
        compiler::turboshaft::OptionalV<
            compiler::turboshaft::FrameState>::Nullopt(),
        context, args, compiler::LazyDeoptOnThrow::kNo);
  }

  template <typename Desc>
    requires(Desc::kNeedsContext && !Desc::kCanTriggerLazyDeopt)
  auto CallBuiltin(compiler::turboshaft::V<Context> context,
                   const Desc::Arguments& args) {
    return Next::template CallBuiltin<Desc>(context, args);
  }

  template <typename Desc>
    requires(!Desc::kNeedsContext && !Desc::kCanTriggerLazyDeopt)
  auto CallBuiltin(const Desc::Arguments& args) {
    return Next::template CallBuiltin<Desc>(args);
  }

  template <typename Desc>
    requires(Desc::kNeedsContext && Desc::kCanTriggerLazyDeopt)
  auto CallBuiltin(compiler::turboshaft::V<Context> context,
                   const Desc::Arguments& args) {
    return Next::template CallBuiltin<Desc>(
        compiler::turboshaft::OptionalV<
            compiler::turboshaft::FrameState>::Nullopt(),
        context, args, compiler::LazyDeoptOnThrow::kNo);
  }

  template <typename Desc>
    requires(!Desc::kNeedsContext && Desc::kCanTriggerLazyDeopt)
  auto CallBuiltin(const Desc::Arguments& args) {
    return Next::template CallBuiltin<Desc>(
        compiler::turboshaft::OptionalV<
            compiler::turboshaft::FrameState>::Nullopt(),
        args, compiler::LazyDeoptOnThrow::kNo);
  }

  V<String> StringConstant(const char* str) {
    Handle<String> internalized_string =
        factory()->InternalizeString(base::OneByteVector(str));
    __ CanonicalizeEmbeddedBuiltinsConstantIfNeeded(internalized_string);
    return V<String>::Cast(__ HeapConstantNoHole(internalized_string));
  }

  V<Number> NumberConstant(double value) {
    int smi_value;
    if (DoubleToSmiInteger(value, &smi_value)) {
      return __ SmiConstant(Smi::FromInt(smi_value));
    }
    // We allocate the heap number constant eagerly at this point instead of
    // deferring allocation to code generation
    // (see AllocateAndInstallRequestedHeapNumbers) since that makes it easier
    // to generate constant lookups for embedded builtins.
    return V<Number>::Cast(__ HeapConstantNoHole(
        isolate()->factory()->NewHeapNumberForCodeAssembler(value)));
  }

  void DCheckReceiver(ConvertReceiverMode mode, V<JSAny> receiver) {
    switch (mode) {
      case ConvertReceiverMode::kNullOrUndefined:
        TSA_DCHECK(this, __ Word32BitwiseOr(__ IsNull(receiver),
                                            __ IsUndefined(receiver)));
        break;
      case ConvertReceiverMode::kNotNullOrUndefined:
        TSA_DCHECK(this, __ Word32Equal(__ IsNull(receiver), 0));
        TSA_DCHECK(this, __ Word32Equal(__ IsUndefined(receiver), 0));
        break;
      case ConvertReceiverMode::kAny:
        break;
    }
  }

  template <typename Callable, typename... Args>
  V<JSAny> Call(V<Context> context, V<Callable> callable,
                ConvertReceiverMode mode, V<JSAny> receiver, V<Args>... args) {
    static_assert(is_subtype_v<Callable, Object>);
    static_assert(!is_subtype_v<Callable, JSFunction>,
                  "Use CallFunction() when the callable is a JSFunction.");

    Handle<HeapObject> tagged;
    if (__ matcher().MatchHeapConstant(receiver, &tagged) &&
        (IsUndefined(*tagged) || IsNull(*tagged))) {
      DCHECK_NE(mode, ConvertReceiverMode::kNotNullOrUndefined);
      return CallJS(Builtins::Call(ConvertReceiverMode::kNullOrUndefined),
                    context, callable, receiver, args...);
    }
    DCheckReceiver(mode, receiver);
    return CallJS(Builtins::Call(mode), context, callable, receiver, args...);
  }

  template <typename Callable, typename... Args>
  V<JSAny> Call(V<Context> context, V<Callable> callable,
                V<JSReceiver> receiver, V<Args>... args) {
    return Call(context, callable, ConvertReceiverMode::kNotNullOrUndefined,
                receiver, args...);
  }

  template <typename Callable, typename... Args>
  V<JSAny> Call(V<Context> context, V<Callable> callable, V<JSAny> receiver,
                V<Args>... args) {
    return Call(context, callable, ConvertReceiverMode::kAny, receiver,
                args...);
  }

  template <typename... Args>
  V<JSAny> CallJS(Builtin builtin, V<Context> context, V<Object> function,
                  V<JSAny> receiver, V<Args>... args) {
    DCHECK(Builtins::IsAnyCall(builtin));
#if V8_ENABLE_WEBASSEMBLY
    // Unimplemented. Add code for switching to the central stack here if
    // needed. See {CallBuiltin} for example.
    DCHECK_IMPLIES(__ data()->info()->IsWasmBuiltin(),
                   wasm::BuiltinLookup::IsWasmBuiltinId(builtin));
#endif
    Callable callable = Builtins::CallableFor(isolate(), builtin);
    int argc = JSParameterCount(static_cast<int>(sizeof...(args)));
    V<Word32> arity = __ Word32Constant(argc);
    V<Code> target = __ HeapConstantNoHole(callable.code());
    return V<JSAny>::Cast(CallJSStubImpl(callable.descriptor(), target, context,
                                         function, OptionalV<Object>::Nullopt(),
                                         arity, OptionalOpIndex::Nullopt(),
                                         {receiver, args...}));
  }

  V<Any> CallJSStubImpl(const CallInterfaceDescriptor& descriptor,
                        V<Code> target, V<Object> context, V<Object> function,
                        OptionalV<Object> new_target, V<Word32> arity,
                        compiler::turboshaft::OptionalOpIndex dispatch_handle,
                        std::initializer_list<OpIndex> args) {
    constexpr size_t kMaxNumArgs = 10;
    DCHECK_GE(kMaxNumArgs, args.size());
    base::SmallVector<OpIndex, kMaxNumArgs + 6> inputs;

    inputs.push_back(function);
    if (new_target.has_value()) inputs.push_back(new_target.value());
    inputs.push_back(arity);
#ifdef V8_JS_LINKAGE_INCLUDES_DISPATCH_HANDLE
    if (dispatch_handle.has_value()) inputs.push_back(dispatch_handle.value());
#endif
    for (OpIndex arg : args) inputs.push_back(arg);
    // Context argument is implicit so isn't counted.
    DCHECK(detail::IsValidArgumentCountFor(descriptor, inputs.size()));
    if (descriptor.HasContextParameter()) {
      inputs.push_back(context);
    }

    return CallStubN(StubCallMode::kCallCodeObject, descriptor, target,
                     base::VectorOf(inputs));
  }

  V<Any> CallStubN(StubCallMode call_mode,
                   const CallInterfaceDescriptor& descriptor, V<Code> target,
                   base::Vector<const OpIndex> inputs) {
    DCHECK(call_mode == StubCallMode::kCallCodeObject ||
           call_mode == StubCallMode::kCallBuiltinPointer);
    constexpr int kTargetCount = 1;

    // implicit nodes are target and optionally context.
    int implicit_nodes = descriptor.HasContextParameter() ? 2 : 1;
    DCHECK_LE(implicit_nodes, inputs.size());
    int argc = static_cast<int>(inputs.size()) + kTargetCount - implicit_nodes;
    DCHECK(detail::IsValidArgumentCountFor(descriptor, argc));
    // Extra arguments not mentioned in the descriptor are passed on the stack.
    int stack_parameter_count = argc - descriptor.GetRegisterParameterCount();
    DCHECK_LE(descriptor.GetStackParameterCount(), stack_parameter_count);

    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), descriptor, stack_parameter_count,
        compiler::CallDescriptor::kNoFlags, compiler::Operator::kNoProperties,
        call_mode);

    return __ AssemblerOpInterface::Call(
        target, OptionalV<compiler::turboshaft::FrameState>::Nullopt(),
        base::VectorOf(inputs),
        compiler::turboshaft::TSCallDescriptor::Create(
            call_descriptor, compiler::CanThrow::kYes,
            compiler::LazyDeoptOnThrow::kNo, __ graph_zone()));
  }

  void ThrowTypeError(V<Context> context, MessageTemplate message,
                      char const* arg0 = nullptr, char const* arg1 = nullptr) {
    using ThrowTypeError = runtime::ThrowTypeError;
    ThrowTypeError::Arguments args;
    args.template_index = __ SmiConstant(Smi::FromEnum(message));
    if (arg0) {
      args.arg0 = StringConstant(arg0);
      if (arg1) {
        args.arg1 = StringConstant(arg1);
      }
    }

    __ template CallRuntime<ThrowTypeError>(context, args);
    __ Unreachable();
  }

  void ThrowTypeError(V<Context> context, MessageTemplate message,
                      OptionalV<Object> arg0, OptionalV<Object> arg1,
                      OptionalV<Object> arg2) {
    using ThrowTypeError = runtime::ThrowTypeError;
    DCHECK_IMPLIES(arg2.has_value(), arg1.has_value());
    DCHECK_IMPLIES(arg1.has_value(), arg0.has_value());
    __ template CallRuntime<ThrowTypeError>(
        context, {.template_index = __ SmiConstant(Smi::FromEnum(message)),
                  .arg0 = arg0,
                  .arg1 = arg1,
                  .arg2 = arg2});
    __ Unreachable();
  }

 private:
  compiler::turboshaft::OperationMatcher matcher_{__ data()->graph()};
  Isolate* isolate() { return __ data() -> isolate(); }
  Factory* factory() { return isolate()->factory(); }
};

template <template <typename> typename Reducer,
          template <typename> typename FeedbackReducer>
class TurboshaftBuiltinsAssembler
    : public compiler::turboshaft::Assembler<
          Reducer, BuiltinsReducer, FeedbackReducer,
          compiler::turboshaft::MachineLoweringReducer,
          compiler::turboshaft::VariableReducer> {
 public:
  DEFINE_TURBOSHAFT_ALIASES()

  using Base = compiler::turboshaft::Assembler<
      Reducer, BuiltinsReducer, FeedbackReducer,
      compiler::turboshaft::MachineLoweringReducer,
      compiler::turboshaft::VariableReducer>;
  TurboshaftBuiltinsAssembler(compiler::turboshaft::PipelineData* data,
                              compiler::turboshaft::Graph& graph,
                              Zone* phase_zone)
      : Base(data, graph, graph, phase_zone) {}

  Isolate* isolate() { return Base::data()->isolate(); }
  Factory* factory() { return isolate()->factory(); }
};

template <typename Next>
class EmptyReducer : public Next {};

using TSAAssembler =
    TurboshaftBuiltinsAssembler<EmptyReducer, NoFeedbackCollectorReducer>;

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal

#endif  // V8_CODEGEN_TURBOSHAFT_BUILTINS_ASSEMBLER_INL_H_
