// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include <memory>

#include "src/assembler-inl.h"
#include "src/assembler.h"
#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"
#include "src/base/v8-fallthrough.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/compiler.h"
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
#include "src/compiler/node-origin-table.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/simd-scalar-lowering.h"
#include "src/compiler/zone-stats.h"
#include "src/heap/factory.h"
#include "src/isolate-inl.h"
#include "src/log-inl.h"
#include "src/optimized-compilation-info.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-text.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

// TODO(titzer): pull WASM_64 up to a common header.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif

#define FATAL_UNSUPPORTED_OPCODE(opcode)        \
  FATAL("Unsupported opcode 0x%x:%s", (opcode), \
        wasm::WasmOpcodes::OpcodeName(opcode));

MachineType assert_size(int expected_size, MachineType type) {
  DCHECK_EQ(expected_size, ElementSizeInBytes(type.representation()));
  return type;
}

#define WASM_INSTANCE_OBJECT_SIZE(name)     \
  (WasmInstanceObject::k##name##OffsetEnd - \
   WasmInstanceObject::k##name##Offset + 1)  // NOLINT(whitespace/indent)

#define WASM_INSTANCE_OBJECT_OFFSET(name) \
  wasm::ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset)

#define LOAD_INSTANCE_FIELD(name, type)                                      \
  SetEffect(graph()->NewNode(                                                \
      mcgraph()->machine()->Load(                                            \
          assert_size(WASM_INSTANCE_OBJECT_SIZE(name), type)),               \
      instance_node_.get(),                                                  \
      mcgraph()->Int32Constant(WASM_INSTANCE_OBJECT_OFFSET(name)), Effect(), \
      Control()))

#define LOAD_TAGGED_POINTER(base_pointer, byte_offset)                        \
  SetEffect(graph()->NewNode(                                                 \
      mcgraph()->machine()->Load(MachineType::TaggedPointer()), base_pointer, \
      mcgraph()->Int32Constant(byte_offset), Effect(), Control()))

#define LOAD_FIXED_ARRAY_SLOT(array_node, index) \
  LOAD_TAGGED_POINTER(                           \
      array_node, wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(index))

constexpr uint32_t kBytesPerExceptionValuesArrayElement = 2;

void MergeControlToEnd(MachineGraph* mcgraph, Node* node) {
  Graph* g = mcgraph->graph();
  if (g->end()) {
    NodeProperties::MergeControlToEnd(g, mcgraph->common(), node);
  } else {
    g->SetEnd(g->NewNode(mcgraph->common()->End(1), node));
  }
}

bool ContainsSimd(wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmS128) return true;
  }
  return false;
}

bool ContainsInt64(wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64) return true;
  }
  return false;
}
}  // namespace

WasmGraphBuilder::WasmGraphBuilder(
    wasm::ModuleEnv* env, Zone* zone, MachineGraph* mcgraph,
    wasm::FunctionSig* sig,
    compiler::SourcePositionTable* source_position_table)
    : zone_(zone),
      mcgraph_(mcgraph),
      env_(env),
      cur_buffer_(def_buffer_),
      cur_bufsize_(kDefaultBufferSize),
      has_simd_(ContainsSimd(sig)),
      untrusted_code_mitigations_(FLAG_untrusted_code_mitigations),
      sig_(sig),
      source_position_table_(source_position_table) {
  DCHECK_IMPLIES(use_trap_handler(), trap_handler::IsTrapHandlerEnabled());
  DCHECK_NOT_NULL(mcgraph_);
}

Node* WasmGraphBuilder::Error() { return mcgraph()->Dead(); }

Node* WasmGraphBuilder::Start(unsigned params) {
  Node* start = graph()->NewNode(mcgraph()->common()->Start(params));
  graph()->SetStart(start);
  return start;
}

Node* WasmGraphBuilder::Param(unsigned index) {
  return graph()->NewNode(mcgraph()->common()->Parameter(index),
                          graph()->start());
}

Node* WasmGraphBuilder::Loop(Node* entry) {
  return graph()->NewNode(mcgraph()->common()->Loop(1), entry);
}

Node* WasmGraphBuilder::Terminate(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(mcgraph()->common()->Terminate(), effect, control);
  MergeControlToEnd(mcgraph(), terminate);
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

  *if_success = graph()->NewNode(mcgraph()->common()->IfSuccess(), node);
  *if_exception =
      graph()->NewNode(mcgraph()->common()->IfException(), node, node);

  return true;
}

void WasmGraphBuilder::AppendToMerge(Node* merge, Node* from) {
  DCHECK(IrOpcode::IsMergeOpcode(merge->opcode()));
  merge->AppendInput(mcgraph()->zone(), from);
  int new_size = merge->InputCount();
  NodeProperties::ChangeOp(
      merge, mcgraph()->common()->ResizeMergeOrPhi(merge->op(), new_size));
}

void WasmGraphBuilder::AppendToPhi(Node* phi, Node* from) {
  DCHECK(IrOpcode::IsPhiOpcode(phi->opcode()));
  int new_size = phi->InputCount();
  phi->InsertInput(mcgraph()->zone(), phi->InputCount() - 1, from);
  NodeProperties::ChangeOp(
      phi, mcgraph()->common()->ResizeMergeOrPhi(phi->op(), new_size));
}

Node* WasmGraphBuilder::Merge(unsigned count, Node** controls) {
  return graph()->NewNode(mcgraph()->common()->Merge(count), count, controls);
}

Node* WasmGraphBuilder::Phi(wasm::ValueType type, unsigned count, Node** vals,
                            Node* control) {
  DCHECK(IrOpcode::IsMergeOpcode(control->opcode()));
  Node** buf = Realloc(vals, count, count + 1);
  buf[count] = control;
  return graph()->NewNode(
      mcgraph()->common()->Phi(wasm::ValueTypes::MachineRepresentationFor(type),
                               count),
      count + 1, buf);
}

Node* WasmGraphBuilder::EffectPhi(unsigned count, Node** effects,
                                  Node* control) {
  DCHECK(IrOpcode::IsMergeOpcode(control->opcode()));
  Node** buf = Realloc(effects, count, count + 1);
  buf[count] = control;
  return graph()->NewNode(mcgraph()->common()->EffectPhi(count), count + 1,
                          buf);
}

Node* WasmGraphBuilder::RefNull() {
  return LOAD_INSTANCE_FIELD(NullValue, MachineType::TaggedPointer());
}

Node* WasmGraphBuilder::NoContextConstant() {
  // TODO(titzer): avoiding a dependency on JSGraph here. Refactor.
  return mcgraph()->IntPtrConstant(0);
}

Node* WasmGraphBuilder::Uint32Constant(uint32_t value) {
  return mcgraph()->Uint32Constant(value);
}

Node* WasmGraphBuilder::Int32Constant(int32_t value) {
  return mcgraph()->Int32Constant(value);
}

Node* WasmGraphBuilder::Int64Constant(int64_t value) {
  return mcgraph()->Int64Constant(value);
}

Node* WasmGraphBuilder::IntPtrConstant(intptr_t value) {
  return mcgraph()->IntPtrConstant(value);
}

void WasmGraphBuilder::StackCheck(wasm::WasmCodePosition position,
                                  Node** effect, Node** control) {
  DCHECK_NOT_NULL(env_);  // Wrappers don't get stack checks.
  if (FLAG_wasm_no_stack_checks || !env_->runtime_exception_support) {
    return;
  }
  if (effect == nullptr) effect = effect_;
  if (control == nullptr) control = control_;

  // This instruction sequence is matched in the instruction selector to
  // load the stack pointer directly on some platforms. Hence, when modifying
  // please also fix WasmStackCheckMatcher in node-matchers.h

  Node* limit_address = graph()->NewNode(
      mcgraph()->machine()->Load(MachineType::Pointer()), instance_node_.get(),
      mcgraph()->Int32Constant(WASM_INSTANCE_OBJECT_OFFSET(StackLimitAddress)),
      *effect, *control);
  Node* limit = graph()->NewNode(
      mcgraph()->machine()->Load(MachineType::Pointer()), limit_address,
      mcgraph()->IntPtrConstant(0), limit_address, *control);
  *effect = limit;
  Node* pointer = graph()->NewNode(mcgraph()->machine()->LoadStackPointer());

  Node* check =
      graph()->NewNode(mcgraph()->machine()->UintLessThan(), limit, pointer);

  Diamond stack_check(graph(), mcgraph()->common(), check, BranchHint::kTrue);
  stack_check.Chain(*control);

  if (stack_check_call_operator_ == nullptr) {
    // Build and cache the stack check call operator and the constant
    // representing the stack check code.
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        mcgraph()->zone(),                    // zone
        NoContextDescriptor{},                // descriptor
        0,                                    // stack parameter count
        CallDescriptor::kNoFlags,             // flags
        Operator::kNoProperties,              // properties
        StubCallMode::kCallWasmRuntimeStub);  // stub call mode
    // A direct call to a wasm runtime stub defined in this module.
    // Just encode the stub index. This will be patched at relocation.
    stack_check_code_node_.set(mcgraph()->RelocatableIntPtrConstant(
        wasm::WasmCode::kWasmStackGuard, RelocInfo::WASM_STUB_CALL));
    stack_check_call_operator_ = mcgraph()->common()->Call(call_descriptor);
  }

  Node* call = graph()->NewNode(stack_check_call_operator_.get(),
                                stack_check_code_node_.get(), *effect,
                                stack_check.if_false);

  SetSourcePosition(call, position);

  Node* ephi = stack_check.EffectPhi(*effect, call);

  *control = stack_check.merge;
  *effect = ephi;
}

void WasmGraphBuilder::PatchInStackCheckIfNeeded() {
  if (!needs_stack_check_) return;

  Node* start = graph()->start();
  // Place a stack check which uses a dummy node as control and effect.
  Node* dummy = graph()->NewNode(mcgraph()->common()->Dead());
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
  MachineOperatorBuilder* m = mcgraph()->machine();
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
  MachineOperatorBuilder* m = mcgraph()->machine();
  switch (opcode) {
    case wasm::kExprI32Eqz:
      op = m->Word32Equal();
      return graph()->NewNode(op, input, mcgraph()->Int32Constant(0));
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
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32SConvertSatF64:
    case wasm::kExprI32UConvertSatF64:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI32UConvertSatF32:
      return BuildIntConvertFloat(input, position, opcode);
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
      return graph()->NewNode(op, input, mcgraph()->Int64Constant(0));
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
    case wasm::kExprI32SExtendI8:
      op = m->SignExtendWord8ToInt32();
      break;
    case wasm::kExprI32SExtendI16:
      op = m->SignExtendWord16ToInt32();
      break;
    case wasm::kExprI64SExtendI8:
      op = m->SignExtendWord8ToInt64();
      break;
    case wasm::kExprI64SExtendI16:
      op = m->SignExtendWord16ToInt64();
      break;
    case wasm::kExprI64SExtendI32:
      op = m->SignExtendWord32ToInt64();
      break;
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64UConvertSatF32:
    case wasm::kExprI64SConvertSatF64:
    case wasm::kExprI64UConvertSatF64:
      return mcgraph()->machine()->Is32()
                 ? BuildCcallConvertFloat(input, position, opcode)
                 : BuildIntConvertFloat(input, position, opcode);
    case wasm::kExprRefIsNull:
      return graph()->NewNode(m->WordEqual(), input, RefNull());
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
  return mcgraph()->Float32Constant(value);
}

Node* WasmGraphBuilder::Float64Constant(double value) {
  return mcgraph()->Float64Constant(value);
}

namespace {
Node* Branch(MachineGraph* mcgraph, Node* cond, Node** true_node,
             Node** false_node, Node* control, BranchHint hint) {
  DCHECK_NOT_NULL(cond);
  DCHECK_NOT_NULL(control);
  Node* branch =
      mcgraph->graph()->NewNode(mcgraph->common()->Branch(hint), cond, control);
  *true_node = mcgraph->graph()->NewNode(mcgraph->common()->IfTrue(), branch);
  *false_node = mcgraph->graph()->NewNode(mcgraph->common()->IfFalse(), branch);
  return branch;
}
}  // namespace

Node* WasmGraphBuilder::BranchNoHint(Node* cond, Node** true_node,
                                     Node** false_node) {
  return Branch(mcgraph(), cond, true_node, false_node, Control(),
                BranchHint::kNone);
}

Node* WasmGraphBuilder::BranchExpectTrue(Node* cond, Node** true_node,
                                         Node** false_node) {
  return Branch(mcgraph(), cond, true_node, false_node, Control(),
                BranchHint::kTrue);
}

Node* WasmGraphBuilder::BranchExpectFalse(Node* cond, Node** true_node,
                                          Node** false_node) {
  return Branch(mcgraph(), cond, true_node, false_node, Control(),
                BranchHint::kFalse);
}

TrapId WasmGraphBuilder::GetTrapIdForTrap(wasm::TrapReason reason) {
  // TODO(wasm): "!env_" should not happen when compiling an actual wasm
  // function.
  if (!env_ || !env_->runtime_exception_support) {
    // We use TrapId::kInvalid as a marker to tell the code generator
    // to generate a call to a testing c-function instead of a runtime
    // stub. This code should only be called from a cctest.
    return TrapId::kInvalid;
  }

  switch (reason) {
#define TRAPREASON_TO_TRAPID(name)                                             \
  case wasm::k##name:                                                          \
    static_assert(                                                             \
        static_cast<int>(TrapId::k##name) == wasm::WasmCode::kThrowWasm##name, \
        "trap id mismatch");                                                   \
    return TrapId::k##name;
    FOREACH_WASM_TRAPREASON(TRAPREASON_TO_TRAPID)
#undef TRAPREASON_TO_TRAPID
    default:
      UNREACHABLE();
  }
}

Node* WasmGraphBuilder::TrapIfTrue(wasm::TrapReason reason, Node* cond,
                                   wasm::WasmCodePosition position) {
  TrapId trap_id = GetTrapIdForTrap(reason);
  Node* node = SetControl(graph()->NewNode(mcgraph()->common()->TrapIf(trap_id),
                                           cond, Effect(), Control()));
  SetSourcePosition(node, position);
  return node;
}

Node* WasmGraphBuilder::TrapIfFalse(wasm::TrapReason reason, Node* cond,
                                    wasm::WasmCodePosition position) {
  TrapId trap_id = GetTrapIdForTrap(reason);
  Node* node = SetControl(graph()->NewNode(
      mcgraph()->common()->TrapUnless(trap_id), cond, Effect(), Control()));
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
                      graph()->NewNode(mcgraph()->machine()->Word32Equal(),
                                       node, mcgraph()->Int32Constant(val)),
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
                    graph()->NewNode(mcgraph()->machine()->Word64Equal(), node,
                                     mcgraph()->Int64Constant(val)),
                    position);
}

// Add a check that traps if {node} is zero.
Node* WasmGraphBuilder::ZeroCheck64(wasm::TrapReason reason, Node* node,
                                    wasm::WasmCodePosition position) {
  return TrapIfEq64(reason, node, 0, position);
}

Node* WasmGraphBuilder::Switch(unsigned count, Node* key) {
  return graph()->NewNode(mcgraph()->common()->Switch(count), key, Control());
}

Node* WasmGraphBuilder::IfValue(int32_t value, Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(mcgraph()->common()->IfValue(value), sw);
}

Node* WasmGraphBuilder::IfDefault(Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(mcgraph()->common()->IfDefault(), sw);
}

Node* WasmGraphBuilder::Return(unsigned count, Node** vals) {
  static const int kStackAllocatedNodeBufferSize = 8;
  Node* stack_buffer[kStackAllocatedNodeBufferSize];
  std::vector<Node*> heap_buffer;

  Node** buf = stack_buffer;
  if (count + 3 > kStackAllocatedNodeBufferSize) {
    heap_buffer.resize(count + 3);
    buf = heap_buffer.data();
  }

  buf[0] = mcgraph()->Int32Constant(0);
  memcpy(buf + 1, vals, sizeof(void*) * count);
  buf[count + 1] = Effect();
  buf[count + 2] = Control();
  Node* ret =
      graph()->NewNode(mcgraph()->common()->Return(count), count + 3, buf);

  MergeControlToEnd(mcgraph(), ret);
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
  if (!mcgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int32Matcher match(node);
    if (match.HasValue()) {
      int32_t masked = (match.Value() & kMask32);
      if (match.Value() != masked) node = mcgraph()->Int32Constant(masked);
    } else {
      node = graph()->NewNode(mcgraph()->machine()->Word32And(), node,
                              mcgraph()->Int32Constant(kMask32));
    }
  }
  return node;
}

Node* WasmGraphBuilder::MaskShiftCount64(Node* node) {
  static const int64_t kMask64 = 0x3F;
  if (!mcgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int64Matcher match(node);
    if (match.HasValue()) {
      int64_t masked = (match.Value() & kMask64);
      if (match.Value() != masked) node = mcgraph()->Int64Constant(masked);
    } else {
      node = graph()->NewNode(mcgraph()->machine()->Word64And(), node,
                              mcgraph()->Int64Constant(kMask64));
    }
  }
  return node;
}

static bool ReverseBytesSupported(MachineOperatorBuilder* m,
                                  size_t size_in_bytes) {
  switch (size_in_bytes) {
    case 4:
    case 16:
      return true;
    case 8:
      return m->Is64();
    default:
      break;
  }
  return false;
}

Node* WasmGraphBuilder::BuildChangeEndiannessStore(
    Node* node, MachineRepresentation mem_rep, wasm::ValueType wasmtype) {
  Node* result;
  Node* value = node;
  MachineOperatorBuilder* m = mcgraph()->machine();
  int valueSizeInBytes = wasm::ValueTypes::ElementSizeInBytes(wasmtype);
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (wasmtype) {
    case wasm::kWasmF64:
      value = graph()->NewNode(m->BitcastFloat64ToInt64(), node);
      isFloat = true;
      V8_FALLTHROUGH;
    case wasm::kWasmI64:
      result = mcgraph()->Int64Constant(0);
      break;
    case wasm::kWasmF32:
      value = graph()->NewNode(m->BitcastFloat32ToInt32(), node);
      isFloat = true;
      V8_FALLTHROUGH;
    case wasm::kWasmI32:
      result = mcgraph()->Int32Constant(0);
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
    valueSizeInBytes = wasm::ValueTypes::ElementSizeInBytes(wasm::kWasmI32);
    valueSizeInBits = 8 * valueSizeInBytes;
    if (mem_rep == MachineRepresentation::kWord16) {
      value =
          graph()->NewNode(m->Word32Shl(), value, mcgraph()->Int32Constant(16));
    }
  } else if (wasmtype == wasm::kWasmI32 &&
             mem_rep == MachineRepresentation::kWord16) {
    value =
        graph()->NewNode(m->Word32Shl(), value, mcgraph()->Int32Constant(16));
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 4:
        result = graph()->NewNode(m->Word32ReverseBytes(), value);
        break;
      case 8:
        result = graph()->NewNode(m->Word64ReverseBytes(), value);
        break;
      case 16: {
        Node* byte_reversed_lanes[4];
        for (int lane = 0; lane < 4; lane++) {
          byte_reversed_lanes[lane] = graph()->NewNode(
              m->Word32ReverseBytes(),
              graph()->NewNode(mcgraph()->machine()->I32x4ExtractLane(lane),
                               value));
        }

        // This is making a copy of the value.
        result =
            graph()->NewNode(mcgraph()->machine()->S128And(), value, value);

        for (int lane = 0; lane < 4; lane++) {
          result =
              graph()->NewNode(mcgraph()->machine()->I32x4ReplaceLane(3 - lane),
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
                                      mcgraph()->Int64Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word64Shr(), value,
                                       mcgraph()->Int64Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word64And(), shiftLower,
            mcgraph()->Int64Constant(static_cast<uint64_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word64And(), shiftHigher,
            mcgraph()->Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = graph()->NewNode(m->Word64Or(), result, lowerByte);
        result = graph()->NewNode(m->Word64Or(), result, higherByte);
      } else {
        shiftLower = graph()->NewNode(m->Word32Shl(), value,
                                      mcgraph()->Int32Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word32Shr(), value,
                                       mcgraph()->Int32Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word32And(), shiftLower,
            mcgraph()->Int32Constant(static_cast<uint32_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word32And(), shiftHigher,
            mcgraph()->Int32Constant(static_cast<uint32_t>(0xFF) << i));
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
  MachineOperatorBuilder* m = mcgraph()->machine();
  int valueSizeInBytes = ElementSizeInBytes(memtype.representation());
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (memtype.representation()) {
    case MachineRepresentation::kFloat64:
      value = graph()->NewNode(m->BitcastFloat64ToInt64(), node);
      isFloat = true;
      V8_FALLTHROUGH;
    case MachineRepresentation::kWord64:
      result = mcgraph()->Int64Constant(0);
      break;
    case MachineRepresentation::kFloat32:
      value = graph()->NewNode(m->BitcastFloat32ToInt32(), node);
      isFloat = true;
      V8_FALLTHROUGH;
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord16:
      result = mcgraph()->Int32Constant(0);
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
            graph()->NewNode(m->Word32ReverseBytes(),
                             graph()->NewNode(m->Word32Shl(), value,
                                              mcgraph()->Int32Constant(16)));
        break;
      case 4:
        result = graph()->NewNode(m->Word32ReverseBytes(), value);
        break;
      case 8:
        result = graph()->NewNode(m->Word64ReverseBytes(), value);
        break;
      case 16: {
        Node* byte_reversed_lanes[4];
        for (int lane = 0; lane < 4; lane++) {
          byte_reversed_lanes[lane] = graph()->NewNode(
              m->Word32ReverseBytes(),
              graph()->NewNode(mcgraph()->machine()->I32x4ExtractLane(lane),
                               value));
        }

        // This is making a copy of the value.
        result =
            graph()->NewNode(mcgraph()->machine()->S128And(), value, value);

        for (int lane = 0; lane < 4; lane++) {
          result =
              graph()->NewNode(mcgraph()->machine()->I32x4ReplaceLane(3 - lane),
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
                                      mcgraph()->Int64Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word64Shr(), value,
                                       mcgraph()->Int64Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word64And(), shiftLower,
            mcgraph()->Int64Constant(static_cast<uint64_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word64And(), shiftHigher,
            mcgraph()->Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = graph()->NewNode(m->Word64Or(), result, lowerByte);
        result = graph()->NewNode(m->Word64Or(), result, higherByte);
      } else {
        shiftLower = graph()->NewNode(m->Word32Shl(), value,
                                      mcgraph()->Int32Constant(shiftCount));
        shiftHigher = graph()->NewNode(m->Word32Shr(), value,
                                       mcgraph()->Int32Constant(shiftCount));
        lowerByte = graph()->NewNode(
            m->Word32And(), shiftLower,
            mcgraph()->Int32Constant(static_cast<uint32_t>(0xFF)
                                     << (valueSizeInBits - 8 - i)));
        higherByte = graph()->NewNode(
            m->Word32And(), shiftHigher,
            mcgraph()->Int32Constant(static_cast<uint32_t>(0xFF) << i));
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
        shiftBitCount = mcgraph()->Int32Constant(64 - valueSizeInBits);
        result = graph()->NewNode(
            m->Word64Sar(),
            graph()->NewNode(m->Word64Shl(),
                             graph()->NewNode(m->ChangeInt32ToInt64(), result),
                             shiftBitCount),
            shiftBitCount);
      } else if (wasmtype == wasm::kWasmI32) {
        shiftBitCount = mcgraph()->Int32Constant(32 - valueSizeInBits);
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
                  mcgraph()->Int32Constant(0x7FFFFFFF)),
            Binop(wasm::kExprI32And, Unop(wasm::kExprI32ReinterpretF32, right),
                  mcgraph()->Int32Constant(0x80000000))));

  return result;
}

Node* WasmGraphBuilder::BuildF64CopySign(Node* left, Node* right) {
#if WASM_64
  Node* result = Unop(
      wasm::kExprF64ReinterpretI64,
      Binop(wasm::kExprI64Ior,
            Binop(wasm::kExprI64And, Unop(wasm::kExprI64ReinterpretF64, left),
                  mcgraph()->Int64Constant(0x7FFFFFFFFFFFFFFF)),
            Binop(wasm::kExprI64And, Unop(wasm::kExprI64ReinterpretF64, right),
                  mcgraph()->Int64Constant(0x8000000000000000))));

  return result;
#else
  MachineOperatorBuilder* m = mcgraph()->machine();

  Node* high_word_left = graph()->NewNode(m->Float64ExtractHighWord32(), left);
  Node* high_word_right =
      graph()->NewNode(m->Float64ExtractHighWord32(), right);

  Node* new_high_word = Binop(wasm::kExprI32Ior,
                              Binop(wasm::kExprI32And, high_word_left,
                                    mcgraph()->Int32Constant(0x7FFFFFFF)),
                              Binop(wasm::kExprI32And, high_word_right,
                                    mcgraph()->Int32Constant(0x80000000)));

  return graph()->NewNode(m->Float64InsertHighWord32(), left, new_high_word);
#endif
}

namespace {

MachineType IntConvertType(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI32SConvertSatF64:
      return MachineType::Int32();
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32UConvertSatF32:
    case wasm::kExprI32UConvertSatF64:
      return MachineType::Uint32();
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64SConvertSatF64:
      return MachineType::Int64();
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64UConvertSatF32:
    case wasm::kExprI64UConvertSatF64:
      return MachineType::Uint64();
    default:
      UNREACHABLE();
  }
}

MachineType FloatConvertType(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI32UConvertSatF32:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64UConvertSatF32:
      return MachineType::Float32();
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI32SConvertSatF64:
    case wasm::kExprI32UConvertSatF64:
    case wasm::kExprI64SConvertSatF64:
    case wasm::kExprI64UConvertSatF64:
      return MachineType::Float64();
    default:
      UNREACHABLE();
  }
}

const Operator* ConvertOp(WasmGraphBuilder* builder, wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32SConvertSatF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToInt32();
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32UConvertSatF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToUint32();
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32SConvertSatF64:
      return builder->mcgraph()->machine()->ChangeFloat64ToInt32();
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32UConvertSatF64:
      return builder->mcgraph()->machine()->TruncateFloat64ToUint32();
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64SConvertSatF32:
      return builder->mcgraph()->machine()->TryTruncateFloat32ToInt64();
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64UConvertSatF32:
      return builder->mcgraph()->machine()->TryTruncateFloat32ToUint64();
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64SConvertSatF64:
      return builder->mcgraph()->machine()->TryTruncateFloat64ToInt64();
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64UConvertSatF64:
      return builder->mcgraph()->machine()->TryTruncateFloat64ToUint64();
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode ConvertBackOp(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32SConvertSatF32:
      return wasm::kExprF32SConvertI32;
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32UConvertSatF32:
      return wasm::kExprF32UConvertI32;
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32SConvertSatF64:
      return wasm::kExprF64SConvertI32;
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI32UConvertSatF64:
      return wasm::kExprF64UConvertI32;
    default:
      UNREACHABLE();
  }
}

bool IsTrappingConvertOp(wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI32SConvertF32:
    case wasm::kExprI32UConvertF32:
    case wasm::kExprI32SConvertF64:
    case wasm::kExprI32UConvertF64:
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64UConvertF64:
      return true;
    case wasm::kExprI32SConvertSatF64:
    case wasm::kExprI32UConvertSatF64:
    case wasm::kExprI32SConvertSatF32:
    case wasm::kExprI32UConvertSatF32:
    case wasm::kExprI64SConvertSatF32:
    case wasm::kExprI64UConvertSatF32:
    case wasm::kExprI64SConvertSatF64:
    case wasm::kExprI64UConvertSatF64:
      return false;
    default:
      UNREACHABLE();
  }
}

Node* Zero(WasmGraphBuilder* builder, const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kWord32:
      return builder->Int32Constant(0);
    case MachineRepresentation::kWord64:
      return builder->Int64Constant(0);
    case MachineRepresentation::kFloat32:
      return builder->Float32Constant(0.0);
    case MachineRepresentation::kFloat64:
      return builder->Float64Constant(0.0);
    default:
      UNREACHABLE();
  }
}

Node* Min(WasmGraphBuilder* builder, const MachineType& ty) {
  switch (ty.semantic()) {
    case MachineSemantic::kInt32:
      return builder->Int32Constant(std::numeric_limits<int32_t>::min());
    case MachineSemantic::kUint32:
      return builder->Int32Constant(std::numeric_limits<uint32_t>::min());
    case MachineSemantic::kInt64:
      return builder->Int64Constant(std::numeric_limits<int64_t>::min());
    case MachineSemantic::kUint64:
      return builder->Int64Constant(std::numeric_limits<uint64_t>::min());
    default:
      UNREACHABLE();
  }
}

Node* Max(WasmGraphBuilder* builder, const MachineType& ty) {
  switch (ty.semantic()) {
    case MachineSemantic::kInt32:
      return builder->Int32Constant(std::numeric_limits<int32_t>::max());
    case MachineSemantic::kUint32:
      return builder->Int32Constant(std::numeric_limits<uint32_t>::max());
    case MachineSemantic::kInt64:
      return builder->Int64Constant(std::numeric_limits<int64_t>::max());
    case MachineSemantic::kUint64:
      return builder->Int64Constant(std::numeric_limits<uint64_t>::max());
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode TruncOp(const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kFloat32:
      return wasm::kExprF32Trunc;
    case MachineRepresentation::kFloat64:
      return wasm::kExprF64Trunc;
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode NeOp(const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kFloat32:
      return wasm::kExprF32Ne;
    case MachineRepresentation::kFloat64:
      return wasm::kExprF64Ne;
    default:
      UNREACHABLE();
  }
}

wasm::WasmOpcode LtOp(const MachineType& ty) {
  switch (ty.representation()) {
    case MachineRepresentation::kFloat32:
      return wasm::kExprF32Lt;
    case MachineRepresentation::kFloat64:
      return wasm::kExprF64Lt;
    default:
      UNREACHABLE();
  }
}

Node* ConvertTrapTest(WasmGraphBuilder* builder, wasm::WasmOpcode opcode,
                      const MachineType& int_ty, const MachineType& float_ty,
                      Node* trunc, Node* converted_value) {
  if (int_ty.representation() == MachineRepresentation::kWord32) {
    Node* check = builder->Unop(ConvertBackOp(opcode), converted_value);
    return builder->Binop(NeOp(float_ty), trunc, check);
  }
  return builder->graph()->NewNode(builder->mcgraph()->common()->Projection(1),
                                   trunc, builder->graph()->start());
}

Node* ConvertSaturateTest(WasmGraphBuilder* builder, wasm::WasmOpcode opcode,
                          const MachineType& int_ty,
                          const MachineType& float_ty, Node* trunc,
                          Node* converted_value) {
  Node* test = ConvertTrapTest(builder, opcode, int_ty, float_ty, trunc,
                               converted_value);
  if (int_ty.representation() == MachineRepresentation::kWord64) {
    test = builder->Binop(wasm::kExprI64Eq, test, builder->Int64Constant(0));
  }
  return test;
}

}  // namespace

Node* WasmGraphBuilder::BuildIntConvertFloat(Node* input,
                                             wasm::WasmCodePosition position,
                                             wasm::WasmOpcode opcode) {
  const MachineType int_ty = IntConvertType(opcode);
  const MachineType float_ty = FloatConvertType(opcode);
  const Operator* conv_op = ConvertOp(this, opcode);
  Node* trunc = nullptr;
  Node* converted_value = nullptr;
  const bool is_int32 =
      int_ty.representation() == MachineRepresentation::kWord32;
  if (is_int32) {
    trunc = Unop(TruncOp(float_ty), input);
    converted_value = graph()->NewNode(conv_op, trunc);
  } else {
    trunc = graph()->NewNode(conv_op, input);
    converted_value = graph()->NewNode(mcgraph()->common()->Projection(0),
                                       trunc, graph()->start());
  }
  if (IsTrappingConvertOp(opcode)) {
    Node* test =
        ConvertTrapTest(this, opcode, int_ty, float_ty, trunc, converted_value);
    if (is_int32) {
      TrapIfTrue(wasm::kTrapFloatUnrepresentable, test, position);
    } else {
      ZeroCheck64(wasm::kTrapFloatUnrepresentable, test, position);
    }
    return converted_value;
  }
  Node* test = ConvertSaturateTest(this, opcode, int_ty, float_ty, trunc,
                                   converted_value);
  Diamond tl_d(graph(), mcgraph()->common(), test, BranchHint::kFalse);
  tl_d.Chain(Control());
  Node* nan_test = Binop(NeOp(float_ty), input, input);
  Diamond nan_d(graph(), mcgraph()->common(), nan_test, BranchHint::kFalse);
  nan_d.Nest(tl_d, true);
  Node* neg_test = Binop(LtOp(float_ty), input, Zero(this, float_ty));
  Diamond sat_d(graph(), mcgraph()->common(), neg_test, BranchHint::kNone);
  sat_d.Nest(nan_d, false);
  Node* sat_val =
      sat_d.Phi(int_ty.representation(), Min(this, int_ty), Max(this, int_ty));
  Node* nan_val =
      nan_d.Phi(int_ty.representation(), Zero(this, int_ty), sat_val);
  return tl_d.Phi(int_ty.representation(), nan_val, converted_value);
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF32(Node* input) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js must use the wacky JS semantics.
  input = graph()->NewNode(m->ChangeFloat32ToFloat64(), input);
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF64(Node* input) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js must use the wacky JS semantics.
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF32(Node* input) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js must use the wacky JS semantics.
  input = graph()->NewNode(m->ChangeFloat32ToFloat64(), input);
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF64(Node* input) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js must use the wacky JS semantics.
  return graph()->NewNode(m->TruncateFloat64ToWord32(), input);
}

Node* WasmGraphBuilder::BuildBitCountingCall(Node* input, ExternalReference ref,
                                             MachineRepresentation input_type) {
  Node* stack_slot_param =
      graph()->NewNode(mcgraph()->machine()->StackSlot(input_type));

  const Operator* store_op = mcgraph()->machine()->Store(
      StoreRepresentation(input_type, kNoWriteBarrier));
  SetEffect(graph()->NewNode(store_op, stack_slot_param,
                             mcgraph()->Int32Constant(0), input, Effect(),
                             Control()));

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);

  Node* function = graph()->NewNode(mcgraph()->common()->ExternalConstant(ref));

  return BuildCCall(&sig, function, stack_slot_param);
}

Node* WasmGraphBuilder::BuildI32Ctz(Node* input) {
  return BuildBitCountingCall(input, ExternalReference::wasm_word32_ctz(),
                              MachineRepresentation::kWord32);
}

Node* WasmGraphBuilder::BuildI64Ctz(Node* input) {
  return Unop(wasm::kExprI64UConvertI32,
              BuildBitCountingCall(input, ExternalReference::wasm_word64_ctz(),
                                   MachineRepresentation::kWord64));
}

Node* WasmGraphBuilder::BuildI32Popcnt(Node* input) {
  return BuildBitCountingCall(input, ExternalReference::wasm_word32_popcnt(),
                              MachineRepresentation::kWord32);
}

Node* WasmGraphBuilder::BuildI64Popcnt(Node* input) {
  return Unop(
      wasm::kExprI64UConvertI32,
      BuildBitCountingCall(input, ExternalReference::wasm_word64_popcnt(),
                           MachineRepresentation::kWord64));
}

Node* WasmGraphBuilder::BuildF32Trunc(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_trunc();

  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32Floor(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32Ceil(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32NearestInt(Node* input) {
  MachineType type = MachineType::Float32();
  ExternalReference ref = ExternalReference::wasm_f32_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Trunc(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_trunc();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Floor(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Ceil(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64NearestInt(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_f64_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Acos(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::f64_acos_wrapper_function();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Asin(Node* input) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::f64_asin_wrapper_function();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64Pow(Node* left, Node* right) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::wasm_float64_pow();
  return BuildCFuncInstruction(ref, type, left, right);
}

Node* WasmGraphBuilder::BuildF64Mod(Node* left, Node* right) {
  MachineType type = MachineType::Float64();
  ExternalReference ref = ExternalReference::f64_mod_wrapper_function();
  return BuildCFuncInstruction(ref, type, left, right);
}

Node* WasmGraphBuilder::BuildCFuncInstruction(ExternalReference ref,
                                              MachineType type, Node* input0,
                                              Node* input1) {
  // We do truncation by calling a C function which calculates the result.
  // The input is passed to the C function as a byte buffer holding the two
  // input doubles. We reserve this byte buffer as a stack slot, store the
  // parameters in this buffer slots, pass a pointer to the buffer to the C
  // function, and after calling the C function we collect the return value from
  // the buffer.

  const int type_size = ElementSizeInBytes(type.representation());
  const int stack_slot_bytes = (input1 == nullptr ? 1 : 2) * type_size;
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(stack_slot_bytes));

  const Operator* store_op = mcgraph()->machine()->Store(
      StoreRepresentation(type.representation(), kNoWriteBarrier));
  SetEffect(graph()->NewNode(store_op, stack_slot, mcgraph()->Int32Constant(0),
                             input0, Effect(), Control()));

  Node* function = graph()->NewNode(mcgraph()->common()->ExternalConstant(ref));

  if (input1 != nullptr) {
    SetEffect(graph()->NewNode(store_op, stack_slot,
                               mcgraph()->Int32Constant(type_size), input1,
                               Effect(), Control()));
  }

  MachineType sig_types[] = {MachineType::Pointer()};
  MachineSignature sig(0, 1, sig_types);
  BuildCCall(&sig, function, stack_slot);

  return SetEffect(graph()->NewNode(mcgraph()->machine()->Load(type),
                                    stack_slot, mcgraph()->Int32Constant(0),
                                    Effect(), Control()));
}

Node* WasmGraphBuilder::BuildF32SConvertI64(Node* input) {
  // TODO(titzer/bradnelson): Check handlng of asm.js case.
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_int64_to_float32(),
      MachineRepresentation::kWord64, MachineType::Float32());
}
Node* WasmGraphBuilder::BuildF32UConvertI64(Node* input) {
  // TODO(titzer/bradnelson): Check handlng of asm.js case.
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_uint64_to_float32(),
      MachineRepresentation::kWord64, MachineType::Float32());
}
Node* WasmGraphBuilder::BuildF64SConvertI64(Node* input) {
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_int64_to_float64(),
      MachineRepresentation::kWord64, MachineType::Float64());
}
Node* WasmGraphBuilder::BuildF64UConvertI64(Node* input) {
  return BuildIntToFloatConversionInstruction(
      input, ExternalReference::wasm_uint64_to_float64(),
      MachineRepresentation::kWord64, MachineType::Float64());
}

Node* WasmGraphBuilder::BuildIntToFloatConversionInstruction(
    Node* input, ExternalReference ref,
    MachineRepresentation parameter_representation,
    const MachineType result_type) {
  int stack_slot_size =
      std::max(ElementSizeInBytes(parameter_representation),
               ElementSizeInBytes(result_type.representation()));
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(stack_slot_size));
  const Operator* store_op = mcgraph()->machine()->Store(
      StoreRepresentation(parameter_representation, kNoWriteBarrier));
  SetEffect(graph()->NewNode(store_op, stack_slot, mcgraph()->Int32Constant(0),
                             input, Effect(), Control()));
  MachineType sig_types[] = {MachineType::Pointer()};
  MachineSignature sig(0, 1, sig_types);
  Node* function = graph()->NewNode(mcgraph()->common()->ExternalConstant(ref));
  BuildCCall(&sig, function, stack_slot);
  return SetEffect(graph()->NewNode(mcgraph()->machine()->Load(result_type),
                                    stack_slot, mcgraph()->Int32Constant(0),
                                    Effect(), Control()));
}

namespace {

ExternalReference convert_ccall_ref(WasmGraphBuilder* builder,
                                    wasm::WasmOpcode opcode) {
  switch (opcode) {
    case wasm::kExprI64SConvertF32:
    case wasm::kExprI64SConvertSatF32:
      return ExternalReference::wasm_float32_to_int64();
    case wasm::kExprI64UConvertF32:
    case wasm::kExprI64UConvertSatF32:
      return ExternalReference::wasm_float32_to_uint64();
    case wasm::kExprI64SConvertF64:
    case wasm::kExprI64SConvertSatF64:
      return ExternalReference::wasm_float64_to_int64();
    case wasm::kExprI64UConvertF64:
    case wasm::kExprI64UConvertSatF64:
      return ExternalReference::wasm_float64_to_uint64();
    default:
      UNREACHABLE();
  }
}

}  // namespace

Node* WasmGraphBuilder::BuildCcallConvertFloat(Node* input,
                                               wasm::WasmCodePosition position,
                                               wasm::WasmOpcode opcode) {
  const MachineType int_ty = IntConvertType(opcode);
  const MachineType float_ty = FloatConvertType(opcode);
  ExternalReference call_ref = convert_ccall_ref(this, opcode);
  int stack_slot_size = std::max(ElementSizeInBytes(int_ty.representation()),
                                 ElementSizeInBytes(float_ty.representation()));
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(stack_slot_size));
  const Operator* store_op = mcgraph()->machine()->Store(
      StoreRepresentation(float_ty.representation(), kNoWriteBarrier));
  SetEffect(graph()->NewNode(store_op, stack_slot, Int32Constant(0), input,
                             Effect(), Control()));
  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);
  Node* function =
      graph()->NewNode(mcgraph()->common()->ExternalConstant(call_ref));
  Node* overflow = BuildCCall(&sig, function, stack_slot);
  if (IsTrappingConvertOp(opcode)) {
    ZeroCheck32(wasm::kTrapFloatUnrepresentable, overflow, position);
    return SetEffect(graph()->NewNode(mcgraph()->machine()->Load(int_ty),
                                      stack_slot, Int32Constant(0), Effect(),
                                      Control()));
  }
  Node* test = Binop(wasm::kExprI32Eq, overflow, Int32Constant(0), position);
  Diamond tl_d(graph(), mcgraph()->common(), test, BranchHint::kFalse);
  tl_d.Chain(Control());
  Node* nan_test = Binop(NeOp(float_ty), input, input);
  Diamond nan_d(graph(), mcgraph()->common(), nan_test, BranchHint::kFalse);
  nan_d.Nest(tl_d, true);
  Node* neg_test = Binop(LtOp(float_ty), input, Zero(this, float_ty));
  Diamond sat_d(graph(), mcgraph()->common(), neg_test, BranchHint::kNone);
  sat_d.Nest(nan_d, false);
  Node* sat_val =
      sat_d.Phi(int_ty.representation(), Min(this, int_ty), Max(this, int_ty));
  Node* load =
      SetEffect(graph()->NewNode(mcgraph()->machine()->Load(int_ty), stack_slot,
                                 Int32Constant(0), Effect(), Control()));
  Node* nan_val =
      nan_d.Phi(int_ty.representation(), Zero(this, int_ty), sat_val);
  return tl_d.Phi(int_ty.representation(), nan_val, load);
}

Node* WasmGraphBuilder::GrowMemory(Node* input) {
  needs_stack_check_ = true;

  WasmGrowMemoryDescriptor interface_descriptor;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      mcgraph()->zone(),                              // zone
      interface_descriptor,                           // descriptor
      interface_descriptor.GetStackParameterCount(),  // stack parameter count
      CallDescriptor::kNoFlags,                       // flags
      Operator::kNoProperties,                        // properties
      StubCallMode::kCallWasmRuntimeStub);            // stub call mode
  // A direct call to a wasm runtime stub defined in this module.
  // Just encode the stub index. This will be patched at relocation.
  Node* call_target = mcgraph()->RelocatableIntPtrConstant(
      wasm::WasmCode::kWasmGrowMemory, RelocInfo::WASM_STUB_CALL);
  return SetEffect(
      SetControl(graph()->NewNode(mcgraph()->common()->Call(call_descriptor),
                                  call_target, input, Effect(), Control())));
}

uint32_t WasmGraphBuilder::GetExceptionEncodedSize(
    const wasm::WasmException* exception) const {
  const wasm::WasmExceptionSig* sig = exception->sig;
  uint32_t encoded_size = 0;
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    size_t byte_size = static_cast<size_t>(
        wasm::ValueTypes::ElementSizeInBytes(sig->GetParam(i)));
    DCHECK_EQ(byte_size % kBytesPerExceptionValuesArrayElement, 0);
    DCHECK_LE(1, byte_size / kBytesPerExceptionValuesArrayElement);
    encoded_size += byte_size / kBytesPerExceptionValuesArrayElement;
  }
  return encoded_size;
}

Node* WasmGraphBuilder::Throw(uint32_t exception_index,
                              const wasm::WasmException* exception,
                              const Vector<Node*> values) {
  needs_stack_check_ = true;
  uint32_t encoded_size = GetExceptionEncodedSize(exception);
  Node* create_parameters[] = {
      LoadExceptionTagFromTable(exception_index),
      BuildChangeUint31ToSmi(Uint32Constant(encoded_size))};
  Node* except_obj =
      BuildCallToRuntime(Runtime::kWasmThrowCreate, create_parameters,
                         arraysize(create_parameters));
  uint32_t index = 0;
  const wasm::WasmExceptionSig* sig = exception->sig;
  MachineOperatorBuilder* m = mcgraph()->machine();
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value = values[i];
    switch (sig->GetParam(i)) {
      case wasm::kWasmF32:
        value = graph()->NewNode(m->BitcastFloat32ToInt32(), value);
        V8_FALLTHROUGH;
      case wasm::kWasmI32:
        BuildEncodeException32BitValue(except_obj, &index, value);
        break;
      case wasm::kWasmF64:
        value = graph()->NewNode(m->BitcastFloat64ToInt64(), value);
        V8_FALLTHROUGH;
      case wasm::kWasmI64: {
        Node* upper32 = graph()->NewNode(
            m->TruncateInt64ToInt32(),
            Binop(wasm::kExprI64ShrU, value, Int64Constant(32)));
        BuildEncodeException32BitValue(except_obj, &index, upper32);
        Node* lower32 = graph()->NewNode(m->TruncateInt64ToInt32(), value);
        BuildEncodeException32BitValue(except_obj, &index, lower32);
        break;
      }
      default:
        UNREACHABLE();
    }
  }
  DCHECK_EQ(encoded_size, index);
  WasmThrowDescriptor interface_descriptor;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      mcgraph()->zone(), interface_descriptor,
      interface_descriptor.GetStackParameterCount(), CallDescriptor::kNoFlags,
      Operator::kNoProperties, StubCallMode::kCallWasmRuntimeStub);
  Node* call_target = mcgraph()->RelocatableIntPtrConstant(
      wasm::WasmCode::kWasmThrow, RelocInfo::WASM_STUB_CALL);
  return SetEffect(SetControl(
      graph()->NewNode(mcgraph()->common()->Call(call_descriptor), call_target,
                       except_obj, Effect(), Control())));
}

void WasmGraphBuilder::BuildEncodeException32BitValue(Node* except_obj,
                                                      uint32_t* index,
                                                      Node* value) {
  MachineOperatorBuilder* machine = mcgraph()->machine();
  Node* upper_parameters[] = {
      except_obj, BuildChangeUint31ToSmi(Int32Constant(*index)),
      BuildChangeUint31ToSmi(
          graph()->NewNode(machine->Word32Shr(), value, Int32Constant(16))),
  };
  BuildCallToRuntime(Runtime::kWasmExceptionSetElement, upper_parameters,
                     arraysize(upper_parameters));
  ++(*index);
  Node* lower_parameters[] = {
      except_obj, BuildChangeUint31ToSmi(Int32Constant(*index)),
      BuildChangeUint31ToSmi(graph()->NewNode(machine->Word32And(), value,
                                              Int32Constant(0xFFFFu))),
  };
  BuildCallToRuntime(Runtime::kWasmExceptionSetElement, lower_parameters,
                     arraysize(lower_parameters));
  ++(*index);
}

Node* WasmGraphBuilder::BuildDecodeException32BitValue(Node* const* values,
                                                       uint32_t* index) {
  MachineOperatorBuilder* machine = mcgraph()->machine();
  Node* upper = BuildChangeSmiToInt32(values[*index]);
  (*index)++;
  upper = graph()->NewNode(machine->Word32Shl(), upper, Int32Constant(16));
  Node* lower = BuildChangeSmiToInt32(values[*index]);
  (*index)++;
  Node* value = graph()->NewNode(machine->Word32Or(), upper, lower);
  return value;
}

Node* WasmGraphBuilder::Rethrow(Node* except_obj) {
  needs_stack_check_ = true;
  WasmThrowDescriptor interface_descriptor;
  auto call_descriptor = Linkage::GetStubCallDescriptor(
      mcgraph()->zone(), interface_descriptor,
      interface_descriptor.GetStackParameterCount(), CallDescriptor::kNoFlags,
      Operator::kNoProperties, StubCallMode::kCallWasmRuntimeStub);
  Node* call_target = mcgraph()->RelocatableIntPtrConstant(
      wasm::WasmCode::kWasmThrow, RelocInfo::WASM_STUB_CALL);
  return SetEffect(SetControl(
      graph()->NewNode(mcgraph()->common()->Call(call_descriptor), call_target,
                       except_obj, Effect(), Control())));
}

Node* WasmGraphBuilder::ExceptionTagEqual(Node* caught_tag,
                                          Node* expected_tag) {
  MachineOperatorBuilder* machine = mcgraph()->machine();
  return graph()->NewNode(machine->WordEqual(), caught_tag, expected_tag);
}

Node* WasmGraphBuilder::LoadExceptionTagFromTable(uint32_t exception_index) {
  Node* exceptions_table =
      LOAD_INSTANCE_FIELD(ExceptionsTable, MachineType::TaggedPointer());
  Node* tag = LOAD_FIXED_ARRAY_SLOT(exceptions_table, exception_index);
  return tag;
}

Node* WasmGraphBuilder::GetExceptionTag(Node* except_obj) {
  needs_stack_check_ = true;
  return BuildCallToRuntime(Runtime::kWasmExceptionGetTag, &except_obj, 1);
}

Node** WasmGraphBuilder::GetExceptionValues(
    Node* except_obj, const wasm::WasmException* except_decl) {
  // TODO(kschimpf): We need to move this code to the function-body-decoder.cc
  // in order to build landing-pad (exception) edges in case the runtime
  // call causes an exception.

  // Start by getting the encoded values from the exception.
  uint32_t encoded_size = GetExceptionEncodedSize(except_decl);
  Node** values = Buffer(encoded_size);
  for (uint32_t i = 0; i < encoded_size; ++i) {
    Node* parameters[] = {except_obj,
                          BuildChangeUint31ToSmi(Uint32Constant(i))};
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
  MachineOperatorBuilder* m = mcgraph()->machine();
  ZeroCheck32(wasm::kTrapDivByZero, right, position);
  Node* before = Control();
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(
      graph()->NewNode(m->Word32Equal(), right, mcgraph()->Int32Constant(-1)),
      &denom_is_m1, &denom_is_not_m1);
  SetControl(denom_is_m1);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, left, kMinInt, position);
  if (Control() != denom_is_m1) {
    SetControl(graph()->NewNode(mcgraph()->common()->Merge(2), denom_is_not_m1,
                                Control()));
  } else {
    SetControl(before);
  }
  return graph()->NewNode(m->Int32Div(), left, right, Control());
}

Node* WasmGraphBuilder::BuildI32RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = mcgraph()->machine();

  ZeroCheck32(wasm::kTrapRemByZero, right, position);

  Diamond d(
      graph(), mcgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, mcgraph()->Int32Constant(-1)),
      BranchHint::kFalse);
  d.Chain(Control());

  return d.Phi(MachineRepresentation::kWord32, mcgraph()->Int32Constant(0),
               graph()->NewNode(m->Int32Mod(), left, right, d.if_false));
}

Node* WasmGraphBuilder::BuildI32DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  return graph()->NewNode(m->Uint32Div(), left, right,
                          ZeroCheck32(wasm::kTrapDivByZero, right, position));
}

Node* WasmGraphBuilder::BuildI32RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  return graph()->NewNode(m->Uint32Mod(), left, right,
                          ZeroCheck32(wasm::kTrapRemByZero, right, position));
}

Node* WasmGraphBuilder::BuildI32AsmjsDivS(Node* left, Node* right) {
  MachineOperatorBuilder* m = mcgraph()->machine();

  Int32Matcher mr(right);
  if (mr.HasValue()) {
    if (mr.Value() == 0) {
      return mcgraph()->Int32Constant(0);
    } else if (mr.Value() == -1) {
      // The result is the negation of the left input.
      return graph()->NewNode(m->Int32Sub(), mcgraph()->Int32Constant(0), left);
    }
    return graph()->NewNode(m->Int32Div(), left, right, Control());
  }

  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Int32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return graph()->NewNode(m->Int32Div(), left, right, graph()->start());
  }

  // Check denominator for zero.
  Diamond z(
      graph(), mcgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, mcgraph()->Int32Constant(0)),
      BranchHint::kFalse);

  // Check numerator for -1. (avoid minint / -1 case).
  Diamond n(
      graph(), mcgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, mcgraph()->Int32Constant(-1)),
      BranchHint::kFalse);

  Node* div = graph()->NewNode(m->Int32Div(), left, right, z.if_false);
  Node* neg =
      graph()->NewNode(m->Int32Sub(), mcgraph()->Int32Constant(0), left);

  return n.Phi(
      MachineRepresentation::kWord32, neg,
      z.Phi(MachineRepresentation::kWord32, mcgraph()->Int32Constant(0), div));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemS(Node* left, Node* right) {
  CommonOperatorBuilder* c = mcgraph()->common();
  MachineOperatorBuilder* m = mcgraph()->machine();
  Node* const zero = mcgraph()->Int32Constant(0);

  Int32Matcher mr(right);
  if (mr.HasValue()) {
    if (mr.Value() == 0 || mr.Value() == -1) {
      return zero;
    }
    return graph()->NewNode(m->Int32Mod(), left, right, Control());
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
  Node* const minus_one = mcgraph()->Int32Constant(-1);

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
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Uint32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return graph()->NewNode(m->Uint32Div(), left, right, graph()->start());
  }

  // Explicit check for x % 0.
  Diamond z(
      graph(), mcgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, mcgraph()->Int32Constant(0)),
      BranchHint::kFalse);

  return z.Phi(MachineRepresentation::kWord32, mcgraph()->Int32Constant(0),
               graph()->NewNode(mcgraph()->machine()->Uint32Div(), left, right,
                                z.if_false));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemU(Node* left, Node* right) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  // asm.js semantics return 0 on divide or mod by zero.
  // Explicit check for x % 0.
  Diamond z(
      graph(), mcgraph()->common(),
      graph()->NewNode(m->Word32Equal(), right, mcgraph()->Int32Constant(0)),
      BranchHint::kFalse);

  Node* rem = graph()->NewNode(mcgraph()->machine()->Uint32Mod(), left, right,
                               z.if_false);
  return z.Phi(MachineRepresentation::kWord32, mcgraph()->Int32Constant(0),
               rem);
}

Node* WasmGraphBuilder::BuildI64DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_int64_div(),
                          MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  ZeroCheck64(wasm::kTrapDivByZero, right, position);
  Node* before = Control();
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(graph()->NewNode(mcgraph()->machine()->Word64Equal(), right,
                                     mcgraph()->Int64Constant(-1)),
                    &denom_is_m1, &denom_is_not_m1);
  SetControl(denom_is_m1);
  TrapIfEq64(wasm::kTrapDivUnrepresentable, left,
             std::numeric_limits<int64_t>::min(), position);
  if (Control() != denom_is_m1) {
    SetControl(graph()->NewNode(mcgraph()->common()->Merge(2), denom_is_not_m1,
                                Control()));
  } else {
    SetControl(before);
  }
  return graph()->NewNode(mcgraph()->machine()->Int64Div(), left, right,
                          Control());
}

Node* WasmGraphBuilder::BuildI64RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_int64_mod(),
                          MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  ZeroCheck64(wasm::kTrapRemByZero, right, position);
  Diamond d(mcgraph()->graph(), mcgraph()->common(),
            graph()->NewNode(mcgraph()->machine()->Word64Equal(), right,
                             mcgraph()->Int64Constant(-1)));

  d.Chain(Control());

  Node* rem = graph()->NewNode(mcgraph()->machine()->Int64Mod(), left, right,
                               d.if_false);

  return d.Phi(MachineRepresentation::kWord64, mcgraph()->Int64Constant(0),
               rem);
}

Node* WasmGraphBuilder::BuildI64DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_uint64_div(),
                          MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  return graph()->NewNode(mcgraph()->machine()->Uint64Div(), left, right,
                          ZeroCheck64(wasm::kTrapDivByZero, right, position));
}
Node* WasmGraphBuilder::BuildI64RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_uint64_mod(),
                          MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  return graph()->NewNode(mcgraph()->machine()->Uint64Mod(), left, right,
                          ZeroCheck64(wasm::kTrapRemByZero, right, position));
}

Node* WasmGraphBuilder::BuildDiv64Call(Node* left, Node* right,
                                       ExternalReference ref,
                                       MachineType result_type,
                                       wasm::TrapReason trap_zero,
                                       wasm::WasmCodePosition position) {
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(2 * sizeof(double)));

  const Operator* store_op = mcgraph()->machine()->Store(
      StoreRepresentation(MachineRepresentation::kWord64, kNoWriteBarrier));
  SetEffect(graph()->NewNode(store_op, stack_slot, mcgraph()->Int32Constant(0),
                             left, Effect(), Control()));
  SetEffect(graph()->NewNode(store_op, stack_slot,
                             mcgraph()->Int32Constant(sizeof(double)), right,
                             Effect(), Control()));

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);

  Node* function = graph()->NewNode(mcgraph()->common()->ExternalConstant(ref));
  Node* call = BuildCCall(&sig, function, stack_slot);

  ZeroCheck32(trap_zero, call, position);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, call, -1, position);
  return SetEffect(graph()->NewNode(mcgraph()->machine()->Load(result_type),
                                    stack_slot, mcgraph()->Int32Constant(0),
                                    Effect(), Control()));
}

template <typename... Args>
Node* WasmGraphBuilder::BuildCCall(MachineSignature* sig, Node* function,
                                   Args... args) {
  DCHECK_LE(sig->return_count(), 1);
  DCHECK_EQ(sizeof...(args), sig->parameter_count());
  Node* const call_args[] = {function, args..., Effect(), Control()};

  auto call_descriptor =
      Linkage::GetSimplifiedCDescriptor(mcgraph()->zone(), sig);

  const Operator* op = mcgraph()->common()->Call(call_descriptor);
  return SetEffect(graph()->NewNode(op, arraysize(call_args), call_args));
}

Node* WasmGraphBuilder::BuildWasmCall(wasm::FunctionSig* sig, Node** args,
                                      Node*** rets,
                                      wasm::WasmCodePosition position,
                                      Node* instance_node,
                                      UseRetpoline use_retpoline) {
  if (instance_node == nullptr) {
    DCHECK_NOT_NULL(instance_node_);
    instance_node = instance_node_.get();
  }
  needs_stack_check_ = true;
  const size_t params = sig->parameter_count();
  const size_t extra = 3;  // instance_node, effect, and control.
  const size_t count = 1 + params + extra;

  // Reallocate the buffer to make space for extra inputs.
  args = Realloc(args, 1 + params, count);

  // Make room for the instance_node parameter at index 1, just after code.
  memmove(&args[2], &args[1], params * sizeof(Node*));
  args[1] = instance_node;

  // Add effect and control inputs.
  args[params + 2] = Effect();
  args[params + 3] = Control();

  auto call_descriptor =
      GetWasmCallDescriptor(mcgraph()->zone(), sig, use_retpoline);
  const Operator* op = mcgraph()->common()->Call(call_descriptor);
  Node* call = SetEffect(graph()->NewNode(op, static_cast<int>(count), args));
  DCHECK(position == wasm::kNoCodePosition || position > 0);
  if (position > 0) SetSourcePosition(call, position);

  size_t ret_count = sig->return_count();
  if (ret_count == 0) return call;  // No return value.

  *rets = Buffer(ret_count);
  if (ret_count == 1) {
    // Only a single return value.
    (*rets)[0] = call;
  } else {
    // Create projections for all return values.
    for (size_t i = 0; i < ret_count; i++) {
      (*rets)[i] = graph()->NewNode(mcgraph()->common()->Projection(i), call,
                                    graph()->start());
    }
  }
  return call;
}

Node* WasmGraphBuilder::BuildImportCall(wasm::FunctionSig* sig, Node** args,
                                        Node*** rets,
                                        wasm::WasmCodePosition position,
                                        int func_index) {
  // Load the instance from the imported_instances array at a known offset.
  Node* imported_instances = LOAD_INSTANCE_FIELD(ImportedFunctionInstances,
                                                 MachineType::TaggedPointer());
  Node* instance_node = LOAD_FIXED_ARRAY_SLOT(imported_instances, func_index);

  // Load the target from the imported_targets array at a known offset.
  Node* imported_targets =
      LOAD_INSTANCE_FIELD(ImportedFunctionTargets, MachineType::Pointer());
  Node* target_node = SetEffect(graph()->NewNode(
      mcgraph()->machine()->Load(MachineType::Pointer()), imported_targets,
      mcgraph()->Int32Constant(func_index * kPointerSize), Effect(),
      Control()));
  args[0] = target_node;
  return BuildWasmCall(sig, args, rets, position, instance_node,
                       untrusted_code_mitigations_ ? kRetpoline : kNoRetpoline);
}

Node* WasmGraphBuilder::BuildImportCall(wasm::FunctionSig* sig, Node** args,
                                        Node*** rets,
                                        wasm::WasmCodePosition position,
                                        Node* func_index) {
  // Load the instance from the imported_instances array.
  Node* imported_instances = LOAD_INSTANCE_FIELD(ImportedFunctionInstances,
                                                 MachineType::TaggedPointer());
  // Access fixed array at {header_size - tag + func_index * kPointerSize}.
  Node* imported_instances_data = graph()->NewNode(
      mcgraph()->machine()->IntAdd(), imported_instances,
      mcgraph()->IntPtrConstant(
          wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(0)));
  Node* func_index_times_pointersize = graph()->NewNode(
      mcgraph()->machine()->IntMul(), Uint32ToUintptr(func_index),
      mcgraph()->Int32Constant(kPointerSize));
  Node* instance_node = SetEffect(
      graph()->NewNode(mcgraph()->machine()->Load(MachineType::TaggedPointer()),
                       imported_instances_data, func_index_times_pointersize,
                       Effect(), Control()));

  // Load the target from the imported_targets array at the offset of
  // {func_index}.
  Node* imported_targets =
      LOAD_INSTANCE_FIELD(ImportedFunctionTargets, MachineType::Pointer());
  Node* target_node = SetEffect(graph()->NewNode(
      mcgraph()->machine()->Load(MachineType::Pointer()), imported_targets,
      func_index_times_pointersize, Effect(), Control()));
  args[0] = target_node;
  return BuildWasmCall(sig, args, rets, position, instance_node,
                       untrusted_code_mitigations_ ? kRetpoline : kNoRetpoline);
}

Node* WasmGraphBuilder::CallDirect(uint32_t index, Node** args, Node*** rets,
                                   wasm::WasmCodePosition position) {
  DCHECK_NULL(args[0]);
  wasm::FunctionSig* sig = env_->module->functions[index].sig;

  if (env_ && index < env_->module->num_imported_functions) {
    // Call to an imported function.
    return BuildImportCall(sig, args, rets, position, index);
  }

  // A direct call to a wasm function defined in this module.
  // Just encode the function index. This will be patched at instantiation.
  Address code = static_cast<Address>(index);
  args[0] = mcgraph()->RelocatableIntPtrConstant(code, RelocInfo::WASM_CALL);

  return BuildWasmCall(sig, args, rets, position, nullptr, kNoRetpoline);
}

Node* WasmGraphBuilder::CallIndirect(uint32_t sig_index, Node** args,
                                     Node*** rets,
                                     wasm::WasmCodePosition position) {
  DCHECK_NOT_NULL(args[0]);
  DCHECK_NOT_NULL(env_);

  // Assume only one table for now.
  wasm::FunctionSig* sig = env_->module->signatures[sig_index];

  Node* ift_size =
      LOAD_INSTANCE_FIELD(IndirectFunctionTableSize, MachineType::Uint32());

  MachineOperatorBuilder* machine = mcgraph()->machine();
  Node* key = args[0];

  // Bounds check against the table size.
  Node* in_bounds = graph()->NewNode(machine->Uint32LessThan(), key, ift_size);
  TrapIfFalse(wasm::kTrapFuncInvalid, in_bounds, position);

  // Mask the key to prevent SSCA.
  if (untrusted_code_mitigations_) {
    // mask = ((key - size) & ~key) >> 31
    Node* neg_key =
        graph()->NewNode(machine->Word32Xor(), key, Int32Constant(-1));
    Node* masked_diff = graph()->NewNode(
        machine->Word32And(),
        graph()->NewNode(machine->Int32Sub(), key, ift_size), neg_key);
    Node* mask =
        graph()->NewNode(machine->Word32Sar(), masked_diff, Int32Constant(31));
    key = graph()->NewNode(machine->Word32And(), key, mask);
  }

  // Load signature from the table and check.
  Node* ift_sig_ids =
      LOAD_INSTANCE_FIELD(IndirectFunctionTableSigIds, MachineType::Pointer());

  int32_t expected_sig_id = env_->module->signature_ids[sig_index];
  Node* scaled_key = Uint32ToUintptr(
      graph()->NewNode(machine->Word32Shl(), key, Int32Constant(2)));

  Node* loaded_sig =
      SetEffect(graph()->NewNode(machine->Load(MachineType::Int32()),
                                 ift_sig_ids, scaled_key, Effect(), Control()));
  Node* sig_match = graph()->NewNode(machine->WordEqual(), loaded_sig,
                                     Int32Constant(expected_sig_id));

  TrapIfFalse(wasm::kTrapFuncSigMismatch, sig_match, position);

  Node* ift_targets =
      LOAD_INSTANCE_FIELD(IndirectFunctionTableTargets, MachineType::Pointer());
  Node* ift_instances = LOAD_INSTANCE_FIELD(IndirectFunctionTableInstances,
                                            MachineType::TaggedPointer());

  scaled_key = graph()->NewNode(machine->Word32Shl(), key,
                                Int32Constant(kPointerSizeLog2));

  Node* target =
      SetEffect(graph()->NewNode(machine->Load(MachineType::Pointer()),
                                 ift_targets, scaled_key, Effect(), Control()));

  Node* target_instance = SetEffect(graph()->NewNode(
      machine->Load(MachineType::TaggedPointer()),
      graph()->NewNode(machine->IntAdd(), ift_instances, scaled_key),
      Int32Constant(wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(0)),
      Effect(), Control()));

  args[0] = target;

  return BuildWasmCall(sig, args, rets, position, target_instance,
                       untrusted_code_mitigations_ ? kRetpoline : kNoRetpoline);
}

Node* WasmGraphBuilder::BuildI32Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word32Rol opcode in TurboFan.
  Int32Matcher m(right);
  if (m.HasValue()) {
    return Binop(wasm::kExprI32Ror, left,
                 mcgraph()->Int32Constant(32 - m.Value()));
  } else {
    return Binop(wasm::kExprI32Ror, left,
                 Binop(wasm::kExprI32Sub, mcgraph()->Int32Constant(32), right));
  }
}

Node* WasmGraphBuilder::BuildI64Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word64Rol opcode in TurboFan.
  Int64Matcher m(right);
  if (m.HasValue()) {
    return Binop(wasm::kExprI64Ror, left,
                 mcgraph()->Int64Constant(64 - m.Value()));
  } else {
    return Binop(wasm::kExprI64Ror, left,
                 Binop(wasm::kExprI64Sub, mcgraph()->Int64Constant(64), right));
  }
}

Node* WasmGraphBuilder::Invert(Node* node) {
  return Unop(wasm::kExprI32Eqz, node);
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

Node* WasmGraphBuilder::BuildChangeInt32ToIntPtr(Node* value) {
  if (mcgraph()->machine()->Is64()) {
    value = graph()->NewNode(mcgraph()->machine()->ChangeInt32ToInt64(), value);
  }
  return value;
}

Node* WasmGraphBuilder::BuildChangeInt32ToSmi(Node* value) {
  value = BuildChangeInt32ToIntPtr(value);
  return graph()->NewNode(mcgraph()->machine()->WordShl(), value,
                          BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildChangeUint31ToSmi(Node* value) {
  return graph()->NewNode(mcgraph()->machine()->WordShl(),
                          Uint32ToUintptr(value), BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildSmiShiftBitsConstant() {
  return mcgraph()->IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* WasmGraphBuilder::BuildChangeSmiToInt32(Node* value) {
  value = graph()->NewNode(mcgraph()->machine()->WordSar(), value,
                           BuildSmiShiftBitsConstant());
  if (mcgraph()->machine()->Is64()) {
    value =
        graph()->NewNode(mcgraph()->machine()->TruncateInt64ToInt32(), value);
  }
  return value;
}

void WasmGraphBuilder::InitInstanceCache(
    WasmInstanceCacheNodes* instance_cache) {
  DCHECK_NOT_NULL(instance_node_);

  // Load the memory start.
  instance_cache->mem_start = SetEffect(graph()->NewNode(
      mcgraph()->machine()->Load(MachineType::UintPtr()), instance_node_.get(),
      mcgraph()->Int32Constant(WASM_INSTANCE_OBJECT_OFFSET(MemoryStart)),
      Effect(), Control()));

  // Load the memory size.
  instance_cache->mem_size = SetEffect(graph()->NewNode(
      mcgraph()->machine()->Load(MachineType::UintPtr()), instance_node_.get(),
      mcgraph()->Int32Constant(WASM_INSTANCE_OBJECT_OFFSET(MemorySize)),
      Effect(), Control()));

  if (untrusted_code_mitigations_) {
    // Load the memory mask.
    instance_cache->mem_mask =
        LOAD_INSTANCE_FIELD(MemoryMask, MachineType::UintPtr());
  } else {
    // Explicitly set to nullptr to ensure a SEGV when we try to use it.
    instance_cache->mem_mask = nullptr;
  }
}

void WasmGraphBuilder::PrepareInstanceCacheForLoop(
    WasmInstanceCacheNodes* instance_cache, Node* control) {
#define INTRODUCE_PHI(field, rep)                                            \
  instance_cache->field = graph()->NewNode(mcgraph()->common()->Phi(rep, 1), \
                                           instance_cache->field, control);

  INTRODUCE_PHI(mem_start, MachineType::PointerRepresentation());
  INTRODUCE_PHI(mem_size, MachineType::PointerRepresentation());
  if (untrusted_code_mitigations_) {
    INTRODUCE_PHI(mem_mask, MachineType::PointerRepresentation());
  }

#undef INTRODUCE_PHI
}

void WasmGraphBuilder::NewInstanceCacheMerge(WasmInstanceCacheNodes* to,
                                             WasmInstanceCacheNodes* from,
                                             Node* merge) {
#define INTRODUCE_PHI(field, rep)                                            \
  if (to->field != from->field) {                                            \
    Node* vals[] = {to->field, from->field, merge};                          \
    to->field = graph()->NewNode(mcgraph()->common()->Phi(rep, 2), 3, vals); \
  }

  INTRODUCE_PHI(mem_start, MachineType::PointerRepresentation());
  INTRODUCE_PHI(mem_size, MachineRepresentation::kWord32);
  if (untrusted_code_mitigations_) {
    INTRODUCE_PHI(mem_mask, MachineRepresentation::kWord32);
  }

#undef INTRODUCE_PHI
}

void WasmGraphBuilder::MergeInstanceCacheInto(WasmInstanceCacheNodes* to,
                                              WasmInstanceCacheNodes* from,
                                              Node* merge) {
  to->mem_size = CreateOrMergeIntoPhi(MachineType::PointerRepresentation(),
                                      merge, to->mem_size, from->mem_size);
  to->mem_start = CreateOrMergeIntoPhi(MachineType::PointerRepresentation(),
                                       merge, to->mem_start, from->mem_start);
  if (untrusted_code_mitigations_) {
    to->mem_mask = CreateOrMergeIntoPhi(MachineType::PointerRepresentation(),
                                        merge, to->mem_mask, from->mem_mask);
  }
}

Node* WasmGraphBuilder::CreateOrMergeIntoPhi(MachineRepresentation rep,
                                             Node* merge, Node* tnode,
                                             Node* fnode) {
  if (IsPhiWithMerge(tnode, merge)) {
    AppendToPhi(tnode, fnode);
  } else if (tnode != fnode) {
    uint32_t count = merge->InputCount();
    // + 1 for the merge node.
    Node** vals = Buffer(count + 1);
    for (uint32_t j = 0; j < count - 1; j++) vals[j] = tnode;
    vals[count - 1] = fnode;
    vals[count] = merge;
    return graph()->NewNode(mcgraph()->common()->Phi(rep, count), count + 1,
                            vals);
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
                                              const wasm::WasmGlobal& global,
                                              Node** base_node,
                                              Node** offset_node) {
  DCHECK_NOT_NULL(instance_node_);
  if (global.mutability && global.imported) {
    if (imported_mutable_globals_ == nullptr) {
      // Load imported_mutable_globals_ from the instance object at runtime.
      imported_mutable_globals_ = graph()->NewNode(
          mcgraph()->machine()->Load(MachineType::UintPtr()),
          instance_node_.get(),
          mcgraph()->Int32Constant(
              WASM_INSTANCE_OBJECT_OFFSET(ImportedMutableGlobals)),
          graph()->start(), graph()->start());
    }
    *base_node = SetEffect(graph()->NewNode(
        mcgraph()->machine()->Load(MachineType::UintPtr()),
        imported_mutable_globals_.get(),
        mcgraph()->Int32Constant(global.index * sizeof(Address)), Effect(),
        Control()));
    *offset_node = mcgraph()->Int32Constant(0);
  } else {
    if (globals_start_ == nullptr) {
      // Load globals_start from the instance object at runtime.
      // TODO(wasm): we currently generate only one load of the {globals_start}
      // start per graph, which means it can be placed anywhere by the
      // scheduler. This is legal because the globals_start should never change.
      // However, in some cases (e.g. if the instance object is already in a
      // register), it is slightly more efficient to reload this value from the
      // instance object. Since this depends on register allocation, it is not
      // possible to express in the graph, and would essentially constitute a
      // "mem2reg" optimization in TurboFan.
      globals_start_ = graph()->NewNode(
          mcgraph()->machine()->Load(MachineType::UintPtr()),
          instance_node_.get(),
          mcgraph()->Int32Constant(WASM_INSTANCE_OBJECT_OFFSET(GlobalsStart)),
          graph()->start(), graph()->start());
    }
    *base_node = globals_start_.get();
    *offset_node = mcgraph()->Int32Constant(global.offset);

    if (mem_type == MachineType::Simd128() && global.offset != 0) {
      // TODO(titzer,bbudge): code generation for SIMD memory offsets is broken.
      *base_node = graph()->NewNode(mcgraph()->machine()->IntAdd(), *base_node,
                                    *offset_node);
      *offset_node = mcgraph()->Int32Constant(0);
    }
  }
}

Node* WasmGraphBuilder::MemBuffer(uint32_t offset) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  DCHECK_NOT_NULL(mem_start);
  if (offset == 0) return mem_start;
  return graph()->NewNode(mcgraph()->machine()->IntAdd(), mem_start,
                          mcgraph()->IntPtrConstant(offset));
}

Node* WasmGraphBuilder::CurrentMemoryPages() {
  // CurrentMemoryPages can not be called from asm.js.
  DCHECK_EQ(wasm::kWasmOrigin, env_->module->origin);
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_size);
  Node* result =
      graph()->NewNode(mcgraph()->machine()->WordShr(), mem_size,
                       mcgraph()->Int32Constant(wasm::kWasmPageSizeLog2));
  if (mcgraph()->machine()->Is64()) {
    result =
        graph()->NewNode(mcgraph()->machine()->TruncateInt64ToInt32(), result);
  }
  return result;
}

Node* WasmGraphBuilder::BuildLoadBuiltinFromInstance(int builtin_index) {
  DCHECK(Builtins::IsBuiltinId(builtin_index));
  Node* roots =
      LOAD_INSTANCE_FIELD(RootsArrayAddress, MachineType::TaggedPointer());
  return LOAD_TAGGED_POINTER(
      roots, Heap::roots_to_builtins_offset() + builtin_index * kPointerSize);
}

// Only call this function for code which is not reused across instantiations,
// as we do not patch the embedded js_context.
Node* WasmGraphBuilder::BuildCallToRuntimeWithContext(Runtime::FunctionId f,
                                                      Node* js_context,
                                                      Node** parameters,
                                                      int parameter_count) {
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  auto call_descriptor = Linkage::GetRuntimeCallDescriptor(
      mcgraph()->zone(), f, fun->nargs, Operator::kNoProperties,
      CallDescriptor::kNoFlags);
  // The CEntryStub is loaded from the instance_node so that generated code is
  // Isolate independent. At the moment this is only done for CEntryStub(1).
  DCHECK_EQ(1, fun->result_size);
  Node* centry_stub =
      LOAD_INSTANCE_FIELD(CEntryStub, MachineType::TaggedPointer());
  // At the moment we only allow 4 parameters. If more parameters are needed,
  // increase this constant accordingly.
  static const int kMaxParams = 4;
  DCHECK_GE(kMaxParams, parameter_count);
  Node* inputs[kMaxParams + 6];
  int count = 0;
  inputs[count++] = centry_stub;
  for (int i = 0; i < parameter_count; i++) {
    inputs[count++] = parameters[i];
  }
  inputs[count++] =
      mcgraph()->ExternalConstant(ExternalReference::Create(f));  // ref
  inputs[count++] = mcgraph()->Int32Constant(fun->nargs);         // arity
  inputs[count++] = js_context;                                   // js_context
  inputs[count++] = Effect();
  inputs[count++] = Control();

  return SetEffect(mcgraph()->graph()->NewNode(
      mcgraph()->common()->Call(call_descriptor), count, inputs));
}

Node* WasmGraphBuilder::BuildCallToRuntime(Runtime::FunctionId f,
                                           Node** parameters,
                                           int parameter_count) {
  return BuildCallToRuntimeWithContext(f, NoContextConstant(), parameters,
                                       parameter_count);
}

Node* WasmGraphBuilder::GetGlobal(uint32_t index) {
  MachineType mem_type =
      wasm::ValueTypes::MachineTypeFor(env_->module->globals[index].type);
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(mem_type, env_->module->globals[index], &base,
                         &offset);
  Node* load = SetEffect(graph()->NewNode(mcgraph()->machine()->Load(mem_type),
                                          base, offset, Effect(), Control()));
#if defined(V8_TARGET_BIG_ENDIAN)
  load = BuildChangeEndiannessLoad(load, mem_type,
                                   env_->module->globals[index].type);
#endif
  return load;
}

Node* WasmGraphBuilder::SetGlobal(uint32_t index, Node* val) {
  MachineType mem_type =
      wasm::ValueTypes::MachineTypeFor(env_->module->globals[index].type);
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(mem_type, env_->module->globals[index], &base,
                         &offset);
  const Operator* op = mcgraph()->machine()->Store(
      StoreRepresentation(mem_type.representation(), kNoWriteBarrier));
#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndiannessStore(val, mem_type.representation(),
                                   env_->module->globals[index].type);
#endif
  return SetEffect(
      graph()->NewNode(op, base, offset, val, Effect(), Control()));
}

Node* WasmGraphBuilder::CheckBoundsAndAlignment(
    uint8_t access_size, Node* index, uint32_t offset,
    wasm::WasmCodePosition position) {
  // Atomic operations access the memory, need to be bound checked till
  // TrapHandlers are enabled on atomic operations
  index =
      BoundsCheckMem(access_size, index, offset, position, kNeedsBoundsCheck);
  Node* effective_address =
      graph()->NewNode(mcgraph()->machine()->IntAdd(), MemBuffer(offset),
                       Uint32ToUintptr(index));
  // Unlike regular memory accesses, unaligned memory accesses for atomic
  // operations should trap
  // Access sizes are in powers of two, calculate mod without using division
  Node* cond =
      graph()->NewNode(mcgraph()->machine()->WordAnd(), effective_address,
                       IntPtrConstant(access_size - 1));
  TrapIfFalse(wasm::kTrapUnalignedAccess,
              graph()->NewNode(mcgraph()->machine()->Word32Equal(), cond,
                               mcgraph()->Int32Constant(0)),
              position);
  return index;
}

Node* WasmGraphBuilder::BoundsCheckMem(uint8_t access_size, Node* index,
                                       uint32_t offset,
                                       wasm::WasmCodePosition position,
                                       EnforceBoundsCheck enforce_check) {
  DCHECK_LE(1, access_size);
  index = Uint32ToUintptr(index);
  if (FLAG_wasm_no_bounds_checks) return index;

  if (use_trap_handler() && enforce_check == kCanOmitBoundsCheck) {
    return index;
  }

  const bool statically_oob = access_size > env_->max_memory_size ||
                              offset > env_->max_memory_size - access_size;
  if (statically_oob) {
    // The access will be out of bounds, even for the largest memory.
    TrapIfEq32(wasm::kTrapMemOutOfBounds, Int32Constant(0), 0, position);
    return mcgraph()->IntPtrConstant(0);
  }
  uint64_t end_offset = uint64_t{offset} + access_size - 1u;
  Node* end_offset_node = IntPtrConstant(end_offset);

  // The accessed memory is [index + offset, index + end_offset].
  // Check that the last read byte (at {index + end_offset}) is in bounds.
  // 1) Check that {end_offset < mem_size}. This also ensures that we can safely
  //    compute {effective_size} as {mem_size - end_offset)}.
  //    {effective_size} is >= 1 if condition 1) holds.
  // 2) Check that {index + end_offset < mem_size} by
  //    - computing {effective_size} as {mem_size - end_offset} and
  //    - checking that {index < effective_size}.

  auto m = mcgraph()->machine();
  Node* mem_size = instance_cache_->mem_size;
  if (end_offset >= env_->min_memory_size) {
    // The end offset is larger than the smallest memory.
    // Dynamically check the end offset against the dynamic memory size.
    Node* cond = graph()->NewNode(m->UintLessThan(), end_offset_node, mem_size);
    TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
  } else {
    // The end offset is smaller than the smallest memory, so only one check is
    // required. Check to see if the index is also a constant.
    UintPtrMatcher match(index);
    if (match.HasValue()) {
      uintptr_t index_val = match.Value();
      if (index_val < env_->min_memory_size - end_offset) {
        // The input index is a constant and everything is statically within
        // bounds of the smallest possible memory.
        return index;
      }
    }
  }

  // This produces a positive number, since {end_offset < min_size <= mem_size}.
  Node* effective_size =
      graph()->NewNode(m->IntSub(), mem_size, end_offset_node);

  // Introduce the actual bounds check.
  Node* cond = graph()->NewNode(m->UintLessThan(), index, effective_size);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);

  if (untrusted_code_mitigations_) {
    // In the fallthrough case, condition the index with the memory mask.
    Node* mem_mask = instance_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index = graph()->NewNode(m->WordAnd(), index, mem_mask);
  }
  return index;
}

const Operator* WasmGraphBuilder::GetSafeLoadOperator(int offset,
                                                      wasm::ValueType type) {
  int alignment = offset % (wasm::ValueTypes::ElementSizeInBytes(type));
  MachineType mach_type = wasm::ValueTypes::MachineTypeFor(type);
  if (alignment == 0 || mcgraph()->machine()->UnalignedLoadSupported(
                            wasm::ValueTypes::MachineRepresentationFor(type))) {
    return mcgraph()->machine()->Load(mach_type);
  }
  return mcgraph()->machine()->UnalignedLoad(mach_type);
}

const Operator* WasmGraphBuilder::GetSafeStoreOperator(int offset,
                                                       wasm::ValueType type) {
  int alignment = offset % (wasm::ValueTypes::ElementSizeInBytes(type));
  MachineRepresentation rep = wasm::ValueTypes::MachineRepresentationFor(type);
  if (alignment == 0 || mcgraph()->machine()->UnalignedStoreSupported(rep)) {
    StoreRepresentation store_rep(rep, WriteBarrierKind::kNoWriteBarrier);
    return mcgraph()->machine()->Store(store_rep);
  }
  UnalignedStoreRepresentation store_rep(rep);
  return mcgraph()->machine()->UnalignedStore(store_rep);
}

Node* WasmGraphBuilder::TraceMemoryOperation(bool is_store,
                                             MachineRepresentation rep,
                                             Node* index, uint32_t offset,
                                             wasm::WasmCodePosition position) {
  int kAlign = 4;  // Ensure that the LSB is 0, such that this looks like a Smi.
  Node* info = graph()->NewNode(
      mcgraph()->machine()->StackSlot(sizeof(wasm::MemoryTracingInfo), kAlign));

  Node* address = graph()->NewNode(mcgraph()->machine()->Int32Add(),
                                   Int32Constant(offset), index);
  auto store = [&](int offset, MachineRepresentation rep, Node* data) {
    SetEffect(graph()->NewNode(
        mcgraph()->machine()->Store(StoreRepresentation(rep, kNoWriteBarrier)),
        info, mcgraph()->Int32Constant(offset), data, Effect(), Control()));
  };
  // Store address, is_store, and mem_rep.
  store(offsetof(wasm::MemoryTracingInfo, address),
        MachineRepresentation::kWord32, address);
  store(offsetof(wasm::MemoryTracingInfo, is_store),
        MachineRepresentation::kWord8,
        mcgraph()->Int32Constant(is_store ? 1 : 0));
  store(offsetof(wasm::MemoryTracingInfo, mem_rep),
        MachineRepresentation::kWord8,
        mcgraph()->Int32Constant(static_cast<int>(rep)));

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
  index = BoundsCheckMem(wasm::ValueTypes::MemSize(memtype), index, offset,
                         position, kCanOmitBoundsCheck);

  if (memtype.representation() == MachineRepresentation::kWord8 ||
      mcgraph()->machine()->UnalignedLoadSupported(memtype.representation())) {
    if (use_trap_handler()) {
      load = graph()->NewNode(mcgraph()->machine()->ProtectedLoad(memtype),
                              MemBuffer(offset), index, Effect(), Control());
      SetSourcePosition(load, position);
    } else {
      load = graph()->NewNode(mcgraph()->machine()->Load(memtype),
                              MemBuffer(offset), index, Effect(), Control());
    }
  } else {
    // TODO(eholk): Support unaligned loads with trap handlers.
    DCHECK(!use_trap_handler());
    load = graph()->NewNode(mcgraph()->machine()->UnalignedLoad(memtype),
                            MemBuffer(offset), index, Effect(), Control());
  }

  SetEffect(load);

#if defined(V8_TARGET_BIG_ENDIAN)
  load = BuildChangeEndiannessLoad(load, memtype, type);
#endif

  if (type == wasm::kWasmI64 &&
      ElementSizeInBytes(memtype.representation()) < 8) {
    // TODO(titzer): TF zeroes the upper bits of 64-bit loads for subword sizes.
    if (memtype.IsSigned()) {
      // sign extend
      load = graph()->NewNode(mcgraph()->machine()->ChangeInt32ToInt64(), load);
    } else {
      // zero extend
      load =
          graph()->NewNode(mcgraph()->machine()->ChangeUint32ToUint64(), load);
    }
  }

  if (FLAG_trace_wasm_memory) {
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

  index = BoundsCheckMem(i::ElementSizeInBytes(mem_rep), index, offset,
                         position, kCanOmitBoundsCheck);

#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndiannessStore(val, mem_rep, type);
#endif

  if (mem_rep == MachineRepresentation::kWord8 ||
      mcgraph()->machine()->UnalignedStoreSupported(mem_rep)) {
    if (use_trap_handler()) {
      store =
          graph()->NewNode(mcgraph()->machine()->ProtectedStore(mem_rep),
                           MemBuffer(offset), index, val, Effect(), Control());
      SetSourcePosition(store, position);
    } else {
      StoreRepresentation rep(mem_rep, kNoWriteBarrier);
      store =
          graph()->NewNode(mcgraph()->machine()->Store(rep), MemBuffer(offset),
                           index, val, Effect(), Control());
    }
  } else {
    // TODO(eholk): Support unaligned stores with trap handlers.
    DCHECK(!use_trap_handler());
    UnalignedStoreRepresentation rep(mem_rep);
    store =
        graph()->NewNode(mcgraph()->machine()->UnalignedStore(rep),
                         MemBuffer(offset), index, val, Effect(), Control());
  }

  SetEffect(store);

  if (FLAG_trace_wasm_memory) {
    TraceMemoryOperation(true, mem_rep, index, offset, position);
  }

  return store;
}

namespace {
Node* GetAsmJsOOBValue(MachineRepresentation rep, MachineGraph* mcgraph) {
  switch (rep) {
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      return mcgraph->Int32Constant(0);
    case MachineRepresentation::kWord64:
      return mcgraph->Int64Constant(0);
    case MachineRepresentation::kFloat32:
      return mcgraph->Float32Constant(std::numeric_limits<float>::quiet_NaN());
    case MachineRepresentation::kFloat64:
      return mcgraph->Float64Constant(std::numeric_limits<double>::quiet_NaN());
    default:
      UNREACHABLE();
  }
}
}  // namespace

Node* WasmGraphBuilder::BuildAsmjsLoadMem(MachineType type, Node* index) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_start);
  DCHECK_NOT_NULL(mem_size);

  // Asm.js semantics are defined in terms of typed arrays, hence OOB
  // reads return {undefined} coerced to the result type (0 for integers, NaN
  // for float and double).
  // Note that we check against the memory size ignoring the size of the
  // stored value, which is conservative if misaligned. Technically, asm.js
  // should never have misaligned accesses.
  index = Uint32ToUintptr(index);
  Diamond bounds_check(
      graph(), mcgraph()->common(),
      graph()->NewNode(mcgraph()->machine()->UintLessThan(), index, mem_size),
      BranchHint::kTrue);
  bounds_check.Chain(Control());

  if (untrusted_code_mitigations_) {
    // Condition the index with the memory mask.
    Node* mem_mask = instance_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index = graph()->NewNode(mcgraph()->machine()->WordAnd(), index, mem_mask);
  }

  Node* load = graph()->NewNode(mcgraph()->machine()->Load(type), mem_start,
                                index, Effect(), bounds_check.if_true);
  SetEffect(bounds_check.EffectPhi(load, Effect()));
  SetControl(bounds_check.merge);
  return bounds_check.Phi(type.representation(), load,
                          GetAsmJsOOBValue(type.representation(), mcgraph()));
}

Node* WasmGraphBuilder::Uint32ToUintptr(Node* node) {
  if (mcgraph()->machine()->Is32()) return node;
  // Fold instances of ChangeUint32ToUint64(IntConstant) directly.
  Uint32Matcher matcher(node);
  if (matcher.HasValue()) {
    uintptr_t value = matcher.Value();
    return mcgraph()->IntPtrConstant(bit_cast<intptr_t>(value));
  }
  return graph()->NewNode(mcgraph()->machine()->ChangeUint32ToUint64(), node);
}

Node* WasmGraphBuilder::BuildAsmjsStoreMem(MachineType type, Node* index,
                                           Node* val) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_start);
  DCHECK_NOT_NULL(mem_size);

  // Asm.js semantics are to ignore OOB writes.
  // Note that we check against the memory size ignoring the size of the
  // stored value, which is conservative if misaligned. Technically, asm.js
  // should never have misaligned accesses.
  Diamond bounds_check(
      graph(), mcgraph()->common(),
      graph()->NewNode(mcgraph()->machine()->Uint32LessThan(), index, mem_size),
      BranchHint::kTrue);
  bounds_check.Chain(Control());

  if (untrusted_code_mitigations_) {
    // Condition the index with the memory mask.
    Node* mem_mask = instance_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index =
        graph()->NewNode(mcgraph()->machine()->Word32And(), index, mem_mask);
  }

  index = Uint32ToUintptr(index);
  const Operator* store_op = mcgraph()->machine()->Store(StoreRepresentation(
      type.representation(), WriteBarrierKind::kNoWriteBarrier));
  Node* store = graph()->NewNode(store_op, mem_start, index, val, Effect(),
                                 bounds_check.if_true);
  SetEffect(bounds_check.EffectPhi(store, Effect()));
  SetControl(bounds_check.merge);
  return val;
}

void WasmGraphBuilder::PrintDebugName(Node* node) {
  PrintF("#%d:%s", node->id(), node->op()->mnemonic());
}

Graph* WasmGraphBuilder::graph() { return mcgraph()->graph(); }

namespace {
Signature<MachineRepresentation>* CreateMachineSignature(
    Zone* zone, wasm::FunctionSig* sig) {
  Signature<MachineRepresentation>::Builder builder(zone, sig->return_count(),
                                                    sig->parameter_count());
  for (auto ret : sig->returns()) {
    builder.AddReturn(wasm::ValueTypes::MachineRepresentationFor(ret));
  }

  for (auto param : sig->parameters()) {
    builder.AddParam(wasm::ValueTypes::MachineRepresentationFor(param));
  }
  return builder.Build();
}
}  // namespace

void WasmGraphBuilder::LowerInt64() {
  if (mcgraph()->machine()->Is64()) return;
  Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(), mcgraph()->common(),
                  mcgraph()->zone(),
                  CreateMachineSignature(mcgraph()->zone(), sig_));
  r.LowerGraph();
}

void WasmGraphBuilder::SimdScalarLoweringForTesting() {
  SimdScalarLowering(mcgraph(), CreateMachineSignature(mcgraph()->zone(), sig_))
      .LowerGraph();
}

void WasmGraphBuilder::SetSourcePosition(Node* node,
                                         wasm::WasmCodePosition position) {
  DCHECK_NE(position, wasm::kNoCodePosition);
  if (source_position_table_)
    source_position_table_->SetSourcePosition(node, SourcePosition(position));
}

Node* WasmGraphBuilder::S128Zero() {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->S128Zero());
}

Node* WasmGraphBuilder::SimdOp(wasm::WasmOpcode opcode, Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF32x4Splat:
      return graph()->NewNode(mcgraph()->machine()->F32x4Splat(), inputs[0]);
    case wasm::kExprF32x4SConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->F32x4SConvertI32x4(),
                              inputs[0]);
    case wasm::kExprF32x4UConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->F32x4UConvertI32x4(),
                              inputs[0]);
    case wasm::kExprF32x4Abs:
      return graph()->NewNode(mcgraph()->machine()->F32x4Abs(), inputs[0]);
    case wasm::kExprF32x4Neg:
      return graph()->NewNode(mcgraph()->machine()->F32x4Neg(), inputs[0]);
    case wasm::kExprF32x4RecipApprox:
      return graph()->NewNode(mcgraph()->machine()->F32x4RecipApprox(),
                              inputs[0]);
    case wasm::kExprF32x4RecipSqrtApprox:
      return graph()->NewNode(mcgraph()->machine()->F32x4RecipSqrtApprox(),
                              inputs[0]);
    case wasm::kExprF32x4Add:
      return graph()->NewNode(mcgraph()->machine()->F32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4AddHoriz:
      return graph()->NewNode(mcgraph()->machine()->F32x4AddHoriz(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Sub:
      return graph()->NewNode(mcgraph()->machine()->F32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Mul:
      return graph()->NewNode(mcgraph()->machine()->F32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Min:
      return graph()->NewNode(mcgraph()->machine()->F32x4Min(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Max:
      return graph()->NewNode(mcgraph()->machine()->F32x4Max(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Eq:
      return graph()->NewNode(mcgraph()->machine()->F32x4Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Ne:
      return graph()->NewNode(mcgraph()->machine()->F32x4Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Lt:
      return graph()->NewNode(mcgraph()->machine()->F32x4Lt(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Le:
      return graph()->NewNode(mcgraph()->machine()->F32x4Le(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Gt:
      return graph()->NewNode(mcgraph()->machine()->F32x4Lt(), inputs[1],
                              inputs[0]);
    case wasm::kExprF32x4Ge:
      return graph()->NewNode(mcgraph()->machine()->F32x4Le(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4Splat:
      return graph()->NewNode(mcgraph()->machine()->I32x4Splat(), inputs[0]);
    case wasm::kExprI32x4SConvertF32x4:
      return graph()->NewNode(mcgraph()->machine()->I32x4SConvertF32x4(),
                              inputs[0]);
    case wasm::kExprI32x4UConvertF32x4:
      return graph()->NewNode(mcgraph()->machine()->I32x4UConvertF32x4(),
                              inputs[0]);
    case wasm::kExprI32x4SConvertI16x8Low:
      return graph()->NewNode(mcgraph()->machine()->I32x4SConvertI16x8Low(),
                              inputs[0]);
    case wasm::kExprI32x4SConvertI16x8High:
      return graph()->NewNode(mcgraph()->machine()->I32x4SConvertI16x8High(),
                              inputs[0]);
    case wasm::kExprI32x4Neg:
      return graph()->NewNode(mcgraph()->machine()->I32x4Neg(), inputs[0]);
    case wasm::kExprI32x4Add:
      return graph()->NewNode(mcgraph()->machine()->I32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4AddHoriz:
      return graph()->NewNode(mcgraph()->machine()->I32x4AddHoriz(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Sub:
      return graph()->NewNode(mcgraph()->machine()->I32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Mul:
      return graph()->NewNode(mcgraph()->machine()->I32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MinS:
      return graph()->NewNode(mcgraph()->machine()->I32x4MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MaxS:
      return graph()->NewNode(mcgraph()->machine()->I32x4MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Eq:
      return graph()->NewNode(mcgraph()->machine()->I32x4Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Ne:
      return graph()->NewNode(mcgraph()->machine()->I32x4Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4LtS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4LeS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4GtS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4GeS:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4UConvertI16x8Low:
      return graph()->NewNode(mcgraph()->machine()->I32x4UConvertI16x8Low(),
                              inputs[0]);
    case wasm::kExprI32x4UConvertI16x8High:
      return graph()->NewNode(mcgraph()->machine()->I32x4UConvertI16x8High(),
                              inputs[0]);
    case wasm::kExprI32x4MinU:
      return graph()->NewNode(mcgraph()->machine()->I32x4MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4MaxU:
      return graph()->NewNode(mcgraph()->machine()->I32x4MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4LtU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4LeU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI32x4GtU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4GeU:
      return graph()->NewNode(mcgraph()->machine()->I32x4GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Splat:
      return graph()->NewNode(mcgraph()->machine()->I16x8Splat(), inputs[0]);
    case wasm::kExprI16x8SConvertI8x16Low:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8SConvertI8x16High:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8Neg:
      return graph()->NewNode(mcgraph()->machine()->I16x8Neg(), inputs[0]);
    case wasm::kExprI16x8SConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Add:
      return graph()->NewNode(mcgraph()->machine()->I16x8Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8AddSaturateS:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8AddHoriz:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddHoriz(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Sub:
      return graph()->NewNode(mcgraph()->machine()->I16x8Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8SubSaturateS:
      return graph()->NewNode(mcgraph()->machine()->I16x8SubSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Mul:
      return graph()->NewNode(mcgraph()->machine()->I16x8Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MinS:
      return graph()->NewNode(mcgraph()->machine()->I16x8MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MaxS:
      return graph()->NewNode(mcgraph()->machine()->I16x8MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Eq:
      return graph()->NewNode(mcgraph()->machine()->I16x8Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Ne:
      return graph()->NewNode(mcgraph()->machine()->I16x8Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8LtS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8LeS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8GtS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8GeS:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8UConvertI8x16Low:
      return graph()->NewNode(mcgraph()->machine()->I16x8UConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8UConvertI8x16High:
      return graph()->NewNode(mcgraph()->machine()->I16x8UConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8UConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->I16x8UConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8AddSaturateU:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8SubSaturateU:
      return graph()->NewNode(mcgraph()->machine()->I16x8SubSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8MinU:
      return graph()->NewNode(mcgraph()->machine()->I16x8MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8MaxU:
      return graph()->NewNode(mcgraph()->machine()->I16x8MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8LtU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8LeU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI16x8GtU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8GeU:
      return graph()->NewNode(mcgraph()->machine()->I16x8GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Splat:
      return graph()->NewNode(mcgraph()->machine()->I8x16Splat(), inputs[0]);
    case wasm::kExprI8x16Neg:
      return graph()->NewNode(mcgraph()->machine()->I8x16Neg(), inputs[0]);
    case wasm::kExprI8x16SConvertI16x8:
      return graph()->NewNode(mcgraph()->machine()->I8x16SConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Add:
      return graph()->NewNode(mcgraph()->machine()->I8x16Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16AddSaturateS:
      return graph()->NewNode(mcgraph()->machine()->I8x16AddSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Sub:
      return graph()->NewNode(mcgraph()->machine()->I8x16Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SubSaturateS:
      return graph()->NewNode(mcgraph()->machine()->I8x16SubSaturateS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Mul:
      return graph()->NewNode(mcgraph()->machine()->I8x16Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MinS:
      return graph()->NewNode(mcgraph()->machine()->I8x16MinS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MaxS:
      return graph()->NewNode(mcgraph()->machine()->I8x16MaxS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Eq:
      return graph()->NewNode(mcgraph()->machine()->I8x16Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Ne:
      return graph()->NewNode(mcgraph()->machine()->I8x16Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16LtS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16LeS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16GtS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16GeS:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16UConvertI16x8:
      return graph()->NewNode(mcgraph()->machine()->I8x16UConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16AddSaturateU:
      return graph()->NewNode(mcgraph()->machine()->I8x16AddSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16SubSaturateU:
      return graph()->NewNode(mcgraph()->machine()->I8x16SubSaturateU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16MinU:
      return graph()->NewNode(mcgraph()->machine()->I8x16MinU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16MaxU:
      return graph()->NewNode(mcgraph()->machine()->I8x16MaxU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16LtU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16LeU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeU(), inputs[1],
                              inputs[0]);
    case wasm::kExprI8x16GtU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GtU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16GeU:
      return graph()->NewNode(mcgraph()->machine()->I8x16GeU(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128And:
      return graph()->NewNode(mcgraph()->machine()->S128And(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Or:
      return graph()->NewNode(mcgraph()->machine()->S128Or(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Xor:
      return graph()->NewNode(mcgraph()->machine()->S128Xor(), inputs[0],
                              inputs[1]);
    case wasm::kExprS128Not:
      return graph()->NewNode(mcgraph()->machine()->S128Not(), inputs[0]);
    case wasm::kExprS128Select:
      return graph()->NewNode(mcgraph()->machine()->S128Select(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprS1x4AnyTrue:
      return graph()->NewNode(mcgraph()->machine()->S1x4AnyTrue(), inputs[0]);
    case wasm::kExprS1x4AllTrue:
      return graph()->NewNode(mcgraph()->machine()->S1x4AllTrue(), inputs[0]);
    case wasm::kExprS1x8AnyTrue:
      return graph()->NewNode(mcgraph()->machine()->S1x8AnyTrue(), inputs[0]);
    case wasm::kExprS1x8AllTrue:
      return graph()->NewNode(mcgraph()->machine()->S1x8AllTrue(), inputs[0]);
    case wasm::kExprS1x16AnyTrue:
      return graph()->NewNode(mcgraph()->machine()->S1x16AnyTrue(), inputs[0]);
    case wasm::kExprS1x16AllTrue:
      return graph()->NewNode(mcgraph()->machine()->S1x16AllTrue(), inputs[0]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane,
                                   Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF32x4ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->F32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF32x4ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->F32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI32x4ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI16x8ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I16x8ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I8x16ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI8x16ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I8x16ReplaceLane(lane),
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
      return graph()->NewNode(mcgraph()->machine()->I32x4Shl(shift), inputs[0]);
    case wasm::kExprI32x4ShrS:
      return graph()->NewNode(mcgraph()->machine()->I32x4ShrS(shift),
                              inputs[0]);
    case wasm::kExprI32x4ShrU:
      return graph()->NewNode(mcgraph()->machine()->I32x4ShrU(shift),
                              inputs[0]);
    case wasm::kExprI16x8Shl:
      return graph()->NewNode(mcgraph()->machine()->I16x8Shl(shift), inputs[0]);
    case wasm::kExprI16x8ShrS:
      return graph()->NewNode(mcgraph()->machine()->I16x8ShrS(shift),
                              inputs[0]);
    case wasm::kExprI16x8ShrU:
      return graph()->NewNode(mcgraph()->machine()->I16x8ShrU(shift),
                              inputs[0]);
    case wasm::kExprI8x16Shl:
      return graph()->NewNode(mcgraph()->machine()->I8x16Shl(shift), inputs[0]);
    case wasm::kExprI8x16ShrS:
      return graph()->NewNode(mcgraph()->machine()->I8x16ShrS(shift),
                              inputs[0]);
    case wasm::kExprI8x16ShrU:
      return graph()->NewNode(mcgraph()->machine()->I8x16ShrU(shift),
                              inputs[0]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::Simd8x16ShuffleOp(const uint8_t shuffle[16],
                                          Node* const* inputs) {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->S8x16Shuffle(shuffle),
                          inputs[0], inputs[1]);
}

#define ATOMIC_BINOP_LIST(V)                        \
  V(I32AtomicAdd, Add, Uint32, Word32)              \
  V(I64AtomicAdd, Add, Uint64, Word64)              \
  V(I32AtomicAdd8U, Add, Uint8, Word32)             \
  V(I32AtomicAdd16U, Add, Uint16, Word32)           \
  V(I64AtomicAdd8U, Add, Uint8, Word64)             \
  V(I64AtomicAdd16U, Add, Uint16, Word64)           \
  V(I64AtomicAdd32U, Add, Uint32, Word64)           \
  V(I32AtomicSub, Sub, Uint32, Word32)              \
  V(I64AtomicSub, Sub, Uint64, Word64)              \
  V(I32AtomicSub8U, Sub, Uint8, Word32)             \
  V(I32AtomicSub16U, Sub, Uint16, Word32)           \
  V(I64AtomicSub8U, Sub, Uint8, Word64)             \
  V(I64AtomicSub16U, Sub, Uint16, Word64)           \
  V(I64AtomicSub32U, Sub, Uint32, Word64)           \
  V(I32AtomicAnd, And, Uint32, Word32)              \
  V(I64AtomicAnd, And, Uint64, Word64)              \
  V(I32AtomicAnd8U, And, Uint8, Word32)             \
  V(I64AtomicAnd16U, And, Uint16, Word64)           \
  V(I32AtomicAnd16U, And, Uint16, Word32)           \
  V(I64AtomicAnd8U, And, Uint8, Word64)             \
  V(I64AtomicAnd32U, And, Uint32, Word64)           \
  V(I32AtomicOr, Or, Uint32, Word32)                \
  V(I64AtomicOr, Or, Uint64, Word64)                \
  V(I32AtomicOr8U, Or, Uint8, Word32)               \
  V(I32AtomicOr16U, Or, Uint16, Word32)             \
  V(I64AtomicOr8U, Or, Uint8, Word64)               \
  V(I64AtomicOr16U, Or, Uint16, Word64)             \
  V(I64AtomicOr32U, Or, Uint32, Word64)             \
  V(I32AtomicXor, Xor, Uint32, Word32)              \
  V(I64AtomicXor, Xor, Uint64, Word64)              \
  V(I32AtomicXor8U, Xor, Uint8, Word32)             \
  V(I32AtomicXor16U, Xor, Uint16, Word32)           \
  V(I64AtomicXor8U, Xor, Uint8, Word64)             \
  V(I64AtomicXor16U, Xor, Uint16, Word64)           \
  V(I64AtomicXor32U, Xor, Uint32, Word64)           \
  V(I32AtomicExchange, Exchange, Uint32, Word32)    \
  V(I64AtomicExchange, Exchange, Uint64, Word64)    \
  V(I32AtomicExchange8U, Exchange, Uint8, Word32)   \
  V(I32AtomicExchange16U, Exchange, Uint16, Word32) \
  V(I64AtomicExchange8U, Exchange, Uint8, Word64)   \
  V(I64AtomicExchange16U, Exchange, Uint16, Word64) \
  V(I64AtomicExchange32U, Exchange, Uint32, Word64)

#define ATOMIC_CMP_EXCHG_LIST(V)                 \
  V(I32AtomicCompareExchange, Uint32, Word32)    \
  V(I64AtomicCompareExchange, Uint64, Word64)    \
  V(I32AtomicCompareExchange8U, Uint8, Word32)   \
  V(I32AtomicCompareExchange16U, Uint16, Word32) \
  V(I64AtomicCompareExchange8U, Uint8, Word64)   \
  V(I64AtomicCompareExchange16U, Uint16, Word64) \
  V(I64AtomicCompareExchange32U, Uint32, Word64)

#define ATOMIC_LOAD_LIST(V)           \
  V(I32AtomicLoad, Uint32, Word32)    \
  V(I64AtomicLoad, Uint64, Word64)    \
  V(I32AtomicLoad8U, Uint8, Word32)   \
  V(I32AtomicLoad16U, Uint16, Word32) \
  V(I64AtomicLoad8U, Uint8, Word64)   \
  V(I64AtomicLoad16U, Uint16, Word64) \
  V(I64AtomicLoad32U, Uint32, Word64)

#define ATOMIC_STORE_LIST(V)                    \
  V(I32AtomicStore, Uint32, kWord32, Word32)    \
  V(I64AtomicStore, Uint64, kWord64, Word64)    \
  V(I32AtomicStore8U, Uint8, kWord8, Word32)    \
  V(I32AtomicStore16U, Uint16, kWord16, Word32) \
  V(I64AtomicStore8U, Uint8, kWord8, Word64)    \
  V(I64AtomicStore16U, Uint16, kWord16, Word64) \
  V(I64AtomicStore32U, Uint32, kWord32, Word64)

Node* WasmGraphBuilder::AtomicOp(wasm::WasmOpcode opcode, Node* const* inputs,
                                 uint32_t alignment, uint32_t offset,
                                 wasm::WasmCodePosition position) {
  Node* node;
  switch (opcode) {
#define BUILD_ATOMIC_BINOP(Name, Operation, Type, Prefix)                     \
  case wasm::kExpr##Name: {                                                   \
    Node* index = CheckBoundsAndAlignment(                                    \
        wasm::ValueTypes::MemSize(MachineType::Type()), inputs[0], offset,    \
        position);                                                            \
    node = graph()->NewNode(                                                  \
        mcgraph()->machine()->Prefix##Atomic##Operation(MachineType::Type()), \
        MemBuffer(offset), index, inputs[1], Effect(), Control());            \
    break;                                                                    \
  }
    ATOMIC_BINOP_LIST(BUILD_ATOMIC_BINOP)
#undef BUILD_ATOMIC_BINOP

#define BUILD_ATOMIC_CMP_EXCHG(Name, Type, Prefix)                            \
  case wasm::kExpr##Name: {                                                   \
    Node* index = CheckBoundsAndAlignment(                                    \
        wasm::ValueTypes::MemSize(MachineType::Type()), inputs[0], offset,    \
        position);                                                            \
    node = graph()->NewNode(                                                  \
        mcgraph()->machine()->Prefix##AtomicCompareExchange(                  \
            MachineType::Type()),                                             \
        MemBuffer(offset), index, inputs[1], inputs[2], Effect(), Control()); \
    break;                                                                    \
  }
    ATOMIC_CMP_EXCHG_LIST(BUILD_ATOMIC_CMP_EXCHG)
#undef BUILD_ATOMIC_CMP_EXCHG

#define BUILD_ATOMIC_LOAD_OP(Name, Type, Prefix)                           \
  case wasm::kExpr##Name: {                                                \
    Node* index = CheckBoundsAndAlignment(                                 \
        wasm::ValueTypes::MemSize(MachineType::Type()), inputs[0], offset, \
        position);                                                         \
    node = graph()->NewNode(                                               \
        mcgraph()->machine()->Prefix##AtomicLoad(MachineType::Type()),     \
        MemBuffer(offset), index, Effect(), Control());                    \
    break;                                                                 \
  }
    ATOMIC_LOAD_LIST(BUILD_ATOMIC_LOAD_OP)
#undef BUILD_ATOMIC_LOAD_OP

#define BUILD_ATOMIC_STORE_OP(Name, Type, Rep, Prefix)                         \
  case wasm::kExpr##Name: {                                                    \
    Node* index = CheckBoundsAndAlignment(                                     \
        wasm::ValueTypes::MemSize(MachineType::Type()), inputs[0], offset,     \
        position);                                                             \
    node = graph()->NewNode(                                                   \
        mcgraph()->machine()->Prefix##AtomicStore(MachineRepresentation::Rep), \
        MemBuffer(offset), index, inputs[1], Effect(), Control());             \
    break;                                                                     \
  }
    ATOMIC_STORE_LIST(BUILD_ATOMIC_STORE_OP)
#undef BUILD_ATOMIC_STORE_OP
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
  return SetEffect(node);
}

#undef ATOMIC_BINOP_LIST
#undef ATOMIC_CMP_EXCHG_LIST
#undef ATOMIC_LOAD_LIST
#undef ATOMIC_STORE_LIST

class WasmDecorator final : public GraphDecorator {
 public:
  explicit WasmDecorator(NodeOriginTable* origins, wasm::Decoder* decoder)
      : origins_(origins), decoder_(decoder) {}

  void Decorate(Node* node) final {
    origins_->SetNodeOrigin(
        node, NodeOrigin("wasm graph creation", "n/a",
                         NodeOrigin::kWasmBytecode, decoder_->position()));
  }

 private:
  compiler::NodeOriginTable* origins_;
  wasm::Decoder* decoder_;
};

void WasmGraphBuilder::AddBytecodePositionDecorator(
    NodeOriginTable* node_origins, wasm::Decoder* decoder) {
  DCHECK_NULL(decorator_);
  decorator_ = new (graph()->zone()) WasmDecorator(node_origins, decoder);
  graph()->AddDecorator(decorator_);
}

void WasmGraphBuilder::RemoveBytecodePositionDecorator() {
  DCHECK_NOT_NULL(decorator_);
  graph()->RemoveDecorator(decorator_);
  decorator_ = nullptr;
}

namespace {
bool must_record_function_compilation(Isolate* isolate) {
  return isolate->logger()->is_listening_to_code_events() ||
         isolate->is_profiling();
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
  PROFILE(isolate, CodeCreateEvent(tag, AbstractCode::cast(*code), *name_str));
}

class WasmWrapperGraphBuilder : public WasmGraphBuilder {
 public:
  WasmWrapperGraphBuilder(Zone* zone, wasm::ModuleEnv* env, JSGraph* jsgraph,
                          wasm::FunctionSig* sig,
                          compiler::SourcePositionTable* spt,
                          StubCallMode stub_mode)
      : WasmGraphBuilder(env, zone, jsgraph, sig, spt),
        isolate_(jsgraph->isolate()),
        jsgraph_(jsgraph),
        stub_mode_(stub_mode) {}

  Node* BuildAllocateHeapNumberWithValue(Node* value, Node* control) {
    MachineOperatorBuilder* machine = mcgraph()->machine();
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = (stub_mode_ == StubCallMode::kCallWasmRuntimeStub)
                       ? mcgraph()->RelocatableIntPtrConstant(
                             wasm::WasmCode::kWasmAllocateHeapNumber,
                             RelocInfo::WASM_STUB_CALL)
                       : jsgraph()->HeapConstant(
                             BUILTIN_CODE(isolate_, AllocateHeapNumber));
    if (!allocate_heap_number_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), AllocateHeapNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoThrow, stub_mode_);
      allocate_heap_number_operator_.set(common->Call(call_descriptor));
    }
    Node* heap_number = graph()->NewNode(allocate_heap_number_operator_.get(),
                                         target, Effect(), control);
    SetEffect(
        graph()->NewNode(machine->Store(StoreRepresentation(
                             MachineRepresentation::kFloat64, kNoWriteBarrier)),
                         heap_number, BuildHeapNumberValueIndexConstant(),
                         value, heap_number, control));
    return heap_number;
  }

  Node* BuildChangeSmiToFloat64(Node* value) {
    return graph()->NewNode(mcgraph()->machine()->ChangeInt32ToFloat64(),
                            BuildChangeSmiToInt32(value));
  }

  Node* BuildTestHeapObject(Node* value) {
    return graph()->NewNode(mcgraph()->machine()->WordAnd(), value,
                            mcgraph()->IntPtrConstant(kHeapObjectTag));
  }

  Node* BuildLoadHeapNumberValue(Node* value) {
    return SetEffect(graph()->NewNode(
        mcgraph()->machine()->Load(MachineType::Float64()), value,
        BuildHeapNumberValueIndexConstant(), Effect(), Control()));
  }

  Node* BuildHeapNumberValueIndexConstant() {
    return mcgraph()->IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag);
  }

  Node* BuildChangeInt32ToTagged(Node* value) {
    MachineOperatorBuilder* machine = mcgraph()->machine();
    CommonOperatorBuilder* common = mcgraph()->common();

    if (SmiValuesAre32Bits()) {
      return BuildChangeInt32ToSmi(value);
    }
    DCHECK(SmiValuesAre31Bits());

    Node* effect = Effect();
    Node* control = Control();
    Node* add = graph()->NewNode(machine->Int32AddWithOverflow(), value, value,
                                 graph()->start());

    Node* ovf = graph()->NewNode(common->Projection(1), add, graph()->start());
    Node* branch =
        graph()->NewNode(common->Branch(BranchHint::kFalse), ovf, control);

    Node* if_true = graph()->NewNode(common->IfTrue(), branch);
    Node* vtrue = BuildAllocateHeapNumberWithValue(
        graph()->NewNode(machine->ChangeInt32ToFloat64(), value), if_true);
    Node* etrue = Effect();

    Node* if_false = graph()->NewNode(common->IfFalse(), branch);
    Node* vfalse = graph()->NewNode(common->Projection(0), add, if_false);
    vfalse = BuildChangeInt32ToIntPtr(vfalse);

    Node* merge =
        SetControl(graph()->NewNode(common->Merge(2), if_true, if_false));
    SetEffect(graph()->NewNode(common->EffectPhi(2), etrue, effect, merge));
    return graph()->NewNode(common->Phi(MachineRepresentation::kTagged, 2),
                            vtrue, vfalse, merge);
  }

  Node* BuildChangeFloat64ToTagged(Node* value) {
    MachineOperatorBuilder* machine = mcgraph()->machine();
    CommonOperatorBuilder* common = mcgraph()->common();

    // Check several conditions:
    //  i32?
    //  ├─ true: zero?
    //  │        ├─ true: negative?
    //  │        │        ├─ true: box
    //  │        │        └─ false: potentially Smi
    //  │        └─ false: potentially Smi
    //  └─ false: box
    // For potential Smi values, depending on whether Smis are 31 or 32 bit, we
    // still need to check whether the value fits in a Smi.

    Node* effect = Effect();
    Node* control = Control();
    Node* value32 = graph()->NewNode(machine->RoundFloat64ToInt32(), value);
    Node* check_i32 = graph()->NewNode(
        machine->Float64Equal(), value,
        graph()->NewNode(machine->ChangeInt32ToFloat64(), value32));
    Node* branch_i32 = graph()->NewNode(common->Branch(), check_i32, control);

    Node* if_i32 = graph()->NewNode(common->IfTrue(), branch_i32);
    Node* if_not_i32 = graph()->NewNode(common->IfFalse(), branch_i32);

    // We only need to check for -0 if the {value} can potentially contain -0.
    Node* check_zero = graph()->NewNode(machine->Word32Equal(), value32,
                                        mcgraph()->Int32Constant(0));
    Node* branch_zero = graph()->NewNode(common->Branch(BranchHint::kFalse),
                                         check_zero, if_i32);

    Node* if_zero = graph()->NewNode(common->IfTrue(), branch_zero);
    Node* if_not_zero = graph()->NewNode(common->IfFalse(), branch_zero);

    // In case of 0, we need to check the high bits for the IEEE -0 pattern.
    Node* check_negative = graph()->NewNode(
        machine->Int32LessThan(),
        graph()->NewNode(machine->Float64ExtractHighWord32(), value),
        mcgraph()->Int32Constant(0));
    Node* branch_negative = graph()->NewNode(common->Branch(BranchHint::kFalse),
                                             check_negative, if_zero);

    Node* if_negative = graph()->NewNode(common->IfTrue(), branch_negative);
    Node* if_not_negative =
        graph()->NewNode(common->IfFalse(), branch_negative);

    // We need to create a box for negative 0.
    Node* if_smi =
        graph()->NewNode(common->Merge(2), if_not_zero, if_not_negative);
    Node* if_box = graph()->NewNode(common->Merge(2), if_not_i32, if_negative);

    // On 64-bit machines we can just wrap the 32-bit integer in a smi, for
    // 32-bit machines we need to deal with potential overflow and fallback to
    // boxing.
    Node* vsmi;
    if (SmiValuesAre32Bits()) {
      vsmi = BuildChangeInt32ToSmi(value32);
    } else {
      DCHECK(SmiValuesAre31Bits());
      Node* smi_tag = graph()->NewNode(machine->Int32AddWithOverflow(), value32,
                                       value32, if_smi);

      Node* check_ovf =
          graph()->NewNode(common->Projection(1), smi_tag, if_smi);
      Node* branch_ovf = graph()->NewNode(common->Branch(BranchHint::kFalse),
                                          check_ovf, if_smi);

      Node* if_ovf = graph()->NewNode(common->IfTrue(), branch_ovf);
      if_box = graph()->NewNode(common->Merge(2), if_ovf, if_box);

      if_smi = graph()->NewNode(common->IfFalse(), branch_ovf);
      vsmi = graph()->NewNode(common->Projection(0), smi_tag, if_smi);
      vsmi = BuildChangeInt32ToIntPtr(vsmi);
    }

    // Allocate the box for the {value}.
    Node* vbox = BuildAllocateHeapNumberWithValue(value, if_box);
    Node* ebox = Effect();

    Node* merge =
        SetControl(graph()->NewNode(common->Merge(2), if_smi, if_box));
    SetEffect(graph()->NewNode(common->EffectPhi(2), effect, ebox, merge));
    return graph()->NewNode(common->Phi(MachineRepresentation::kTagged, 2),
                            vsmi, vbox, merge);
  }

  int AddArgumentNodes(Node** args, int pos, int param_count,
                       wasm::FunctionSig* sig) {
    // Convert wasm numbers to JS values.
    for (int i = 0; i < param_count; ++i) {
      Node* param =
          Param(i + 1);  // Start from index 1 to drop the instance_node.
      args[pos++] = ToJS(param, sig->GetParam(i));
    }
    return pos;
  }

  Node* BuildJavaScriptToNumber(Node* node, Node* js_context) {
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        mcgraph()->zone(), TypeConversionDescriptor{}, 0,
        CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
    Node* stub_code =
        (stub_mode_ == StubCallMode::kCallWasmRuntimeStub)
            ? mcgraph()->RelocatableIntPtrConstant(
                  wasm::WasmCode::kWasmToNumber, RelocInfo::WASM_STUB_CALL)
            : jsgraph()->HeapConstant(BUILTIN_CODE(isolate_, ToNumber));

    Node* result = SetEffect(
        graph()->NewNode(mcgraph()->common()->Call(call_descriptor), stub_code,
                         node, js_context, Effect(), Control()));

    SetSourcePosition(result, 1);

    return result;
  }

  Node* BuildChangeTaggedToFloat64(Node* value) {
    MachineOperatorBuilder* machine = mcgraph()->machine();
    CommonOperatorBuilder* common = mcgraph()->common();

    // Implement the following decision tree:
    //  heap object?
    //  ├─ true: undefined?
    //  │        ├─ true: f64 const
    //  │        └─ false: load heap number value
    //  └─ false: smi to float64

    Node* check_heap_object = BuildTestHeapObject(value);
    Diamond is_heap_object(graph(), common, check_heap_object,
                           BranchHint::kFalse);
    is_heap_object.Chain(Control());

    SetControl(is_heap_object.if_true);
    Node* orig_effect = Effect();

    Node* undefined_node =
        LOAD_INSTANCE_FIELD(UndefinedValue, MachineType::TaggedPointer());
    Node* check_undefined =
        graph()->NewNode(machine->WordEqual(), value, undefined_node);
    Node* effect_tagged = Effect();

    Diamond is_undefined(graph(), common, check_undefined, BranchHint::kFalse);
    is_undefined.Nest(is_heap_object, true);

    SetControl(is_undefined.if_false);
    Node* vheap_number = BuildLoadHeapNumberValue(value);
    Node* effect_undefined = Effect();

    SetControl(is_undefined.merge);
    Node* vundefined =
        mcgraph()->Float64Constant(std::numeric_limits<double>::quiet_NaN());
    Node* vtagged = is_undefined.Phi(MachineRepresentation::kFloat64,
                                     vundefined, vheap_number);

    effect_tagged = is_undefined.EffectPhi(effect_tagged, effect_undefined);

    // If input is Smi: just convert to float64.
    Node* vfrom_smi = BuildChangeSmiToFloat64(value);

    SetControl(is_heap_object.merge);
    SetEffect(is_heap_object.EffectPhi(effect_tagged, orig_effect));
    return is_heap_object.Phi(MachineRepresentation::kFloat64, vtagged,
                              vfrom_smi);
  }

  Node* ToJS(Node* node, wasm::ValueType type) {
    switch (type) {
      case wasm::kWasmI32:
        return BuildChangeInt32ToTagged(node);
      case wasm::kWasmS128:
      case wasm::kWasmI64:
        UNREACHABLE();
      case wasm::kWasmF32:
        node = graph()->NewNode(mcgraph()->machine()->ChangeFloat32ToFloat64(),
                                node);
        return BuildChangeFloat64ToTagged(node);
      case wasm::kWasmF64:
        return BuildChangeFloat64ToTagged(node);
      case wasm::kWasmAnyRef:
        return node;
      default:
        UNREACHABLE();
    }
  }

  Node* FromJS(Node* node, Node* js_context, wasm::ValueType type) {
    DCHECK_NE(wasm::kWasmStmt, type);

    // The parameter is of type AnyRef, we take it as is.
    if (type == wasm::kWasmAnyRef) {
      return node;
    }

    // Do a JavaScript ToNumber.
    Node* num = BuildJavaScriptToNumber(node, js_context);

    // Change representation.
    num = BuildChangeTaggedToFloat64(num);

    switch (type) {
      case wasm::kWasmI32: {
        num = graph()->NewNode(mcgraph()->machine()->TruncateFloat64ToWord32(),
                               num);
        break;
      }
      case wasm::kWasmS128:
      case wasm::kWasmI64:
        UNREACHABLE();
      case wasm::kWasmF32:
        num = graph()->NewNode(mcgraph()->machine()->TruncateFloat64ToFloat32(),
                               num);
        break;
      case wasm::kWasmF64:
        break;
      default:
        UNREACHABLE();
    }
    return num;
  }

  void BuildModifyThreadInWasmFlag(bool new_value) {
    if (!trap_handler::IsTrapHandlerEnabled()) return;
    Node* thread_in_wasm_flag_address_address =
        graph()->NewNode(mcgraph()->common()->ExternalConstant(
            ExternalReference::wasm_thread_in_wasm_flag_address_address(
                isolate_)));
    Node* thread_in_wasm_flag_address = SetEffect(graph()->NewNode(
        mcgraph()->machine()->Load(LoadRepresentation(MachineType::Pointer())),
        thread_in_wasm_flag_address_address, mcgraph()->Int32Constant(0),
        Effect(), Control()));
    SetEffect(graph()->NewNode(
        mcgraph()->machine()->Store(StoreRepresentation(
            MachineRepresentation::kWord32, kNoWriteBarrier)),
        thread_in_wasm_flag_address, mcgraph()->Int32Constant(0),
        mcgraph()->Int32Constant(new_value ? 1 : 0), Effect(), Control()));
  }

  Node* BuildLoadFunctionDataFromExportedFunction(Node* closure) {
    Node* shared = SetEffect(graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::AnyTagged()), closure,
        jsgraph()->Int32Constant(
            wasm::ObjectAccess::SharedFunctionInfoOffsetInTaggedJSFunction()),
        Effect(), Control()));
    return SetEffect(graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::AnyTagged()), shared,
        jsgraph()->Int32Constant(SharedFunctionInfo::kFunctionDataOffset -
                                 kHeapObjectTag),
        Effect(), Control()));
  }

  Node* BuildLoadInstanceFromExportedFunctionData(Node* function_data) {
    return SetEffect(graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::AnyTagged()), function_data,
        jsgraph()->Int32Constant(WasmExportedFunctionData::kInstanceOffset -
                                 kHeapObjectTag),
        Effect(), Control()));
  }

  Node* BuildLoadFunctionIndexFromExportedFunctionData(Node* function_data) {
    Node* function_index_smi = SetEffect(graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::AnyTagged()), function_data,
        jsgraph()->Int32Constant(
            WasmExportedFunctionData::kFunctionIndexOffset - kHeapObjectTag),
        Effect(), Control()));
    Node* function_index = BuildChangeSmiToInt32(function_index_smi);
    return function_index;
  }

  Node* BuildLoadJumpTableOffsetFromExportedFunctionData(Node* function_data) {
    Node* jump_table_offset_smi = SetEffect(graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::AnyTagged()), function_data,
        jsgraph()->Int32Constant(
            WasmExportedFunctionData::kJumpTableOffsetOffset - kHeapObjectTag),
        Effect(), Control()));
    Node* jump_table_offset = BuildChangeSmiToInt32(jump_table_offset_smi);
    return jump_table_offset;
  }

  void BuildJSToWasmWrapper(bool is_import) {
    const int wasm_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the JS parameter nodes.
    SetEffect(SetControl(Start(wasm_count + 5)));

    // Create the js_closure and js_context parameters.
    Node* js_closure =
        graph()->NewNode(jsgraph()->common()->Parameter(
                             Linkage::kJSCallClosureParamIndex, "%closure"),
                         graph()->start());
    Node* js_context = graph()->NewNode(
        mcgraph()->common()->Parameter(
            Linkage::GetJSCallContextParamIndex(wasm_count + 1), "%context"),
        graph()->start());

    // Create the instance_node node to pass as parameter. It is loaded from
    // an actual reference to an instance or a placeholder reference,
    // called {WasmExportedFunction} via the {WasmExportedFunctionData}
    // structure.
    Node* function_data = BuildLoadFunctionDataFromExportedFunction(js_closure);
    instance_node_.set(
        BuildLoadInstanceFromExportedFunctionData(function_data));

    if (!wasm::IsJSCompatibleSignature(sig_)) {
      // Throw a TypeError. Use the js_context of the calling javascript
      // function (passed as a parameter), such that the generated code is
      // js_context independent.
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowTypeError, js_context,
                                    nullptr, 0);
      Return(jsgraph()->SmiConstant(0));
      return;
    }

    const int args_count = wasm_count + 1;  // +1 for wasm_code.
    Node** args = Buffer(args_count);
    Node** rets;

    // Convert JS parameters to wasm numbers.
    for (int i = 0; i < wasm_count; ++i) {
      Node* param = Param(i + 1);
      Node* wasm_param = FromJS(param, js_context, sig_->GetParam(i));
      args[i + 1] = wasm_param;
    }

    // Set the ThreadInWasm flag before we do the actual call.
    BuildModifyThreadInWasmFlag(true);

    if (is_import) {
      // Call to an imported function.
      // Load function index from {WasmExportedFunctionData}.
      Node* function_index =
          BuildLoadFunctionIndexFromExportedFunctionData(function_data);
      BuildImportCall(sig_, args, &rets, wasm::kNoCodePosition, function_index);
    } else {
      // Call to a wasm function defined in this module.
      // The call target is the jump table slot for that function.
      Node* jump_table_start =
          LOAD_INSTANCE_FIELD(JumpTableStart, MachineType::Pointer());
      Node* jump_table_offset =
          BuildLoadJumpTableOffsetFromExportedFunctionData(function_data);
      Node* jump_table_slot = graph()->NewNode(
          mcgraph()->machine()->IntAdd(), jump_table_start, jump_table_offset);
      args[0] = jump_table_slot;

      BuildWasmCall(sig_, args, &rets, wasm::kNoCodePosition, nullptr,
                    kNoRetpoline);
    }

    // Clear the ThreadInWasm flag.
    BuildModifyThreadInWasmFlag(false);

    Node* jsval = sig_->return_count() == 0 ? jsgraph()->UndefinedConstant()
                                            : ToJS(rets[0], sig_->GetReturn());
    Return(jsval);
  }

  bool BuildWasmImportCallWrapper(WasmImportCallKind kind, int func_index) {
    int wasm_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the parameter nodes.
    SetEffect(SetControl(Start(wasm_count + 3)));

    // Create the instance_node from the passed parameter.
    instance_node_.set(Param(wasm::kWasmInstanceParameterIndex));

    Node* native_context =
        LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer());

    if (kind == WasmImportCallKind::kRuntimeTypeError) {
      // =======================================================================
      // === Runtime TypeError =================================================
      // =======================================================================
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowTypeError,
                                    native_context, nullptr, 0);
      // We don't need to return a value here, as the runtime call will not
      // return anyway (the c entry stub will trigger stack unwinding).
      ReturnVoid();
      return false;
    }

    Node* callables_node = LOAD_INSTANCE_FIELD(ImportedFunctionCallables,
                                               MachineType::TaggedPointer());
    Node* callable_node = LOAD_FIXED_ARRAY_SLOT(callables_node, func_index);
    Node* undefined_node =
        LOAD_INSTANCE_FIELD(UndefinedValue, MachineType::TaggedPointer());

    Node* call = nullptr;
    bool sloppy_receiver = true;

    BuildModifyThreadInWasmFlag(false);  // exiting WASM via call.

    switch (kind) {
      // =======================================================================
      // === JS Functions with matching arity ==================================
      // =======================================================================
      case WasmImportCallKind::kJSFunctionArityMatch:
        sloppy_receiver = false;
        V8_FALLTHROUGH;  // fallthru
      case WasmImportCallKind::kJSFunctionArityMatchSloppy: {
        Node** args = Buffer(wasm_count + 9);
        int pos = 0;
        Node* function_context = SetEffect(graph()->NewNode(
            mcgraph()->machine()->Load(MachineType::TaggedPointer()),
            callable_node,
            mcgraph()->Int32Constant(
                wasm::ObjectAccess::ContextOffsetInTaggedJSFunction()),
            Effect(), Control()));
        args[pos++] = callable_node;  // target callable.
        // Receiver.
        if (sloppy_receiver) {
          Node* global_proxy = LOAD_FIXED_ARRAY_SLOT(
              native_context, Context::GLOBAL_PROXY_INDEX);
          args[pos++] = global_proxy;
        } else {
          args[pos++] = undefined_node;
        }

        auto call_descriptor = Linkage::GetJSCallDescriptor(
            graph()->zone(), false, wasm_count + 1, CallDescriptor::kNoFlags);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(args, pos, wasm_count, sig_);

        args[pos++] = undefined_node;                        // new target
        args[pos++] = mcgraph()->Int32Constant(wasm_count);  // argument count
        args[pos++] = function_context;
        args[pos++] = Effect();
        args[pos++] = Control();

        call = graph()->NewNode(mcgraph()->common()->Call(call_descriptor), pos,
                                args);
        break;
      }
      // =======================================================================
      // === JS Functions with arguments adapter ===============================
      // =======================================================================
      case WasmImportCallKind::kJSFunctionArityMismatch:
        sloppy_receiver = false;
        V8_FALLTHROUGH;  // fallthru
      case WasmImportCallKind::kJSFunctionArityMismatchSloppy: {
        Node** args = Buffer(wasm_count + 9);
        int pos = 0;
        Node* function_context = SetEffect(graph()->NewNode(
            mcgraph()->machine()->Load(MachineType::TaggedPointer()),
            callable_node,
            mcgraph()->Int32Constant(
                wasm::ObjectAccess::ContextOffsetInTaggedJSFunction()),
            Effect(), Control()));
        args[pos++] =
            BuildLoadBuiltinFromInstance(Builtins::kArgumentsAdaptorTrampoline);
        args[pos++] = callable_node;                         // target callable
        args[pos++] = undefined_node;                        // new target
        args[pos++] = mcgraph()->Int32Constant(wasm_count);  // argument count

        // Load shared function info, and then the formal parameter count.
        Node* shared_function_info = SetEffect(graph()->NewNode(
            mcgraph()->machine()->Load(MachineType::TaggedPointer()),
            callable_node,
            mcgraph()->Int32Constant(
                wasm::ObjectAccess::
                    SharedFunctionInfoOffsetInTaggedJSFunction()),
            Effect(), Control()));
        Node* formal_param_count = SetEffect(graph()->NewNode(
            mcgraph()->machine()->Load(MachineType::Uint16()),
            shared_function_info,
            mcgraph()->Int32Constant(
                wasm::ObjectAccess::
                    FormalParameterCountOffsetInSharedFunctionInfo()),
            Effect(), Control()));
        args[pos++] = formal_param_count;

        // Receiver.
        if (sloppy_receiver) {
          Node* global_proxy = LOAD_FIXED_ARRAY_SLOT(
              native_context, Context::GLOBAL_PROXY_INDEX);
          args[pos++] = global_proxy;
        } else {
          args[pos++] = undefined_node;
        }

        auto call_descriptor = Linkage::GetStubCallDescriptor(
            mcgraph()->zone(), ArgumentsAdaptorDescriptor{}, 1 + wasm_count,
            CallDescriptor::kNoFlags, Operator::kNoProperties);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(args, pos, wasm_count, sig_);
        args[pos++] = function_context;
        args[pos++] = Effect();
        args[pos++] = Control();
        call = graph()->NewNode(mcgraph()->common()->Call(call_descriptor), pos,
                                args);
        break;
      }
      // =======================================================================
      // === General case of unknown callable ==================================
      // =======================================================================
      case WasmImportCallKind::kUseCallBuiltin: {
        Node** args = Buffer(wasm_count + 9);
        int pos = 0;
        args[pos++] = mcgraph()->RelocatableIntPtrConstant(
            wasm::WasmCode::kWasmCallJavaScript, RelocInfo::WASM_STUB_CALL);
        args[pos++] = callable_node;
        args[pos++] = mcgraph()->Int32Constant(wasm_count);  // argument count
        args[pos++] = undefined_node;                        // receiver

        auto call_descriptor = Linkage::GetStubCallDescriptor(
            graph()->zone(), CallTrampolineDescriptor{}, wasm_count + 1,
            CallDescriptor::kNoFlags, Operator::kNoProperties,
            StubCallMode::kCallWasmRuntimeStub);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(args, pos, wasm_count, sig_);

        // The native_context is sufficient here, because all kind of callables
        // which depend on the context provide their own context. The context
        // here is only needed if the target is a constructor to throw a
        // TypeError, if the target is a native function, or if the target is a
        // callable JSObject, which can only be constructed by the runtime.
        args[pos++] = native_context;
        args[pos++] = Effect();
        args[pos++] = Control();

        call = graph()->NewNode(mcgraph()->common()->Call(call_descriptor), pos,
                                args);
        break;
      }
      default:
        UNREACHABLE();
    }
    DCHECK_NOT_NULL(call);

    SetEffect(call);
    SetSourcePosition(call, 0);

    // Convert the return value back.
    Node* val = sig_->return_count() == 0
                    ? mcgraph()->Int32Constant(0)
                    : FromJS(call, native_context, sig_->GetReturn());

    BuildModifyThreadInWasmFlag(true);  // reentering WASM upon return.

    Return(val);
    return true;
  }

  void BuildWasmInterpreterEntry(uint32_t func_index) {
    int param_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the parameter nodes.
    SetEffect(SetControl(Start(param_count + 3)));

    // Create the instance_node from the passed parameter.
    instance_node_.set(Param(wasm::kWasmInstanceParameterIndex));

    // Compute size for the argument buffer.
    int args_size_bytes = 0;
    for (wasm::ValueType type : sig_->parameters()) {
      args_size_bytes += wasm::ValueTypes::ElementSizeInBytes(type);
    }

    // The return value is also passed via this buffer:
    DCHECK_GE(wasm::kV8MaxWasmFunctionReturns, sig_->return_count());
    // TODO(wasm): Handle multi-value returns.
    DCHECK_EQ(1, wasm::kV8MaxWasmFunctionReturns);
    int return_size_bytes =
        sig_->return_count() == 0
            ? 0
            : wasm::ValueTypes::ElementSizeInBytes(sig_->GetReturn());

    // Get a stack slot for the arguments.
    Node* arg_buffer =
        args_size_bytes == 0 && return_size_bytes == 0
            ? mcgraph()->IntPtrConstant(0)
            : graph()->NewNode(mcgraph()->machine()->StackSlot(
                  std::max(args_size_bytes, return_size_bytes), 8));

    // Now store all our arguments to the buffer.
    int offset = 0;

    for (int i = 0; i < param_count; ++i) {
      wasm::ValueType type = sig_->GetParam(i);
      // Start from the parameter with index 1 to drop the instance_node.
      SetEffect(graph()->NewNode(GetSafeStoreOperator(offset, type), arg_buffer,
                                 Int32Constant(offset), Param(i + 1), Effect(),
                                 Control()));
      offset += wasm::ValueTypes::ElementSizeInBytes(type);
    }
    DCHECK_EQ(args_size_bytes, offset);

    // We are passing the raw arg_buffer here. To the GC and other parts, it
    // looks like a Smi (lowest bit not set). In the runtime function however,
    // don't call Smi::value on it, but just cast it to a byte pointer.
    Node* parameters[] = {jsgraph()->SmiConstant(func_index), arg_buffer};
    BuildCallToRuntime(Runtime::kWasmRunInterpreter, parameters,
                       arraysize(parameters));

    // Read back the return value.
    if (sig_->return_count() == 0) {
      Return(Int32Constant(0));
    } else {
      // TODO(wasm): Implement multi-return.
      DCHECK_EQ(1, sig_->return_count());
      MachineType load_rep =
          wasm::ValueTypes::MachineTypeFor(sig_->GetReturn());
      Node* val = SetEffect(
          graph()->NewNode(mcgraph()->machine()->Load(load_rep), arg_buffer,
                           Int32Constant(0), Effect(), Control()));
      Return(val);
    }

    if (ContainsInt64(sig_)) LowerInt64();
  }

  void BuildCWasmEntry() {
    // Build the start and the JS parameter nodes.
    SetEffect(SetControl(Start(CWasmEntryParameters::kNumParameters + 5)));

    // Create parameter nodes (offset by 1 for the receiver parameter).
    Node* code_entry = Param(CWasmEntryParameters::kCodeEntry + 1);
    Node* instance_node = Param(CWasmEntryParameters::kWasmInstance + 1);
    Node* arg_buffer = Param(CWasmEntryParameters::kArgumentsBuffer + 1);

    int wasm_arg_count = static_cast<int>(sig_->parameter_count());
    int arg_count = wasm_arg_count + 4;  // code, instance_node, control, effect
    Node** args = Buffer(arg_count);

    int pos = 0;
    args[pos++] = code_entry;
    args[pos++] = instance_node;

    int offset = 0;
    for (wasm::ValueType type : sig_->parameters()) {
      Node* arg_load = SetEffect(
          graph()->NewNode(GetSafeLoadOperator(offset, type), arg_buffer,
                           Int32Constant(offset), Effect(), Control()));
      args[pos++] = arg_load;
      offset += wasm::ValueTypes::ElementSizeInBytes(type);
    }

    args[pos++] = Effect();
    args[pos++] = Control();
    DCHECK_EQ(arg_count, pos);

    // Call the wasm code.
    auto call_descriptor = GetWasmCallDescriptor(mcgraph()->zone(), sig_);

    Node* call = SetEffect(graph()->NewNode(
        mcgraph()->common()->Call(call_descriptor), arg_count, args));

    // Store the return value.
    DCHECK_GE(1, sig_->return_count());
    if (sig_->return_count() == 1) {
      StoreRepresentation store_rep(
          wasm::ValueTypes::MachineRepresentationFor(sig_->GetReturn()),
          kNoWriteBarrier);
      SetEffect(graph()->NewNode(mcgraph()->machine()->Store(store_rep),
                                 arg_buffer, Int32Constant(0), call, Effect(),
                                 Control()));
    }
    Return(jsgraph()->SmiConstant(0));

    if (mcgraph()->machine()->Is32() && ContainsInt64(sig_)) {
      MachineRepresentation sig_reps[] = {
          MachineRepresentation::kWord32,  // return value
          MachineRepresentation::kTagged,  // receiver
          MachineRepresentation::kTagged,  // arg0 (code)
          MachineRepresentation::kTagged   // arg1 (buffer)
      };
      Signature<MachineRepresentation> c_entry_sig(1, 2, sig_reps);
      Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(),
                      mcgraph()->common(), mcgraph()->zone(), &c_entry_sig);
      r.LowerGraph();
    }
  }

  JSGraph* jsgraph() { return jsgraph_; }

 private:
  Isolate* const isolate_;
  JSGraph* jsgraph_;
  StubCallMode stub_mode_;
  SetOncePointer<const Operator> allocate_heap_number_operator_;
};

void AppendSignature(char* buffer, size_t max_name_len,
                     wasm::FunctionSig* sig) {
  size_t name_len = strlen(buffer);
  auto append_name_char = [&](char c) {
    if (name_len + 1 < max_name_len) buffer[name_len++] = c;
  };
  for (wasm::ValueType t : sig->parameters()) {
    append_name_char(wasm::ValueTypes::ShortNameOf(t));
  }
  append_name_char(':');
  for (wasm::ValueType t : sig->returns()) {
    append_name_char(wasm::ValueTypes::ShortNameOf(t));
  }
  buffer[name_len] = '\0';
}

}  // namespace

MaybeHandle<Code> CompileJSToWasmWrapper(Isolate* isolate,
                                         wasm::FunctionSig* sig,
                                         bool is_import) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "CompileJSToWasmWrapper");
  //----------------------------------------------------------------------------
  // Create the Graph.
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

  wasm::ModuleEnv env(nullptr, wasm::kNoTrapHandler,
                      wasm::kRuntimeExceptionSupport);
  WasmWrapperGraphBuilder builder(&zone, &env, &jsgraph, sig, nullptr,
                                  StubCallMode::kCallOnHeapBuiltin);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildJSToWasmWrapper(is_import);

  //----------------------------------------------------------------------------
  // Run the compilation pipeline.
  //----------------------------------------------------------------------------
  static constexpr size_t kMaxNameLen = 128;
  char debug_name[kMaxNameLen] = "js_to_wasm:";
  AppendSignature(debug_name, kMaxNameLen, sig);

  // Schedule and compile to machine code.
  int params = static_cast<int>(sig->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      &zone, false, params + 1, CallDescriptor::kNoFlags);

  MaybeHandle<Code> maybe_code = Pipeline::GenerateCodeForWasmStub(
      isolate, incoming, &graph, Code::JS_TO_WASM_FUNCTION, debug_name,
      WasmAssemblerOptions());
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    return maybe_code;
  }
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble(debug_name, os);
  }
#endif

  if (must_record_function_compilation(isolate)) {
    RecordFunctionCompilation(CodeEventListener::STUB_TAG, isolate, code, "%s",
                              debug_name);
  }

  return code;
}

WasmImportCallKind GetWasmImportCallKind(Handle<JSReceiver> target,
                                         wasm::FunctionSig* expected_sig) {
  if (WasmExportedFunction::IsWasmExportedFunction(*target)) {
    auto imported_function = WasmExportedFunction::cast(*target);
    wasm::FunctionSig* imported_sig =
        imported_function->instance()
            ->module()
            ->functions[imported_function->function_index()]
            .sig;
    if (*imported_sig != *expected_sig) {
      return WasmImportCallKind::kLinkError;
    }
    return WasmImportCallKind::kWasmToWasm;
  }
  // Assuming we are calling to JS, check whether this would be a runtime error.
  if (!wasm::IsJSCompatibleSignature(expected_sig)) {
    return WasmImportCallKind::kRuntimeTypeError;
  }
  // For JavaScript calls, determine whether the target has an arity match
  // and whether it has a sloppy receiver.
  if (target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(target);
    if (IsClassConstructor(function->shared()->kind())) {
      // Class constructor will throw anyway.
      return WasmImportCallKind::kUseCallBuiltin;
    }
    bool sloppy = is_sloppy(function->shared()->language_mode()) &&
                  !function->shared()->native();
    if (function->shared()->internal_formal_parameter_count() ==
        expected_sig->parameter_count()) {
      return sloppy ? WasmImportCallKind::kJSFunctionArityMatchSloppy
                    : WasmImportCallKind::kJSFunctionArityMatch;
    }
    return sloppy ? WasmImportCallKind::kJSFunctionArityMismatchSloppy
                  : WasmImportCallKind::kJSFunctionArityMismatch;
  }
  // Unknown case. Use the call builtin.
  return WasmImportCallKind::kUseCallBuiltin;
}

MaybeHandle<Code> CompileWasmImportCallWrapper(
    Isolate* isolate, WasmImportCallKind kind, wasm::FunctionSig* sig,
    uint32_t index, wasm::ModuleOrigin origin,
    wasm::UseTrapHandler use_trap_handler) {
  DCHECK_NE(WasmImportCallKind::kLinkError, kind);
  DCHECK_NE(WasmImportCallKind::kWasmToWasm, kind);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "CompileWasmImportCallWrapper");
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

  wasm::ModuleEnv env(nullptr, use_trap_handler,
                      wasm::kRuntimeExceptionSupport);

  WasmWrapperGraphBuilder builder(&zone, &env, &jsgraph, sig,
                                  source_position_table,
                                  StubCallMode::kCallWasmRuntimeStub);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildWasmImportCallWrapper(kind, index);

  EmbeddedVector<char, 32> func_name;
  func_name.Truncate(SNPrintF(func_name, "wasm-to-js#%d", index));

  // Schedule and compile to machine code.
  CallDescriptor* incoming = GetWasmCallDescriptor(&zone, sig);
  if (machine.Is32()) {
    incoming = GetI32WasmCallDescriptor(&zone, incoming);
  }
  MaybeHandle<Code> maybe_code = Pipeline::GenerateCodeForWasmStub(
      isolate, incoming, &graph, Code::WASM_TO_JS_FUNCTION, func_name.start(),
      AssemblerOptions::Default(isolate), source_position_table);
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    return maybe_code;
  }
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble(func_name.start(), os);
  }
#endif

  if (must_record_function_compilation(isolate)) {
    RecordFunctionCompilation(CodeEventListener::STUB_TAG, isolate, code,
                              "%.*s", func_name.length(), func_name.start());
  }

  return code;
}

MaybeHandle<Code> CompileWasmInterpreterEntry(Isolate* isolate,
                                              uint32_t func_index,
                                              wasm::FunctionSig* sig) {
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

  WasmWrapperGraphBuilder builder(&zone, nullptr, &jsgraph, sig, nullptr,
                                  StubCallMode::kCallWasmRuntimeStub);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildWasmInterpreterEntry(func_index);

  // Schedule and compile to machine code.
  CallDescriptor* incoming = GetWasmCallDescriptor(&zone, sig);
  if (machine.Is32()) {
    incoming = GetI32WasmCallDescriptor(&zone, incoming);
  }

  EmbeddedVector<char, 32> func_name;
  func_name.Truncate(
      SNPrintF(func_name, "wasm-interpreter-entry#%d", func_index));

  MaybeHandle<Code> maybe_code = Pipeline::GenerateCodeForWasmStub(
      isolate, incoming, &graph, Code::WASM_INTERPRETER_ENTRY,
      func_name.start(), AssemblerOptions::Default(isolate));
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    return maybe_code;
  }
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble(func_name.start(), os);
  }
#endif

  if (must_record_function_compilation(isolate)) {
    RecordFunctionCompilation(CodeEventListener::STUB_TAG, isolate, code,
                              "%.*s", func_name.length(), func_name.start());
  }

  return maybe_code;
}

MaybeHandle<Code> CompileCWasmEntry(Isolate* isolate, wasm::FunctionSig* sig) {
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

  WasmWrapperGraphBuilder builder(&zone, nullptr, &jsgraph, sig, nullptr,
                                  StubCallMode::kCallOnHeapBuiltin);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildCWasmEntry();

  // Schedule and compile to machine code.
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      &zone, false, CWasmEntryParameters::kNumParameters + 1,
      CallDescriptor::kNoFlags);

  // Build a name in the form "c-wasm-entry:<params>:<returns>".
  static constexpr size_t kMaxNameLen = 128;
  char debug_name[kMaxNameLen] = "c-wasm-entry:";
  AppendSignature(debug_name, kMaxNameLen, sig);

  MaybeHandle<Code> maybe_code = Pipeline::GenerateCodeForWasmStub(
      isolate, incoming, &graph, Code::C_WASM_ENTRY, debug_name,
      AssemblerOptions::Default(isolate));
  Handle<Code> code;
  if (!maybe_code.ToHandle(&code)) {
    return maybe_code;
  }
#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_opt_code) {
    CodeTracer::Scope tracing_scope(isolate->GetCodeTracer());
    OFStream os(tracing_scope.file());
    code->Disassemble(debug_name, os);
  }
#endif

  return code;
}

TurbofanWasmCompilationUnit::TurbofanWasmCompilationUnit(
    wasm::WasmCompilationUnit* wasm_unit)
    : wasm_unit_(wasm_unit) {}

// Clears unique_ptrs, but (part of) the type is forward declared in the header.
TurbofanWasmCompilationUnit::~TurbofanWasmCompilationUnit() = default;

SourcePositionTable* TurbofanWasmCompilationUnit::BuildGraphForWasmFunction(
    wasm::WasmFeatures* detected, double* decode_ms, MachineGraph* mcgraph,
    NodeOriginTable* node_origins) {
  base::ElapsedTimer decode_timer;
  if (FLAG_trace_wasm_decode_time) {
    decode_timer.Start();
  }

  // Create a TF graph during decoding.
  SourcePositionTable* source_position_table =
      new (mcgraph->zone()) SourcePositionTable(mcgraph->graph());
  WasmGraphBuilder builder(wasm_unit_->env_, mcgraph->zone(), mcgraph,
                           wasm_unit_->func_body_.sig, source_position_table);
  graph_construction_result_ = wasm::BuildTFGraph(
      wasm_unit_->wasm_engine_->allocator(),
      wasm_unit_->native_module_->enabled_features(), wasm_unit_->env_->module,
      &builder, detected, wasm_unit_->func_body_, node_origins);
  if (graph_construction_result_.failed()) {
    if (FLAG_trace_wasm_compiler) {
      StdoutStream{} << "Compilation failed: "
                     << graph_construction_result_.error_msg() << std::endl;
    }
    return nullptr;
  }

  builder.LowerInt64();

  if (builder.has_simd() &&
      (!CpuFeatures::SupportsWasmSimd128() || wasm_unit_->env_->lower_simd)) {
    SimdScalarLowering(
        mcgraph,
        CreateMachineSignature(mcgraph->zone(), wasm_unit_->func_body_.sig))
        .LowerGraph();
  }

  if (wasm_unit_->func_index_ >= FLAG_trace_wasm_ast_start &&
      wasm_unit_->func_index_ < FLAG_trace_wasm_ast_end) {
    PrintRawWasmCode(wasm_unit_->wasm_engine_->allocator(),
                     wasm_unit_->func_body_, wasm_unit_->env_->module,
                     wasm::kPrintLocals);
  }
  if (FLAG_trace_wasm_decode_time) {
    *decode_ms = decode_timer.Elapsed().InMillisecondsF();
  }
  return source_position_table;
}

namespace {
Vector<const char> GetDebugName(Zone* zone, int index) {
  // TODO(herhut): Use name from module if available.
  constexpr int kBufferLength = 24;

  EmbeddedVector<char, kBufferLength> name_vector;
  int name_len = SNPrintF(name_vector, "wasm-function#%d", index);
  DCHECK(name_len > 0 && name_len < name_vector.length());

  char* index_name = zone->NewArray<char>(name_len);
  memcpy(index_name, name_vector.start(), name_len);
  return Vector<const char>(index_name, name_len);
}

}  // namespace

void TurbofanWasmCompilationUnit::ExecuteCompilation(
    wasm::WasmFeatures* detected) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm"),
               "ExecuteTurbofanCompilation");
  double decode_ms = 0;
  size_t node_count = 0;

  // Scope for the {graph_zone}.
  {
    Zone graph_zone(wasm_unit_->wasm_engine_->allocator(), ZONE_NAME);
    MachineGraph* mcgraph = new (&graph_zone)
        MachineGraph(new (&graph_zone) Graph(&graph_zone),
                     new (&graph_zone) CommonOperatorBuilder(&graph_zone),
                     new (&graph_zone) MachineOperatorBuilder(
                         &graph_zone, MachineType::PointerRepresentation(),
                         InstructionSelector::SupportedMachineOperatorFlags(),
                         InstructionSelector::AlignmentRequirements()));

    Zone compilation_zone(wasm_unit_->wasm_engine_->allocator(), ZONE_NAME);

    OptimizedCompilationInfo info(
        GetDebugName(&compilation_zone, wasm_unit_->func_index_),
        &compilation_zone, Code::WASM_FUNCTION);
    if (wasm_unit_->env_->runtime_exception_support) {
      info.SetWasmRuntimeExceptionSupport();
    }

    if (info.trace_turbo_json_enabled()) {
      TurboCfgFile tcf;
      tcf << AsC1VCompilation(&info);
    }

    NodeOriginTable* node_origins = info.trace_turbo_json_enabled()
                                        ? new (&graph_zone)
                                              NodeOriginTable(mcgraph->graph())
                                        : nullptr;
    SourcePositionTable* source_positions =
        BuildGraphForWasmFunction(detected, &decode_ms, mcgraph, node_origins);

    if (graph_construction_result_.failed()) {
      ok_ = false;
      return;
    }

    if (node_origins) {
      node_origins->AddDecorator();
    }

    base::ElapsedTimer pipeline_timer;
    if (FLAG_trace_wasm_decode_time) {
      node_count = mcgraph->graph()->NodeCount();
      pipeline_timer.Start();
    }

    // Run the compiler pipeline to generate machine code.
    auto call_descriptor =
        GetWasmCallDescriptor(&compilation_zone, wasm_unit_->func_body_.sig);
    if (mcgraph->machine()->Is32()) {
      call_descriptor =
          GetI32WasmCallDescriptor(&compilation_zone, call_descriptor);
    }

    std::unique_ptr<OptimizedCompilationJob> job(
        Pipeline::NewWasmCompilationJob(
            &info, wasm_unit_->wasm_engine_, mcgraph, call_descriptor,
            source_positions, node_origins, wasm_unit_->func_body_,
            const_cast<wasm::WasmModule*>(wasm_unit_->env_->module),
            wasm_unit_->native_module_, wasm_unit_->func_index_,
            wasm_unit_->env_->module->origin));
    ok_ = job->ExecuteJob() == CompilationJob::SUCCEEDED;
    // TODO(bradnelson): Improve histogram handling of size_t.
    wasm_unit_->counters_->wasm_compile_function_peak_memory_bytes()->AddSample(
        static_cast<int>(mcgraph->graph()->zone()->allocation_size()));

    if (FLAG_trace_wasm_decode_time) {
      double pipeline_ms = pipeline_timer.Elapsed().InMillisecondsF();
      PrintF(
          "wasm-compilation phase 1 ok: %u bytes, %0.3f ms decode, %zu nodes, "
          "%0.3f ms pipeline\n",
          static_cast<unsigned>(wasm_unit_->func_body_.end -
                                wasm_unit_->func_body_.start),
          decode_ms, node_count, pipeline_ms);
    }
    if (ok_) wasm_code_ = info.wasm_code();
  }
  if (ok_) wasm_unit_->native_module()->PublishCode(wasm_code_);
}

wasm::WasmCode* TurbofanWasmCompilationUnit::FinishCompilation(
    wasm::ErrorThrower* thrower) {
  if (!ok_) {
    if (graph_construction_result_.failed()) {
      // Add the function as another context for the exception. This is
      // user-visible, so use official format.
      EmbeddedVector<char, 128> message;
      wasm::ModuleWireBytes wire_bytes(
          wasm_unit_->native_module()->wire_bytes());
      wasm::WireBytesRef name_ref =
          wasm_unit_->native_module()->module()->LookupFunctionName(
              wire_bytes, wasm_unit_->func_index_);
      if (name_ref.is_set()) {
        wasm::WasmName name = wire_bytes.GetNameOrNull(name_ref);
        SNPrintF(message, "Compiling wasm function \"%.*s\" failed",
                 name.length(), name.start());
      } else {
        SNPrintF(message,
                 "Compiling wasm function \"wasm-function[%d]\" failed",
                 wasm_unit_->func_index_);
      }
      thrower->CompileFailed(message.start(), graph_construction_result_);
    }

    return nullptr;
  }
  return wasm_code_;
}

namespace {
// Helper for allocating either an GP or FP reg, or the next stack slot.
class LinkageLocationAllocator {
 public:
  template <size_t kNumGpRegs, size_t kNumFpRegs>
  constexpr LinkageLocationAllocator(const Register (&gp)[kNumGpRegs],
                                     const DoubleRegister (&fp)[kNumFpRegs])
      : allocator_(wasm::LinkageAllocator(gp, fp)) {}

  LinkageLocation Next(MachineRepresentation rep) {
    MachineType type = MachineType::TypeForRepresentation(rep);
    if (IsFloatingPoint(rep)) {
      if (allocator_.CanAllocateFP(rep)) {
        int reg_code = allocator_.NextFpReg(rep);
        return LinkageLocation::ForRegister(reg_code, type);
      }
    } else if (allocator_.CanAllocateGP()) {
      int reg_code = allocator_.NextGpReg();
      return LinkageLocation::ForRegister(reg_code, type);
    }
    // Cannot use register; use stack slot.
    int index = -1 - allocator_.NextStackSlot(rep);
    return LinkageLocation::ForCallerFrameSlot(index, type);
  }

  void SetStackOffset(int offset) { allocator_.SetStackOffset(offset); }
  int NumStackSlots() const { return allocator_.NumStackSlots(); }

 private:
  wasm::LinkageAllocator allocator_;
};
}  // namespace

// General code uses the above configuration data.
CallDescriptor* GetWasmCallDescriptor(
    Zone* zone, wasm::FunctionSig* fsig,
    WasmGraphBuilder::UseRetpoline use_retpoline) {
  // The '+ 1' here is to accomodate the instance object as first parameter.
  LocationSignature::Builder locations(zone, fsig->return_count(),
                                       fsig->parameter_count() + 1);

  // Add register and/or stack parameter(s).
  LinkageLocationAllocator params(wasm::kGpParamRegisters,
                                  wasm::kFpParamRegisters);

  // The instance object.
  locations.AddParam(params.Next(MachineRepresentation::kTaggedPointer));

  const int parameter_count = static_cast<int>(fsig->parameter_count());
  for (int i = 0; i < parameter_count; i++) {
    MachineRepresentation param =
        wasm::ValueTypes::MachineRepresentationFor(fsig->GetParam(i));
    auto l = params.Next(param);
    locations.AddParam(l);
  }

  // Add return location(s).
  LinkageLocationAllocator rets(wasm::kGpReturnRegisters,
                                wasm::kFpReturnRegisters);

  int parameter_slots = params.NumStackSlots();
  if (kPadArguments) parameter_slots = RoundUp(parameter_slots, 2);

  rets.SetStackOffset(parameter_slots);

  const int return_count = static_cast<int>(locations.return_count_);
  for (int i = 0; i < return_count; i++) {
    MachineRepresentation ret =
        wasm::ValueTypes::MachineRepresentationFor(fsig->GetReturn(i));
    auto l = rets.Next(ret);
    locations.AddReturn(l);
  }

  const RegList kCalleeSaveRegisters = 0;
  const RegList kCalleeSaveFPRegisters = 0;

  // The target for wasm calls is always a code object.
  MachineType target_type = MachineType::Pointer();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister(target_type);

  CallDescriptor::Kind kind = CallDescriptor::kCallWasmFunction;

  CallDescriptor::Flags flags =
      use_retpoline ? CallDescriptor::kRetpoline : CallDescriptor::kNoFlags;
  return new (zone) CallDescriptor(             // --
      kind,                                     // kind
      target_type,                              // target MachineType
      target_loc,                               // target location
      locations.Build(),                        // location_sig
      parameter_slots,                          // stack_parameter_count
      compiler::Operator::kNoProperties,        // properties
      kCalleeSaveRegisters,                     // callee-saved registers
      kCalleeSaveFPRegisters,                   // callee-saved fp regs
      flags,                                    // flags
      "wasm-call",                              // debug name
      0,                                        // allocatable registers
      rets.NumStackSlots() - parameter_slots);  // stack_return_count
}

namespace {
CallDescriptor* ReplaceTypeInCallDescriptorWith(
    Zone* zone, CallDescriptor* call_descriptor, size_t num_replacements,
    MachineType input_type, MachineRepresentation output_type) {
  size_t parameter_count = call_descriptor->ParameterCount();
  size_t return_count = call_descriptor->ReturnCount();
  for (size_t i = 0; i < call_descriptor->ParameterCount(); i++) {
    if (call_descriptor->GetParameterType(i) == input_type) {
      parameter_count += num_replacements - 1;
    }
  }
  for (size_t i = 0; i < call_descriptor->ReturnCount(); i++) {
    if (call_descriptor->GetReturnType(i) == input_type) {
      return_count += num_replacements - 1;
    }
  }
  if (parameter_count == call_descriptor->ParameterCount() &&
      return_count == call_descriptor->ReturnCount()) {
    return call_descriptor;
  }

  LocationSignature::Builder locations(zone, return_count, parameter_count);

  LinkageLocationAllocator params(wasm::kGpParamRegisters,
                                  wasm::kFpParamRegisters);
  for (size_t i = 0; i < call_descriptor->ParameterCount(); i++) {
    if (call_descriptor->GetParameterType(i) == input_type) {
      for (size_t j = 0; j < num_replacements; j++) {
        locations.AddParam(params.Next(output_type));
      }
    } else {
      locations.AddParam(
          params.Next(call_descriptor->GetParameterType(i).representation()));
    }
  }

  LinkageLocationAllocator rets(wasm::kGpReturnRegisters,
                                wasm::kFpReturnRegisters);
  rets.SetStackOffset(params.NumStackSlots());
  for (size_t i = 0; i < call_descriptor->ReturnCount(); i++) {
    if (call_descriptor->GetReturnType(i) == input_type) {
      for (size_t j = 0; j < num_replacements; j++) {
        locations.AddReturn(rets.Next(output_type));
      }
    } else {
      locations.AddReturn(
          rets.Next(call_descriptor->GetReturnType(i).representation()));
    }
  }

  return new (zone) CallDescriptor(                    // --
      call_descriptor->kind(),                         // kind
      call_descriptor->GetInputType(0),                // target MachineType
      call_descriptor->GetInputLocation(0),            // target location
      locations.Build(),                               // location_sig
      params.NumStackSlots(),                          // stack_parameter_count
      call_descriptor->properties(),                   // properties
      call_descriptor->CalleeSavedRegisters(),         // callee-saved registers
      call_descriptor->CalleeSavedFPRegisters(),       // callee-saved fp regs
      call_descriptor->flags(),                        // flags
      call_descriptor->debug_name(),                   // debug name
      call_descriptor->AllocatableRegisters(),         // allocatable registers
      rets.NumStackSlots() - params.NumStackSlots());  // stack_return_count
}
}  // namespace

CallDescriptor* GetI32WasmCallDescriptor(Zone* zone,
                                         CallDescriptor* call_descriptor) {
  return ReplaceTypeInCallDescriptorWith(zone, call_descriptor, 2,
                                         MachineType::Int64(),
                                         MachineRepresentation::kWord32);
}

CallDescriptor* GetI32WasmCallDescriptorForSimd(
    Zone* zone, CallDescriptor* call_descriptor) {
  return ReplaceTypeInCallDescriptorWith(zone, call_descriptor, 4,
                                         MachineType::Simd128(),
                                         MachineRepresentation::kWord32);
}

AssemblerOptions WasmAssemblerOptions() {
  AssemblerOptions options;
  options.record_reloc_info_for_serialization = true;
  options.enable_root_array_delta_access = false;
  return options;
}

#undef WASM_64
#undef FATAL_UNSUPPORTED_OPCODE
#undef WASM_INSTANCE_OBJECT_SIZE
#undef WASM_INSTANCE_OBJECT_OFFSET
#undef LOAD_INSTANCE_FIELD
#undef LOAD_TAGGED_POINTER
#undef LOAD_FIXED_ARRAY_SLOT

}  // namespace compiler
}  // namespace internal
}  // namespace v8
