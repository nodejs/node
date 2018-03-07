// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include <memory>

#include "src/assembler-inl.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/code-generator.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/graph.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/js-graph.h"
#include "src/compiler/js-operator.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/simd-scalar-lowering.h"
#include "src/compiler/zone-stats.h"
#include "src/factory.h"
#include "src/isolate-inl.h"
#include "src/log-inl.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-text.h"

namespace v8 {
namespace internal {
namespace compiler {

// TODO(titzer): pull WASM_64 up to a common header.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif

#define FATAL_UNSUPPORTED_OPCODE(opcode)                              \
  V8_Fatal(__FILE__, __LINE__, "Unsupported opcode #%d:%s", (opcode), \
           wasm::WasmOpcodes::OpcodeName(opcode));

namespace {

constexpr uint32_t kBytesPerExceptionValuesArrayElement = 2;

void MergeControlToEnd(JSGraph* jsgraph, Node* node) {
  Graph* g = jsgraph->graph();
  if (g->end()) {
    NodeProperties::MergeControlToEnd(g, jsgraph->common(), node);
  } else {
    g->SetEnd(g->NewNode(jsgraph->common()->End(1), node));
  }
}

bool ContainsSimd(wasm::FunctionSig* sig) {
  for (wasm::ValueType t : sig->all()) {
    if (t == wasm::kWasmS128) return true;
  }
  return false;
}

}  // namespace

WasmGraphBuilder::WasmGraphBuilder(
    ModuleEnv* env, Zone* zone, JSGraph* jsgraph, Handle<Code> centry_stub,
    wasm::FunctionSig* sig,
    compiler::SourcePositionTable* source_position_table,
    RuntimeExceptionSupport exception_support)
    : zone_(zone),
      jsgraph_(jsgraph),
      centry_stub_node_(jsgraph_->HeapConstant(centry_stub)),
      env_(env),
      function_tables_(zone),
      cur_buffer_(def_buffer_),
      cur_bufsize_(kDefaultBufferSize),
      has_simd_(ContainsSimd(sig)),
      untrusted_code_mitigations_(FLAG_untrusted_code_mitigations),
      runtime_exception_support_(exception_support),
      sig_(sig),
      source_position_table_(source_position_table) {
  DCHECK_IMPLIES(use_trap_handler(), trap_handler::IsTrapHandlerEnabled());
  DCHECK_NOT_NULL(jsgraph_);
}

Node* WasmGraphBuilder::Error() { return jsgraph()->Dead(); }

Node* WasmGraphBuilder::Start(unsigned params) {
  Node* start = graph()->NewNode(jsgraph()->common()->Start(params));
  graph()->SetStart(start);
  return start;
}

Node* WasmGraphBuilder::Param(unsigned index) {
  return graph()->NewNode(jsgraph()->common()->Parameter(index),
                          graph()->start());
}

Node* WasmGraphBuilder::Loop(Node* entry) {
  return graph()->NewNode(jsgraph()->common()->Loop(1), entry);
}

Node* WasmGraphBuilder::Terminate(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(jsgraph()->common()->Terminate(), effect, control);
  MergeControlToEnd(jsgraph(), terminate);
  return terminate;
}

bool WasmGraphBuilder::IsPhiWithMerge(Node* phi, Node* merge) {
  return phi && IrOpcode::IsPhiOpcode(phi->opcode()) &&
         NodeProperties::GetControlInput(phi) == merge;
}

bool WasmGraphBuilder::ThrowsException(Node* node, Node** if_success,
                                       Node** if_exception) {
  if (node->op()->HasProperty(compiler::Operator::kNoThrow)) {
    return false;
  }

  *if_success = graph()->NewNode(jsgraph()->common()->IfSuccess(), node);
  *if_exception =
      graph()->NewNode(jsgraph()->common()->IfException(), node, node);

  return true;
}

void WasmGraphBuilder::AppendToMerge(Node* merge, Node* from) {
  DCHECK(IrOpcode::IsMergeOpcode(merge->opcode()));
  merge->AppendInput(jsgraph()->zone(), from);
  int new_size = merge->InputCount();
  NodeProperties::ChangeOp(
      merge, jsgraph()->common()->ResizeMergeOrPhi(merge->op(), new_size));
}

void WasmGraphBuilder::AppendToPhi(Node* phi, Node* from) {
  DCHECK(IrOpcode::IsPhiOpcode(phi->opcode()));
  int new_size = phi->InputCount();
  phi->InsertInput(jsgraph()->zone(), phi->InputCount() - 1, from);
  NodeProperties::ChangeOp(
      phi, jsgraph()->common()->ResizeMergeOrPhi(phi->op(), new_size));
}

Node* WasmGraphBuilder::Merge(unsigned count, Node** controls) {
  return graph()->NewNode(jsgraph()->common()->Merge(count), count, controls);
}

Node* WasmGraphBuilder::Phi(wasm::ValueType type, unsigned count, Node** vals,
                            Node* control) {
  DCHECK(IrOpcode::IsMergeOpcode(control->opcode()));
  Node** buf = Realloc(vals, count, count + 1);
  buf[count] = control;
  return graph()->NewNode(jsgraph()->common()->Phi(type, count), count + 1,
                          buf);
}

Node* WasmGraphBuilder::EffectPhi(unsigned count, Node** effects,
                                  Node* control) {
  DCHECK(IrOpcode::IsMergeOpcode(control->opcode()));
  Node** buf = Realloc(effects, count, count + 1);
  buf[count] = control;
  return graph()->NewNode(jsgraph()->common()->EffectPhi(count), count + 1,
                          buf);
}

Node* WasmGraphBuilder::NumberConstant(int32_t value) {
  return jsgraph()->Constant(value);
}

Node* WasmGraphBuilder::Uint32Constant(uint32_t value) {
  return jsgraph()->Uint32Constant(value);
}

Node* WasmGraphBuilder::Int32Constant(int32_t value) {
  return jsgraph()->Int32Constant(value);
}

Node* WasmGraphBuilder::Int64Constant(int64_t value) {
  return jsgraph()->Int64Constant(value);
}

Node* WasmGraphBuilder::IntPtrConstant(intptr_t value) {
  return jsgraph()->IntPtrConstant(value);
}

void WasmGraphBuilder::StackCheck(wasm::WasmCodePosition position,
                                  Node** effect, Node** control) {
  // TODO(mtrofin): "!env_" happens when we generate a wrapper.
  // We should factor wrappers separately from wasm codegen.
  if (FLAG_wasm_no_stack_checks || !env_ || !runtime_exception_support_) {
    return;
  }
  if (effect == nullptr) effect = effect_;
  if (control == nullptr) control = control_;

  Node* limit = graph()->NewNode(
      jsgraph()->machine()->Load(MachineType::Pointer()),
      jsgraph()->ExternalConstant(
          ExternalReference::address_of_stack_limit(jsgraph()->isolate())),
      jsgraph()->IntPtrConstant(0), *effect, *control);
  *effect = limit;
  Node* pointer = graph()->NewNode(jsgraph()->machine()->LoadStackPointer());

  Node* check =
      graph()->NewNode(jsgraph()->machine()->UintLessThan(), limit, pointer);

  Diamond stack_check(graph(), jsgraph()->common(), check, BranchHint::kTrue);
  stack_check.Chain(*control);

  Handle<Code> code = BUILTIN_CODE(jsgraph()->isolate(), WasmStackGuard);
  CallInterfaceDescriptor idesc =
      WasmRuntimeCallDescriptor(jsgraph()->isolate());
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      jsgraph()->isolate(), jsgraph()->zone(), idesc, 0,
      CallDescriptor::kNoFlags, Operator::kNoProperties,
      MachineType::AnyTagged(), 1, Linkage::kNoContext);
  Node* stub_code = jsgraph()->HeapConstant(code);

  Node* call = graph()->NewNode(jsgraph()->common()->Call(desc), stub_code,
                                *effect, stack_check.if_false);

  SetSourcePosition(call, position);

  Node* ephi = graph()->NewNode(jsgraph()->common()->EffectPhi(2), *effect,
                                call, stack_check.merge);

  *control = stack_check.merge;
  *effect = ephi;
}

void WasmGraphBuilder::PatchInStackCheckIfNeeded() {
  if (!needs_stack_check_) return;

  Node* start = graph()->start();
  // Place a stack check which uses a dummy node as control and effect.
  Node* dummy = graph()->NewNode(jsgraph()->common()->Dead());
  Node* control = dummy;
  Node* effect = dummy;
  // The function-prologue stack check is associated with position 0, which
  // is never a position of any instruction in the function.
  StackCheck(0, &effect, &control);

  // In testing, no steck checks were emitted. Nothing to rewire then.
  if (effect == dummy) return;

  // Now patch all control uses of {start} to use {control} and all effect uses
  // to use {effect} instead. Then rewire the dummy node to use start instead.
  NodeProperties::ReplaceUses(start, start, effect, control);
  NodeProperties::ReplaceUses(dummy, nullptr, start, start);
}

Node* WasmGraphBuilder::Binop(wasm::WasmOpcode opcode, Node* left, Node* right,
                              wasm::WasmCodePosition position) {
  const Operator* op;
  MachineOperatorBuilder* m = jsgraph()->machine();
  switch (opcode) {
    case wasm::kExprI32Add:
      op = m->Int32Add();
      break;
    case wasm::kExprI32Sub:
      op = m->Int32Sub();
      break;
    case wasm::kExprI32Mul:
      op = m->Int32Mul();
      break;
    case wasm::kExprI32DivS:
      return BuildI32DivS(left, right, position);
    case wasm::kExprI32DivU:
      return BuildI32DivU(left, right, position);
    case wasm::kExprI32RemS:
      return BuildI32RemS(left, right, position);
    case wasm::kExprI32RemU:
      return BuildI32RemU(left, right, position);
    case wasm::kExprI32And:
      op = m->Word32And();
      break;
    case wasm::kExprI32Ior:
      op = m->Word32Or();
      break;
    case wasm::kExprI32Xor:
      op = m->Word32Xor();
      break;
    case wasm::kExprI32Shl:
      op = m->Word32Shl();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32ShrU:
      op = m->Word32Shr();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32ShrS:
      op = m->Word32Sar();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32Ror:
      op = m->Word32Ror();
      right = MaskShiftCount32(right);
      break;
    case wasm::kExprI32Rol:
      right = MaskShiftCount32(right);
      return BuildI32Rol(left, right);
    case wasm::kExprI32Eq:
      op = m->Word32Equal();
      break;
    case wasm::kExprI32Ne:
      return Invert(Binop(wasm::kExprI32Eq, left, right));
    case wasm::kExprI32LtS:
      op = m->Int32LessThan();
      break;
    case wasm::kExprI32LeS:
      op = m->Int32LessThanOrEqual();
      break;
    case wasm::kExprI32LtU:
      op = m->Uint32LessThan();
      break;
    case wasm::kExprI32LeU:
      op = m->Uint32LessThanOrEqual();
      break;
    case wasm::kExprI32GtS:
      op = m->Int32LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI32GeS:
      op = m->Int32LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI32GtU:
      op = m->Uint32LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI32GeU:
      op = m->Uint32LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI64And:
      op = m->Word64And();
      break;
    case wasm::kExprI64Add:
      op = m->Int64Add();
      break;
    case wasm::kExprI64Sub:
      op = m->Int64Sub();
      break;
    case wasm::kExprI64Mul:
      op = m->Int64Mul();
      break;
    case wasm::kExprI64DivS:
      return BuildI64DivS(left, right, position);
    case wasm::kExprI64DivU:
      return BuildI64DivU(left, right, position);
    case wasm::kExprI64RemS:
      return BuildI64RemS(left, right, position);
    case wasm::kExprI64RemU:
      return BuildI64RemU(left, right, position);
    case wasm::kExprI64Ior:
      op = m->Word64Or();
      break;
    case wasm::kExprI64Xor:
      op = m->Word64Xor();
      break;
    case wasm::kExprI64Shl:
      op = m->Word64Shl();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64ShrU:
      op = m->Word64Shr();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64ShrS:
      op = m->Word64Sar();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64Eq:
      op = m->Word64Equal();
      break;
    case wasm::kExprI64Ne:
      return Invert(Binop(wasm::kExprI64Eq, left, right));
    case wasm::kExprI64LtS:
      op = m->Int64LessThan();
      break;
    case wasm::kExprI64LeS:
      op = m->Int64LessThanOrEqual();
      break;
    case wasm::kExprI64LtU:
      op = m->Uint64LessThan();
      break;
    case wasm::kExprI64LeU:
      op = m->Uint64LessThanOrEqual();
      break;
    case wasm::kExprI64GtS:
      op = m->Int64LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI64GeS:
      op = m->Int64LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI64GtU:
      op = m->Uint64LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprI64GeU:
      op = m->Uint64LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprI64Ror:
      op = m->Word64Ror();
      right = MaskShiftCount64(right);
      break;
    case wasm::kExprI64Rol:
      return BuildI64Rol(left, right);
    case wasm::kExprF32CopySign:
      return BuildF32CopySign(left, right);
    case wasm::kExprF64CopySign:
      return BuildF64CopySign(left, right);
    case wasm::kExprF32Add:
      op = m->Float32Add();
      break;
    case wasm::kExprF32Sub:
      op = m->Float32Sub();
      break;
    case wasm::kExprF32Mul:
      op = m->Float32Mul();
      break;
    case wasm::kExprF32Div:
      op = m->Float32Div();
      break;
    case wasm::kExprF32Eq:
      op = m->Float32Equal();
      break;
    case wasm::kExprF32Ne:
      return Invert(Binop(wasm::kExprF32Eq, left, right));
    case wasm::kExprF32Lt:
      op = m->Float32LessThan();
      break;
    case wasm::kExprF32Ge:
      op = m->Float32LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprF32Gt:
      op = m->Float32LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprF32Le:
      op = m->Float32LessThanOrEqual();
      break;
    case wasm::kExprF64Add:
      op = m->Float64Add();
      break;
    case wasm::kExprF64Sub:
      op = m->Float64Sub();
      break;
    case wasm::kExprF64Mul:
      op = m->Float64Mul();
      break;
    case wasm::kExprF64Div:
      op = m->Float64Div();
      break;
    case wasm::kExprF64Eq:
      op = m->Float64Equal();
      break;
    case wasm::kExprF64Ne:
      return Invert(Binop(wasm::kExprF64Eq, left, right));
    case wasm::kExprF64Lt:
      op = m->Float64LessThan();
      break;
    case wasm::kExprF64Le:
      op = m->Float64LessThanOrEqual();
      break;
    case wasm::kExprF64Gt:
      op = m->Float64LessThan();
      std::swap(left, right);
      break;
    case wasm::kExprF64Ge:
      op = m->Float64LessThanOrEqual();
      std::swap(left, right);
      break;
    case wasm::kExprF32Min:
      op = m->Float32Min();
      break;
    case wasm::kExprF64Min:
      op = m->Float64Min();
      break;
    case wasm::kExprF32Max:
      op = m->Float32Max();
      break;
    case wasm::kExprF64Max:
      op = m->Float64Max();
      break;
    case wasm::kExprF64Pow:
      return BuildF64Pow(left, right);
    case wasm::kExprF64Atan2:
      op = m->Float64Atan2();
      break;
    case wasm::kExprF64Mod:
      return BuildF64Mod(left, right);
    case wasm::kExprI32AsmjsDivS:
      return BuildI32AsmjsDivS(left, right);
    case wasm::kExprI32AsmjsDivU:
      return BuildI32AsmjsDivU(left, right);
    case wasm::kExprI32AsmjsRemS:
      return BuildI32AsmjsRemS(left, right);
    case wasm::kExprI32AsmjsRemU:
      return BuildI32AsmjsRemU(left, right);
    case wasm::kExprI32AsmjsStoreMem8:
      return BuildAsmjsStoreMem(MachineType::Int8(), left, right);
    case wasm::kExprI32AsmjsStoreMem16:
      return BuildAsmjsStoreMem(MachineType::Int16(), left, right);
    case wasm::kExprI32AsmjsStoreMem:
      return BuildAsmjsStoreMem(MachineType::Int32(), left, right);
    case wasm::kExprF32AsmjsStoreMem:
      return BuildAsmjsStoreMem(MachineType::Float32(), left, right);
    case wasm::kExprF64AsmjsStoreMem:
      return BuildAsmjsStoreMem(MachineType::Float64(), left, right);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
  return graph()->NewNode(op, left, right);
}

Node* WasmGraphBuilder::Unop(wasm::WasmOpcode opcode, Node* input,
                             wasm::WasmCodePosition position) {
  const Operator* op;
  MachineOperatorBuilder* m = jsgraph()->machine();
  switch (opcode) {
    case wasm::kExprI32Eqz:
      op = m->Word32Equal();
      return graph()->NewNode(op, input, jsgraph()->Int32Constant(0));
    case wasm::kExprF32Abs:
      op = m->Float32Abs();
      break;
    case wasm::kExprF32Neg: {
      op = m->Float32Neg();
      break;
    }
    case wasm::kExprF32Sqrt:
      op = m->Float32Sqrt();
      break;
    case wasm::kExprF64Abs:
      op = m->Float64Abs();
      break;
    case wasm::kExprF64Neg: {
      op = m->Float64Neg();
      break;
    }
    case wasm::kExprF64Sqrt:
      op = m->Float64Sqrt();
      break;
    case wasm::kExprI32SConvertF64:
      return BuildI32SConvertF64(input, position, NumericImplementation::kTrap);
    case wasm::kExprI32SConvertSatF64:
      return BuildI32SConvertF64(input, position,
                                 NumericImplementation::kSaturate);
    case wasm::kExprI32UConvertF64:
      return BuildI32UConvertF64(input, position, NumericImplementation::kTrap);
    case wasm::kExprI32UConvertSatF64:
      return BuildI32UConvertF64(input, position,
                                 NumericImplementation::kSaturate);
    case wasm::kExprI32AsmjsSConvertF64:
      return BuildI32AsmjsSConvertF64(input);
    case wasm::kExprI32AsmjsUConvertF64:
      return BuildI32AsmjsUConvertF64(input);
    case wasm::kExprF32ConvertF64:
      op = m->TruncateFloat64ToFloat32();
      break;
    case wasm::kExprF64SConvertI32:
      op = m->ChangeInt32ToFloat64();
      break;
    case wasm::kExprF64UConvertI32:
      op = m->ChangeUint32ToFloat64();
      break;
    case wasm::kExprF32SConvertI32:
      op = m->RoundInt32ToFloat32();
      break;
    case wasm::kExprF32UConvertI32:
      op = m->RoundUint32ToFloat32();
      break;
    case wasm::kExprI32SConvertF32:
      return BuildI32SConvertF32(input, position, NumericImplementation::kTrap);
    case wasm::kExprI32SConvertSatF32:
      return BuildI32SConvertF32(input, position,
                                 NumericImplementation::kSaturate);
    case wasm::kExprI32UConvertF32:
      return BuildI32UConvertF32(input, position, NumericImplementation::kTrap);
    case wasm::kExprI32UConvertSatF32:
      return BuildI32UConvertF32(input, position,
                                 NumericImplementation::kSaturate);
    case wasm::kExprI32AsmjsSConvertF32:
      return BuildI32AsmjsSConvertF32(input);
    case wasm::kExprI32AsmjsUConvertF32:
      return BuildI32AsmjsUConvertF32(input);
    case wasm::kExprF64ConvertF32:
      op = m->ChangeFloat32ToFloat64();
      break;
    case wasm::kExprF32ReinterpretI32:
      op = m->BitcastInt32ToFloat32();
      break;
    case wasm::kExprI32ReinterpretF32:
      op = m->BitcastFloat32ToInt32();
      break;
    case wasm::kExprI32Clz:
      op = m->Word32Clz();
      break;
    case wasm::kExprI32Ctz: {
      if (m->Word32Ctz().IsSupported()) {
        op = m->Word32Ctz().op();
        break;
      } else if (m->Word32ReverseBits().IsSupported()) {
        Node* reversed = graph()->NewNode(m->Word32ReverseBits().op(), input);
        Node* result = graph()->NewNode(m->Word32Clz(), reversed);
        return result;
      } else {
        return BuildI32Ctz(input);
      }
    }
    case wasm::kExprI32Popcnt: {
      if (m->Word32Popcnt().IsSupported()) {
        op = m->Word32Popcnt().op();
        break;
      } else {
        return BuildI32Popcnt(input);
      }
    }
    case wasm::kExprF32Floor: {
      if (!m->Float32RoundDown().IsSupported()) return BuildF32Floor(input);
      op = m->Float32RoundDown().op();
      break;
    }
    case wasm::kExprF32Ceil: {
      if (!m->Float32RoundUp().IsSupported()) return BuildF32Ceil(input);
      op = m->Float32RoundUp().op();
      break;
    }
    case wasm::kExprF32Trunc: {
      if (!m->Float32RoundTruncate().IsSupported()) return BuildF32Trunc(input);
      op = m->Float32RoundTruncate().op();
      break;
    }
    case wasm::kExprF32NearestInt: {
      if (!m->Float32RoundTiesEven().IsSupported())
        return BuildF32NearestInt(input);
      op = m->Float32RoundTiesEven().op();
      break;
    }
    case wasm::kExprF64Floor: {
      if (!m->Float64RoundDown().IsSupported()) return BuildF64Floor(input);
      op = m->Float64RoundDown().op();
      break;
    }
    case wasm::kExprF64Ceil: {
      if (!m->Float64RoundUp().IsSupported()) return BuildF64Ceil(input);
      op = m->Float64RoundUp().op();
      break;
    }
    case wasm::kExprF64Trunc: {
      if (!m->Float64RoundTruncate().IsSupported()) return BuildF64Trunc(input);
      op = m->Float64RoundTruncate().op();
      break;
    }
    case wasm::kExprF64NearestInt: {
      if (!m->Float64RoundTiesEven().IsSupported())
        return BuildF64NearestInt(input);
      op = m->Float64RoundTiesEven().op();
      break;
    }
    case wasm::kExprF64Acos: {
      return BuildF64Acos(input);
    }
    case wasm::kExprF64Asin: {
      return BuildF64Asin(input);
    }
    case wasm::kExprF64Atan:
      op = m->Float64Atan();
      break;
    case wasm::kExprF64Cos: {
      op = m->Float64Cos();
      break;
    }
    case wasm::kExprF64Sin: {
      op = m->Float64Sin();
      break;
    }
    case wasm::kExprF64Tan: {
      op = m->Float64Tan();
      break;
    }
    case wasm::kExprF64Exp: {
      op = m->Float64Exp();
      break;
    }
    case wasm::kExprF64Log:
      op = m->Float64Log();
      break;
    case wasm::kExprI32ConvertI64:
      op = m->TruncateInt64ToInt32();
      break;
    case wasm::kExprI64SConvertI32:
      op = m->ChangeInt32ToInt64();
      break;
    case wasm::kExprI64UConvertI32:
      op = m->ChangeUint32ToUint64();
      break;
    case wasm::kExprF64ReinterpretI64:
      op = m->BitcastInt64ToFloat64();
      break;
    case wasm::kExprI64ReinterpretF64:
      op = m->BitcastFloat64ToInt64();
      break;
    case wasm::kExprI64Clz:
      op = m->Word64Clz();
      break;
    case wasm::kExprI64Ctz: {
      OptionalOperator ctz64 = m->Word64Ctz();
      if (ctz64.IsSupported()) {
        op = ctz64.op();
        break;
      } else if (m->Is32() && m->Word32Ctz().IsSupported()) {
        op = ctz64.placeholder();
        break;
      } else if (m->Word64ReverseBits().IsSupported()) {
        Node* reversed = graph()->NewNode(m->Word64ReverseBits().op(), input);
        Node* result = graph()->NewNode(m->Word64Clz(), reversed);
        return result;
      } else {
        return BuildI64Ctz(input);
      }
    }
    case wasm::kExprI64Popcnt: {
      OptionalOperator popcnt64 = m->Word64Popcnt();
      if (popcnt64.IsSupported()) {
        op = popcnt64.op();
      } else if (m->Is32() && m->Word32Popcnt().IsSupported()) {
        op = popcnt64.placeholder();
      } else {
        return BuildI64Popcnt(input);
      }
      break;
    }
    case wasm::kExprI64Eqz:
      op = m->Word64Equal();
      return graph()->NewNode(op, input, jsgraph()->Int64Constant(0));
    case wasm::kExprF32SConvertI64:
      if (m->Is32()) {
        return BuildF32SConvertI64(input);
      }
      op = m->RoundInt64ToFloat32();
      break;
    case wasm::kExprF32UConvertI64:
      if (m->Is32()) {
        return BuildF32UConvertI64(input);
      }
      op = m->RoundUint64ToFloat32();
      break;
    case wasm::kExprF64SConvertI64:
      if (m->Is32()) {
        return BuildF64SConvertI64(input);
      }
      op = m->RoundInt64ToFloat64();
      break;
    case wasm::kExprF64UConvertI64:
      if (m->Is32()) {
        return BuildF64UConvertI64(input);
      }
      op = m->RoundUint64ToFloat64();
      break;
    case wasm::kExprI64SConvertF32:
      return BuildI64SConvertF32(input, position);
    case wasm::kExprI64SConvertF64:
      return BuildI64SConvertF64(input, position);
    case wasm::kExprI64UConvertF32:
      return BuildI64UConvertF32(input, position);
    case wasm::kExprI64UConvertF64:
      return BuildI64UConvertF64(input, position);
    case wasm::kExprI32AsmjsLoadMem8S:
      return BuildAsmjsLoadMem(MachineType::Int8(), input);
    case wasm::kExprI32AsmjsLoadMem8U:
      return BuildAsmjsLoadMem(MachineType::Uint8(), input);
    case wasm::kExprI32AsmjsLoadMem16S:
      return BuildAsmjsLoadMem(MachineType::Int16(), input);
    case wasm::kExprI32AsmjsLoadMem16U:
      return BuildAsmjsLoadMem(MachineType::Uint16(), input);
    case wasm::kExprI32AsmjsLoadMem:
      return BuildAsmjsLoadMem(MachineType::Int32(), input);
    case wasm::kExprF32AsmjsLoadMem:
      return BuildAsmjsLoadMem(MachineType::Float32(), input);
    case wasm::kExprF64AsmjsLoadMem:
      return BuildAsmjsLoadMem(MachineType::Float64(), input);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
  return graph()->NewNode(op, input);
}

Node* WasmGraphBuilder::Float32Constant(float value) {
  return jsgraph()->Float32Constant(value);
}

Node* WasmGraphBuilder::Float64Constant(double value) {
  return jsgraph()->Float64Constant(value);
}

Node* WasmGraphBuilder::HeapConstant(Handle<HeapObject> value) {
  return jsgraph()->HeapConstant(value);
}

namespace {
Node* Branch(JSGraph* jsgraph, Node* cond, Node** true_node, Node** false_node,
             Node* control, BranchHint hint) {
  DCHECK_NOT_NULL(cond);
  DCHECK_NOT_NULL(control);
  Node* branch =
      jsgraph->graph()->NewNode(jsgraph->common()->Branch(hint), cond, control);
  *true_node = jsgraph->graph()->NewNode(jsgraph->common()->IfTrue(), branch);
  *false_node = jsgraph->graph()->NewNode(jsgraph->common()->IfFalse(), branch);
  return branch;
}
}  // namespace

Node* WasmGraphBuilder::BranchNoHint(Node* cond, Node** true_node,
                                     Node** false_node) {
  return Branch(jsgraph(), cond, true_node, false_node, *control_,
                BranchHint::kNone);
}

Node* WasmGraphBuilder::BranchExpectTrue(Node* cond, Node** true_node,
                                         Node** false_node) {
  return Branch(jsgraph(), cond, true_node, false_node, *control_,
                BranchHint::kTrue);
}

Node* WasmGraphBuilder::BranchExpectFalse(Node* cond, Node** true_node,
                                          Node** false_node) {
  return Branch(jsgraph(), cond, true_node, false_node, *control_,
                BranchHint::kFalse);
}

Builtins::Name WasmGraphBuilder::GetBuiltinIdForTrap(wasm::TrapReason reason) {
  if (runtime_exception_support_ == kNoRuntimeExceptionSupport) {
    // We use Builtins::builtin_count as a marker to tell the code generator
    // to generate a call to a testing c-function instead of a runtime
    // function. This code should only be called from a cctest.
    return Builtins::builtin_count;
  }

  switch (reason) {
#define TRAPREASON_TO_MESSAGE(name) \
  case wasm::k##name:               \
    return Builtins::kThrowWasm##name;
    FOREACH_WASM_TRAPREASON(TRAPREASON_TO_MESSAGE)
#undef TRAPREASON_TO_MESSAGE
    default:
      UNREACHABLE();
  }
}

Node* WasmGraphBuilder::TrapIfTrue(wasm::TrapReason reason, Node* cond,
                                   wasm::WasmCodePosition position) {
  Builtins::Name trap_id = GetBuiltinIdForTrap(reason);
  Node* node = graph()->NewNode(jsgraph()->common()->TrapIf(trap_id), cond,
                                Effect(), Control());
  *control_ = node;
  SetSourcePosition(node, position);
  return node;
}

Node* WasmGraphBuilder::TrapIfFalse(wasm::TrapReason reason, Node* cond,
                                    wasm::WasmCodePosition position) {
  Builtins::Name trap_id = GetBuiltinIdForTrap(reason);

  Node* node = graph()->NewNode(jsgraph()->common()->TrapUnless(trap_id), cond,
                                Effect(), Control());
  *control_ = node;
  SetSourcePosition(node, position);
  return node;
}

// Add a check that traps if {node} is equal to {val}.
Node* WasmGraphBuilder::TrapIfEq32(wasm::TrapReason reason, Node* node,
                                   int32_t val,
                                   wasm::WasmCodePosition position) {
  Int32Matcher m(node);
  if (m.HasValue() && !m.Is(val)) return graph()->start();
  if (val == 0) {
    return TrapIfFalse(reason, node, position);
  } else {
    return TrapIfTrue(reason,
                      graph()->NewNode(jsgraph()->machine()->Word32Equal(),
                                       node, jsgraph()->Int32Constant(val)),
                      position);
  }
}

// Add a check that traps if {node} is zero.
Node* WasmGraphBuilder::ZeroCheck32(wasm::TrapReason reason, Node* node,
                                    wasm::WasmCodePosition position) {
  return TrapIfEq32(reason, node, 0, position);
}

// Add a check that traps if {node} is equal to {val}.
Node* WasmGraphBuilder::TrapIfEq64(wasm::TrapReason reason, Node* node,
                                   int64_t val,
                                   wasm::WasmCodePosition position) {
  Int64Matcher m(node);
  if (m.HasValue() && !m.Is(val)) return graph()->start();
  return TrapIfTrue(reason,
                    graph()->NewNode(jsgraph()->machine()->Word64Equal(), node,
                                     jsgraph()->Int64Constant(val)),
                    position);
}

// Add a check that traps if {node} is zero.
Node* WasmGraphBuilder::ZeroCheck64(wasm::TrapReason reason, Node* node,
                                    wasm::WasmCodePosition position) {
  return TrapIfEq64(reason, node, 0, position);
}

Node* WasmGraphBuilder::Switch(unsigned count, Node* key) {
  return graph()->NewNode(jsgraph()->common()->Switch(count), key, *control_);
}

Node* WasmGraphBuilder::IfValue(int32_t value, Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(jsgraph()->common()->IfValue(value), sw);
}

Node* WasmGraphBuilder::IfDefault(Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(jsgraph()->common()->IfDefault(), sw);
}

Node* WasmGraphBuilder::Return(unsigned count, Node** vals) {
  DCHECK_NOT_NULL(*control_);
  DCHECK_NOT_NULL(*effect_);

  static const int kStackAllocatedNodeBufferSize = 8;
  Node* stack_buffer[kStackAllocatedNodeBufferSize];
  std::vector<Node*> heap_buffer;

  Node** buf = stack_buffer;
  if (count + 3 > kStackAllocatedNodeBufferSize) {
    heap_buffer.resize(count + 3);
    buf = heap_buffer.data();
  }

  buf[0] = jsgraph()->Int32Constant(0);
  memcpy(buf + 1, vals, sizeof(void*) * count);
  buf[count + 1] = *effect_;
  buf[count + 2] = *control_;
  Node* ret =
      graph()->NewNode(jsgraph()->common()->Return(count), count + 3, buf);

  MergeControlToEnd(jsgraph(), ret);
  return ret;
}

Node* WasmGraphBuilder::ReturnVoid() { return Return(0, nullptr); }

Node* WasmGraphBuilder::Unreachable(wasm::WasmCodePosition position) {
  TrapIfFalse(wasm::TrapReason::kTrapUnreachable, Int32Constant(0), position);
  ReturnVoid();
  return nullptr;
}

Node* WasmGraphBuilder::MaskShiftCount32(Node* node) {
  static const int32_t kMask32 = 0x1F;
  if (!jsgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int32Matcher match(node);
    if (match.HasValue()) {
      int32_t masked = (match.Value() & kMask32);
      if (match.Value() != masked) node = jsgraph()->Int32Constant(masked);
    } else {
      node = graph()->NewNode(jsgraph()->machine()->Word32And(), node,
                              jsgraph()->Int32Constant(kMask32));
    }
  }
  return node;
}

Node* WasmGraphBuilder::MaskShiftCount64(Node* node) {
  static const int64_t kMask64 = 0x3F;
  if (!jsgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int64Matcher match(node);
    if (match.HasValue()) {
      int64_t masked = (match.Value() & kMask64);
      if (match.Value() != masked) node = jsgraph()->Int64Constant(masked);
    } else {
      node = graph()->NewNode(jsgraph()->machine()->Word64And(), node,
                              jsgraph()->Int64Constant(kMask64));
    }
  }
  return node;
}

static bool ReverseBytesSupported(MachineOperatorBuilder* m,
                                  size_t size_in_bytes) {
  switch (size_in_bytes) {
    case 4:
    case 16:
      return m->Word32ReverseBytes().IsSupported();
    case 8:
      return m->Word64ReverseBytes().IsSupported();
    default:
      break;
  }
  return false;
}

Node* WasmGraphBuilder::BuildChangeEndiannessStore(
    Node* node, MachineRepresentation mem_rep, wasm::ValueType wasmtype) {
  Node* result;
  Node* value = node;
  MachineOperatorBuilder* m = jsgraph()->machine();
  int valueSizeInBytes = 1 << ElementSizeLog2Of(wasmtype);
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (wasmtype) {
    case wasm::kWasmF64:
      value = graph()->NewNode(m->BitcastFloat64ToInt64(), node);
      isFloat = true;
    case wasm::kWasmI64:
      result = jsgraph()->Int64Constant(0);
      break;
    case wasm::kWasmF32:
      value = graph()->NewNode(m->BitcastFloat32ToInt32(), node);
      isFloat = true;
    case wasm::kWasmI32:
      result = jsgraph()->Int32Constant(0);
      break;
    case wasm::kWasmS128:
      DCHECK(ReverseBytesSupported(m, valueSizeInBytes));
      break;
    default:
      UNREACHABLE();
      break;
  }

  if (mem_rep == MachineRepresentation::kWord8) {
    // No need to change endianness for byte size, return original node
    return node;
  }
  if (wasmtype == wasm::kWasmI64 && mem_rep < MachineRepresentation::kWord64) {
    // In case we store lower part of WasmI64 expression, we can truncate
    // upper 32bits
    value = graph()->NewNode(m->TruncateInt64ToInt32(), value);
    valueSizeInBytes = 1 << ElementSizeLog2Of(wasm::kWasmI32);
    valueSizeInBits = 8 * valueSizeInBytes;
    if (mem_rep == MachineRepresentation::kWord16) {
      value =
          graph()->NewNode(m->Word32Shl(), value, jsgraph()->Int32Constant(16));
    }
  } else if (wasmtype == wasm::kWasmI32 &&
             mem_rep == MachineRepresentation::kWord16) {
    value =
        graph()->NewNode(m->Word32Shl(), value, jsgraph()->Int32Constant(16));
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 4:
        result = graph()->NewNode(m->Word32ReverseBytes().op(), value);
        break;
      case 8:
        result = graph()->NewNode(m->Word64ReverseBytes().op(), value);
        break;
      case 16: {
        Node* byte_reversed_lanes[4];
        for (int lane = 0; lane < 4; lane++) {
          byte_reversed_lanes[lane] = graph()->NewNode(
              m->Word32ReverseBytes().op(),
              graph()->NewNode(jsgraph()->machine()->I32x4ExtractLane(lane),
                               value));
        }

        // This is making a copy of the value.
        result =
            graph()->NewNode(jsgraph()->machine()->S128And(), value, value);

        for (int lane = 0; lane < 4; lane++) {
          result =
              graph()->NewNode(jsgraph()->machine()->I32x4ReplaceLane(3 - lane),
                               result, byte_reversed_lanes[lane]);
        }

        break;
      }
      default:
        UNREACHABLE();
        break;
    }
  } else {
    for (i = 0, shiftCount = valueSizeInBits - 8; i < valueSizeInBits / 2;
         i += 8, shiftCount -= 16) {
      Node* shiftLower;
      Node* shiftHigher;
      Node* lowerByte;
      Node* higherByte;

      DCHECK_LT(0, shiftCount);
      DCHECK_EQ(0, (shiftCount + 8) % 16);

      if (valueSizeInBits > 32) {
        shiftLower = graph()->NewNode(m->Word64Shl(), value,
                                      jsgraph()->Int64Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word64Shr(), value,
                                       jsgraph()->Int64Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word64And(), shiftLower,
            jsgraph()->Int64Constant(static_cast<uint64_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word64And(), shiftHigher,
            jsgraph()->Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = graph()->NewNode(m->Word64Or(), result, lowerByte);
        result = graph()->NewNode(m->Word64Or(), result, higherByte);
      } else {
        shiftLower = graph()->NewNode(m->Word32Shl(), value,
                                      jsgraph()->Int32Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word32Shr(), value,
                                       jsgraph()->Int32Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word32And(), shiftLower,
            jsgraph()->Int32Constant(static_cast<uint32_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word32And(), shiftHigher,
            jsgraph()->Int32Constant(static_cast<uint32_t>(0xFF) << i));
        result = graph()->NewNode(m->Word32Or(), result, lowerByte);
        result = graph()->NewNode(m->Word32Or(), result, higherByte);
      }
    }
  }

  if (isFloat) {
    switch (wasmtype) {
      case wasm::kWasmF64:
        result = graph()->NewNode(m->BitcastInt64ToFloat64(), result);
        break;
      case wasm::kWasmF32:
        result = graph()->NewNode(m->BitcastInt32ToFloat32(), result);
        break;
      default:
        UNREACHABLE();
        break;
    }
  }

  return result;
}

Node* WasmGraphBuilder::BuildChangeEndiannessLoad(Node* node,
                                                  MachineType memtype,
                                                  wasm::ValueType wasmtype) {
  Node* result;
  Node* value = node;
  MachineOperatorBuilder* m = jsgraph()->machine();
  int valueSizeInBytes = 1 << ElementSizeLog2Of(memtype.representation());
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (memtype.representation()) {
    case MachineRepresentation::kFloat64:
      value = graph()->NewNode(m->BitcastFloat64ToInt64(), node);
      isFloat = true;
    case MachineRepresentation::kWord64:
      result = jsgraph()->Int64Constant(0);
      break;
    case MachineRepresentation::kFloat32:
      value = graph()->NewNode(m->BitcastFloat32ToInt32(), node);
      isFloat = true;
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord16:
      result = jsgraph()->Int32Constant(0);
      break;
    case MachineRepresentation::kWord8:
      // No need to change endianness for byte size, return original node
      return node;
      break;
    case MachineRepresentation::kSimd128:
      DCHECK(ReverseBytesSupported(m, valueSizeInBytes));
      break;
    default:
      UNREACHABLE();
      break;
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes < 4 ? 4 : valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 2:
        result =
            graph()->NewNode(m->Word32ReverseBytes().op(),
                             graph()->NewNode(m->Word32Shl(), value,
                                              jsgraph()->Int32Constant(16)));
        break;
      case 4:
        result = graph()->NewNode(m->Word32ReverseBytes().op(), value);
        break;
      case 8:
        result = graph()->NewNode(m->Word64ReverseBytes().op(), value);
        break;
      case 16: {
        Node* byte_reversed_lanes[4];
        for (int lane = 0; lane < 4; lane++) {
          byte_reversed_lanes[lane] = graph()->NewNode(
              m->Word32ReverseBytes().op(),
              graph()->NewNode(jsgraph()->machine()->I32x4ExtractLane(lane),
                               value));
        }

        // This is making a copy of the value.
        result =
            graph()->NewNode(jsgraph()->machine()->S128And(), value, value);

        for (int lane = 0; lane < 4; lane++) {
          result =
              graph()->NewNode(jsgraph()->machine()->I32x4ReplaceLane(3 - lane),
                               result, byte_reversed_lanes[lane]);
        }

        break;
      }
      default:
        UNREACHABLE();
    }
  } else {
    for (i = 0, shiftCount = valueSizeInBits - 8; i < valueSizeInBits / 2;
         i += 8, shiftCount -= 16) {
      Node* shiftLower;
      Node* shiftHigher;
      Node* lowerByte;
      Node* higherByte;

      DCHECK_LT(0, shiftCount);
      DCHECK_EQ(0, (shiftCount + 8) % 16);

      if (valueSizeInBits > 32) {
        shiftLower = graph()->NewNode(m->Word64Shl(), value,
                                      jsgraph()->Int64Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word64Shr(), value,
                                       jsgraph()->Int64Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word64And(), shiftLower,
            jsgraph()->Int64Constant(static_cast<uint64_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word64And(), shiftHigher,
            jsgraph()->Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = graph()->NewNode(m->Word64Or(), result, lowerByte);
        result = graph()->NewNode(m->Word64Or(), result, higherByte);
      } else {
        shiftLower = graph()->NewNode(m->Word32Shl(), value,
                                      jsgraph()->Int32Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word32Shr(), value,
                                       jsgraph()->Int32Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word32And(), shiftLower,
            jsgraph()->Int32Constant(static_cast<uint32_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word32And(), shiftHigher,
            jsgraph()->Int32Constant(static_cast<uint32_t>(0xFF) << i));
        result = graph()->NewNode(m->Word32Or(), result, lowerByte);
        result = graph()->NewNode(m->Word32Or(), result, higherByte);
      }
    }
  }

  if (isFloat) {
    switch (memtype.representation()) {
      case MachineRepresentation::kFloat64:
        result = graph()->NewNode(m->BitcastInt64ToFloat64(), result);
        break;
      case MachineRepresentation::kFloat32:
        result = graph()->NewNode(m->BitcastInt32ToFloat32(), result);
        break;
      default:
        UNREACHABLE();
        break;
    }
  }

  // We need to sign extend the value
  if (memtype.IsSigned()) {
    DCHECK(!isFloat);
    if (valueSizeInBits < 32) {
      Node* shiftBitCount;
      // Perform sign extension using following trick
      // result = (x << machine_width - type_width) >> (machine_width -
      // type_width)
      if (wasmtype == wasm::kWasmI64) {
        shiftBitCount = jsgraph()->Int32Constant(64 - valueSizeInBits);
        result = graph()->NewNode(
            m->Word64Sar(),
            graph()->NewNode(m->Word64Shl(),
                             graph()->NewNode(m->ChangeInt32ToInt64(), result),
                             shiftBitCount),
            shiftBitCount);
      } else if (wasmtype == wasm::kWasmI32) {
        shiftBitCount = jsgraph()->Int32Constant(32 - valueSizeInBits);
        result = graph()->NewNode(
            m->Word32Sar(),
            graph()->NewNode(m->Word32Shl(), result, shiftBitCount),
            shiftBitCount);
      }
    }
  }

  return result;
}

Node* WasmGraphBuilder::BuildF32CopySign(Node* left, Node* right) {
  Node* result = Unop(
      wasm::kExprF32ReinterpretI32,
      Binop(wasm::kExprI32Ior,
            Binop(wasm::kExprI32And, Unop(wasm::kExprI32ReinterpretF32, left),
                  jsgraph()->Int32Constant(0x7FFFFFFF)),
            Binop(wasm::kExprI32And, Unop(wasm::kExprI32ReinterpretF32, right),
                  jsgraph()->Int32Constant(0x80000000))));

  return result;
}

Node* WasmGraphBuilder::BuildF64CopySign(Node* left, Node* right) {
#if WASM_64
  Node* result = Unop(
      wasm::kExprF64ReinterpretI64,
      Binop(wasm::kExprI64Ior,
            Binop(wasm::kExprI64And, Unop(wasm::kExprI64ReinterpretF64, left),
                  jsgraph()->Int64Constant(0x7FFFFFFFFFFFFFFF)),
            Binop(wasm::kExprI64And, Unop(wasm::kExprI64ReinterpretF64, right),
                  jsgraph()->Int64Constant(0x8000000000000000))));

  return result;
#else
  MachineOperatorBuilder* m = jsgraph()->machine();

  Node* high_word_left = graph()->NewNode(m->Float64ExtractHighWord32(), left);
  Node* high_word_right =
      graph()->NewNode(m->Float64ExtractHighWord32(), right);

  Node* new_high_word = Binop(wasm::kExprI32Ior,
                              Binop(wasm::kExprI32And, high_word_left,
                                    jsgraph()->Int32Constant(0x7FFFFFFF)),
                              Binop(wasm::kExprI32And, high_word_right,
                                    jsgraph()->Int32Constant(0x80000000)));

  return graph()->NewNode(m->Float64InsertHighWord32(), left, new_high_word);
#endif
}

// Helper classes for float to int conversions.
struct WasmGraphBuilder::IntConvertOps {
  MachineRepresentation word_rep() const {
    return MachineRepresentation::kWord32;
  }
  Node* zero() const { return builder_->Int32Constant(0); }
  virtual Node* min() const = 0;
  virtual Node* max() const = 0;
  virtual ~IntConvertOps() = default;

 protected:
  explicit IntConvertOps(WasmGraphBuilder* builder) : builder_(builder) {}
  WasmGraphBuilder* builder_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(IntConvertOps);
};

struct I32SConvertOps final : public WasmGraphBuilder::IntConvertOps {
  explicit I32SConvertOps(WasmGraphBuilder* builder)
      : WasmGraphBuilder::IntConvertOps(builder) {}
  ~I32SConvertOps() = default;
  Node* min() const {
    return builder_->Int32Constant(std::numeric_limits<int32_t>::min());
  }
  Node* max() const {
    return builder_->Int32Constant(std::numeric_limits<int32_t>::max());
  }
  DISALLOW_IMPLICIT_CONSTRUCTORS(I32SConvertOps);
};

struct I32UConvertOps final : public WasmGraphBuilder::IntConvertOps {
  explicit I32UConvertOps(WasmGraphBuilder* builder)
      : WasmGraphBuilder::IntConvertOps(builder) {}
  ~I32UConvertOps() = default;
  Node* min() const {
    return builder_->Int32Constant(std::numeric_limits<uint32_t>::min());
  }
  Node* max() const {
    return builder_->Int32Constant(std::numeric_limits<uint32_t>::max());
  }
  DISALLOW_IMPLICIT_CONSTRUCTORS(I32UConvertOps);
};

struct WasmGraphBuilder::FloatConvertOps {
  virtual Node* zero() const = 0;
  virtual wasm::WasmOpcode trunc_op() const = 0;
  virtual wasm::WasmOpcode ne_op() const = 0;
  virtual wasm::WasmOpcode lt_op() const = 0;
  virtual ~FloatConvertOps() = default;

 protected:
  explicit FloatConvertOps(WasmGraphBuilder* builder) : builder_(builder) {}
  WasmGraphBuilder* builder_;
  DISALLOW_IMPLICIT_CONSTRUCTORS(FloatConvertOps);
};

struct F32ConvertOps final : public WasmGraphBuilder::FloatConvertOps {
  explicit F32ConvertOps(WasmGraphBuilder* builder)
      : WasmGraphBuilder::FloatConvertOps(builder) {}
  ~F32ConvertOps() = default;
  Node* zero() const { return builder_->Float32Constant(0.0); }
  wasm::WasmOpcode trunc_op() const { return wasm::kExprF32Trunc; }
  wasm::WasmOpcode ne_op() const { return wasm::kExprF32Ne; }
  wasm::WasmOpcode lt_op() const { return wasm::kExprF32Lt; }
  DISALLOW_IMPLICIT_CONSTRUCTORS(F32ConvertOps);
};

struct F64ConvertOps final : public WasmGraphBuilder::FloatConvertOps {
  explicit F64ConvertOps(WasmGraphBuilder* builder)
      : WasmGraphBuilder::FloatConvertOps(builder) {}
  ~F64ConvertOps() = default;
  Node* zero() const { return builder_->Float64Constant(0.0); }
  wasm::WasmOpcode trunc_op() const { return wasm::kExprF64Trunc; }
  wasm::WasmOpcode ne_op() const { return wasm::kExprF64Ne; }
  wasm::WasmOpcode lt_op() const { return wasm::kExprF64Lt; }
  DISALLOW_IMPLICIT_CONSTRUCTORS(F64ConvertOps);
};

Node* WasmGraphBuilder::BuildConvertCheck(Node* test, Node* result, Node* input,
                                          wasm::WasmCodePosition position,
                                          NumericImplementation impl,
                                          const IntConvertOps* int_ops,
                                          const FloatConvertOps* float_ops) {
  switch (impl) {
    case NumericImplementation::kTrap:
      TrapIfTrue(wasm::kTrapFloatUnrepresentable, test, position);
      return result;
    case NumericImplementation::kSaturate: {
      Diamond tl_d(graph(), jsgraph()->common(), test, BranchHint::kFalse);
      tl_d.Chain(*control_);
      Diamond nan_d(graph(), jsgraph()->common(),
                    Binop(float_ops->ne_op(), input, input),  // Checks if NaN.
                    BranchHint::kFalse);
      nan_d.Nest(tl_d, true);
      Diamond sat_d(graph(), jsgraph()->common(),
                    Binop(float_ops->lt_op(), input, float_ops->zero()),
                    BranchHint::kNone);
      sat_d.Nest(nan_d, false);
      Node* sat_val =
          sat_d.Phi(int_ops->word_rep(), int_ops->min(), int_ops->max());
      Node* nan_val = nan_d.Phi(int_ops->word_rep(), int_ops->zero(), sat_val);
      return tl_d.Phi(int_ops->word_rep(), nan_val, result);
    }
  }
  UNREACHABLE();
}

Node* WasmGraphBuilder::BuildI32ConvertOp(
    Node* input, wasm::WasmCodePosition position, NumericImplementation impl,
    const Operator* op, wasm::WasmOpcode check_op, const IntConvertOps* int_ops,
    const FloatConvertOps* float_ops) {
  // Truncation of the input value is needed for the overflow check later.
  Node* trunc = Unop(float_ops->trunc_op(), input);
  Node* result = graph()->NewNode(op, trunc);

  // Convert the result back to f64. If we end up at a different value than the
  // truncated input value, then there has been an overflow and we
  // trap/saturate.
  Node* check = Unop(check_op, result);
  Node* overflow = Binop(float_ops->ne_op(), trunc, check);
  return BuildConvertCheck(overflow, result, input, position, impl, int_ops,
                           float_ops);
}

Node* WasmGraphBuilder::BuildI32SConvertF32(Node* input,
                                            wasm::WasmCodePosition position,
                                            NumericImplementation impl) {
  I32SConvertOps int_ops(this);
  F32ConvertOps float_ops(this);
  return BuildI32ConvertOp(input, position, impl,
                           jsgraph()->machine()->TruncateFloat32ToInt32(),
                           wasm::kExprF32SConvertI32, &int_ops, &float_ops);
}

Node* WasmGraphBuilder::BuildI32SConvertF64(Node* input,
                                            wasm::WasmCodePosition position,
                                            NumericImplementation impl) {
  I32SConvertOps int_ops(this);
  F64ConvertOps float_ops(this);
  return BuildI32ConvertOp(input, position, impl,
                           jsgraph()->machine()->ChangeFloat64ToInt32(),
                           wasm::kExprF64SConvertI32, &int_ops, &float_ops);
}

Node* WasmGraphBuilder::BuildI32UConvertF32(Node* input,
                                            wasm::WasmCodePosition position,
                                            NumericImplementation impl) {
  I32UConvertOps int_ops(this);
  F32ConvertOps float_ops(this);
  return BuildI32ConvertOp(input, position, impl,
                           jsgraph()->machine()->TruncateFloat32ToUint32(),
                           wasm::kExprF32UConvertI32, &int_ops, &float_ops);
}

Node* WasmGraphBuilder::BuildI32UConvertF64(Node* input,
                                            wasm::WasmCodePosition position,
                                            NumericImplementation impl) {
  I32UConvertOps int_ops(this);
  F64ConvertOps float_ops(this);
  return BuildI32ConvertOp(input, position, impl,
                           jsgraph()->machine()->TruncateFloat64ToUint32(),
                           wasm::kExprF64UConvertI32, &int_ops, &float_ops);
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF32(Node* input) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // asm.js must use the wacky JS semantics.
  input = graph()->NewNode(m->ChangeFloat32ToFloat64(), input);
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF64(Node* input) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // asm.js must use the wacky JS semantics.
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF32(Node* input) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // asm.js must use the wacky JS semantics.
  input = graph()->NewNode(m->ChangeFloat32ToFloat64(), input);
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF64(Node* input) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // asm.js must use the wacky JS semantics.
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildBitCountingCall(Node* input, ExternalReference ref,
                                             MachineRepresentation input_type) {
  Node* stack_slot_param =
      graph()->NewNode(jsgraph()->machine()->StackSlot(input_type));

  const Operator* store_op = jsgraph()->machine()->Store(
      StoreRepresentation(input_type, kNoWriteBarrier));
  *effect_ =
      graph()->NewNode(store_op, stack_slot_param, jsgraph()->Int32Constant(0),
                       input, *effect_, *control_);

  MachineSignature::Builder sig_builder(jsgraph()->zone(), 1, 1);
  sig_builder.AddReturn(MachineType::Int32());
  sig_builder.AddParam(MachineType::Pointer());

  Node* function = graph()->NewNode(jsgraph()->common()->ExternalConstant(ref));

  return BuildCCall(sig_builder.Build(), function, stack_slot_param);
}

Node* WasmGraphBuilder::BuildI32Ctz(Node* input) {
  return BuildBitCountingCall(
      input, ExternalReference::wasm_word32_ctz(jsgraph()->isolate()),
      MachineRepresentation::kWord32);
}

Node* WasmGraphBuilder::BuildI64Ctz(Node* input) {
  return Unop(wasm::kExprI64UConvertI32,
              BuildBitCountingCall(input, ExternalReference::wasm_word64_ctz(
                                              jsgraph()->isolate()),
                                   MachineRepresentation::kWord64));
}

Node* WasmGraphBuilder::BuildI32Popcnt(Node* input) {
  return BuildBitCountingCall(
      input, ExternalReference::wasm_word32_popcnt(jsgraph()->isolate()),
      MachineRepresentation::kWord32);
}

Node* WasmGraphBuilder::BuildI64Popcnt(Node* input) {
  return Unop(wasm::kExprI64UConvertI32,
              BuildBitCountingCall(input, ExternalReference::wasm_word64_popcnt(
                                              jsgraph()->isolate()),
                                   MachineRepresentation::kWord64));
}

Node* WasmGraphBuilder::BuildF32Trunc(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref =
      ExternalReference::wasm_f32_trunc(jsgraph()->isolate());

  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32Floor(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref =
      ExternalReference::wasm_f32_floor(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32Ceil(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref =
      ExternalReference::wasm_f32_ceil(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32NearestInt(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref =
      ExternalReference::wasm_f32_nearest_int(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Trunc(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::wasm_f64_trunc(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Floor(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::wasm_f64_floor(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Ceil(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::wasm_f64_ceil(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64NearestInt(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::wasm_f64_nearest_int(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Acos(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::f64_acos_wrapper_function(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Asin(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::f64_asin_wrapper_function(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Pow(Node* left, Node* right) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::wasm_float64_pow(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, left, right);
}

Node* WasmGraphBuilder::BuildF64Mod(Node* left, Node* right) {
  MachineType type = MachineType::Float64();
  ExternalReference ref =
      ExternalReference::f64_mod_wrapper_function(jsgraph()->isolate());
  return BuildCFuncInstruction(ref, type, left, right);
}

Node* WasmGraphBuilder::BuildCFuncInstruction(ExternalReference ref,
                                              MachineType type, Node* input0,
                                              Node* input1) {
  // We do truncation by calling a C function which calculates the result.
  // The input is passed to the C function as a double*'s to avoid double
  // parameters. For this we reserve slots on the stack, store the parameters
  // in those slots, pass pointers to the slot to the C function,
  // and after calling the C function we collect the return value from
  // the stack slot.

  Node* stack_slot_param0 =
      graph()->NewNode(jsgraph()->machine()->StackSlot(type.representation()));

  const Operator* store_op0 = jsgraph()->machine()->Store(
      StoreRepresentation(type.representation(), kNoWriteBarrier));
  *effect_ = graph()->NewNode(store_op0, stack_slot_param0,
                              jsgraph()->Int32Constant(0), input0, *effect_,
                              *control_);

  Node* function = graph()->NewNode(jsgraph()->common()->ExternalConstant(ref));

  if (input1 == nullptr) {
    const int input_count = 1;
    Signature<MachineType>::Builder sig_builder(jsgraph()->zone(), 0,
                                                input_count);
    sig_builder.AddParam(MachineType::Pointer());
    BuildCCall(sig_builder.Build(), function, stack_slot_param0);
  } else {
    Node* stack_slot_param1 = graph()->NewNode(
        jsgraph()->machine()->StackSlot(type.representation()));
    const Operator* store_op1 = jsgraph()->machine()->Store(
        StoreRepresentation(type.representation(), kNoWriteBarrier));
    *effect_ = graph()->NewNode(store_op1, stack_slot_param1,
                                jsgraph()->Int32Constant(0), input1, *effect_,
                                *control_);

    const int input_count = 2;
    Signature<MachineType>::Builder sig_builder(jsgraph()->zone(), 0,
                                                input_count);
    sig_builder.AddParam(MachineType::Pointer());
    sig_builder.AddParam(MachineType::Pointer());
    BuildCCall(sig_builder.Build(), function, stack_slot_param0,
               stack_slot_param1);
  }

  const Operator* load_op = jsgraph()->machine()->Load(type);

  Node* load =
      graph()->NewNode(load_op, stack_slot_param0, jsgraph()->Int32Constant(0),
                       *effect_, *control_);
  *effect_ = load;
  return load;
}

Node* WasmGraphBuilder::BuildF32SConvertI64(Node* input) {
  // TODO(titzer/bradnelson): Check handlng of asm.js case.
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_int64_to_float32(jsgraph()->isolate()),
      MachineRepresentation::kWord64, MachineType::Float32());
}
Node* WasmGraphBuilder::BuildF32UConvertI64(Node* input) {
  // TODO(titzer/bradnelson): Check handlng of asm.js case.
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_uint64_to_float32(jsgraph()->isolate()),
      MachineRepresentation::kWord64, MachineType::Float32());
}
Node* WasmGraphBuilder::BuildF64SConvertI64(Node* input) {
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_int64_to_float64(jsgraph()->isolate()),
      MachineRepresentation::kWord64, MachineType::Float64());
}
Node* WasmGraphBuilder::BuildF64UConvertI64(Node* input) {
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_uint64_to_float64(jsgraph()->isolate()),
      MachineRepresentation::kWord64, MachineType::Float64());
}

Node* WasmGraphBuilder::BuildIntToFloatConversionInstruction(
    Node* input, ExternalReference ref,
    MachineRepresentation parameter_representation,
    const MachineType result_type) {
  Node* stack_slot_param = graph()->NewNode(
      jsgraph()->machine()->StackSlot(parameter_representation));
  Node* stack_slot_result = graph()->NewNode(
      jsgraph()->machine()->StackSlot(result_type.representation()));
  const Operator* store_op = jsgraph()->machine()->Store(
      StoreRepresentation(parameter_representation, kNoWriteBarrier));
  *effect_ =
      graph()->NewNode(store_op, stack_slot_param, jsgraph()->Int32Constant(0),
                       input, *effect_, *control_);
  MachineSignature::Builder sig_builder(jsgraph()->zone(), 0, 2);
  sig_builder.AddParam(MachineType::Pointer());
  sig_builder.AddParam(MachineType::Pointer());
  Node* function = graph()->NewNode(jsgraph()->common()->ExternalConstant(ref));
  BuildCCall(sig_builder.Build(), function, stack_slot_param,
             stack_slot_result);
  const Operator* load_op = jsgraph()->machine()->Load(result_type);
  Node* load =
      graph()->NewNode(load_op, stack_slot_result, jsgraph()->Int32Constant(0),
                       *effect_, *control_);
  *effect_ = load;
  return load;
}

Node* WasmGraphBuilder::BuildI64SConvertF32(Node* input,
                                            wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildFloatToIntConversionInstruction(
        input, ExternalReference::wasm_float32_to_int64(jsgraph()->isolate()),
        MachineRepresentation::kFloat32, MachineType::Int64(), position);
  } else {
    Node* trunc = graph()->NewNode(
        jsgraph()->machine()->TryTruncateFloat32ToInt64(), input);
    Node* result = graph()->NewNode(jsgraph()->common()->Projection(0), trunc,
                                    graph()->start());
    Node* overflow = graph()->NewNode(jsgraph()->common()->Projection(1), trunc,
                                      graph()->start());
    ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
    return result;
  }
}

Node* WasmGraphBuilder::BuildI64UConvertF32(Node* input,
                                            wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildFloatToIntConversionInstruction(
        input, ExternalReference::wasm_float32_to_uint64(jsgraph()->isolate()),
        MachineRepresentation::kFloat32, MachineType::Int64(), position);
  } else {
    Node* trunc = graph()->NewNode(
        jsgraph()->machine()->TryTruncateFloat32ToUint64(), input);
    Node* result = graph()->NewNode(jsgraph()->common()->Projection(0), trunc,
                                    graph()->start());
    Node* overflow = graph()->NewNode(jsgraph()->common()->Projection(1), trunc,
                                      graph()->start());
    ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
    return result;
  }
}

Node* WasmGraphBuilder::BuildI64SConvertF64(Node* input,
                                            wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildFloatToIntConversionInstruction(
        input, ExternalReference::wasm_float64_to_int64(jsgraph()->isolate()),
        MachineRepresentation::kFloat64, MachineType::Int64(), position);
  } else {
    Node* trunc = graph()->NewNode(
        jsgraph()->machine()->TryTruncateFloat64ToInt64(), input);
    Node* result = graph()->NewNode(jsgraph()->common()->Projection(0), trunc,
                                    graph()->start());
    Node* overflow = graph()->NewNode(jsgraph()->common()->Projection(1), trunc,
                                      graph()->start());
    ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
    return result;
  }
}

Node* WasmGraphBuilder::BuildI64UConvertF64(Node* input,
                                            wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildFloatToIntConversionInstruction(
        input, ExternalReference::wasm_float64_to_uint64(jsgraph()->isolate()),
        MachineRepresentation::kFloat64, MachineType::Int64(), position);
  } else {
    Node* trunc = graph()->NewNode(
        jsgraph()->machine()->TryTruncateFloat64ToUint64(), input);
    Node* result = graph()->NewNode(jsgraph()->common()->Projection(0), trunc,
                                    graph()->start());
    Node* overflow = graph()->NewNode(jsgraph()->common()->Projection(1), trunc,
                                      graph()->start());
    ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
    return result;
  }
}

Node* WasmGraphBuilder::BuildFloatToIntConversionInstruction(
    Node* input, ExternalReference ref,
    MachineRepresentation parameter_representation,
    const MachineType result_type, wasm::WasmCodePosition position) {
  Node* stack_slot_param = graph()->NewNode(
      jsgraph()->machine()->StackSlot(parameter_representation));
  Node* stack_slot_result = graph()->NewNode(
      jsgraph()->machine()->StackSlot(result_type.representation()));
  const Operator* store_op = jsgraph()->machine()->Store(
      StoreRepresentation(parameter_representation, kNoWriteBarrier));
  *effect_ =
      graph()->NewNode(store_op, stack_slot_param, jsgraph()->Int32Constant(0),
                       input, *effect_, *control_);
  MachineSignature::Builder sig_builder(jsgraph()->zone(), 1, 2);
  sig_builder.AddReturn(MachineType::Int32());
  sig_builder.AddParam(MachineType::Pointer());
  sig_builder.AddParam(MachineType::Pointer());
  Node* function = graph()->NewNode(jsgraph()->common()->ExternalConstant(ref));
  ZeroCheck32(wasm::kTrapFloatUnrepresentable,
              BuildCCall(sig_builder.Build(), function, stack_slot_param,
                         stack_slot_result),
              position);
  const Operator* load_op = jsgraph()->machine()->Load(result_type);
  Node* load =
      graph()->NewNode(load_op, stack_slot_result, jsgraph()->Int32Constant(0),
                       *effect_, *control_);
  *effect_ = load;
  return load;
}

Node* WasmGraphBuilder::GrowMemory(Node* input) {
  SetNeedsStackCheck();
  Diamond check_input_range(
      graph(), jsgraph()->common(),
      graph()->NewNode(jsgraph()->machine()->Uint32LessThanOrEqual(), input,
                       jsgraph()->Uint32Constant(FLAG_wasm_max_mem_pages)),
      BranchHint::kTrue);

  check_input_range.Chain(*control_);

  Node* parameters[] = {BuildChangeUint32ToSmi(input)};
  Node* old_effect = *effect_;
  *control_ = check_input_range.if_true;
  Node* call = BuildCallToRuntime(Runtime::kWasmGrowMemory, parameters,
                                  arraysize(parameters));

  Node* result = BuildChangeSmiToInt32(call);

  result = check_input_range.Phi(MachineRepresentation::kWord32, result,
                                 jsgraph()->Int32Constant(-1));
  *effect_ = graph()->NewNode(jsgraph()->common()->EffectPhi(2), *effect_,
                              old_effect, check_input_range.merge);
  *control_ = check_input_range.merge;
  return result;
}

uint32_t WasmGraphBuilder::GetExceptionEncodedSize(
    const wasm::WasmException* exception) const {
  const wasm::WasmExceptionSig* sig = exception->sig;
  uint32_t encoded_size = 0;
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    size_t byte_size = size_t(1) << ElementSizeLog2Of(sig->GetParam(i));
    DCHECK_EQ(byte_size % kBytesPerExceptionValuesArrayElement, 0);
    DCHECK_LE(1, byte_size / kBytesPerExceptionValuesArrayElement);
    encoded_size += byte_size / kBytesPerExceptionValuesArrayElement;
  }
  return encoded_size;
}

Node* WasmGraphBuilder::Throw(uint32_t tag,
                              const wasm::WasmException* exception,
                              const Vector<Node*> values) {
  SetNeedsStackCheck();
  uint32_t encoded_size = GetExceptionEncodedSize(exception);
  Node* create_parameters[] = {
      BuildChangeUint32ToSmi(ConvertExceptionTagToRuntimeId(tag)),
      BuildChangeUint32ToSmi(Uint32Constant(encoded_size))};
  BuildCallToRuntime(Runtime::kWasmThrowCreate, create_parameters,
                     arraysize(create_parameters));
  uint32_t index = 0;
  const wasm::WasmExceptionSig* sig = exception->sig;
  MachineOperatorBuilder* m = jsgraph()->machine();
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value = values[i];
    switch (sig->GetParam(i)) {
      case wasm::kWasmF32:
        value = graph()->NewNode(m->BitcastFloat32ToInt32(), value);
      // Intentionally fall to next case.
      case wasm::kWasmI32:
        BuildEncodeException32BitValue(&index, value);
        break;
      case wasm::kWasmF64:
        value = graph()->NewNode(m->BitcastFloat64ToInt64(), value);
      // Intentionally fall to next case.
      case wasm::kWasmI64: {
        Node* upper32 = graph()->NewNode(
            m->TruncateInt64ToInt32(),
            Binop(wasm::kExprI64ShrU, value, Int64Constant(32)));
        BuildEncodeException32BitValue(&index, upper32);
        Node* lower32 = graph()->NewNode(m->TruncateInt64ToInt32(), value);
        BuildEncodeException32BitValue(&index, lower32);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  DCHECK_EQ(encoded_size, index);
  return BuildCallToRuntime(Runtime::kWasmThrow, nullptr, 0);
}

void WasmGraphBuilder::BuildEncodeException32BitValue(uint32_t* index,
                                                      Node* value) {
  MachineOperatorBuilder* machine = jsgraph()->machine();
  Node* upper_parameters[] = {
      BuildChangeUint32ToSmi(Int32Constant(*index)),
      BuildChangeUint32ToSmi(
          graph()->NewNode(machine->Word32Shr(), value, Int32Constant(16))),
  };
  BuildCallToRuntime(Runtime::kWasmExceptionSetElement, upper_parameters,
                     arraysize(upper_parameters));
  ++(*index);
  Node* lower_parameters[] = {
      BuildChangeUint32ToSmi(Int32Constant(*index)),
      BuildChangeUint32ToSmi(graph()->NewNode(machine->Word32And(), value,
                                              Int32Constant(0xFFFFu))),
  };
  BuildCallToRuntime(Runtime::kWasmExceptionSetElement, lower_parameters,
                     arraysize(lower_parameters));
  ++(*index);
}

Node* WasmGraphBuilder::BuildDecodeException32BitValue(Node* const* values,
                                                       uint32_t* index) {
  MachineOperatorBuilder* machine = jsgraph()->machine();
  Node* upper = BuildChangeSmiToInt32(values[*index]);
  (*index)++;
  upper = graph()->NewNode(machine->Word32Shl(), upper, Int32Constant(16));
  Node* lower = BuildChangeSmiToInt32(values[*index]);
  (*index)++;
  Node* value = graph()->NewNode(machine->Word32Or(), upper, lower);
  return value;
}

Node* WasmGraphBuilder::Rethrow() {
  SetNeedsStackCheck();
  Node* result = BuildCallToRuntime(Runtime::kWasmThrow, nullptr, 0);
  return result;
}

Node* WasmGraphBuilder::ConvertExceptionTagToRuntimeId(uint32_t tag) {
  // TODO(kschimpf): Handle exceptions from different modules, when they are
  // linked at runtime.
  return Uint32Constant(tag);
}

Node* WasmGraphBuilder::GetExceptionRuntimeId() {
  SetNeedsStackCheck();
  return BuildChangeSmiToInt32(
      BuildCallToRuntime(Runtime::kWasmGetExceptionRuntimeId, nullptr, 0));
}

Node** WasmGraphBuilder::GetExceptionValues(
    const wasm::WasmException* except_decl) {
  // TODO(kschimpf): We need to move this code to the function-body-decoder.cc
  // in order to build landing-pad (exception) edges in case the runtime
  // call causes an exception.

  // Start by getting the encoded values from the exception.
  uint32_t encoded_size = GetExceptionEncodedSize(except_decl);
  Node** values = Buffer(encoded_size);
  for (uint32_t i = 0; i < encoded_size; ++i) {
    Node* parameters[] = {BuildChangeUint32ToSmi(Uint32Constant(i))};
    values[i] = BuildCallToRuntime(Runtime::kWasmExceptionGetElement,
                                   parameters, arraysize(parameters));
  }

  // Now convert the leading entries to the corresponding parameter values.
  uint32_t index = 0;
  const wasm::WasmExceptionSig* sig = except_decl->sig;
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value = BuildDecodeException32BitValue(values, &index);
    switch (wasm::ValueType type = sig->GetParam(i)) {
      case wasm::kWasmF32: {
        value = Unop(wasm::kExprF32ReinterpretI32, value);
        break;
      }
      case wasm::kWasmI32:
        break;
      case wasm::kWasmF64:
      case wasm::kWasmI64: {
        Node* upper =
            Binop(wasm::kExprI64Shl, Unop(wasm::kExprI64UConvertI32, value),
                  Int64Constant(32));
        Node* lower = Unop(wasm::kExprI64UConvertI32,
                           BuildDecodeException32BitValue(values, &index));
        value = Binop(wasm::kExprI64Ior, upper, lower);
        if (type == wasm::kWasmF64) {
          value = Unop(wasm::kExprF64ReinterpretI64, value);
        }
        break;
      }
      default:
        UNREACHABLE();
    }
    values[i] = value;
  }
  DCHECK_EQ(index, encoded_size);
  return values;
}

Node* WasmGraphBuilder::BuildI32DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  ZeroCheck32(wasm::kTrapDivByZero, right, position);
  Node* before = *control_;
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(-1)),
      &denom_is_m1, &denom_is_not_m1);
  *control_ = denom_is_m1;
  TrapIfEq32(wasm::kTrapDivUnrepresentable, left, kMinInt, position);
  if (*control_ != denom_is_m1) {
    *control_ = graph()->NewNode(jsgraph()->common()->Merge(2), denom_is_not_m1,
                                 *control_);
  } else {
    *control_ = before;
  }
  return graph()->NewNode(m->Int32Div(), left, right, *control_);
}

Node* WasmGraphBuilder::BuildI32RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();

  ZeroCheck32(wasm::kTrapRemByZero, right, position);

  Diamond d(
      graph(), jsgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(-1)),
      BranchHint::kFalse);
  d.Chain(*control_);

  return d.Phi(MachineRepresentation::kWord32, jsgraph()->Int32Constant(0),
               graph()->NewNode(m->Int32Mod(), left, right, d.if_false));
}

Node* WasmGraphBuilder::BuildI32DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  return graph()->NewNode(m->Uint32Div(), left, right,
                          ZeroCheck32(wasm::kTrapDivByZero, right, position));
}

Node* WasmGraphBuilder::BuildI32RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  return graph()->NewNode(m->Uint32Mod(), left, right,
                          ZeroCheck32(wasm::kTrapRemByZero, right, position));
}

Node* WasmGraphBuilder::BuildI32AsmjsDivS(Node* left, Node* right) {
  MachineOperatorBuilder* m = jsgraph()->machine();

  Int32Matcher mr(right);
  if (mr.HasValue()) {
    if (mr.Value() == 0) {
      return jsgraph()->Int32Constant(0);
    } else if (mr.Value() == -1) {
      // The result is the negation of the left input.
      return graph()->NewNode(m->Int32Sub(), jsgraph()->Int32Constant(0), left);
    }
    return graph()->NewNode(m->Int32Div(), left, right, *control_);
  }

  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Int32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return graph()->NewNode(m->Int32Div(), left, right, graph()->start());
  }

  // Check denominator for zero.
  Diamond z(
      graph(), jsgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(0)),
      BranchHint::kFalse);

  // Check numerator for -1. (avoid minint / -1 case).
  Diamond n(
      graph(), jsgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(-1)),
      BranchHint::kFalse);

  Node* div = graph()->NewNode(m->Int32Div(), left, right, z.if_false);
  Node* neg =
      graph()->NewNode(m->Int32Sub(), jsgraph()->Int32Constant(0), left);

  return n.Phi(
      MachineRepresentation::kWord32, neg,
      z.Phi(MachineRepresentation::kWord32, jsgraph()->Int32Constant(0), div));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemS(Node* left, Node* right) {
  CommonOperatorBuilder* c = jsgraph()->common();
  MachineOperatorBuilder* m = jsgraph()->machine();
  Node* const zero = jsgraph()->Int32Constant(0);

  Int32Matcher mr(right);
  if (mr.HasValue()) {
    if (mr.Value() == 0 || mr.Value() == -1) {
      return zero;
    }
    return graph()->NewNode(m->Int32Mod(), left, right, *control_);
  }

  // General case for signed integer modulus, with optimization for (unknown)
  // power of 2 right hand side.
  //
  //   if 0 < right then
  //     msk = right - 1
  //     if right & msk != 0 then
  //       left % right
  //     else
  //       if left < 0 then
  //         -(-left & msk)
  //       else
  //         left & msk
  //   else
  //     if right < -1 then
  //       left % right
  //     else
  //       zero
  //
  // Note: We do not use the Diamond helper class here, because it really hurts
  // readability with nested diamonds.
  Node* const minus_one = jsgraph()->Int32Constant(-1);

  const Operator* const merge_op = c->Merge(2);
  const Operator* const phi_op = c->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = graph()->NewNode(m->Int32LessThan(), zero, right);
  Node* branch0 =
      graph()->NewNode(c->Branch(BranchHint::kTrue), check0, graph()->start());

  Node* if_true0 = graph()->NewNode(c->IfTrue(), branch0);
  Node* true0;
  {
    Node* msk = graph()->NewNode(m->Int32Add(), right, minus_one);

    Node* check1 = graph()->NewNode(m->Word32And(), right, msk);
    Node* branch1 = graph()->NewNode(c->Branch(), check1, if_true0);

    Node* if_true1 = graph()->NewNode(c->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(m->Int32Mod(), left, right, if_true1);

    Node* if_false1 = graph()->NewNode(c->IfFalse(), branch1);
    Node* false1;
    {
      Node* check2 = graph()->NewNode(m->Int32LessThan(), left, zero);
      Node* branch2 =
          graph()->NewNode(c->Branch(BranchHint::kFalse), check2, if_false1);

      Node* if_true2 = graph()->NewNode(c->IfTrue(), branch2);
      Node* true2 = graph()->NewNode(
          m->Int32Sub(), zero,
          graph()->NewNode(m->Word32And(),
                           graph()->NewNode(m->Int32Sub(), zero, left), msk));

      Node* if_false2 = graph()->NewNode(c->IfFalse(), branch2);
      Node* false2 = graph()->NewNode(m->Word32And(), left, msk);

      if_false1 = graph()->NewNode(merge_op, if_true2, if_false2);
      false1 = graph()->NewNode(phi_op, true2, false2, if_false1);
    }

    if_true0 = graph()->NewNode(merge_op, if_true1, if_false1);
    true0 = graph()->NewNode(phi_op, true1, false1, if_true0);
  }

  Node* if_false0 = graph()->NewNode(c->IfFalse(), branch0);
  Node* false0;
  {
    Node* check1 = graph()->NewNode(m->Int32LessThan(), right, minus_one);
    Node* branch1 =
        graph()->NewNode(c->Branch(BranchHint::kTrue), check1, if_false0);

    Node* if_true1 = graph()->NewNode(c->IfTrue(), branch1);
    Node* true1 = graph()->NewNode(m->Int32Mod(), left, right, if_true1);

    Node* if_false1 = graph()->NewNode(c->IfFalse(), branch1);
    Node* false1 = zero;

    if_false0 = graph()->NewNode(merge_op, if_true1, if_false1);
    false0 = graph()->NewNode(phi_op, true1, false1, if_false0);
  }

  Node* merge0 = graph()->NewNode(merge_op, if_true0, if_false0);
  return graph()->NewNode(phi_op, true0, false0, merge0);
}

Node* WasmGraphBuilder::BuildI32AsmjsDivU(Node* left, Node* right) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Uint32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return graph()->NewNode(m->Uint32Div(), left, right, graph()->start());
  }

  // Explicit check for x % 0.
  Diamond z(
      graph(), jsgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(0)),
      BranchHint::kFalse);

  return z.Phi(MachineRepresentation::kWord32, jsgraph()->Int32Constant(0),
               graph()->NewNode(jsgraph()->machine()->Uint32Div(), left, right,
                                z.if_false));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemU(Node* left, Node* right) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // asm.js semantics return 0 on divide or mod by zero.
  // Explicit check for x % 0.
  Diamond z(
      graph(), jsgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(0)),
      BranchHint::kFalse);

  Node* rem = graph()->NewNode(jsgraph()->machine()->Uint32Mod(), left, right,
                               z.if_false);
  return z.Phi(MachineRepresentation::kWord32, jsgraph()->Int32Constant(0),
               rem);
}

Node* WasmGraphBuilder::BuildI64DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildDiv64Call(
        left, right, ExternalReference::wasm_int64_div(jsgraph()->isolate()),
        MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  ZeroCheck64(wasm::kTrapDivByZero, right, position);
  Node* before = *control_;
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(graph()->NewNode(jsgraph()->machine()->Word64Equal(), right,
                                     jsgraph()->Int64Constant(-1)),
                    &denom_is_m1, &denom_is_not_m1);
  *control_ = denom_is_m1;
  TrapIfEq64(wasm::kTrapDivUnrepresentable, left,
             std::numeric_limits<int64_t>::min(), position);
  if (*control_ != denom_is_m1) {
    *control_ = graph()->NewNode(jsgraph()->common()->Merge(2), denom_is_not_m1,
                                 *control_);
  } else {
    *control_ = before;
  }
  return graph()->NewNode(jsgraph()->machine()->Int64Div(), left, right,
                          *control_);
}

Node* WasmGraphBuilder::BuildI64RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildDiv64Call(
        left, right, ExternalReference::wasm_int64_mod(jsgraph()->isolate()),
        MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  ZeroCheck64(wasm::kTrapRemByZero, right, position);
  Diamond d(jsgraph()->graph(), jsgraph()->common(),
            graph()->NewNode(jsgraph()->machine()->Word64Equal(), right,
                             jsgraph()->Int64Constant(-1)));

  d.Chain(*control_);

  Node* rem = graph()->NewNode(jsgraph()->machine()->Int64Mod(), left, right,
                               d.if_false);

  return d.Phi(MachineRepresentation::kWord64, jsgraph()->Int64Constant(0),
               rem);
}

Node* WasmGraphBuilder::BuildI64DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildDiv64Call(
        left, right, ExternalReference::wasm_uint64_div(jsgraph()->isolate()),
        MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  return graph()->NewNode(jsgraph()->machine()->Uint64Div(), left, right,
                          ZeroCheck64(wasm::kTrapDivByZero, right, position));
}
Node* WasmGraphBuilder::BuildI64RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildDiv64Call(
        left, right, ExternalReference::wasm_uint64_mod(jsgraph()->isolate()),
        MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  return graph()->NewNode(jsgraph()->machine()->Uint64Mod(), left, right,
                          ZeroCheck64(wasm::kTrapRemByZero, right, position));
}

Node* WasmGraphBuilder::BuildDiv64Call(Node* left, Node* right,
                                       ExternalReference ref,
                                       MachineType result_type, int trap_zero,
                                       wasm::WasmCodePosition position) {
  Node* stack_slot_dst = graph()->NewNode(
      jsgraph()->machine()->StackSlot(MachineRepresentation::kWord64));
  Node* stack_slot_src = graph()->NewNode(
      jsgraph()->machine()->StackSlot(MachineRepresentation::kWord64));

  const Operator* store_op = jsgraph()->machine()->Store(
      StoreRepresentation(MachineRepresentation::kWord64, kNoWriteBarrier));
  *effect_ =
      graph()->NewNode(store_op, stack_slot_dst, jsgraph()->Int32Constant(0),
                       left, *effect_, *control_);
  *effect_ =
      graph()->NewNode(store_op, stack_slot_src, jsgraph()->Int32Constant(0),
                       right, *effect_, *control_);

  MachineSignature::Builder sig_builder(jsgraph()->zone(), 1, 2);
  sig_builder.AddReturn(MachineType::Int32());
  sig_builder.AddParam(MachineType::Pointer());
  sig_builder.AddParam(MachineType::Pointer());

  Node* function = graph()->NewNode(jsgraph()->common()->ExternalConstant(ref));
  Node* call =
      BuildCCall(sig_builder.Build(), function, stack_slot_dst, stack_slot_src);

  ZeroCheck32(static_cast<wasm::TrapReason>(trap_zero), call, position);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, call, -1, position);
  const Operator* load_op = jsgraph()->machine()->Load(result_type);
  Node* load =
      graph()->NewNode(load_op, stack_slot_dst, jsgraph()->Int32Constant(0),
                       *effect_, *control_);
  *effect_ = load;
  return load;
}

template <typename... Args>
Node* WasmGraphBuilder::BuildCCall(MachineSignature* sig, Node* function,
                                   Args... args) {
  DCHECK_LE(sig->return_count(), 1);
  DCHECK_EQ(sizeof...(args), sig->parameter_count());
  Node* const call_args[] = {function, args..., *effect_, *control_};

  CallDescriptor* desc =
      Linkage::GetSimplifiedCDescriptor(jsgraph()->zone(), sig);

  const Operator* op = jsgraph()->common()->Call(desc);
  Node* call = graph()->NewNode(op, arraysize(call_args), call_args);
  *effect_ = call;
  return call;
}

Node* WasmGraphBuilder::BuildWasmCall(wasm::FunctionSig* sig, Node** args,
                                      Node*** rets,
                                      wasm::WasmCodePosition position) {
  DCHECK_NOT_NULL(wasm_context_);
  SetNeedsStackCheck();
  const size_t params = sig->parameter_count();
  const size_t extra = 3;  // wasm_context, effect, and control.
  const size_t count = 1 + params + extra;

  // Reallocate the buffer to make space for extra inputs.
  args = Realloc(args, 1 + params, count);

  // Make room for the wasm_context parameter at index 1, just after code.
  memmove(&args[2], &args[1], params * sizeof(Node*));
  args[1] = wasm_context_.get();

  // Add effect and control inputs.
  args[params + 2] = *effect_;
  args[params + 3] = *control_;

  CallDescriptor* descriptor = GetWasmCallDescriptor(jsgraph()->zone(), sig);
  const Operator* op = jsgraph()->common()->Call(descriptor);
  Node* call = graph()->NewNode(op, static_cast<int>(count), args);
  SetSourcePosition(call, position);

  *effect_ = call;
  size_t ret_count = sig->return_count();
  if (ret_count == 0) return call;  // No return value.

  *rets = Buffer(ret_count);
  if (ret_count == 1) {
    // Only a single return value.
    (*rets)[0] = call;
  } else {
    // Create projections for all return values.
    for (size_t i = 0; i < ret_count; i++) {
      (*rets)[i] = graph()->NewNode(jsgraph()->common()->Projection(i), call,
                                    graph()->start());
    }
  }
  return call;
}

Node* WasmGraphBuilder::CallDirect(uint32_t index, Node** args, Node*** rets,
                                   wasm::WasmCodePosition position) {
  DCHECK_NULL(args[0]);
  wasm::FunctionSig* sig = env_->module->functions[index].sig;
  if (FLAG_wasm_jit_to_native) {
    // Just encode the function index. This will be patched at instantiation.
    Address code = reinterpret_cast<Address>(index);
    args[0] = jsgraph()->RelocatableIntPtrConstant(
        reinterpret_cast<intptr_t>(code), RelocInfo::WASM_CALL);
  } else {
    // Add code object as constant.
    Handle<Code> code = index < env_->function_code.size()
                            ? env_->function_code[index]
                            : env_->default_function_code;

    DCHECK(!code.is_null());
    args[0] = HeapConstant(code);
  }

  return BuildWasmCall(sig, args, rets, position);
}

Node* WasmGraphBuilder::CallIndirect(uint32_t sig_index, Node** args,
                                     Node*** rets,
                                     wasm::WasmCodePosition position) {
  DCHECK_NOT_NULL(args[0]);
  DCHECK_NOT_NULL(env_);

  // Assume only one table for now.
  uint32_t table_index = 0;
  wasm::FunctionSig* sig = env_->module->signatures[sig_index];

  EnsureFunctionTableNodes();
  MachineOperatorBuilder* machine = jsgraph()->machine();
  Node* key = args[0];

  // Bounds check against the table size.
  Node* size = function_tables_[table_index].size;
  Node* in_bounds = graph()->NewNode(machine->Uint32LessThan(), key, size);
  TrapIfFalse(wasm::kTrapFuncInvalid, in_bounds, position);
  Node* table_address = function_tables_[table_index].table_addr;
  Node* table = graph()->NewNode(
      jsgraph()->machine()->Load(MachineType::AnyTagged()), table_address,
      jsgraph()->IntPtrConstant(0), *effect_, *control_);
  // Load signature from the table and check.
  // The table is a FixedArray; signatures are encoded as SMIs.
  // [sig1, code1, sig2, code2, sig3, code3, ...]
  static_assert(compiler::kFunctionTableEntrySize == 2, "consistency");
  static_assert(compiler::kFunctionTableSignatureOffset == 0, "consistency");
  static_assert(compiler::kFunctionTableCodeOffset == 1, "consistency");
  ElementAccess access = AccessBuilder::ForFixedArrayElement();
  const int fixed_offset = access.header_size - access.tag();
  Node* key_offset = graph()->NewNode(machine->Word32Shl(), key,
                                      Int32Constant(kPointerSizeLog2 + 1));
  Node* load_sig =
      graph()->NewNode(machine->Load(MachineType::AnyTagged()), table,
                       graph()->NewNode(machine->Int32Add(), key_offset,
                                        Int32Constant(fixed_offset)),
                       *effect_, *control_);
  int32_t canonical_sig_num = env_->module->signature_ids[sig_index];
  CHECK_GE(sig_index, 0);
  Node* sig_match = graph()->NewNode(machine->WordEqual(), load_sig,
                                     jsgraph()->SmiConstant(canonical_sig_num));
  TrapIfFalse(wasm::kTrapFuncSigMismatch, sig_match, position);

  // Load code object from the table. It is held by a Foreign.
  Node* entry = graph()->NewNode(
      machine->Load(MachineType::AnyTagged()), table,
      graph()->NewNode(machine->Int32Add(), key_offset,
                       Uint32Constant(fixed_offset + kPointerSize)),
      *effect_, *control_);
  if (FLAG_wasm_jit_to_native) {
    Node* address = graph()->NewNode(
        machine->Load(MachineType::Pointer()), entry,
        Int32Constant(Foreign::kForeignAddressOffset - kHeapObjectTag),
        *effect_, *control_);
    args[0] = address;
  } else {
    args[0] = entry;
  }
  return BuildWasmCall(sig, args, rets, position);
}

Node* WasmGraphBuilder::BuildI32Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word32Rol opcode in TurboFan.
  Int32Matcher m(right);
  if (m.HasValue()) {
    return Binop(wasm::kExprI32Ror, left,
                 jsgraph()->Int32Constant(32 - m.Value()));
  } else {
    return Binop(wasm::kExprI32Ror, left,
                 Binop(wasm::kExprI32Sub, jsgraph()->Int32Constant(32), right));
  }
}

Node* WasmGraphBuilder::BuildI64Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word64Rol opcode in TurboFan.
  Int64Matcher m(right);
  if (m.HasValue()) {
    return Binop(wasm::kExprI64Ror, left,
                 jsgraph()->Int64Constant(64 - m.Value()));
  } else {
    return Binop(wasm::kExprI64Ror, left,
                 Binop(wasm::kExprI64Sub, jsgraph()->Int64Constant(64), right));
  }
}

Node* WasmGraphBuilder::Invert(Node* node) {
  return Unop(wasm::kExprI32Eqz, node);
}

Node* WasmGraphBuilder::BuildChangeInt32ToTagged(Node* value) {
  MachineOperatorBuilder* machine = jsgraph()->machine();
  CommonOperatorBuilder* common = jsgraph()->common();

  if (machine->Is64()) {
    return BuildChangeInt32ToSmi(value);
  }

  Node* add = graph()->NewNode(machine->Int32AddWithOverflow(), value, value,
                               graph()->start());

  Node* ovf = graph()->NewNode(common->Projection(1), add, graph()->start());
  Node* branch = graph()->NewNode(common->Branch(BranchHint::kFalse), ovf,
                                  graph()->start());

  Node* if_true = graph()->NewNode(common->IfTrue(), branch);
  Node* vtrue = BuildAllocateHeapNumberWithValue(
      graph()->NewNode(machine->ChangeInt32ToFloat64(), value), if_true);

  Node* if_false = graph()->NewNode(common->IfFalse(), branch);
  Node* vfalse = graph()->NewNode(common->Projection(0), add, if_false);

  Node* merge = graph()->NewNode(common->Merge(2), if_true, if_false);
  Node* phi = graph()->NewNode(common->Phi(MachineRepresentation::kTagged, 2),
                               vtrue, vfalse, merge);
  return phi;
}

Node* WasmGraphBuilder::BuildChangeFloat64ToTagged(Node* value) {
  MachineOperatorBuilder* machine = jsgraph()->machine();
  CommonOperatorBuilder* common = jsgraph()->common();

  Node* value32 = graph()->NewNode(machine->RoundFloat64ToInt32(), value);
  Node* check_same = graph()->NewNode(
      machine->Float64Equal(), value,
      graph()->NewNode(machine->ChangeInt32ToFloat64(), value32));
  Node* branch_same =
      graph()->NewNode(common->Branch(), check_same, graph()->start());

  Node* if_smi = graph()->NewNode(common->IfTrue(), branch_same);
  Node* vsmi;
  Node* if_box = graph()->NewNode(common->IfFalse(), branch_same);
  Node* vbox;

  // We only need to check for -0 if the {value} can potentially contain -0.
  Node* check_zero = graph()->NewNode(machine->Word32Equal(), value32,
                                      jsgraph()->Int32Constant(0));
  Node* branch_zero =
      graph()->NewNode(common->Branch(BranchHint::kFalse), check_zero, if_smi);

  Node* if_zero = graph()->NewNode(common->IfTrue(), branch_zero);
  Node* if_notzero = graph()->NewNode(common->IfFalse(), branch_zero);

  // In case of 0, we need to check the high bits for the IEEE -0 pattern.
  Node* check_negative = graph()->NewNode(
      machine->Int32LessThan(),
      graph()->NewNode(machine->Float64ExtractHighWord32(), value),
      jsgraph()->Int32Constant(0));
  Node* branch_negative = graph()->NewNode(common->Branch(BranchHint::kFalse),
                                           check_negative, if_zero);

  Node* if_negative = graph()->NewNode(common->IfTrue(), branch_negative);
  Node* if_notnegative = graph()->NewNode(common->IfFalse(), branch_negative);

  // We need to create a box for negative 0.
  if_smi = graph()->NewNode(common->Merge(2), if_notzero, if_notnegative);
  if_box = graph()->NewNode(common->Merge(2), if_box, if_negative);

  // On 64-bit machines we can just wrap the 32-bit integer in a smi, for 32-bit
  // machines we need to deal with potential overflow and fallback to boxing.
  if (machine->Is64()) {
    vsmi = BuildChangeInt32ToSmi(value32);
  } else {
    Node* smi_tag = graph()->NewNode(machine->Int32AddWithOverflow(), value32,
                                     value32, if_smi);

    Node* check_ovf = graph()->NewNode(common->Projection(1), smi_tag, if_smi);
    Node* branch_ovf =
        graph()->NewNode(common->Branch(BranchHint::kFalse), check_ovf, if_smi);

    Node* if_ovf = graph()->NewNode(common->IfTrue(), branch_ovf);
    if_box = graph()->NewNode(common->Merge(2), if_ovf, if_box);

    if_smi = graph()->NewNode(common->IfFalse(), branch_ovf);
    vsmi = graph()->NewNode(common->Projection(0), smi_tag, if_smi);
  }

  // Allocate the box for the {value}.
  vbox = BuildAllocateHeapNumberWithValue(value, if_box);

  Node* control = graph()->NewNode(common->Merge(2), if_smi, if_box);
  value = graph()->NewNode(common->Phi(MachineRepresentation::kTagged, 2), vsmi,
                           vbox, control);
  return value;
}

Node* WasmGraphBuilder::ToJS(Node* node, wasm::ValueType type) {
  switch (type) {
    case wasm::kWasmI32:
      return BuildChangeInt32ToTagged(node);
    case wasm::kWasmS128:
    case wasm::kWasmI64:
      UNREACHABLE();
    case wasm::kWasmF32:
      node = graph()->NewNode(jsgraph()->machine()->ChangeFloat32ToFloat64(),
                              node);
      return BuildChangeFloat64ToTagged(node);
    case wasm::kWasmF64:
      return BuildChangeFloat64ToTagged(node);
    case wasm::kWasmStmt:
      return jsgraph()->UndefinedConstant();
    default:
      UNREACHABLE();
  }
}

Node* WasmGraphBuilder::BuildJavaScriptToNumber(Node* node, Node* js_context) {
  Callable callable =
      Builtins::CallableFor(jsgraph()->isolate(), Builtins::kToNumber);
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      jsgraph()->isolate(), jsgraph()->zone(), callable.descriptor(), 0,
      CallDescriptor::kNoFlags, Operator::kNoProperties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());

  Node* result = graph()->NewNode(jsgraph()->common()->Call(desc), stub_code,
                                  node, js_context, *effect_, *control_);

  SetSourcePosition(result, 1);

  *effect_ = result;

  return result;
}

bool CanCover(Node* value, IrOpcode::Value opcode) {
  if (value->opcode() != opcode) return false;
  bool first = true;
  for (Edge const edge : value->use_edges()) {
    if (NodeProperties::IsControlEdge(edge)) continue;
    if (NodeProperties::IsEffectEdge(edge)) continue;
    DCHECK(NodeProperties::IsValueEdge(edge));
    if (!first) return false;
    first = false;
  }
  return true;
}

Node* WasmGraphBuilder::BuildChangeTaggedToFloat64(Node* value) {
  MachineOperatorBuilder* machine = jsgraph()->machine();
  CommonOperatorBuilder* common = jsgraph()->common();

  Node* check = BuildTestNotSmi(value);
  Node* branch = graph()->NewNode(common->Branch(BranchHint::kFalse), check,
                                  graph()->start());

  Node* if_not_smi = graph()->NewNode(common->IfTrue(), branch);

  Node* vnot_smi;
  Node* check_undefined = graph()->NewNode(machine->WordEqual(), value,
                                           jsgraph()->UndefinedConstant());
  Node* branch_undefined = graph()->NewNode(common->Branch(BranchHint::kFalse),
                                            check_undefined, if_not_smi);

  Node* if_undefined = graph()->NewNode(common->IfTrue(), branch_undefined);
  Node* vundefined =
      jsgraph()->Float64Constant(std::numeric_limits<double>::quiet_NaN());

  Node* if_not_undefined =
      graph()->NewNode(common->IfFalse(), branch_undefined);
  Node* vheap_number = BuildLoadHeapNumberValue(value, if_not_undefined);

  if_not_smi =
      graph()->NewNode(common->Merge(2), if_undefined, if_not_undefined);
  vnot_smi = graph()->NewNode(common->Phi(MachineRepresentation::kFloat64, 2),
                              vundefined, vheap_number, if_not_smi);

  Node* if_smi = graph()->NewNode(common->IfFalse(), branch);
  Node* vfrom_smi = BuildChangeSmiToFloat64(value);

  Node* merge = graph()->NewNode(common->Merge(2), if_not_smi, if_smi);
  Node* phi = graph()->NewNode(common->Phi(MachineRepresentation::kFloat64, 2),
                               vnot_smi, vfrom_smi, merge);

  return phi;
}

Node* WasmGraphBuilder::FromJS(Node* node, Node* js_context,
                               wasm::ValueType type) {
  DCHECK_NE(wasm::kWasmStmt, type);

  // Do a JavaScript ToNumber.
  Node* num = BuildJavaScriptToNumber(node, js_context);

  // Change representation.
  SimplifiedOperatorBuilder simplified(jsgraph()->zone());
  num = BuildChangeTaggedToFloat64(num);

  switch (type) {
    case wasm::kWasmI32: {
      num = graph()->NewNode(jsgraph()->machine()->TruncateFloat64ToWord32(),
                             num);
      break;
    }
    case wasm::kWasmS128:
    case wasm::kWasmI64:
      UNREACHABLE();
    case wasm::kWasmF32:
      num = graph()->NewNode(jsgraph()->machine()->TruncateFloat64ToFloat32(),
                             num);
      break;
    case wasm::kWasmF64:
      break;
    default:
      UNREACHABLE();
  }
  return num;
}

Node* WasmGraphBuilder::BuildChangeInt32ToSmi(Node* value) {
  if (jsgraph()->machine()->Is64()) {
    value = graph()->NewNode(jsgraph()->machine()->ChangeInt32ToInt64(), value);
  }
  return graph()->NewNode(jsgraph()->machine()->WordShl(), value,
                          BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildChangeSmiToInt32(Node* value) {
  value = graph()->NewNode(jsgraph()->machine()->WordSar(), value,
                           BuildSmiShiftBitsConstant());
  if (jsgraph()->machine()->Is64()) {
    value =
        graph()->NewNode(jsgraph()->machine()->TruncateInt64ToInt32(), value);
  }
  return value;
}

Node* WasmGraphBuilder::BuildChangeUint32ToSmi(Node* value) {
  return graph()->NewNode(jsgraph()->machine()->WordShl(),
                          Uint32ToUintptr(value), BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildChangeSmiToFloat64(Node* value) {
  return graph()->NewNode(jsgraph()->machine()->ChangeInt32ToFloat64(),
                          BuildChangeSmiToInt32(value));
}

Node* WasmGraphBuilder::BuildTestNotSmi(Node* value) {
  STATIC_ASSERT(kSmiTag == 0);
  STATIC_ASSERT(kSmiTagMask == 1);
  return graph()->NewNode(jsgraph()->machine()->WordAnd(), value,
                          jsgraph()->IntPtrConstant(kSmiTagMask));
}

Node* WasmGraphBuilder::BuildSmiShiftBitsConstant() {
  return jsgraph()->IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* WasmGraphBuilder::BuildAllocateHeapNumberWithValue(Node* value,
                                                         Node* control) {
  MachineOperatorBuilder* machine = jsgraph()->machine();
  CommonOperatorBuilder* common = jsgraph()->common();
  // The AllocateHeapNumber builtin does not use the js_context, so we can
  // safely pass in Smi zero here.
  Callable callable = Builtins::CallableFor(jsgraph()->isolate(),
                                            Builtins::kAllocateHeapNumber);
  Node* target = jsgraph()->HeapConstant(callable.code());
  Node* js_context = jsgraph()->NoContextConstant();
  Node* effect =
      graph()->NewNode(common->BeginRegion(RegionObservability::kNotObservable),
                       graph()->start());
  if (!allocate_heap_number_operator_.is_set()) {
    CallDescriptor* descriptor = Linkage::GetStubCallDescriptor(
        jsgraph()->isolate(), jsgraph()->zone(), callable.descriptor(), 0,
        CallDescriptor::kNoFlags, Operator::kNoThrow);
    allocate_heap_number_operator_.set(common->Call(descriptor));
  }
  Node* heap_number = graph()->NewNode(allocate_heap_number_operator_.get(),
                                       target, js_context, effect, control);
  Node* store =
      graph()->NewNode(machine->Store(StoreRepresentation(
                           MachineRepresentation::kFloat64, kNoWriteBarrier)),
                       heap_number, BuildHeapNumberValueIndexConstant(), value,
                       heap_number, control);
  return graph()->NewNode(common->FinishRegion(), heap_number, store);
}

Node* WasmGraphBuilder::BuildLoadHeapNumberValue(Node* value, Node* control) {
  return graph()->NewNode(jsgraph()->machine()->Load(MachineType::Float64()),
                          value, BuildHeapNumberValueIndexConstant(),
                          graph()->start(), control);
}

Node* WasmGraphBuilder::BuildHeapNumberValueIndexConstant() {
  return jsgraph()->IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag);
}

void WasmGraphBuilder::BuildJSToWasmWrapper(WasmCodeWrapper wasm_code,
                                            Address wasm_context_address) {
  const int wasm_count = static_cast<int>(sig_->parameter_count());
  const int count =
      wasm_count + 4;  // wasm_code, wasm_context, effect, and control.
  Node** args = Buffer(count);

  // Build the start and the JS parameter nodes.
  Node* start = Start(wasm_count + 5);
  *control_ = start;
  *effect_ = start;

  // Create the js_context parameter
  Node* js_context = graph()->NewNode(
      jsgraph()->common()->Parameter(
          Linkage::GetJSCallContextParamIndex(wasm_count + 1), "%context"),
      graph()->start());

  // Create the wasm_context node to pass as parameter. This must be a
  // RelocatableIntPtrConstant because JSToWasm wrappers are compiled at module
  // compile time and patched at instance build time.
  DCHECK_NULL(wasm_context_);
  wasm_context_ = jsgraph()->RelocatableIntPtrConstant(
      reinterpret_cast<uintptr_t>(wasm_context_address),
      RelocInfo::WASM_CONTEXT_REFERENCE);

  Node* wasm_code_node = nullptr;
  if (!wasm_code.IsCodeObject()) {
    const wasm::WasmCode* code = wasm_code.GetWasmCode();
    Address instr_start =
        code == nullptr ? nullptr : code->instructions().start();
    wasm_code_node = jsgraph()->RelocatableIntPtrConstant(
        reinterpret_cast<intptr_t>(instr_start), RelocInfo::JS_TO_WASM_CALL);
  } else {
    wasm_code_node = HeapConstant(wasm_code.GetCode());
  }
  if (!wasm::IsJSCompatibleSignature(sig_)) {
    // Throw a TypeError. Use the js_context of the calling javascript function
    // (passed as a parameter), such that the generated code is js_context
    // independent.
    BuildCallToRuntimeWithContext(Runtime::kWasmThrowTypeError, js_context,
                                  nullptr, 0);

    // Add a dummy call to the wasm function so that the generated wrapper
    // contains a reference to the wrapped wasm function. Without this reference
    // the wasm function could not be re-imported into another wasm module.
    int pos = 0;
    args[pos++] = wasm_code_node;
    args[pos++] = wasm_context_.get();
    args[pos++] = *effect_;
    args[pos++] = *control_;

    // We only need a dummy call descriptor.
    wasm::FunctionSig::Builder dummy_sig_builder(jsgraph()->zone(), 0, 0);
    CallDescriptor* desc =
        GetWasmCallDescriptor(jsgraph()->zone(), dummy_sig_builder.Build());
    *effect_ = graph()->NewNode(jsgraph()->common()->Call(desc), pos, args);
    Return(jsgraph()->UndefinedConstant());
    return;
  }

  int pos = 0;
  args[pos++] = wasm_code_node;
  args[pos++] = wasm_context_.get();

  // Convert JS parameters to wasm numbers.
  for (int i = 0; i < wasm_count; ++i) {
    Node* param = Param(i + 1);
    Node* wasm_param = FromJS(param, js_context, sig_->GetParam(i));
    args[pos++] = wasm_param;
  }

  // Set the ThreadInWasm flag before we do the actual call.
  BuildModifyThreadInWasmFlag(true);

  args[pos++] = *effect_;
  args[pos++] = *control_;

  // Call the wasm code.
  CallDescriptor* desc = GetWasmCallDescriptor(jsgraph()->zone(), sig_);

  Node* call = graph()->NewNode(jsgraph()->common()->Call(desc), count, args);
  *effect_ = call;

  // Clear the ThreadInWasmFlag
  BuildModifyThreadInWasmFlag(false);

  Node* retval = call;
  Node* jsval = ToJS(
      retval, sig_->return_count() == 0 ? wasm::kWasmStmt : sig_->GetReturn());
  Return(jsval);
}

int WasmGraphBuilder::AddParameterNodes(Node** args, int pos, int param_count,
                                        wasm::FunctionSig* sig) {
  // Convert wasm numbers to JS values.
  for (int i = 0; i < param_count; ++i) {
    Node* param = Param(i + 1);  // Start from index 1 to drop the wasm_context.
    args[pos++] = ToJS(param, sig->GetParam(i));
  }
  return pos;
}

Node* WasmGraphBuilder::LoadImportDataAtOffset(int offset, Node* table) {
  offset = FixedArray::OffsetOfElementAt(offset) - kHeapObjectTag;
  Node* offset_node = jsgraph()->Int32Constant(offset);
  Node* import_data = graph()->NewNode(
      jsgraph()->machine()->Load(LoadRepresentation::TaggedPointer()), table,
      offset_node, *effect_, *control_);
  *effect_ = import_data;
  return import_data;
}

Node* WasmGraphBuilder::LoadNativeContext(Node* table) {
  // The js_imports_table is set up so that index 0 has isolate->native_context
  return LoadImportDataAtOffset(0, table);
}

int OffsetForImportData(int index, WasmGraphBuilder::ImportDataType type) {
  // The js_imports_table is set up so that index 0 has isolate->native_context
  // and for every index, 3*index+1 has the JSReceiver, 3*index+2 has function's
  // global proxy and 3*index+3 has function's context.
  return 3 * index + type;
}

Node* WasmGraphBuilder::LoadImportData(int index, ImportDataType type,
                                       Node* table) {
  return LoadImportDataAtOffset(OffsetForImportData(index, type), table);
}

bool WasmGraphBuilder::BuildWasmToJSWrapper(
    Handle<JSReceiver> target, Handle<FixedArray> global_js_imports_table,
    int index) {
  DCHECK(target->IsCallable());

  int wasm_count = static_cast<int>(sig_->parameter_count());

  // Build the start and the parameter nodes.
  Isolate* isolate = jsgraph()->isolate();
  CallDescriptor* desc;
  Node* start = Start(wasm_count + 3);
  *effect_ = start;
  *control_ = start;

  // We add the target function to a table and look it up during runtime. This
  // ensures that if the GC kicks in, it doesn't need to patch the code for the
  // JS function.
  // js_imports_table is fixed array with global handle scope whose lifetime is
  // tied to the instance.
  // TODO(aseemgarg): explore using per-import global handle instead of a table
  Node* table_ptr = jsgraph()->IntPtrConstant(
      reinterpret_cast<intptr_t>(global_js_imports_table.location()));
  Node* table = graph()->NewNode(
      jsgraph()->machine()->Load(LoadRepresentation::TaggedPointer()),
      table_ptr, jsgraph()->IntPtrConstant(0), *effect_, *control_);
  *effect_ = table;

  if (!wasm::IsJSCompatibleSignature(sig_)) {
    // Throw a TypeError.
    Node* native_context = LoadNativeContext(table);
    BuildCallToRuntimeWithContext(Runtime::kWasmThrowTypeError, native_context,
                                  nullptr, 0);
    // We don't need to return a value here, as the runtime call will not return
    // anyway (the c entry stub will trigger stack unwinding).
    ReturnVoid();
    return false;
  }

  Node** args = Buffer(wasm_count + 7);

  Node* call = nullptr;

  BuildModifyThreadInWasmFlag(false);

  if (target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(target);
    if (function->shared()->internal_formal_parameter_count() == wasm_count) {
      int pos = 0;
      args[pos++] =
          LoadImportData(index, kFunction, table);  // target callable.
      // Receiver.
      if (is_sloppy(function->shared()->language_mode()) &&
          !function->shared()->native()) {
        args[pos++] = LoadImportData(index, kGlobalProxy, table);
      } else {
        args[pos++] = jsgraph()->Constant(
            handle(isolate->heap()->undefined_value(), isolate));
      }

      desc = Linkage::GetJSCallDescriptor(
          graph()->zone(), false, wasm_count + 1, CallDescriptor::kNoFlags);

      // Convert wasm numbers to JS values.
      pos = AddParameterNodes(args, pos, wasm_count, sig_);

      args[pos++] = jsgraph()->UndefinedConstant();        // new target
      args[pos++] = jsgraph()->Int32Constant(wasm_count);  // argument count
      args[pos++] = LoadImportData(index, kFunctionContext, table);
      args[pos++] = *effect_;
      args[pos++] = *control_;

      call = graph()->NewNode(jsgraph()->common()->Call(desc), pos, args);
    }
  }

  // We cannot call the target directly, we have to use the Call builtin.
  Node* native_context = nullptr;
  if (!call) {
    int pos = 0;
    Callable callable = CodeFactory::Call(isolate);
    args[pos++] = jsgraph()->HeapConstant(callable.code());
    args[pos++] = LoadImportData(index, kFunction, table);  // target callable.
    args[pos++] = jsgraph()->Int32Constant(wasm_count);  // argument count
    args[pos++] = jsgraph()->Constant(
        handle(isolate->heap()->undefined_value(), isolate));  // receiver

    desc = Linkage::GetStubCallDescriptor(isolate, graph()->zone(),
                                          callable.descriptor(), wasm_count + 1,
                                          CallDescriptor::kNoFlags);

    // Convert wasm numbers to JS values.
    pos = AddParameterNodes(args, pos, wasm_count, sig_);

    // The native_context is sufficient here, because all kind of callables
    // which depend on the context provide their own context. The context here
    // is only needed if the target is a constructor to throw a TypeError, if
    // the target is a native function, or if the target is a callable JSObject,
    // which can only be constructed by the runtime.
    native_context = LoadNativeContext(table);
    args[pos++] = native_context;
    args[pos++] = *effect_;
    args[pos++] = *control_;

    call = graph()->NewNode(jsgraph()->common()->Call(desc), pos, args);
  }

  *effect_ = call;
  SetSourcePosition(call, 0);

  BuildModifyThreadInWasmFlag(true);

  // Convert the return value back.
  Node* val = sig_->return_count() == 0
                  ? jsgraph()->Int32Constant(0)
                  : FromJS(call,
                           native_context != nullptr ? native_context
                                                     : LoadNativeContext(table),
                           sig_->GetReturn());
  Return(val);
  return true;
}

namespace {
bool HasInt64ParamOrReturn(wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64) return true;
  }
  return false;
}
}  // namespace

void WasmGraphBuilder::BuildWasmToWasmWrapper(WasmCodeWrapper wasm_code,
                                              Address new_context_address) {
  int wasm_count = static_cast<int>(sig_->parameter_count());
  int count = wasm_count + 4;  // wasm_code, wasm_context, effect, and control.
  Node** args = Buffer(count);

  // Build the start node.
  Node* start = Start(count + 1);
  *control_ = start;
  *effect_ = start;

  int pos = 0;
  // Add the wasm code target.
  if (!wasm_code.IsCodeObject()) {
    const wasm::WasmCode* code = wasm_code.GetWasmCode();
    Address instr_start =
        code == nullptr ? nullptr : code->instructions().start();
    args[pos++] = jsgraph()->RelocatableIntPtrConstant(
        reinterpret_cast<intptr_t>(instr_start), RelocInfo::JS_TO_WASM_CALL);
  } else {
    args[pos++] = jsgraph()->HeapConstant(wasm_code.GetCode());
  }
  // Add the wasm_context of the other instance.
  args[pos++] = jsgraph()->IntPtrConstant(
      reinterpret_cast<uintptr_t>(new_context_address));
  // Add the parameters starting from index 1 since the parameter with index 0
  // is the old wasm_context.
  for (int i = 0; i < wasm_count; ++i) {
    args[pos++] = Param(i + 1);
  }
  args[pos++] = *effect_;
  args[pos++] = *control_;

  // Tail-call the wasm code.
  CallDescriptor* desc = GetWasmCallDescriptor(jsgraph()->zone(), sig_);
  Node* tail_call =
      graph()->NewNode(jsgraph()->common()->TailCall(desc), count, args);
  MergeControlToEnd(jsgraph(), tail_call);
}

void WasmGraphBuilder::BuildWasmInterpreterEntry(uint32_t func_index) {
  int param_count = static_cast<int>(sig_->parameter_count());

  // Build the start and the parameter nodes.
  Node* start = Start(param_count + 3);
  *effect_ = start;
  *control_ = start;

  // Compute size for the argument buffer.
  int args_size_bytes = 0;
  for (wasm::ValueType type : sig_->parameters()) {
    args_size_bytes += 1 << ElementSizeLog2Of(type);
  }

  // The return value is also passed via this buffer:
  DCHECK_GE(wasm::kV8MaxWasmFunctionReturns, sig_->return_count());
  // TODO(wasm): Handle multi-value returns.
  DCHECK_EQ(1, wasm::kV8MaxWasmFunctionReturns);
  int return_size_bytes =
      sig_->return_count() == 0 ? 0 : 1 << ElementSizeLog2Of(sig_->GetReturn());

  // Get a stack slot for the arguments.
  Node* arg_buffer =
      args_size_bytes == 0 && return_size_bytes == 0
          ? jsgraph()->IntPtrConstant(0)
          : graph()->NewNode(jsgraph()->machine()->StackSlot(
                std::max(args_size_bytes, return_size_bytes), 8));

  // Now store all our arguments to the buffer.
  int offset = 0;

  for (int i = 0; i < param_count; ++i) {
    wasm::ValueType type = sig_->GetParam(i);
    // Start from the parameter with index 1 to drop the wasm_context.
    *effect_ = graph()->NewNode(GetSafeStoreOperator(offset, type), arg_buffer,
                                Int32Constant(offset), Param(i + 1), *effect_,
                                *control_);
    offset += 1 << ElementSizeLog2Of(type);
  }
  DCHECK_EQ(args_size_bytes, offset);

  // We are passing the raw arg_buffer here. To the GC and other parts, it looks
  // like a Smi (lowest bit not set). In the runtime function however, don't
  // call Smi::value on it, but just cast it to a byte pointer.
  Node* parameters[] = {
      jsgraph()->SmiConstant(func_index),  // function index
      arg_buffer,                          // argument buffer
  };
  BuildCallToRuntime(Runtime::kWasmRunInterpreter, parameters,
                     arraysize(parameters));

  // Read back the return value.
  if (sig_->return_count() == 0) {
    Return(Int32Constant(0));
  } else {
    // TODO(wasm): Implement multi-return.
    DCHECK_EQ(1, sig_->return_count());
    MachineType load_rep = wasm::WasmOpcodes::MachineTypeFor(sig_->GetReturn());
    Node* val =
        graph()->NewNode(jsgraph()->machine()->Load(load_rep), arg_buffer,
                         Int32Constant(0), *effect_, *control_);
    Return(val);
  }

  if (HasInt64ParamOrReturn(sig_)) LowerInt64();
}

void WasmGraphBuilder::BuildCWasmEntry(Address wasm_context_address) {
  // Build the start and the JS parameter nodes.
  Node* start = Start(CWasmEntryParameters::kNumParameters + 5);
  *control_ = start;
  *effect_ = start;

  // Create the wasm_context node to pass as parameter.
  DCHECK_NULL(wasm_context_);
  wasm_context_ = jsgraph()->IntPtrConstant(
      reinterpret_cast<uintptr_t>(wasm_context_address));

  // Create parameter nodes (offset by 1 for the receiver parameter).
  Node* code_obj = nullptr;
  if (FLAG_wasm_jit_to_native) {
    Node* foreign_code_obj = Param(CWasmEntryParameters::kCodeObject + 1);
    MachineOperatorBuilder* machine = jsgraph()->machine();
    code_obj = graph()->NewNode(
        machine->Load(MachineType::Pointer()), foreign_code_obj,
        Int32Constant(Foreign::kForeignAddressOffset - kHeapObjectTag),
        *effect_, *control_);
  } else {
    code_obj = Param(CWasmEntryParameters::kCodeObject + 1);
  }
  Node* arg_buffer = Param(CWasmEntryParameters::kArgumentsBuffer + 1);

  int wasm_arg_count = static_cast<int>(sig_->parameter_count());
  int arg_count = wasm_arg_count + 4;  // code, wasm_context, control, effect
  Node** args = Buffer(arg_count);

  int pos = 0;
  args[pos++] = code_obj;
  args[pos++] = wasm_context_.get();

  int offset = 0;
  for (wasm::ValueType type : sig_->parameters()) {
    Node* arg_load =
        graph()->NewNode(GetSafeLoadOperator(offset, type), arg_buffer,
                         Int32Constant(offset), *effect_, *control_);
    *effect_ = arg_load;
    args[pos++] = arg_load;
    offset += 1 << ElementSizeLog2Of(type);
  }

  args[pos++] = *effect_;
  args[pos++] = *control_;
  DCHECK_EQ(arg_count, pos);

  // Call the wasm code.
  CallDescriptor* desc = GetWasmCallDescriptor(jsgraph()->zone(), sig_);

  Node* call =
      graph()->NewNode(jsgraph()->common()->Call(desc), arg_count, args);
  *effect_ = call;

  // Store the return value.
  DCHECK_GE(1, sig_->return_count());
  if (sig_->return_count() == 1) {
    StoreRepresentation store_rep(sig_->GetReturn(), kNoWriteBarrier);
    Node* store =
        graph()->NewNode(jsgraph()->machine()->Store(store_rep), arg_buffer,
                         Int32Constant(0), call, *effect_, *control_);
    *effect_ = store;
  }
  Return(jsgraph()->SmiConstant(0));

  if (jsgraph()->machine()->Is32() && HasInt64ParamOrReturn(sig_)) {
    MachineRepresentation sig_reps[] = {
        MachineRepresentation::kWord32,  // return value
        MachineRepresentation::kTagged,  // receiver
        MachineRepresentation::kTagged,  // arg0 (code)
        MachineRepresentation::kTagged   // arg1 (buffer)
    };
    wasm::FunctionSig c_entry_sig(1, 2, sig_reps);
    Int64Lowering r(jsgraph()->graph(), jsgraph()->machine(),
                    jsgraph()->common(), jsgraph()->zone(), &c_entry_sig);
    r.LowerGraph();
  }
}

void WasmGraphBuilder::InitContextCache(WasmContextCacheNodes* context_cache) {
  DCHECK_NOT_NULL(wasm_context_);
  DCHECK_NOT_NULL(*control_);
  DCHECK_NOT_NULL(*effect_);

  // Load the memory start.
  Node* mem_start = graph()->NewNode(
      jsgraph()->machine()->Load(MachineType::UintPtr()), wasm_context_.get(),
      jsgraph()->Int32Constant(
          static_cast<int32_t>(offsetof(WasmContext, mem_start))),
      *effect_, *control_);
  *effect_ = mem_start;
  context_cache->mem_start = mem_start;

  // Load the memory size.
  Node* mem_size = graph()->NewNode(
      jsgraph()->machine()->Load(MachineType::Uint32()), wasm_context_.get(),
      jsgraph()->Int32Constant(
          static_cast<int32_t>(offsetof(WasmContext, mem_size))),
      *effect_, *control_);
  *effect_ = mem_size;
  context_cache->mem_size = mem_size;

  if (untrusted_code_mitigations_) {
    // Load the memory mask.
    Node* mem_mask = graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::Uint32()), wasm_context_.get(),
        jsgraph()->Int32Constant(
            static_cast<int32_t>(offsetof(WasmContext, mem_mask))),
        *effect_, *control_);
    *effect_ = mem_mask;
    context_cache->mem_mask = mem_mask;
  } else {
    // Explicitly set to nullptr to ensure a SEGV when we try to use it.
    context_cache->mem_mask = nullptr;
  }
}

void WasmGraphBuilder::PrepareContextCacheForLoop(
    WasmContextCacheNodes* context_cache, Node* control) {
#define INTRODUCE_PHI(field, rep) \
  context_cache->field = Phi(rep, 1, &context_cache->field, control);

  INTRODUCE_PHI(mem_start, MachineType::PointerRepresentation());
  INTRODUCE_PHI(mem_size, MachineRepresentation::kWord32);
  if (untrusted_code_mitigations_) {
    INTRODUCE_PHI(mem_mask, MachineRepresentation::kWord32);
  }

#undef INTRODUCE_PHI
}

void WasmGraphBuilder::NewContextCacheMerge(WasmContextCacheNodes* to,
                                            WasmContextCacheNodes* from,
                                            Node* merge) {
#define INTRODUCE_PHI(field, rep)            \
  if (to->field != from->field) {            \
    Node* vals[] = {to->field, from->field}; \
    to->field = Phi(rep, 2, vals, merge);    \
  }

  INTRODUCE_PHI(mem_start, MachineType::PointerRepresentation());
  INTRODUCE_PHI(mem_size, MachineRepresentation::kWord32);
  if (untrusted_code_mitigations_) {
    INTRODUCE_PHI(mem_mask, MachineRepresentation::kWord32);
  }

#undef INTRODUCE_PHI
}

void WasmGraphBuilder::MergeContextCacheInto(WasmContextCacheNodes* to,
                                             WasmContextCacheNodes* from,
                                             Node* merge) {
  to->mem_size = CreateOrMergeIntoPhi(MachineRepresentation::kWord32, merge,
                                      to->mem_size, from->mem_size);
  to->mem_start = CreateOrMergeIntoPhi(MachineType::PointerRepresentation(),
                                       merge, to->mem_start, from->mem_start);
  if (untrusted_code_mitigations_) {
    to->mem_mask = CreateOrMergeIntoPhi(MachineRepresentation::kWord32, merge,
                                        to->mem_mask, from->mem_mask);
  }
}

Node* WasmGraphBuilder::CreateOrMergeIntoPhi(wasm::ValueType type, Node* merge,
                                             Node* tnode, Node* fnode) {
  if (IsPhiWithMerge(tnode, merge)) {
    AppendToPhi(tnode, fnode);
  } else if (tnode != fnode) {
    uint32_t count = merge->InputCount();
    Node** vals = Buffer(count);
    for (uint32_t j = 0; j < count - 1; j++) vals[j] = tnode;
    vals[count - 1] = fnode;
    return Phi(type, count, vals, merge);
  }
  return tnode;
}

Node* WasmGraphBuilder::CreateOrMergeIntoEffectPhi(Node* merge, Node* tnode,
                                                   Node* fnode) {
  if (IsPhiWithMerge(tnode, merge)) {
    AppendToPhi(tnode, fnode);
  } else if (tnode != fnode) {
    uint32_t count = merge->InputCount();
    Node** effects = Buffer(count);
    for (uint32_t j = 0; j < count - 1; j++) {
      effects[j] = tnode;
    }
    effects[count - 1] = fnode;
    tnode = EffectPhi(count, effects, merge);
  }
  return tnode;
}

void WasmGraphBuilder::GetGlobalBaseAndOffset(MachineType mem_type,
                                              uint32_t offset, Node** base_node,
                                              Node** offset_node) {
  DCHECK_NOT_NULL(wasm_context_);
  if (globals_start_ == nullptr) {
    // Load globals_start from the WasmContext at runtime.
    // TODO(wasm): we currently generate only one load of the {globals_start}
    // start per graph, which means it can be placed anywhere by the scheduler.
    // This is legal because the globals_start should never change.
    // However, in some cases (e.g. if the WasmContext is already in a
    // register), it is slightly more efficient to reload this value from the
    // WasmContext. Since this depends on register allocation, it is not
    // possible to express in the graph, and would essentially constitute a
    // "mem2reg" optimization in TurboFan.
    globals_start_ = graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::UintPtr()), wasm_context_.get(),
        jsgraph()->Int32Constant(
            static_cast<int32_t>(offsetof(WasmContext, globals_start))),
        graph()->start(), graph()->start());
  }
  *base_node = globals_start_.get();
  *offset_node = jsgraph()->Int32Constant(offset);

  if (mem_type == MachineType::Simd128() && offset != 0) {
    // TODO(titzer,bbudge): code generation for SIMD memory offsets is broken.
    *base_node =
        graph()->NewNode(kPointerSize == 4 ? jsgraph()->machine()->Int32Add()
                                           : jsgraph()->machine()->Int64Add(),
                         *base_node, *offset_node);
    *offset_node = jsgraph()->Int32Constant(0);
  }
}

Node* WasmGraphBuilder::MemBuffer(uint32_t offset) {
  DCHECK_NOT_NULL(context_cache_);
  Node* mem_start = context_cache_->mem_start;
  DCHECK_NOT_NULL(mem_start);
  if (offset == 0) return mem_start;
  return graph()->NewNode(jsgraph()->machine()->IntAdd(), mem_start,
                          jsgraph()->IntPtrConstant(offset));
}

Node* WasmGraphBuilder::CurrentMemoryPages() {
  // CurrentMemoryPages can not be called from asm.js.
  DCHECK_EQ(wasm::kWasmOrigin, env_->module->origin());
  DCHECK_NOT_NULL(context_cache_);
  Node* mem_size = context_cache_->mem_size;
  DCHECK_NOT_NULL(mem_size);
  if (jsgraph()->machine()->Is64()) {
    mem_size = graph()->NewNode(jsgraph()->machine()->TruncateInt64ToInt32(),
                                mem_size);
  }
  return graph()->NewNode(
      jsgraph()->machine()->Word32Shr(), mem_size,
      jsgraph()->Int32Constant(WhichPowerOf2(wasm::kWasmPageSize)));
}

void WasmGraphBuilder::EnsureFunctionTableNodes() {
  if (function_tables_.size() > 0) return;
  size_t tables_size = env_->function_tables.size();
  for (size_t i = 0; i < tables_size; ++i) {
    wasm::GlobalHandleAddress function_handle_address =
        env_->function_tables[i];
    Node* table_addr = jsgraph()->RelocatableIntPtrConstant(
        reinterpret_cast<intptr_t>(function_handle_address),
        RelocInfo::WASM_GLOBAL_HANDLE);
    uint32_t table_size = env_->module->function_tables[i].initial_size;
    Node* size = jsgraph()->RelocatableInt32Constant(
        static_cast<uint32_t>(table_size),
        RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE);
    function_tables_.push_back({table_addr, size});
  }
}

Node* WasmGraphBuilder::BuildModifyThreadInWasmFlag(bool new_value) {
  // TODO(eholk): generate code to modify the thread-local storage directly,
  // rather than calling the runtime.
  if (!use_trap_handler()) {
    return *control_;
  }

  // Using two functions instead of taking the new value as a parameter saves
  // one instruction on each call to set up the parameter.
  ExternalReference ref =
      new_value ? ExternalReference::wasm_set_thread_in_wasm_flag(
                      jsgraph()->isolate())
                : ExternalReference::wasm_clear_thread_in_wasm_flag(
                      jsgraph()->isolate());
  MachineSignature::Builder sig_builder(jsgraph()->zone(), 0, 0);
  return BuildCCall(
      sig_builder.Build(),
      graph()->NewNode(jsgraph()->common()->ExternalConstant(ref)));
}

// Only call this function for code which is not reused across instantiations,
// as we do not patch the embedded js_context.
Node* WasmGraphBuilder::BuildCallToRuntimeWithContext(Runtime::FunctionId f,
                                                      Node* js_context,
                                                      Node** parameters,
                                                      int parameter_count) {
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(
      jsgraph()->zone(), f, fun->nargs, Operator::kNoProperties,
      CallDescriptor::kNoFlags);
  // CEntryStubConstant nodes have to be created and cached in the main
  // thread. At the moment this is only done for CEntryStubConstant(1).
  DCHECK_EQ(1, fun->result_size);
  // At the moment we only allow 4 parameters. If more parameters are needed,
  // increase this constant accordingly.
  static const int kMaxParams = 4;
  DCHECK_GE(kMaxParams, parameter_count);
  Node* inputs[kMaxParams + 6];
  int count = 0;
  inputs[count++] = centry_stub_node_;
  for (int i = 0; i < parameter_count; i++) {
    inputs[count++] = parameters[i];
  }
  inputs[count++] = jsgraph()->ExternalConstant(
      ExternalReference(f, jsgraph()->isolate()));         // ref
  inputs[count++] = jsgraph()->Int32Constant(fun->nargs);  // arity
  inputs[count++] = js_context;                            // js_context
  inputs[count++] = *effect_;
  inputs[count++] = *control_;

  Node* node = jsgraph()->graph()->NewNode(jsgraph()->common()->Call(desc),
                                           count, inputs);
  *effect_ = node;

  return node;
}

Node* WasmGraphBuilder::BuildCallToRuntime(Runtime::FunctionId f,
                                           Node** parameters,
                                           int parameter_count) {
  return BuildCallToRuntimeWithContext(f, jsgraph()->NoContextConstant(),
                                       parameters, parameter_count);
}

Node* WasmGraphBuilder::GetGlobal(uint32_t index) {
  MachineType mem_type =
      wasm::WasmOpcodes::MachineTypeFor(env_->module->globals[index].type);
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(mem_type, env_->module->globals[index].offset, &base,
                         &offset);
  Node* node = graph()->NewNode(jsgraph()->machine()->Load(mem_type), base,
                                offset, *effect_, *control_);
  *effect_ = node;
  return node;
}

Node* WasmGraphBuilder::SetGlobal(uint32_t index, Node* val) {
  MachineType mem_type =
      wasm::WasmOpcodes::MachineTypeFor(env_->module->globals[index].type);
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(mem_type, env_->module->globals[index].offset, &base,
                         &offset);
  const Operator* op = jsgraph()->machine()->Store(
      StoreRepresentation(mem_type.representation(), kNoWriteBarrier));
  Node* node = graph()->NewNode(op, base, offset, val, *effect_, *control_);
  *effect_ = node;
  return node;
}

Node* WasmGraphBuilder::BoundsCheckMem(uint8_t access_size, Node* index,
                                       uint32_t offset,
                                       wasm::WasmCodePosition position,
                                       EnforceBoundsCheck enforce_check) {
  if (FLAG_wasm_no_bounds_checks) return Uint32ToUintptr(index);
  DCHECK_NOT_NULL(context_cache_);
  Node* mem_size = context_cache_->mem_size;
  DCHECK_NOT_NULL(mem_size);

  auto m = jsgraph()->machine();
  if (use_trap_handler() && enforce_check == kCanOmitBoundsCheck) {
    // Simply zero out the 32-bits on 64-bit targets and let the trap handler
    // do its job.
    return Uint32ToUintptr(index);
  }

  uint32_t min_size = env_->module->initial_pages * wasm::kWasmPageSize;
  uint32_t max_size =
      (env_->module->has_maximum_pages ? env_->module->maximum_pages
                                       : wasm::kV8MaxWasmMemoryPages) *
      wasm::kWasmPageSize;

  if (access_size > max_size || offset > max_size - access_size) {
    // The access will be out of bounds, even for the largest memory.
    TrapIfEq32(wasm::kTrapMemOutOfBounds, Int32Constant(0), 0, position);
    return jsgraph()->IntPtrConstant(0);
  }
  DCHECK_LE(1, access_size);
  // This computation cannot overflow, since
  //   {offset <= max_size - access_size <= kMaxUint32 - access_size}.
  // It also cannot underflow, since {access_size >= 1}.
  uint32_t end_offset = offset + access_size - 1;
  Node* end_offset_node = Int32Constant(end_offset);

  // The accessed memory is [index + offset, index + end_offset].
  // Check that the last read byte (at {index + end_offset}) is in bounds.
  // 1) Check that {end_offset < mem_size}. This also ensures that we can safely
  //    compute {effective_size} as {mem_size - end_offset)}.
  //    {effective_size} is >= 1 if condition 1) holds.
  // 2) Check that {index + end_offset < mem_size} by
  //    - computing {effective_size} as {mem_size - end_offset} and
  //    - checking that {index < effective_size}.

  if (end_offset >= min_size) {
    // The end offset is larger than the smallest memory.
    // Dynamically check the end offset against the actual memory size, which
    // is not known at compile time.
    Node* cond = graph()->NewNode(jsgraph()->machine()->Uint32LessThan(),
                                  end_offset_node, mem_size);
    TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
  } else {
    // The end offset is within the bounds of the smallest memory, so only
    // one check is required. Check to see if the index is also a constant.
    Uint32Matcher match(index);
    if (match.HasValue()) {
      uint32_t index_val = match.Value();
      if (index_val < min_size - end_offset) {
        // The input index is a constant and everything is statically within
        // bounds of the smallest possible memory.
        return Uint32ToUintptr(index);
      }
    }
  }

  // This produces a positive number, since {end_offset < min_size <= mem_size}.
  Node* effective_size = graph()->NewNode(jsgraph()->machine()->Int32Sub(),
                                          mem_size, end_offset_node);

  // Introduce the actual bounds check.
  Node* cond = graph()->NewNode(m->Uint32LessThan(), index, effective_size);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);

  if (untrusted_code_mitigations_) {
    // In the fallthrough case, condition the index with the memory mask.
    Node* mem_mask = context_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index = graph()->NewNode(m->Word32And(), index, mem_mask);
  }
  return Uint32ToUintptr(index);
}

const Operator* WasmGraphBuilder::GetSafeLoadOperator(int offset,
                                                      wasm::ValueType type) {
  int alignment = offset % (1 << ElementSizeLog2Of(type));
  MachineType mach_type = wasm::WasmOpcodes::MachineTypeFor(type);
  if (alignment == 0 || jsgraph()->machine()->UnalignedLoadSupported(type)) {
    return jsgraph()->machine()->Load(mach_type);
  }
  return jsgraph()->machine()->UnalignedLoad(mach_type);
}

const Operator* WasmGraphBuilder::GetSafeStoreOperator(int offset,
                                                       wasm::ValueType type) {
  int alignment = offset % (1 << ElementSizeLog2Of(type));
  if (alignment == 0 || jsgraph()->machine()->UnalignedStoreSupported(type)) {
    StoreRepresentation rep(type, WriteBarrierKind::kNoWriteBarrier);
    return jsgraph()->machine()->Store(rep);
  }
  UnalignedStoreRepresentation rep(type);
  return jsgraph()->machine()->UnalignedStore(rep);
}

Node* WasmGraphBuilder::TraceMemoryOperation(bool is_store,
                                             MachineRepresentation rep,
                                             Node* index, uint32_t offset,
                                             wasm::WasmCodePosition position) {
  int kAlign = 4;  // Ensure that the LSB is 0, such that this looks like a Smi.
  Node* info = graph()->NewNode(
      jsgraph()->machine()->StackSlot(sizeof(wasm::MemoryTracingInfo), kAlign));

  Node* address = graph()->NewNode(jsgraph()->machine()->Int32Add(),
                                   Int32Constant(offset), index);
  auto store = [&](int offset, MachineRepresentation rep, Node* data) {
    *effect_ = graph()->NewNode(
        jsgraph()->machine()->Store(StoreRepresentation(rep, kNoWriteBarrier)),
        info, jsgraph()->Int32Constant(offset), data, *effect_, *control_);
  };
  // Store address, is_store, and mem_rep.
  store(offsetof(wasm::MemoryTracingInfo, address),
        MachineRepresentation::kWord32, address);
  store(offsetof(wasm::MemoryTracingInfo, is_store),
        MachineRepresentation::kWord8,
        jsgraph()->Int32Constant(is_store ? 1 : 0));
  store(offsetof(wasm::MemoryTracingInfo, mem_rep),
        MachineRepresentation::kWord8,
        jsgraph()->Int32Constant(static_cast<int>(rep)));

  Node* call = BuildCallToRuntime(Runtime::kWasmTraceMemory, &info, 1);
  SetSourcePosition(call, position);
  return call;
}

Node* WasmGraphBuilder::LoadMem(wasm::ValueType type, MachineType memtype,
                                Node* index, uint32_t offset,
                                uint32_t alignment,
                                wasm::WasmCodePosition position) {
  Node* load;

  // Wasm semantics throw on OOB. Introduce explicit bounds check and
  // conditioning when not using the trap handler.
  index = BoundsCheckMem(wasm::WasmOpcodes::MemSize(memtype), index, offset,
                         position, kCanOmitBoundsCheck);

  if (memtype.representation() == MachineRepresentation::kWord8 ||
      jsgraph()->machine()->UnalignedLoadSupported(memtype.representation())) {
    if (use_trap_handler()) {
      load = graph()->NewNode(jsgraph()->machine()->ProtectedLoad(memtype),
                              MemBuffer(offset), index, *effect_, *control_);
      SetSourcePosition(load, position);
    } else {
      load = graph()->NewNode(jsgraph()->machine()->Load(memtype),
                              MemBuffer(offset), index, *effect_, *control_);
    }
  } else {
    // TODO(eholk): Support unaligned loads with trap handlers.
    DCHECK(!use_trap_handler());
    load = graph()->NewNode(jsgraph()->machine()->UnalignedLoad(memtype),
                            MemBuffer(offset), index, *effect_, *control_);
  }

  *effect_ = load;

#if defined(V8_TARGET_BIG_ENDIAN)
  load = BuildChangeEndiannessLoad(load, memtype, type);
#endif

  if (type == wasm::kWasmI64 &&
      ElementSizeLog2Of(memtype.representation()) < 3) {
    // TODO(titzer): TF zeroes the upper bits of 64-bit loads for subword sizes.
    if (memtype.IsSigned()) {
      // sign extend
      load = graph()->NewNode(jsgraph()->machine()->ChangeInt32ToInt64(), load);
    } else {
      // zero extend
      load =
          graph()->NewNode(jsgraph()->machine()->ChangeUint32ToUint64(), load);
    }
  }

  if (FLAG_wasm_trace_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, offset,
                         position);
  }

  return load;
}

Node* WasmGraphBuilder::StoreMem(MachineRepresentation mem_rep, Node* index,
                                 uint32_t offset, uint32_t alignment, Node* val,
                                 wasm::WasmCodePosition position,
                                 wasm::ValueType type) {
  Node* store;

  index = BoundsCheckMem(wasm::WasmOpcodes::MemSize(mem_rep), index, offset,
                         position, kCanOmitBoundsCheck);

#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndiannessStore(val, mem_rep, type);
#endif

  if (mem_rep == MachineRepresentation::kWord8 ||
      jsgraph()->machine()->UnalignedStoreSupported(mem_rep)) {
    if (use_trap_handler()) {
      store =
          graph()->NewNode(jsgraph()->machine()->ProtectedStore(mem_rep),
                           MemBuffer(offset), index, val, *effect_, *control_);
      SetSourcePosition(store, position);
    } else {
      StoreRepresentation rep(mem_rep, kNoWriteBarrier);
      store =
          graph()->NewNode(jsgraph()->machine()->Store(rep), MemBuffer(offset),
                           index, val, *effect_, *control_);
    }
  } else {
    // TODO(eholk): Support unaligned stores with trap handlers.
    DCHECK(!use_trap_handler());
    UnalignedStoreRepresentation rep(mem_rep);
    store =
        graph()->NewNode(jsgraph()->machine()->UnalignedStore(rep),
                         MemBuffer(offset), index, val, *effect_, *control_);
  }

  *effect_ = store;

  if (FLAG_wasm_trace_memory) {
    TraceMemoryOperation(true, mem_rep, index, offset, position);
  }

  return store;
}

namespace {
Node* GetAsmJsOOBValue(MachineRepresentation rep, JSGraph* jsgraph) {
  switch (rep) {
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return jsgraph->Int32Constant(0);
    case MachineRepresentation::kWord64:
      return jsgraph->Int64Constant(0);
    case MachineRepresentation::kFloat32:
      return jsgraph->Float32Constant(std::numeric_limits<float>::quiet_NaN());
    case MachineRepresentation::kFloat64:
      return jsgraph->Float64Constant(std::numeric_limits<double>::quiet_NaN());
    default:
      UNREACHABLE();
  }
}
}  // namespace

Node* WasmGraphBuilder::BuildAsmjsLoadMem(MachineType type, Node* index) {
  DCHECK_NOT_NULL(context_cache_);
  Node* mem_start = context_cache_->mem_start;
  Node* mem_size = context_cache_->mem_size;
  DCHECK_NOT_NULL(mem_start);
  DCHECK_NOT_NULL(mem_size);

  // Asm.js semantics are defined along the lines of typed arrays, hence OOB
  // reads return {undefined} coerced to the result type (0 for integers, NaN
  // for float and double).
  // Note that we check against the memory size ignoring the size of the
  // stored value, which is conservative if misaligned. Technically, asm.js
  // should never have misaligned accesses.
  Diamond bounds_check(
      graph(), jsgraph()->common(),
      graph()->NewNode(jsgraph()->machine()->Uint32LessThan(), index, mem_size),
      BranchHint::kTrue);
  bounds_check.Chain(*control_);

  if (untrusted_code_mitigations_) {
    // Condition the index with the memory mask.
    Node* mem_mask = context_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index =
        graph()->NewNode(jsgraph()->machine()->Word32And(), index, mem_mask);
  }

  index = Uint32ToUintptr(index);
  Node* load = graph()->NewNode(jsgraph()->machine()->Load(type), mem_start,
                                index, *effect_, bounds_check.if_true);
  Node* value_phi =
      bounds_check.Phi(type.representation(), load,
                       GetAsmJsOOBValue(type.representation(), jsgraph()));
  Node* effect_phi = graph()->NewNode(jsgraph()->common()->EffectPhi(2), load,
                                      *effect_, bounds_check.merge);
  *effect_ = effect_phi;
  *control_ = bounds_check.merge;
  return value_phi;
}

Node* WasmGraphBuilder::Uint32ToUintptr(Node* node) {
  if (jsgraph()->machine()->Is32()) return node;
  return graph()->NewNode(jsgraph()->machine()->ChangeUint32ToUint64(), node);
}

Node* WasmGraphBuilder::BuildAsmjsStoreMem(MachineType type, Node* index,
                                           Node* val) {
  DCHECK_NOT_NULL(context_cache_);
  Node* mem_start = context_cache_->mem_start;
  Node* mem_size = context_cache_->mem_size;
  DCHECK_NOT_NULL(mem_start);
  DCHECK_NOT_NULL(mem_size);

  // Asm.js semantics are to ignore OOB writes.
  // Note that we check against the memory size ignoring the size of the
  // stored value, which is conservative if misaligned. Technically, asm.js
  // should never have misaligned accesses.
  Diamond bounds_check(
      graph(), jsgraph()->common(),
      graph()->NewNode(jsgraph()->machine()->Uint32LessThan(), index, mem_size),
      BranchHint::kTrue);
  bounds_check.Chain(*control_);

  if (untrusted_code_mitigations_) {
    // Condition the index with the memory mask.
    Node* mem_mask = context_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index =
        graph()->NewNode(jsgraph()->machine()->Word32And(), index, mem_mask);
  }

  index = Uint32ToUintptr(index);
  const Operator* store_op = jsgraph()->machine()->Store(StoreRepresentation(
      type.representation(), WriteBarrierKind::kNoWriteBarrier));
  Node* store = graph()->NewNode(store_op, mem_start, index, val, *effect_,
                                 bounds_check.if_true);
  Node* effect_phi = graph()->NewNode(jsgraph()->common()->EffectPhi(2), store,
                                      *effect_, bounds_check.merge);
  *effect_ = effect_phi;
  *control_ = bounds_check.merge;
  return val;
}

void WasmGraphBuilder::PrintDebugName(Node* node) {
  PrintF("#%d:%s", node->id(), node->op()->mnemonic());
}

Node* WasmGraphBuilder::String(const char* string) {
  return jsgraph()->Constant(
      jsgraph()->isolate()->factory()->NewStringFromAsciiChecked(string));
}

Graph* WasmGraphBuilder::graph() { return jsgraph()->graph(); }

void WasmGraphBuilder::LowerInt64() {
  if (jsgraph()->machine()->Is64()) return;
  Int64Lowering r(jsgraph()->graph(), jsgraph()->machine(), jsgraph()->common(),
                  jsgraph()->zone(), sig_);
  r.LowerGraph();
}

void WasmGraphBuilder::SimdScalarLoweringForTesting() {
  SimdScalarLowering(jsgraph(), sig_).LowerGraph();
}

void WasmGraphBuilder::SetSourcePosition(Node* node,
                                         wasm::WasmCodePosition position) {
  DCHECK_NE(position, wasm::kNoCodePosition);
  if (source_position_table_)
    source_position_table_->SetSourcePosition(node, SourcePosition(position));
}

Node* WasmGraphBuilder::S128Zero() {
  has_simd_ = true;
  return graph()->NewNode(jsgraph()->machine()->S128Zero());
}

Node* WasmGraphBuilder::SimdOp(wasm::WasmOpcode opcode, Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF32x4Splat:
      return graph()->NewNode(jsgraph()->machine()->F32x4Splat(), inputs[0]);
    case wasm::kExprF32x4SConvertI32x4:
      return graph()->NewNode(jsgraph()->machine()->F32x4SConvertI32x4(),
                              inputs[0]);
    case wasm::kExprF32x4UConvertI32x4:
      return graph()->NewNode(jsgraph()->machine()->F32x4UConvertI32x4(),
                              inputs[0]);
    case wasm::kExprF32x4Abs:
      return graph()->NewNode(jsgraph()->machine()->F32x4Abs(), inputs[0]);
    case wasm::kExprF32x4Neg:
      return graph()->NewNode(jsgraph()->machine()->F32x4Neg(), inputs[0]);
    case wasm::kExprF32x4RecipApprox:
      return graph()->NewNode(jsgraph()->machine()->F32x4RecipApprox(),
                              inputs[0]);
    case wasm::kExprF32x4RecipSqrtApprox:
      return graph()->NewNode(jsgraph()->machine()->F32x4RecipSqrtApprox(),
                              inputs[0]);
    case wasm::kExprF32x4Add:
      return graph()->NewNode(jsgraph()->machine()->F32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4AddHoriz:
      return graph()->NewNode(jsgraph()->machine()->F32x4AddHoriz(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Sub:
      return graph()->NewNode(jsgraph()->machine()->F32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Mul:
      return graph()->NewNode(jsgraph()->machine()->F32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Min:
      return graph()->NewNode(jsgraph()->machine()->F32x4Min(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Max:
      return graph()->NewNode(jsgraph()->machine()->F32x4Max(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Eq:
      return graph()->NewNode(jsgraph()->machine()->F32x4Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Ne:
      return graph()->NewNode(jsgraph()->machine()->F32x4Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Lt:
      return graph()->NewNode(jsgraph()->machine()->F32x4Lt(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Le:
      return graph()->NewNode(jsgraph()->machine()->F32x4Le(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Gt:
      return graph()->NewNode(jsgraph()->machine()->F32x4Lt(), inputs[1],
                              inputs[0]);
    case wasm::kExprF32x4Ge:
      return graph()->NewNode(jsgraph()->machine()->F32x4Le(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4Splat:
      return graph()->NewNode(jsgraph()->machine()->I32x4Splat(), inputs[0]);
    case wasm::kExprI32x4SConvertF32x4:
      return graph()->NewNode(jsgraph()->machine()->I32x4SConvertF32x4(),
                              inputs[0]);
    case wasm::kExprI32x4UConvertF32x4:
      return graph()->NewNode(jsgraph()->machine()->I32x4UConvertF32x4(),
                              inputs[0]);
    case wasm::kExprI32x4SConvertI16x8Low:
      return graph()->NewNode(jsgraph()->machine()->I32x4SConvertI16x8Low(),
                              inputs[0]);
    case wasm::kExprI32x4SConvertI16x8High:
      return graph()->NewNode(jsgraph()->machine()->I32x4SConvertI16x8High(),
                              inputs[0]);
    case wasm::kExprI32x4Neg:
      return graph()->NewNode(jsgraph()->machine()->I32x4Neg(), inputs[0]);
    case wasm::kExprI32x4Add:
      return graph()->NewNode(jsgraph()->machine()->I32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4AddHoriz:
      return graph()->NewNode(jsgraph()->machine()->I32x4AddHoriz(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Sub:
      return graph()->NewNode(jsgraph()->machine()->I32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Mul:
      return graph()->NewNode(jsgraph()->machine()->I32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MinS:
      return graph()->NewNode(jsgraph()->machine()->I32x4MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MaxS:
      return graph()->NewNode(jsgraph()->machine()->I32x4MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Eq:
      return graph()->NewNode(jsgraph()->machine()->I32x4Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Ne:
      return graph()->NewNode(jsgraph()->machine()->I32x4Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4LtS:
      return graph()->NewNode(jsgraph()->machine()->I32x4GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4LeS:
      return graph()->NewNode(jsgraph()->machine()->I32x4GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4GtS:
      return graph()->NewNode(jsgraph()->machine()->I32x4GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4GeS:
      return graph()->NewNode(jsgraph()->machine()->I32x4GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4UConvertI16x8Low:
      return graph()->NewNode(jsgraph()->machine()->I32x4UConvertI16x8Low(),
                              inputs[0]);
    case wasm::kExprI32x4UConvertI16x8High:
      return graph()->NewNode(jsgraph()->machine()->I32x4UConvertI16x8High(),
                              inputs[0]);
    case wasm::kExprI32x4MinU:
      return graph()->NewNode(jsgraph()->machine()->I32x4MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MaxU:
      return graph()->NewNode(jsgraph()->machine()->I32x4MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4LtU:
      return graph()->NewNode(jsgraph()->machine()->I32x4GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4LeU:
      return graph()->NewNode(jsgraph()->machine()->I32x4GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4GtU:
      return graph()->NewNode(jsgraph()->machine()->I32x4GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4GeU:
      return graph()->NewNode(jsgraph()->machine()->I32x4GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Splat:
      return graph()->NewNode(jsgraph()->machine()->I16x8Splat(), inputs[0]);
    case wasm::kExprI16x8SConvertI8x16Low:
      return graph()->NewNode(jsgraph()->machine()->I16x8SConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8SConvertI8x16High:
      return graph()->NewNode(jsgraph()->machine()->I16x8SConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8Neg:
      return graph()->NewNode(jsgraph()->machine()->I16x8Neg(), inputs[0]);
    case wasm::kExprI16x8SConvertI32x4:
      return graph()->NewNode(jsgraph()->machine()->I16x8SConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Add:
      return graph()->NewNode(jsgraph()->machine()->I16x8Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8AddSaturateS:
      return graph()->NewNode(jsgraph()->machine()->I16x8AddSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8AddHoriz:
      return graph()->NewNode(jsgraph()->machine()->I16x8AddHoriz(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Sub:
      return graph()->NewNode(jsgraph()->machine()->I16x8Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8SubSaturateS:
      return graph()->NewNode(jsgraph()->machine()->I16x8SubSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Mul:
      return graph()->NewNode(jsgraph()->machine()->I16x8Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MinS:
      return graph()->NewNode(jsgraph()->machine()->I16x8MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MaxS:
      return graph()->NewNode(jsgraph()->machine()->I16x8MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Eq:
      return graph()->NewNode(jsgraph()->machine()->I16x8Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Ne:
      return graph()->NewNode(jsgraph()->machine()->I16x8Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8LtS:
      return graph()->NewNode(jsgraph()->machine()->I16x8GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8LeS:
      return graph()->NewNode(jsgraph()->machine()->I16x8GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8GtS:
      return graph()->NewNode(jsgraph()->machine()->I16x8GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8GeS:
      return graph()->NewNode(jsgraph()->machine()->I16x8GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8UConvertI8x16Low:
      return graph()->NewNode(jsgraph()->machine()->I16x8UConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8UConvertI8x16High:
      return graph()->NewNode(jsgraph()->machine()->I16x8UConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8UConvertI32x4:
      return graph()->NewNode(jsgraph()->machine()->I16x8UConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8AddSaturateU:
      return graph()->NewNode(jsgraph()->machine()->I16x8AddSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8SubSaturateU:
      return graph()->NewNode(jsgraph()->machine()->I16x8SubSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8MinU:
      return graph()->NewNode(jsgraph()->machine()->I16x8MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MaxU:
      return graph()->NewNode(jsgraph()->machine()->I16x8MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8LtU:
      return graph()->NewNode(jsgraph()->machine()->I16x8GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8LeU:
      return graph()->NewNode(jsgraph()->machine()->I16x8GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8GtU:
      return graph()->NewNode(jsgraph()->machine()->I16x8GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8GeU:
      return graph()->NewNode(jsgraph()->machine()->I16x8GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Splat:
      return graph()->NewNode(jsgraph()->machine()->I8x16Splat(), inputs[0]);
    case wasm::kExprI8x16Neg:
      return graph()->NewNode(jsgraph()->machine()->I8x16Neg(), inputs[0]);
    case wasm::kExprI8x16SConvertI16x8:
      return graph()->NewNode(jsgraph()->machine()->I8x16SConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Add:
      return graph()->NewNode(jsgraph()->machine()->I8x16Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16AddSaturateS:
      return graph()->NewNode(jsgraph()->machine()->I8x16AddSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Sub:
      return graph()->NewNode(jsgraph()->machine()->I8x16Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SubSaturateS:
      return graph()->NewNode(jsgraph()->machine()->I8x16SubSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Mul:
      return graph()->NewNode(jsgraph()->machine()->I8x16Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MinS:
      return graph()->NewNode(jsgraph()->machine()->I8x16MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MaxS:
      return graph()->NewNode(jsgraph()->machine()->I8x16MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Eq:
      return graph()->NewNode(jsgraph()->machine()->I8x16Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Ne:
      return graph()->NewNode(jsgraph()->machine()->I8x16Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16LtS:
      return graph()->NewNode(jsgraph()->machine()->I8x16GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16LeS:
      return graph()->NewNode(jsgraph()->machine()->I8x16GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16GtS:
      return graph()->NewNode(jsgraph()->machine()->I8x16GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16GeS:
      return graph()->NewNode(jsgraph()->machine()->I8x16GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16UConvertI16x8:
      return graph()->NewNode(jsgraph()->machine()->I8x16UConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16AddSaturateU:
      return graph()->NewNode(jsgraph()->machine()->I8x16AddSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16SubSaturateU:
      return graph()->NewNode(jsgraph()->machine()->I8x16SubSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16MinU:
      return graph()->NewNode(jsgraph()->machine()->I8x16MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MaxU:
      return graph()->NewNode(jsgraph()->machine()->I8x16MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16LtU:
      return graph()->NewNode(jsgraph()->machine()->I8x16GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16LeU:
      return graph()->NewNode(jsgraph()->machine()->I8x16GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16GtU:
      return graph()->NewNode(jsgraph()->machine()->I8x16GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16GeU:
      return graph()->NewNode(jsgraph()->machine()->I8x16GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128And:
      return graph()->NewNode(jsgraph()->machine()->S128And(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Or:
      return graph()->NewNode(jsgraph()->machine()->S128Or(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Xor:
      return graph()->NewNode(jsgraph()->machine()->S128Xor(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Not:
      return graph()->NewNode(jsgraph()->machine()->S128Not(), inputs[0]);
    case wasm::kExprS128Select:
      return graph()->NewNode(jsgraph()->machine()->S128Select(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprS1x4AnyTrue:
      return graph()->NewNode(jsgraph()->machine()->S1x4AnyTrue(), inputs[0]);
    case wasm::kExprS1x4AllTrue:
      return graph()->NewNode(jsgraph()->machine()->S1x4AllTrue(), inputs[0]);
    case wasm::kExprS1x8AnyTrue:
      return graph()->NewNode(jsgraph()->machine()->S1x8AnyTrue(), inputs[0]);
    case wasm::kExprS1x8AllTrue:
      return graph()->NewNode(jsgraph()->machine()->S1x8AllTrue(), inputs[0]);
    case wasm::kExprS1x16AnyTrue:
      return graph()->NewNode(jsgraph()->machine()->S1x16AnyTrue(), inputs[0]);
    case wasm::kExprS1x16AllTrue:
      return graph()->NewNode(jsgraph()->machine()->S1x16AllTrue(), inputs[0]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane,
                                   Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF32x4ExtractLane:
      return graph()->NewNode(jsgraph()->machine()->F32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF32x4ReplaceLane:
      return graph()->NewNode(jsgraph()->machine()->F32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtractLane:
      return graph()->NewNode(jsgraph()->machine()->I32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI32x4ReplaceLane:
      return graph()->NewNode(jsgraph()->machine()->I32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtractLane:
      return graph()->NewNode(jsgraph()->machine()->I16x8ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI16x8ReplaceLane:
      return graph()->NewNode(jsgraph()->machine()->I16x8ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16ExtractLane:
      return graph()->NewNode(jsgraph()->machine()->I8x16ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI8x16ReplaceLane:
      return graph()->NewNode(jsgraph()->machine()->I8x16ReplaceLane(lane),
                              inputs[0], inputs[1]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::SimdShiftOp(wasm::WasmOpcode opcode, uint8_t shift,
                                    Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprI32x4Shl:
      return graph()->NewNode(jsgraph()->machine()->I32x4Shl(shift), inputs[0]);
    case wasm::kExprI32x4ShrS:
      return graph()->NewNode(jsgraph()->machine()->I32x4ShrS(shift),
                              inputs[0]);
    case wasm::kExprI32x4ShrU:
      return graph()->NewNode(jsgraph()->machine()->I32x4ShrU(shift),
                              inputs[0]);
    case wasm::kExprI16x8Shl:
      return graph()->NewNode(jsgraph()->machine()->I16x8Shl(shift), inputs[0]);
    case wasm::kExprI16x8ShrS:
      return graph()->NewNode(jsgraph()->machine()->I16x8ShrS(shift),
                              inputs[0]);
    case wasm::kExprI16x8ShrU:
      return graph()->NewNode(jsgraph()->machine()->I16x8ShrU(shift),
                              inputs[0]);
    case wasm::kExprI8x16Shl:
      return graph()->NewNode(jsgraph()->machine()->I8x16Shl(shift), inputs[0]);
    case wasm::kExprI8x16ShrS:
      return graph()->NewNode(jsgraph()->machine()->I8x16ShrS(shift),
                              inputs[0]);
    case wasm::kExprI8x16ShrU:
      return graph()->NewNode(jsgraph()->machine()->I8x16ShrU(shift),
                              inputs[0]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::Simd8x16ShuffleOp(const uint8_t shuffle[16],
                                          Node* const* inputs) {
  has_simd_ = true;
  return graph()->NewNode(jsgraph()->machine()->S8x16Shuffle(shuffle),
                          inputs[0], inputs[1]);
}

#define ATOMIC_BINOP_LIST(V)              \
  V(I32AtomicAdd, Add, Uint32)            \
  V(I32AtomicSub, Sub, Uint32)            \
  V(I32AtomicAnd, And, Uint32)            \
  V(I32AtomicOr, Or, Uint32)              \
  V(I32AtomicXor, Xor, Uint32)            \
  V(I32AtomicExchange, Exchange, Uint32)  \
  V(I32AtomicAdd8U, Add, Uint8)           \
  V(I32AtomicSub8U, Sub, Uint8)           \
  V(I32AtomicAnd8U, And, Uint8)           \
  V(I32AtomicOr8U, Or, Uint8)             \
  V(I32AtomicXor8U, Xor, Uint8)           \
  V(I32AtomicExchange8U, Exchange, Uint8) \
  V(I32AtomicAdd16U, Add, Uint16)         \
  V(I32AtomicSub16U, Sub, Uint16)         \
  V(I32AtomicAnd16U, And, Uint16)         \
  V(I32AtomicOr16U, Or, Uint16)           \
  V(I32AtomicXor16U, Xor, Uint16)         \
  V(I32AtomicExchange16U, Exchange, Uint16)

#define ATOMIC_TERNARY_LIST(V)                          \
  V(I32AtomicCompareExchange, CompareExchange, Uint32)  \
  V(I32AtomicCompareExchange8U, CompareExchange, Uint8) \
  V(I32AtomicCompareExchange16U, CompareExchange, Uint16)

#define ATOMIC_LOAD_LIST(V) \
  V(I32AtomicLoad, Uint32)  \
  V(I32AtomicLoad8U, Uint8) \
  V(I32AtomicLoad16U, Uint16)

#define ATOMIC_STORE_LIST(V)         \
  V(I32AtomicStore, Uint32, kWord32) \
  V(I32AtomicStore8U, Uint8, kWord8) \
  V(I32AtomicStore16U, Uint16, kWord16)

Node* WasmGraphBuilder::AtomicOp(wasm::WasmOpcode opcode, Node* const* inputs,
                                 uint32_t alignment, uint32_t offset,
                                 wasm::WasmCodePosition position) {
  // TODO(gdeepti): Add alignment validation, traps on misalignment
  Node* node;
  switch (opcode) {
#define BUILD_ATOMIC_BINOP(Name, Operation, Type)                       \
  case wasm::kExpr##Name: {                                             \
    Node* index =                                                       \
        BoundsCheckMem(wasm::WasmOpcodes::MemSize(MachineType::Type()), \
                       inputs[0], offset, position, kNeedsBoundsCheck); \
    node = graph()->NewNode(                                            \
        jsgraph()->machine()->Atomic##Operation(MachineType::Type()),   \
        MemBuffer(offset), index, inputs[1], *effect_, *control_);      \
    break;                                                              \
  }
    ATOMIC_BINOP_LIST(BUILD_ATOMIC_BINOP)
#undef BUILD_ATOMIC_BINOP

#define BUILD_ATOMIC_TERNARY_OP(Name, Operation, Type)                        \
  case wasm::kExpr##Name: {                                                   \
    Node* index =                                                             \
        BoundsCheckMem(wasm::WasmOpcodes::MemSize(MachineType::Type()),       \
                       inputs[0], offset, position, kNeedsBoundsCheck);       \
    node = graph()->NewNode(                                                  \
        jsgraph()->machine()->Atomic##Operation(MachineType::Type()),         \
        MemBuffer(offset), index, inputs[1], inputs[2], *effect_, *control_); \
    break;                                                                    \
  }
    ATOMIC_TERNARY_LIST(BUILD_ATOMIC_TERNARY_OP)
#undef BUILD_ATOMIC_TERNARY_OP

#define BUILD_ATOMIC_LOAD_OP(Name, Type)                                \
  case wasm::kExpr##Name: {                                             \
    Node* index =                                                       \
        BoundsCheckMem(wasm::WasmOpcodes::MemSize(MachineType::Type()), \
                       inputs[0], offset, position, kNeedsBoundsCheck); \
    node = graph()->NewNode(                                            \
        jsgraph()->machine()->AtomicLoad(MachineType::Type()),          \
        MemBuffer(offset), index, *effect_, *control_);                 \
    break;                                                              \
  }
    ATOMIC_LOAD_LIST(BUILD_ATOMIC_LOAD_OP)
#undef BUILD_ATOMIC_LOAD_OP

#define BUILD_ATOMIC_STORE_OP(Name, Type, Rep)                          \
  case wasm::kExpr##Name: {                                             \
    Node* index =                                                       \
        BoundsCheckMem(wasm::WasmOpcodes::MemSize(MachineType::Type()), \
                       inputs[0], offset, position, kNeedsBoundsCheck); \
    node = graph()->NewNode(                                            \
        jsgraph()->machine()->AtomicStore(MachineRepresentation::Rep),  \
        MemBuffer(offset), index, inputs[1], *effect_, *control_);      \
    break;                                                              \
  }
    ATOMIC_STORE_LIST(BUILD_ATOMIC_STORE_OP)
#undef BUILD_ATOMIC_STORE_OP
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
  *effect_ = node;
  return node;
}

#undef ATOMIC_BINOP_LIST
#undef ATOMIC_TERNARY_LIST
#undef ATOMIC_LOAD_LIST
#undef ATOMIC_STORE_LIST

namespace {
bool must_record_function_compilation(Isolate* isolate) {
  return isolate->logger()->is_logging_code_events() || isolate->is_profiling();
}

PRINTF_FORMAT(4, 5)
void RecordFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                               Isolate* isolate, Handle<Code> code,
                               const char* format, ...) {
  DCHECK(must_record_function_compilation(isolate));

  ScopedVector<char> buffer(128);
  va_list arguments;
  va_start(arguments, format);
  int len = VSNPrintF(buffer, format, arguments);
  CHECK_LT(0, len);
  va_end(arguments);
  Handle<String> name_str =
      isolate->factory()->NewStringFromAsciiChecked(buffer.start());
  Handle<String> script_str =
      isolate->factory()->NewStringFromAsciiChecked("(wasm)");
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfo(name_str, code, false);
  PROFILE(isolate, CodeCreateEvent(tag, AbstractCode::cast(*code), *shared,
                                   *script_str, 0, 0));
}
}  // namespace

Handle<Code> CompileJSToWasmWrapper(Isolate* isolate, wasm::WasmModule* module,
                                    WasmCodeWrapper wasm_code, uint32_t index,
                                    Address wasm_context_address,
                                    bool use_trap_handler) {
  const wasm::WasmFunction* func = &module->functions[index];

  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  // TODO(titzer): compile JS to WASM wrappers without a {ModuleEnv}.
  ModuleEnv env(module,
                // TODO(mtrofin): remove the Illegal builtin when we don't need
                // FLAG_wasm_jit_to_native
                BUILTIN_CODE(isolate, Illegal),  // default_function_code
                use_trap_handler);

  WasmGraphBuilder builder(&env, &zone, &jsgraph,
                           CEntryStub(isolate, 1).GetCode(), func->sig);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildJSToWasmWrapper(wasm_code, wasm_context_address);

  //----------------------------------------------------------------------------
  // Run the compilation pipeline.
  //----------------------------------------------------------------------------
  if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
    OFStream os(stdout);
    os << "-- Graph after change lowering -- " << std::endl;
    os << AsRPO(graph);
  }

  // Schedule and compile to machine code.
  int params =
      static_cast<int>(module->functions[index].sig->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      &zone, false, params + 1, CallDescriptor::kNoFlags);

#ifdef DEBUG
  EmbeddedVector<char, 32> func_name;
  static unsigned id = 0;
  func_name.Truncate(SNPrintF(func_name, "js-to-wasm#%d", id++));
#else
  Vector<const char> func_name = CStrVector("js-to-wasm");
#endif

  CompilationInfo info(func_name, &zone, Code::JS_TO_WASM_FUNCTION);
  Handle<Code> code =
      Pipeline::GenerateCodeForTesting(&info, isolate, incoming, &graph);
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code && !code.is_null()) {
    OFStream os(stdout);
    code->Disassemble(func_name.start(), os);
  }
#endif

  if (must_record_function_compilation(isolate)) {
    RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate, code,
                              "%.*s", func_name.length(), func_name.start());
  }

  return code;
}

namespace {

void ValidateImportWrapperReferencesImmovables(Handle<Code> wrapper) {
#ifdef DEBUG
  // We expect the only embedded objects to be those originating from
  // a snapshot, which are immovable.
  DisallowHeapAllocation no_gc;
  if (wrapper.is_null()) return;
  static constexpr int kAllGCRefs = (1 << (RelocInfo::LAST_GCED_ENUM + 1)) - 1;
  for (RelocIterator it(*wrapper, kAllGCRefs); !it.done(); it.next()) {
    RelocInfo::Mode mode = it.rinfo()->rmode();
    Object* target = nullptr;
    switch (mode) {
      case RelocInfo::CODE_TARGET:
        // this would be either one of the stubs or builtins, because
        // we didn't link yet.
        target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
        break;
      case RelocInfo::EMBEDDED_OBJECT:
        target = it.rinfo()->target_object();
        break;
      default:
        UNREACHABLE();
    }
    DCHECK_NOT_NULL(target);
    bool is_immovable =
        target->IsSmi() || Heap::IsImmovable(HeapObject::cast(target));
    bool is_allowed_stub = false;
    if (target->IsCode()) {
      Code* code = Code::cast(target);
      is_allowed_stub =
          code->kind() == Code::STUB &&
          CodeStub::MajorKeyFromKey(code->stub_key()) == CodeStub::DoubleToI;
    }
    DCHECK(is_immovable || is_allowed_stub);
  }
#endif
}

}  // namespace

Handle<Code> CompileWasmToJSWrapper(
    Isolate* isolate, Handle<JSReceiver> target, wasm::FunctionSig* sig,
    uint32_t index, wasm::ModuleOrigin origin, bool use_trap_handler,
    Handle<FixedArray> global_js_imports_table) {
  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  SourcePositionTable* source_position_table =
      origin == wasm::kAsmJsOrigin ? new (&zone) SourcePositionTable(&graph)
                                   : nullptr;

  ModuleEnv env(nullptr, Handle<Code>::null(), use_trap_handler);
  WasmGraphBuilder builder(&env, &zone, &jsgraph,
                           CEntryStub(isolate, 1).GetCode(), sig,
                           source_position_table);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  if (builder.BuildWasmToJSWrapper(target, global_js_imports_table, index)) {
    global_js_imports_table->set(
        OffsetForImportData(index, WasmGraphBuilder::kFunction), *target);
    if (target->IsJSFunction()) {
      Handle<JSFunction> function = Handle<JSFunction>::cast(target);
      global_js_imports_table->set(
          OffsetForImportData(index, WasmGraphBuilder::kFunctionContext),
          function->context());
      global_js_imports_table->set(
          OffsetForImportData(index, WasmGraphBuilder::kGlobalProxy),
          function->context()->global_proxy());
    }
  }

    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      OFStream os(stdout);
      os << "-- Graph after change lowering -- " << std::endl;
      os << AsRPO(graph);
    }

    // Schedule and compile to machine code.
    CallDescriptor* incoming = GetWasmCallDescriptor(&zone, sig);
    if (machine.Is32()) {
      incoming = GetI32WasmCallDescriptor(&zone, incoming);
    }

#ifdef DEBUG
    EmbeddedVector<char, 32> func_name;
    static unsigned id = 0;
    func_name.Truncate(SNPrintF(func_name, "wasm-to-js#%d", id++));
#else
    Vector<const char> func_name = CStrVector("wasm-to-js");
#endif

    CompilationInfo info(func_name, &zone, Code::WASM_TO_JS_FUNCTION);
    Handle<Code> code = Pipeline::GenerateCodeForTesting(
        &info, isolate, incoming, &graph, nullptr, source_position_table);
    ValidateImportWrapperReferencesImmovables(code);
    Handle<FixedArray> deopt_data =
        isolate->factory()->NewFixedArray(2, TENURED);
    intptr_t loc =
        reinterpret_cast<intptr_t>(global_js_imports_table.location());
    Handle<Object> loc_handle = isolate->factory()->NewHeapNumberFromBits(loc);
    deopt_data->set(0, *loc_handle);
    Handle<Object> index_handle = isolate->factory()->NewNumberFromInt(
        OffsetForImportData(index, WasmGraphBuilder::kFunction));
    deopt_data->set(1, *index_handle);
    code->set_deoptimization_data(*deopt_data);
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code && !code.is_null()) {
      OFStream os(stdout);
      code->Disassemble(func_name.start(), os);
    }
#endif

  if (must_record_function_compilation(isolate)) {
    RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate, code,
                              "%.*s", func_name.length(), func_name.start());
  }

  return code;
}

Handle<Code> CompileWasmToWasmWrapper(Isolate* isolate, WasmCodeWrapper target,
                                      wasm::FunctionSig* sig,
                                      Address new_wasm_context_address) {
  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  ModuleEnv env(
      nullptr, Handle<Code>::null(),
      !target.IsCodeObject() && target.GetWasmCode()->HasTrapHandlerIndex());
  WasmGraphBuilder builder(&env, &zone, &jsgraph, Handle<Code>(), sig);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildWasmToWasmWrapper(target, new_wasm_context_address);
  if (HasInt64ParamOrReturn(sig)) builder.LowerInt64();

  if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
    OFStream os(stdout);
    os << "-- Graph after change lowering -- " << std::endl;
    os << AsRPO(graph);
  }

  // Schedule and compile to machine code.
  CallDescriptor* incoming = GetWasmCallDescriptor(&zone, sig);
  if (machine.Is32()) {
    incoming = GetI32WasmCallDescriptor(&zone, incoming);
  }
  bool debugging =
#if DEBUG
      true;
#else
      FLAG_print_opt_code || FLAG_trace_turbo || FLAG_trace_turbo_graph;
#endif
  Vector<const char> func_name = ArrayVector("wasm-to-wasm");
  static unsigned id = 0;
  Vector<char> buffer;
  if (debugging) {
    buffer = Vector<char>::New(128);
    int chars = SNPrintF(buffer, "wasm-to-wasm#%d", id);
    func_name = Vector<const char>::cast(buffer.SubVector(0, chars));
  }

  CompilationInfo info(func_name, &zone, Code::WASM_TO_WASM_FUNCTION);
  Handle<Code> code =
      Pipeline::GenerateCodeForTesting(&info, isolate, incoming, &graph);
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code && !code.is_null()) {
    OFStream os(stdout);
    code->Disassemble(buffer.start(), os);
  }
#endif
  if (debugging) {
    buffer.Dispose();
  }
  if (isolate->logger()->is_logging_code_events() || isolate->is_profiling()) {
    RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate, code,
                              "wasm-to-wasm");
  }

  return code;
}

Handle<Code> CompileWasmInterpreterEntry(Isolate* isolate, uint32_t func_index,
                                         wasm::FunctionSig* sig,
                                         Handle<WasmInstanceObject> instance) {
  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  WasmGraphBuilder builder(nullptr, &zone, &jsgraph,
                           CEntryStub(isolate, 1).GetCode(), sig);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildWasmInterpreterEntry(func_index);

  Handle<Code> code = Handle<Code>::null();
  {
    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      OFStream os(stdout);
      os << "-- Wasm interpreter entry graph -- " << std::endl;
      os << AsRPO(graph);
    }

    // Schedule and compile to machine code.
    CallDescriptor* incoming = GetWasmCallDescriptor(&zone, sig);
    if (machine.Is32()) {
      incoming = GetI32WasmCallDescriptor(&zone, incoming);
    }
#ifdef DEBUG
    EmbeddedVector<char, 32> func_name;
    func_name.Truncate(
        SNPrintF(func_name, "wasm-interpreter-entry#%d", func_index));
#else
    Vector<const char> func_name = CStrVector("wasm-interpreter-entry");
#endif

    CompilationInfo info(func_name, &zone, Code::WASM_INTERPRETER_ENTRY);
    code = Pipeline::GenerateCodeForTesting(&info, isolate, incoming, &graph,
                                            nullptr);
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code && !code.is_null()) {
      OFStream os(stdout);
      code->Disassemble(func_name.start(), os);
    }
#endif

    if (must_record_function_compilation(isolate)) {
      RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate, code,
                                "%.*s", func_name.length(), func_name.start());
    }
  }

  if (!FLAG_wasm_jit_to_native) {
    Handle<FixedArray> deopt_data =
        isolate->factory()->NewFixedArray(1, TENURED);
    Handle<WeakCell> weak_instance = isolate->factory()->NewWeakCell(instance);
    deopt_data->set(0, *weak_instance);
    code->set_deoptimization_data(*deopt_data);
  }
  return code;
}

Handle<Code> CompileCWasmEntry(Isolate* isolate, wasm::FunctionSig* sig,
                               Address wasm_context_address) {
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  WasmGraphBuilder builder(nullptr, &zone, &jsgraph,
                           CEntryStub(isolate, 1).GetCode(), sig);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildCWasmEntry(wasm_context_address);

  if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
    OFStream os(stdout);
    os << "-- C Wasm entry graph -- " << std::endl;
    os << AsRPO(graph);
  }

  // Schedule and compile to machine code.
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      &zone, false, CWasmEntryParameters::kNumParameters + 1,
      CallDescriptor::kNoFlags);

  // Build a name in the form "c-wasm-entry:<params>:<returns>".
  static constexpr size_t kMaxNameLen = 128;
  char debug_name[kMaxNameLen] = "c-wasm-entry:";
  size_t name_len = strlen(debug_name);
  auto append_name_char = [&](char c) {
    if (name_len + 1 < kMaxNameLen) debug_name[name_len++] = c;
  };
  for (wasm::ValueType t : sig->parameters()) {
    append_name_char(wasm::WasmOpcodes::ShortNameOf(t));
  }
  append_name_char(':');
  for (wasm::ValueType t : sig->returns()) {
    append_name_char(wasm::WasmOpcodes::ShortNameOf(t));
  }
  debug_name[name_len] = '\0';
  Vector<const char> debug_name_vec(debug_name, name_len);

  CompilationInfo info(debug_name_vec, &zone, Code::C_WASM_ENTRY);
  Handle<Code> code =
      Pipeline::GenerateCodeForTesting(&info, isolate, incoming, &graph);
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code && !code.is_null()) {
    OFStream os(stdout);
    code->Disassemble(debug_name, os);
  }
#endif

  return code;
}

SourcePositionTable* WasmCompilationUnit::BuildGraphForWasmFunction(
    double* decode_ms) {

  base::ElapsedTimer decode_timer;
  if (FLAG_trace_wasm_decode_time) {
    decode_timer.Start();
  }
  // Create a TF graph during decoding.

  SourcePositionTable* source_position_table =
      new (tf_.jsgraph_->zone()) SourcePositionTable(tf_.jsgraph_->graph());
  WasmGraphBuilder builder(env_, tf_.jsgraph_->zone(), tf_.jsgraph_,
                           centry_stub_, func_body_.sig, source_position_table,
                           runtime_exception_support_);
  tf_.graph_construction_result_ =
      wasm::BuildTFGraph(isolate_->allocator(), &builder, func_body_);
  if (tf_.graph_construction_result_.failed()) {
    if (FLAG_trace_wasm_compiler) {
      OFStream os(stdout);
      os << "Compilation failed: " << tf_.graph_construction_result_.error_msg()
         << std::endl;
    }
    return nullptr;
  }

  builder.LowerInt64();

  if (builder.has_simd() &&
      (!CpuFeatures::SupportsWasmSimd128() || lower_simd_)) {
    SimdScalarLowering(tf_.jsgraph_, func_body_.sig).LowerGraph();
  }

  if (func_index_ >= FLAG_trace_wasm_ast_start &&
      func_index_ < FLAG_trace_wasm_ast_end) {
    PrintRawWasmCode(isolate_->allocator(), func_body_, env_->module,
                     wasm::kPrintLocals);
  }
  if (FLAG_trace_wasm_decode_time) {
    *decode_ms = decode_timer.Elapsed().InMillisecondsF();
  }
  return source_position_table;
}

namespace {
Vector<const char> GetDebugName(Zone* zone, wasm::WasmName name, int index) {
  if (!name.is_empty()) {
    return name;
  }
#ifdef DEBUG
  constexpr int kBufferLength = 15;

  EmbeddedVector<char, kBufferLength> name_vector;
  int name_len = SNPrintF(name_vector, "wasm#%d", index);
  DCHECK(name_len > 0 && name_len < name_vector.length());

  char* index_name = zone->NewArray<char>(name_len);
  memcpy(index_name, name_vector.start(), name_len);
  return Vector<const char>(index_name, name_len);
#else
  return {};
#endif
}

}  // namespace

// static
WasmCompilationUnit::CompilationMode
WasmCompilationUnit::GetDefaultCompilationMode() {
  return FLAG_liftoff ? WasmCompilationUnit::CompilationMode::kLiftoff
                      : WasmCompilationUnit::CompilationMode::kTurbofan;
}

WasmCompilationUnit::WasmCompilationUnit(
    Isolate* isolate, ModuleEnv* env, wasm::NativeModule* native_module,
    wasm::FunctionBody body, wasm::WasmName name, int index,
    Handle<Code> centry_stub, CompilationMode mode, Counters* counters,
    RuntimeExceptionSupport exception_support, bool lower_simd)
    : isolate_(isolate),
      env_(env),
      func_body_(body),
      func_name_(name),
      counters_(counters ? counters : isolate->counters()),
      centry_stub_(centry_stub),
      func_index_(index),
      runtime_exception_support_(exception_support),
      native_module_(native_module),
      lower_simd_(lower_simd),
      protected_instructions_(
          new std::vector<trap_handler::ProtectedInstructionData>()),
      mode_(mode) {
  switch (mode_) {
    case WasmCompilationUnit::CompilationMode::kLiftoff:
      new (&liftoff_) LiftoffData(isolate);
      break;
    case WasmCompilationUnit::CompilationMode::kTurbofan:
      new (&tf_) TurbofanData();
      break;
  }
}

WasmCompilationUnit::~WasmCompilationUnit() {
  switch (mode_) {
    case WasmCompilationUnit::CompilationMode::kLiftoff:
      liftoff_.~LiftoffData();
      break;
    case WasmCompilationUnit::CompilationMode::kTurbofan:
      tf_.~TurbofanData();
      break;
  }
}

void WasmCompilationUnit::ExecuteCompilation() {
  auto size_histogram = env_->module->is_wasm()
                            ? counters()->wasm_wasm_function_size_bytes()
                            : counters()->wasm_asm_function_size_bytes();
  size_histogram->AddSample(
      static_cast<int>(func_body_.end - func_body_.start));
  auto timed_histogram = env_->module->is_wasm()
                             ? counters()->wasm_compile_wasm_function_time()
                             : counters()->wasm_compile_asm_function_time();
  TimedHistogramScope wasm_compile_function_time_scope(timed_histogram);

  if (FLAG_trace_wasm_compiler) {
    if (func_name_.start() != nullptr) {
      PrintF("Compiling wasm function %d:'%.*s'\n\n", func_index(),
             func_name_.length(), func_name_.start());
    } else {
      PrintF("Compiling wasm function %d:<unnamed>\n\n", func_index());
    }
  }

  switch (mode_) {
    case WasmCompilationUnit::CompilationMode::kLiftoff:
      if (ExecuteLiftoffCompilation()) break;
      // Otherwise, fall back to turbofan.
      liftoff_.~LiftoffData();
      mode_ = WasmCompilationUnit::CompilationMode::kTurbofan;
      new (&tf_) TurbofanData();
    // fall-through
    case WasmCompilationUnit::CompilationMode::kTurbofan:
      ExecuteTurbofanCompilation();
      break;
  }
}

void WasmCompilationUnit::ExecuteTurbofanCompilation() {
  double decode_ms = 0;
  size_t node_count = 0;

  // Scope for the {graph_zone}.
  {
    Zone graph_zone(isolate_->allocator(), ZONE_NAME);
    tf_.jsgraph_ = new (&graph_zone) JSGraph(
        isolate_, new (&graph_zone) Graph(&graph_zone),
        new (&graph_zone) CommonOperatorBuilder(&graph_zone), nullptr, nullptr,
        new (&graph_zone) MachineOperatorBuilder(
            &graph_zone, MachineType::PointerRepresentation(),
            InstructionSelector::SupportedMachineOperatorFlags(),
            InstructionSelector::AlignmentRequirements()));
    SourcePositionTable* source_positions =
        BuildGraphForWasmFunction(&decode_ms);

    if (tf_.graph_construction_result_.failed()) {
      ok_ = false;
      return;
    }

    base::ElapsedTimer pipeline_timer;
    if (FLAG_trace_wasm_decode_time) {
      node_count = tf_.jsgraph_->graph()->NodeCount();
      pipeline_timer.Start();
    }

    tf_.compilation_zone_.reset(new Zone(isolate_->allocator(), ZONE_NAME));

    // Run the compiler pipeline to generate machine code.
    CallDescriptor* descriptor =
        GetWasmCallDescriptor(tf_.compilation_zone_.get(), func_body_.sig);
    if (tf_.jsgraph_->machine()->Is32()) {
      descriptor =
          GetI32WasmCallDescriptor(tf_.compilation_zone_.get(), descriptor);
    }
    tf_.info_.reset(new CompilationInfo(
        GetDebugName(tf_.compilation_zone_.get(), func_name_, func_index_),
        tf_.compilation_zone_.get(), Code::WASM_FUNCTION));

    tf_.job_.reset(Pipeline::NewWasmCompilationJob(
        tf_.info_.get(), isolate_, tf_.jsgraph_, descriptor, source_positions,
        protected_instructions_.get(), env_->module->origin()));
    ok_ = tf_.job_->ExecuteJob() == CompilationJob::SUCCEEDED;
    // TODO(bradnelson): Improve histogram handling of size_t.
    counters()->wasm_compile_function_peak_memory_bytes()->AddSample(
        static_cast<int>(tf_.jsgraph_->graph()->zone()->allocation_size()));

    if (FLAG_trace_wasm_decode_time) {
      double pipeline_ms = pipeline_timer.Elapsed().InMillisecondsF();
      PrintF(
          "wasm-compilation phase 1 ok: %u bytes, %0.3f ms decode, %zu nodes, "
          "%0.3f ms pipeline\n",
          static_cast<unsigned>(func_body_.end - func_body_.start), decode_ms,
          node_count, pipeline_ms);
    }
    // The graph zone is about to get out of scope. Avoid invalid references.
    tf_.jsgraph_ = nullptr;
  }

  // Record the memory cost this unit places on the system until
  // it is finalized.
  memory_cost_ = tf_.job_->AllocatedMemory();
}

// WasmCompilationUnit::ExecuteLiftoffCompilation() is defined in
// liftoff-compiler.cc.

WasmCodeWrapper WasmCompilationUnit::FinishCompilation(
    wasm::ErrorThrower* thrower) {
  WasmCodeWrapper ret;
  switch (mode_) {
    case WasmCompilationUnit::CompilationMode::kLiftoff:
      ret = FinishLiftoffCompilation(thrower);
      break;
    case WasmCompilationUnit::CompilationMode::kTurbofan:
      ret = FinishTurbofanCompilation(thrower);
      break;
    default:
      UNREACHABLE();
  }
  if (!ret.IsCodeObject() && ret.is_null()) {
    thrower->RuntimeError("Error finalizing code.");
  }
  return ret;
}

WasmCodeWrapper WasmCompilationUnit::FinishTurbofanCompilation(
    wasm::ErrorThrower* thrower) {
  if (!ok_) {
    if (tf_.graph_construction_result_.failed()) {
      // Add the function as another context for the exception.
      EmbeddedVector<char, 128> message;
      if (func_name_.start() == nullptr) {
        SNPrintF(message, "Compiling wasm function #%d failed", func_index_);
      } else {
        wasm::TruncatedUserString<> trunc_name(func_name_);
        SNPrintF(message, "Compiling wasm function #%d:%.*s failed",
                 func_index_, trunc_name.length(), trunc_name.start());
      }
      thrower->CompileFailed(message.start(), tf_.graph_construction_result_);
    }

    return {};
  }
  base::ElapsedTimer codegen_timer;
  if (FLAG_trace_wasm_decode_time) {
    codegen_timer.Start();
  }

  if (tf_.job_->FinalizeJob(isolate_) != CompilationJob::SUCCEEDED) {
    return {};
  }
  if (!FLAG_wasm_jit_to_native) {
    Handle<Code> code = tf_.info_->code();
    DCHECK(!code.is_null());

    if (FLAG_trace_wasm_decode_time) {
      double codegen_ms = codegen_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-code-generation ok: %u bytes, %0.3f ms code generation\n",
             static_cast<unsigned>(func_body_.end - func_body_.start),
             codegen_ms);
    }

    PackProtectedInstructions(code);
    return WasmCodeWrapper(code);
  } else {
    // TODO(mtrofin): when we crystalize a design in lieu of WasmCodeDesc, that
    // works for both wasm and non-wasm, we can simplify AddCode to just take
    // that as a parameter.
    const CodeDesc& desc =
        tf_.job_->compilation_info()->wasm_code_desc()->code_desc;
    wasm::WasmCode* code = native_module_->AddCode(
        desc, tf_.job_->compilation_info()->wasm_code_desc()->frame_slot_count,
        func_index_,
        tf_.job_->compilation_info()->wasm_code_desc()->safepoint_table_offset,
        std::move(protected_instructions_));
    if (!code) {
      return WasmCodeWrapper(code);
    }
    // TODO(mtrofin): add CodeEventListener call - see the non-native case.
    if (FLAG_trace_wasm_decode_time) {
      double codegen_ms = codegen_timer.Elapsed().InMillisecondsF();
      PrintF("wasm-code-generation ok: %u bytes, %0.3f ms code generation\n",
             static_cast<unsigned>(func_body_.end - func_body_.start),
             codegen_ms);
    }

    Handle<ByteArray> source_positions =
        tf_.job_->compilation_info()->wasm_code_desc()->source_positions_table;
    MaybeHandle<HandlerTable> handler_table =
        tf_.job_->compilation_info()->wasm_code_desc()->handler_table;

    native_module_->compiled_module()->source_positions()->set(
        func_index_, *source_positions);
    if (!handler_table.is_null()) {
      native_module_->compiled_module()->handler_table()->set(
          func_index_, *handler_table.ToHandleChecked());
    }

#ifdef ENABLE_DISASSEMBLER
    // Note: only do this after setting source positions, as this will be
    // accessed and printed here.
    if (FLAG_print_code || FLAG_print_wasm_code) {
      // TODO(wasm): Use proper log files, here and elsewhere.
      PrintF("--- Native Wasm code ---\n");
      code->Print(isolate_);
      PrintF("--- End code ---\n");
    }
#endif

    // TODO(mtrofin): this should probably move up in the common caller,
    // once liftoff has source positions. Until then, we'd need to handle
    // undefined values, which is complicating the code.
    LOG_CODE_EVENT(isolate_,
                   CodeLinePosInfoRecordEvent(code->instructions().start(),
                                              *source_positions));
    return WasmCodeWrapper(code);
  }
}

// TODO(mtrofin): remove when FLAG_wasm_jit_to_native is not needed
void WasmCompilationUnit::PackProtectedInstructions(Handle<Code> code) const {
  if (protected_instructions_->empty()) return;
  DCHECK_LT(protected_instructions_->size(), std::numeric_limits<int>::max());
  const int num_instructions =
      static_cast<int>(protected_instructions_->size());
  Handle<FixedArray> fn_protected = isolate_->factory()->NewFixedArray(
      num_instructions * Code::kTrapDataSize, TENURED);
  for (int i = 0; i < num_instructions; ++i) {
    const trap_handler::ProtectedInstructionData& instruction =
        protected_instructions_->at(i);
    fn_protected->set(Code::kTrapDataSize * i + Code::kTrapCodeOffset,
                      Smi::FromInt(instruction.instr_offset));
    fn_protected->set(Code::kTrapDataSize * i + Code::kTrapLandingOffset,
                      Smi::FromInt(instruction.landing_offset));
  }
  code->set_protected_instructions(*fn_protected);
}

WasmCodeWrapper WasmCompilationUnit::FinishLiftoffCompilation(
    wasm::ErrorThrower* thrower) {
  CodeDesc desc;
  liftoff_.asm_.GetCode(isolate_, &desc);

  Handle<ByteArray> source_positions =
      liftoff_.source_position_table_builder_.ToSourcePositionTable(isolate_);

  WasmCodeWrapper ret;
  if (!FLAG_wasm_jit_to_native) {
    Handle<Code> code;
    code = isolate_->factory()->NewCode(
        desc, Code::WASM_FUNCTION, code, Builtins::kNoBuiltinId,
        MaybeHandle<HandlerTable>(), source_positions,
        MaybeHandle<DeoptimizationData>(), kMovable,
        0,                                       // stub_key
        false,                                   // is_turbofanned
        liftoff_.asm_.GetTotalFrameSlotCount(),  // stack_slots
        liftoff_.safepoint_table_offset_);
    if (isolate_->logger()->is_logging_code_events() ||
        isolate_->is_profiling()) {
      RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate_, code,
                                "wasm#%d-liftoff", func_index_);
    }

    PackProtectedInstructions(code);
    ret = WasmCodeWrapper(code);
  } else {
    // TODO(mtrofin): figure a way to raise events.
    // Consider lifting it to FinishCompilation.
    native_module_->compiled_module()->source_positions()->set(
        func_index_, *source_positions);
    ret = WasmCodeWrapper(
        native_module_->AddCode(desc, liftoff_.asm_.GetTotalFrameSlotCount(),
                                func_index_, liftoff_.safepoint_table_offset_,
                                std::move(protected_instructions_), true));
  }
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code || FLAG_print_wasm_code) {
    // TODO(wasm): Use proper log files, here and elsewhere.
    OFStream os(stdout);
    os << "--- Wasm liftoff code ---\n";
    EmbeddedVector<char, 64> func_name;
    if (func_name_.start() != nullptr) {
      SNPrintF(func_name, "#%d:%.*s", func_index(), func_name_.length(),
               func_name_.start());
    } else {
      SNPrintF(func_name, "wasm#%d", func_index());
    }
    ret.Disassemble(func_name.start(), isolate_, os);
    os << "--- End code ---\n";
  }
#endif
  return ret;
}

// static
WasmCodeWrapper WasmCompilationUnit::CompileWasmFunction(
    wasm::NativeModule* native_module, wasm::ErrorThrower* thrower,
    Isolate* isolate, const wasm::ModuleWireBytes& wire_bytes, ModuleEnv* env,
    const wasm::WasmFunction* function, CompilationMode mode) {
  wasm::FunctionBody function_body{
      function->sig, function->code.offset(),
      wire_bytes.start() + function->code.offset(),
      wire_bytes.start() + function->code.end_offset()};

  WasmCompilationUnit unit(isolate, env, native_module, function_body,
                           wire_bytes.GetNameOrNull(function),
                           function->func_index,
                           CEntryStub(isolate, 1).GetCode(), mode);
  unit.ExecuteCompilation();
  return unit.FinishCompilation(thrower);
}

#undef WASM_64
#undef FATAL_UNSUPPORTED_OPCODE

}  // namespace compiler
}  // namespace internal
}  // namespace v8
