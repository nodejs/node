// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_REDUCER_INL_H_
#define V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_REDUCER_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/compiler/frame-states.h"
#include "src/compiler/js-inlining.h"
#include "src/compiler/turboshaft/assembler.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/wasm-assembler-helpers.h"
#include "src/compiler/turboshaft/wasm-wrappers-inl.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/turboshaft-graph-interface-inl.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8::internal::compiler::turboshaft {

#include "src/compiler/turboshaft/define-assembler-macros.inc"

#define TRACE(x)                         \
  do {                                   \
    if (v8_flags.trace_turbo_inlining) { \
      StdoutStream() << x << std::endl;  \
    }                                    \
  } while (false)

template <class Next>
class WasmInJSInliningReducer : public Next {
 public:
  TURBOSHAFT_REDUCER_BOILERPLATE(WasmInJSInlining)

  // Strip ProcessWasmArgumentOp, returning just the unwrapped value so the op
  // never survives to the instruction selector. The eager frame state is
  // recovered by REDUCE_INPUT_GRAPH(Call) which has direct access to the input
  // graph CallOp and its arguments.
  V<Object> REDUCE(ProcessWasmArgument)(V<Object> value,
                                        V<FrameState> frame_state) {
    // TODO(493307329): It would be cleaner to let the ProcessWasmArgument
    // actually perform the argument conversion, and get rid of the weird logic
    // in REDUCE_INPUT_GRAPH(Call). The ProcessWasmArgument should lower to the
    // actual argument conversion (the FromJS() logic) and then the reducer for
    // the call wouldn't do the argument conversion, but only the result
    // conversion.
    return value;
  }

  // Intercept the input-graph CallOp to find ProcessWasmArgumentOp arguments
  // and extract the caller frame state (crbug.com/493307329). This runs before
  // inputs are mapped, so we can inspect the input graph directly.
  // The stash is consumed by REDUCE(Call) within the same invocation chain,
  // so no interleaving or multi-call confusion is possible.
  OpIndex REDUCE_INPUT_GRAPH(Call)(OpIndex ig_index, const CallOp& op) {
    DCHECK(!pending_caller_frame_state_.has_value());
    if (op.descriptor->js_wasm_call_parameters) {
      for (OpIndex arg : op.arguments()) {
        if (__ input_graph().Get(arg).template Is<ProcessWasmArgumentOp>()) {
          pending_caller_frame_state_ =
              __ MapToNewGraph(__ input_graph()
                                   .Get(arg)
                                   .template Cast<ProcessWasmArgumentOp>()
                                   .frame_state());
          break;
        }
      }
    }
    return Next::ReduceInputGraphCall(ig_index, op);
  }

  V<Any> REDUCE(Call)(V<CallTarget> callee, OptionalV<FrameState> frame_state,
                      base::Vector<const OpIndex> arguments,
                      const TSCallDescriptor* descriptor, OpEffects effects) {
    // Consume the caller frame state stashed by REDUCE_INPUT_GRAPH(Call).
    OptionalV<FrameState> caller_frame_state = pending_caller_frame_state_;
    pending_caller_frame_state_ = {};

    if (descriptor->js_wasm_call_parameters) {
      // TODO(353475584): Wrapper and body inlining in Turboshaft is only
      // implemented for the Turbolev frontend right now.
      CHECK(v8_flags.turbolev);
      CHECK(v8_flags.turbolev_inline_js_wasm_wrappers);

      // We need a `FrameState` for building correct stack traces when inlining
      // potentially trapping Wasm operations.
      CHECK(frame_state.has_value());

      wasm::NativeModule* native_module =
          descriptor->js_wasm_call_parameters->native_module();
      const wasm::WasmModule* module = native_module->module();

      // The `WasmModule` on the `PipelineData` (set by the Turbolev frontend)
      // and the one from the `NativeModule` should be the same.
      CHECK_EQ(__ data()->wasm_module(), module);

      uint32_t func_idx = descriptor->js_wasm_call_parameters->function_index();

      V<JSFunction> js_closure = V<JSFunction>::Cast(callee);
      V<Context> js_context = V<Context>::Cast(arguments[arguments.size() - 1]);

      // Prepare data for the Wasm-in-JS body inlining, if enabled.
      std::optional<WasmInlinedFunctionData> inlined_function_data;
      if (v8_flags.turboshaft_wasm_in_js_inlining) {
        V<AnyOrNone> origin = __ current_operation_origin();
        CHECK(origin.valid());
        SourcePosition call_pos = __ input_graph().source_positions()[origin];
        CHECK(call_pos.IsKnown());
        int inlining_id = __ data() -> info()->AddInlinedFunction(
            descriptor->js_wasm_call_parameters->shared_fct_info().object(),
            Handle<BytecodeArray>(), call_pos);
        V<FrameState> wasm_inlined_frame_state =
            CreateWasmInlinedIntoJSFrameState(
                js_context, frame_state.value(),
                descriptor->js_wasm_call_parameters->shared_fct_info());
        inlined_function_data = {native_module, func_idx,
                                 wasm_inlined_frame_state, inlining_id};
      }

      // The JS-to-Wasm wrapper is always inlined at this point, because the
      // bailouts for that have already happened in `turbolev-graph-builder.cc`.
      // The Wasm-in-JS body inlining may still bailout in `TryInlineWasmBody`.
      const wasm::WasmFunction& func = module->functions[func_idx];
      wasm::CanonicalTypeIndex sig_id =
          module->canonical_sig_id(func.sig_index);
      const wasm::CanonicalSig* sig =
          wasm::GetTypeCanonicalizer()->LookupFunctionSignature(sig_id);
      constexpr bool kInliningIntoJs = true;
      using GraphBuilder = WasmWrapperTSGraphBuilder<assembler_t>;
      GraphBuilder builder(Asm().phase_zone(), Asm(), sig, kInliningIntoJs,
                           inlined_function_data);
      V<FrameState> continuation_frame_state =
          CreateJSWasmCallBuiltinContinuationFrameState(
              js_context, frame_state.value(), sig);
      // When inlining into JS, pass the caller frame state (a pre-call Maglev
      // checkpoint) so that we can deopt if a parameter conversion fails
      // (crbug.com/493307329). For numeric types, this enables eager deopt
      // guards that eagerly deopt if we fall off the conversion fastpath, e.g.,
      // not Smi or HeapNumber. In particular, we do not call builtins that
      // could transitively cause a lazy deopt via valueOf/Symbol.toPrimitive.
      V<Any> result = builder.BuildJSToWasmWrapper(
          js_closure, js_context, arguments, continuation_frame_state,
          descriptor->lazy_deopt_on_throw, caller_frame_state);
      // The wrapper inlining will always succeed and produce a JS value, except
      // if the wrapper or body inlining _unconditionally_ produced a trap /
      // type error and we are thus in unreachable code.
      CHECK_EQ(!result.valid(), __ generating_unreachable_operations());
      return result;
    }

    // Do not inline this particular call (because of a bailout or because it
    // is not a Wasm call to begin with.)
    return Next::ReduceCall(callee, frame_state, arguments, descriptor,
                            effects);
  }

  WasmBodyInliningResult TryInlineWasmBody(
      const WasmInlinedFunctionData& inlined_data,
      base::Vector<const OpIndex> arguments,
      LazyDeoptOnThrow lazy_deopt_on_throw);

  void set_current_wasm_source_position(SourcePosition pos) {
    current_wasm_source_position_ = pos;
  }

  SourcePosition GetSourcePositionFor(OpIndex index) {
    if (current_wasm_source_position_.IsKnown()) {
      return current_wasm_source_position_;
    }
    return Next::GetSourcePositionFor(index);
  }

 private:
  V<FrameState> CreateWasmInlinedIntoJSFrameState(
      V<Context> js_context, V<FrameState> outer_frame_state,
      SharedFunctionInfoRef shared_function_info);
  V<FrameState> CreateJSWasmCallBuiltinContinuationFrameState(
      V<Context> js_context, V<FrameState> outer_frame_state,
      const wasm::CanonicalSig* signature);

  // Caller frame state set by REDUCE_INPUT_GRAPH(Call), consumed by the
  // immediately following REDUCE(Call) within the same ReduceInputGraphCall
  // invocation (crbug.com/493307329).
  OptionalV<FrameState> pending_caller_frame_state_;

  SourcePosition current_wasm_source_position_ = SourcePosition::Unknown();
};

using wasm::ValueType;
using wasm::WasmOpcode;

namespace detail {

struct Value : public wasm::ValueBase<wasm::Decoder::NoValidationTag> {
  V<Any> op = V<Any>::Invalid();

  template <typename T>
  V<T> get() const {
    return V<T>::Cast(op);
  }

  template <typename... Args>
  explicit Value(Args&&... args) V8_NOEXCEPT
      : ValueBase(std::forward<Args>(args)...) {}
};

}  // namespace detail

template <class Assembler>
class WasmInJsInliningInterface {
 public:
  using ValidationTag = wasm::Decoder::NoValidationTag;
  using FullDecoder =
      wasm::WasmFullDecoder<ValidationTag, WasmInJsInliningInterface>;
  using Value = detail::Value;
  // TODO(dlehmann,353475584): Introduce a proper `Control` struct/class, once
  // we want to support control-flow in the inlinee.
  using Control = wasm::ControlBase<Value, ValidationTag>;
  static constexpr bool kUsesPoppedArgs = false;

  WasmInJsInliningInterface(Assembler& assembler,
                            base::Vector<const OpIndex> arguments,
                            V<WasmTrustedInstanceData> trusted_instance_data,
                            V<FrameState> frame_state, int inlining_id)
      : asm_(assembler),
        locals_(assembler.phase_zone()),
        arguments_(arguments),
        trusted_instance_data_(trusted_instance_data),
        frame_state_(frame_state),
        inlining_id_(inlining_id) {}

  ~WasmInJsInliningInterface() {
    // End of inlining the Wasm function, so unset the Wasm source position.
    __ set_current_wasm_source_position(SourcePosition::Unknown());
  }

  WasmBodyInliningResult result() const { return result_; }

  void OnFirstError(FullDecoder*) {}

  void Bailout(FullDecoder* decoder) {
    decoder->errorf("unsupported operation: %s",
                    decoder->SafeOpcodeNameAt(decoder->pc()));
  }

#define BAILOUT_WASM_OP(name)                  \
  template <typename... Args>                  \
  void name(FullDecoder* decoder, Args&&...) { \
    Bailout(decoder);                          \
  }

  void StartFunction(FullDecoder* decoder) {
    // Make sure we have a source positions for generated code before the first
    // Wasm opcode, e.g., initializing locals.
    set_current_wasm_source_position(decoder);

    locals_.resize(decoder->num_locals());

    // Initialize the function parameters, which are part of the local space.
    CHECK_LE(arguments_.size(), locals_.size());
    std::copy(arguments_.begin(), arguments_.end(), locals_.begin());

    // Initialize the non-parameter locals.
    uint32_t index = static_cast<uint32_t>(decoder->sig_->parameter_count());
    CHECK_EQ(index, arguments_.size());
    while (index < decoder->num_locals()) {
      ValueType type = decoder->local_type(index);
      V<Any> op;
      if (!type.is_defaultable()) {
        DCHECK(type.is_ref());
        op = __ RootConstant(RootIndex::kOptimizedOut);
      } else {
        op = __ WasmDefaultValue(type);
      }
      while (index < decoder->num_locals() &&
             decoder->local_type(index) == type) {
        locals_[index++] = op;
      }
    }
  }
  void StartFunctionBody(FullDecoder* decoder, Control* block) {}
  void FinishFunction(FullDecoder* decoder) {}

  void NextInstruction(FullDecoder* decoder, WasmOpcode) {
    set_current_wasm_source_position(decoder);
  }

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {
    // This is just for testing bailouts in Liftoff, here it's just a nop.
  }

  void UnOp(FullDecoder* decoder, WasmOpcode opcode, const Value& value,
            Value* result) {
    result->op = UnOpImpl(decoder, opcode, value.op, value.type);
  }
  void BinOp(FullDecoder* decoder, WasmOpcode opcode, const Value& lhs,
             const Value& rhs, Value* result) {
    result->op = BinOpImpl(decoder, opcode, lhs.op, rhs.op);
  }

  OpIndex UnOpImpl(FullDecoder* decoder, WasmOpcode opcode, OpIndex arg,
                   ValueType input_type /* for ref.is_null only*/) {
    switch (opcode) {
      case wasm::kExprI32Eqz:
        return __ Word32Equal(arg, 0);
      case wasm::kExprF32Abs:
        return __ Float32Abs(arg);
      case wasm::kExprF32Neg:
        return __ Float32Negate(arg);
      case wasm::kExprF32Sqrt:
        return __ Float32Sqrt(arg);
      case wasm::kExprF64Abs:
        return __ Float64Abs(arg);
      case wasm::kExprF64Neg:
        return __ Float64Negate(arg);
      case wasm::kExprF64Sqrt:
        return __ Float64Sqrt(arg);
      case wasm::kExprF64SConvertI32:
        return __ ChangeInt32ToFloat64(arg);
      case wasm::kExprF64UConvertI32:
        return __ ChangeUint32ToFloat64(arg);
      case wasm::kExprF32SConvertI32:
        return __ ChangeInt32ToFloat32(arg);
      case wasm::kExprF32UConvertI32:
        return __ ChangeUint32ToFloat32(arg);
      case wasm::kExprF32ConvertF64:
        return __ TruncateFloat64ToFloat32(arg);
      case wasm::kExprF64ConvertF32:
        return __ ChangeFloat32ToFloat64(arg);
      case wasm::kExprF32ReinterpretI32:
        return __ BitcastWord32ToFloat32(arg);
      case wasm::kExprI32ReinterpretF32:
        return __ BitcastFloat32ToWord32(arg);
      case wasm::kExprI32Clz:
        return __ Word32CountLeadingZeros(arg);
      case wasm::kExprI32SExtendI8:
        return __ Word32SignExtend8(arg);
      case wasm::kExprI32SExtendI16:
        return __ Word32SignExtend16(arg);
      case wasm::kExprRefIsNull:
        return __ IsNull(arg, input_type);
      case wasm::kExprRefAsNonNull:
        // We abuse ref.as_non_null opcode (which is ordinarily a binary op and
        // not otherwise used in this switch) as a sentinel for the negation of
        // ref.is_null. See `function-body-decoder-impl.h`
        return __ Word32Equal(__ IsNull(arg, input_type), 0);
      case wasm::kExprAnyConvertExtern:
        return __ AnyConvertExtern(arg, input_type.is_shared(),
                                   input_type.is_nullable());
      case wasm::kExprExternConvertAny:
        return __ ExternConvertAny(arg, input_type.is_nullable());

      // We currently don't support operations that:
      // - could trap,
      // - call a builtin,
      // - need the instance,
      // - require later Int64Lowering (which is currently only compatible with
      //   the Wasm pipeline),
      // - are actually asm.js opcodes and not spec'ed in Wasm.
      case wasm::kExprI32ConvertI64:
      case wasm::kExprI64SConvertI32:
      case wasm::kExprI64UConvertI32:
      case wasm::kExprF64ReinterpretI64:
      case wasm::kExprI64ReinterpretF64:
      case wasm::kExprI64Clz:
      case wasm::kExprI64Eqz:
      case wasm::kExprI64SExtendI8:
      case wasm::kExprI64SExtendI16:
      case wasm::kExprI64SExtendI32:
      case wasm::kExprI32SConvertF32:
      case wasm::kExprI32UConvertF32:
      case wasm::kExprI32SConvertF64:
      case wasm::kExprI32UConvertF64:
      case wasm::kExprI64SConvertF32:
      case wasm::kExprI64UConvertF32:
      case wasm::kExprI64SConvertF64:
      case wasm::kExprI64UConvertF64:
      case wasm::kExprI32SConvertSatF32:
      case wasm::kExprI32UConvertSatF32:
      case wasm::kExprI32SConvertSatF64:
      case wasm::kExprI32UConvertSatF64:
      case wasm::kExprI64SConvertSatF32:
      case wasm::kExprI64UConvertSatF32:
      case wasm::kExprI64SConvertSatF64:
      case wasm::kExprI64UConvertSatF64:
      case wasm::kExprI32Ctz:
      case wasm::kExprI32Popcnt:
      case wasm::kExprF32Floor:
      case wasm::kExprF32Ceil:
      case wasm::kExprF32Trunc:
      case wasm::kExprF32NearestInt:
      case wasm::kExprF64Floor:
      case wasm::kExprF64Ceil:
      case wasm::kExprF64Trunc:
      case wasm::kExprF64NearestInt:
      case wasm::kExprI64Ctz:
      case wasm::kExprI64Popcnt:
      case wasm::kExprF32SConvertI64:
      case wasm::kExprF32UConvertI64:
      case wasm::kExprF64SConvertI64:
      case wasm::kExprF64UConvertI64:
        // Not supported, but we are aware of it. Bailout below.
        break;

      // asm.js:
      case wasm::kExprF64Acos:
      case wasm::kExprF64Asin:
      case wasm::kExprF64Atan:
      case wasm::kExprF64Cos:
      case wasm::kExprF64Sin:
      case wasm::kExprF64Tan:
      case wasm::kExprF64Exp:
      case wasm::kExprF64Log:
      case wasm::kExprI32AsmjsLoadMem8S:
      case wasm::kExprI32AsmjsLoadMem8U:
      case wasm::kExprI32AsmjsLoadMem16S:
      case wasm::kExprI32AsmjsLoadMem16U:
      case wasm::kExprI32AsmjsLoadMem:
      case wasm::kExprF32AsmjsLoadMem:
      case wasm::kExprF64AsmjsLoadMem:
      case wasm::kExprI32AsmjsSConvertF32:
      case wasm::kExprI32AsmjsUConvertF32:
      case wasm::kExprI32AsmjsSConvertF64:
      case wasm::kExprI32AsmjsUConvertF64:
        // We already bailout for asm.js modules earlier, so we should never
        // see these operations.
        UNREACHABLE();

      default:
        // Fuzzers should alert us when we forget ops, but it's not security
        // critical, so we just bailout below as well.
        DCHECK_WITH_MSG(false, "unhandled unary op");
    }

    Bailout(decoder);
    return V<Any>::Invalid();
  }

  V<Any> BinOpImpl(FullDecoder* decoder, WasmOpcode opcode, OpIndex lhs,
                   OpIndex rhs) {
    switch (opcode) {
      case wasm::kExprI32Add:
        return __ Word32Add(lhs, rhs);
      case wasm::kExprI32Sub:
        return __ Word32Sub(lhs, rhs);
      case wasm::kExprI32Mul:
        return __ Word32Mul(lhs, rhs);
      case wasm::kExprI32And:
        return __ Word32BitwiseAnd(lhs, rhs);
      case wasm::kExprI32Ior:
        return __ Word32BitwiseOr(lhs, rhs);
      case wasm::kExprI32Xor:
        return __ Word32BitwiseXor(lhs, rhs);
      case wasm::kExprI32Shl:
        // If possible, the bitwise-and gets optimized away later.
        return __ Word32ShiftLeft(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
      case wasm::kExprI32ShrS:
        return __ Word32ShiftRightArithmetic(lhs,
                                             __ Word32BitwiseAnd(rhs, 0x1f));
      case wasm::kExprI32ShrU:
        return __ Word32ShiftRightLogical(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
      case wasm::kExprI32Ror:
        return __ Word32RotateRight(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
      case wasm::kExprI32Rol:
        if (SupportedOperations::word32_rol()) {
          return __ Word32RotateLeft(lhs, __ Word32BitwiseAnd(rhs, 0x1f));
        } else {
          return __ Word32RotateRight(
              lhs, __ Word32Sub(32, __ Word32BitwiseAnd(rhs, 0x1f)));
        }
      case wasm::kExprI32Eq:
        return __ Word32Equal(lhs, rhs);
      case wasm::kExprI32Ne:
        return __ Word32Equal(__ Word32Equal(lhs, rhs), 0);
      case wasm::kExprI32LtS:
        return __ Int32LessThan(lhs, rhs);
      case wasm::kExprI32LeS:
        return __ Int32LessThanOrEqual(lhs, rhs);
      case wasm::kExprI32LtU:
        return __ Uint32LessThan(lhs, rhs);
      case wasm::kExprI32LeU:
        return __ Uint32LessThanOrEqual(lhs, rhs);
      case wasm::kExprI32GtS:
        return __ Int32LessThan(rhs, lhs);
      case wasm::kExprI32GeS:
        return __ Int32LessThanOrEqual(rhs, lhs);
      case wasm::kExprI32GtU:
        return __ Uint32LessThan(rhs, lhs);
      case wasm::kExprI32GeU:
        return __ Uint32LessThanOrEqual(rhs, lhs);

      case wasm::kExprF32CopySign: {
        V<Word32> lhs_without_sign =
            __ Word32BitwiseAnd(__ BitcastFloat32ToWord32(lhs), 0x7fffffff);
        V<Word32> rhs_sign =
            __ Word32BitwiseAnd(__ BitcastFloat32ToWord32(rhs), 0x80000000);
        return __ BitcastWord32ToFloat32(
            __ Word32BitwiseOr(lhs_without_sign, rhs_sign));
      }
      case wasm::kExprF32Add:
        return __ Float32Add(lhs, rhs);
      case wasm::kExprF32Sub:
        return __ Float32Sub(lhs, rhs);
      case wasm::kExprF32Mul:
        return __ Float32Mul(lhs, rhs);
      case wasm::kExprF32Div:
        return __ Float32Div(lhs, rhs);
      case wasm::kExprF32Eq:
        return __ Float32Equal(lhs, rhs);
      case wasm::kExprF32Ne:
        return __ Word32Equal(__ Float32Equal(lhs, rhs), 0);
      case wasm::kExprF32Lt:
        return __ Float32LessThan(lhs, rhs);
      case wasm::kExprF32Le:
        return __ Float32LessThanOrEqual(lhs, rhs);
      case wasm::kExprF32Gt:
        return __ Float32LessThan(rhs, lhs);
      case wasm::kExprF32Ge:
        return __ Float32LessThanOrEqual(rhs, lhs);
      case wasm::kExprF32Min:
        return __ Float32Min(rhs, lhs);
      case wasm::kExprF32Max:
        return __ Float32Max(rhs, lhs);
      case wasm::kExprF64Add:
        return __ Float64Add(lhs, rhs);
      case wasm::kExprF64Sub:
        return __ Float64Sub(lhs, rhs);
      case wasm::kExprF64Mul:
        return __ Float64Mul(lhs, rhs);
      case wasm::kExprF64Div:
        return __ Float64Div(lhs, rhs);
      case wasm::kExprF64Eq:
        return __ Float64Equal(lhs, rhs);
      case wasm::kExprF64Ne:
        return __ Word32Equal(__ Float64Equal(lhs, rhs), 0);
      case wasm::kExprF64Lt:
        return __ Float64LessThan(lhs, rhs);
      case wasm::kExprF64Le:
        return __ Float64LessThanOrEqual(lhs, rhs);
      case wasm::kExprF64Gt:
        return __ Float64LessThan(rhs, lhs);
      case wasm::kExprF64Ge:
        return __ Float64LessThanOrEqual(rhs, lhs);
      case wasm::kExprF64Min:
        return __ Float64Min(lhs, rhs);
      case wasm::kExprF64Max:
        return __ Float64Max(lhs, rhs);
      case wasm::kExprRefEq:
        return __ TaggedEqual(lhs, rhs);

      // We currently don't support operations that:
      // - could trap,
      // - call a builtin,
      // - need the instance,
      // - require later Int64Lowering (which is currently only compatible with
      //   the Wasm pipeline),
      // - are actually asm.js opcodes and not spec'ed in Wasm.
      case wasm::kExprI64Add:
      case wasm::kExprI64Sub:
      case wasm::kExprI64Mul:
      case wasm::kExprI64And:
      case wasm::kExprI64Ior:
      case wasm::kExprI64Xor:
      case wasm::kExprI64Shl:
      case wasm::kExprI64ShrS:
      case wasm::kExprI64ShrU:
      case wasm::kExprI64Ror:
      case wasm::kExprI64Rol:
      case wasm::kExprI64Eq:
      case wasm::kExprI64Ne:
      case wasm::kExprI64LtS:
      case wasm::kExprI64LeS:
      case wasm::kExprI64LtU:
      case wasm::kExprI64LeU:
      case wasm::kExprI64GtS:
      case wasm::kExprI64GeS:
      case wasm::kExprI64GtU:
      case wasm::kExprI64GeU:

      case wasm::kExprF64CopySign:

      case wasm::kExprI32DivS:
      case wasm::kExprI32DivU:
      case wasm::kExprI32RemS:
      case wasm::kExprI32RemU:
      case wasm::kExprI64DivS:
      case wasm::kExprI64DivU:
      case wasm::kExprI64RemS:
      case wasm::kExprI64RemU:
        // Not supported, but we are aware of it. Bailout below.
        break;

      // asm.js:
      case wasm::kExprF64Atan2:
      case wasm::kExprF64Pow:
      case wasm::kExprF64Mod:
      case wasm::kExprI32AsmjsDivS:
      case wasm::kExprI32AsmjsDivU:
      case wasm::kExprI32AsmjsRemS:
      case wasm::kExprI32AsmjsRemU:
      case wasm::kExprI32AsmjsStoreMem8:
      case wasm::kExprI32AsmjsStoreMem16:
      case wasm::kExprI32AsmjsStoreMem:
      case wasm::kExprF32AsmjsStoreMem:
      case wasm::kExprF64AsmjsStoreMem:
        // We already bailout for asm.js modules earlier, so we should never
        // see these operations.
        UNREACHABLE();

      default:
        // Fuzzers should alert us when we forget ops, but it's not security
        // critical, so we just bailout below as well.
        DCHECK_WITH_MSG(false, "unhandled binary op");
    }

    Bailout(decoder);
    return V<Any>::Invalid();
  }

  void I32Const(FullDecoder* decoder, Value* result, int32_t value) {
    result->op = __ Word32Constant(value);
  }

  void I64Const(FullDecoder* decoder, Value* result, int64_t value) {
    result->op = __ Word64Constant(value);
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    result->op = __ Float32Constant(value);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    result->op = __ Float64Constant(value);
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    result->op = __ Null(type);
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    result->op = __ WasmRefFunc(trusted_instance_data_, function_index);
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    result->op = __ AssertNotNull(arg.get<Object>(), frame_state_, arg.type,
                                  TrapId::kTrapNullDereference);
  }

  void Drop(FullDecoder* decoder) {}

  void LocalGet(FullDecoder* decoder, Value* result,
                const wasm::IndexImmediate& imm) {
    result->op = locals_[imm.index];
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const wasm::IndexImmediate& imm) {
    locals_[imm.index] = value.op;
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const wasm::IndexImmediate& imm) {
    locals_[imm.index] = result->op = value.op;
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const wasm::GlobalIndexImmediate& imm) {
    SharedFlag shared = decoder->module_->globals[imm.index].shared;
    if (shared == SharedFlag::kYes) {
      return Bailout(decoder);
    }
    result->op = __ GlobalGet(trusted_instance_data_, imm.global);
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const wasm::GlobalIndexImmediate& imm) {
    SharedFlag shared = decoder->module_->globals[imm.index].shared;
    if (shared == SharedFlag::kYes) {
      return Bailout(decoder);
    }
    __ GlobalSet(trusted_instance_data_, value.op, imm.global);
  }

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {
    to->op = from.op;
  }

  void DoReturn(FullDecoder* decoder, uint32_t drop_values) {
    size_t return_count = decoder->sig_->return_count();

    if (return_count == 1) {
      result_.result =
          decoder
              ->stack_value(static_cast<uint32_t>(return_count + drop_values))
              ->op;
    } else if (return_count == 0) {
      // A void function doesn't produce a result. The surrounding JS-to-Wasm
      // wrapper (outside this inlined Wasm body) is responsible for producing
      // the JavaScript `undefined` value.
    } else {
      // We currently don't support wrapper inlining with multi-value returns,
      // so this should never be hit.
      UNREACHABLE();
    }
    result_.success = true;
  }

  void Trap(FullDecoder* decoder, wasm::TrapReason reason) {
    __ WasmTrap(frame_state_, GetTrapIdForTrap(reason));

    // Once we support control-flow in the inlinee, we can't just set the
    // result here, since other blocks / instructions could follow.
    // TODO(dlehmann,353475584): Handle it similar to the
    // `TurboshaftGraphBuildingInterface` for Wasm-in-Wasm inlining. That is,
    // have a `return_block_` and `return_phis_` passed in by the caller
    // and terminator instructions adding phi values and jumping to said block.
    result_.success = true;
  }
  void AssertNullTypecheck(FullDecoder* decoder, const Value& obj,
                           Value* result) {
    __ TrapIfNot(__ IsNull(obj.get<Object>(), obj.type), frame_state_,
                 TrapId::kTrapIllegalCast);
    Forward(decoder, obj, result);
  }
  void AssertNotNullTypecheck(FullDecoder* decoder, const Value& obj,
                              Value* result) {
    __ TrapIf(__ IsNull(obj.get<Object>(), obj.type), frame_state_,
              TrapId::kTrapIllegalCast);
    Forward(decoder, obj, result);
  }

  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const wasm::FieldImmediate& field, bool is_signed,
                 Value* result) {
    result->op = __ StructGet(
        struct_object.get<WasmStructNullable>(), frame_state_,
        field.struct_imm.struct_type, field.struct_imm.index,
        field.field_imm.index, is_signed,
        struct_object.type.is_nullable() ? kWithNullCheck : kWithoutNullCheck,
        {});
  }

  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const wasm::FieldImmediate& field, const Value& field_value) {
    __ StructSet(
        struct_object.get<WasmStructNullable>(), frame_state_, field_value.op,
        field.struct_imm.struct_type, field.struct_imm.index,
        field.field_imm.index,
        struct_object.type.is_nullable() ? kWithNullCheck : kWithoutNullCheck,
        {}, wasm::FieldImmediateToWriteBarrier(field));
  }

  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const wasm::ArrayIndexImmediate& imm, const Value& index,
                bool is_signed, Value* result) {
    auto array_value = array_obj.get<WasmArrayNullable>();
    __ WasmBoundsCheckArray(array_value, index.get<Word32>(), array_obj.type,
                            frame_state_);
    result->op = __ ArrayGet(array_value, index.get<Word32>(), imm.array_type,
                             is_signed, {});
  }

  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const wasm::ArrayIndexImmediate& imm, const Value& index,
                const Value& value) {
    auto array_value = array_obj.get<WasmArrayNullable>();
    __ WasmBoundsCheckArray(array_value, index.get<Word32>(), array_obj.type,
                            frame_state_);
    __ ArraySet(array_value, index.get<Word32>(), value.op,
                imm.array_type->element_type(), {},
                wasm::ArrayIndexImmediateToWriteBarrier(imm));
  }

  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    result->op = __ ArrayLength(
        array_obj.get<WasmArrayNullable>(), frame_state_,
        array_obj.type.is_nullable() ? kWithNullCheck : kWithoutNullCheck);
  }

  V<FixedArray> managed_object_maps() {
    // TODO(dlehmann): Optimize this by baking in the Map into the generated
    // code as a HeapConstant (which we can do in JS, but not in Wasm).
    return LOAD_IMMUTABLE_INSTANCE_FIELD(trusted_instance_data_,
                                         ManagedObjectMaps,
                                         MemoryRepresentation::TaggedPointer());
  }

  // TODO(dlehmann): This and other decoder interface method implementations
  // share a lot of code with the regular Wasm pipeline in
  // `turboshaft-graph-interface.cc`. Think of a way to DRY, e.g., by moving
  // functionality into Turboshaft `Assembler::Wasm*()` methods or introducing a
  // new, common reducer.
  void RefCast(FullDecoder* decoder, const Value& object, Value* result) {
    ValueType target = result->type;
    if (target.is_shared() == SharedFlag::kYes) {
      return Bailout(decoder);
    }
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // TODO(14108): Implement type guards.
      Forward(decoder, object, result);
      return;
    }
    V<Map> rtt = __ RttCanon(managed_object_maps(), target.ref_index());
    compiler::WasmTypeCheckConfig config{
        object.type, target,
        compiler::GetExactness(decoder->module_, target.heap_type())};
    result->op =
        __ WasmTypeCast(object.get<Object>(), rtt, config, frame_state_);
  }

  void RefCastAbstract(FullDecoder* decoder, const Value& object,
                       wasm::HeapType type, Value* result, bool null_succeeds) {
    if (v8_flags.experimental_wasm_assume_ref_cast_succeeds) {
      // TODO(14108): Implement type guards.
      Forward(decoder, object, result);
      return;
    }
    // TODO(jkummerow): {type} is redundant.
    DCHECK_IMPLIES(null_succeeds, result->type.is_nullable());
    DCHECK_EQ(type, result->type.heap_type());
    compiler::WasmTypeCheckConfig config{
        object.type,
        ValueType::RefMaybeNull(
            type, null_succeeds ? wasm::kNullable : wasm::kNonNullable)};
    V<Map> rtt = OpIndex::Invalid();
    result->op =
        __ WasmTypeCast(object.get<Object>(), rtt, config, frame_state_);
  }

  // TODO(dlehmann,353475584): Support linear memory in the inlinee.
  BAILOUT_WASM_OP(LoadMem)
  BAILOUT_WASM_OP(StoreMem)
  BAILOUT_WASM_OP(CurrentMemoryPages)
  BAILOUT_WASM_OP(MemoryGrow)

  // TODO(dlehmann,353475584): Support control-flow in the inlinee.
  BAILOUT_WASM_OP(Block)
  BAILOUT_WASM_OP(Loop)
  BAILOUT_WASM_OP(If)
  BAILOUT_WASM_OP(Else)
  BAILOUT_WASM_OP(BrOrRet)
  BAILOUT_WASM_OP(BrIf)
  BAILOUT_WASM_OP(BrTable)
  BAILOUT_WASM_OP(FallThruTo)
  BAILOUT_WASM_OP(PopControl)
  BAILOUT_WASM_OP(Select)

  // TODO(dlehmann,353475584): Support exceptions in the inlinee.
  BAILOUT_WASM_OP(Try)
  BAILOUT_WASM_OP(Throw)
  BAILOUT_WASM_OP(Rethrow)
  BAILOUT_WASM_OP(CatchException)
  BAILOUT_WASM_OP(Delegate)
  BAILOUT_WASM_OP(CatchAll)
  BAILOUT_WASM_OP(TryTable)
  BAILOUT_WASM_OP(CatchCase)
  BAILOUT_WASM_OP(ThrowRef)

  // TODO(dlehmann,353475584): Support non-leaf functions as the inlinee (i.e.,
  // calls).
  BAILOUT_WASM_OP(CallDirect)
  BAILOUT_WASM_OP(ReturnCall)
  BAILOUT_WASM_OP(CallIndirect)
  BAILOUT_WASM_OP(ReturnCallIndirect)
  BAILOUT_WASM_OP(CallRef)
  BAILOUT_WASM_OP(ReturnCallRef)

  // Continuations / stack switching:
  BAILOUT_WASM_OP(ContNew)
  BAILOUT_WASM_OP(ContBind)
  BAILOUT_WASM_OP(Resume)
  BAILOUT_WASM_OP(BeginEffectHandlers)
  BAILOUT_WASM_OP(EndEffectHandlers)
  BAILOUT_WASM_OP(ResumeHandler)
  BAILOUT_WASM_OP(ResumeThrow)
  BAILOUT_WASM_OP(ResumeThrowRef)
  BAILOUT_WASM_OP(Switch)
  BAILOUT_WASM_OP(Suspend)
  BAILOUT_WASM_OP(EffectHandlerTable)

  // Atomics:
  BAILOUT_WASM_OP(AtomicNotify)
  BAILOUT_WASM_OP(AtomicWait)
  BAILOUT_WASM_OP(AtomicOp)
  BAILOUT_WASM_OP(AtomicFence)
  BAILOUT_WASM_OP(Pause)

  BAILOUT_WASM_OP(StructAtomicRMW)
  BAILOUT_WASM_OP(StructAtomicCompareExchange)
  BAILOUT_WASM_OP(StructWait)
  BAILOUT_WASM_OP(WaitqueueNotify)
  BAILOUT_WASM_OP(WaitqueueNew)

  BAILOUT_WASM_OP(ArrayAtomicRMW)
  BAILOUT_WASM_OP(ArrayAtomicCompareExchange)

  // Bulk memory:
  BAILOUT_WASM_OP(MemoryInit)
  BAILOUT_WASM_OP(MemoryCopy)
  BAILOUT_WASM_OP(MemoryFill)
  BAILOUT_WASM_OP(DataDrop)

  // Tables:
  BAILOUT_WASM_OP(TableGet)
  BAILOUT_WASM_OP(TableSet)
  BAILOUT_WASM_OP(TableInit)
  BAILOUT_WASM_OP(TableCopy)
  BAILOUT_WASM_OP(TableGrow)
  BAILOUT_WASM_OP(TableFill)
  BAILOUT_WASM_OP(TableSize)
  BAILOUT_WASM_OP(ElemDrop)

  // Wasm GC / structs and arrays:
  BAILOUT_WASM_OP(StructNew)
  BAILOUT_WASM_OP(StructNewDefault)

  BAILOUT_WASM_OP(ArrayNew)
  BAILOUT_WASM_OP(ArrayNewDefault)

  BAILOUT_WASM_OP(StructAtomicGet)
  BAILOUT_WASM_OP(StructAtomicSet)

  BAILOUT_WASM_OP(ArrayAtomicGet)
  BAILOUT_WASM_OP(ArrayAtomicSet)

  BAILOUT_WASM_OP(ArrayCopy)
  BAILOUT_WASM_OP(ArrayFill)
  BAILOUT_WASM_OP(ArrayNewFixed)
  BAILOUT_WASM_OP(ArrayNewSegment)
  BAILOUT_WASM_OP(ArrayInitSegment)

  BAILOUT_WASM_OP(RefI31)
  BAILOUT_WASM_OP(I31GetS)
  BAILOUT_WASM_OP(I31GetU)
  BAILOUT_WASM_OP(RefGetDesc)
  BAILOUT_WASM_OP(RefTest)
  BAILOUT_WASM_OP(RefTestAbstract)

  BAILOUT_WASM_OP(RefCastDescEq)

  BAILOUT_WASM_OP(BrOnNull)
  BAILOUT_WASM_OP(BrOnNonNull)
  BAILOUT_WASM_OP(BrOnCast)
  BAILOUT_WASM_OP(BrOnCastDescEq)
  BAILOUT_WASM_OP(BrOnCastAbstract)
  BAILOUT_WASM_OP(BrOnCastFail)
  BAILOUT_WASM_OP(BrOnCastDescEqFail)
  BAILOUT_WASM_OP(BrOnCastFailAbstract)

  // SIMD:
  BAILOUT_WASM_OP(S128Const)
  BAILOUT_WASM_OP(SimdOp)
  BAILOUT_WASM_OP(SimdLaneOp)
  BAILOUT_WASM_OP(Simd8x16ShuffleOp)
  BAILOUT_WASM_OP(LoadTransform)
  BAILOUT_WASM_OP(LoadLane)
  BAILOUT_WASM_OP(StoreLane)

  // Strings:
  BAILOUT_WASM_OP(StringNewWtf8)
  BAILOUT_WASM_OP(StringNewWtf8Array)
  BAILOUT_WASM_OP(StringNewWtf16)
  BAILOUT_WASM_OP(StringNewWtf16Array)
  BAILOUT_WASM_OP(StringConst)
  BAILOUT_WASM_OP(StringMeasureWtf8)
  BAILOUT_WASM_OP(StringMeasureWtf16)
  BAILOUT_WASM_OP(StringEncodeWtf8)
  BAILOUT_WASM_OP(StringEncodeWtf8Array)
  BAILOUT_WASM_OP(StringEncodeWtf16)
  BAILOUT_WASM_OP(StringEncodeWtf16Array)
  BAILOUT_WASM_OP(StringConcat)
  BAILOUT_WASM_OP(StringEq)
  BAILOUT_WASM_OP(StringIsUSVSequence)
  BAILOUT_WASM_OP(StringAsWtf8)
  BAILOUT_WASM_OP(StringViewWtf8Advance)
  BAILOUT_WASM_OP(StringViewWtf8Encode)
  BAILOUT_WASM_OP(StringViewWtf8Slice)
  BAILOUT_WASM_OP(StringAsWtf16)
  BAILOUT_WASM_OP(StringViewWtf16GetCodeUnit)
  BAILOUT_WASM_OP(StringViewWtf16Encode)
  BAILOUT_WASM_OP(StringViewWtf16Slice)
  BAILOUT_WASM_OP(StringAsIter)
  BAILOUT_WASM_OP(StringViewIterNext)
  BAILOUT_WASM_OP(StringViewIterAdvance)
  BAILOUT_WASM_OP(StringViewIterRewind)
  BAILOUT_WASM_OP(StringViewIterSlice)
  BAILOUT_WASM_OP(StringCompare)
  BAILOUT_WASM_OP(StringFromCodePoint)
  BAILOUT_WASM_OP(StringHash)

  // Misc. proposals:
  BAILOUT_WASM_OP(TraceInstruction)
  BAILOUT_WASM_OP(WideOp2)
  BAILOUT_WASM_OP(WideOp4)

 private:
  void set_current_wasm_source_position(FullDecoder* decoder) {
    __ set_current_wasm_source_position(
        SourcePosition(decoder->position(), inlining_id_));
  }

  Assembler& Asm() { return asm_; }
  Assembler& asm_;

  // Since we don't have support for blocks and control-flow yet, this is
  // essentially a stripped-down version of `ssa_env_` from
  // `TurboshaftGraphBuildingInterface`.
  ZoneVector<OpIndex> locals_;

  // The arguments passed to the to-be-inlined function, _excluding_ the
  // Wasm instance.
  base::Vector<const OpIndex> arguments_;
  V<WasmTrustedInstanceData> trusted_instance_data_;

  V<turboshaft::FrameState> frame_state_;
  int inlining_id_;

  // Populated only after decoding finished successfully, i.e., didn't bail out.
  WasmBodyInliningResult result_ = WasmBodyInliningResult::Failed();
};

#undef BAILOUT_WASM_OP

template <class Next>
WasmBodyInliningResult WasmInJSInliningReducer<Next>::TryInlineWasmBody(
    const WasmInlinedFunctionData& inlined_data,
    base::Vector<const OpIndex> arguments,
    compiler::LazyDeoptOnThrow lazy_deopt_on_throw) {
  wasm::NativeModule* native_module = inlined_data.native_module;
  uint32_t func_idx = inlined_data.function_index;
  V<turboshaft::FrameState> frame_state = inlined_data.js_caller_frame_state;
  int inlining_id = inlined_data.inlining_id;
  const wasm::WasmModule* module = native_module->module();
  const wasm::WasmFunction& func = module->functions[func_idx];

  TRACE("Considering Wasm function ["
        << func_idx << "] "
        << JSInliner::WasmFunctionNameForTrace(native_module, func_idx)
        << " of module " << module << " for inlining");

  // TODO(353475584): Support 32-bit platforms by using `Int64LoweringReducer`
  // in the JS pipeline.
  if (!Is64()) {
    TRACE("- not inlining: 32-bit platforms are not supported");
    return WasmBodyInliningResult::Failed();
  }

  if (wasm::is_asmjs_module(module)) {
    TRACE("- not inlining: asm.js-in-JS inlining is not supported");
    return WasmBodyInliningResult::Failed();
  }

  if (func_idx < module->num_imported_functions) {
    TRACE("- not inlining: call to an imported function");
    return WasmBodyInliningResult::Failed();
  }
  DCHECK_LT(func_idx - module->num_imported_functions,
            module->num_declared_functions);

  // TODO(42204563): Support shared-everything proposal (at some point, or
  // possibly never).
  SharedFlag is_shared = module->type(func.sig_index).is_shared;
  if (is_shared == SharedFlag::kYes) {
    TRACE("- not inlining: shared everything is not supported");
    return WasmBodyInliningResult::Failed();
  }

  const bool has_current_catch_block = __ current_catch_block();
  if (lazy_deopt_on_throw == LazyDeoptOnThrow::kYes) {
    // Maglev either emits a catch block or uses lazy deopt on throw, but never
    // both.
    DCHECK(!has_current_catch_block);
    // TODO(mliedtke,dlehmann): Support lazy deopts in Wasm in order to allow
    // inlining calls that have LazyDeoptOnThrow::kYes.
    TRACE("- not inlining: call marked with LazyDeoptOnThrow::kYes");
    return WasmBodyInliningResult::Failed();
  }

  if (has_current_catch_block) {
    // TODO(dlehmann): Currently, we never inline into try-blocks due to Wasm
    // traps ignoring catch handlers in the inlined JS frame.
    // Relax this in the future (but this would be an extension over the old
    // Turbofan inlining, so it's not in the MVP.)
    TRACE("- not inlining: a current catch block is set");
    return WasmBodyInliningResult::Failed();
  }

  base::Vector<const uint8_t> module_bytes = native_module->wire_bytes();
  const uint8_t* start = module_bytes.begin() + func.code.offset();
  const uint8_t* end = module_bytes.begin() + func.code.end_offset();

  wasm::FunctionBody func_body{func.sig, func.code.offset(), start, end,
                               is_shared};

  auto env = wasm::CompilationEnv::ForModule(native_module);
  wasm::WasmDetectedFeatures detected{};

  // Before executing compilation, make sure that the function was validated.
  if (V8_UNLIKELY(!env.module->function_was_validated(func_idx))) {
    CHECK(v8_flags.wasm_lazy_validation);
    if (ValidateFunctionBody(Asm().phase_zone(), env.enabled_features,
                             env.module, &detected, func_body)
            .failed()) {
      TRACE("- not inlining: validation failed");
      return WasmBodyInliningResult::Failed();
    }
    env.module->set_function_validated(func_idx);
  }

  // JS-to-Wasm wrapper inlining doesn't support multi-value at the moment,
  // so we should never reach here with more than 1 return value.
  DCHECK_LE(func.sig->return_count(), 1);
  base::Vector<const OpIndex> arguments_without_instance =
      arguments.SubVectorFrom(1);
  V<WasmTrustedInstanceData> trusted_instance_data =
      arguments[wasm::kWasmInstanceDataParameterIndex];

  Block* inlinee_body_and_rest = __ NewBlock();
  __ Goto(inlinee_body_and_rest);

  // First pass: Decode Wasm body to see if we could inline or would bail out.
  // Emit into an unreachable block. We are not interested in the operations at
  // this point, only in the decoder status afterwards.
  Block* unreachable = __ NewBlock();
  __ Bind(unreachable);

  using Interface = WasmInJsInliningInterface<assembler_t>;
  using Decoder =
      wasm::WasmFullDecoder<typename Interface::ValidationTag, Interface>;
  {
    Decoder can_inline_decoder(Asm().phase_zone(), env.module,
                               env.enabled_features, &detected, func_body,
                               Asm(), arguments_without_instance,
                               trusted_instance_data, frame_state, inlining_id);
    can_inline_decoder.Decode();

    // The function was already validated, so decoding can only fail if we
    // bailed out due to an unsupported instruction.
    if (!can_inline_decoder.ok()) {
      TRACE("- not inlining: " << can_inline_decoder.error().message());
      __ Bind(inlinee_body_and_rest);
      return WasmBodyInliningResult::Failed();
    }
    DCHECK(can_inline_decoder.interface().result().success);
  }

  // Second pass: Actually emit the inlinee instructions now.
  __ Bind(inlinee_body_and_rest);
  Decoder emitting_decoder(Asm().phase_zone(), env.module, env.enabled_features,
                           &detected, func_body, Asm(),
                           arguments_without_instance, trusted_instance_data,
                           frame_state, inlining_id);
  emitting_decoder.Decode();
  DCHECK(emitting_decoder.ok());
  DCHECK(emitting_decoder.interface().result().success);
  TRACE("- inlining Wasm function");
  return emitting_decoder.interface().result();
}

template <class Next>
V<turboshaft::FrameState>
WasmInJSInliningReducer<Next>::CreateWasmInlinedIntoJSFrameState(
    V<Context> js_context, V<turboshaft::FrameState> outer_frame_state,
    SharedFunctionInfoRef shared_function_info) {
  // Wasm functions don't have JavaScript parameters or locals.
  // The only important part of the `FrameState` is the `SharedFunctionInfo`
  // and `FrameStateType`.
  constexpr uint16_t kParameterCount = 0;
  constexpr uint16_t kMaxArguments = 0;
  constexpr int kLocalCount = 0;
  const FrameStateType frame_type = FrameStateType::kWasmInlinedIntoJS;
  IndirectHandle<BytecodeArray> bytecode_array_handle = {};
  Zone* zone = __ data() -> compilation_zone();

  const FrameStateFunctionInfo* frame_state_function_info =
      zone->template New<compiler::FrameStateFunctionInfo>(
          frame_type, kParameterCount, kMaxArguments, kLocalCount,
          shared_function_info.object(), bytecode_array_handle);
  const FrameStateInfo* frame_state_info = zone->template New<FrameStateInfo>(
      BytecodeOffset::None(), OutputFrameStateCombine::Ignore(),
      frame_state_function_info);

  // We are adding several dummy inputs to the `FrameState` here to satisfy
  // invariants in, e.g. the `InstructionSelector`, even though we never
  // actually deopt to an inlined Wasm function.
  // This `FrameState` is just required for accurate stack traces.
  FrameStateData::Builder builder;
  builder.AddParentFrameState(outer_frame_state);
  // Closure.
  builder.AddInput(MachineType::AnyTagged(),
                   __ template LoadRoot<RootIndex::kUndefinedValue>());
  builder.AddInput(MachineType::AnyTagged(), js_context);
  // Dummy param, probably for accumulator in Ignition.
  builder.AddUnusedRegister();

  constexpr bool kInlined = true;
  return __ FrameState(builder.Inputs(), kInlined,
                       builder.AllocateFrameStateData(*frame_state_info, zone));
}

template <class Next>
V<turboshaft::FrameState>
WasmInJSInliningReducer<Next>::CreateJSWasmCallBuiltinContinuationFrameState(
    V<Context> js_context, V<turboshaft::FrameState> outer_frame_state,
    const wasm::CanonicalSig* signature) {
  constexpr uint16_t kParameterCount = 0;
  constexpr int kLocalCount = 0;
  const FrameStateType frame_type =
      FrameStateType::kJSToWasmBuiltinContinuation;
  Handle<SharedFunctionInfo> shared = Handle<SharedFunctionInfo>();
  Zone* zone = __ data() -> compilation_zone();

  const FrameStateFunctionInfo* frame_state_function_info =
      zone->template New<compiler::JSToWasmFrameStateFunctionInfo>(
          frame_type, kParameterCount, kLocalCount, shared, signature);
  const FrameStateInfo* frame_state_info = zone->template New<FrameStateInfo>(
      Builtins::GetContinuationBytecodeOffset(
          Builtin::kJSToWasmLazyDeoptContinuation),
      OutputFrameStateCombine::Ignore(), frame_state_function_info);

  FrameStateData::Builder builder;
  builder.AddParentFrameState(outer_frame_state);
  // Closure
  builder.AddInput(MachineType::AnyTagged(),
                   __ template LoadRoot<RootIndex::kUndefinedValue>());
  builder.AddInput(MachineType::AnyTagged(), js_context);

  constexpr bool kInlined = true;
  return __ FrameState(builder.Inputs(), kInlined,
                       builder.AllocateFrameStateData(*frame_state_info, zone));
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_REDUCER_INL_H_
