// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include <memory>

#include "src/isolate-inl.h"

#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"

#include "src/compiler/access-builder.h"
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

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/factory.h"
#include "src/log-inl.h"

#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/wasm/wasm-text.h"

// TODO(titzer): pull WASM_64 up to a common header.
#if !V8_TARGET_ARCH_32_BIT || V8_TARGET_ARCH_X64
#define WASM_64 1
#else
#define WASM_64 0
#endif

namespace v8 {
namespace internal {
namespace compiler {

namespace {
const Operator* UnsupportedOpcode(wasm::WasmOpcode opcode) {
  V8_Fatal(__FILE__, __LINE__, "Unsupported opcode #%d:%s", opcode,
           wasm::WasmOpcodes::OpcodeName(opcode));
  return nullptr;
}

void MergeControlToEnd(JSGraph* jsgraph, Node* node) {
  Graph* g = jsgraph->graph();
  if (g->end()) {
    NodeProperties::MergeControlToEnd(g, jsgraph->common(), node);
  } else {
    g->SetEnd(g->NewNode(jsgraph->common()->End(1), node));
  }
}

Node* BuildCallToRuntime(Runtime::FunctionId f, JSGraph* jsgraph,
                         Handle<Context> context, Node** parameters,
                         int parameter_count, Node** effect_ptr,
                         Node* control) {
  const Runtime::Function* fun = Runtime::FunctionForId(f);
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(
      jsgraph->zone(), f, fun->nargs, Operator::kNoProperties,
      CallDescriptor::kNoFlags);
  // CEntryStubConstant nodes have to be created and cached in the main
  // thread. At the moment this is only done for CEntryStubConstant(1).
  DCHECK_EQ(1, fun->result_size);
  // At the moment we only allow 2 parameters. If more parameters are needed,
  // increase this constant accordingly.
  static const int kMaxParams = 3;
  DCHECK_GE(kMaxParams, parameter_count);
  Node* inputs[kMaxParams + 6];
  int count = 0;
  inputs[count++] = jsgraph->CEntryStubConstant(fun->result_size);
  for (int i = 0; i < parameter_count; i++) {
    inputs[count++] = parameters[i];
  }
  inputs[count++] = jsgraph->ExternalConstant(
      ExternalReference(f, jsgraph->isolate()));         // ref
  inputs[count++] = jsgraph->Int32Constant(fun->nargs);  // arity
  inputs[count++] = jsgraph->HeapConstant(context);      // context
  inputs[count++] = *effect_ptr;
  inputs[count++] = control;

  Node* node =
      jsgraph->graph()->NewNode(jsgraph->common()->Call(desc), count, inputs);
  *effect_ptr = node;
  return node;
}

}  // namespace

// TODO(eholk): Support trap handlers on other platforms.
#if V8_TARGET_ARCH_X64 && V8_OS_LINUX
const bool kTrapHandlerSupported = true;
#else
const bool kTrapHandlerSupported = false;
#endif

// A helper that handles building graph fragments for trapping.
// To avoid generating a ton of redundant code that just calls the runtime
// to trap, we generate a per-trap-reason block of code that all trap sites
// in this function will branch to.
class WasmTrapHelper : public ZoneObject {
 public:
  explicit WasmTrapHelper(WasmGraphBuilder* builder)
      : builder_(builder),
        jsgraph_(builder->jsgraph()),
        graph_(builder->jsgraph() ? builder->jsgraph()->graph() : nullptr) {}

  // Make the current control path trap to unreachable.
  void Unreachable(wasm::WasmCodePosition position) {
    ConnectTrap(wasm::kTrapUnreachable, position);
  }

  // Always trap with the given reason.
  void TrapAlways(wasm::TrapReason reason, wasm::WasmCodePosition position) {
    ConnectTrap(reason, position);
  }

  // Add a check that traps if {node} is equal to {val}.
  Node* TrapIfEq32(wasm::TrapReason reason, Node* node, int32_t val,
                   wasm::WasmCodePosition position) {
    Int32Matcher m(node);
    if (m.HasValue() && !m.Is(val)) return graph()->start();
    if (val == 0) {
      AddTrapIfFalse(reason, node, position);
    } else {
      AddTrapIfTrue(reason,
                    graph()->NewNode(jsgraph()->machine()->Word32Equal(), node,
                                     jsgraph()->Int32Constant(val)),
                    position);
    }
    return builder_->Control();
  }

  // Add a check that traps if {node} is zero.
  Node* ZeroCheck32(wasm::TrapReason reason, Node* node,
                    wasm::WasmCodePosition position) {
    return TrapIfEq32(reason, node, 0, position);
  }

  // Add a check that traps if {node} is equal to {val}.
  Node* TrapIfEq64(wasm::TrapReason reason, Node* node, int64_t val,
                   wasm::WasmCodePosition position) {
    Int64Matcher m(node);
    if (m.HasValue() && !m.Is(val)) return graph()->start();
    AddTrapIfTrue(reason, graph()->NewNode(jsgraph()->machine()->Word64Equal(),
                                           node, jsgraph()->Int64Constant(val)),
                  position);
    return builder_->Control();
  }

  // Add a check that traps if {node} is zero.
  Node* ZeroCheck64(wasm::TrapReason reason, Node* node,
                    wasm::WasmCodePosition position) {
    return TrapIfEq64(reason, node, 0, position);
  }

  Runtime::FunctionId GetFunctionIdForTrap(wasm::TrapReason reason) {
    if (builder_->module_ && !builder_->module_->instance->context.is_null()) {
      switch (reason) {
#define TRAPREASON_TO_MESSAGE(name) \
  case wasm::k##name:               \
    return Runtime::kThrowWasm##name;
        FOREACH_WASM_TRAPREASON(TRAPREASON_TO_MESSAGE)
#undef TRAPREASON_TO_MESSAGE
        default:
          UNREACHABLE();
          return Runtime::kNumFunctions;
      }
    } else {
      // We use Runtime::kNumFunctions as a marker to tell the code generator
      // to generate a call to a testing c-function instead of a runtime
      // function. This code should only be called from a cctest.
      return Runtime::kNumFunctions;
    }
  }

#if V8_TARGET_ARCH_X64 || V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_ARM ||      \
    V8_TARGET_ARCH_ARM64 || V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64 || \
    V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64 || V8_TARGET_ARCH_S390 ||    \
    V8_TARGET_ARCH_S390X
#define WASM_TRAP_IF_SUPPORTED
#endif

  // Add a trap if {cond} is true.
  void AddTrapIfTrue(wasm::TrapReason reason, Node* cond,
                     wasm::WasmCodePosition position) {
#ifdef WASM_TRAP_IF_SUPPORTED
    if (FLAG_wasm_trap_if) {
      int32_t trap_id = GetFunctionIdForTrap(reason);
      Node* node = graph()->NewNode(common()->TrapIf(trap_id), cond,
                                    builder_->Effect(), builder_->Control());
      *builder_->control_ = node;
      builder_->SetSourcePosition(node, position);
      return;
    }
#endif  // WASM_TRAP_IF_SUPPORTED
    BuildTrapIf(reason, cond, true, position);
  }

  // Add a trap if {cond} is false.
  void AddTrapIfFalse(wasm::TrapReason reason, Node* cond,
                      wasm::WasmCodePosition position) {
#ifdef WASM_TRAP_IF_SUPPORTED
    if (FLAG_wasm_trap_if) {
      int32_t trap_id = GetFunctionIdForTrap(reason);

      Node* node = graph()->NewNode(common()->TrapUnless(trap_id), cond,
                                    builder_->Effect(), builder_->Control());
      *builder_->control_ = node;
      builder_->SetSourcePosition(node, position);
      return;
    }
#endif  // WASM_TRAP_IF_SUPPORTED

    BuildTrapIf(reason, cond, false, position);
  }

  // Add a trap if {cond} is true or false according to {iftrue}.
  void BuildTrapIf(wasm::TrapReason reason, Node* cond, bool iftrue,
                   wasm::WasmCodePosition position) {
    Node** effect_ptr = builder_->effect_;
    Node** control_ptr = builder_->control_;
    Node* before = *effect_ptr;
    BranchHint hint = iftrue ? BranchHint::kFalse : BranchHint::kTrue;
    Node* branch = graph()->NewNode(common()->Branch(hint), cond, *control_ptr);
    Node* if_true = graph()->NewNode(common()->IfTrue(), branch);
    Node* if_false = graph()->NewNode(common()->IfFalse(), branch);

    *control_ptr = iftrue ? if_true : if_false;
    ConnectTrap(reason, position);
    *control_ptr = iftrue ? if_false : if_true;
    *effect_ptr = before;
  }

  Node* GetTrapValue(wasm::FunctionSig* sig) {
    if (sig->return_count() > 0) {
      return GetTrapValue(sig->GetReturn());
    } else {
      return jsgraph()->Int32Constant(0xdeadbeef);
    }
  }

  Node* GetTrapValue(wasm::ValueType type) {
    switch (type) {
      case wasm::kWasmI32:
        return jsgraph()->Int32Constant(0xdeadbeef);
      case wasm::kWasmI64:
        return jsgraph()->Int64Constant(0xdeadbeefdeadbeef);
      case wasm::kWasmF32:
        return jsgraph()->Float32Constant(bit_cast<float>(0xdeadbeef));
      case wasm::kWasmF64:
        return jsgraph()->Float64Constant(bit_cast<double>(0xdeadbeefdeadbeef));
        break;
      case wasm::kWasmS128:
        return builder_->CreateS128Value(0xdeadbeef);
        break;
      default:
        UNREACHABLE();
        return nullptr;
    }
  }

 private:
  WasmGraphBuilder* builder_;
  JSGraph* jsgraph_;
  Graph* graph_;
  Node* trap_merge_ = nullptr;
  Node* trap_effect_;
  Node* trap_reason_;
  Node* trap_position_;

  JSGraph* jsgraph() { return jsgraph_; }
  Graph* graph() { return jsgraph_->graph(); }
  CommonOperatorBuilder* common() { return jsgraph()->common(); }

  void ConnectTrap(wasm::TrapReason reason, wasm::WasmCodePosition position) {
    DCHECK(position != wasm::kNoCodePosition);
    Node* reason_node = builder_->Int32Constant(
        wasm::WasmOpcodes::TrapReasonToMessageId(reason));
    Node* position_node = builder_->Int32Constant(position);
    if (trap_merge_ == nullptr) {
      // Create trap code for the first time.
      return BuildTrapCode(reason_node, position_node);
    }
    // Connect the current control and effect to the existing trap code.
    builder_->AppendToMerge(trap_merge_, builder_->Control());
    builder_->AppendToPhi(trap_effect_, builder_->Effect());
    builder_->AppendToPhi(trap_reason_, reason_node);
    builder_->AppendToPhi(trap_position_, position_node);
  }

  void BuildTrapCode(Node* reason_node, Node* position_node) {
    Node** control_ptr = builder_->control_;
    Node** effect_ptr = builder_->effect_;
    wasm::ModuleEnv* module = builder_->module_;
    DCHECK(trap_merge_ == NULL);
    *control_ptr = trap_merge_ =
        graph()->NewNode(common()->Merge(1), *control_ptr);
    *effect_ptr = trap_effect_ =
        graph()->NewNode(common()->EffectPhi(1), *effect_ptr, *control_ptr);
    trap_reason_ =
        graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 1),
                         reason_node, *control_ptr);
    trap_position_ =
        graph()->NewNode(common()->Phi(MachineRepresentation::kWord32, 1),
                         position_node, *control_ptr);

    Node* trap_reason_smi = builder_->BuildChangeInt32ToSmi(trap_reason_);
    Node* trap_position_smi = builder_->BuildChangeInt32ToSmi(trap_position_);

    if (module && !module->instance->context.is_null()) {
      Node* parameters[] = {trap_reason_smi,     // message id
                            trap_position_smi};  // byte position
      BuildCallToRuntime(Runtime::kThrowWasmError, jsgraph(),
                         module->instance->context, parameters,
                         arraysize(parameters), effect_ptr, *control_ptr);
    }
    if (false) {
      // End the control flow with a throw
      Node* thrw =
          graph()->NewNode(common()->Throw(), jsgraph()->ZeroConstant(),
                           *effect_ptr, *control_ptr);
      MergeControlToEnd(jsgraph(), thrw);
    } else {
      // End the control flow with returning 0xdeadbeef
      Node* ret_value = GetTrapValue(builder_->GetFunctionSignature());
      builder_->Return(ret_value);
    }
  }
};

WasmGraphBuilder::WasmGraphBuilder(
    wasm::ModuleEnv* module_env, Zone* zone, JSGraph* jsgraph,
    wasm::FunctionSig* sig,
    compiler::SourcePositionTable* source_position_table)
    : zone_(zone),
      jsgraph_(jsgraph),
      module_(module_env),
      signature_tables_(zone),
      function_tables_(zone),
      function_table_sizes_(zone),
      cur_buffer_(def_buffer_),
      cur_bufsize_(kDefaultBufferSize),
      trap_(new (zone) WasmTrapHelper(this)),
      sig_(sig),
      source_position_table_(source_position_table) {
  for (size_t i = 0; i < sig->parameter_count(); i++) {
    if (sig->GetParam(i) == wasm::kWasmS128) has_simd_ = true;
  }
  for (size_t i = 0; i < sig->return_count(); i++) {
    if (sig->GetReturn(i) == wasm::kWasmS128) has_simd_ = true;
  }
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

unsigned WasmGraphBuilder::InputCount(Node* node) {
  return static_cast<unsigned>(node->InputCount());
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

void WasmGraphBuilder::StackCheck(wasm::WasmCodePosition position,
                                  Node** effect, Node** control) {
  if (FLAG_wasm_no_stack_checks) return;
  if (effect == nullptr) {
    effect = effect_;
  }
  if (control == nullptr) {
    control = control_;
  }
  // We do not generate stack checks for cctests.
  if (module_ && !module_->instance->context.is_null()) {
    Node* limit = graph()->NewNode(
        jsgraph()->machine()->Load(MachineType::Pointer()),
        jsgraph()->ExternalConstant(
            ExternalReference::address_of_stack_limit(jsgraph()->isolate())),
        jsgraph()->IntPtrConstant(0), *effect, *control);
    Node* pointer = graph()->NewNode(jsgraph()->machine()->LoadStackPointer());

    Node* check =
        graph()->NewNode(jsgraph()->machine()->UintLessThan(), limit, pointer);

    Diamond stack_check(graph(), jsgraph()->common(), check, BranchHint::kTrue);
    stack_check.Chain(*control);
    Node* effect_true = *effect;

    // Generate a call to the runtime if there is a stack check failure.
    Node* call = BuildCallToRuntime(Runtime::kStackGuard, jsgraph(),
                                    module_->instance->context, nullptr, 0,
                                    effect, stack_check.if_false);
    SetSourcePosition(call, position);

    Node* ephi = graph()->NewNode(jsgraph()->common()->EffectPhi(2),
                                  effect_true, call, stack_check.merge);

    *control = stack_check.merge;
    *effect = ephi;
  }
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
      op = UnsupportedOpcode(opcode);
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
      return BuildI32SConvertF64(input, position);
    case wasm::kExprI32UConvertF64:
      return BuildI32UConvertF64(input, position);
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
      return BuildI32SConvertF32(input, position);
    case wasm::kExprI32UConvertF32:
      return BuildI32UConvertF32(input, position);
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
      op = UnsupportedOpcode(opcode);
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

Node* WasmGraphBuilder::ReturnVoid() { return Return(0, Buffer(0)); }

Node* WasmGraphBuilder::Unreachable(wasm::WasmCodePosition position) {
  trap_->Unreachable(position);
  return nullptr;
}

Node* WasmGraphBuilder::MaskShiftCount32(Node* node) {
  static const int32_t kMask32 = 0x1f;
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
  static const int64_t kMask64 = 0x3f;
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
      return m->Word32ReverseBytes().IsSupported();
    case 8:
      return m->Word64ReverseBytes().IsSupported();
    default:
      break;
  }
  return false;
}

Node* WasmGraphBuilder::BuildChangeEndianness(Node* node, MachineType memtype,
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

      DCHECK(shiftCount > 0);
      DCHECK((shiftCount + 8) % 16 == 0);

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
                  jsgraph()->Int32Constant(0x7fffffff)),
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
                  jsgraph()->Int64Constant(0x7fffffffffffffff)),
            Binop(wasm::kExprI64And, Unop(wasm::kExprI64ReinterpretF64, right),
                  jsgraph()->Int64Constant(0x8000000000000000))));

  return result;
#else
  MachineOperatorBuilder* m = jsgraph()->machine();

  Node* high_word_left = graph()->NewNode(m->Float64ExtractHighWord32(), left);
  Node* high_word_right =
      graph()->NewNode(m->Float64ExtractHighWord32(), right);

  Node* new_high_word =
      Binop(wasm::kExprI32Ior, Binop(wasm::kExprI32And, high_word_left,
                                     jsgraph()->Int32Constant(0x7fffffff)),
            Binop(wasm::kExprI32And, high_word_right,
                  jsgraph()->Int32Constant(0x80000000)));

  return graph()->NewNode(m->Float64InsertHighWord32(), left, new_high_word);
#endif
}

Node* WasmGraphBuilder::BuildI32SConvertF32(Node* input,
                                            wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // Truncation of the input value is needed for the overflow check later.
  Node* trunc = Unop(wasm::kExprF32Trunc, input);
  Node* result = graph()->NewNode(m->TruncateFloat32ToInt32(), trunc);

  // Convert the result back to f64. If we end up at a different value than the
  // truncated input value, then there has been an overflow and we trap.
  Node* check = Unop(wasm::kExprF32SConvertI32, result);
  Node* overflow = Binop(wasm::kExprF32Ne, trunc, check);
  trap_->AddTrapIfTrue(wasm::kTrapFloatUnrepresentable, overflow, position);

  return result;
}

Node* WasmGraphBuilder::BuildI32SConvertF64(Node* input,
                                            wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // Truncation of the input value is needed for the overflow check later.
  Node* trunc = Unop(wasm::kExprF64Trunc, input);
  Node* result = graph()->NewNode(m->ChangeFloat64ToInt32(), trunc);

  // Convert the result back to f64. If we end up at a different value than the
  // truncated input value, then there has been an overflow and we trap.
  Node* check = Unop(wasm::kExprF64SConvertI32, result);
  Node* overflow = Binop(wasm::kExprF64Ne, trunc, check);
  trap_->AddTrapIfTrue(wasm::kTrapFloatUnrepresentable, overflow, position);

  return result;
}

Node* WasmGraphBuilder::BuildI32UConvertF32(Node* input,
                                            wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // Truncation of the input value is needed for the overflow check later.
  Node* trunc = Unop(wasm::kExprF32Trunc, input);
  Node* result = graph()->NewNode(m->TruncateFloat32ToUint32(), trunc);

  // Convert the result back to f32. If we end up at a different value than the
  // truncated input value, then there has been an overflow and we trap.
  Node* check = Unop(wasm::kExprF32UConvertI32, result);
  Node* overflow = Binop(wasm::kExprF32Ne, trunc, check);
  trap_->AddTrapIfTrue(wasm::kTrapFloatUnrepresentable, overflow, position);

  return result;
}

Node* WasmGraphBuilder::BuildI32UConvertF64(Node* input,
                                            wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  // Truncation of the input value is needed for the overflow check later.
  Node* trunc = Unop(wasm::kExprF64Trunc, input);
  Node* result = graph()->NewNode(m->TruncateFloat64ToUint32(), trunc);

  // Convert the result back to f64. If we end up at a different value than the
  // truncated input value, then there has been an overflow and we trap.
  Node* check = Unop(wasm::kExprF64UConvertI32, result);
  Node* overflow = Binop(wasm::kExprF64Ne, trunc, check);
  trap_->AddTrapIfTrue(wasm::kTrapFloatUnrepresentable, overflow, position);

  return result;
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
  Node* args[] = {function, stack_slot_param};

  return BuildCCall(sig_builder.Build(), args);
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
  Node** args = Buffer(5);
  args[0] = function;
  args[1] = stack_slot_param0;
  int input_count = 1;

  if (input1 != nullptr) {
    Node* stack_slot_param1 = graph()->NewNode(
        jsgraph()->machine()->StackSlot(type.representation()));
    const Operator* store_op1 = jsgraph()->machine()->Store(
        StoreRepresentation(type.representation(), kNoWriteBarrier));
    *effect_ = graph()->NewNode(store_op1, stack_slot_param1,
                                jsgraph()->Int32Constant(0), input1, *effect_,
                                *control_);
    args[2] = stack_slot_param1;
    ++input_count;
  }

  Signature<MachineType>::Builder sig_builder(jsgraph()->zone(), 0,
                                              input_count);
  sig_builder.AddParam(MachineType::Pointer());
  if (input1 != nullptr) {
    sig_builder.AddParam(MachineType::Pointer());
  }
  BuildCCall(sig_builder.Build(), args);

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
  Node* args[] = {function, stack_slot_param, stack_slot_result};
  BuildCCall(sig_builder.Build(), args);
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
    trap_->ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
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
    trap_->ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
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
    trap_->ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
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
    trap_->ZeroCheck64(wasm::kTrapFloatUnrepresentable, overflow, position);
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
  Node* args[] = {function, stack_slot_param, stack_slot_result};
  trap_->ZeroCheck32(wasm::kTrapFloatUnrepresentable,
                     BuildCCall(sig_builder.Build(), args), position);
  const Operator* load_op = jsgraph()->machine()->Load(result_type);
  Node* load =
      graph()->NewNode(load_op, stack_slot_result, jsgraph()->Int32Constant(0),
                       *effect_, *control_);
  *effect_ = load;
  return load;
}

Node* WasmGraphBuilder::GrowMemory(Node* input) {
  Diamond check_input_range(
      graph(), jsgraph()->common(),
      graph()->NewNode(jsgraph()->machine()->Uint32LessThanOrEqual(), input,
                       jsgraph()->Uint32Constant(wasm::kV8MaxWasmMemoryPages)),
      BranchHint::kTrue);

  check_input_range.Chain(*control_);

  Runtime::FunctionId function_id = Runtime::kWasmGrowMemory;
  const Runtime::Function* function = Runtime::FunctionForId(function_id);
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(
      jsgraph()->zone(), function_id, function->nargs, Operator::kNoThrow,
      CallDescriptor::kNoFlags);
  wasm::ModuleEnv* module = module_;
  input = BuildChangeUint32ToSmi(input);
  Node* inputs[] = {
      jsgraph()->CEntryStubConstant(function->result_size), input,  // C entry
      jsgraph()->ExternalConstant(
          ExternalReference(function_id, jsgraph()->isolate())),  // ref
      jsgraph()->Int32Constant(function->nargs),                  // arity
      jsgraph()->HeapConstant(module->instance->context),         // context
      *effect_,
      check_input_range.if_true};
  Node* call = graph()->NewNode(jsgraph()->common()->Call(desc),
                                static_cast<int>(arraysize(inputs)), inputs);

  Node* result = BuildChangeSmiToInt32(call);

  result = check_input_range.Phi(MachineRepresentation::kWord32, result,
                                 jsgraph()->Int32Constant(-1));
  *effect_ = graph()->NewNode(jsgraph()->common()->EffectPhi(2), call, *effect_,
                              check_input_range.merge);
  *control_ = check_input_range.merge;
  return result;
}

Node* WasmGraphBuilder::Throw(Node* input) {
  MachineOperatorBuilder* machine = jsgraph()->machine();

  // Pass the thrown value as two SMIs:
  //
  // upper = static_cast<uint32_t>(input) >> 16;
  // lower = input & 0xFFFF;
  //
  // This is needed because we can't safely call BuildChangeInt32ToTagged from
  // this method.
  //
  // TODO(wasm): figure out how to properly pass this to the runtime function.
  Node* upper = BuildChangeInt32ToSmi(
      graph()->NewNode(machine->Word32Shr(), input, Int32Constant(16)));
  Node* lower = BuildChangeInt32ToSmi(
      graph()->NewNode(machine->Word32And(), input, Int32Constant(0xFFFFu)));

  Node* parameters[] = {lower, upper};  // thrown value
  return BuildCallToRuntime(Runtime::kWasmThrow, jsgraph(),
                            module_->instance->context, parameters,
                            arraysize(parameters), effect_, *control_);
}

Node* WasmGraphBuilder::Catch(Node* input, wasm::WasmCodePosition position) {
  CommonOperatorBuilder* common = jsgraph()->common();

  Node* parameters[] = {input};  // caught value
  Node* value =
      BuildCallToRuntime(Runtime::kWasmGetCaughtExceptionValue, jsgraph(),
                         module_->instance->context, parameters,
                         arraysize(parameters), effect_, *control_);

  Node* is_smi;
  Node* is_heap;
  BranchExpectFalse(BuildTestNotSmi(value), &is_heap, &is_smi);

  // is_smi
  Node* smi_i32 = BuildChangeSmiToInt32(value);
  Node* is_smi_effect = *effect_;

  // is_heap
  *control_ = is_heap;
  Node* heap_f64 = BuildLoadHeapNumberValue(value, is_heap);

  // *control_ needs to point to the current control dependency (is_heap) in
  // case BuildI32SConvertF64 needs to insert nodes that depend on the "current"
  // control node.
  Node* heap_i32 = BuildI32SConvertF64(heap_f64, position);
  // *control_ contains the control node that should be used when merging the
  // result for the catch clause. It may be different than *control_ because
  // BuildI32SConvertF64 may introduce a new control node (used for trapping if
  // heap_f64 cannot be converted to an i32.
  is_heap = *control_;
  Node* is_heap_effect = *effect_;

  Node* merge = graph()->NewNode(common->Merge(2), is_heap, is_smi);
  Node* effect_merge = graph()->NewNode(common->EffectPhi(2), is_heap_effect,
                                        is_smi_effect, merge);

  Node* value_i32 = graph()->NewNode(
      common->Phi(MachineRepresentation::kWord32, 2), heap_i32, smi_i32, merge);

  *control_ = merge;
  *effect_ = effect_merge;
  return value_i32;
}

Node* WasmGraphBuilder::BuildI32DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  trap_->ZeroCheck32(wasm::kTrapDivByZero, right, position);
  Node* before = *control_;
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(
      graph()->NewNode(m->Word32Equal(), right, jsgraph()->Int32Constant(-1)),
      &denom_is_m1, &denom_is_not_m1);
  *control_ = denom_is_m1;
  trap_->TrapIfEq32(wasm::kTrapDivUnrepresentable, left, kMinInt, position);
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

  trap_->ZeroCheck32(wasm::kTrapRemByZero, right, position);

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
  return graph()->NewNode(
      m->Uint32Div(), left, right,
      trap_->ZeroCheck32(wasm::kTrapDivByZero, right, position));
}

Node* WasmGraphBuilder::BuildI32RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = jsgraph()->machine();
  return graph()->NewNode(
      m->Uint32Mod(), left, right,
      trap_->ZeroCheck32(wasm::kTrapRemByZero, right, position));
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
  trap_->ZeroCheck64(wasm::kTrapDivByZero, right, position);
  Node* before = *control_;
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(graph()->NewNode(jsgraph()->machine()->Word64Equal(), right,
                                     jsgraph()->Int64Constant(-1)),
                    &denom_is_m1, &denom_is_not_m1);
  *control_ = denom_is_m1;
  trap_->TrapIfEq64(wasm::kTrapDivUnrepresentable, left,
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
  trap_->ZeroCheck64(wasm::kTrapRemByZero, right, position);
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
  return graph()->NewNode(
      jsgraph()->machine()->Uint64Div(), left, right,
      trap_->ZeroCheck64(wasm::kTrapDivByZero, right, position));
}
Node* WasmGraphBuilder::BuildI64RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (jsgraph()->machine()->Is32()) {
    return BuildDiv64Call(
        left, right, ExternalReference::wasm_uint64_mod(jsgraph()->isolate()),
        MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  return graph()->NewNode(
      jsgraph()->machine()->Uint64Mod(), left, right,
      trap_->ZeroCheck64(wasm::kTrapRemByZero, right, position));
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
  Node* args[] = {function, stack_slot_dst, stack_slot_src};

  Node* call = BuildCCall(sig_builder.Build(), args);

  // TODO(wasm): This can get simpler if we have a specialized runtime call to
  // throw WASM exceptions by trap code instead of by string.
  trap_->ZeroCheck32(static_cast<wasm::TrapReason>(trap_zero), call, position);
  trap_->TrapIfEq32(wasm::kTrapDivUnrepresentable, call, -1, position);
  const Operator* load_op = jsgraph()->machine()->Load(result_type);
  Node* load =
      graph()->NewNode(load_op, stack_slot_dst, jsgraph()->Int32Constant(0),
                       *effect_, *control_);
  *effect_ = load;
  return load;
}

Node* WasmGraphBuilder::BuildCCall(MachineSignature* sig, Node** args) {
  const size_t params = sig->parameter_count();
  const size_t extra = 2;  // effect and control inputs.
  const size_t count = 1 + params + extra;

  // Reallocate the buffer to make space for extra inputs.
  args = Realloc(args, 1 + params, count);

  // Add effect and control inputs.
  args[params + 1] = *effect_;
  args[params + 2] = *control_;

  CallDescriptor* desc =
      Linkage::GetSimplifiedCDescriptor(jsgraph()->zone(), sig);

  const Operator* op = jsgraph()->common()->Call(desc);
  Node* call = graph()->NewNode(op, static_cast<int>(count), args);
  *effect_ = call;
  return call;
}

Node* WasmGraphBuilder::BuildWasmCall(wasm::FunctionSig* sig, Node** args,
                                      Node*** rets,
                                      wasm::WasmCodePosition position) {
  const size_t params = sig->parameter_count();
  const size_t extra = 2;  // effect and control inputs.
  const size_t count = 1 + params + extra;

  // Reallocate the buffer to make space for extra inputs.
  args = Realloc(args, 1 + params, count);

  // Add effect and control inputs.
  args[params + 1] = *effect_;
  args[params + 2] = *control_;

  CallDescriptor* descriptor =
      wasm::ModuleEnv::GetWasmCallDescriptor(jsgraph()->zone(), sig);
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

  // Add code object as constant.
  Handle<Code> code = module_->GetFunctionCode(index);
  DCHECK(!code.is_null());
  args[0] = HeapConstant(code);
  wasm::FunctionSig* sig = module_->GetFunctionSignature(index);

  return BuildWasmCall(sig, args, rets, position);
}

Node* WasmGraphBuilder::CallIndirect(uint32_t sig_index, Node** args,
                                     Node*** rets,
                                     wasm::WasmCodePosition position) {
  DCHECK_NOT_NULL(args[0]);
  DCHECK(module_ && module_->instance);

  // Assume only one table for now.
  uint32_t table_index = 0;
  wasm::FunctionSig* sig = module_->GetSignature(sig_index);

  DCHECK(module_->IsValidTable(table_index));

  EnsureFunctionTableNodes();
  MachineOperatorBuilder* machine = jsgraph()->machine();
  Node* key = args[0];

  // Bounds check against the table size.
  Node* size = function_table_sizes_[table_index];
  Node* in_bounds = graph()->NewNode(machine->Uint32LessThan(), key, size);
  trap_->AddTrapIfFalse(wasm::kTrapFuncInvalid, in_bounds, position);
  Node* table = function_tables_[table_index];
  Node* signatures = signature_tables_[table_index];

  // Load signature from the table and check.
  // The table is a FixedArray; signatures are encoded as SMIs.
  // [sig1, sig2, sig3, ...., code1, code2, code3 ...]
  ElementAccess access = AccessBuilder::ForFixedArrayElement();
  const int fixed_offset = access.header_size - access.tag();
  {
    Node* load_sig = graph()->NewNode(
        machine->Load(MachineType::AnyTagged()), signatures,
        graph()->NewNode(machine->Int32Add(),
                         graph()->NewNode(machine->Word32Shl(), key,
                                          Int32Constant(kPointerSizeLog2)),
                         Int32Constant(fixed_offset)),
        *effect_, *control_);
    auto map = const_cast<wasm::SignatureMap&>(
        module_->module->function_tables[0].map);
    Node* sig_match = graph()->NewNode(
        machine->WordEqual(), load_sig,
        jsgraph()->SmiConstant(static_cast<int>(map.FindOrInsert(sig))));
    trap_->AddTrapIfFalse(wasm::kTrapFuncSigMismatch, sig_match, position);
  }

  // Load code object from the table.
  Node* load_code = graph()->NewNode(
      machine->Load(MachineType::AnyTagged()), table,
      graph()->NewNode(machine->Int32Add(),
                       graph()->NewNode(machine->Word32Shl(), key,
                                        Int32Constant(kPointerSizeLog2)),
                       Uint32Constant(fixed_offset)),
      *effect_, *control_);

  args[0] = load_code;
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
      return nullptr;
  }
}

Node* WasmGraphBuilder::BuildJavaScriptToNumber(Node* node, Node* context) {
  Callable callable = CodeFactory::ToNumber(jsgraph()->isolate());
  CallDescriptor* desc = Linkage::GetStubCallDescriptor(
      jsgraph()->isolate(), jsgraph()->zone(), callable.descriptor(), 0,
      CallDescriptor::kNoFlags, Operator::kNoProperties);
  Node* stub_code = jsgraph()->HeapConstant(callable.code());

  Node* result = graph()->NewNode(jsgraph()->common()->Call(desc), stub_code,
                                  node, context, *effect_, *control_);

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

  if (CanCover(value, IrOpcode::kJSToNumber)) {
    // ChangeTaggedToFloat64(JSToNumber(x)) =>
    //   if IsSmi(x) then ChangeSmiToFloat64(x)
    //   else let y = JSToNumber(x) in
    //     if IsSmi(y) then ChangeSmiToFloat64(y)
    //     else BuildLoadHeapNumberValue(y)
    Node* object = NodeProperties::GetValueInput(value, 0);
    Node* context = NodeProperties::GetContextInput(value);
    Node* frame_state = NodeProperties::GetFrameStateInput(value);
    Node* effect = NodeProperties::GetEffectInput(value);
    Node* control = NodeProperties::GetControlInput(value);

    const Operator* merge_op = common->Merge(2);
    const Operator* ephi_op = common->EffectPhi(2);
    const Operator* phi_op = common->Phi(MachineRepresentation::kFloat64, 2);

    Node* check1 = BuildTestNotSmi(object);
    Node* branch1 =
        graph()->NewNode(common->Branch(BranchHint::kFalse), check1, control);

    Node* if_true1 = graph()->NewNode(common->IfTrue(), branch1);
    Node* vtrue1 = graph()->NewNode(value->op(), object, context, frame_state,
                                    effect, if_true1);
    Node* etrue1 = vtrue1;

    Node* check2 = BuildTestNotSmi(vtrue1);
    Node* branch2 = graph()->NewNode(common->Branch(), check2, if_true1);

    Node* if_true2 = graph()->NewNode(common->IfTrue(), branch2);
    Node* vtrue2 = BuildLoadHeapNumberValue(vtrue1, if_true2);

    Node* if_false2 = graph()->NewNode(common->IfFalse(), branch2);
    Node* vfalse2 = BuildChangeSmiToFloat64(vtrue1);

    if_true1 = graph()->NewNode(merge_op, if_true2, if_false2);
    vtrue1 = graph()->NewNode(phi_op, vtrue2, vfalse2, if_true1);

    Node* if_false1 = graph()->NewNode(common->IfFalse(), branch1);
    Node* vfalse1 = BuildChangeSmiToFloat64(object);
    Node* efalse1 = effect;

    Node* merge1 = graph()->NewNode(merge_op, if_true1, if_false1);
    Node* ephi1 = graph()->NewNode(ephi_op, etrue1, efalse1, merge1);
    Node* phi1 = graph()->NewNode(phi_op, vtrue1, vfalse1, merge1);

    // Wire the new diamond into the graph, {JSToNumber} can still throw.
    NodeProperties::ReplaceUses(value, phi1, ephi1, etrue1, etrue1);

    // TODO(mstarzinger): This iteration cuts out the IfSuccess projection from
    // the node and places it inside the diamond. Come up with a helper method!
    for (Node* use : etrue1->uses()) {
      if (use->opcode() == IrOpcode::kIfSuccess) {
        use->ReplaceUses(merge1);
        NodeProperties::ReplaceControlInput(branch2, use);
      }
    }
    return phi1;
  }

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

Node* WasmGraphBuilder::FromJS(Node* node, Node* context,
                               wasm::ValueType type) {
  DCHECK_NE(wasm::kWasmStmt, type);

  // Do a JavaScript ToNumber.
  Node* num = BuildJavaScriptToNumber(node, context);

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
      return nullptr;
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
  if (jsgraph()->machine()->Is64()) {
    value =
        graph()->NewNode(jsgraph()->machine()->ChangeUint32ToUint64(), value);
  }
  return graph()->NewNode(jsgraph()->machine()->WordShl(), value,
                          BuildSmiShiftBitsConstant());
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
  // The AllocateHeapNumberStub does not use the context, so we can safely pass
  // in Smi zero here.
  Callable callable = CodeFactory::AllocateHeapNumber(jsgraph()->isolate());
  Node* target = jsgraph()->HeapConstant(callable.code());
  Node* context = jsgraph()->NoContextConstant();
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
                                       target, context, effect, control);
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

bool IsJSCompatible(wasm::ValueType type) {
  return (type != wasm::kWasmI64) && (type != wasm::kWasmS128);
}

bool HasJSCompatibleSignature(wasm::FunctionSig* sig) {
  for (size_t i = 0; i < sig->parameter_count(); i++) {
    if (!IsJSCompatible(sig->GetParam(i))) {
      return false;
    }
  }
  for (size_t i = 0; i < sig->return_count(); i++) {
    if (!IsJSCompatible(sig->GetReturn(i))) {
      return false;
    }
  }
  return true;
}

void WasmGraphBuilder::BuildJSToWasmWrapper(Handle<Code> wasm_code,
                                            wasm::FunctionSig* sig) {
  int wasm_count = static_cast<int>(sig->parameter_count());
  int count = wasm_count + 3;
  Node** args = Buffer(count);

  // Build the start and the JS parameter nodes.
  Node* start = Start(wasm_count + 5);
  *control_ = start;
  *effect_ = start;

  if (!HasJSCompatibleSignature(sig_)) {
    // Throw a TypeError. The native context is good enough here because we
    // only throw a TypeError.
    BuildCallToRuntime(Runtime::kWasmThrowTypeError, jsgraph(),
                       jsgraph()->isolate()->native_context(), nullptr, 0,
                       effect_, *control_);

    // Add a dummy call to the wasm function so that the generated wrapper
    // contains a reference to the wrapped wasm function. Without this reference
    // the wasm function could not be re-imported into another wasm module.
    int pos = 0;
    args[pos++] = HeapConstant(wasm_code);
    args[pos++] = *effect_;
    args[pos++] = *control_;

    // We only need a dummy call descriptor.
    wasm::FunctionSig::Builder dummy_sig_builder(jsgraph()->zone(), 0, 0);
    CallDescriptor* desc = wasm::ModuleEnv::GetWasmCallDescriptor(
        jsgraph()->zone(), dummy_sig_builder.Build());
    *effect_ = graph()->NewNode(jsgraph()->common()->Call(desc), pos, args);
    Return(jsgraph()->UndefinedConstant());
    return;
  }

  // Create the context parameter
  Node* context = graph()->NewNode(
      jsgraph()->common()->Parameter(
          Linkage::GetJSCallContextParamIndex(wasm_count + 1), "%context"),
      graph()->start());

  int pos = 0;
  args[pos++] = HeapConstant(wasm_code);

  // Convert JS parameters to WASM numbers.
  for (int i = 0; i < wasm_count; ++i) {
    Node* param = Param(i + 1);
    Node* wasm_param = FromJS(param, context, sig->GetParam(i));
    args[pos++] = wasm_param;
  }

  args[pos++] = *effect_;
  args[pos++] = *control_;

  // Call the WASM code.
  CallDescriptor* desc =
      wasm::ModuleEnv::GetWasmCallDescriptor(jsgraph()->zone(), sig);

  Node* call = graph()->NewNode(jsgraph()->common()->Call(desc), count, args);
  *effect_ = call;
  Node* retval = call;
  Node* jsval = ToJS(
      retval, sig->return_count() == 0 ? wasm::kWasmStmt : sig->GetReturn());
  Return(jsval);
}

int WasmGraphBuilder::AddParameterNodes(Node** args, int pos, int param_count,
                                        wasm::FunctionSig* sig) {
  // Convert WASM numbers to JS values.
  int param_index = 0;
  for (int i = 0; i < param_count; ++i) {
    Node* param = Param(param_index++);
    args[pos++] = ToJS(param, sig->GetParam(i));
  }
  return pos;
}

void WasmGraphBuilder::BuildWasmToJSWrapper(Handle<JSReceiver> target,
                                            wasm::FunctionSig* sig) {
  DCHECK(target->IsCallable());

  int wasm_count = static_cast<int>(sig->parameter_count());

  // Build the start and the parameter nodes.
  Isolate* isolate = jsgraph()->isolate();
  CallDescriptor* desc;
  Node* start = Start(wasm_count + 3);
  *effect_ = start;
  *control_ = start;

  if (!HasJSCompatibleSignature(sig_)) {
    // Throw a TypeError. The native context is good enough here because we
    // only throw a TypeError.
    Return(BuildCallToRuntime(Runtime::kWasmThrowTypeError, jsgraph(),
                              jsgraph()->isolate()->native_context(), nullptr,
                              0, effect_, *control_));
    return;
  }

  Node** args = Buffer(wasm_count + 7);

  Node* call;
  bool direct_call = false;

  if (target->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(target);
    if (function->shared()->internal_formal_parameter_count() == wasm_count) {
      direct_call = true;
      int pos = 0;
      args[pos++] = jsgraph()->Constant(target);  // target callable.
      // Receiver.
      if (is_sloppy(function->shared()->language_mode()) &&
          !function->shared()->native()) {
        args[pos++] =
            HeapConstant(handle(function->context()->global_proxy(), isolate));
      } else {
        args[pos++] = jsgraph()->Constant(
            handle(isolate->heap()->undefined_value(), isolate));
      }

      desc = Linkage::GetJSCallDescriptor(
          graph()->zone(), false, wasm_count + 1, CallDescriptor::kNoFlags);

      // Convert WASM numbers to JS values.
      pos = AddParameterNodes(args, pos, wasm_count, sig);

      args[pos++] = jsgraph()->UndefinedConstant();        // new target
      args[pos++] = jsgraph()->Int32Constant(wasm_count);  // argument count
      args[pos++] = HeapConstant(handle(function->context()));
      args[pos++] = *effect_;
      args[pos++] = *control_;

      call = graph()->NewNode(jsgraph()->common()->Call(desc), pos, args);
    }
  }

  // We cannot call the target directly, we have to use the Call builtin.
  if (!direct_call) {
    int pos = 0;
    Callable callable = CodeFactory::Call(isolate);
    args[pos++] = jsgraph()->HeapConstant(callable.code());
    args[pos++] = jsgraph()->Constant(target);           // target callable
    args[pos++] = jsgraph()->Int32Constant(wasm_count);  // argument count
    args[pos++] = jsgraph()->Constant(
        handle(isolate->heap()->undefined_value(), isolate));  // receiver

    desc = Linkage::GetStubCallDescriptor(isolate, graph()->zone(),
                                          callable.descriptor(), wasm_count + 1,
                                          CallDescriptor::kNoFlags);

    // Convert WASM numbers to JS values.
    pos = AddParameterNodes(args, pos, wasm_count, sig);

    // The native_context is sufficient here, because all kind of callables
    // which depend on the context provide their own context. The context here
    // is only needed if the target is a constructor to throw a TypeError, if
    // the target is a native function, or if the target is a callable JSObject,
    // which can only be constructed by the runtime.
    args[pos++] = HeapConstant(isolate->native_context());
    args[pos++] = *effect_;
    args[pos++] = *control_;

    call = graph()->NewNode(jsgraph()->common()->Call(desc), pos, args);
  }

  *effect_ = call;
  SetSourcePosition(call, 0);

  // Convert the return value back.
  Node* i32_zero = jsgraph()->Int32Constant(0);
  Node* val = sig->return_count() == 0
                  ? i32_zero
                  : FromJS(call, HeapConstant(isolate->native_context()),
                           sig->GetReturn());
  Return(val);
}

void WasmGraphBuilder::BuildWasmInterpreterEntry(
    uint32_t function_index, wasm::FunctionSig* sig,
    Handle<WasmInstanceObject> instance) {
  int wasm_count = static_cast<int>(sig->parameter_count());
  int param_count = jsgraph()->machine()->Is64()
                        ? wasm_count
                        : Int64Lowering::GetParameterCountAfterLowering(sig);

  // Build the start and the parameter nodes.
  Node* start = Start(param_count + 3);
  *effect_ = start;
  *control_ = start;

  // Compute size for the argument buffer.
  int args_size_bytes = 0;
  for (int i = 0; i < wasm_count; i++) {
    args_size_bytes += 1 << ElementSizeLog2Of(sig->GetParam(i));
  }

  // The return value is also passed via this buffer:
  DCHECK_GE(wasm::kV8MaxWasmFunctionReturns, sig->return_count());
  // TODO(wasm): Handle multi-value returns.
  DCHECK_EQ(1, wasm::kV8MaxWasmFunctionReturns);
  int return_size_bytes =
      sig->return_count() == 0 ? 0 : 1 << ElementSizeLog2Of(sig->GetReturn(0));

  // Get a stack slot for the arguments.
  Node* arg_buffer = graph()->NewNode(jsgraph()->machine()->StackSlot(
      std::max(args_size_bytes, return_size_bytes)));

  // Now store all our arguments to the buffer.
  int param_index = 0;
  int offset = 0;
  for (int i = 0; i < wasm_count; i++) {
    Node* param = Param(param_index++);
    bool is_i64_as_two_params =
        jsgraph()->machine()->Is32() && sig->GetParam(i) == wasm::kWasmI64;
    MachineRepresentation param_rep =
        is_i64_as_two_params ? wasm::kWasmI32 : sig->GetParam(i);
    StoreRepresentation store_rep(param_rep, WriteBarrierKind::kNoWriteBarrier);
    *effect_ =
        graph()->NewNode(jsgraph()->machine()->Store(store_rep), arg_buffer,
                         Int32Constant(offset), param, *effect_, *control_);
    offset += 1 << ElementSizeLog2Of(param_rep);
    // TODO(clemensh): Respect endianess here. Might need to swap upper and
    // lower word.
    if (is_i64_as_two_params) {
      // Also store the upper half.
      param = Param(param_index++);
      StoreRepresentation store_rep(wasm::kWasmI32,
                                    WriteBarrierKind::kNoWriteBarrier);
      *effect_ =
          graph()->NewNode(jsgraph()->machine()->Store(store_rep), arg_buffer,
                           Int32Constant(offset), param, *effect_, *control_);
      offset += 1 << ElementSizeLog2Of(wasm::kWasmI32);
    }
  }
  DCHECK_EQ(param_count, param_index);
  DCHECK_EQ(args_size_bytes, offset);

  // We are passing the raw arg_buffer here. To the GC and other parts, it looks
  // like a Smi (lowest bit not set). In the runtime function however, don't
  // call Smi::value on it, but just cast it to a byte pointer.
  Node* parameters[] = {
      jsgraph()->HeapConstant(instance),       // wasm instance
      jsgraph()->SmiConstant(function_index),  // function index
      arg_buffer,                              // argument buffer
  };
  BuildCallToRuntime(Runtime::kWasmRunInterpreter, jsgraph(),
                     jsgraph()->isolate()->native_context(), parameters,
                     arraysize(parameters), effect_, *control_);

  // Read back the return value.
  if (jsgraph()->machine()->Is32() && sig->return_count() > 0 &&
      sig->GetReturn() == wasm::kWasmI64) {
    MachineType load_rep = wasm::WasmOpcodes::MachineTypeFor(wasm::kWasmI32);
    Node* lower =
        graph()->NewNode(jsgraph()->machine()->Load(load_rep), arg_buffer,
                         Int32Constant(0), *effect_, *control_);
    Node* upper =
        graph()->NewNode(jsgraph()->machine()->Load(load_rep), arg_buffer,
                         Int32Constant(sizeof(int32_t)), *effect_, *control_);
    Return(upper, lower);
  } else {
    Node* val;
    if (sig->return_count() == 0) {
      val = Int32Constant(0);
    } else {
      MachineType load_rep =
          wasm::WasmOpcodes::MachineTypeFor(sig->GetReturn());
      val = graph()->NewNode(jsgraph()->machine()->Load(load_rep), arg_buffer,
                             Int32Constant(0), *effect_, *control_);
    }
    Return(val);
  }
}

Node* WasmGraphBuilder::MemBuffer(uint32_t offset) {
  DCHECK(module_ && module_->instance);
  if (offset == 0) {
    if (!mem_buffer_) {
      mem_buffer_ = jsgraph()->RelocatableIntPtrConstant(
          reinterpret_cast<uintptr_t>(module_->instance->mem_start),
          RelocInfo::WASM_MEMORY_REFERENCE);
    }
    return mem_buffer_;
  } else {
    return jsgraph()->RelocatableIntPtrConstant(
        reinterpret_cast<uintptr_t>(module_->instance->mem_start + offset),
        RelocInfo::WASM_MEMORY_REFERENCE);
  }
}

Node* WasmGraphBuilder::CurrentMemoryPages() {
  Runtime::FunctionId function_id = Runtime::kWasmMemorySize;
  const Runtime::Function* function = Runtime::FunctionForId(function_id);
  CallDescriptor* desc = Linkage::GetRuntimeCallDescriptor(
      jsgraph()->zone(), function_id, function->nargs, Operator::kNoThrow,
      CallDescriptor::kNoFlags);
  wasm::ModuleEnv* module = module_;
  Node* inputs[] = {
      jsgraph()->CEntryStubConstant(function->result_size),  // C entry
      jsgraph()->ExternalConstant(
          ExternalReference(function_id, jsgraph()->isolate())),  // ref
      jsgraph()->Int32Constant(function->nargs),                  // arity
      jsgraph()->HeapConstant(module->instance->context),         // context
      *effect_,
      *control_};
  Node* call = graph()->NewNode(jsgraph()->common()->Call(desc),
                                static_cast<int>(arraysize(inputs)), inputs);

  Node* result = BuildChangeSmiToInt32(call);

  *effect_ = call;
  return result;
}

Node* WasmGraphBuilder::MemSize(uint32_t offset) {
  DCHECK(module_ && module_->instance);
  uint32_t size = static_cast<uint32_t>(module_->instance->mem_size);
  if (offset == 0) {
    if (!mem_size_)
      mem_size_ = jsgraph()->RelocatableInt32Constant(
          size, RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
    return mem_size_;
  } else {
    return jsgraph()->RelocatableInt32Constant(
        size + offset, RelocInfo::WASM_MEMORY_SIZE_REFERENCE);
  }
}

void WasmGraphBuilder::EnsureFunctionTableNodes() {
  if (function_tables_.size() > 0) return;
  size_t tables_size = module_->instance->function_tables.size();
  DCHECK(tables_size == module_->instance->signature_tables.size());
  for (size_t i = 0; i < tables_size; ++i) {
    auto function_handle = module_->instance->function_tables[i];
    auto signature_handle = module_->instance->signature_tables[i];
    DCHECK(!function_handle.is_null() && !signature_handle.is_null());
    function_tables_.push_back(HeapConstant(function_handle));
    signature_tables_.push_back(HeapConstant(signature_handle));
    uint32_t table_size = module_->module->function_tables[i].min_size;
    function_table_sizes_.push_back(jsgraph()->RelocatableInt32Constant(
        static_cast<uint32_t>(table_size),
        RelocInfo::WASM_FUNCTION_TABLE_SIZE_REFERENCE));
  }
}

Node* WasmGraphBuilder::GetGlobal(uint32_t index) {
  MachineType mem_type =
      wasm::WasmOpcodes::MachineTypeFor(module_->GetGlobalType(index));
  Node* addr = jsgraph()->RelocatableIntPtrConstant(
      reinterpret_cast<uintptr_t>(module_->instance->globals_start +
                                  module_->module->globals[index].offset),
      RelocInfo::WASM_GLOBAL_REFERENCE);
  const Operator* op = jsgraph()->machine()->Load(mem_type);
  Node* node = graph()->NewNode(op, addr, jsgraph()->Int32Constant(0), *effect_,
                                *control_);
  *effect_ = node;
  return node;
}

Node* WasmGraphBuilder::SetGlobal(uint32_t index, Node* val) {
  MachineType mem_type =
      wasm::WasmOpcodes::MachineTypeFor(module_->GetGlobalType(index));
  Node* addr = jsgraph()->RelocatableIntPtrConstant(
      reinterpret_cast<uintptr_t>(module_->instance->globals_start +
                                  module_->module->globals[index].offset),
      RelocInfo::WASM_GLOBAL_REFERENCE);
  const Operator* op = jsgraph()->machine()->Store(
      StoreRepresentation(mem_type.representation(), kNoWriteBarrier));
  Node* node = graph()->NewNode(op, addr, jsgraph()->Int32Constant(0), val,
                                *effect_, *control_);
  *effect_ = node;
  return node;
}

void WasmGraphBuilder::BoundsCheckMem(MachineType memtype, Node* index,
                                      uint32_t offset,
                                      wasm::WasmCodePosition position) {
  DCHECK(module_ && module_->instance);
  if (FLAG_wasm_no_bounds_checks) return;
  uint32_t size = module_->instance->mem_size;
  byte memsize = wasm::WasmOpcodes::MemSize(memtype);

  size_t effective_size;
  if (size <= offset || size < (static_cast<uint64_t>(offset) + memsize)) {
    // Two checks are needed in the case where the offset is statically
    // out of bounds; one check for the offset being in bounds, and the next for
    // the offset + index being out of bounds for code to be patched correctly
    // on relocation.

    // Check for overflows.
    if ((std::numeric_limits<uint32_t>::max() - memsize) + 1 < offset) {
      // Always trap. Do not use TrapAlways because it does not create a valid
      // graph here.
      trap_->TrapIfEq32(wasm::kTrapMemOutOfBounds, jsgraph()->Int32Constant(0),
                        0, position);
      return;
    }
    size_t effective_offset = (offset - 1) + memsize;

    Node* cond = graph()->NewNode(jsgraph()->machine()->Uint32LessThan(),
                                  jsgraph()->IntPtrConstant(effective_offset),
                                  jsgraph()->RelocatableInt32Constant(
                                      static_cast<uint32_t>(size),
                                      RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
    trap_->AddTrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
    // For offset > effective size, this relies on check above to fail and
    // effective size can be negative, relies on wrap around.
    effective_size = size - offset - memsize + 1;
  } else {
    effective_size = size - offset - memsize + 1;
    CHECK(effective_size <= kMaxUInt32);

    Uint32Matcher m(index);
    if (m.HasValue()) {
      uint32_t value = m.Value();
      if (value < effective_size) {
        // The bounds check will always succeed.
        return;
      }
    }
  }

  Node* cond = graph()->NewNode(jsgraph()->machine()->Uint32LessThan(), index,
                                jsgraph()->RelocatableInt32Constant(
                                    static_cast<uint32_t>(effective_size),
                                    RelocInfo::WASM_MEMORY_SIZE_REFERENCE));
  trap_->AddTrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
}

Node* WasmGraphBuilder::LoadMem(wasm::ValueType type, MachineType memtype,
                                Node* index, uint32_t offset,
                                uint32_t alignment,
                                wasm::WasmCodePosition position) {
  Node* load;

  // WASM semantics throw on OOB. Introduce explicit bounds check.
  if (!FLAG_wasm_trap_handler || !kTrapHandlerSupported) {
    BoundsCheckMem(memtype, index, offset, position);
  }
  bool aligned = static_cast<int>(alignment) >=
                 ElementSizeLog2Of(memtype.representation());

  if (aligned ||
      jsgraph()->machine()->UnalignedLoadSupported(memtype, alignment)) {
    if (FLAG_wasm_trap_handler && kTrapHandlerSupported) {
      DCHECK(FLAG_wasm_guard_pages);
      Node* position_node = jsgraph()->Int32Constant(position);
      load = graph()->NewNode(jsgraph()->machine()->ProtectedLoad(memtype),
                              MemBuffer(offset), index, position_node, *effect_,
                              *control_);
    } else {
      load = graph()->NewNode(jsgraph()->machine()->Load(memtype),
                              MemBuffer(offset), index, *effect_, *control_);
    }
  } else {
    // TODO(eholk): Support unaligned loads with trap handlers.
    DCHECK(!FLAG_wasm_trap_handler || !kTrapHandlerSupported);
    load = graph()->NewNode(jsgraph()->machine()->UnalignedLoad(memtype),
                            MemBuffer(offset), index, *effect_, *control_);
  }

  *effect_ = load;

#if defined(V8_TARGET_BIG_ENDIAN)
  load = BuildChangeEndianness(load, memtype, type);
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

  return load;
}


Node* WasmGraphBuilder::StoreMem(MachineType memtype, Node* index,
                                 uint32_t offset, uint32_t alignment, Node* val,
                                 wasm::WasmCodePosition position) {
  Node* store;

  // WASM semantics throw on OOB. Introduce explicit bounds check.
  if (!FLAG_wasm_trap_handler || !kTrapHandlerSupported) {
    BoundsCheckMem(memtype, index, offset, position);
  }
  StoreRepresentation rep(memtype.representation(), kNoWriteBarrier);

  bool aligned = static_cast<int>(alignment) >=
                 ElementSizeLog2Of(memtype.representation());

#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndianness(val, memtype);
#endif

  if (aligned ||
      jsgraph()->machine()->UnalignedStoreSupported(memtype, alignment)) {
    if (FLAG_wasm_trap_handler && kTrapHandlerSupported) {
      Node* position_node = jsgraph()->Int32Constant(position);
      store = graph()->NewNode(
          jsgraph()->machine()->ProtectedStore(memtype.representation()),
          MemBuffer(offset), index, val, position_node, *effect_, *control_);
    } else {
      StoreRepresentation rep(memtype.representation(), kNoWriteBarrier);
      store =
          graph()->NewNode(jsgraph()->machine()->Store(rep), MemBuffer(offset),
                           index, val, *effect_, *control_);
    }
  } else {
    // TODO(eholk): Support unaligned stores with trap handlers.
    DCHECK(!FLAG_wasm_trap_handler || !kTrapHandlerSupported);
    UnalignedStoreRepresentation rep(memtype.representation());
    store =
        graph()->NewNode(jsgraph()->machine()->UnalignedStore(rep),
                         MemBuffer(offset), index, val, *effect_, *control_);
  }

  *effect_ = store;

  return store;
}

Node* WasmGraphBuilder::BuildAsmjsLoadMem(MachineType type, Node* index) {
  // TODO(turbofan): fold bounds checks for constant asm.js loads.
  // asm.js semantics use CheckedLoad (i.e. OOB reads return 0ish).
  const Operator* op = jsgraph()->machine()->CheckedLoad(type);
  Node* load = graph()->NewNode(op, MemBuffer(0), index, MemSize(0), *effect_,
                                *control_);
  *effect_ = load;
  return load;
}

Node* WasmGraphBuilder::BuildAsmjsStoreMem(MachineType type, Node* index,
                                           Node* val) {
  // TODO(turbofan): fold bounds checks for constant asm.js stores.
  // asm.js semantics use CheckedStore (i.e. ignore OOB writes).
  const Operator* op =
      jsgraph()->machine()->CheckedStore(type.representation());
  Node* store = graph()->NewNode(op, MemBuffer(0), index, MemSize(0), val,
                                 *effect_, *control_);
  *effect_ = store;
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

void WasmGraphBuilder::Int64LoweringForTesting() {
  if (jsgraph()->machine()->Is32()) {
    Int64Lowering r(jsgraph()->graph(), jsgraph()->machine(),
                    jsgraph()->common(), jsgraph()->zone(), sig_);
    r.LowerGraph();
  }
}

void WasmGraphBuilder::SimdScalarLoweringForTesting() {
  SimdScalarLowering(jsgraph()->graph(), jsgraph()->machine(),
                     jsgraph()->common(), jsgraph()->zone(), sig_)
      .LowerGraph();
}

void WasmGraphBuilder::SetSourcePosition(Node* node,
                                         wasm::WasmCodePosition position) {
  DCHECK_NE(position, wasm::kNoCodePosition);
  if (source_position_table_)
    source_position_table_->SetSourcePosition(node, SourcePosition(position));
}

Node* WasmGraphBuilder::CreateS128Value(int32_t value) {
  // TODO(gdeepti): Introduce Simd128Constant to common-operator.h and use
  // instead of creating a SIMD Value.
  has_simd_ = true;
  return graph()->NewNode(jsgraph()->machine()->CreateInt32x4(),
                          Int32Constant(value), Int32Constant(value),
                          Int32Constant(value), Int32Constant(value));
}

Node* WasmGraphBuilder::SimdOp(wasm::WasmOpcode opcode,
                               const NodeVector& inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF32x4Splat:
      return graph()->NewNode(jsgraph()->machine()->CreateFloat32x4(),
                              inputs[0], inputs[0], inputs[0], inputs[0]);
    case wasm::kExprF32x4FromInt32x4:
      return graph()->NewNode(jsgraph()->machine()->Float32x4FromInt32x4(),
                              inputs[0]);
    case wasm::kExprF32x4FromUint32x4:
      return graph()->NewNode(jsgraph()->machine()->Float32x4FromUint32x4(),
                              inputs[0]);
    case wasm::kExprF32x4Abs:
      return graph()->NewNode(jsgraph()->machine()->Float32x4Abs(), inputs[0]);
    case wasm::kExprF32x4Neg:
      return graph()->NewNode(jsgraph()->machine()->Float32x4Neg(), inputs[0]);
    case wasm::kExprF32x4Add:
      return graph()->NewNode(jsgraph()->machine()->Float32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Sub:
      return graph()->NewNode(jsgraph()->machine()->Float32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Eq:
      return graph()->NewNode(jsgraph()->machine()->Float32x4Equal(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Ne:
      return graph()->NewNode(jsgraph()->machine()->Float32x4NotEqual(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4Splat:
      return graph()->NewNode(jsgraph()->machine()->CreateInt32x4(), inputs[0],
                              inputs[0], inputs[0], inputs[0]);
    case wasm::kExprI32x4FromFloat32x4:
      return graph()->NewNode(jsgraph()->machine()->Int32x4FromFloat32x4(),
                              inputs[0]);
    case wasm::kExprUi32x4FromFloat32x4:
      return graph()->NewNode(jsgraph()->machine()->Uint32x4FromFloat32x4(),
                              inputs[0]);
    case wasm::kExprI32x4Add:
      return graph()->NewNode(jsgraph()->machine()->Int32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Sub:
      return graph()->NewNode(jsgraph()->machine()->Int32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Eq:
      return graph()->NewNode(jsgraph()->machine()->Int32x4Equal(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Ne:
      return graph()->NewNode(jsgraph()->machine()->Int32x4NotEqual(),
                              inputs[0], inputs[1]);
    case wasm::kExprS32x4Select:
      return graph()->NewNode(jsgraph()->machine()->Simd32x4Select(), inputs[0],
                              inputs[1], inputs[2]);
    default:
      return graph()->NewNode(UnsupportedOpcode(opcode), nullptr);
  }
}

Node* WasmGraphBuilder::SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane,
                                   const NodeVector& inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprI32x4ExtractLane:
      return graph()->NewNode(jsgraph()->common()->Int32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI32x4ReplaceLane:
      return graph()->NewNode(jsgraph()->common()->Int32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprF32x4ExtractLane:
      return graph()->NewNode(jsgraph()->common()->Float32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF32x4ReplaceLane:
      return graph()->NewNode(jsgraph()->common()->Float32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    default:
      return graph()->NewNode(UnsupportedOpcode(opcode), nullptr);
  }
}

static void RecordFunctionCompilation(CodeEventListener::LogEventsAndTags tag,
                                      Isolate* isolate, Handle<Code> code,
                                      const char* message, uint32_t index,
                                      const wasm::WasmName& module_name,
                                      const wasm::WasmName& func_name) {
  DCHECK(isolate->logger()->is_logging_code_events() ||
         isolate->is_profiling());

  ScopedVector<char> buffer(128);
  SNPrintF(buffer, "%s#%d:%.*s:%.*s", message, index, module_name.length(),
           module_name.start(), func_name.length(), func_name.start());
  Handle<String> name_str =
      isolate->factory()->NewStringFromAsciiChecked(buffer.start());
  Handle<String> script_str =
      isolate->factory()->NewStringFromAsciiChecked("(WASM)");
  Handle<SharedFunctionInfo> shared =
      isolate->factory()->NewSharedFunctionInfo(name_str, code, false);
  PROFILE(isolate, CodeCreateEvent(tag, AbstractCode::cast(*code), *shared,
                                   *script_str, 0, 0));
}

Handle<Code> CompileJSToWasmWrapper(Isolate* isolate,
                                    const wasm::WasmModule* module,
                                    Handle<Code> wasm_code, uint32_t index) {
  const wasm::WasmFunction* func = &module->functions[index];

  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(&zone);
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  wasm::ModuleEnv module_env(module, nullptr);
  WasmGraphBuilder builder(&module_env, &zone, &jsgraph, func->sig);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildJSToWasmWrapper(wasm_code, func->sig);

  //----------------------------------------------------------------------------
  // Run the compilation pipeline.
  //----------------------------------------------------------------------------
  if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
    OFStream os(stdout);
    os << "-- Graph after change lowering -- " << std::endl;
    os << AsRPO(graph);
  }

  // Schedule and compile to machine code.
  int params = static_cast<int>(
      module_env.GetFunctionSignature(index)->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      &zone, false, params + 1, CallDescriptor::kNoFlags);
  Code::Flags flags = Code::ComputeFlags(Code::JS_TO_WASM_FUNCTION);
  bool debugging =
#if DEBUG
      true;
#else
      FLAG_print_opt_code || FLAG_trace_turbo || FLAG_trace_turbo_graph;
#endif
  Vector<const char> func_name = ArrayVector("js-to-wasm");

  static unsigned id = 0;
  Vector<char> buffer;
  if (debugging) {
    buffer = Vector<char>::New(128);
    int chars = SNPrintF(buffer, "js-to-wasm#%d", id);
    func_name = Vector<const char>::cast(buffer.SubVector(0, chars));
  }

  CompilationInfo info(func_name, isolate, &zone, flags);
  Handle<Code> code = Pipeline::GenerateCodeForTesting(&info, incoming, &graph);
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
    char func_name[32];
    SNPrintF(ArrayVector(func_name), "js-to-wasm#%d", func->func_index);
    RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate, code,
                              "js-to-wasm", index, wasm::WasmName("export"),
                              CStrVector(func_name));
  }
  return code;
}

Handle<Code> CompileWasmToJSWrapper(Isolate* isolate, Handle<JSReceiver> target,
                                    wasm::FunctionSig* sig, uint32_t index,
                                    Handle<String> module_name,
                                    MaybeHandle<String> import_name,
                                    wasm::ModuleOrigin origin) {
  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(isolate->allocator(), ZONE_NAME);
  Graph graph(&zone);
  CommonOperatorBuilder common(&zone);
  MachineOperatorBuilder machine(&zone);
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  SourcePositionTable* source_position_table =
      origin == wasm::kAsmJsOrigin ? new (&zone) SourcePositionTable(&graph)
                                   : nullptr;

  WasmGraphBuilder builder(nullptr, &zone, &jsgraph, sig,
                           source_position_table);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildWasmToJSWrapper(target, sig);

  Handle<Code> code = Handle<Code>::null();
  {
    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      OFStream os(stdout);
      os << "-- Graph after change lowering -- " << std::endl;
      os << AsRPO(graph);
    }

    // Schedule and compile to machine code.
    CallDescriptor* incoming =
        wasm::ModuleEnv::GetWasmCallDescriptor(&zone, sig);
    if (machine.Is32()) {
      incoming = wasm::ModuleEnv::GetI32WasmCallDescriptor(&zone, incoming);
    }
    Code::Flags flags = Code::ComputeFlags(Code::WASM_TO_JS_FUNCTION);
    bool debugging =
#if DEBUG
        true;
#else
        FLAG_print_opt_code || FLAG_trace_turbo || FLAG_trace_turbo_graph;
#endif
    Vector<const char> func_name = ArrayVector("wasm-to-js");
    static unsigned id = 0;
    Vector<char> buffer;
    if (debugging) {
      buffer = Vector<char>::New(128);
      int chars = SNPrintF(buffer, "wasm-to-js#%d", id);
      func_name = Vector<const char>::cast(buffer.SubVector(0, chars));
    }

    CompilationInfo info(func_name, isolate, &zone, flags);
    code = Pipeline::GenerateCodeForTesting(&info, incoming, &graph, nullptr,
                                            source_position_table);
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code && !code.is_null()) {
      OFStream os(stdout);
      code->Disassemble(buffer.start(), os);
    }
#endif
    if (debugging) {
      buffer.Dispose();
    }
  }
  if (isolate->logger()->is_logging_code_events() || isolate->is_profiling()) {
    const char* function_name = nullptr;
    int function_name_size = 0;
    if (!import_name.is_null()) {
      Handle<String> handle = import_name.ToHandleChecked();
      function_name = handle->ToCString().get();
      function_name_size = handle->length();
    }
    RecordFunctionCompilation(
        CodeEventListener::FUNCTION_TAG, isolate, code, "wasm-to-js", index,
        {module_name->ToCString().get(), module_name->length()},
        {function_name, function_name_size});
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
  MachineOperatorBuilder machine(&zone);
  JSGraph jsgraph(isolate, &graph, &common, nullptr, nullptr, &machine);

  Node* control = nullptr;
  Node* effect = nullptr;

  WasmGraphBuilder builder(nullptr, &zone, &jsgraph, sig);
  builder.set_control_ptr(&control);
  builder.set_effect_ptr(&effect);
  builder.BuildWasmInterpreterEntry(func_index, sig, instance);

  Handle<Code> code = Handle<Code>::null();
  {
    if (FLAG_trace_turbo_graph) {  // Simple textual RPO.
      OFStream os(stdout);
      os << "-- Wasm to interpreter graph -- " << std::endl;
      os << AsRPO(graph);
    }

    // Schedule and compile to machine code.
    CallDescriptor* incoming =
        wasm::ModuleEnv::GetWasmCallDescriptor(&zone, sig);
    if (machine.Is32()) {
      incoming = wasm::ModuleEnv::GetI32WasmCallDescriptor(&zone, incoming);
    }
    Code::Flags flags = Code::ComputeFlags(Code::WASM_INTERPRETER_ENTRY);
    EmbeddedVector<char, 32> debug_name;
    int name_len = SNPrintF(debug_name, "wasm-to-interpreter#%d", func_index);
    DCHECK(name_len > 0 && name_len < debug_name.length());
    debug_name.Truncate(name_len);
    DCHECK_EQ('\0', debug_name.start()[debug_name.length()]);

    CompilationInfo info(debug_name, isolate, &zone, flags);
    code = Pipeline::GenerateCodeForTesting(&info, incoming, &graph, nullptr);
#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_opt_code && !code.is_null()) {
      OFStream os(stdout);
      code->Disassemble(debug_name.start(), os);
    }
#endif

    if (isolate->logger()->is_logging_code_events() ||
        isolate->is_profiling()) {
      RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate, code,
                                "wasm-to-interpreter", func_index,
                                wasm::WasmName("module"), debug_name);
    }
  }

  Handle<FixedArray> deopt_data = isolate->factory()->NewFixedArray(1, TENURED);
  Handle<WeakCell> weak_instance = isolate->factory()->NewWeakCell(instance);
  deopt_data->set(0, *weak_instance);
  code->set_deoptimization_data(*deopt_data);

  return code;
}

SourcePositionTable* WasmCompilationUnit::BuildGraphForWasmFunction(
    double* decode_ms) {
  base::ElapsedTimer decode_timer;
  if (FLAG_trace_wasm_decode_time) {
    decode_timer.Start();
  }
  // Create a TF graph during decoding.

  Graph* graph = jsgraph_->graph();
  CommonOperatorBuilder* common = jsgraph_->common();
  MachineOperatorBuilder* machine = jsgraph_->machine();
  SourcePositionTable* source_position_table =
      new (jsgraph_->zone()) SourcePositionTable(graph);
  WasmGraphBuilder builder(module_env_, jsgraph_->zone(), jsgraph_,
                           function_->sig, source_position_table);
  const byte* module_start = module_env_->module_bytes.start();
  wasm::FunctionBody body = {function_->sig, module_start,
                             module_start + function_->code_start_offset,
                             module_start + function_->code_end_offset};
  graph_construction_result_ =
      wasm::BuildTFGraph(isolate_->allocator(), &builder, body);

  if (graph_construction_result_.failed()) {
    if (FLAG_trace_wasm_compiler) {
      OFStream os(stdout);
      os << "Compilation failed: " << graph_construction_result_ << std::endl;
    }
    return nullptr;
  }

  if (machine->Is32()) {
    Int64Lowering(graph, machine, common, jsgraph_->zone(), function_->sig)
        .LowerGraph();
  }

  if (builder.has_simd() && !CpuFeatures::SupportsSimd128()) {
    SimdScalarLowering(graph, machine, common, jsgraph_->zone(), function_->sig)
        .LowerGraph();
  }

  int index = static_cast<int>(function_->func_index);

  if (index >= FLAG_trace_wasm_ast_start && index < FLAG_trace_wasm_ast_end) {
    OFStream os(stdout);
    PrintRawWasmCode(isolate_->allocator(), body, module_env_->module);
  }
  if (index >= FLAG_trace_wasm_text_start && index < FLAG_trace_wasm_text_end) {
    OFStream os(stdout);
    PrintWasmText(module_env_->module, *module_env_, function_->func_index, os,
                  nullptr);
  }
  if (FLAG_trace_wasm_decode_time) {
    *decode_ms = decode_timer.Elapsed().InMillisecondsF();
  }
  return source_position_table;
}

WasmCompilationUnit::WasmCompilationUnit(wasm::ErrorThrower* thrower,
                                         Isolate* isolate,
                                         wasm::ModuleBytesEnv* module_env,
                                         const wasm::WasmFunction* function,
                                         uint32_t index)
    : thrower_(thrower),
      isolate_(isolate),
      module_env_(module_env),
      function_(&module_env->module->functions[index]),
      graph_zone_(new Zone(isolate->allocator(), ZONE_NAME)),
      jsgraph_(new (graph_zone()) JSGraph(
          isolate, new (graph_zone()) Graph(graph_zone()),
          new (graph_zone()) CommonOperatorBuilder(graph_zone()), nullptr,
          nullptr, new (graph_zone()) MachineOperatorBuilder(
                       graph_zone(), MachineType::PointerRepresentation(),
                       InstructionSelector::SupportedMachineOperatorFlags(),
                       InstructionSelector::AlignmentRequirements()))),
      compilation_zone_(isolate->allocator(), ZONE_NAME),
      info_(function->name_length != 0 ? module_env->GetNameOrNull(function)
                                       : ArrayVector("wasm"),
            isolate, &compilation_zone_,
            Code::ComputeFlags(Code::WASM_FUNCTION)),
      job_(),
      index_(index),
      ok_(true),
      protected_instructions_(&compilation_zone_) {
  // Create and cache this node in the main thread.
  jsgraph_->CEntryStubConstant(1);
}

void WasmCompilationUnit::ExecuteCompilation() {
  // TODO(ahaas): The counters are not thread-safe at the moment.
  //    HistogramTimerScope wasm_compile_function_time_scope(
  //        isolate_->counters()->wasm_compile_function_time());
  if (FLAG_trace_wasm_compiler) {
    OFStream os(stdout);
    os << "Compiling WASM function "
       << wasm::WasmFunctionName(function_, module_env_) << std::endl;
    os << std::endl;
  }

  double decode_ms = 0;
  size_t node_count = 0;

  std::unique_ptr<Zone> graph_zone(graph_zone_.release());
  SourcePositionTable* source_positions = BuildGraphForWasmFunction(&decode_ms);

  if (graph_construction_result_.failed()) {
    ok_ = false;
    return;
  }

  base::ElapsedTimer pipeline_timer;
  if (FLAG_trace_wasm_decode_time) {
    node_count = jsgraph_->graph()->NodeCount();
    pipeline_timer.Start();
  }

  // Run the compiler pipeline to generate machine code.
  CallDescriptor* descriptor = wasm::ModuleEnv::GetWasmCallDescriptor(
      &compilation_zone_, function_->sig);
  if (jsgraph_->machine()->Is32()) {
    descriptor =
        module_env_->GetI32WasmCallDescriptor(&compilation_zone_, descriptor);
  }
  job_.reset(Pipeline::NewWasmCompilationJob(
      &info_, jsgraph_, descriptor, source_positions, &protected_instructions_,
      module_env_->module->origin != wasm::kWasmOrigin));
  ok_ = job_->ExecuteJob() == CompilationJob::SUCCEEDED;
  // TODO(bradnelson): Improve histogram handling of size_t.
  // TODO(ahaas): The counters are not thread-safe at the moment.
  //    isolate_->counters()->wasm_compile_function_peak_memory_bytes()
  // ->AddSample(
  //        static_cast<int>(jsgraph->graph()->zone()->allocation_size()));

  if (FLAG_trace_wasm_decode_time) {
    double pipeline_ms = pipeline_timer.Elapsed().InMillisecondsF();
    PrintF(
        "wasm-compilation phase 1 ok: %d bytes, %0.3f ms decode, %zu nodes, "
        "%0.3f ms pipeline\n",
        static_cast<int>(function_->code_end_offset -
                         function_->code_start_offset),
        decode_ms, node_count, pipeline_ms);
  }
}

Handle<Code> WasmCompilationUnit::FinishCompilation() {
  if (!ok_) {
    if (graph_construction_result_.failed()) {
      // Add the function as another context for the exception
      ScopedVector<char> buffer(128);
      wasm::WasmName name = module_env_->GetName(function_);
      SNPrintF(buffer, "Compiling WASM function #%d:%.*s failed:",
               function_->func_index, name.length(), name.start());
      thrower_->CompileFailed(buffer.start(), graph_construction_result_);
    }

    return Handle<Code>::null();
  }
  if (job_->FinalizeJob() != CompilationJob::SUCCEEDED) {
    return Handle<Code>::null();
  }
  base::ElapsedTimer compile_timer;
  if (FLAG_trace_wasm_decode_time) {
    compile_timer.Start();
  }
  Handle<Code> code = info_.code();
  DCHECK(!code.is_null());

  if (isolate_->logger()->is_logging_code_events() ||
      isolate_->is_profiling()) {
    RecordFunctionCompilation(CodeEventListener::FUNCTION_TAG, isolate_, code,
                              "WASM_function", function_->func_index,
                              wasm::WasmName("module"),
                              module_env_->GetName(function_));
  }

  if (FLAG_trace_wasm_decode_time) {
    double compile_ms = compile_timer.Elapsed().InMillisecondsF();
    PrintF("wasm-code-generation ok: %d bytes, %0.3f ms code generation\n",
           static_cast<int>(function_->code_end_offset -
                            function_->code_start_offset),
           compile_ms);
  }

  Handle<FixedArray> protected_instructions = PackProtectedInstructions();
  code->set_protected_instructions(*protected_instructions);

  return code;
}

Handle<FixedArray> WasmCompilationUnit::PackProtectedInstructions() const {
  const int num_instructions = static_cast<int>(protected_instructions_.size());
  Handle<FixedArray> fn_protected = isolate_->factory()->NewFixedArray(
      num_instructions * Code::kTrapDataSize, TENURED);
  for (unsigned i = 0; i < protected_instructions_.size(); ++i) {
    const trap_handler::ProtectedInstructionData& instruction =
        protected_instructions_[i];
    fn_protected->set(Code::kTrapDataSize * i + Code::kTrapCodeOffset,
                      Smi::FromInt(instruction.instr_offset));
    fn_protected->set(Code::kTrapDataSize * i + Code::kTrapLandingOffset,
                      Smi::FromInt(instruction.landing_offset));
  }
  return fn_protected;
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
