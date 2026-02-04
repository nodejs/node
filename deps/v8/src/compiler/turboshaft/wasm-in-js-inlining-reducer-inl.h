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
#include "src/heap/factory-inl.h"
#include "src/objects/instance-type-inl.h"
#include "src/wasm/compilation-environment-inl.h"
#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/turboshaft-graph-interface-inl.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-opcodes-inl.h"
#include "src/wasm/wasm-subtyping.h"
#include "src/wasm/wrappers-inl.h"

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
  const bool has_wasm_in_js_inlining_reducer = true;

  V<Any> REDUCE(Call)(V<CallTarget> callee,
                      OptionalV<turboshaft::FrameState> frame_state,
                      base::Vector<const OpIndex> arguments,
                      const TSCallDescriptor* descriptor, OpEffects effects) {
    LABEL_BLOCK(non_inlined_call) {
      return Next::ReduceCall(callee, frame_state, arguments, descriptor,
                              effects);
    }

    if (!descriptor->js_wasm_call_parameters) {
      // Regular call, nothing to do with Wasm or inlining. Proceed untouched...
      goto non_inlined_call;
    }

    wasm::NativeModule* native_module =
        descriptor->js_wasm_call_parameters->native_module();
    uint32_t func_idx = descriptor->js_wasm_call_parameters->function_index();

    if (v8_flags.turbolev_inline_js_wasm_wrappers && frame_state.has_value()) {
      // TODO(353475584): Wrapper inlining in Turboshaft is only implemented for
      // the Turbolev frontend right now.
      CHECK(v8_flags.turbolev);
      V<JSFunction> js_closure = V<JSFunction>::Cast(callee);
      V<Context> js_context = V<Context>::Cast(arguments[arguments.size() - 1]);

      // Wrapper inlining can return an `V<Any>::Invalid()`, e.g., when the Wasm
      // function signature unconditionally causes a type error to be thrown at
      // runtime (see `IsJSCompatibleSignature` and `BuildJSToWasmWrapperImpl`).
      // We conservatively still emit the non-inlined call in those cases,
      // even though that may be dead code / superfluous.
      // TODO(dlehmann,paolosev@microsoft.com): Change to a stronger return
      // type for `TryInlineJSWasmCallWrapperAndBody` (something like
      // `WasmBodyInliningResult`) and enforce the invariant that an invalid
      // wrapper inlining result always means we do NOT need the code for the
      // non-inlined call (i.e., wrapper inlining never "fails" at this point.)
      V<Any> result = TryInlineJSWasmCallWrapperAndBody(
          native_module, func_idx, arguments, js_closure, js_context,
          descriptor->js_wasm_call_parameters->receiver_is_first_param(),
          frame_state.value(), descriptor->lazy_deopt_on_throw);
      if (result.valid()) {
        return result;
      } else {
        goto non_inlined_call;
      }
    } else if (descriptor->lazy_deopt_on_throw != LazyDeoptOnThrow::kYes) {
      // TODO(mliedtke,dlehmann): Support lazy deopts in Wasm in order to allow
      // inlining calls that have LazyDeoptOnThrow::kYes.

      // TODO(dlehmann): Investigate if we need to prevent inlining into
      // try-blocks (due to wasm traps ignoring catch handlers in the inlined JS
      // frame).

      // We shouldn't have attached `JSWasmCallParameters` at this call in the
      // Turbofan frontend, unless we have TS Wasm-in-JS inlining enabled.
      CHECK(v8_flags.turboshaft_wasm_in_js_inlining);

      WasmBodyInliningResult inlining_result =
          TryInlineWasmCall(native_module, func_idx, arguments);

      switch (inlining_result.type) {
        case WasmBodyInliningResult::Type::kSuccessWithValue:
          return inlining_result.value.value();
        case WasmBodyInliningResult::Type::kSuccessVoid:
          // Inlining succeeded for a void function. The original call had no
          // outputs, so the result is an invalid value (but unlike in the next
          // case, the original call is reduced away).
          return V<Any>::Invalid();
        case WasmBodyInliningResult::Type::kFailed:
          // Inlining failed. Simply generate the unmodified Wasm call.
          goto non_inlined_call;
      }
    }

    // Inlining not supported for this particular call (e.g., because of lazy
    // deopts or else, see above).
    goto non_inlined_call;
  }

  WasmBodyInliningResult TryInlineWasmCall(
      wasm::NativeModule* native_module, uint32_t func_idx,
      base::Vector<const OpIndex> arguments);

 private:
  V<Any> TryInlineJSWasmCallWrapperAndBody(
      wasm::NativeModule* native_module, uint32_t func_idx,
      base::Vector<const OpIndex> arguments, V<JSFunction> js_closure,
      V<Context> js_context, bool receiver_is_first_param,
      V<turboshaft::FrameState> frame_state,
      compiler::LazyDeoptOnThrow lazy_deopt_on_throw);

  V<turboshaft::FrameState> CreateJSWasmCallBuiltinContinuationFrameState(
      V<Context> js_context, V<turboshaft::FrameState> outer_frame_state,
      const wasm::CanonicalSig* signature);
};

using wasm::ArrayIndexImmediate;
using wasm::BranchTableImmediate;
using wasm::CallFunctionImmediate;
using wasm::CallIndirectImmediate;
using wasm::FieldImmediate;
using wasm::GlobalIndexImmediate;
using wasm::IndexImmediate;
using wasm::MemoryAccessImmediate;
using wasm::MemoryCopyImmediate;
using wasm::MemoryIndexImmediate;
using wasm::MemoryInitImmediate;
using wasm::Simd128Immediate;
using wasm::SimdLaneImmediate;
using wasm::StringConstImmediate;
using wasm::StructIndexImmediate;
using wasm::TableCopyImmediate;
using wasm::TableIndexImmediate;
using wasm::TableInitImmediate;
using wasm::TagIndexImmediate;
using wasm::ValueType;
using wasm::WasmOpcode;

template <class Assembler>
class WasmInJsInliningInterface {
 public:
  using ValidationTag = wasm::Decoder::NoValidationTag;
  using FullDecoder =
      wasm::WasmFullDecoder<ValidationTag, WasmInJsInliningInterface>;
  struct Value : public wasm::ValueBase<ValidationTag> {
    V<Any> op = V<Any>::Invalid();
    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };
  // TODO(dlehmann,353475584): Introduce a proper `Control` struct/class, once
  // we want to support control-flow in the inlinee.
  using Control = wasm::ControlBase<Value, ValidationTag>;
  static constexpr bool kUsesPoppedArgs = false;

  WasmInJsInliningInterface(Assembler& assembler,
                            base::Vector<const OpIndex> arguments,
                            V<WasmTrustedInstanceData> trusted_instance_data)
      : asm_(assembler),
        locals_(assembler.phase_zone()),
        arguments_(arguments),
        trusted_instance_data_(trusted_instance_data) {}

  WasmBodyInliningResult Result() { return result_; }

  void OnFirstError(FullDecoder*) {}

  void Bailout(FullDecoder* decoder) {
    decoder->errorf("unsupported operation: %s",
                    decoder->SafeOpcodeNameAt(decoder->pc()));
  }

  void StartFunction(FullDecoder* decoder) {
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
        op = DefaultValue(type);
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
    // TODO(dlehmann,353475584): Copied from Turboshaft graph builder, still
    // need to understand what the inlining ID is / where to get it.
    // __ SetCurrentOrigin(
    //     WasmPositionToOpIndex(decoder->position(), inlining_id_));
  }

  void NopForTestingUnsupportedInLiftoff(FullDecoder* decoder) {
    // This is just for testing bailouts in Liftoff, here it's just a nop.
  }

  void TraceInstruction(FullDecoder* decoder, uint32_t markid) {
    Bailout(decoder);
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

      // TODO(dlehmann): Fix arm64 no-ptr-compression builds, see relevant
      // earlier CL https://crrev.com/c/4311941/1..4.
      case wasm::kExprAnyConvertExtern:
      case wasm::kExprExternConvertAny:

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
        // Not supported, but we are aware of it. Bailout below.
        break;

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
        // Not supported, but we are aware of it. Bailout below.
        break;

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
    // TODO(dlehmann,353475584): Support i64 ops.
    Bailout(decoder);
  }

  void F32Const(FullDecoder* decoder, Value* result, float value) {
    result->op = __ Float32Constant(value);
  }

  void F64Const(FullDecoder* decoder, Value* result, double value) {
    result->op = __ Float64Constant(value);
  }

  void S128Const(FullDecoder* decoder, const Simd128Immediate& imm,
                 Value* result) {
    Bailout(decoder);
  }

  void RefNull(FullDecoder* decoder, ValueType type, Value* result) {
    Bailout(decoder);
  }

  void RefFunc(FullDecoder* decoder, uint32_t function_index, Value* result) {
    Bailout(decoder);
  }

  void RefAsNonNull(FullDecoder* decoder, const Value& arg, Value* result) {
    Bailout(decoder);
  }

  void Drop(FullDecoder* decoder) {}

  void LocalGet(FullDecoder* decoder, Value* result,
                const IndexImmediate& imm) {
    result->op = locals_[imm.index];
  }

  void LocalSet(FullDecoder* decoder, const Value& value,
                const IndexImmediate& imm) {
    locals_[imm.index] = value.op;
  }

  void LocalTee(FullDecoder* decoder, const Value& value, Value* result,
                const IndexImmediate& imm) {
    locals_[imm.index] = result->op = value.op;
  }

  void GlobalGet(FullDecoder* decoder, Value* result,
                 const GlobalIndexImmediate& imm) {
    bool shared = decoder->module_->globals[imm.index].shared;
    if (shared) {
      return Bailout(decoder);
    }
    result->op = __ GlobalGet(trusted_instance_data_, imm.global);
  }

  void GlobalSet(FullDecoder* decoder, const Value& value,
                 const GlobalIndexImmediate& imm) {
    bool shared = decoder->module_->globals[imm.index].shared;
    if (shared) {
      return Bailout(decoder);
    }
    __ GlobalSet(trusted_instance_data_, value.op, imm.global);
  }

  // TODO(dlehmann,353475584): Support control-flow in the inlinee.

  void Block(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void Loop(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void If(FullDecoder* decoder, const Value& cond, Control* if_block) {
    Bailout(decoder);
  }
  void Else(FullDecoder* decoder, Control* if_block) { Bailout(decoder); }
  void BrOrRet(FullDecoder* decoder, uint32_t depth, uint32_t drop_values = 0) {
    Bailout(decoder);
  }
  void BrIf(FullDecoder* decoder, const Value& cond, uint32_t depth) {
    Bailout(decoder);
  }
  void BrTable(FullDecoder* decoder, const BranchTableImmediate& imm,
               const Value& key) {
    Bailout(decoder);
  }

  void FallThruTo(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void PopControl(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void DoReturn(FullDecoder* decoder, uint32_t drop_values) {
    size_t return_count = decoder->sig_->return_count();

    if (return_count == 1) {
      V<Any> return_value =
          decoder
              ->stack_value(static_cast<uint32_t>(return_count + drop_values))
              ->op;
      result_ = WasmBodyInliningResult::SuccessWithValue(return_value);
    } else if (return_count == 0) {
      // A void function doesn't produce a result. The operation that replaces
      // the call will have no outputs. The surrounding JS-to-Wasm wrapper
      // (outside this inlined Wasm body) is responsible for producing the
      // JavaScript `undefined` value if the caller expects one.
      result_ = WasmBodyInliningResult::SuccessVoid();
    } else {
      // We currently don't support wrapper inlining with multi-value returns,
      // so this should never be hit.
      UNREACHABLE();
    }
  }
  void Select(FullDecoder* decoder, const Value& cond, const Value& fval,
              const Value& tval, Value* result) {
    Bailout(decoder);
  }

  // TODO(dlehmann,353475584): Support exceptions in the inlinee.

  void Try(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void Throw(FullDecoder* decoder, const TagIndexImmediate& imm,
             const Value arg_values[]) {
    Bailout(decoder);
  }
  void Rethrow(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void CatchException(FullDecoder* decoder, const TagIndexImmediate& imm,
                      Control* block, base::Vector<Value> values) {
    Bailout(decoder);
  }
  void Delegate(FullDecoder* decoder, uint32_t depth, Control* block) {
    Bailout(decoder);
  }

  void CatchAll(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void TryTable(FullDecoder* decoder, Control* block) { Bailout(decoder); }
  void CatchCase(FullDecoder* decoder, Control* block,
                 const wasm::CatchCase& catch_case,
                 base::Vector<Value> values) {
    Bailout(decoder);
  }
  void ThrowRef(FullDecoder* decoder, Value* value) { Bailout(decoder); }

  void ContNew(FullDecoder* decoder, const wasm::ContIndexImmediate& imm,
               const Value& func_ref, Value* result) {
    Bailout(decoder);
  }

  void ContBind(FullDecoder* decoder, const wasm::ContIndexImmediate& orig_imm,
                Value input_cont, const Value args[],
                const wasm::ContIndexImmediate& new_imm, Value* result) {
    Bailout(decoder);
  }

  void Resume(FullDecoder* decoder, const wasm::ContIndexImmediate& imm,
              base::Vector<wasm::HandlerCase> handlers, const Value& cont_ref,
              const Value args[], const Value returns[]) {
    Bailout(decoder);
  }

  void ResumeHandler(FullDecoder* decoder,
                     base::Vector<const wasm::HandlerCase> handlers,
                     size_t handler_index, Value* cont_val, Value* tag_params) {
    Bailout(decoder);
  }

  void ResumeThrow(FullDecoder* decoder,
                   const wasm::ContIndexImmediate& cont_imm,
                   const TagIndexImmediate& exc_imm,
                   base::Vector<wasm::HandlerCase> handlers, const Value& cont,
                   const Value args[], const Value returns[]) {
    Bailout(decoder);
  }

  void ResumeThrowRef(FullDecoder* decoder,
                      const wasm::ContIndexImmediate& cont_imm,
                      base::Vector<wasm::HandlerCase> handlers,
                      const Value& cont, const Value& exn,
                      const Value returns[]) {
    Bailout(decoder);
  }

  void Switch(FullDecoder* decoder, const TagIndexImmediate& tag_imm,
              const wasm::ContIndexImmediate& con_imm, const Value& cont_ref,
              const Value args[], Value returns[]) {
    Bailout(decoder);
  }

  void Suspend(FullDecoder* decoder, const TagIndexImmediate& imm,
               const Value args[], const Value returns[]) {
    Bailout(decoder);
  }

  void EffectHandlerTable(FullDecoder* decoder, Control* block) {
    Bailout(decoder);
  }

  // TODO(dlehmann,353475584): Support traps in the inlinee.

  void Trap(FullDecoder* decoder, wasm::TrapReason reason) { Bailout(decoder); }
  void AssertNullTypecheck(FullDecoder* decoder, const Value& obj,
                           Value* result) {
    Bailout(decoder);
  }

  void AssertNotNullTypecheck(FullDecoder* decoder, const Value& obj,
                              Value* result) {
    Bailout(decoder);
  }
  void AtomicNotify(FullDecoder* decoder, const MemoryAccessImmediate& imm,
                    OpIndex index, OpIndex num_waiters_to_wake, Value* result) {
    Bailout(decoder);
  }
  void AtomicWait(FullDecoder* decoder, WasmOpcode opcode,
                  const MemoryAccessImmediate& imm, OpIndex index,
                  OpIndex expected, V<Word64> timeout, Value* result) {
    Bailout(decoder);
  }
  void AtomicOp(FullDecoder* decoder, WasmOpcode opcode, const Value args[],
                const size_t argc, const MemoryAccessImmediate& imm,
                Value* result) {
    Bailout(decoder);
  }
  void AtomicFence(FullDecoder* decoder) { Bailout(decoder); }

  void Pause(FullDecoder* decoder) { Bailout(decoder); }

  void StructAtomicRMW(FullDecoder* decoder, WasmOpcode opcode,
                       const Value& struct_object, const FieldImmediate& field,
                       const Value& field_value, AtomicMemoryOrder order,
                       Value* result) {
    Bailout(decoder);
  }

  void StructAtomicCompareExchange(FullDecoder* decoder, WasmOpcode opcode,
                                   const Value& struct_object,
                                   const FieldImmediate& field,
                                   const Value& expected_value,
                                   const Value& new_value,
                                   AtomicMemoryOrder order, Value* result) {
    Bailout(decoder);
  }

  void ArrayAtomicRMW(FullDecoder* decoder, WasmOpcode opcode,
                      const Value& array_obj, const ArrayIndexImmediate& imm,
                      const Value& index, const Value& value,
                      AtomicMemoryOrder order, Value* result) {
    Bailout(decoder);
  }

  void ArrayAtomicCompareExchange(FullDecoder* decoder, WasmOpcode opcode,
                                  const Value& array_obj,
                                  const ArrayIndexImmediate& imm,
                                  const Value& index,
                                  const Value& expected_value,
                                  const Value& new_value,
                                  AtomicMemoryOrder order, Value* result) {
    Bailout(decoder);
  }

  void MemoryInit(FullDecoder* decoder, const MemoryInitImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {
    Bailout(decoder);
  }
  void MemoryCopy(FullDecoder* decoder, const MemoryCopyImmediate& imm,
                  const Value& dst, const Value& src, const Value& size) {
    Bailout(decoder);
  }
  void MemoryFill(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& dst, const Value& value, const Value& size) {
    Bailout(decoder);
  }
  void DataDrop(FullDecoder* decoder, const IndexImmediate& imm) {
    Bailout(decoder);
  }

  void TableGet(FullDecoder* decoder, const Value& index, Value* result,
                const TableIndexImmediate& imm) {
    Bailout(decoder);
  }
  void TableSet(FullDecoder* decoder, const Value& index, const Value& value,
                const TableIndexImmediate& imm) {
    Bailout(decoder);
  }
  void TableInit(FullDecoder* decoder, const TableInitImmediate& imm,
                 const Value& dst_val, const Value& src_val,
                 const Value& size_val) {
    Bailout(decoder);
  }
  void TableCopy(FullDecoder* decoder, const TableCopyImmediate& imm,
                 const Value& dst_val, const Value& src_val,
                 const Value& size_val) {
    Bailout(decoder);
  }
  void TableGrow(FullDecoder* decoder, const TableIndexImmediate& imm,
                 const Value& value, const Value& delta, Value* result) {
    Bailout(decoder);
  }
  void TableFill(FullDecoder* decoder, const TableIndexImmediate& imm,
                 const Value& start, const Value& value, const Value& count) {
    Bailout(decoder);
  }
  void TableSize(FullDecoder* decoder, const TableIndexImmediate& imm,
                 Value* result) {
    Bailout(decoder);
  }

  void ElemDrop(FullDecoder* decoder, const IndexImmediate& imm) {
    Bailout(decoder);
  }

  void StructNew(FullDecoder* decoder, const StructIndexImmediate& imm,
                 const Value& descriptor, const Value args[], Value* result) {
    Bailout(decoder);
  }
  void StructNewDefault(FullDecoder* decoder, const StructIndexImmediate& imm,
                        const Value& descriptor, Value* result) {
    Bailout(decoder);
  }
  void StructGet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, bool is_signed, Value* result) {
    Bailout(decoder);
  }
  void StructSet(FullDecoder* decoder, const Value& struct_object,
                 const FieldImmediate& field, const Value& field_value) {
    Bailout(decoder);
  }
  void StructAtomicGet(FullDecoder* decoder, const Value& struct_obj,
                       const FieldImmediate& field, bool is_signed,
                       AtomicMemoryOrder memory_order, Value* result) {
    Bailout(decoder);
  }
  void StructAtomicSet(FullDecoder* decoder, const Value& struct_object,
                       const FieldImmediate& field, const Value& field_value,
                       AtomicMemoryOrder memory_order) {
    Bailout(decoder);
  }
  void ArrayNew(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                const Value& length, const Value& initial_value,
                Value* result) {
    Bailout(decoder);
  }
  void ArrayNewDefault(FullDecoder* decoder, const ArrayIndexImmediate& imm,
                       const Value& length, Value* result) {
    // TODO(dlehmann): This will pull in/cause a lot of code duplication wrt.
    // the Wasm pipeline / `TurboshaftGraphBuildingInterface`.
    // How to reduce duplication best? Common superclass? But both will have
    // different Assemblers (reducer stacks), so I will have to templatize that.
    Bailout(decoder);
  }
  void ArrayGet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                bool is_signed, Value* result) {
    Bailout(decoder);
  }
  void ArrayAtomicGet(FullDecoder* decoder, const Value& array_obj,
                      const ArrayIndexImmediate& imm, const Value& index,
                      bool is_signed, AtomicMemoryOrder memory_order,
                      Value* result) {
    Bailout(decoder);
  }
  void ArraySet(FullDecoder* decoder, const Value& array_obj,
                const ArrayIndexImmediate& imm, const Value& index,
                const Value& value) {
    Bailout(decoder);
  }
  void ArrayAtomicSet(FullDecoder* decoder, const Value& array_obj,
                      const ArrayIndexImmediate& imm, const Value& index_val,
                      const Value& value_val, AtomicMemoryOrder order) {
    Bailout(decoder);
  }
  void ArrayLen(FullDecoder* decoder, const Value& array_obj, Value* result) {
    Bailout(decoder);
  }
  void ArrayCopy(FullDecoder* decoder, const Value& dst, const Value& dst_index,
                 const Value& src, const Value& src_index,
                 const ArrayIndexImmediate& src_imm, const Value& length) {
    Bailout(decoder);
  }
  void ArrayFill(FullDecoder* decoder, ArrayIndexImmediate& imm,
                 const Value& array, const Value& index, const Value& value,
                 const Value& length) {
    Bailout(decoder);
  }
  void ArrayNewFixed(FullDecoder* decoder, const ArrayIndexImmediate& array_imm,
                     const IndexImmediate& length_imm, const Value elements[],
                     Value* result) {
    Bailout(decoder);
  }
  void ArrayNewSegment(FullDecoder* decoder,
                       const ArrayIndexImmediate& array_imm,
                       const IndexImmediate& segment_imm, const Value& offset,
                       const Value& length, Value* result) {
    Bailout(decoder);
  }
  void ArrayInitSegment(FullDecoder* decoder,
                        const ArrayIndexImmediate& array_imm,
                        const IndexImmediate& segment_imm, const Value& array,
                        const Value& array_index, const Value& segment_offset,
                        const Value& length) {
    Bailout(decoder);
  }

  void RefI31(FullDecoder* decoder, const Value& input, Value* result) {
    Bailout(decoder);
  }
  void I31GetS(FullDecoder* decoder, const Value& input, Value* result) {
    Bailout(decoder);
  }

  void I31GetU(FullDecoder* decoder, const Value& input, Value* result) {
    Bailout(decoder);
  }

  void RefGetDesc(FullDecoder* decoder, wasm::ModuleTypeIndex struct_index,
                  const Value& ref, Value* desc) {
    Bailout(decoder);
  }

  void RefTest(FullDecoder* decoder, wasm::HeapType target_type,
               const Value& object, Value* result, bool null_succeeds) {
    Bailout(decoder);
  }
  void RefTestAbstract(FullDecoder* decoder, const Value& object,
                       wasm::HeapType type, Value* result, bool null_succeeds) {
    Bailout(decoder);
  }
  void RefCast(FullDecoder* decoder, const Value& object, Value* result) {
    Bailout(decoder);
  }
  void RefCastDesc(FullDecoder* decoder, const Value& object,
                   const Value& descriptor, Value* result) {
    Bailout(decoder);
  }
  void RefCastAbstract(FullDecoder* decoder, const Value& object,
                       wasm::HeapType type, Value* result, bool null_succeeds) {
    Bailout(decoder);
  }
  void LoadMem(FullDecoder* decoder, wasm::LoadType type,
               const MemoryAccessImmediate& imm, const Value& index,
               Value* result) {
    Bailout(decoder);
  }
  void LoadTransform(FullDecoder* decoder, wasm::LoadType type,
                     wasm::LoadTransformationKind transform,
                     const MemoryAccessImmediate& imm, const Value& index,
                     Value* result) {
    Bailout(decoder);
  }
  void LoadLane(FullDecoder* decoder, wasm::LoadType type, const Value& value,
                const Value& index, const MemoryAccessImmediate& imm,
                const uint8_t laneidx, Value* result) {
    Bailout(decoder);
  }

  void StoreMem(FullDecoder* decoder, wasm::StoreType type,
                const MemoryAccessImmediate& imm, const Value& index,
                const Value& value) {
    Bailout(decoder);
  }

  void StoreLane(FullDecoder* decoder, wasm::StoreType type,
                 const MemoryAccessImmediate& imm, const Value& index,
                 const Value& value, const uint8_t laneidx) {
    Bailout(decoder);
  }

  void CurrentMemoryPages(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                          Value* result) {
    Bailout(decoder);
  }

  void MemoryGrow(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                  const Value& value, Value* result) {
    Bailout(decoder);
  }

  // TODO(dlehmann,353475584): Support non-leaf functions as the inlinee (i.e.,
  // calls).

  void CallDirect(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[], Value returns[]) {
    Bailout(decoder);
  }
  void ReturnCall(FullDecoder* decoder, const CallFunctionImmediate& imm,
                  const Value args[]) {
    Bailout(decoder);
  }
  void CallIndirect(FullDecoder* decoder, const Value& index,
                    const CallIndirectImmediate& imm, const Value args[],
                    Value returns[]) {
    Bailout(decoder);
  }
  void ReturnCallIndirect(FullDecoder* decoder, const Value& index,
                          const CallIndirectImmediate& imm,
                          const Value args[]) {
    Bailout(decoder);
  }
  void CallRef(FullDecoder* decoder, const Value& func_ref,
               const wasm::FunctionSig* sig, const Value args[],
               Value returns[]) {
    Bailout(decoder);
  }

  void ReturnCallRef(FullDecoder* decoder, const Value& func_ref,
                     const wasm::FunctionSig* sig, const Value args[]) {
    Bailout(decoder);
  }

  void BrOnNull(FullDecoder* decoder, const Value& ref_object, uint32_t depth,
                bool pass_null_along_branch, Value* result_on_fallthrough) {
    Bailout(decoder);
  }

  void BrOnNonNull(FullDecoder* decoder, const Value& ref_object, Value* result,
                   uint32_t depth, bool /* drop_null_on_fallthrough */) {
    Bailout(decoder);
  }

  void BrOnCast(FullDecoder* decoder, wasm::HeapType target_type,
                const Value& object, Value* value_on_branch, uint32_t br_depth,
                bool null_succeeds) {
    Bailout(decoder);
  }
  void BrOnCastDesc(FullDecoder* decoder, wasm::HeapType target_type,
                    const Value& object, const Value& descriptor,
                    Value* value_on_branch, uint32_t br_depth,
                    bool null_succeeds) {
    Bailout(decoder);
  }
  void BrOnCastAbstract(FullDecoder* decoder, const Value& object,
                        wasm::HeapType type, Value* value_on_branch,
                        uint32_t br_depth, bool null_succeeds) {
    Bailout(decoder);
  }
  void BrOnCastFail(FullDecoder* decoder, wasm::HeapType target_type,
                    const Value& object, Value* value_on_fallthrough,
                    uint32_t br_depth, bool null_succeeds) {
    Bailout(decoder);
  }
  void BrOnCastDescFail(FullDecoder* decoder, wasm::HeapType target_type,
                        const Value& object, const Value& descriptor,
                        Value* value_on_fallthrough, uint32_t br_depth,
                        bool null_succeeds) {
    Bailout(decoder);
  }
  void BrOnCastFailAbstract(FullDecoder* decoder, const Value& object,
                            wasm::HeapType type, Value* value_on_fallthrough,
                            uint32_t br_depth, bool null_succeeds) {
    Bailout(decoder);
  }

  // SIMD:

  void SimdOp(FullDecoder* decoder, WasmOpcode opcode, const Value* args,
              Value* result) {
    Bailout(decoder);
  }
  void SimdLaneOp(FullDecoder* decoder, WasmOpcode opcode,
                  const SimdLaneImmediate& imm,
                  base::Vector<const Value> inputs, Value* result) {
    Bailout(decoder);
  }
  void Simd8x16ShuffleOp(FullDecoder* decoder, const Simd128Immediate& imm,
                         const Value& input0, const Value& input1,
                         Value* result) {
    Bailout(decoder);
  }

  // String stuff:

  void StringNewWtf8(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                     const unibrow::Utf8Variant variant, const Value& offset,
                     const Value& size, Value* result) {
    Bailout(decoder);
  }
  void StringNewWtf8Array(FullDecoder* decoder,
                          const unibrow::Utf8Variant variant,
                          const Value& array, const Value& start,
                          const Value& end, Value* result) {
    Bailout(decoder);
  }
  void StringNewWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                      const Value& offset, const Value& size, Value* result) {
    Bailout(decoder);
  }
  void StringNewWtf16Array(FullDecoder* decoder, const Value& array,
                           const Value& start, const Value& end,
                           Value* result) {
    Bailout(decoder);
  }
  void StringConst(FullDecoder* decoder, const StringConstImmediate& imm,
                   Value* result) {
    Bailout(decoder);
  }
  void StringMeasureWtf8(FullDecoder* decoder,
                         const unibrow::Utf8Variant variant, const Value& str,
                         Value* result) {
    Bailout(decoder);
  }
  void StringMeasureWtf16(FullDecoder* decoder, const Value& str,
                          Value* result) {
    Bailout(decoder);
  }
  void StringEncodeWtf8(FullDecoder* decoder,
                        const MemoryIndexImmediate& memory,
                        const unibrow::Utf8Variant variant, const Value& str,
                        const Value& offset, Value* result) {
    Bailout(decoder);
  }
  void StringEncodeWtf8Array(FullDecoder* decoder,
                             const unibrow::Utf8Variant variant,
                             const Value& str, const Value& array,
                             const Value& start, Value* result) {
    Bailout(decoder);
  }
  void StringEncodeWtf16(FullDecoder* decoder, const MemoryIndexImmediate& imm,
                         const Value& str, const Value& offset, Value* result) {
    Bailout(decoder);
  }
  void StringEncodeWtf16Array(FullDecoder* decoder, const Value& str,
                              const Value& array, const Value& start,
                              Value* result) {
    Bailout(decoder);
  }
  void StringConcat(FullDecoder* decoder, const Value& head, const Value& tail,
                    Value* result) {
    Bailout(decoder);
  }
  void StringEq(FullDecoder* decoder, const Value& a, const Value& b,
                Value* result) {
    Bailout(decoder);
  }
  void StringIsUSVSequence(FullDecoder* decoder, const Value& str,
                           Value* result) {
    Bailout(decoder);
  }
  void StringAsWtf8(FullDecoder* decoder, const Value& str, Value* result) {
    Bailout(decoder);
  }
  void StringViewWtf8Advance(FullDecoder* decoder, const Value& view,
                             const Value& pos, const Value& bytes,
                             Value* result) {
    Bailout(decoder);
  }
  void StringViewWtf8Encode(FullDecoder* decoder,
                            const MemoryIndexImmediate& memory,
                            const unibrow::Utf8Variant variant,
                            const Value& view, const Value& addr,
                            const Value& pos, const Value& bytes,
                            Value* next_pos, Value* bytes_written) {
    Bailout(decoder);
  }
  void StringViewWtf8Slice(FullDecoder* decoder, const Value& view,
                           const Value& start, const Value& end,
                           Value* result) {
    Bailout(decoder);
  }
  void StringAsWtf16(FullDecoder* decoder, const Value& str, Value* result) {
    Bailout(decoder);
  }
  void StringViewWtf16GetCodeUnit(FullDecoder* decoder, const Value& view,
                                  const Value& pos, Value* result) {
    Bailout(decoder);
  }
  void StringViewWtf16Encode(FullDecoder* decoder,
                             const MemoryIndexImmediate& imm, const Value& view,
                             const Value& offset, const Value& pos,
                             const Value& codeunits, Value* result) {
    Bailout(decoder);
  }
  void StringViewWtf16Slice(FullDecoder* decoder, const Value& view,
                            const Value& start, const Value& end,
                            Value* result) {
    Bailout(decoder);
  }
  void StringAsIter(FullDecoder* decoder, const Value& str, Value* result) {
    Bailout(decoder);
  }

  void StringViewIterNext(FullDecoder* decoder, const Value& view,
                          Value* result) {
    Bailout(decoder);
  }
  void StringViewIterAdvance(FullDecoder* decoder, const Value& view,
                             const Value& codepoints, Value* result) {
    Bailout(decoder);
  }
  void StringViewIterRewind(FullDecoder* decoder, const Value& view,
                            const Value& codepoints, Value* result) {
    Bailout(decoder);
  }
  void StringViewIterSlice(FullDecoder* decoder, const Value& view,
                           const Value& codepoints, Value* result) {
    Bailout(decoder);
  }
  void StringCompare(FullDecoder* decoder, const Value& lhs, const Value& rhs,
                     Value* result) {
    Bailout(decoder);
  }
  void StringFromCodePoint(FullDecoder* decoder, const Value& code_point,
                           Value* result) {
    Bailout(decoder);
  }
  void StringHash(FullDecoder* decoder, const Value& string, Value* result) {
    Bailout(decoder);
  }

  void Forward(FullDecoder* decoder, const Value& from, Value* to) {
    Bailout(decoder);
  }

 private:
  // TODO(dlehmann): copied from `TurboshaftGraphBuildingInterface`, DRY.
  V<Any> DefaultValue(ValueType type) {
    switch (type.kind()) {
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kI32:
        return __ Word32Constant(int32_t{0});
      case wasm::kI64:
        return __ Word64Constant(int64_t{0});
      case wasm::kF16:
      case wasm::kF32:
        return __ Float32Constant(0.0f);
      case wasm::kF64:
        return __ Float64Constant(0.0);
      case wasm::kRefNull:
        return __ Null(type);
      case wasm::kS128: {
        uint8_t value[kSimd128Size] = {};
        return __ Simd128Constant(value);
      }
      case wasm::kVoid:
      case wasm::kRef:
      case wasm::kBottom:
      case wasm::kTop:
        UNREACHABLE();
    }
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

  // Populated only after decoding finished successfully, i.e., didn't bail out.
  WasmBodyInliningResult result_ = WasmBodyInliningResult::Failed();
};

template <class Next>
V<Any> WasmInJSInliningReducer<Next>::TryInlineJSWasmCallWrapperAndBody(
    wasm::NativeModule* native_module, uint32_t func_idx,
    base::Vector<const OpIndex> arguments, V<JSFunction> js_closure,
    V<Context> js_context, bool receiver_is_first_param,
    V<turboshaft::FrameState> outer_frame_state,
    compiler::LazyDeoptOnThrow lazy_deopt_on_throw) {
  const wasm::WasmModule* module = native_module->module();
  DCHECK_LT(func_idx, module->functions.size());
  const wasm::WasmFunction& func = module->functions[func_idx];
  wasm::CanonicalTypeIndex sig_id = module->canonical_sig_id(func.sig_index);
  const wasm::CanonicalSig* sig =
      wasm::GetTypeCanonicalizer()->LookupFunctionSignature(sig_id);

  V<turboshaft::FrameState> frame_state =
      CreateJSWasmCallBuiltinContinuationFrameState(js_context,
                                                    outer_frame_state, sig);
  using GraphBuilder = wasm::WasmWrapperTSGraphBuilder<assembler_t>;
  std::optional<typename GraphBuilder::InlinedFunctionData>
      inlined_function_data;
  if (v8_flags.turboshaft_wasm_in_js_inlining) {
    inlined_function_data = {native_module, func_idx};
  }

  TRACE("Inlining JS-to-Wasm wrapper for Wasm function ["
        << func_idx << "] "
        << JSInliner::WasmFunctionNameForTrace(native_module, func_idx)
        << " of module " << module);

  constexpr bool kInliningIntoJs = true;
  GraphBuilder builder(Asm().phase_zone(), Asm(), sig, kInliningIntoJs,
                       inlined_function_data);
  return builder.BuildJSToWasmWrapperImpl(receiver_is_first_param, js_closure,
                                          js_context, arguments, frame_state,
                                          lazy_deopt_on_throw);
}

template <class Next>
WasmBodyInliningResult WasmInJSInliningReducer<Next>::TryInlineWasmCall(
    wasm::NativeModule* native_module, uint32_t func_idx,
    base::Vector<const OpIndex> arguments) {
  const wasm::WasmModule* module = native_module->module();
  const wasm::WasmFunction& func = module->functions[func_idx];

  TRACE("Considering wasm function ["
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
  bool is_shared = module->type(func.sig_index).is_shared;
  if (is_shared) {
    TRACE("- not inlining: shared everything is not supported");
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
  Decoder can_inline_decoder(Asm().phase_zone(), env.module,
                             env.enabled_features, &detected, func_body, Asm(),
                             arguments_without_instance, trusted_instance_data);
  can_inline_decoder.Decode();

  // The function was already validated, so decoding can only fail if we bailed
  // out due to an unsupported instruction.
  DCHECK_EQ(can_inline_decoder.ok(),
            can_inline_decoder.interface().Result().IsSuccess());
  if (!can_inline_decoder.ok()) {
    TRACE("- not inlining: " << can_inline_decoder.error().message());
    __ Bind(inlinee_body_and_rest);
    return WasmBodyInliningResult::Failed();
  }

  // Second pass: Actually emit the inlinee instructions now.
  __ Bind(inlinee_body_and_rest);
  Decoder emitting_decoder(Asm().phase_zone(), env.module, env.enabled_features,
                           &detected, func_body, Asm(),
                           arguments_without_instance, trusted_instance_data);
  emitting_decoder.Decode();
  DCHECK(emitting_decoder.ok());
  DCHECK(emitting_decoder.interface().Result().IsSuccess());
  TRACE("- inlining");
  return emitting_decoder.interface().Result();
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
  builder.AddInput(
      MachineType::AnyTagged(),
      __ HeapConstant(
          __ data()->isolate()->factory()->undefined_value()));  // Closure.
  builder.AddInput(MachineType::AnyTagged(), js_context);

  constexpr bool kInlined = true;
  return __ FrameState(builder.Inputs(), kInlined,
                       builder.AllocateFrameStateData(*frame_state_info, zone));
}

#include "src/compiler/turboshaft/undef-assembler-macros.inc"

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_IN_JS_INLINING_REDUCER_INL_H_
