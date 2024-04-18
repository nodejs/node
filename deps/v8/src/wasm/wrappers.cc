// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/bailout-reason.h"
#include "src/compiler/linkage.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/wasm-assembler-helpers.h"
#include "src/objects/object-list-macros.h"
#include "src/wasm/turboshaft-graph-interface.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-module.h"
#include "src/zone/zone.h"

namespace v8::internal::wasm {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

using compiler::CallDescriptor;
using compiler::Operator;
using compiler::turboshaft::Float32;
using compiler::turboshaft::Float64;
using compiler::turboshaft::Label;
using compiler::turboshaft::LoadOp;
using compiler::turboshaft::MemoryRepresentation;
using TSBlock = compiler::turboshaft::Block;
using compiler::turboshaft::OpEffects;
using compiler::turboshaft::OpIndex;
using compiler::turboshaft::OptionalOpIndex;
using compiler::turboshaft::RegisterRepresentation;
using compiler::turboshaft::StoreOp;
using compiler::turboshaft::Tagged;
using compiler::turboshaft::TSCallDescriptor;
using compiler::turboshaft::V;
using compiler::turboshaft::Variable;
using compiler::turboshaft::Word32;
using compiler::turboshaft::WordPtr;

class WasmWrapperTSGraphBuilder : public WasmGraphBuilderBase {
 public:
  WasmWrapperTSGraphBuilder(Zone* zone, Assembler& assembler,
                            const WasmModule* module,
                            const wasm::FunctionSig* sig,
                            StubCallMode stub_mode)
      : WasmGraphBuilderBase(zone, assembler),
        module_(module),
        sig_(sig),
        stub_mode_(stub_mode) {}

  void AbortIfNot(V<Word32> condition, AbortReason abort_reason) {
    if (!v8_flags.debug_code) return;
    IF_NOT (condition) {
      V<Number> message_id =
          __ NumberConstant(static_cast<int32_t>(abort_reason));
      CallRuntime(__ phase_zone(), Runtime::kAbort, {message_id},
                  __ NoContextConstant());
    }
    END_IF
  }

  void BuildModifyThreadInWasmFlagHelper(V<WordPtr> thread_in_wasm_flag_address,
                                         bool new_value) {
    if (v8_flags.debug_code) {
      V<Word32> flag_value =
          __ Load(thread_in_wasm_flag_address, LoadOp::Kind::RawAligned(),
                  MemoryRepresentation::UintPtr());
      V<Word32> check =
          __ Word32Equal(flag_value, __ Word32Constant(new_value ? 0 : 1));
      AbortIfNot(check, new_value ? AbortReason::kUnexpectedThreadInWasmSet
                                  : AbortReason::kUnexpectedThreadInWasmUnset);
    }

    __ Store(thread_in_wasm_flag_address, __ Word32Constant(new_value ? 1 : 0),
             StoreOp::Kind::RawAligned(), MemoryRepresentation::Int32(),
             compiler::WriteBarrierKind::kNoWriteBarrier);
  }

  class ModifyThreadInWasmFlagScope {
   public:
    ModifyThreadInWasmFlagScope(
        WasmWrapperTSGraphBuilder* wasm_wrapper_graph_builder, Assembler& asm_)
        : wasm_wrapper_graph_builder_(wasm_wrapper_graph_builder) {
      if (!trap_handler::IsTrapHandlerEnabled()) return;

      thread_in_wasm_flag_address_ =
          asm_.Load(asm_.LoadRootRegister(), OptionalOpIndex::Invalid(),
                    LoadOp::Kind::RawAligned(), MemoryRepresentation::UintPtr(),
                    RegisterRepresentation::WordPtr(),
                    Isolate::thread_in_wasm_flag_address_offset());
      wasm_wrapper_graph_builder_->BuildModifyThreadInWasmFlagHelper(
          thread_in_wasm_flag_address_, true);
    }

    ModifyThreadInWasmFlagScope(const ModifyThreadInWasmFlagScope&) = delete;

    ~ModifyThreadInWasmFlagScope() {
      if (!trap_handler::IsTrapHandlerEnabled()) return;
      wasm_wrapper_graph_builder_->BuildModifyThreadInWasmFlagHelper(
          thread_in_wasm_flag_address_, false);
    }

   private:
    WasmWrapperTSGraphBuilder* wasm_wrapper_graph_builder_;
    V<WordPtr> thread_in_wasm_flag_address_;
  };

  V<Smi> LoadExportedFunctionIndexAsSmi(V<Tagged> exported_function_data) {
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
    return WasmGraphBuilderBase::GetTargetForBuiltinCall(builtin, stub_mode_);
  }

  template <typename Descriptor, typename... Args>
  OpIndex CallBuiltin(Builtin name, OpIndex frame_state,
                      Operator::Properties properties, Args... args) {
    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), Descriptor(), 0,
        frame_state.valid() ? CallDescriptor::kNeedsFrameState
                            : CallDescriptor::kNoFlags,
        Operator::kNoProperties, stub_mode_);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, __ graph_zone());
    V<WordPtr> call_target = GetTargetForBuiltinCall(name);
    return __ Call(call_target, frame_state, base::VectorOf({args...}),
                   ts_call_descriptor);
  }

  template <typename Descriptor, typename... Args>
  OpIndex CallBuiltin(Builtin name, Operator::Properties properties,
                      Args... args) {
    auto call_descriptor = compiler::Linkage::GetStubCallDescriptor(
        __ graph_zone(), Descriptor(), 0, CallDescriptor::kNoFlags,
        Operator::kNoProperties, stub_mode_);
    const TSCallDescriptor* ts_call_descriptor = TSCallDescriptor::Create(
        call_descriptor, compiler::CanThrow::kNo, __ graph_zone());
    V<WordPtr> call_target = GetTargetForBuiltinCall(name);
    return __ Call(call_target, {args...}, ts_call_descriptor);
  }

  V<Number> BuildChangeInt32ToNumber(V<Word32> value) {
    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    if (SmiValuesAre32Bits()) {
      return BuildChangeInt32ToSmi(value);
    }
    DCHECK(SmiValuesAre31Bits());

    // Double value to test if value can be a Smi, and if so, to convert it.
    V<Word32> add = __ Int32AddCheckOverflow(value, value);
    V<Word32> ovf = __ Projection<Word32>(add, 1);
    Variable result = __ NewVariable(RegisterRepresentation::Tagged());
    IF_NOT (ovf) {
      // If it didn't overflow, the result is {2 * value} as pointer-sized
      // value.
      __ SetVariable(result,
                     __ ChangeInt32ToIntPtr(__ Projection<Word32>(add, 0)));
    }
    ELSE {
      // Otherwise, call builtin, to convert to a HeapNumber.
      V<HeapNumber> call = CallBuiltin<WasmInt32ToHeapNumberDescriptor>(
          Builtin::kWasmInt32ToHeapNumber, Operator::kNoProperties, value);
      __ SetVariable(result, call);
    }
    END_IF
    return __ GetVariable(result);
  }

  V<Number> BuildChangeFloat32ToNumber(V<Float32> value) {
    return CallBuiltin<WasmFloat32ToNumberDescriptor>(
        Builtin::kWasmFloat32ToNumber, Operator::kNoProperties, value);
  }

  V<Number> BuildChangeFloat64ToNumber(V<Float64> value) {
    return CallBuiltin<WasmFloat64ToTaggedDescriptor>(
        Builtin::kWasmFloat64ToNumber, Operator::kNoProperties, value);
  }

  V<Tagged> ToJS(OpIndex ret, ValueType type, V<Context> context) {
    switch (type.kind()) {
      case wasm::kI32:
        return BuildChangeInt32ToNumber(ret);
      case wasm::kI64:
        return BuildChangeInt64ToBigInt(ret, stub_mode_);
      case wasm::kF32:
        return BuildChangeFloat32ToNumber(ret);
      case wasm::kF64:
        return BuildChangeFloat64ToNumber(ret);
      case wasm::kRef:
        switch (type.heap_representation()) {
          case wasm::HeapType::kEq:
          case wasm::HeapType::kI31:
          case wasm::HeapType::kStruct:
          case wasm::HeapType::kArray:
          case wasm::HeapType::kAny:
          case wasm::HeapType::kExtern:
          case wasm::HeapType::kString:
          case wasm::HeapType::kNone:
          case wasm::HeapType::kNoFunc:
          case wasm::HeapType::kNoExtern:
          case wasm::HeapType::kExn:
          case wasm::HeapType::kNoExn:
            return ret;
          case wasm::HeapType::kBottom:
          case wasm::HeapType::kStringViewWtf8:
          case wasm::HeapType::kStringViewWtf16:
          case wasm::HeapType::kStringViewIter:
            UNREACHABLE();
          case wasm::HeapType::kFunc:
          default:
            if (type.heap_representation() == wasm::HeapType::kFunc ||
                module_->has_signature(type.ref_index())) {
              // Typed function. Extract the external function.
              Variable maybe_external =
                  __ NewVariable(RegisterRepresentation::Tagged());
              __ SetVariable(maybe_external,
                             __ Load(ret, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::AnyTagged(),
                                     WasmInternalFunction::kExternalOffset));
              IF (__ TaggedEqual(__ GetVariable(maybe_external),
                                 LOAD_IMMUTABLE_ROOT(UndefinedValue))) {
                __ SetVariable(
                    maybe_external,
                    CallBuiltin<WasmInternalFunctionCreateExternalDescriptor>(
                        Builtin::kWasmInternalFunctionCreateExternal,
                        Operator::kNoProperties, ret, context));
              }
              END_IF
              return __ GetVariable(maybe_external);
            } else {
              return ret;
            }
        }
      case wasm::kRefNull:
        switch (type.heap_representation()) {
          case wasm::HeapType::kExtern:
          case wasm::HeapType::kNoExtern:
          case wasm::HeapType::kExn:
          case wasm::HeapType::kNoExn:
            return ret;
          case wasm::HeapType::kNone:
          case wasm::HeapType::kNoFunc:
            return LOAD_ROOT(NullValue);
          case wasm::HeapType::kEq:
          case wasm::HeapType::kStruct:
          case wasm::HeapType::kArray:
          case wasm::HeapType::kString:
          case wasm::HeapType::kI31:
          case wasm::HeapType::kAny: {
            Variable result = __ NewVariable(RegisterRepresentation::Tagged());
            IF_NOT (__ IsNull(ret, type)) {
              __ SetVariable(result, ret);
            }
            ELSE {
              __ SetVariable(result, LOAD_ROOT(NullValue));
            }
            END_IF
            return __ GetVariable(result);
          }
          case wasm::HeapType::kFunc:
          default: {
            if (type == wasm::kWasmFuncRef ||
                module_->has_signature(type.ref_index())) {
              Variable result =
                  __ NewVariable(RegisterRepresentation::Tagged());
              IF (__ IsNull(ret, type)) {
                __ SetVariable(result, LOAD_ROOT(NullValue));
              }
              ELSE {
                V<Tagged> maybe_external =
                    __ Load(ret, LoadOp::Kind::TaggedBase(),
                            MemoryRepresentation::AnyTagged(),
                            WasmInternalFunction::kExternalOffset);
                IF (__ TaggedEqual(maybe_external,
                                   LOAD_IMMUTABLE_ROOT(UndefinedValue))) {
                  V<Tagged> from_builtin =
                      CallBuiltin<WasmInternalFunctionCreateExternalDescriptor>(
                          Builtin::kWasmInternalFunctionCreateExternal,
                          Operator::kNoProperties, ret, context);
                  __ SetVariable(result, from_builtin);
                }
                ELSE {
                  __ SetVariable(result, maybe_external);
                }
                END_IF
              }
              END_IF
              return __ GetVariable(result);
            } else {
              Variable result =
                  __ NewVariable(RegisterRepresentation::Tagged());
              IF (__ IsNull(ret, type)) {
                __ SetVariable(result, LOAD_IMMUTABLE_ROOT(NullValue));
              }
              ELSE {
                __ SetVariable(result, ret);
              }
              END_IF
              return __ GetVariable(result);
            }
          }
        }
      case wasm::kRtt:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kS128:
      case wasm::kVoid:
      case wasm::kBottom:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        UNREACHABLE();
    }
  }

  // Generate a call to the AllocateJSArray builtin.
  V<JSArray> BuildCallAllocateJSArray(V<Number> array_length,
                                      V<Tagged> context) {
    // Since we don't check that args will fit in an array,
    // we make sure this is true based on statically known limits.
    static_assert(wasm::kV8MaxWasmFunctionReturns <=
                  JSArray::kInitialMaxFastElementArray);
    return CallBuiltin<WasmAllocateJSArrayDescriptor>(
        Builtin::kWasmAllocateJSArray, Operator::kEliminatable, array_length,
        context);
  }

  void BuildCallWasmFromWrapper(Zone* zone, const FunctionSig* sig,
                                V<WordPtr> callee,
                                V<HeapObject> implicit_first_arg,
                                base::SmallVector<OpIndex, 16> args,
                                base::SmallVector<OpIndex, 1>& returns) {
    const TSCallDescriptor* descriptor = TSCallDescriptor::Create(
        compiler::GetWasmCallDescriptor(__ graph_zone(), sig),
        compiler::CanThrow::kYes, __ graph_zone());

    args[0] = implicit_first_arg;
    OpIndex call = __ Call(callee, OpIndex::Invalid(), base::VectorOf(args),
                           descriptor, OpEffects().CanCallAnything());

    if (sig->return_count() == 1) {
      returns[0] = AnnotateResultIfReference(call, sig->GetReturn(0));
    } else if (sig->return_count() > 1) {
      for (uint32_t i = 0; i < sig->return_count(); i++) {
        wasm::ValueType type = sig->GetReturn(i);
        returns[i] = AnnotateResultIfReference(
            __ Projection(call, i, RepresentationFor(type)), type);
      }
    }
  }

  OpIndex BuildCallAndReturn(bool is_import, V<Context> js_context,
                             V<Tagged> function_data,
                             base::SmallVector<OpIndex, 16> args,
                             bool do_conversion, bool set_in_wasm_flag) {
    const int rets_count = static_cast<int>(sig_->return_count());
    base::SmallVector<OpIndex, 1> rets(rets_count);

    // Set the ThreadInWasm flag before we do the actual call.
    {
      base::Optional<ModifyThreadInWasmFlagScope>
          modify_thread_in_wasm_flag_builder;
      if (set_in_wasm_flag) {
        modify_thread_in_wasm_flag_builder.emplace(this, Asm());
      }

      V<HeapObject> instance_object =
          __ Load(function_data, LoadOp::Kind::TaggedBase(),
                  MemoryRepresentation::TaggedPointer(),
                  WasmExportedFunctionData::kInstanceOffset);
      V<WasmTrustedInstanceData> instance_data =
          LoadTrustedDataFromInstanceObject(instance_object);

      if (is_import) {
        // Call to an imported function.
        // Load function index from {WasmExportedFunctionData}.
        V<WordPtr> function_index = BuildChangeSmiToIntPtr(
            LoadExportedFunctionIndexAsSmi(function_data));
        auto [target, ref] =
            BuildImportedFunctionTargetAndRef(function_index, instance_data);
        BuildCallWasmFromWrapper(__ phase_zone(), sig_, target, ref, args,
                                 rets);
      } else {
        // Call to a wasm function defined in this module.
        // The (cached) call target is the jump table slot for that function.
        V<Tagged> internal = __ Load(function_data, LoadOp::Kind::TaggedBase(),
                                     MemoryRepresentation::TaggedPointer(),
                                     WasmFunctionData::kInternalOffset);
#ifdef V8_ENABLE_SANDBOX
        V<Word32> target_handle =
            __ Load(internal, LoadOp::Kind::TaggedBase(),
                    MemoryRepresentation::Uint32(),
                    WasmInternalFunction::kCallTargetOffset);
        V<WordPtr> callee = __ DecodeExternalPointer(
            target_handle, kWasmInternalFunctionCallTargetTag);
#else
        V<WordPtr> callee = __ Load(internal, LoadOp::Kind::TaggedBase(),
                                    MemoryRepresentation::UintPtr(),
                                    WasmInternalFunction::kCallTargetOffset);
#endif
        BuildCallWasmFromWrapper(__ phase_zone(), sig_, callee, instance_data,
                                 args, rets);
      }
    }

    V<Tagged> jsval;
    if (sig_->return_count() == 0) {
      jsval = LOAD_IMMUTABLE_ROOT(UndefinedValue);
    } else if (sig_->return_count() == 1) {
      jsval = do_conversion ? ToJS(rets[0], sig_->GetReturn(), js_context)
                            : rets[0];
    } else {
      int32_t return_count = static_cast<int32_t>(sig_->return_count());
      V<Smi> size = __ SmiConstant(Smi::FromInt(return_count));

      jsval = BuildCallAllocateJSArray(size, js_context);

      V<FixedArray> fixed_array = __ Load(jsval, LoadOp::Kind::TaggedBase(),
                                          MemoryRepresentation::TaggedPointer(),
                                          JSObject::kElementsOffset);

      for (int i = 0; i < return_count; ++i) {
        V<Tagged> value = ToJS(rets[i], sig_->GetReturn(i), js_context);
        __ StoreFixedArrayElement(fixed_array, i, value,
                                  compiler::kFullWriteBarrier);
      }
    }
    return jsval;
  }

  void BuildJSToWasmWrapper(
      bool is_import, bool do_conversion = true,
      compiler::turboshaft::OptionalOpIndex frame_state =
          compiler::turboshaft::OptionalOpIndex::Invalid(),
      bool set_in_wasm_flag = true) {
    const int wasm_param_count = static_cast<int>(sig_->parameter_count());

    __ Bind(__ NewBlock());

    // Create the js_closure and js_context parameters.
    V<JSFunction> js_closure =
        __ Parameter(compiler::Linkage::kJSCallClosureParamIndex,
                     RegisterRepresentation::Tagged());
    V<Context> js_context = __ Parameter(
        compiler::Linkage::GetJSCallContextParamIndex(wasm_param_count + 1),
        RegisterRepresentation::Tagged());
    V<SharedFunctionInfo> shared =
        __ Load(js_closure, LoadOp::Kind::TaggedBase(),
                MemoryRepresentation::TaggedPointer(),
                JSFunction::kSharedFunctionInfoOffset);
    V<Tagged> function_data = __ Load(shared, LoadOp::Kind::TaggedBase(),
                                      MemoryRepresentation::TaggedPointer(),
                                      SharedFunctionInfo::kFunctionDataOffset);

    if (!wasm::IsJSCompatibleSignature(sig_)) {
      // Throw a TypeError. Use the js_context of the calling javascript
      // function (passed as a parameter), such that the generated code is
      // js_context independent.
      CallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                  js_context);
      __ Unreachable();
      return;
    }

    const int args_count = wasm_param_count + 1;  // +1 for wasm_code.

    // // Check whether the signature of the function allows for a fast
    // // transformation (if any params exist that need transformation).
    // // Create a fast transformation path, only if it does.
    bool include_fast_path =
        do_conversion && wasm_param_count > 0 && QualifiesForFastTransform();

    // Prepare Param() nodes. Param() nodes can only be created once,
    // so we need to use the same nodes along all possible transformation paths.
    base::SmallVector<OpIndex, 16> params(args_count);
    for (int i = 0; i < wasm_param_count; ++i) {
      params[i + 1] = __ Parameter(i + 1, RegisterRepresentation::Tagged());
    }

    Label<Tagged> done(&Asm());
    V<Tagged> jsval;
    if (include_fast_path) {
      TSBlock* slow_path = __ NewBlock();
      // Check if the params received on runtime can be actually transformed
      // using the fast transformation. When a param that cannot be transformed
      // fast is encountered, skip checking the rest and fall back to the slow
      // path.
      for (int i = 0; i < wasm_param_count; ++i) {
        CanTransformFast(params[i + 1], sig_->GetParam(i), slow_path);
      }
      // Convert JS parameters to wasm numbers using the fast transformation
      // and build the call.
      base::SmallVector<OpIndex, 16> args(args_count);
      for (int i = 0; i < wasm_param_count; ++i) {
        OpIndex wasm_param = FromJSFast(params[i + 1], sig_->GetParam(i));
        args[i + 1] = wasm_param;
      }
      jsval = BuildCallAndReturn(is_import, js_context, function_data, args,
                                 do_conversion, set_in_wasm_flag);
      GOTO(done, jsval);
      __ Bind(slow_path);
    }
    // Convert JS parameters to wasm numbers using the default transformation
    // and build the call.
    base::SmallVector<OpIndex, 16> args(args_count);
    for (int i = 0; i < wasm_param_count; ++i) {
      if (do_conversion) {
        args[i + 1] = FromJS(params[i + 1], js_context, sig_->GetParam(i),
                             module_, frame_state);
      } else {
        OpIndex wasm_param = params[i + 1];

        // For Float32 parameters
        // we set UseInfo::CheckedNumberOrOddballAsFloat64 in
        // simplified-lowering and we need to add here a conversion from Float64
        // to Float32.
        if (sig_->GetParam(i).kind() == wasm::kF32) {
          wasm_param = __ ChangeFloat64ToFloat32(wasm_param);
        }
        args[i + 1] = wasm_param;
      }
    }

    jsval = BuildCallAndReturn(is_import, js_context, function_data, args,
                               do_conversion, set_in_wasm_flag);
    // If both the default and a fast transformation paths are present,
    // get the return value based on the path used.
    if (include_fast_path) {
      GOTO(done, jsval);
      BIND(done, result);
      __ Return(result);
    } else {
      __ Return(jsval);
    }
  }

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

  V<WordPtr> BuildChangeSmiToIntPtr(OpIndex value) {
    return COMPRESS_POINTERS_BOOL ? __ ChangeInt32ToIntPtr(
                                        __ Word32ShiftRightArithmetic(
                                            value,
                                            BuildSmiShiftBitsConstant32()))
                                  : __ WordPtrShiftRightArithmetic(
                                        value, BuildSmiShiftBitsConstant());
  }

  V<Float64> HeapNumberToFloat64(V<HeapNumber> input) {
    return __ template LoadField<Float64>(
        input, compiler::AccessBuilder::ForHeapNumberValue());
  }

  OpIndex FromJSFast(OpIndex input, wasm::ValueType type) {
    switch (type.kind()) {
      case wasm::kI32:
        return BuildChangeSmiToInt32(input);
      case wasm::kF32: {
        Variable result = __ NewVariable(RegisterRepresentation::Float32());
        IF (__ IsSmi(input)) {
          __ SetVariable(result, __ ChangeInt32ToFloat32(__ UntagSmi(input)));
        }
        ELSE {
          __ SetVariable(result,
                         __ ChangeFloat64ToFloat32(HeapNumberToFloat64(input)));
        }
        END_IF
        return __ GetVariable(result);
      }
      case wasm::kF64: {
        Variable result = __ NewVariable(RegisterRepresentation::Float64());
        IF (__ IsSmi(input)) {
          __ SetVariable(result, __ ChangeInt32ToFloat64(__ UntagSmi(input)));
        }
        ELSE {
          __ SetVariable(result, HeapNumberToFloat64(input));
        }
        END_IF
        return __ GetVariable(result);
      }
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kI64:
      case wasm::kRtt:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        UNREACHABLE();
    }
  }

  OpIndex LoadInstanceType(V<Map> map) {
    return __ Load(map, LoadOp::Kind::TaggedBase().Immutable(),
                   MemoryRepresentation::Uint16(), Map::kInstanceTypeOffset);
  }

  OpIndex BuildCheckString(OpIndex input, OpIndex js_context, ValueType type) {
    auto done = __ NewBlock();
    auto type_error = __ NewBlock();
    Variable result = __ NewVariable(RegisterRepresentation::Tagged());
    __ SetVariable(result, LOAD_IMMUTABLE_ROOT(WasmNull));
    __ GotoIf(__ IsSmi(input), type_error, BranchHint::kFalse);
    if (type.is_nullable()) {
      auto not_null = __ NewBlock();
      __ GotoIfNot(__ IsNull(input, wasm::kWasmExternRef), not_null);
      __ Goto(done);
      __ Bind(not_null);
    }
    V<Map> map = LoadMap(input);
    OpIndex instance_type = LoadInstanceType(map);
    OpIndex check = __ Uint32LessThan(instance_type,
                                      __ Word32Constant(FIRST_NONSTRING_TYPE));
    __ SetVariable(result, input);
    __ GotoIf(check, done, BranchHint::kTrue);
    __ Goto(type_error);
    __ Bind(type_error);
    CallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                js_context);
    __ Unreachable();
    __ Bind(done);
    return __ GetVariable(result);
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
    Variable result = __ NewVariable(RegisterRepresentation::Word32());
    IF (__ IsSmi(value)) {
      __ SetVariable(result, BuildChangeSmiToInt32(value));
    }
    ELSE {
      OpIndex call =
          frame_state.valid()
              ? CallBuiltin<WasmTaggedNonSmiToInt32Descriptor>(
                    Builtin::kWasmTaggedNonSmiToInt32, frame_state.value(),
                    Operator::kNoProperties, value, context)
              : CallBuiltin<WasmTaggedNonSmiToInt32Descriptor>(
                    Builtin::kWasmTaggedNonSmiToInt32, Operator::kNoProperties,
                    value, context);
      __ SetVariable(result, call);
      // The source position here is needed for asm.js, see the comment on the
      // source position of the call to JavaScript in the wasm-to-js wrapper.
      __ output_graph().source_positions()[call] = SourcePosition(1);
    }
    END_IF
    return __ GetVariable(result);
  }

  CallDescriptor* GetBigIntToI64CallDescriptor(bool needs_frame_state) {
    return wasm::GetWasmEngine()->call_descriptors()->GetBigIntToI64Descriptor(
        stub_mode_, needs_frame_state);
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
        call_descriptor, compiler::CanThrow::kNo, __ graph_zone());
    return frame_state.valid()
               ? __ Call(target, frame_state.value(),
                         base::VectorOf({input, context}), ts_call_descriptor)
               : __ Call(target, {input, context}, ts_call_descriptor);
  }

  OpIndex FromJS(OpIndex input, OpIndex context, ValueType type,
                 const WasmModule* module, OptionalOpIndex frame_state) {
    switch (type.kind()) {
      case wasm::kRef:
      case wasm::kRefNull: {
        switch (type.heap_representation()) {
          // TODO(14034): Add more fast paths?
          case wasm::HeapType::kExtern:
          case wasm::HeapType::kNoExtern:
            if (type.kind() == wasm::kRef) {
              IF (__ TaggedEqual(input, LOAD_IMMUTABLE_ROOT(NullValue))) {
                CallRuntime(__ phase_zone(), Runtime::kWasmThrowJSTypeError, {},
                            context);
                __ Unreachable();
              }
              END_IF
              return input;
            }
            return input;
          case wasm::HeapType::kString:
            return BuildCheckString(input, context, type);
          case wasm::HeapType::kExn:
          case wasm::HeapType::kNoExn:
            return input;
          case wasm::HeapType::kNone:
          case wasm::HeapType::kNoFunc:
          case wasm::HeapType::kI31:
          case wasm::HeapType::kAny:
          case wasm::HeapType::kFunc:
          case wasm::HeapType::kStruct:
          case wasm::HeapType::kArray:
          case wasm::HeapType::kEq:
          default: {
            // Make sure ValueType fits in a Smi.
            static_assert(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);

            if (type.has_index()) {
              DCHECK_NOT_NULL(module);
              uint32_t canonical_index =
                  module->isorecursive_canonical_type_ids[type.ref_index()];
              type = wasm::ValueType::RefMaybeNull(canonical_index,
                                                   type.nullability());
            }

            std::initializer_list<const OpIndex> inputs = {
                input, __ IntPtrConstant(
                           IntToSmi(static_cast<int>(type.raw_bit_field())))};
            return CallRuntime(__ phase_zone(), Runtime::kWasmJSToWasmObject,
                               inputs, context);
          }
        }
      }
      case wasm::kF32:
        return __ ChangeFloat64ToFloat32(
            BuildChangeTaggedToFloat64(input, context, frame_state));

      case wasm::kF64:
        return BuildChangeTaggedToFloat64(input, context, frame_state);

      case wasm::kI32:
        return BuildChangeTaggedToInt32(input, context, frame_state);

      case wasm::kI64:
        // i64 values can only come from BigInt.
        return BuildChangeBigIntToInt64(input, context, frame_state);

      case wasm::kRtt:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        UNREACHABLE();
    }
  }

  bool QualifiesForFastTransform() {
    const int wasm_count = static_cast<int>(sig_->parameter_count());
    for (int i = 0; i < wasm_count; ++i) {
      wasm::ValueType type = sig_->GetParam(i);
      switch (type.kind()) {
        case wasm::kRef:
        case wasm::kRefNull:
        case wasm::kI64:
        case wasm::kRtt:
        case wasm::kS128:
        case wasm::kI8:
        case wasm::kI16:
        case wasm::kBottom:
        case wasm::kVoid:
          return false;
        case wasm::kI32:
        case wasm::kF32:
        case wasm::kF64:
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

  V<Map> LoadMap(V<Tagged> object) {
    // TODO(thibaudm): Handle map packing.
    OpIndex map_word = __ Load(object, LoadOp::Kind::TaggedBase(),
                               MemoryRepresentation::TaggedPointer(), 0);
#ifdef V8_MAP_PACKING
    return UnpackMapWord(map_word);
#else
    return map_word;
#endif
  }

  void CanTransformFast(OpIndex input, wasm::ValueType type,
                        TSBlock* slow_path) {
    switch (type.kind()) {
      case wasm::kI32: {
        __ GotoIfNot(__ IsSmi(input), slow_path);
        return;
      }
      case wasm::kF32:
      case wasm::kF64: {
        TSBlock* done = __ NewBlock();
        __ GotoIf(__ IsSmi(input), done);
        V<Map> map = LoadMap(input);
        V<Map> heap_number_map = LOAD_ROOT(HeapNumberMap);
        // TODO(thibaudm): Handle map packing.
        V<Word32> is_heap_number = __ TaggedEqual(heap_number_map, map);
        __ GotoIf(is_heap_number, done);
        __ Goto(slow_path);
        __ Bind(done);
        return;
      }
      case wasm::kRef:
      case wasm::kRefNull:
      case wasm::kI64:
      case wasm::kRtt:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        UNREACHABLE();
    }
  }

 private:
  const WasmModule* module_;
  const wasm::FunctionSig* const sig_;
  StubCallMode stub_mode_;
};

void BuildWasmWrapper(AccountingAllocator* allocator,
                      compiler::turboshaft::Graph& graph, CodeKind code_kind,
                      const wasm::FunctionSig* sig, bool is_import,
                      const WasmModule* module) {
  Zone zone(allocator, ZONE_NAME);
  WasmGraphBuilderBase::Assembler assembler(graph, graph, &zone);
  WasmWrapperTSGraphBuilder builder(&zone, assembler, module, sig,
                                    StubCallMode::kCallBuiltinPointer);
  if (code_kind == CodeKind::JS_TO_WASM_FUNCTION) {
    builder.BuildJSToWasmWrapper(is_import);
  } else {
    // TODO(thibaudm): Port remaining wrappers.
    UNREACHABLE();
  }
}

}  // namespace v8::internal::wasm
