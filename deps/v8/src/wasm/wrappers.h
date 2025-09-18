// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WRAPPERS_H_
#define V8_WASM_WRAPPERS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/wasm-assembler-helpers.h"
#include "src/wasm/turboshaft-graph-interface.h"
#include "src/wasm/wasm-engine.h"

namespace v8::internal::wasm {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

const compiler::turboshaft::TSCallDescriptor* GetBuiltinCallDescriptor(
    Builtin name, Zone* zone);

template <typename Assembler>
class WasmWrapperTSGraphBuilder : public WasmGraphBuilderBase<Assembler> {
  using typename WasmGraphBuilderBase<Assembler>::Any;

  using CallDescriptor = compiler::CallDescriptor;
  using Operator = compiler::Operator;
  using Float32 = compiler::turboshaft::Float32;
  using Float64 = compiler::turboshaft::Float64;
  template <typename... Ts>
  using Label = v8::internal::compiler::turboshaft::Label<Ts...>;
  using LoadOp = compiler::turboshaft::LoadOp;
  using MemoryRepresentation = compiler::turboshaft::MemoryRepresentation;
  using TSBlock = compiler::turboshaft::Block;
  using OpEffects = compiler::turboshaft::OpEffects;
  using OpIndex = compiler::turboshaft::OpIndex;
  using OptionalOpIndex = compiler::turboshaft::OptionalOpIndex;
  using RegisterRepresentation = compiler::turboshaft::RegisterRepresentation;
  template <typename T>
  using ScopedVar = compiler::turboshaft::ScopedVar<T, Assembler>;
  using StoreOp = compiler::turboshaft::StoreOp;
  using TSCallDescriptor = compiler::turboshaft::TSCallDescriptor;
  template <typename... Ts>
  using Tuple = compiler::turboshaft::Tuple<Ts...>;
  template <typename T>
  using V = compiler::turboshaft::V<T>;
  using Variable = compiler::turboshaft::Variable;
  using Word32 = compiler::turboshaft::Word32;
  using WordPtr = compiler::turboshaft::WordPtr;

 public:
  using WasmGraphBuilderBase<Assembler>::Asm;

  WasmWrapperTSGraphBuilder(Zone* zone, Assembler& assembler,
                            const CanonicalSig* sig)
      : WasmGraphBuilderBase<Assembler>(zone, assembler), sig_(sig) {}

  void AbortIfNot(V<Word32> condition, AbortReason abort_reason);

  V<Smi> LoadExportedFunctionIndexAsSmi(V<Object> exported_function_data) {
    return __ Load(exported_function_data,
                   LoadOp::Kind::TaggedBase().Immutable(),
                   MemoryRepresentation::TaggedSigned(),
                   WasmExportedFunctionData::kFunctionIndexOffset);
  }

  V<Smi> BuildChangeInt32ToSmi(V<Word32> value) {
    // With pointer compression, only the lower 32 bits are used.
    return V<Smi>::Cast(COMPRESS_POINTERS_BOOL
                            ? __ BitcastWord32ToWord64(__ Word32ShiftLeft(
                                  value, BuildSmiShiftBitsConstant32()))
                            : __ Word64ShiftLeft(__ ChangeInt32ToInt64(value),
                                                 BuildSmiShiftBitsConstant()));
  }

  V<WordPtr> GetTargetForBuiltinCall(Builtin builtin) {
    return WasmGraphBuilderBase<Assembler>::GetTargetForBuiltinCall(
        builtin, StubCallMode::kCallBuiltinPointer);
  }

  template <typename Descriptor, typename... Args>
  OpIndex CallBuiltin(Builtin name, OpIndex frame_state,
                      Operator::Properties properties, Args... args) {
    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), Descriptor(), 0,
        frame_state.valid() ? CallDescriptor::kNeedsFrameState
                            : CallDescriptor::kNoFlags,
        Operator::kNoProperties, StubCallMode::kCallBuiltinPointer);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo,
        compiler::LazyDeoptOnThrow::kNo, __ graph_zone());
    V<WordPtr> call_target = GetTargetForBuiltinCall(name);
    return __ Call(call_target, frame_state, base::VectorOf({args...}),
                   ts_call_descriptor);
  }

  template <typename Descriptor, typename... Args>
  OpIndex CallBuiltin(Builtin name, Operator::Properties properties,
                      Args... args) {
    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), Descriptor(), 0, CallDescriptor::kNoFlags,
        Operator::kNoProperties, StubCallMode::kCallBuiltinPointer);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo,
        compiler::LazyDeoptOnThrow::kNo, __ graph_zone());
    V<WordPtr> call_target = GetTargetForBuiltinCall(name);
    return __ Call(call_target, {args...}, ts_call_descriptor);
  }

  V<Number> BuildChangeInt32ToNumber(V<Word32> value);

  V<Number> BuildChangeFloat32ToNumber(V<Float32> value) {
    return CallBuiltin<WasmFloat32ToNumberDescriptor>(
        Builtin::kWasmFloat32ToNumber, Operator::kNoProperties, value);
  }

  V<Number> BuildChangeFloat64ToNumber(V<Float64> value) {
    return CallBuiltin<WasmFloat64ToTaggedDescriptor>(
        Builtin::kWasmFloat64ToNumber, Operator::kNoProperties, value);
  }

  V<Object> ToJS(OpIndex ret, CanonicalValueType type, V<Context> context);

  // Generate a call to the AllocateJSArray builtin.
  V<JSArray> BuildCallAllocateJSArray(V<Number> array_length,
                                      V<Object> context) {
    // Since we don't check that args will fit in an array,
    // we make sure this is true based on statically known limits.
    static_assert(kV8MaxWasmFunctionReturns <=
                  JSArray::kInitialMaxFastElementArray);
    return CallBuiltin<WasmAllocateJSArrayDescriptor>(
        Builtin::kWasmAllocateJSArray, Operator::kEliminatable, array_length,
        context);
  }

  void BuildCallWasmFromWrapper(Zone* zone, const CanonicalSig* sig,
                                V<Word32> callee,
                                const base::Vector<OpIndex> args,
                                base::Vector<OpIndex> returns);

  OpIndex BuildCallAndReturn(V<Context> js_context, V<HeapObject> function_data,
                             base::Vector<OpIndex> args, bool do_conversion);

  void BuildJSToWasmWrapper(bool receiver_is_first_param);

  void BuildWasmToJSWrapper(ImportCallKind kind, int expected_arity,
                            Suspend suspend);

  void BuildWasmStackEntryWrapper();

  void BuildCapiCallWrapper();

  V<Word32> BuildSmiShiftBitsConstant() {
    return __ Word32Constant(kSmiShiftSize + kSmiTagSize);
  }

  V<Word32> BuildSmiShiftBitsConstant32() {
    return __ Word32Constant(kSmiShiftSize + kSmiTagSize);
  }

  V<Word32> BuildChangeSmiToInt32(OpIndex value) {
    return COMPRESS_POINTERS_BOOL
               ? __ Word32ShiftRightArithmetic(value,
                                               BuildSmiShiftBitsConstant32())
               : __
                 TruncateWordPtrToWord32(__ WordPtrShiftRightArithmetic(
                     value, BuildSmiShiftBitsConstant()));
  }

  V<Float64> HeapNumberToFloat64(V<HeapNumber> input) {
    return __ template LoadField<Float64>(
        input, compiler::AccessBuilder::ForHeapNumberValue());
  }

  OpIndex FromJSFast(OpIndex input, CanonicalValueType type) {
    switch (type.kind()) {
      case kI32:
        return BuildChangeSmiToInt32(input);
      case kF32: {
        ScopedVar<Float32> result(this, OpIndex::Invalid());
        IF (__ IsSmi(input)) {
          result = __ ChangeInt32ToFloat32(__ UntagSmi(input));
        } ELSE {
          result = __ TruncateFloat64ToFloat32(HeapNumberToFloat64(input));
        }
        return result;
      }
      case kF64: {
        ScopedVar<Float64> result(this, OpIndex::Invalid());
        IF (__ IsSmi(input)) {
          result = __ ChangeInt32ToFloat64(__ UntagSmi(input));
        } ELSE{
          result = HeapNumberToFloat64(input);
        }
        return result;
      }
      case kRef:
      case kRefNull:
      case kI64:
      case kS128:
      case kI8:
      case kI16:
      case kF16:
      case kTop:
      case kBottom:
      case kVoid:
        UNREACHABLE();
    }
  }

  OpIndex LoadInstanceType(V<Map> map) {
    return __ Load(map, LoadOp::Kind::TaggedBase().Immutable(),
                   MemoryRepresentation::Uint16(), Map::kInstanceTypeOffset);
  }

  OpIndex BuildCheckString(OpIndex input, OpIndex js_context,
                           CanonicalValueType type) {
    auto done = __ NewBlock();
    auto type_error = __ NewBlock();
    ScopedVar<Object> result(this, LOAD_ROOT(WasmNull));
    __ GotoIf(__ IsSmi(input), type_error, BranchHint::kFalse);
    if (type.is_nullable()) {
      auto not_null = __ NewBlock();
      __ GotoIfNot(__ TaggedEqual(input, LOAD_ROOT(NullValue)), not_null);
      __ Goto(done);
      __ Bind(not_null);
    }
    V<Map> map = LoadMap(input);
    OpIndex instance_type = LoadInstanceType(map);
    OpIndex check = __ Uint32LessThan(instance_type,
                                      __ Word32Constant(FIRST_NONSTRING_TYPE));
    result = input;
    __ GotoIf(check, done, BranchHint::kTrue);
    __ Goto(type_error);
    __ Bind(type_error);
    __ WasmCallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                       js_context);
    __ Unreachable();
    __ Bind(done);
    return result;
  }

  V<Float64> BuildChangeTaggedToFloat64(
      OpIndex value, OpIndex context,
      compiler::turboshaft::OptionalOpIndex frame_state) {
    OpIndex call = frame_state.valid()
                       ? CallBuiltin<WasmTaggedToFloat64Descriptor>(
                             Builtin::kWasmTaggedToFloat64, frame_state.value(),
                             Operator::kNoProperties, value, context)
                       : CallBuiltin<WasmTaggedToFloat64Descriptor>(
                             Builtin::kWasmTaggedToFloat64,
                             Operator::kNoProperties, value, context);
    // The source position here is needed for asm.js, see the comment on the
    // source position of the call to JavaScript in the wasm-to-js wrapper.
    __ output_graph().source_positions()[call] = SourcePosition(1);
    return call;
  }

  OpIndex BuildChangeTaggedToInt32(
      OpIndex value, OpIndex context,
      compiler::turboshaft::OptionalOpIndex frame_state) {
    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    ScopedVar<Word32> result(this, OpIndex::Invalid());
    IF (__ IsSmi(value)) {
      result = BuildChangeSmiToInt32(value);
    } ELSE{
      OpIndex call =
          frame_state.valid()
              ? CallBuiltin<WasmTaggedNonSmiToInt32Descriptor>(
                    Builtin::kWasmTaggedNonSmiToInt32, frame_state.value(),
                    Operator::kNoProperties, value, context)
              : CallBuiltin<WasmTaggedNonSmiToInt32Descriptor>(
                    Builtin::kWasmTaggedNonSmiToInt32, Operator::kNoProperties,
                    value, context);
      result = call;
      // The source position here is needed for asm.js, see the comment on the
      // source position of the call to JavaScript in the wasm-to-js wrapper.
      __ output_graph().source_positions()[call] = SourcePosition(1);
    }
    return result;
  }

#ifdef V8_ENABLE_TURBOFAN
  CallDescriptor* GetBigIntToI64CallDescriptor(bool needs_frame_state) {
    return GetWasmEngine()->call_descriptors()->GetBigIntToI64Descriptor(
        needs_frame_state);
  }

  OpIndex BuildChangeBigIntToInt64(
      OpIndex input, OpIndex context,
      compiler::turboshaft::OptionalOpIndex frame_state) {
    OpIndex target;
    if (Is64()) {
      target = GetTargetForBuiltinCall(Builtin::kBigIntToI64);
    } else {
      // On 32-bit platforms we already set the target to the
      // BigIntToI32Pair builtin here, so that we don't have to replace the
      // target in the int64-lowering.
      target = GetTargetForBuiltinCall(Builtin::kBigIntToI32Pair);
    }

    CallDescriptor* call_descriptor =
        GetBigIntToI64CallDescriptor(frame_state.valid());
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo,
        compiler::LazyDeoptOnThrow::kNo, __ graph_zone());
    return frame_state.valid()
               ? __ Call(target, frame_state.value(),
                         base::VectorOf({input, context}), ts_call_descriptor)
               : __ Call(target, {input, context}, ts_call_descriptor);
  }
#endif

  OpIndex FromJS(V<Object> input, OpIndex context, CanonicalValueType type,
                 OptionalOpIndex frame_state = {}) {
    switch (type.kind()) {
      case kRef:
      case kRefNull: {
        switch (type.heap_representation_non_shared()) {
          // TODO(14034): Add more fast paths?
          case HeapType::kExtern: {
            if (type.kind() == kRef) {
              IF (UNLIKELY(__ TaggedEqual(input, LOAD_ROOT(NullValue)))) {
                __ WasmCallRuntime(__ phase_zone(),
                                   Runtime::kWasmThrowJSTypeError, {}, context);
                __ Unreachable();
              }
            }
            if (v8_flags.experimental_wasm_shared &&
                type.heap_representation() == HeapType::kExternShared) {
              Label<Object> done(&Asm());
              IF_NOT (__ IsSmi(input)) {
                V<WordPtr> flags = __ LoadPageFlags(V<HeapObject>::Cast(input));
                V<WordPtr> shared_or_read_only = __ WordPtrBitwiseAnd(
                    flags, static_cast<uintptr_t>(
                               MemoryChunk::IN_WRITABLE_SHARED_SPACE |
                               MemoryChunk::READ_ONLY_HEAP));
                IF (UNLIKELY(__ WordPtrEqual(shared_or_read_only, 0))) {
                  // If it isn't shared, yet, use the runtime function.
                  std::initializer_list<const OpIndex> inputs = {
                      input, __ IntPtrConstant(IntToSmi(
                                 static_cast<int>(type.raw_bit_field())))};
                  GOTO(done, __ WasmCallRuntime(__ phase_zone(),
                                                Runtime::kWasmJSToWasmObject,
                                                inputs, context));
                }
              }
              GOTO(done, input);
              BIND(done, result);
              return result;
            }
            return input;
          }
          case HeapType::kString:
            return BuildCheckString(input, context, type);
          case HeapType::kExn:
          case HeapType::kNoExn: {
            UNREACHABLE();
          }
          case HeapType::kNoExtern:
          case HeapType::kNone:
          case HeapType::kNoFunc:
          case HeapType::kI31:
          case HeapType::kAny:
          case HeapType::kFunc:
          case HeapType::kStruct:
          case HeapType::kArray:
          case HeapType::kEq:
          default: {
            // Make sure ValueType fits in a Smi.
            static_assert(ValueType::kLastUsedBit + 1 <= kSmiValueSize);

            std::initializer_list<const OpIndex> inputs = {
                input, __ IntPtrConstant(
                           IntToSmi(static_cast<int>(type.raw_bit_field())))};
            return __ WasmCallRuntime(
                __ phase_zone(), Runtime::kWasmJSToWasmObject, inputs, context);
          }
        }
      }
      case kF32:
        return __ TruncateFloat64ToFloat32(
            BuildChangeTaggedToFloat64(input, context, frame_state));

      case kF64:
        return BuildChangeTaggedToFloat64(input, context, frame_state);

      case kI32:
        return BuildChangeTaggedToInt32(input, context, frame_state);

      case kI64:
#ifdef V8_ENABLE_TURBOFAN
        // i64 values can only come from BigInt.
        return BuildChangeBigIntToInt64(input, context, frame_state);
#endif

      case kS128:
      case kI8:
      case kI16:
      case kF16:
      case kTop:
      case kBottom:
      case kVoid:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        UNREACHABLE();
    }
  }

  bool QualifiesForFastTransform() {
    const int wasm_count = static_cast<int>(sig_->parameter_count());
    for (int i = 0; i < wasm_count; ++i) {
      CanonicalValueType type = sig_->GetParam(i);
      switch (type.kind()) {
        case kRef:
        case kRefNull:
        case kI64:
        case kS128:
        case kI8:
        case kI16:
        case kF16:
        case kTop:
        case kBottom:
        case kVoid:
          return false;
        case kI32:
        case kF32:
        case kF64:
          break;
      }
    }
    return true;
  }

#ifdef V8_MAP_PACKING
  V<Map> UnpackMapWord(OpIndex map_word) {
    map_word = __ BitcastTaggedToWordPtrForTagAndSmiBits(map_word);
    // TODO(wenyuzhao): Clear header metadata.
    OpIndex map = __ WordBitwiseXor(
        map_word, __ IntPtrConstant(Internals::kMapWordXorMask),
        WordRepresentation::UintPtr());
    return V<Map>::Cast(__ BitcastWordPtrToTagged(map));
  }
#endif

  V<Map> LoadMap(V<Object> object) {
    // TODO(thibaudm): Handle map packing.
    OpIndex map_word = __ Load(object, LoadOp::Kind::TaggedBase(),
                               MemoryRepresentation::TaggedPointer(), 0);
#ifdef V8_MAP_PACKING
    return UnpackMapWord(map_word);
#else
    return map_word;
#endif
  }

  void CanTransformFast(OpIndex input, CanonicalValueType type,
                        TSBlock* slow_path) {
    switch (type.kind()) {
      case kI32: {
        __ GotoIfNot(LIKELY(__ IsSmi(input)), slow_path);
        return;
      }
      case kF32:
      case kF64: {
        TSBlock* done = __ NewBlock();
        __ GotoIf(__ IsSmi(input), done);
        V<Map> map = LoadMap(input);
        V<Map> heap_number_map = LOAD_ROOT(HeapNumberMap);
        // TODO(thibaudm): Handle map packing.
        V<Word32> is_heap_number = __ TaggedEqual(heap_number_map, map);
        __ GotoIf(LIKELY(is_heap_number), done);
        __ Goto(slow_path);
        __ Bind(done);
        return;
      }
      case kRef:
      case kRefNull:
      case kI64:
      case kS128:
      case kI8:
      case kI16:
      case kF16:
      case kTop:
      case kBottom:
      case kVoid:
        UNREACHABLE();
    }
  }

  // Must be called in the first block to emit the Parameter ops.
  int AddArgumentNodes(base::Vector<OpIndex> args, int pos,
                       base::SmallVector<OpIndex, 16> wasm_params,
                       const CanonicalSig* sig, V<Context> context) {
    // Convert wasm numbers to JS values.
    for (size_t i = 0; i < wasm_params.size(); ++i) {
      args[pos++] = ToJS(wasm_params[i], sig->GetParam(i), context);
    }
    return pos;
  }

  OpIndex LoadSharedFunctionInfo(V<Object> js_function) {
    return __ Load(js_function, LoadOp::Kind::TaggedBase(),
                   MemoryRepresentation::TaggedPointer(),
                   JSFunction::kSharedFunctionInfoOffset);
  }

  OpIndex BuildReceiverNode(OpIndex callable_node, OpIndex native_context,
                            V<Undefined> undefined_node) {
    // Check function strict bit.
    V<SharedFunctionInfo> shared_function_info =
        LoadSharedFunctionInfo(callable_node);
    OpIndex flags = __ Load(shared_function_info, LoadOp::Kind::TaggedBase(),
                            MemoryRepresentation::Int32(),
                            SharedFunctionInfo::kFlagsOffset);
    OpIndex strict_check = __ Word32BitwiseAnd(
        flags, __ Word32Constant(SharedFunctionInfo::IsNativeBit::kMask |
                                 SharedFunctionInfo::IsStrictBit::kMask));

    // Load global receiver if sloppy else use undefined.
    ScopedVar<Object> strict_d(this, OpIndex::Invalid());
    IF (strict_check) {
      strict_d = undefined_node;
    } ELSE {
      strict_d =
          __ LoadFixedArrayElement(native_context, Context::GLOBAL_PROXY_INDEX);
    }
    return strict_d;
  }

  V<Context> LoadContextFromJSFunction(V<JSFunction> js_function) {
    return __ Load(js_function, LoadOp::Kind::TaggedBase(),
                   MemoryRepresentation::TaggedPointer(),
                   JSFunction::kContextOffset);
  }

  V<Object> BuildSuspend(V<Object> value, V<Object> import_data,
                         V<Object> suspender, V<WordPtr>* old_sp,
                         V<WordPtr> old_limit) {
    // If value is a promise, suspend to the js-to-wasm prompt, and resume later
    // with the promise's resolved value.
    ScopedVar<Object> result(this, value);
    ScopedVar<WordPtr> old_sp_var(this, *old_sp);

    OpIndex native_context = __ Load(import_data, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     WasmImportData::kNativeContextOffset);

    OpIndex promise_ctor = __ LoadFixedArrayElement(
        native_context, Context::PROMISE_FUNCTION_INDEX);

    OpIndex promise_resolve =
        this->GetBuiltinPointerTarget(Builtin::kPromiseResolve);
    auto* resolve_call_desc =
        GetBuiltinCallDescriptor(Builtin::kPromiseResolve, __ graph_zone());
    base::SmallVector<OpIndex, 16> resolve_args{promise_ctor, value,
                                                native_context};
    OpIndex promise = __ Call(promise_resolve, OpIndex::Invalid(),
                              base::VectorOf(resolve_args), resolve_call_desc);

    V<Object> on_fulfilled = __ Load(suspender, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     WasmSuspenderObject::kResumeOffset);
    V<Object> on_rejected = __ Load(suspender, LoadOp::Kind::TaggedBase(),
                                    MemoryRepresentation::TaggedPointer(),
                                    WasmSuspenderObject::kRejectOffset);

    OpIndex promise_then =
        this->GetBuiltinPointerTarget(Builtin::kPerformPromiseThen);
    auto* then_call_desc =
        GetBuiltinCallDescriptor(Builtin::kPerformPromiseThen, __ graph_zone());
    base::SmallVector<OpIndex, 16> args{promise, on_fulfilled, on_rejected,
                                        LOAD_ROOT(UndefinedValue),
                                        native_context};
    __ Call(promise_then, OpIndex::Invalid(), base::VectorOf(args),
            then_call_desc);

    OpIndex suspend = GetTargetForBuiltinCall(Builtin::kWasmSuspend);
    auto* suspend_call_descriptor =
        GetBuiltinCallDescriptor(Builtin::kWasmSuspend, __ graph_zone());
    this->BuildSwitchBackFromCentralStack(*old_sp, old_limit);
    V<Object> resolved =
        __ template Call<Object>(suspend, {suspender}, suspend_call_descriptor);
    old_sp_var = this->BuildSwitchToTheCentralStack(old_limit);
    result = resolved;

    *old_sp = old_sp_var;
    return result;
  }

  V<FixedArray> BuildMultiReturnFixedArrayFromIterable(OpIndex iterable,
                                                       V<Context> context) {
    V<Smi> length = __ SmiConstant(Smi::FromIntptr(sig_->return_count()));
    return CallBuiltin<IterableToFixedArrayForWasmDescriptor>(
        Builtin::kIterableToFixedArrayForWasm, Operator::kEliminatable,
        iterable, length, context);
  }

  void SafeStore(int offset, CanonicalValueType type, OpIndex base,
                 OpIndex value) {
    int alignment = offset % type.value_kind_size();
    auto rep = MemoryRepresentation::FromMachineRepresentation(
        type.machine_representation());
    if (COMPRESS_POINTERS_BOOL && rep.IsCompressibleTagged()) {
      // We are storing tagged value to off-heap location, so we need to store
      // it as a full word otherwise we will not be able to decompress it.
      rep = MemoryRepresentation::UintPtr();
      value = __ BitcastTaggedToWordPtr(value);
    }
    StoreOp::Kind store_kind =
        alignment == 0 || compiler::turboshaft::SupportedOperations::
                              IsUnalignedStoreSupported(rep)
            ? StoreOp::Kind::RawAligned()
            : StoreOp::Kind::RawUnaligned();
    __ Store(base, value, store_kind, rep, compiler::kNoWriteBarrier, offset);
  }

  V<WordPtr> BuildLoadCallTargetFromExportedFunctionData(
      V<WasmFunctionData> function_data) {
    // TODO(sroettger): this code should do a signature check, but it's only
    // used for CAPI.
    V<WasmInternalFunction> internal =
        V<WasmInternalFunction>::Cast(__ LoadProtectedPointerField(
            function_data, LoadOp::Kind::TaggedBase().Immutable(),
            WasmFunctionData::kProtectedInternalOffset));
    V<Word32> code_pointer = __ Load(
        internal, LoadOp::Kind::TaggedBase(), MemoryRepresentation::Uint32(),
        WasmInternalFunction::kRawCallTargetOffset);
    constexpr size_t entry_size_log2 =
        std::bit_width(sizeof(WasmCodePointerTableEntry)) - 1;
    return __ Load(
        __ ExternalConstant(ExternalReference::wasm_code_pointer_table()),
        __ ChangeUint32ToUintPtr(code_pointer), LoadOp::Kind::RawAligned(),
        MemoryRepresentation::UintPtr(), 0, entry_size_log2);
  }

  const OpIndex SafeLoad(OpIndex base, int offset, CanonicalValueType type) {
    int alignment = offset % type.value_kind_size();
    auto rep = MemoryRepresentation::FromMachineRepresentation(
        type.machine_representation());
    if (COMPRESS_POINTERS_BOOL && rep.IsCompressibleTagged()) {
      // We are loading tagged value from off-heap location, so we need to load
      // it as a full word otherwise we will not be able to decompress it.
      rep = MemoryRepresentation::UintPtr();
    }
    LoadOp::Kind load_kind = alignment == 0 ||
                                     compiler::turboshaft::SupportedOperations::
                                         IsUnalignedLoadSupported(rep)
                                 ? LoadOp::Kind::RawAligned()
                                 : LoadOp::Kind::RawUnaligned();
    return __ Load(base, load_kind, rep, offset);
  }

 private:
  const CanonicalSig* const sig_;
};

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::wasm

#endif  // V8_WASM_WRAPPERS_H_
