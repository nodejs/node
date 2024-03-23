// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_REDUCE_ARGS_HELPER_H_
#define V8_COMPILER_TURBOSHAFT_REDUCE_ARGS_HELPER_H_

#include "src/compiler/turboshaft/operations.h"

namespace v8::internal::compiler::turboshaft {

template <class Callback>
class CallWithReduceArgsHelper {
 public:
  explicit CallWithReduceArgsHelper(Callback callback)
      : callback_(std::move(callback)) {
#define TEST(op) \
  static_assert( \
      std::is_same_v<decltype((*this)(std::declval<op##Op>())), OpIndex>);
    TURBOSHAFT_OPERATION_LIST(TEST)
#undef TEST
  }

  OpIndex operator()(const GotoOp& op) {
    return callback_(op.destination, op.is_backedge);
  }

  OpIndex operator()(const BranchOp& op) {
    return callback_(op.condition(), op.if_true, op.if_false, op.hint);
  }

  OpIndex operator()(const SwitchOp& op) {
    return callback_(op.input(), op.cases, op.default_case, op.default_hint);
  }

  OpIndex operator()(const PhiOp& op) { return callback_(op.inputs(), op.rep); }

  OpIndex operator()(const PendingLoopPhiOp& op) {
    return callback_(op.first(), op.rep);
  }

  OpIndex operator()(const FrameStateOp& op) {
    return callback_(op.inputs(), op.inlined, op.data);
  }

  OpIndex operator()(const CallOp& op) {
    return callback_(op.callee(), op.frame_state(), op.arguments(),
                     op.descriptor, op.Effects());
  }

  OpIndex operator()(const CheckExceptionOp& op) {
    return callback_(op.throwing_operation(), op.didnt_throw_block,
                     op.catch_block);
  }

  OpIndex operator()(const CatchBlockBeginOp& op) { return callback_(); }

  OpIndex operator()(const DidntThrowOp& op) {
    return callback_(op.throwing_operation(), op.has_catch_block,
                     op.results_rep);
  }

  OpIndex operator()(const TailCallOp& op) {
    return callback_(op.callee(), op.arguments(), op.descriptor);
  }

  OpIndex operator()(const ReturnOp& op) {
    return callback_(op.pop_count(), op.return_values());
  }

  OpIndex operator()(const OverflowCheckedBinopOp& op) {
    return callback_(op.left(), op.right(), op.kind, op.rep);
  }

  OpIndex operator()(const WordUnaryOp& op) {
    return callback_(op.input(), op.kind, op.rep);
  }

  OpIndex operator()(const FloatUnaryOp& op) {
    return callback_(op.input(), op.kind, op.rep);
  }

  OpIndex operator()(const ShiftOp& op) {
    return callback_(op.left(), op.right(), op.kind, op.rep);
  }

  OpIndex operator()(const ComparisonOp& op) {
    return callback_(op.left(), op.right(), op.kind, op.rep);
  }

  OpIndex operator()(const ChangeOp& op) {
    return callback_(op.input(), op.kind, op.assumption, op.from, op.to);
  }

  OpIndex operator()(const ChangeOrDeoptOp& op) {
    return callback_(op.input(), op.frame_state(), op.kind, op.minus_zero_mode,
                     op.feedback);
  }

  OpIndex operator()(const TryChangeOp& op) {
    return callback_(op.input(), op.kind, op.from, op.to);
  }

  OpIndex operator()(const BitcastWord32PairToFloat64Op& op) {
    return callback_(op.high_word32(), op.low_word32());
  }

  OpIndex operator()(const TaggedBitcastOp& op) {
    return callback_(op.input(), op.from, op.to, op.kind);
  }

  OpIndex operator()(const ObjectIsOp& op) {
    return callback_(op.input(), op.kind, op.input_assumptions);
  }

  OpIndex operator()(const FloatIsOp& op) {
    return callback_(op.input(), op.kind, op.input_rep);
  }

  OpIndex operator()(const ObjectIsNumericValueOp& op) {
    return callback_(op.input(), op.kind, op.input_rep);
  }

  OpIndex operator()(const ConvertOp& op) {
    return callback_(op.input(), op.from, op.to);
  }

  OpIndex operator()(const ConvertUntaggedToJSPrimitiveOp& op) {
    return callback_(op.input(), op.kind, op.input_rep, op.input_interpretation,
                     op.minus_zero_mode);
  }

  OpIndex operator()(const ConvertUntaggedToJSPrimitiveOrDeoptOp& op) {
    return callback_(op.input(), op.frame_state(), op.kind, op.input_rep,
                     op.input_interpretation, op.feedback);
  }

  OpIndex operator()(const ConvertJSPrimitiveToUntaggedOp& op) {
    return callback_(op.input(), op.kind, op.input_assumptions);
  }

  OpIndex operator()(const ConvertJSPrimitiveToUntaggedOrDeoptOp& op) {
    return callback_(op.input(), op.frame_state(), op.from_kind, op.to_kind,
                     op.minus_zero_mode, op.feedback);
  }

  OpIndex operator()(const TruncateJSPrimitiveToUntaggedOp& op) {
    return callback_(op.input(), op.kind, op.input_assumptions);
  }

  OpIndex operator()(const TruncateJSPrimitiveToUntaggedOrDeoptOp& op) {
    return callback_(op.input(), op.frame_state(), op.kind,
                     op.input_requirement, op.feedback);
  }

  OpIndex operator()(const ConvertJSPrimitiveToObjectOp& op) {
    return callback_(op.value(), op.native_context(), op.global_proxy(),
                     op.mode);
  }

  OpIndex operator()(const SelectOp& op) {
    return callback_(op.cond(), op.vtrue(), op.vfalse(), op.rep, op.hint,
                     op.implem);
  }

  OpIndex operator()(const ConstantOp& op) {
    return callback_(op.kind, op.storage);
  }

  OpIndex operator()(const LoadOp& op) {
    return callback_(op.base(), op.index(), op.kind, op.loaded_rep,
                     op.result_rep, op.offset, op.element_size_log2);
  }

  OpIndex operator()(const StoreOp& op) {
    return callback_(op.base(), op.index(), op.value(), op.kind, op.stored_rep,
                     op.write_barrier, op.offset, op.element_size_log2,
                     op.maybe_initializing_or_transitioning,
                     op.indirect_pointer_tag());
  }

  OpIndex operator()(const AllocateOp& op) {
    return callback_(op.size(), op.type);
  }

  OpIndex operator()(const DecodeExternalPointerOp& op) {
    return callback_(op.handle(), op.tag);
  }

  OpIndex operator()(const RetainOp& op) { return callback_(op.retained()); }

  OpIndex operator()(const ParameterOp& op) {
    return callback_(op.parameter_index, op.rep, op.debug_name);
  }

  OpIndex operator()(const OsrValueOp& op) { return callback_(op.index); }

  OpIndex operator()(const StackPointerGreaterThanOp& op) {
    return callback_(op.stack_limit(), op.kind);
  }

  OpIndex operator()(const StackSlotOp& op) {
    return callback_(op.size, op.alignment);
  }

  OpIndex operator()(const FrameConstantOp& op) { return callback_(op.kind); }

  OpIndex operator()(const DeoptimizeOp& op) {
    return callback_(op.frame_state(), op.parameters);
  }

  OpIndex operator()(const DeoptimizeIfOp& op) {
    return callback_(op.condition(), op.frame_state(), op.negated,
                     op.parameters);
  }

#if V8_ENABLE_WEBASSEMBLY
  OpIndex operator()(const TrapIfOp& op) {
    return callback_(op.condition(), op.frame_state(), op.negated, op.trap_id);
  }
#endif

  OpIndex operator()(const ProjectionOp& op) {
    return callback_(op.input(), op.index, op.rep);
  }

  OpIndex operator()(const WordBinopOp& op) {
    return callback_(op.left(), op.right(), op.kind, op.rep);
  }

  OpIndex operator()(const FloatBinopOp& op) {
    return callback_(op.left(), op.right(), op.kind, op.rep);
  }

  OpIndex operator()(const UnreachableOp& op) { return callback_(); }

  OpIndex operator()(const StaticAssertOp& op) {
    return callback_(op.condition(), op.source);
  }

  OpIndex operator()(const CheckTurboshaftTypeOfOp& op) {
    return callback_(op.input(), op.rep, op.type, op.successful);
  }

  OpIndex operator()(const NewConsStringOp& op) {
    return callback_(op.length(), op.first(), op.second());
  }

  OpIndex operator()(const NewArrayOp& op) {
    return callback_(op.length(), op.kind, op.allocation_type);
  }

  OpIndex operator()(const DoubleArrayMinMaxOp& op) {
    return callback_(op.array(), op.kind);
  }

  OpIndex operator()(const LoadFieldByIndexOp& op) {
    return callback_(op.object(), op.index());
  }

  OpIndex operator()(const DebugBreakOp& op) { return callback_(); }

  OpIndex operator()(const DebugPrintOp& op) {
    return callback_(op.input(), op.rep);
  }

  OpIndex operator()(const CommentOp& op) { return callback_(op.message); }

  OpIndex operator()(const BigIntBinopOp& op) {
    return callback_(op.left(), op.right(), op.frame_state(), op.kind);
  }

  OpIndex operator()(const BigIntComparisonOp& op) {
    return callback_(op.left(), op.right(), op.kind);
  }

  OpIndex operator()(const BigIntUnaryOp& op) {
    return callback_(op.input(), op.kind);
  }

  OpIndex operator()(const LoadRootRegisterOp& op) { return callback_(); }

  OpIndex operator()(const StringAtOp& op) {
    return callback_(op.string(), op.position(), op.kind);
  }

#ifdef V8_INTL_SUPPORT
  OpIndex operator()(const StringToCaseIntlOp& op) {
    return callback_(op.string(), op.kind);
  }
#endif

  OpIndex operator()(const StringLengthOp& op) {
    return callback_(op.string());
  }

  OpIndex operator()(const StringIndexOfOp& op) {
    return callback_(op.string(), op.search(), op.position());
  }

  OpIndex operator()(const StringFromCodePointAtOp& op) {
    return callback_(op.string(), op.index());
  }

  OpIndex operator()(const StringSubstringOp& op) {
    return callback_(op.string(), op.start(), op.end());
  }

  OpIndex operator()(const StringConcatOp& op) {
    return callback_(op.left(), op.right());
  }

  OpIndex operator()(const StringComparisonOp& op) {
    return callback_(op.left(), op.right(), op.kind);
  }

  OpIndex operator()(const ArgumentsLengthOp& op) {
    return callback_(op.kind, op.formal_parameter_count);
  }

  OpIndex operator()(const NewArgumentsElementsOp& op) {
    return callback_(op.arguments_count(), op.type, op.formal_parameter_count);
  }

  OpIndex operator()(const LoadTypedElementOp& op) {
    return callback_(op.buffer(), op.base(), op.external(), op.index(),
                     op.array_type);
  }

  OpIndex operator()(const LoadDataViewElementOp& op) {
    return callback_(op.object(), op.storage(), op.index(),
                     op.is_little_endian(), op.element_type);
  }

  OpIndex operator()(const LoadStackArgumentOp& op) {
    return callback_(op.base(), op.index());
  }

  OpIndex operator()(const StoreTypedElementOp& op) {
    return callback_(op.buffer(), op.base(), op.external(), op.index(),
                     op.value(), op.array_type);
  }

  OpIndex operator()(const StoreDataViewElementOp& op) {
    return callback_(op.object(), op.storage(), op.index(), op.value(),
                     op.is_little_endian(), op.element_type);
  }

  OpIndex operator()(const TransitionAndStoreArrayElementOp& op) {
    return callback_(op.array(), op.index(), op.value(), op.kind, op.fast_map,
                     op.double_map);
  }

  OpIndex operator()(const CompareMapsOp& op) {
    return callback_(op.heap_object(), op.maps);
  }

  OpIndex operator()(const CheckMapsOp& op) {
    return callback_(op.heap_object(), op.frame_state(), op.maps, op.flags,
                     op.feedback);
  }

  OpIndex operator()(const AssumeMapOp& op) {
    return callback_(op.heap_object(), op.maps);
  }

  OpIndex operator()(const CheckedClosureOp& op) {
    return callback_(op.input(), op.frame_state(), op.feedback_cell);
  }

  OpIndex operator()(const CheckEqualsInternalizedStringOp& op) {
    return callback_(op.expected(), op.value(), op.frame_state());
  }

  OpIndex operator()(const LoadMessageOp& op) { return callback_(op.offset()); }

  OpIndex operator()(const StoreMessageOp& op) {
    return callback_(op.offset(), op.object());
  }

  OpIndex operator()(const SameValueOp& op) {
    return callback_(op.left(), op.right(), op.mode);
  }

  OpIndex operator()(const Float64SameValueOp& op) {
    return callback_(op.left(), op.right());
  }

  OpIndex operator()(const FastApiCallOp& op) {
    return callback_(op.data_argument(), op.arguments(), op.parameters);
  }

  OpIndex operator()(const RuntimeAbortOp& op) { return callback_(op.reason); }

  OpIndex operator()(const EnsureWritableFastElementsOp& op) {
    return callback_(op.object(), op.elements());
  }

  OpIndex operator()(const MaybeGrowFastElementsOp& op) {
    return callback_(op.object(), op.elements(), op.index(),
                     op.elements_length(), op.frame_state(), op.mode,
                     op.feedback);
  }

  OpIndex operator()(const TransitionElementsKindOp& op) {
    return callback_(op.object(), op.transition);
  }

  OpIndex operator()(const FindOrderedHashEntryOp& op) {
    return callback_(op.data_structure(), op.key(), op.kind);
  }

  OpIndex operator()(const SpeculativeNumberBinopOp& op) {
    return callback_(op.left(), op.right(), op.frame_state(), op.kind);
  }

  OpIndex operator()(const Word32PairBinopOp& op) {
    return callback_(op.left_low(), op.left_high(), op.right_low(),
                     op.right_high(), op.kind);
  }

  OpIndex operator()(const TupleOp& tuple) {
    return callback_(const_cast<TupleOp&>(tuple).inputs());
  }

  OpIndex operator()(const AtomicRMWOp& op) {
    return callback_(op.base(), op.index(), op.value(), op.expected(),
                     op.bin_op, op.result_rep, op.input_rep,
                     op.memory_access_kind);
  }

  OpIndex operator()(const AtomicWord32PairOp& op) {
    return callback_(op.base(), op.index(), op.value_low(), op.value_high(),
                     op.expected_low(), op.expected_high(), op.kind, op.offset);
  }

  OpIndex operator()(const MemoryBarrierOp& op) {
    return callback_(op.memory_order);
  }

  OpIndex operator()(const StackCheckOp& op) {
    return callback_(op.check_origin, op.check_kind);
  }

#ifdef V8_ENABLE_WEBASSEMBLY
  OpIndex operator()(const GlobalGetOp& op) {
    return callback_(op.instance(), op.global);
  }

  OpIndex operator()(const GlobalSetOp& op) {
    return callback_(op.instance(), op.value(), op.global);
  }

  OpIndex operator()(const NullOp& op) { return callback_(op.type); }

  OpIndex operator()(const IsNullOp& op) {
    return callback_(op.object(), op.type);
  }

  OpIndex operator()(const AssertNotNullOp& op) {
    return callback_(op.object(), op.type, op.trap_id);
  }

  OpIndex operator()(const RttCanonOp& op) {
    return callback_(op.rtts(), op.type_index);
  }

  OpIndex operator()(const WasmTypeCheckOp& op) {
    return callback_(op.object(), op.rtt(), op.config);
  }

  OpIndex operator()(const WasmTypeCastOp& op) {
    return callback_(op.object(), op.rtt(), op.config);
  }

  OpIndex operator()(const StructGetOp& op) {
    return callback_(op.object(), op.type, op.type_index, op.field_index,
                     op.is_signed, op.null_check);
  }

  OpIndex operator()(const StructSetOp& op) {
    return callback_(op.object(), op.value(), op.type, op.type_index,
                     op.field_index, op.null_check);
  }

  OpIndex operator()(const ArrayGetOp& op) {
    return callback_(op.array(), op.index(), op.array_type, op.is_signed);
  }

  OpIndex operator()(const ArraySetOp& op) {
    return callback_(op.array(), op.index(), op.value(), op.element_type);
  }

  OpIndex operator()(const ArrayLengthOp& op) {
    return callback_(op.array(), op.null_check);
  }

  OpIndex operator()(const WasmAllocateArrayOp& op) {
    return callback_(op.rtt(), op.length(), op.array_type);
  }

  OpIndex operator()(const WasmAllocateStructOp& op) {
    return callback_(op.rtt(), op.struct_type);
  }

  OpIndex operator()(const WasmRefFuncOp& op) {
    return callback_(op.instance(), op.function_index);
  }

  OpIndex operator()(const Simd128ConstantOp& op) {
    return callback_(op.value);
  }

  OpIndex operator()(const Simd128BinopOp& op) {
    return callback_(op.left(), op.right(), op.kind);
  }

  OpIndex operator()(const Simd128UnaryOp& op) {
    return callback_(op.input(), op.kind);
  }

  OpIndex operator()(const Simd128ShiftOp& op) {
    return callback_(op.input(), op.shift(), op.kind);
  }

  OpIndex operator()(const Simd128TestOp& op) {
    return callback_(op.input(), op.kind);
  }

  OpIndex operator()(const Simd128SplatOp& op) {
    return callback_(op.input(), op.kind);
  }

  OpIndex operator()(const Simd128TernaryOp& op) {
    return callback_(op.first(), op.second(), op.third(), op.kind);
  }

  OpIndex operator()(const Simd128ExtractLaneOp& op) {
    return callback_(op.input(), op.kind, op.lane);
  }

  OpIndex operator()(const Simd128ReplaceLaneOp& op) {
    return callback_(op.into(), op.new_lane(), op.kind, op.lane);
  }

  OpIndex operator()(const Simd128LaneMemoryOp& op) {
    return callback_(op.base(), op.index(), op.value(), op.mode, op.kind,
                     op.lane_kind, op.lane, op.offset);
  }

  OpIndex operator()(const Simd128LoadTransformOp& op) {
    return callback_(op.base(), op.index(), op.load_kind, op.transform_kind,
                     op.offset);
  }

  OpIndex operator()(const Simd128ShuffleOp& op) {
    return callback_(op.left(), op.right(), op.shuffle);
  }

  OpIndex operator()(const StringAsWtf16Op& op) {
    return callback_(op.string());
  }

  OpIndex operator()(const StringPrepareForGetCodeUnitOp& op) {
    return callback_(op.string());
  }

  OpIndex operator()(const ExternConvertAnyOp& op) {
    return callback_(op.object());
  }

  OpIndex operator()(const AnyConvertExternOp& op) {
    return callback_(op.object());
  }

  OpIndex operator()(const WasmTypeAnnotationOp& op) {
    return callback_(op.value(), op.type);
  }

  OpIndex operator()(const LoadStackPointerOp& op) { return callback_(); }

  OpIndex operator()(const SetStackPointerOp& op) {
    return callback_(op.value(), op.fp_scope);
  }
#endif

 private:
  Callback callback_;
};

// Construct a callable that takes the Operation as an argument and calls the
// provided callback with the REDUCE(Op) signature.. This can be used to
// "unpack" an Operation and pass its values back inta a Reduce function.
template <class Callback>
auto CallWithReduceArgs(Callback callback) {
  return CallWithReduceArgsHelper<Callback>(callback);
}

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_REDUCE_ARGS_HELPER_H_
