// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/wasm-compiler.h"

#include <memory>

#include "src/base/optional.h"
#include "src/base/platform/elapsed-timer.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/wrappers.h"
#include "src/base/small-vector.h"
#include "src/base/v8-fallthrough.h"
#include "src/codegen/assembler-inl.h"
#include "src/codegen/assembler.h"
#include "src/codegen/code-factory.h"
#include "src/codegen/compiler.h"
#include "src/codegen/interface-descriptors.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/backend/code-generator.h"
#include "src/compiler/backend/instruction-selector.h"
#include "src/compiler/common-operator.h"
#include "src/compiler/compiler-source-position-table.h"
#include "src/compiler/diamond.h"
#include "src/compiler/graph-assembler.h"
#include "src/compiler/graph-visualizer.h"
#include "src/compiler/graph.h"
#include "src/compiler/int64-lowering.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/node-matchers.h"
#include "src/compiler/node-origin-table.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/simd-scalar-lowering.h"
#include "src/compiler/zone-stats.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/heap-number.h"
#include "src/roots/roots.h"
#include "src/tracing/trace-event.h"
#include "src/trap-handler/trap-handler.h"
#include "src/utils/vector.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/function-compiler.h"
#include "src/wasm/graph-builder-interface.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/memory-tracing.h"
#include "src/wasm/object-access.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-linkage.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

namespace {

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

#define LOAD_MUTABLE_INSTANCE_FIELD(name, type)                          \
  gasm_->LoadFromObject(                                                 \
      assert_size(WASM_INSTANCE_OBJECT_SIZE(name), type), GetInstance(), \
      wasm::ObjectAccess::ToTagged(WasmInstanceObject::k##name##Offset))

// TODO(11510): Using LoadImmutable for tagged values causes registers to be
// spilled and added to the safepoint table, resulting in large code size
// regressions. A possible solution would be to not spill the register at all,
// but rather reload the value from memory. This will require non-trivial
// changes in the register allocator and instuction selector.
#define LOAD_INSTANCE_FIELD(name, type)                          \
  (CanBeTaggedOrCompressedPointer((type).representation())       \
       ? LOAD_MUTABLE_INSTANCE_FIELD(name, type)                 \
       : gasm_->LoadImmutable(                                   \
             assert_size(WASM_INSTANCE_OBJECT_SIZE(name), type), \
             GetInstance(),                                      \
             wasm::ObjectAccess::ToTagged(                       \
                 WasmInstanceObject::k##name##Offset)))

#define LOAD_ROOT(root_name, factory_name)                   \
  (use_js_isolate_and_params()                               \
       ? graph()->NewNode(mcgraph()->common()->HeapConstant( \
             isolate_->factory()->factory_name()))           \
       : gasm_->LoadImmutable(                               \
             MachineType::Pointer(), BuildLoadIsolateRoot(), \
             IsolateData::root_slot_offset(RootIndex::k##root_name)))

bool ContainsSimd(const wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmS128) return true;
  }
  return false;
}

bool ContainsInt64(const wasm::FunctionSig* sig) {
  for (auto type : sig->all()) {
    if (type == wasm::kWasmI64) return true;
  }
  return false;
}

constexpr Builtins::Name WasmRuntimeStubIdToBuiltinName(
    wasm::WasmCode::RuntimeStubId runtime_stub_id) {
  switch (runtime_stub_id) {
#define DEF_CASE(name)          \
  case wasm::WasmCode::k##name: \
    return Builtins::k##name;
#define DEF_TRAP_CASE(name) DEF_CASE(ThrowWasm##name)
    WASM_RUNTIME_STUB_LIST(DEF_CASE, DEF_TRAP_CASE)
#undef DEF_CASE
#undef DEF_TRAP_CASE
    default:
#if V8_HAS_CXX14_CONSTEXPR
      UNREACHABLE();
#else
      return Builtins::kAbort;
#endif
  }
}

CallDescriptor* GetBuiltinCallDescriptor(
    Builtins::Name name, Zone* zone, StubCallMode stub_mode,
    bool needs_frame_state = false,
    Operator::Properties properties = Operator::kNoProperties) {
  CallInterfaceDescriptor interface_descriptor =
      Builtins::CallInterfaceDescriptorFor(name);
  return Linkage::GetStubCallDescriptor(
      zone,                                           // zone
      interface_descriptor,                           // descriptor
      interface_descriptor.GetStackParameterCount(),  // stack parameter count
      needs_frame_state ? CallDescriptor::kNeedsFrameState
                        : CallDescriptor::kNoFlags,  // flags
      properties,                                    // properties
      stub_mode);                                    // stub call mode
}

ObjectAccess ObjectAccessForGCStores(wasm::ValueType type) {
  return ObjectAccess(
      MachineType::TypeForRepresentation(type.machine_representation(),
                                         !type.is_packed()),
      type.is_reference() ? kFullWriteBarrier : kNoWriteBarrier);
}
}  // namespace

JSWasmCallData::JSWasmCallData(const wasm::FunctionSig* wasm_signature)
    : result_needs_conversion_(wasm_signature->return_count() == 1 &&
                               wasm_signature->GetReturn().kind() ==
                                   wasm::kI64) {
  arg_needs_conversion_.resize(wasm_signature->parameter_count());
  for (size_t i = 0; i < wasm_signature->parameter_count(); i++) {
    wasm::ValueType type = wasm_signature->GetParam(i);
    arg_needs_conversion_[i] = type.kind() == wasm::kI64;
  }
}

class WasmGraphAssembler : public GraphAssembler {
 public:
  WasmGraphAssembler(MachineGraph* mcgraph, Zone* zone)
      : GraphAssembler(mcgraph, zone), simplified_(zone) {}

  template <typename... Args>
  Node* CallRuntimeStub(wasm::WasmCode::RuntimeStubId stub_id, Args*... args) {
    auto* call_descriptor = GetBuiltinCallDescriptor(
        WasmRuntimeStubIdToBuiltinName(stub_id), temp_zone(),
        StubCallMode::kCallWasmRuntimeStub);
    // A direct call to a wasm runtime stub defined in this module.
    // Just encode the stub index. This will be patched at relocation.
    Node* call_target = mcgraph()->RelocatableIntPtrConstant(
        stub_id, RelocInfo::WASM_STUB_CALL);
    return Call(call_descriptor, call_target, args...);
  }

  template <typename... Args>
  Node* CallBuiltin(Builtins::Name name, Operator::Properties properties,
                    Args*... args) {
    auto* call_descriptor = GetBuiltinCallDescriptor(
        name, temp_zone(), StubCallMode::kCallBuiltinPointer, false,
        properties);
    Node* call_target = GetBuiltinPointerTarget(name);
    return Call(call_descriptor, call_target, args...);
  }

  void EnsureEnd() {
    if (graph()->end() == nullptr) {
      graph()->SetEnd(graph()->NewNode(mcgraph()->common()->End(0)));
    }
  }

  void MergeControlToEnd(Node* node) {
    EnsureEnd();
    NodeProperties::MergeControlToEnd(graph(), mcgraph()->common(), node);
  }

  void AssertFalse(Node* condition) {
#if DEBUG
    if (FLAG_debug_code) {
      auto ok = MakeLabel();
      GotoIfNot(condition, &ok);
      EnsureEnd();
      Unreachable();
      Bind(&ok);
    }
#endif
  }

  Node* GetBuiltinPointerTarget(Builtins::Name builtin_id) {
    static_assert(std::is_same<Smi, BuiltinPtr>(), "BuiltinPtr must be Smi");
    return NumberConstant(builtin_id);
  }

  // Sets {true_node} and {false_node} to their corresponding Branch outputs.
  // Returns the Branch node. Does not change control().
  Node* Branch(Node* cond, Node** true_node, Node** false_node,
               BranchHint hint) {
    DCHECK_NOT_NULL(cond);
    Node* branch =
        graph()->NewNode(mcgraph()->common()->Branch(hint), cond, control());
    *true_node = graph()->NewNode(mcgraph()->common()->IfTrue(), branch);
    *false_node = graph()->NewNode(mcgraph()->common()->IfFalse(), branch);
    return branch;
  }

  Node* NumberConstant(volatile double value) {
    return graph()->NewNode(mcgraph()->common()->NumberConstant(value));
  }

  // Helper functions for dealing with HeapObjects.
  // Rule of thumb: if access to a given field in an object is required in
  // at least two places, put a helper function here.

  Node* LoadFromObject(MachineType type, Node* base, Node* offset) {
    return AddNode(graph()->NewNode(
        simplified_.LoadFromObject(ObjectAccess(type, kNoWriteBarrier)), base,
        offset, effect(), control()));
  }

  Node* LoadFromObject(MachineType type, Node* base, int offset) {
    return LoadFromObject(type, base, IntPtrConstant(offset));
  }

  Node* LoadImmutable(LoadRepresentation rep, Node* base, Node* offset) {
    return AddNode(graph()->NewNode(mcgraph()->machine()->LoadImmutable(rep),
                                    base, offset));
  }

  Node* LoadImmutable(LoadRepresentation rep, Node* base, int offset) {
    return LoadImmutable(rep, base, IntPtrConstant(offset));
  }

  Node* StoreToObject(ObjectAccess access, Node* base, Node* offset,
                      Node* value) {
    return AddNode(graph()->NewNode(simplified_.StoreToObject(access), base,
                                    offset, value, effect(), control()));
  }

  Node* StoreToObject(ObjectAccess access, Node* base, int offset,
                      Node* value) {
    return StoreToObject(access, base, IntPtrConstant(offset), value);
  }

  Node* IsI31(Node* object) {
    if (COMPRESS_POINTERS_BOOL) {
      return Word32Equal(Word32And(object, Int32Constant(kSmiTagMask)),
                         Int32Constant(kSmiTag));
    } else {
      return WordEqual(WordAnd(object, IntPtrConstant(kSmiTagMask)),
                       IntPtrConstant(kSmiTag));
    }
  }

  // Maps and their contents.

  Node* LoadMap(Node* heap_object) {
    return LoadFromObject(MachineType::TaggedPointer(), heap_object,
                          wasm::ObjectAccess::ToTagged(HeapObject::kMapOffset));
  }
  Node* LoadInstanceType(Node* map) {
    return LoadFromObject(
        MachineType::Uint16(), map,
        wasm::ObjectAccess::ToTagged(Map::kInstanceTypeOffset));
  }
  Node* LoadWasmTypeInfo(Node* map) {
    int offset = Map::kConstructorOrBackPointerOrNativeContextOffset;
    return LoadFromObject(MachineType::TaggedPointer(), map,
                          wasm::ObjectAccess::ToTagged(offset));
  }

  Node* LoadSupertypes(Node* wasm_type_info) {
    return LoadFromObject(
        MachineType::TaggedPointer(), wasm_type_info,
        wasm::ObjectAccess::ToTagged(WasmTypeInfo::kSupertypesOffset));
  }

  // FixedArrays.

  Node* LoadFixedArrayLengthAsSmi(Node* fixed_array) {
    return LoadFromObject(
        MachineType::TaggedSigned(), fixed_array,
        wasm::ObjectAccess::ToTagged(FixedArray::kLengthOffset));
  }

  Node* LoadFixedArrayElement(Node* fixed_array, Node* index_intptr,
                              MachineType type = MachineType::AnyTagged()) {
    Node* offset = IntAdd(
        IntMul(index_intptr, IntPtrConstant(kTaggedSize)),
        IntPtrConstant(wasm::ObjectAccess::ToTagged(FixedArray::kHeaderSize)));
    return LoadFromObject(type, fixed_array, offset);
  }

  Node* LoadFixedArrayElement(Node* array, int index, MachineType type) {
    return LoadFromObject(
        type, array,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(index));
  }

  Node* LoadFixedArrayElementSmi(Node* array, int index) {
    return LoadFixedArrayElement(array, index, MachineType::TaggedSigned());
  }

  Node* LoadFixedArrayElementPtr(Node* array, int index) {
    return LoadFixedArrayElement(array, index, MachineType::TaggedPointer());
  }

  Node* LoadFixedArrayElementAny(Node* array, int index) {
    return LoadFixedArrayElement(array, index, MachineType::AnyTagged());
  }

  Node* StoreFixedArrayElement(Node* array, int index, Node* value,
                               ObjectAccess access) {
    return StoreToObject(
        access, array,
        wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(index), value);
  }

  Node* StoreFixedArrayElementSmi(Node* array, int index, Node* value) {
    return StoreFixedArrayElement(
        array, index, value,
        ObjectAccess(MachineType::TaggedSigned(), kNoWriteBarrier));
  }

  Node* StoreFixedArrayElementAny(Node* array, int index, Node* value) {
    return StoreFixedArrayElement(
        array, index, value,
        ObjectAccess(MachineType::AnyTagged(), kFullWriteBarrier));
  }

  // Functions, SharedFunctionInfos, FunctionData.

  Node* LoadSharedFunctionInfo(Node* js_function) {
    return LoadFromObject(
        MachineType::TaggedPointer(), js_function,
        wasm::ObjectAccess::SharedFunctionInfoOffsetInTaggedJSFunction());
  }
  Node* LoadContextFromJSFunction(Node* js_function) {
    return LoadFromObject(
        MachineType::TaggedPointer(), js_function,
        wasm::ObjectAccess::ContextOffsetInTaggedJSFunction());
  }

  Node* LoadFunctionDataFromJSFunction(Node* js_function) {
    Node* shared = LoadSharedFunctionInfo(js_function);
    return LoadFromObject(
        MachineType::TaggedPointer(), shared,
        wasm::ObjectAccess::ToTagged(SharedFunctionInfo::kFunctionDataOffset));
  }

  Node* LoadExportedFunctionIndexAsSmi(Node* exported_function_data) {
    return LoadFromObject(MachineType::TaggedSigned(), exported_function_data,
                          wasm::ObjectAccess::ToTagged(
                              WasmExportedFunctionData::kFunctionIndexOffset));
  }
  Node* LoadExportedFunctionInstance(Node* exported_function_data) {
    return LoadFromObject(MachineType::TaggedPointer(), exported_function_data,
                          wasm::ObjectAccess::ToTagged(
                              WasmExportedFunctionData::kInstanceOffset));
  }

  // JavaScript objects.

  Node* LoadJSArrayElements(Node* js_array) {
    return LoadFromObject(
        MachineType::AnyTagged(), js_array,
        wasm::ObjectAccess::ToTagged(JSObject::kElementsOffset));
  }

  // WasmGC objects.

  Node* FieldOffset(const wasm::StructType* type, uint32_t field_index) {
    return IntPtrConstant(wasm::ObjectAccess::ToTagged(
        WasmStruct::kHeaderSize + type->field_offset(field_index)));
  }

  Node* StoreStructField(Node* struct_object, const wasm::StructType* type,
                         uint32_t field_index, Node* value) {
    return StoreToObject(ObjectAccessForGCStores(type->field(field_index)),
                         struct_object, FieldOffset(type, field_index), value);
  }

  Node* WasmArrayElementOffset(Node* index, wasm::ValueType element_type) {
    return Int32Add(
        Int32Constant(wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize)),
        Int32Mul(index, Int32Constant(element_type.element_size_bytes())));
  }

  Node* LoadWasmArrayLength(Node* array) {
    return LoadFromObject(
        MachineType::Uint32(), array,
        wasm::ObjectAccess::ToTagged(WasmArray::kLengthOffset));
  }

  Node* IsDataRefMap(Node* map) {
    Node* instance_type = LoadInstanceType(map);
    // We're going to test a range of instance types with a single unsigned
    // comparison. Statically assert that this is safe, i.e. that there are
    // no instance types between array and struct types that might possibly
    // occur (i.e. internal types are OK, types of Wasm objects are not).
    // At the time of this writing:
    // WASM_ARRAY_TYPE = 180
    // WASM_CAPI_FUNCTION_DATA_TYPE = 181
    // WASM_STRUCT_TYPE = 182
    // The specific values don't matter; the relative order does.
    static_assert(
        WASM_STRUCT_TYPE == static_cast<InstanceType>(WASM_ARRAY_TYPE + 2),
        "Relying on specific InstanceType values here");
    static_assert(WASM_CAPI_FUNCTION_DATA_TYPE ==
                      static_cast<InstanceType>(WASM_ARRAY_TYPE + 1),
                  "Relying on specific InstanceType values here");
    Node* comparison_value =
        Int32Sub(instance_type, Int32Constant(WASM_ARRAY_TYPE));
    return Uint32LessThanOrEqual(
        comparison_value, Int32Constant(WASM_STRUCT_TYPE - WASM_ARRAY_TYPE));
  }

  // Generic HeapObject helpers.

  Node* HasInstanceType(Node* heap_object, InstanceType type) {
    Node* map = LoadMap(heap_object);
    Node* instance_type = LoadInstanceType(map);
    return Word32Equal(instance_type, Int32Constant(type));
  }

  SimplifiedOperatorBuilder* simplified() { return &simplified_; }

 private:
  SimplifiedOperatorBuilder simplified_;
};

WasmGraphBuilder::WasmGraphBuilder(
    wasm::CompilationEnv* env, Zone* zone, MachineGraph* mcgraph,
    const wasm::FunctionSig* sig,
    compiler::SourcePositionTable* source_position_table, Isolate* isolate)
    : gasm_(std::make_unique<WasmGraphAssembler>(mcgraph, zone)),
      zone_(zone),
      mcgraph_(mcgraph),
      env_(env),
      has_simd_(ContainsSimd(sig)),
      untrusted_code_mitigations_(FLAG_untrusted_code_mitigations),
      sig_(sig),
      source_position_table_(source_position_table),
      isolate_(isolate) {
  DCHECK_IMPLIES(use_trap_handler(), trap_handler::IsTrapHandlerEnabled());
  DCHECK_NOT_NULL(mcgraph_);
}

// Destructor define here where the definition of {WasmGraphAssembler} is
// available.
WasmGraphBuilder::~WasmGraphBuilder() = default;

void WasmGraphBuilder::Start(unsigned params) {
  Node* start = graph()->NewNode(mcgraph()->common()->Start(params));
  graph()->SetStart(start);
  SetEffectControl(start);
  // Initialize parameter nodes.
  parameters_ = zone_->NewArray<Node*>(params);
  for (unsigned i = 0; i < params; i++) {
    parameters_[i] = nullptr;
  }
  // Initialize instance node.
  instance_node_ =
      use_js_isolate_and_params()
          ? gasm_->LoadExportedFunctionInstance(
                gasm_->LoadFunctionDataFromJSFunction(
                    Param(Linkage::kJSCallClosureParamIndex, "%closure")))
          : Param(wasm::kWasmInstanceParameterIndex);
}

Node* WasmGraphBuilder::Param(int index, const char* debug_name) {
  DCHECK_NOT_NULL(graph()->start());
  // Turbofan allows negative parameter indices.
  static constexpr int kMinParameterIndex = -1;
  DCHECK_GE(index, kMinParameterIndex);
  int array_index = index - kMinParameterIndex;
  if (parameters_[array_index] == nullptr) {
    parameters_[array_index] = graph()->NewNode(
        mcgraph()->common()->Parameter(index, debug_name), graph()->start());
  }
  return parameters_[array_index];
}

Node* WasmGraphBuilder::Loop(Node* entry) {
  return graph()->NewNode(mcgraph()->common()->Loop(1), entry);
}

void WasmGraphBuilder::TerminateLoop(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(mcgraph()->common()->Terminate(), effect, control);
  gasm_->MergeControlToEnd(terminate);
}

Node* WasmGraphBuilder::LoopExit(Node* loop_node) {
  DCHECK(loop_node->opcode() == IrOpcode::kLoop);
  Node* loop_exit =
      graph()->NewNode(mcgraph()->common()->LoopExit(), control(), loop_node);
  Node* loop_exit_effect = graph()->NewNode(
      mcgraph()->common()->LoopExitEffect(), effect(), loop_exit);
  SetEffectControl(loop_exit_effect, loop_exit);
  return loop_exit;
}

Node* WasmGraphBuilder::LoopExitValue(Node* value,
                                      MachineRepresentation representation) {
  DCHECK(control()->opcode() == IrOpcode::kLoopExit);
  return graph()->NewNode(mcgraph()->common()->LoopExitValue(representation),
                          value, control());
}

void WasmGraphBuilder::TerminateThrow(Node* effect, Node* control) {
  Node* terminate =
      graph()->NewNode(mcgraph()->common()->Throw(), effect, control);
  gasm_->MergeControlToEnd(terminate);
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

template <typename... Nodes>
Node* WasmGraphBuilder::Merge(Node* fst, Nodes*... args) {
  return graph()->NewNode(this->mcgraph()->common()->Merge(1 + sizeof...(args)),
                          fst, args...);
}

Node* WasmGraphBuilder::Merge(unsigned count, Node** controls) {
  return graph()->NewNode(mcgraph()->common()->Merge(count), count, controls);
}

Node* WasmGraphBuilder::Phi(wasm::ValueType type, unsigned count,
                            Node** vals_and_control) {
  DCHECK(IrOpcode::IsMergeOpcode(vals_and_control[count]->opcode()));
  DCHECK_EQ(vals_and_control[count]->op()->ControlInputCount(), count);
  return graph()->NewNode(
      mcgraph()->common()->Phi(type.machine_representation(), count), count + 1,
      vals_and_control);
}

Node* WasmGraphBuilder::EffectPhi(unsigned count, Node** effects_and_control) {
  DCHECK(IrOpcode::IsMergeOpcode(effects_and_control[count]->opcode()));
  return graph()->NewNode(mcgraph()->common()->EffectPhi(count), count + 1,
                          effects_and_control);
}

Node* WasmGraphBuilder::RefNull() { return LOAD_ROOT(NullValue, null_value); }

Node* WasmGraphBuilder::RefFunc(uint32_t function_index) {
  return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmRefFunc,
                                gasm_->Uint32Constant(function_index));
}

Node* WasmGraphBuilder::RefAsNonNull(Node* arg,
                                     wasm::WasmCodePosition position) {
  TrapIfTrue(wasm::kTrapIllegalCast, gasm_->WordEqual(arg, RefNull()),
             position);
  return arg;
}

Node* WasmGraphBuilder::NoContextConstant() {
  return mcgraph()->IntPtrConstant(0);
}

Node* WasmGraphBuilder::GetInstance() { return instance_node_.get(); }

Node* WasmGraphBuilder::BuildLoadIsolateRoot() {
  if (use_js_isolate_and_params()) {
    return mcgraph()->IntPtrConstant(isolate_->isolate_root());
  } else {
    // For wasm functions, the IsolateRoot is loaded from the instance node so
    // that the generated code is Isolate independent.
    return LOAD_INSTANCE_FIELD(IsolateRoot, MachineType::Pointer());
  }
}

Node* WasmGraphBuilder::Int32Constant(int32_t value) {
  return mcgraph()->Int32Constant(value);
}

Node* WasmGraphBuilder::Int64Constant(int64_t value) {
  return mcgraph()->Int64Constant(value);
}

void WasmGraphBuilder::StackCheck(wasm::WasmCodePosition position) {
  DCHECK_NOT_NULL(env_);  // Wrappers don't get stack checks.
  if (!FLAG_wasm_stack_checks || !env_->runtime_exception_support) {
    return;
  }

  Node* limit_address =
      LOAD_INSTANCE_FIELD(StackLimitAddress, MachineType::Pointer());
  Node* limit = gasm_->LoadFromObject(MachineType::Pointer(), limit_address, 0);

  Node* check = SetEffect(graph()->NewNode(
      mcgraph()->machine()->StackPointerGreaterThan(StackCheckKind::kWasm),
      limit, effect()));

  Diamond stack_check(graph(), mcgraph()->common(), check, BranchHint::kTrue);
  stack_check.Chain(control());

  if (stack_check_call_operator_ == nullptr) {
    // Build and cache the stack check call operator and the constant
    // representing the stack check code.

    // A direct call to a wasm runtime stub defined in this module.
    // Just encode the stub index. This will be patched at relocation.
    stack_check_code_node_.set(mcgraph()->RelocatableIntPtrConstant(
        wasm::WasmCode::kWasmStackGuard, RelocInfo::WASM_STUB_CALL));

    auto call_descriptor = Linkage::GetStubCallDescriptor(
        mcgraph()->zone(),                    // zone
        NoContextDescriptor{},                // descriptor
        0,                                    // stack parameter count
        CallDescriptor::kNoFlags,             // flags
        Operator::kNoProperties,              // properties
        StubCallMode::kCallWasmRuntimeStub);  // stub call mode
    stack_check_call_operator_ = mcgraph()->common()->Call(call_descriptor);
  }

  Node* call = graph()->NewNode(stack_check_call_operator_.get(),
                                stack_check_code_node_.get(), effect(),
                                stack_check.if_false);

  SetSourcePosition(call, position);

  Node* ephi = stack_check.EffectPhi(effect(), call);

  SetEffectControl(ephi, stack_check.merge);
}

void WasmGraphBuilder::PatchInStackCheckIfNeeded() {
  if (!needs_stack_check_) return;

  Node* start = graph()->start();
  // Place a stack check which uses a dummy node as control and effect.
  Node* dummy = graph()->NewNode(mcgraph()->common()->Dead());
  SetEffectControl(dummy);
  // The function-prologue stack check is associated with position 0, which
  // is never a position of any instruction in the function.
  StackCheck(0);

  // In testing, no steck checks were emitted. Nothing to rewire then.
  if (effect() == dummy) return;

  // Now patch all control uses of {start} to use {control} and all effect uses
  // to use {effect} instead. Then rewire the dummy node to use start instead.
  NodeProperties::ReplaceUses(start, start, effect(), control());
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
      if (m->Word32Rol().IsSupported()) {
        op = m->Word32Rol().op();
        right = MaskShiftCount32(right);
        break;
      }
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
      if (m->Word64Rol().IsSupported()) {
        op = m->Word64Rol().op();
        right = MaskShiftCount64(right);
        break;
      } else if (m->Word32Rol().IsSupported()) {
        op = m->Word64Rol().placeholder();
        break;
      }
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
    case wasm::kExprRefEq:
      return gasm_->TaggedEqual(left, right);
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
      return gasm_->Word32Equal(input, Int32Constant(0));
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
      return gasm_->Word64Equal(input, Int64Constant(0));
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
      return gasm_->WordEqual(input, RefNull());
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

Node* WasmGraphBuilder::Simd128Constant(const uint8_t value[16]) {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->S128Const(value));
}

Node* WasmGraphBuilder::BranchNoHint(Node* cond, Node** true_node,
                                     Node** false_node) {
  return gasm_->Branch(cond, true_node, false_node, BranchHint::kNone);
}

Node* WasmGraphBuilder::BranchExpectFalse(Node* cond, Node** true_node,
                                          Node** false_node) {
  return gasm_->Branch(cond, true_node, false_node, BranchHint::kFalse);
}

Node* WasmGraphBuilder::Select(Node *cond, Node* true_node,
                               Node* false_node, wasm::ValueType type) {
  MachineOperatorBuilder* m = mcgraph()->machine();
  wasm::ValueKind kind = type.kind();
  // Lower to select if supported.
  if (kind == wasm::kF32 && m->Float32Select().IsSupported()) {
    return mcgraph()->graph()->NewNode(m->Float32Select().op(), cond,
                                       true_node, false_node);
  }
  if (kind == wasm::kF64 && m->Float64Select().IsSupported()) {
    return mcgraph()->graph()->NewNode(m->Float64Select().op(), cond,
                                       true_node, false_node);
  }
  // Default to control-flow.
  Node* controls[2];
  BranchNoHint(cond, &controls[0], &controls[1]);
  Node* merge = Merge(2, controls);
  SetControl(merge);
  Node* inputs[] = {true_node, false_node, merge};
  return Phi(type, 2, inputs);
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

void WasmGraphBuilder::TrapIfTrue(wasm::TrapReason reason, Node* cond,
                                  wasm::WasmCodePosition position) {
  TrapId trap_id = GetTrapIdForTrap(reason);
  Node* node = SetControl(graph()->NewNode(mcgraph()->common()->TrapIf(trap_id),
                                           cond, effect(), control()));
  SetSourcePosition(node, position);
}

void WasmGraphBuilder::TrapIfFalse(wasm::TrapReason reason, Node* cond,
                                   wasm::WasmCodePosition position) {
  TrapId trap_id = GetTrapIdForTrap(reason);
  Node* node = SetControl(graph()->NewNode(
      mcgraph()->common()->TrapUnless(trap_id), cond, effect(), control()));
  SetSourcePosition(node, position);
}

// Add a check that traps if {node} is equal to {val}.
void WasmGraphBuilder::TrapIfEq32(wasm::TrapReason reason, Node* node,
                                  int32_t val,
                                  wasm::WasmCodePosition position) {
  Int32Matcher m(node);
  if (m.HasResolvedValue() && !m.Is(val)) return;
  if (val == 0) {
    TrapIfFalse(reason, node, position);
  } else {
    TrapIfTrue(reason, gasm_->Word32Equal(node, Int32Constant(val)), position);
  }
}

// Add a check that traps if {node} is zero.
void WasmGraphBuilder::ZeroCheck32(wasm::TrapReason reason, Node* node,
                                   wasm::WasmCodePosition position) {
  TrapIfEq32(reason, node, 0, position);
}

// Add a check that traps if {node} is equal to {val}.
void WasmGraphBuilder::TrapIfEq64(wasm::TrapReason reason, Node* node,
                                  int64_t val,
                                  wasm::WasmCodePosition position) {
  Int64Matcher m(node);
  if (m.HasResolvedValue() && !m.Is(val)) return;
  TrapIfTrue(reason, gasm_->Word64Equal(node, Int64Constant(val)), position);
}

// Add a check that traps if {node} is zero.
void WasmGraphBuilder::ZeroCheck64(wasm::TrapReason reason, Node* node,
                                   wasm::WasmCodePosition position) {
  TrapIfEq64(reason, node, 0, position);
}

Node* WasmGraphBuilder::Switch(unsigned count, Node* key) {
  // The instruction selector will use {kArchTableSwitch} for large switches,
  // which has limited input count, see {InstructionSelector::EmitTableSwitch}.
  DCHECK_LE(count, Instruction::kMaxInputCount - 2);          // value_range + 2
  DCHECK_LE(count, wasm::kV8MaxWasmFunctionBrTableSize + 1);  // plus IfDefault
  return graph()->NewNode(mcgraph()->common()->Switch(count), key, control());
}

Node* WasmGraphBuilder::IfValue(int32_t value, Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(mcgraph()->common()->IfValue(value), sw);
}

Node* WasmGraphBuilder::IfDefault(Node* sw) {
  DCHECK_EQ(IrOpcode::kSwitch, sw->opcode());
  return graph()->NewNode(mcgraph()->common()->IfDefault(), sw);
}

Node* WasmGraphBuilder::Return(Vector<Node*> vals) {
  unsigned count = static_cast<unsigned>(vals.size());
  base::SmallVector<Node*, 8> buf(count + 3);

  buf[0] = Int32Constant(0);
  if (count > 0) {
    base::Memcpy(buf.data() + 1, vals.begin(), sizeof(void*) * count);
  }
  buf[count + 1] = effect();
  buf[count + 2] = control();
  Node* ret = graph()->NewNode(mcgraph()->common()->Return(count), count + 3,
                               buf.data());

  gasm_->MergeControlToEnd(ret);
  return ret;
}

void WasmGraphBuilder::Trap(wasm::TrapReason reason,
                            wasm::WasmCodePosition position) {
  TrapIfFalse(reason, Int32Constant(0), position);
  // Connect control to end via a Throw() node.
  TerminateThrow(effect(), control());
}

Node* WasmGraphBuilder::MaskShiftCount32(Node* node) {
  static const int32_t kMask32 = 0x1F;
  if (!mcgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int32Matcher match(node);
    if (match.HasResolvedValue()) {
      int32_t masked = (match.ResolvedValue() & kMask32);
      if (match.ResolvedValue() != masked) node = Int32Constant(masked);
    } else {
      node = gasm_->Word32And(node, Int32Constant(kMask32));
    }
  }
  return node;
}

Node* WasmGraphBuilder::MaskShiftCount64(Node* node) {
  static const int64_t kMask64 = 0x3F;
  if (!mcgraph()->machine()->Word32ShiftIsSafe()) {
    // Shifts by constants are so common we pattern-match them here.
    Int64Matcher match(node);
    if (match.HasResolvedValue()) {
      int64_t masked = (match.ResolvedValue() & kMask64);
      if (match.ResolvedValue() != masked) node = Int64Constant(masked);
    } else {
      node = gasm_->Word64And(node, Int64Constant(kMask64));
    }
  }
  return node;
}

namespace {

bool ReverseBytesSupported(MachineOperatorBuilder* m, size_t size_in_bytes) {
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

}  // namespace

Node* WasmGraphBuilder::BuildChangeEndiannessStore(
    Node* node, MachineRepresentation mem_rep, wasm::ValueType wasmtype) {
  Node* result;
  Node* value = node;
  MachineOperatorBuilder* m = mcgraph()->machine();
  int valueSizeInBytes = wasmtype.element_size_bytes();
  int valueSizeInBits = 8 * valueSizeInBytes;
  bool isFloat = false;

  switch (wasmtype.kind()) {
    case wasm::kF64:
      value = gasm_->BitcastFloat64ToInt64(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case wasm::kI64:
      result = Int64Constant(0);
      break;
    case wasm::kF32:
      value = gasm_->BitcastFloat32ToInt32(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case wasm::kI32:
      result = Int32Constant(0);
      break;
    case wasm::kS128:
      DCHECK(ReverseBytesSupported(m, valueSizeInBytes));
      break;
    default:
      UNREACHABLE();
  }

  if (mem_rep == MachineRepresentation::kWord8) {
    // No need to change endianness for byte size, return original node
    return node;
  }
  if (wasmtype == wasm::kWasmI64 && mem_rep < MachineRepresentation::kWord64) {
    // In case we store lower part of WasmI64 expression, we can truncate
    // upper 32bits
    value = gasm_->TruncateInt64ToInt32(value);
    valueSizeInBytes = wasm::kWasmI32.element_size_bytes();
    valueSizeInBits = 8 * valueSizeInBytes;
    if (mem_rep == MachineRepresentation::kWord16) {
      value = gasm_->Word32Shl(value, Int32Constant(16));
    }
  } else if (wasmtype == wasm::kWasmI32 &&
             mem_rep == MachineRepresentation::kWord16) {
    value = gasm_->Word32Shl(value, Int32Constant(16));
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 4:
        result = gasm_->Word32ReverseBytes(value);
        break;
      case 8:
        result = gasm_->Word64ReverseBytes(value);
        break;
      case 16:
        result = graph()->NewNode(m->Simd128ReverseBytes(), value);
        break;
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
        shiftLower = gasm_->Word64Shl(value, Int64Constant(shiftCount));
        shiftHigher = gasm_->Word64Shr(value, Int64Constant(shiftCount));
        lowerByte = gasm_->Word64And(
            shiftLower, Int64Constant(static_cast<uint64_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word64And(
            shiftHigher, Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = gasm_->Word64Or(result, lowerByte);
        result = gasm_->Word64Or(result, higherByte);
      } else {
        shiftLower = gasm_->Word32Shl(value, Int32Constant(shiftCount));
        shiftHigher = gasm_->Word32Shr(value, Int32Constant(shiftCount));
        lowerByte = gasm_->Word32And(
            shiftLower, Int32Constant(static_cast<uint32_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word32And(
            shiftHigher, Int32Constant(static_cast<uint32_t>(0xFF) << i));
        result = gasm_->Word32Or(result, lowerByte);
        result = gasm_->Word32Or(result, higherByte);
      }
    }
  }

  if (isFloat) {
    switch (wasmtype.kind()) {
      case wasm::kF64:
        result = gasm_->BitcastInt64ToFloat64(result);
        break;
      case wasm::kF32:
        result = gasm_->BitcastInt32ToFloat32(result);
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
      value = gasm_->BitcastFloat64ToInt64(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case MachineRepresentation::kWord64:
      result = Int64Constant(0);
      break;
    case MachineRepresentation::kFloat32:
      value = gasm_->BitcastFloat32ToInt32(node);
      isFloat = true;
      V8_FALLTHROUGH;
    case MachineRepresentation::kWord32:
    case MachineRepresentation::kWord16:
      result = Int32Constant(0);
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
  }

  int i;
  uint32_t shiftCount;

  if (ReverseBytesSupported(m, valueSizeInBytes < 4 ? 4 : valueSizeInBytes)) {
    switch (valueSizeInBytes) {
      case 2:
        result = gasm_->Word32ReverseBytes(
            gasm_->Word32Shl(value, Int32Constant(16)));
        break;
      case 4:
        result = gasm_->Word32ReverseBytes(value);
        break;
      case 8:
        result = gasm_->Word64ReverseBytes(value);
        break;
      case 16:
        result = graph()->NewNode(m->Simd128ReverseBytes(), value);
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

      DCHECK_LT(0, shiftCount);
      DCHECK_EQ(0, (shiftCount + 8) % 16);

      if (valueSizeInBits > 32) {
        shiftLower = gasm_->Word64Shl(value, Int64Constant(shiftCount));
        shiftHigher = gasm_->Word64Shr(value, Int64Constant(shiftCount));
        lowerByte = gasm_->Word64And(
            shiftLower, Int64Constant(static_cast<uint64_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word64And(
            shiftHigher, Int64Constant(static_cast<uint64_t>(0xFF) << i));
        result = gasm_->Word64Or(result, lowerByte);
        result = gasm_->Word64Or(result, higherByte);
      } else {
        shiftLower = gasm_->Word32Shl(value, Int32Constant(shiftCount));
        shiftHigher = gasm_->Word32Shr(value, Int32Constant(shiftCount));
        lowerByte = gasm_->Word32And(
            shiftLower, Int32Constant(static_cast<uint32_t>(0xFF)
                                      << (valueSizeInBits - 8 - i)));
        higherByte = gasm_->Word32And(
            shiftHigher, Int32Constant(static_cast<uint32_t>(0xFF) << i));
        result = gasm_->Word32Or(result, lowerByte);
        result = gasm_->Word32Or(result, higherByte);
      }
    }
  }

  if (isFloat) {
    switch (memtype.representation()) {
      case MachineRepresentation::kFloat64:
        result = gasm_->BitcastInt64ToFloat64(result);
        break;
      case MachineRepresentation::kFloat32:
        result = gasm_->BitcastInt32ToFloat32(result);
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
        shiftBitCount = Int32Constant(64 - valueSizeInBits);
        result = gasm_->Word64Sar(
            gasm_->Word64Shl(gasm_->ChangeInt32ToInt64(result), shiftBitCount),
            shiftBitCount);
      } else if (wasmtype == wasm::kWasmI32) {
        shiftBitCount = Int32Constant(32 - valueSizeInBits);
        result = gasm_->Word32Sar(gasm_->Word32Shl(result, shiftBitCount),
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
                  Int32Constant(0x7FFFFFFF)),
            Binop(wasm::kExprI32And, Unop(wasm::kExprI32ReinterpretF32, right),
                  Int32Constant(0x80000000))));

  return result;
}

Node* WasmGraphBuilder::BuildF64CopySign(Node* left, Node* right) {
  if (mcgraph()->machine()->Is64()) {
    return gasm_->BitcastInt64ToFloat64(
        gasm_->Word64Or(gasm_->Word64And(gasm_->BitcastFloat64ToInt64(left),
                                         Int64Constant(0x7FFFFFFFFFFFFFFF)),
                        gasm_->Word64And(gasm_->BitcastFloat64ToInt64(right),
                                         Int64Constant(0x8000000000000000))));
  }

  DCHECK(mcgraph()->machine()->Is32());

  Node* high_word_left = gasm_->Float64ExtractHighWord32(left);
  Node* high_word_right = gasm_->Float64ExtractHighWord32(right);

  Node* new_high_word = gasm_->Word32Or(
      gasm_->Word32And(high_word_left, Int32Constant(0x7FFFFFFF)),
      gasm_->Word32And(high_word_right, Int32Constant(0x80000000)));

  return gasm_->Float64InsertHighWord32(left, new_high_word);
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
      return builder->mcgraph()->machine()->TruncateFloat32ToInt32(
          TruncateKind::kSetOverflowToMin);
    case wasm::kExprI32SConvertSatF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToInt32(
          TruncateKind::kArchitectureDefault);
    case wasm::kExprI32UConvertF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToUint32(
          TruncateKind::kSetOverflowToMin);
    case wasm::kExprI32UConvertSatF32:
      return builder->mcgraph()->machine()->TruncateFloat32ToUint32(
          TruncateKind::kArchitectureDefault);
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
  if (mcgraph()->machine()->SatConversionIsSafe()) {
    return converted_value;
  }
  Node* test = ConvertSaturateTest(this, opcode, int_ty, float_ty, trunc,
                                   converted_value);
  Diamond tl_d(graph(), mcgraph()->common(), test, BranchHint::kFalse);
  tl_d.Chain(control());
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
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(gasm_->ChangeFloat32ToFloat64(input));
}

Node* WasmGraphBuilder::BuildI32AsmjsSConvertF64(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(input);
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF32(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(gasm_->ChangeFloat32ToFloat64(input));
}

Node* WasmGraphBuilder::BuildI32AsmjsUConvertF64(Node* input) {
  // asm.js must use the wacky JS semantics.
  return gasm_->TruncateFloat64ToWord32(input);
}

Node* WasmGraphBuilder::BuildBitCountingCall(Node* input, ExternalReference ref,
                                             MachineRepresentation input_type) {
  Node* stack_slot_param = StoreArgsInStackSlot({{input_type, input}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);

  Node* function = gasm_->ExternalConstant(ref);

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
  Node* stack_slot;
  if (input1) {
    stack_slot = StoreArgsInStackSlot(
        {{type.representation(), input0}, {type.representation(), input1}});
  } else {
    stack_slot = StoreArgsInStackSlot({{type.representation(), input0}});
  }

  MachineType sig_types[] = {MachineType::Pointer()};
  MachineSignature sig(0, 1, sig_types);
  Node* function = gasm_->ExternalConstant(ref);
  BuildCCall(&sig, function, stack_slot);

  return gasm_->LoadFromObject(type, stack_slot, 0);
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
  auto store_rep =
      StoreRepresentation(parameter_representation, kNoWriteBarrier);
  gasm_->Store(store_rep, stack_slot, 0, input);
  MachineType sig_types[] = {MachineType::Pointer()};
  MachineSignature sig(0, 1, sig_types);
  Node* function = gasm_->ExternalConstant(ref);
  BuildCCall(&sig, function, stack_slot);
  return gasm_->LoadFromObject(result_type, stack_slot, 0);
}

namespace {

ExternalReference convert_ccall_ref(wasm::WasmOpcode opcode) {
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
  ExternalReference call_ref = convert_ccall_ref(opcode);
  int stack_slot_size = std::max(ElementSizeInBytes(int_ty.representation()),
                                 ElementSizeInBytes(float_ty.representation()));
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(stack_slot_size));
  auto store_rep =
      StoreRepresentation(float_ty.representation(), kNoWriteBarrier);
  gasm_->Store(store_rep, stack_slot, 0, input);
  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);
  Node* function = gasm_->ExternalConstant(call_ref);
  Node* overflow = BuildCCall(&sig, function, stack_slot);
  if (IsTrappingConvertOp(opcode)) {
    ZeroCheck32(wasm::kTrapFloatUnrepresentable, overflow, position);
    return gasm_->LoadFromObject(int_ty, stack_slot, 0);
  }
  Node* test = Binop(wasm::kExprI32Eq, overflow, Int32Constant(0), position);
  Diamond tl_d(graph(), mcgraph()->common(), test, BranchHint::kFalse);
  tl_d.Chain(control());
  Node* nan_test = Binop(NeOp(float_ty), input, input);
  Diamond nan_d(graph(), mcgraph()->common(), nan_test, BranchHint::kFalse);
  nan_d.Nest(tl_d, true);
  Node* neg_test = Binop(LtOp(float_ty), input, Zero(this, float_ty));
  Diamond sat_d(graph(), mcgraph()->common(), neg_test, BranchHint::kNone);
  sat_d.Nest(nan_d, false);
  Node* sat_val =
      sat_d.Phi(int_ty.representation(), Min(this, int_ty), Max(this, int_ty));
  Node* load = gasm_->LoadFromObject(int_ty, stack_slot, 0);
  Node* nan_val =
      nan_d.Phi(int_ty.representation(), Zero(this, int_ty), sat_val);
  return tl_d.Phi(int_ty.representation(), nan_val, load);
}

Node* WasmGraphBuilder::MemoryGrow(Node* input) {
  needs_stack_check_ = true;
  if (!env_->module->is_memory64) {
    // For 32-bit memories, just call the builtin.
    return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmMemoryGrow, input);
  }

  // If the input is not a positive int32, growing will always fail
  // (growing negative or requesting >= 256 TB).
  Node* old_effect = effect();
  Diamond is_32_bit(graph(), mcgraph()->common(),
                    gasm_->Uint64LessThanOrEqual(input, Int64Constant(kMaxInt)),
                    BranchHint::kTrue);
  is_32_bit.Chain(control());

  SetControl(is_32_bit.if_true);

  Node* grow_result = gasm_->ChangeInt32ToInt64(gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmMemoryGrow, gasm_->TruncateInt64ToInt32(input)));

  Node* diamond_result = is_32_bit.Phi(MachineRepresentation::kWord64,
                                       grow_result, gasm_->Int64Constant(-1));
  SetEffectControl(is_32_bit.EffectPhi(effect(), old_effect), is_32_bit.merge);
  return diamond_result;
}

Node* WasmGraphBuilder::Throw(uint32_t exception_index,
                              const wasm::WasmException* exception,
                              const Vector<Node*> values,
                              wasm::WasmCodePosition position) {
  needs_stack_check_ = true;
  uint32_t encoded_size = WasmExceptionPackage::GetEncodedSize(exception);

  Node* values_array =
      gasm_->CallRuntimeStub(wasm::WasmCode::kWasmAllocateFixedArray,
                             gasm_->IntPtrConstant(encoded_size));
  SetSourcePosition(values_array, position);

  uint32_t index = 0;
  const wasm::WasmExceptionSig* sig = exception->sig;
  MachineOperatorBuilder* m = mcgraph()->machine();
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value = values[i];
    switch (sig->GetParam(i).kind()) {
      case wasm::kF32:
        value = gasm_->BitcastFloat32ToInt32(value);
        V8_FALLTHROUGH;
      case wasm::kI32:
        BuildEncodeException32BitValue(values_array, &index, value);
        break;
      case wasm::kF64:
        value = gasm_->BitcastFloat64ToInt64(value);
        V8_FALLTHROUGH;
      case wasm::kI64: {
        Node* upper32 = gasm_->TruncateInt64ToInt32(
            Binop(wasm::kExprI64ShrU, value, Int64Constant(32)));
        BuildEncodeException32BitValue(values_array, &index, upper32);
        Node* lower32 = gasm_->TruncateInt64ToInt32(value);
        BuildEncodeException32BitValue(values_array, &index, lower32);
        break;
      }
      case wasm::kS128:
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(0), value));
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(1), value));
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(2), value));
        BuildEncodeException32BitValue(
            values_array, &index,
            graph()->NewNode(m->I32x4ExtractLane(3), value));
        break;
      case wasm::kRef:
      case wasm::kOptRef:
      case wasm::kRtt:
      case wasm::kRttWithDepth:
        gasm_->StoreFixedArrayElementAny(values_array, index, value);
        ++index;
        break;
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
  }
  DCHECK_EQ(encoded_size, index);

  Node* exception_tag = LoadExceptionTagFromTable(exception_index);

  Node* throw_call = gasm_->CallRuntimeStub(wasm::WasmCode::kWasmThrow,
                                            exception_tag, values_array);
  SetSourcePosition(throw_call, position);
  return throw_call;
}

void WasmGraphBuilder::BuildEncodeException32BitValue(Node* values_array,
                                                      uint32_t* index,
                                                      Node* value) {
  Node* upper_halfword_as_smi =
      BuildChangeUint31ToSmi(gasm_->Word32Shr(value, Int32Constant(16)));
  gasm_->StoreFixedArrayElementSmi(values_array, *index, upper_halfword_as_smi);
  ++(*index);
  Node* lower_halfword_as_smi =
      BuildChangeUint31ToSmi(gasm_->Word32And(value, Int32Constant(0xFFFFu)));
  gasm_->StoreFixedArrayElementSmi(values_array, *index, lower_halfword_as_smi);
  ++(*index);
}

Node* WasmGraphBuilder::BuildDecodeException32BitValue(Node* values_array,
                                                       uint32_t* index) {
  Node* upper = BuildChangeSmiToInt32(
      gasm_->LoadFixedArrayElementSmi(values_array, *index));
  (*index)++;
  upper = gasm_->Word32Shl(upper, Int32Constant(16));
  Node* lower = BuildChangeSmiToInt32(
      gasm_->LoadFixedArrayElementSmi(values_array, *index));
  (*index)++;
  Node* value = gasm_->Word32Or(upper, lower);
  return value;
}

Node* WasmGraphBuilder::BuildDecodeException64BitValue(Node* values_array,
                                                       uint32_t* index) {
  Node* upper = Binop(wasm::kExprI64Shl,
                      Unop(wasm::kExprI64UConvertI32,
                           BuildDecodeException32BitValue(values_array, index)),
                      Int64Constant(32));
  Node* lower = Unop(wasm::kExprI64UConvertI32,
                     BuildDecodeException32BitValue(values_array, index));
  return Binop(wasm::kExprI64Ior, upper, lower);
}

Node* WasmGraphBuilder::Rethrow(Node* except_obj) {
  // TODO(v8:8091): Currently the message of the original exception is not being
  // preserved when rethrown to the console. The pending message will need to be
  // saved when caught and restored here while being rethrown.
  return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmRethrow, except_obj);
}

Node* WasmGraphBuilder::ExceptionTagEqual(Node* caught_tag,
                                          Node* expected_tag) {
  return gasm_->WordEqual(caught_tag, expected_tag);
}

Node* WasmGraphBuilder::LoadExceptionTagFromTable(uint32_t exception_index) {
  Node* exceptions_table =
      LOAD_INSTANCE_FIELD(ExceptionsTable, MachineType::TaggedPointer());
  Node* tag =
      gasm_->LoadFixedArrayElementPtr(exceptions_table, exception_index);
  return tag;
}

Node* WasmGraphBuilder::GetExceptionTag(Node* except_obj) {
  return gasm_->CallBuiltin(
      Builtins::kWasmGetOwnProperty, Operator::kEliminatable, except_obj,
      LOAD_ROOT(wasm_exception_tag_symbol, wasm_exception_tag_symbol),
      LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
}

Node* WasmGraphBuilder::GetExceptionValues(Node* except_obj,
                                           const wasm::WasmException* exception,
                                           Vector<Node*> values) {
  Node* values_array = gasm_->CallBuiltin(
      Builtins::kWasmGetOwnProperty, Operator::kEliminatable, except_obj,
      LOAD_ROOT(wasm_exception_values_symbol, wasm_exception_values_symbol),
      LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
  uint32_t index = 0;
  const wasm::WasmExceptionSig* sig = exception->sig;
  DCHECK_EQ(sig->parameter_count(), values.size());
  for (size_t i = 0; i < sig->parameter_count(); ++i) {
    Node* value;
    switch (sig->GetParam(i).kind()) {
      case wasm::kI32:
        value = BuildDecodeException32BitValue(values_array, &index);
        break;
      case wasm::kI64:
        value = BuildDecodeException64BitValue(values_array, &index);
        break;
      case wasm::kF32: {
        value = Unop(wasm::kExprF32ReinterpretI32,
                     BuildDecodeException32BitValue(values_array, &index));
        break;
      }
      case wasm::kF64: {
        value = Unop(wasm::kExprF64ReinterpretI64,
                     BuildDecodeException64BitValue(values_array, &index));
        break;
      }
      case wasm::kS128:
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4Splat(),
            BuildDecodeException32BitValue(values_array, &index));
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4ReplaceLane(1), value,
            BuildDecodeException32BitValue(values_array, &index));
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4ReplaceLane(2), value,
            BuildDecodeException32BitValue(values_array, &index));
        value = graph()->NewNode(
            mcgraph()->machine()->I32x4ReplaceLane(3), value,
            BuildDecodeException32BitValue(values_array, &index));
        break;
      case wasm::kRef:
      case wasm::kOptRef:
      case wasm::kRtt:
      case wasm::kRttWithDepth:
        value = gasm_->LoadFixedArrayElementAny(values_array, index);
        ++index;
        break;
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kVoid:
      case wasm::kBottom:
        UNREACHABLE();
    }
    values[i] = value;
  }
  DCHECK_EQ(index, WasmExceptionPackage::GetEncodedSize(exception));
  return values_array;
}

Node* WasmGraphBuilder::BuildI32DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  ZeroCheck32(wasm::kTrapDivByZero, right, position);
  Node* before = control();
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(gasm_->Word32Equal(right, Int32Constant(-1)), &denom_is_m1,
                    &denom_is_not_m1);
  SetControl(denom_is_m1);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, left, kMinInt, position);
  if (control() != denom_is_m1) {
    SetControl(Merge(denom_is_not_m1, control()));
  } else {
    SetControl(before);
  }
  return gasm_->Int32Div(left, right);
}

Node* WasmGraphBuilder::BuildI32RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  MachineOperatorBuilder* m = mcgraph()->machine();

  ZeroCheck32(wasm::kTrapRemByZero, right, position);

  Diamond d(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(-1)), BranchHint::kFalse);
  d.Chain(control());

  return d.Phi(MachineRepresentation::kWord32, Int32Constant(0),
               graph()->NewNode(m->Int32Mod(), left, right, d.if_false));
}

Node* WasmGraphBuilder::BuildI32DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  ZeroCheck32(wasm::kTrapDivByZero, right, position);
  return gasm_->Uint32Div(left, right);
}

Node* WasmGraphBuilder::BuildI32RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  ZeroCheck32(wasm::kTrapRemByZero, right, position);
  return gasm_->Uint32Mod(left, right);
}

Node* WasmGraphBuilder::BuildI32AsmjsDivS(Node* left, Node* right) {
  MachineOperatorBuilder* m = mcgraph()->machine();

  Int32Matcher mr(right);
  if (mr.HasResolvedValue()) {
    if (mr.ResolvedValue() == 0) {
      return Int32Constant(0);
    } else if (mr.ResolvedValue() == -1) {
      // The result is the negation of the left input.
      return gasm_->Int32Sub(Int32Constant(0), left);
    }
    return gasm_->Int32Div(left, right);
  }

  // asm.js semantics return 0 on divide or mod by zero.
  if (m->Int32DivIsSafe()) {
    // The hardware instruction does the right thing (e.g. arm).
    return gasm_->Int32Div(left, right);
  }

  // Check denominator for zero.
  Diamond z(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(0)), BranchHint::kFalse);
  z.Chain(control());

  // Check denominator for -1. (avoid minint / -1 case).
  Diamond n(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(-1)), BranchHint::kFalse);
  n.Chain(z.if_false);

  Node* div = graph()->NewNode(m->Int32Div(), left, right, n.if_false);

  Node* neg = gasm_->Int32Sub(Int32Constant(0), left);

  return z.Phi(MachineRepresentation::kWord32, Int32Constant(0),
               n.Phi(MachineRepresentation::kWord32, neg, div));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemS(Node* left, Node* right) {
  CommonOperatorBuilder* c = mcgraph()->common();
  MachineOperatorBuilder* m = mcgraph()->machine();
  Node* const zero = Int32Constant(0);

  Int32Matcher mr(right);
  if (mr.HasResolvedValue()) {
    if (mr.ResolvedValue() == 0 || mr.ResolvedValue() == -1) {
      return zero;
    }
    return gasm_->Int32Mod(left, right);
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
  Node* const minus_one = Int32Constant(-1);

  const Operator* const merge_op = c->Merge(2);
  const Operator* const phi_op = c->Phi(MachineRepresentation::kWord32, 2);

  Node* check0 = gasm_->Int32LessThan(zero, right);
  Node* branch0 =
      graph()->NewNode(c->Branch(BranchHint::kTrue), check0, control());

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
    return gasm_->Uint32Div(left, right);
  }

  // Explicit check for x % 0.
  Diamond z(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(0)), BranchHint::kFalse);
  z.Chain(control());

  return z.Phi(MachineRepresentation::kWord32, Int32Constant(0),
               graph()->NewNode(mcgraph()->machine()->Uint32Div(), left, right,
                                z.if_false));
}

Node* WasmGraphBuilder::BuildI32AsmjsRemU(Node* left, Node* right) {
  // asm.js semantics return 0 on divide or mod by zero.
  // Explicit check for x % 0.
  Diamond z(graph(), mcgraph()->common(),
            gasm_->Word32Equal(right, Int32Constant(0)), BranchHint::kFalse);
  z.Chain(control());

  Node* rem = graph()->NewNode(mcgraph()->machine()->Uint32Mod(), left, right,
                               z.if_false);
  return z.Phi(MachineRepresentation::kWord32, Int32Constant(0), rem);
}

Node* WasmGraphBuilder::BuildI64DivS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_int64_div(),
                          MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  ZeroCheck64(wasm::kTrapDivByZero, right, position);
  Node* before = control();
  Node* denom_is_m1;
  Node* denom_is_not_m1;
  BranchExpectFalse(gasm_->Word64Equal(right, Int64Constant(-1)), &denom_is_m1,
                    &denom_is_not_m1);
  SetControl(denom_is_m1);
  TrapIfEq64(wasm::kTrapDivUnrepresentable, left,
             std::numeric_limits<int64_t>::min(), position);
  if (control() != denom_is_m1) {
    SetControl(Merge(denom_is_not_m1, control()));
  } else {
    SetControl(before);
  }
  return gasm_->Int64Div(left, right);
}

Node* WasmGraphBuilder::BuildI64RemS(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_int64_mod(),
                          MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  ZeroCheck64(wasm::kTrapRemByZero, right, position);
  Diamond d(mcgraph()->graph(), mcgraph()->common(),
            gasm_->Word64Equal(right, Int64Constant(-1)));

  d.Chain(control());

  Node* rem = graph()->NewNode(mcgraph()->machine()->Int64Mod(), left, right,
                               d.if_false);

  return d.Phi(MachineRepresentation::kWord64, Int64Constant(0), rem);
}

Node* WasmGraphBuilder::BuildI64DivU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_uint64_div(),
                          MachineType::Int64(), wasm::kTrapDivByZero, position);
  }
  ZeroCheck64(wasm::kTrapDivByZero, right, position);
  return gasm_->Uint64Div(left, right);
}
Node* WasmGraphBuilder::BuildI64RemU(Node* left, Node* right,
                                     wasm::WasmCodePosition position) {
  if (mcgraph()->machine()->Is32()) {
    return BuildDiv64Call(left, right, ExternalReference::wasm_uint64_mod(),
                          MachineType::Int64(), wasm::kTrapRemByZero, position);
  }
  ZeroCheck64(wasm::kTrapRemByZero, right, position);
  return gasm_->Uint64Mod(left, right);
}

Node* WasmGraphBuilder::BuildDiv64Call(Node* left, Node* right,
                                       ExternalReference ref,
                                       MachineType result_type,
                                       wasm::TrapReason trap_zero,
                                       wasm::WasmCodePosition position) {
  Node* stack_slot =
      StoreArgsInStackSlot({{MachineRepresentation::kWord64, left},
                            {MachineRepresentation::kWord64, right}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);

  Node* function = gasm_->ExternalConstant(ref);
  Node* call = BuildCCall(&sig, function, stack_slot);

  ZeroCheck32(trap_zero, call, position);
  TrapIfEq32(wasm::kTrapDivUnrepresentable, call, -1, position);
  return gasm_->LoadFromObject(result_type, stack_slot, 0);
}

template <typename... Args>
Node* WasmGraphBuilder::BuildCCall(MachineSignature* sig, Node* function,
                                   Args... args) {
  DCHECK_LE(sig->return_count(), 1);
  DCHECK_EQ(sizeof...(args), sig->parameter_count());
  Node* call_args[] = {function, args..., effect(), control()};

  auto call_descriptor =
      Linkage::GetSimplifiedCDescriptor(mcgraph()->zone(), sig);

  return gasm_->Call(call_descriptor, arraysize(call_args), call_args);
}

Node* WasmGraphBuilder::BuildCallNode(const wasm::FunctionSig* sig,
                                      Vector<Node*> args,
                                      wasm::WasmCodePosition position,
                                      Node* instance_node, const Operator* op,
                                      Node* frame_state) {
  if (instance_node == nullptr) {
    instance_node = GetInstance();
  }
  needs_stack_check_ = true;
  const size_t params = sig->parameter_count();
  const size_t has_frame_state = frame_state != nullptr ? 1 : 0;
  const size_t extra = 3;  // instance_node, effect, and control.
  const size_t count = 1 + params + extra + has_frame_state;

  // Reallocate the buffer to make space for extra inputs.
  base::SmallVector<Node*, 16 + extra> inputs(count);
  DCHECK_EQ(1 + params, args.size());

  // Make room for the instance_node parameter at index 1, just after code.
  inputs[0] = args[0];  // code
  inputs[1] = instance_node;
  if (params > 0) base::Memcpy(&inputs[2], &args[1], params * sizeof(Node*));

  // Add effect and control inputs.
  if (has_frame_state != 0) inputs[params + 2] = frame_state;
  inputs[params + has_frame_state + 2] = effect();
  inputs[params + has_frame_state + 3] = control();

  Node* call = graph()->NewNode(op, static_cast<int>(count), inputs.begin());
  // Return calls have no effect output. Other calls are the new effect node.
  if (op->EffectOutputCount() > 0) SetEffect(call);
  DCHECK(position == wasm::kNoCodePosition || position > 0);
  if (position > 0) SetSourcePosition(call, position);

  return call;
}

Node* WasmGraphBuilder::BuildWasmCall(const wasm::FunctionSig* sig,
                                      Vector<Node*> args, Vector<Node*> rets,
                                      wasm::WasmCodePosition position,
                                      Node* instance_node,
                                      UseRetpoline use_retpoline,
                                      Node* frame_state) {
  CallDescriptor* call_descriptor =
      GetWasmCallDescriptor(mcgraph()->zone(), sig, use_retpoline,
                            kWasmFunction, frame_state != nullptr);
  const Operator* op = mcgraph()->common()->Call(call_descriptor);
  Node* call =
      BuildCallNode(sig, args, position, instance_node, op, frame_state);
  // TODO(manoskouk): Don't always set control if we ever add properties to wasm
  // calls.
  SetControl(call);

  size_t ret_count = sig->return_count();
  if (ret_count == 0) return call;  // No return value.

  DCHECK_EQ(ret_count, rets.size());
  if (ret_count == 1) {
    // Only a single return value.
    rets[0] = call;
  } else {
    // Create projections for all return values.
    for (size_t i = 0; i < ret_count; i++) {
      rets[i] = graph()->NewNode(mcgraph()->common()->Projection(i), call,
                                 graph()->start());
    }
  }
  return call;
}

Node* WasmGraphBuilder::BuildWasmReturnCall(const wasm::FunctionSig* sig,
                                            Vector<Node*> args,
                                            wasm::WasmCodePosition position,
                                            Node* instance_node,
                                            UseRetpoline use_retpoline) {
  CallDescriptor* call_descriptor =
      GetWasmCallDescriptor(mcgraph()->zone(), sig, use_retpoline);
  const Operator* op = mcgraph()->common()->TailCall(call_descriptor);
  Node* call = BuildCallNode(sig, args, position, instance_node, op);

  // TODO(manoskouk): {call} will not always be a control node if we ever add
  // properties to wasm calls.
  gasm_->MergeControlToEnd(call);

  return call;
}

Node* WasmGraphBuilder::BuildImportCall(const wasm::FunctionSig* sig,
                                        Vector<Node*> args, Vector<Node*> rets,
                                        wasm::WasmCodePosition position,
                                        int func_index,
                                        IsReturnCall continuation) {
  return BuildImportCall(sig, args, rets, position,
                         gasm_->Uint32Constant(func_index), continuation);
}

Node* WasmGraphBuilder::BuildImportCall(const wasm::FunctionSig* sig,
                                        Vector<Node*> args, Vector<Node*> rets,
                                        wasm::WasmCodePosition position,
                                        Node* func_index,
                                        IsReturnCall continuation) {
  // Load the imported function refs array from the instance.
  Node* imported_function_refs =
      LOAD_INSTANCE_FIELD(ImportedFunctionRefs, MachineType::TaggedPointer());
  // Access fixed array at {header_size - tag + func_index * kTaggedSize}.
  Node* func_index_intptr = Uint32ToUintptr(func_index);
  Node* ref_node = gasm_->LoadFixedArrayElement(
      imported_function_refs, func_index_intptr, MachineType::TaggedPointer());

  // Load the target from the imported_targets array at the offset of
  // {func_index}.
  Node* func_index_times_pointersize = gasm_->IntMul(
      func_index_intptr, gasm_->IntPtrConstant(kSystemPointerSize));
  Node* imported_targets =
      LOAD_INSTANCE_FIELD(ImportedFunctionTargets, MachineType::Pointer());
  Node* target_node = gasm_->LoadFromObject(
      MachineType::Pointer(), imported_targets, func_index_times_pointersize);
  args[0] = target_node;
  const UseRetpoline use_retpoline =
      untrusted_code_mitigations_ ? kRetpoline : kNoRetpoline;

  switch (continuation) {
    case kCallContinues:
      return BuildWasmCall(sig, args, rets, position, ref_node, use_retpoline);
    case kReturnCall:
      DCHECK(rets.empty());
      return BuildWasmReturnCall(sig, args, position, ref_node, use_retpoline);
  }
}

Node* WasmGraphBuilder::CallDirect(uint32_t index, Vector<Node*> args,
                                   Vector<Node*> rets,
                                   wasm::WasmCodePosition position) {
  DCHECK_NULL(args[0]);
  const wasm::FunctionSig* sig = env_->module->functions[index].sig;

  if (env_ && index < env_->module->num_imported_functions) {
    // Call to an imported function.
    return BuildImportCall(sig, args, rets, position, index, kCallContinues);
  }

  // A direct call to a wasm function defined in this module.
  // Just encode the function index. This will be patched at instantiation.
  Address code = static_cast<Address>(index);
  args[0] = mcgraph()->RelocatableIntPtrConstant(code, RelocInfo::WASM_CALL);

  return BuildWasmCall(sig, args, rets, position, nullptr, kNoRetpoline);
}

Node* WasmGraphBuilder::CallIndirect(uint32_t table_index, uint32_t sig_index,
                                     Vector<Node*> args, Vector<Node*> rets,
                                     wasm::WasmCodePosition position) {
  return BuildIndirectCall(table_index, sig_index, args, rets, position,
                           kCallContinues);
}

void WasmGraphBuilder::LoadIndirectFunctionTable(uint32_t table_index,
                                                 Node** ift_size,
                                                 Node** ift_sig_ids,
                                                 Node** ift_targets,
                                                 Node** ift_instances) {
  if (table_index == 0) {
    *ift_size = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableSize,
                                            MachineType::Uint32());
    *ift_sig_ids = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableSigIds,
                                               MachineType::Pointer());
    *ift_targets = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableTargets,
                                               MachineType::Pointer());
    *ift_instances = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTableRefs,
                                                 MachineType::TaggedPointer());
    return;
  }

  Node* ift_tables = LOAD_MUTABLE_INSTANCE_FIELD(IndirectFunctionTables,
                                                 MachineType::TaggedPointer());
  Node* ift_table = gasm_->LoadFixedArrayElementAny(ift_tables, table_index);

  *ift_size = gasm_->LoadFromObject(
      MachineType::Int32(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kSizeOffset));

  *ift_sig_ids = gasm_->LoadFromObject(
      MachineType::Pointer(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kSigIdsOffset));

  *ift_targets = gasm_->LoadFromObject(
      MachineType::Pointer(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kTargetsOffset));

  *ift_instances = gasm_->LoadFromObject(
      MachineType::TaggedPointer(), ift_table,
      wasm::ObjectAccess::ToTagged(WasmIndirectFunctionTable::kRefsOffset));
}

Node* WasmGraphBuilder::BuildIndirectCall(uint32_t table_index,
                                          uint32_t sig_index,
                                          Vector<Node*> args,
                                          Vector<Node*> rets,
                                          wasm::WasmCodePosition position,
                                          IsReturnCall continuation) {
  DCHECK_NOT_NULL(args[0]);
  DCHECK_NOT_NULL(env_);

  // First we have to load the table.
  Node* ift_size;
  Node* ift_sig_ids;
  Node* ift_targets;
  Node* ift_instances;
  LoadIndirectFunctionTable(table_index, &ift_size, &ift_sig_ids, &ift_targets,
                            &ift_instances);

  const wasm::FunctionSig* sig = env_->module->signature(sig_index);

  Node* key = args[0];

  // Bounds check against the table size.
  Node* in_bounds = gasm_->Uint32LessThan(key, ift_size);
  TrapIfFalse(wasm::kTrapTableOutOfBounds, in_bounds, position);

  // Mask the key to prevent SSCA.
  if (untrusted_code_mitigations_) {
    // mask = ((key - size) & ~key) >> 31
    Node* neg_key = gasm_->Word32Xor(key, Int32Constant(-1));
    Node* masked_diff =
        gasm_->Word32And(gasm_->Int32Sub(key, ift_size), neg_key);
    Node* mask = gasm_->Word32Sar(masked_diff, Int32Constant(31));
    key = gasm_->Word32And(key, mask);
  }

  const wasm::ValueType table_type = env_->module->tables[table_index].type;
  // Check that the table entry is not null and that the type of the function is
  // a subtype of the function type declared at the call site. In the absence of
  // function subtyping, the latter can only happen if the table type is (ref
  // null? func). Also, subtyping reduces to normalized signature equality
  // checking.
  // TODO(7748): Expand this with function subtyping once we have that.
  const bool needs_signature_check =
      table_type.is_reference_to(wasm::HeapType::kFunc) ||
      table_type.is_nullable();
  if (needs_signature_check) {
    Node* int32_scaled_key =
        Uint32ToUintptr(gasm_->Word32Shl(key, Int32Constant(2)));

    Node* loaded_sig = gasm_->LoadFromObject(MachineType::Int32(), ift_sig_ids,
                                             int32_scaled_key);

    if (table_type.is_reference_to(wasm::HeapType::kFunc)) {
      int32_t expected_sig_id = env_->module->canonicalized_type_ids[sig_index];
      Node* sig_match =
          gasm_->Word32Equal(loaded_sig, Int32Constant(expected_sig_id));
      TrapIfFalse(wasm::kTrapFuncSigMismatch, sig_match, position);
    } else {
      // If the table entries are nullable, we still have to check that the
      // entry is initialized.
      Node* function_is_null =
          gasm_->Word32Equal(loaded_sig, Int32Constant(-1));
      TrapIfTrue(wasm::kTrapNullDereference, function_is_null, position);
    }
  }

  Node* key_intptr = Uint32ToUintptr(key);

  Node* target_instance = gasm_->LoadFixedArrayElement(
      ift_instances, key_intptr, MachineType::TaggedPointer());

  Node* intptr_scaled_key =
      gasm_->IntMul(key_intptr, gasm_->IntPtrConstant(kSystemPointerSize));

  Node* target = gasm_->LoadFromObject(MachineType::Pointer(), ift_targets,
                                       intptr_scaled_key);

  args[0] = target;
  const UseRetpoline use_retpoline =
      untrusted_code_mitigations_ ? kRetpoline : kNoRetpoline;

  switch (continuation) {
    case kCallContinues:
      return BuildWasmCall(sig, args, rets, position, target_instance,
                           use_retpoline);
    case kReturnCall:
      return BuildWasmReturnCall(sig, args, position, target_instance,
                                 use_retpoline);
  }
}

Node* WasmGraphBuilder::BuildLoadJumpTableOffsetFromExportedFunctionData(
    Node* function_data) {
  Node* jump_table_offset_smi = gasm_->LoadFromObject(
      MachineType::TaggedSigned(), function_data,
      wasm::ObjectAccess::ToTagged(
          WasmExportedFunctionData::kJumpTableOffsetOffset));
  return BuildChangeSmiToIntPtr(jump_table_offset_smi);
}

// TODO(9495): Support CAPI function refs.
Node* WasmGraphBuilder::BuildCallRef(uint32_t sig_index, Vector<Node*> args,
                                     Vector<Node*> rets,
                                     CheckForNull null_check,
                                     IsReturnCall continuation,
                                     wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    TrapIfTrue(wasm::kTrapNullDereference, gasm_->WordEqual(args[0], RefNull()),
               position);
  }

  const wasm::FunctionSig* sig = env_->module->signature(sig_index);

  Node* function_data = gasm_->LoadFunctionDataFromJSFunction(args[0]);

  Node* is_js_function =
      gasm_->HasInstanceType(function_data, WASM_JS_FUNCTION_DATA_TYPE);

  auto js_label = gasm_->MakeLabel();
  auto end_label = gasm_->MakeLabel(MachineRepresentation::kTaggedPointer,
                                    MachineRepresentation::kTaggedPointer);

  gasm_->GotoIf(is_js_function, &js_label);

  {
    // Call to a WasmExportedFunction.
    // Load instance object corresponding to module where callee is defined.
    Node* callee_instance = gasm_->LoadExportedFunctionInstance(function_data);
    Node* function_index = gasm_->LoadExportedFunctionIndexAsSmi(function_data);

    auto imported_label = gasm_->MakeLabel();

    // Check if callee is a locally defined or imported function in its module.
    Node* imported_function_refs = gasm_->LoadFromObject(
        MachineType::TaggedPointer(), callee_instance,
        wasm::ObjectAccess::ToTagged(
            WasmInstanceObject::kImportedFunctionRefsOffset));
    Node* imported_functions_num =
        gasm_->LoadFixedArrayLengthAsSmi(imported_function_refs);
    gasm_->GotoIf(gasm_->SmiLessThan(function_index, imported_functions_num),
                  &imported_label);
    {
      // Function locally defined in module.
      Node* jump_table_start =
          gasm_->LoadFromObject(MachineType::Pointer(), callee_instance,
                                wasm::ObjectAccess::ToTagged(
                                    WasmInstanceObject::kJumpTableStartOffset));
      Node* jump_table_offset =
          BuildLoadJumpTableOffsetFromExportedFunctionData(function_data);
      Node* jump_table_slot =
          gasm_->IntAdd(jump_table_start, jump_table_offset);

      gasm_->Goto(&end_label, jump_table_slot,
                  callee_instance /* Unused, dummy value */);
    }

    {
      // Function imported to module.
      gasm_->Bind(&imported_label);
      Node* function_index_intptr = BuildChangeSmiToIntPtr(function_index);

      Node* imported_instance = gasm_->LoadFixedArrayElement(
          imported_function_refs, function_index_intptr,
          MachineType::TaggedPointer());

      Node* imported_function_targets = gasm_->LoadFromObject(
          MachineType::Pointer(), callee_instance,
          wasm::ObjectAccess::ToTagged(
              WasmInstanceObject::kImportedFunctionTargetsOffset));

      Node* target_node = gasm_->LoadFromObject(
          MachineType::Pointer(), imported_function_targets,
          gasm_->IntMul(function_index_intptr,
                        gasm_->IntPtrConstant(kSystemPointerSize)));

      gasm_->Goto(&end_label, target_node, imported_instance);
    }
  }

  {
    // Call to a WasmJSFunction. The call target is
    // function_data->wasm_to_js_wrapper_code()->instruction_start().
    // The instance_node is the pair
    // (current WasmInstanceObject, function_data->callable()).
    gasm_->Bind(&js_label);

    Node* wrapper_code = gasm_->LoadFromObject(
        MachineType::TaggedPointer(), function_data,
        wasm::ObjectAccess::ToTagged(
            WasmJSFunctionData::kWasmToJsWrapperCodeOffset));
    Node* call_target = gasm_->IntAdd(
        wrapper_code,
        gasm_->IntPtrConstant(wasm::ObjectAccess::ToTagged(Code::kHeaderSize)));

    Node* callable = gasm_->LoadFromObject(
        MachineType::TaggedPointer(), function_data,
        wasm::ObjectAccess::ToTagged(WasmJSFunctionData::kCallableOffset));
    // TODO(manoskouk): Find an elegant way to avoid allocating this pair for
    // every call.
    Node* function_instance_node =
        gasm_->CallBuiltin(Builtins::kWasmAllocatePair, Operator::kEliminatable,
                           GetInstance(), callable);

    gasm_->Goto(&end_label, call_target, function_instance_node);
  }

  gasm_->Bind(&end_label);

  args[0] = end_label.PhiAt(0);
  Node* instance_node = end_label.PhiAt(1);

  const UseRetpoline use_retpoline =
      untrusted_code_mitigations_ ? kRetpoline : kNoRetpoline;

  Node* call = continuation == kCallContinues
                   ? BuildWasmCall(sig, args, rets, position, instance_node,
                                   use_retpoline)
                   : BuildWasmReturnCall(sig, args, position, instance_node,
                                         use_retpoline);
  return call;
}

Node* WasmGraphBuilder::CallRef(uint32_t sig_index, Vector<Node*> args,
                                Vector<Node*> rets,
                                WasmGraphBuilder::CheckForNull null_check,
                                wasm::WasmCodePosition position) {
  return BuildCallRef(sig_index, args, rets, null_check,
                      IsReturnCall::kCallContinues, position);
}

Node* WasmGraphBuilder::ReturnCallRef(uint32_t sig_index, Vector<Node*> args,
                                      WasmGraphBuilder::CheckForNull null_check,
                                      wasm::WasmCodePosition position) {
  return BuildCallRef(sig_index, args, {}, null_check,
                      IsReturnCall::kReturnCall, position);
}

Node* WasmGraphBuilder::ReturnCall(uint32_t index, Vector<Node*> args,
                                   wasm::WasmCodePosition position) {
  DCHECK_NULL(args[0]);
  const wasm::FunctionSig* sig = env_->module->functions[index].sig;

  if (env_ && index < env_->module->num_imported_functions) {
    // Return Call to an imported function.
    return BuildImportCall(sig, args, {}, position, index, kReturnCall);
  }

  // A direct tail call to a wasm function defined in this module.
  // Just encode the function index. This will be patched during code
  // generation.
  Address code = static_cast<Address>(index);
  args[0] = mcgraph()->RelocatableIntPtrConstant(code, RelocInfo::WASM_CALL);

  return BuildWasmReturnCall(sig, args, position, nullptr, kNoRetpoline);
}

Node* WasmGraphBuilder::ReturnCallIndirect(uint32_t table_index,
                                           uint32_t sig_index,
                                           Vector<Node*> args,
                                           wasm::WasmCodePosition position) {
  return BuildIndirectCall(table_index, sig_index, args, {}, position,
                           kReturnCall);
}

void WasmGraphBuilder::BrOnNull(Node* ref_object, Node** null_node,
                                Node** non_null_node) {
  BranchExpectFalse(gasm_->WordEqual(ref_object, RefNull()), null_node,
                    non_null_node);
}

Node* WasmGraphBuilder::BuildI32Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word32Rol opcode in TurboFan.
  Int32Matcher m(right);
  if (m.HasResolvedValue()) {
    return Binop(wasm::kExprI32Ror, left,
                 Int32Constant(32 - (m.ResolvedValue() & 0x1F)));
  } else {
    return Binop(wasm::kExprI32Ror, left,
                 Binop(wasm::kExprI32Sub, Int32Constant(32), right));
  }
}

Node* WasmGraphBuilder::BuildI64Rol(Node* left, Node* right) {
  // Implement Rol by Ror since TurboFan does not have Rol opcode.
  // TODO(weiliang): support Word64Rol opcode in TurboFan.
  Int64Matcher m(right);
  Node* inv_right = m.HasResolvedValue()
                        ? Int64Constant(64 - (m.ResolvedValue() & 0x3F))
                        : Binop(wasm::kExprI64Sub, Int64Constant(64), right);
  return Binop(wasm::kExprI64Ror, left, inv_right);
}

Node* WasmGraphBuilder::Invert(Node* node) {
  return Unop(wasm::kExprI32Eqz, node);
}

Node* WasmGraphBuilder::BuildTruncateIntPtrToInt32(Node* value) {
  return mcgraph()->machine()->Is64() ? gasm_->TruncateInt64ToInt32(value)
                                      : value;
}

Node* WasmGraphBuilder::BuildChangeInt32ToIntPtr(Node* value) {
  return mcgraph()->machine()->Is64() ? gasm_->ChangeInt32ToInt64(value)
                                      : value;
}

Node* WasmGraphBuilder::BuildChangeIntPtrToInt64(Node* value) {
  return mcgraph()->machine()->Is32() ? gasm_->ChangeInt32ToInt64(value)
                                      : value;
}

Node* WasmGraphBuilder::BuildChangeInt32ToSmi(Node* value) {
  // With pointer compression, only the lower 32 bits are used.
  return COMPRESS_POINTERS_BOOL
             ? gasm_->Word32Shl(value, BuildSmiShiftBitsConstant32())
             : gasm_->WordShl(BuildChangeInt32ToIntPtr(value),
                              BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildChangeUint31ToSmi(Node* value) {
  return COMPRESS_POINTERS_BOOL
             ? gasm_->Word32Shl(value, BuildSmiShiftBitsConstant32())
             : gasm_->WordShl(Uint32ToUintptr(value),
                              BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildSmiShiftBitsConstant() {
  return gasm_->IntPtrConstant(kSmiShiftSize + kSmiTagSize);
}

Node* WasmGraphBuilder::BuildSmiShiftBitsConstant32() {
  return Int32Constant(kSmiShiftSize + kSmiTagSize);
}

Node* WasmGraphBuilder::BuildChangeSmiToInt32(Node* value) {
  return COMPRESS_POINTERS_BOOL
             ? gasm_->Word32Sar(gasm_->TruncateInt64ToInt32(value),
                                BuildSmiShiftBitsConstant32())
             : BuildTruncateIntPtrToInt32(BuildChangeSmiToIntPtr(value));
}

Node* WasmGraphBuilder::BuildChangeSmiToIntPtr(Node* value) {
  if (COMPRESS_POINTERS_BOOL) {
    value = BuildChangeSmiToInt32(value);
    return BuildChangeInt32ToIntPtr(value);
  }
  return gasm_->WordSar(value, BuildSmiShiftBitsConstant());
}

Node* WasmGraphBuilder::BuildConvertUint32ToSmiWithSaturation(Node* value,
                                                              uint32_t maxval) {
  DCHECK(Smi::IsValid(maxval));
  Node* max = mcgraph()->Uint32Constant(maxval);
  Node* check = gasm_->Uint32LessThanOrEqual(value, max);
  Node* valsmi = BuildChangeUint31ToSmi(value);
  Node* maxsmi = gasm_->NumberConstant(maxval);
  Diamond d(graph(), mcgraph()->common(), check, BranchHint::kTrue);
  d.Chain(control());
  return d.Phi(MachineRepresentation::kTagged, valsmi, maxsmi);
}

void WasmGraphBuilder::InitInstanceCache(
    WasmInstanceCacheNodes* instance_cache) {

  // Load the memory start.
  instance_cache->mem_start =
      LOAD_MUTABLE_INSTANCE_FIELD(MemoryStart, MachineType::UintPtr());

  // Load the memory size.
  instance_cache->mem_size =
      LOAD_MUTABLE_INSTANCE_FIELD(MemorySize, MachineType::UintPtr());

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
    // Note that it is not safe to use {Buffer} here since this method is used
    // via {CheckForException} while the {Buffer} is in use by another method.
    uint32_t count = merge->InputCount();
    // + 1 for the merge node.
    base::SmallVector<Node*, 9> inputs(count + 1);
    for (uint32_t j = 0; j < count - 1; j++) inputs[j] = tnode;
    inputs[count - 1] = fnode;
    inputs[count] = merge;
    tnode = graph()->NewNode(mcgraph()->common()->Phi(rep, count), count + 1,
                             inputs.begin());
  }
  return tnode;
}

Node* WasmGraphBuilder::CreateOrMergeIntoEffectPhi(Node* merge, Node* tnode,
                                                   Node* fnode) {
  if (IsPhiWithMerge(tnode, merge)) {
    AppendToPhi(tnode, fnode);
  } else if (tnode != fnode) {
    // Note that it is not safe to use {Buffer} here since this method is used
    // via {CheckForException} while the {Buffer} is in use by another method.
    uint32_t count = merge->InputCount();
    // + 1 for the merge node.
    base::SmallVector<Node*, 9> inputs(count + 1);
    for (uint32_t j = 0; j < count - 1; j++) {
      inputs[j] = tnode;
    }
    inputs[count - 1] = fnode;
    inputs[count] = merge;
    tnode = graph()->NewNode(mcgraph()->common()->EffectPhi(count), count + 1,
                             inputs.begin());
  }
  return tnode;
}

Node* WasmGraphBuilder::effect() { return gasm_->effect(); }

Node* WasmGraphBuilder::control() { return gasm_->control(); }

Node* WasmGraphBuilder::SetEffect(Node* node) {
  SetEffectControl(node, control());
  return node;
}

Node* WasmGraphBuilder::SetControl(Node* node) {
  SetEffectControl(effect(), node);
  return node;
}

void WasmGraphBuilder::SetEffectControl(Node* effect, Node* control) {
  gasm_->InitializeEffectControl(effect, control);
}

Node* WasmGraphBuilder::GetImportedMutableGlobals() {
  return LOAD_INSTANCE_FIELD(ImportedMutableGlobals, MachineType::UintPtr());
}

void WasmGraphBuilder::GetGlobalBaseAndOffset(MachineType mem_type,
                                              const wasm::WasmGlobal& global,
                                              Node** base_node,
                                              Node** offset_node) {
  if (global.mutability && global.imported) {
    *base_node = gasm_->LoadFromObject(
        MachineType::UintPtr(), GetImportedMutableGlobals(),
        Int32Constant(global.index * sizeof(Address)));
    *offset_node = Int32Constant(0);
  } else {
    Node* globals_start =
        LOAD_INSTANCE_FIELD(GlobalsStart, MachineType::UintPtr());
    *base_node = globals_start;
    *offset_node = Int32Constant(global.offset);

    if (mem_type == MachineType::Simd128() && global.offset != 0) {
      // TODO(titzer,bbudge): code generation for SIMD memory offsets is broken.
      *base_node = gasm_->IntAdd(*base_node, *offset_node);
      *offset_node = Int32Constant(0);
    }
  }
}

void WasmGraphBuilder::GetBaseAndOffsetForImportedMutableExternRefGlobal(
    const wasm::WasmGlobal& global, Node** base, Node** offset) {
  // Load the base from the ImportedMutableGlobalsBuffer of the instance.
  Node* buffers = LOAD_INSTANCE_FIELD(ImportedMutableGlobalsBuffers,
                                      MachineType::TaggedPointer());
  *base = gasm_->LoadFixedArrayElementAny(buffers, global.index);

  // For the offset we need the index of the global in the buffer, and then
  // calculate the actual offset from the index. Load the index from the
  // ImportedMutableGlobals array of the instance.
  Node* index =
      gasm_->LoadFromObject(MachineType::UintPtr(), GetImportedMutableGlobals(),
                            Int32Constant(global.index * sizeof(Address)));

  // From the index, calculate the actual offset in the FixedArray. This
  // is kHeaderSize + (index * kTaggedSize). kHeaderSize can be acquired with
  // wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(0).
  Node* index_times_tagged_size =
      gasm_->IntMul(Uint32ToUintptr(index), Int32Constant(kTaggedSize));
  *offset = gasm_->IntAdd(
      index_times_tagged_size,
      mcgraph()->IntPtrConstant(
          wasm::ObjectAccess::ElementOffsetInTaggedFixedArray(0)));
}

Node* WasmGraphBuilder::MemBuffer(uintptr_t offset) {
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_start = instance_cache_->mem_start;
  DCHECK_NOT_NULL(mem_start);
  if (offset == 0) return mem_start;
  return gasm_->IntAdd(mem_start, gasm_->UintPtrConstant(offset));
}

Node* WasmGraphBuilder::CurrentMemoryPages() {
  // CurrentMemoryPages can not be called from asm.js.
  DCHECK_EQ(wasm::kWasmOrigin, env_->module->origin);
  DCHECK_NOT_NULL(instance_cache_);
  Node* mem_size = instance_cache_->mem_size;
  DCHECK_NOT_NULL(mem_size);
  Node* result =
      gasm_->WordShr(mem_size, Int32Constant(wasm::kWasmPageSizeLog2));
  result = env_->module->is_memory64 ? BuildChangeIntPtrToInt64(result)
                                     : BuildTruncateIntPtrToInt32(result);
  return result;
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
  // The CEntryStub is loaded from the IsolateRoot so that generated code is
  // Isolate independent. At the moment this is only done for CEntryStub(1).
  Node* isolate_root = BuildLoadIsolateRoot();
  DCHECK_EQ(1, fun->result_size);
  auto centry_id =
      Builtins::kCEntry_Return1_DontSaveFPRegs_ArgvOnStack_NoBuiltinExit;
  Node* centry_stub =
      gasm_->LoadFromObject(MachineType::Pointer(), isolate_root,
                            IsolateData::builtin_slot_offset(centry_id));
  // TODO(titzer): allow arbitrary number of runtime arguments
  // At the moment we only allow 5 parameters. If more parameters are needed,
  // increase this constant accordingly.
  static const int kMaxParams = 5;
  DCHECK_GE(kMaxParams, parameter_count);
  Node* inputs[kMaxParams + 6];
  int count = 0;
  inputs[count++] = centry_stub;
  for (int i = 0; i < parameter_count; i++) {
    inputs[count++] = parameters[i];
  }
  inputs[count++] =
      mcgraph()->ExternalConstant(ExternalReference::Create(f));  // ref
  inputs[count++] = Int32Constant(fun->nargs);                    // arity
  inputs[count++] = js_context;                                   // js_context
  inputs[count++] = effect();
  inputs[count++] = control();

  return gasm_->Call(call_descriptor, count, inputs);
}

Node* WasmGraphBuilder::BuildCallToRuntime(Runtime::FunctionId f,
                                           Node** parameters,
                                           int parameter_count) {
  return BuildCallToRuntimeWithContext(f, NoContextConstant(), parameters,
                                       parameter_count);
}

Node* WasmGraphBuilder::GlobalGet(uint32_t index) {
  const wasm::WasmGlobal& global = env_->module->globals[index];
  if (global.type.is_reference()) {
    if (global.mutability && global.imported) {
      Node* base = nullptr;
      Node* offset = nullptr;
      GetBaseAndOffsetForImportedMutableExternRefGlobal(global, &base, &offset);
      return gasm_->LoadFromObject(MachineType::AnyTagged(), base, offset);
    }
    Node* globals_buffer =
        LOAD_INSTANCE_FIELD(TaggedGlobalsBuffer, MachineType::TaggedPointer());
    return gasm_->LoadFixedArrayElementAny(globals_buffer, global.offset);
  }

  MachineType mem_type = global.type.machine_type();
  if (mem_type.representation() == MachineRepresentation::kSimd128) {
    has_simd_ = true;
  }
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(mem_type, global, &base, &offset);
  // TODO(manoskouk): Cannot use LoadFromObject here due to
  // GetGlobalBaseAndOffset pointer arithmetic.
  Node* result = gasm_->Load(mem_type, base, offset);
#if defined(V8_TARGET_BIG_ENDIAN)
  result = BuildChangeEndiannessLoad(result, mem_type, global.type);
#endif
  return result;
}

void WasmGraphBuilder::GlobalSet(uint32_t index, Node* val) {
  const wasm::WasmGlobal& global = env_->module->globals[index];
  if (global.type.is_reference()) {
    if (global.mutability && global.imported) {
      Node* base = nullptr;
      Node* offset = nullptr;
      GetBaseAndOffsetForImportedMutableExternRefGlobal(global, &base, &offset);

      gasm_->StoreToObject(
          ObjectAccess(MachineType::AnyTagged(), kFullWriteBarrier), base,
          offset, val);
      return;
    }
    Node* globals_buffer =
        LOAD_INSTANCE_FIELD(TaggedGlobalsBuffer, MachineType::TaggedPointer());
    gasm_->StoreFixedArrayElementAny(globals_buffer, global.offset, val);
    return;
  }

  MachineType mem_type = global.type.machine_type();
  if (mem_type.representation() == MachineRepresentation::kSimd128) {
    has_simd_ = true;
  }
  Node* base = nullptr;
  Node* offset = nullptr;
  GetGlobalBaseAndOffset(mem_type, global, &base, &offset);
  auto store_rep =
      StoreRepresentation(mem_type.representation(), kNoWriteBarrier);
#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndiannessStore(val, mem_type.representation(), global.type);
#endif
  // TODO(manoskouk): Cannot use StoreToObject here due to
  // GetGlobalBaseAndOffset pointer arithmetic.
  gasm_->Store(store_rep, base, offset, val);
}

Node* WasmGraphBuilder::TableGet(uint32_t table_index, Node* index,
                                 wasm::WasmCodePosition position) {
  return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmTableGet,
                                gasm_->IntPtrConstant(table_index), index);
}

void WasmGraphBuilder::TableSet(uint32_t table_index, Node* index, Node* val,
                                wasm::WasmCodePosition position) {
  gasm_->CallRuntimeStub(wasm::WasmCode::kWasmTableSet,
                         gasm_->IntPtrConstant(table_index), index, val);
}

Node* WasmGraphBuilder::CheckBoundsAndAlignment(
    int8_t access_size, Node* index, uint64_t offset,
    wasm::WasmCodePosition position) {
  // Atomic operations need bounds checks until the backend can emit protected
  // loads.
  index =
      BoundsCheckMem(access_size, index, offset, position, kNeedsBoundsCheck);

  const uintptr_t align_mask = access_size - 1;

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  // Don't emit an alignment check if the index is a constant.
  // TODO(wasm): a constant match is also done above in {BoundsCheckMem}.
  UintPtrMatcher match(index);
  if (match.HasResolvedValue()) {
    uintptr_t effective_offset = match.ResolvedValue() + capped_offset;
    if ((effective_offset & align_mask) != 0) {
      // statically known to be unaligned; trap.
      TrapIfEq32(wasm::kTrapUnalignedAccess, Int32Constant(0), 0, position);
    }
    return index;
  }

  // Unlike regular memory accesses, atomic memory accesses should trap if
  // the effective offset is misaligned.
  // TODO(wasm): this addition is redundant with one inserted by {MemBuffer}.
  Node* effective_offset = gasm_->IntAdd(MemBuffer(capped_offset), index);

  Node* cond =
      gasm_->WordAnd(effective_offset, gasm_->IntPtrConstant(align_mask));
  TrapIfFalse(wasm::kTrapUnalignedAccess,
              gasm_->Word32Equal(cond, Int32Constant(0)), position);
  return index;
}

// Insert code to bounds check a memory access if necessary. Return the
// bounds-checked index, which is guaranteed to have (the equivalent of)
// {uintptr_t} representation.
Node* WasmGraphBuilder::BoundsCheckMem(uint8_t access_size, Node* index,
                                       uint64_t offset,
                                       wasm::WasmCodePosition position,
                                       EnforceBoundsCheck enforce_check) {
  DCHECK_LE(1, access_size);
  if (!env_->module->is_memory64) index = Uint32ToUintptr(index);
  if (!FLAG_wasm_bounds_checks) return index;

  if (use_trap_handler() && enforce_check == kCanOmitBoundsCheck) {
    return index;
  }

  // If the offset does not fit in a uintptr_t, this can never succeed on this
  // machine.
  if (offset > std::numeric_limits<uintptr_t>::max() ||
      !base::IsInBounds<uintptr_t>(offset, access_size,
                                   env_->max_memory_size)) {
    // The access will be out of bounds, even for the largest memory.
    TrapIfEq32(wasm::kTrapMemOutOfBounds, Int32Constant(0), 0, position);
    return gasm_->UintPtrConstant(0);
  }
  uintptr_t end_offset = offset + access_size - 1u;
  Node* end_offset_node = mcgraph_->UintPtrConstant(end_offset);

  // In memory64 mode on 32-bit systems, the upper 32 bits need to be zero to
  // succeed the bounds check.
  if (kSystemPointerSize == kInt32Size && env_->module->is_memory64) {
    Node* high_word =
        gasm_->TruncateInt64ToInt32(gasm_->Word64Shr(index, Int32Constant(32)));
    TrapIfTrue(wasm::kTrapMemOutOfBounds, high_word, position);
    // Only use the low word for the following bounds check.
    index = gasm_->TruncateInt64ToInt32(index);
  }

  // The accessed memory is [index + offset, index + end_offset].
  // Check that the last read byte (at {index + end_offset}) is in bounds.
  // 1) Check that {end_offset < mem_size}. This also ensures that we can safely
  //    compute {effective_size} as {mem_size - end_offset)}.
  //    {effective_size} is >= 1 if condition 1) holds.
  // 2) Check that {index + end_offset < mem_size} by
  //    - computing {effective_size} as {mem_size - end_offset} and
  //    - checking that {index < effective_size}.

  Node* mem_size = instance_cache_->mem_size;
  if (end_offset > env_->min_memory_size) {
    // The end offset is larger than the smallest memory.
    // Dynamically check the end offset against the dynamic memory size.
    Node* cond = gasm_->UintLessThan(end_offset_node, mem_size);
    TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);
  } else {
    // The end offset is <= the smallest memory, so only one check is
    // required. Check to see if the index is also a constant.
    UintPtrMatcher match(index);
    if (match.HasResolvedValue()) {
      uintptr_t index_val = match.ResolvedValue();
      if (index_val < env_->min_memory_size - end_offset) {
        // The input index is a constant and everything is statically within
        // bounds of the smallest possible memory.
        return index;
      }
    }
  }

  // This produces a positive number since {end_offset <= min_size <= mem_size}.
  Node* effective_size = gasm_->IntSub(mem_size, end_offset_node);

  // Introduce the actual bounds check.
  Node* cond = gasm_->UintLessThan(index, effective_size);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, cond, position);

  if (untrusted_code_mitigations_) {
    // In the fallthrough case, condition the index with the memory mask.
    Node* mem_mask = instance_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index = gasm_->WordAnd(index, mem_mask);
  }
  return index;
}

const Operator* WasmGraphBuilder::GetSafeLoadOperator(int offset,
                                                      wasm::ValueType type) {
  int alignment = offset % type.element_size_bytes();
  MachineType mach_type = type.machine_type();
  if (COMPRESS_POINTERS_BOOL && mach_type.IsTagged()) {
    // We are loading tagged value from off-heap location, so we need to load
    // it as a full word otherwise we will not be able to decompress it.
    mach_type = MachineType::Pointer();
  }
  if (alignment == 0 || mcgraph()->machine()->UnalignedLoadSupported(
                            type.machine_representation())) {
    return mcgraph()->machine()->Load(mach_type);
  }
  return mcgraph()->machine()->UnalignedLoad(mach_type);
}

const Operator* WasmGraphBuilder::GetSafeStoreOperator(int offset,
                                                       wasm::ValueType type) {
  int alignment = offset % type.element_size_bytes();
  MachineRepresentation rep = type.machine_representation();
  if (COMPRESS_POINTERS_BOOL && IsAnyTagged(rep)) {
    // We are storing tagged value to off-heap location, so we need to store
    // it as a full word otherwise we will not be able to decompress it.
    rep = MachineType::PointerRepresentation();
  }
  if (alignment == 0 || mcgraph()->machine()->UnalignedStoreSupported(rep)) {
    StoreRepresentation store_rep(rep, WriteBarrierKind::kNoWriteBarrier);
    return mcgraph()->machine()->Store(store_rep);
  }
  UnalignedStoreRepresentation store_rep(rep);
  return mcgraph()->machine()->UnalignedStore(store_rep);
}

void WasmGraphBuilder::TraceFunctionEntry(wasm::WasmCodePosition position) {
  Node* call = BuildCallToRuntime(Runtime::kWasmTraceEnter, nullptr, 0);
  SetSourcePosition(call, position);
}

void WasmGraphBuilder::TraceFunctionExit(Vector<Node*> vals,
                                         wasm::WasmCodePosition position) {
  Node* info = gasm_->IntPtrConstant(0);
  size_t num_returns = vals.size();
  if (num_returns == 1) {
    wasm::ValueType return_type = sig_->GetReturn(0);
    MachineRepresentation rep = return_type.machine_representation();
    int size = ElementSizeInBytes(rep);
    info = gasm_->StackSlot(size, size);

    gasm_->Store(StoreRepresentation(rep, kNoWriteBarrier), info,
                 Int32Constant(0), vals[0]);
  }

  Node* call = BuildCallToRuntime(Runtime::kWasmTraceExit, &info, 1);
  SetSourcePosition(call, position);
}

void WasmGraphBuilder::TraceMemoryOperation(bool is_store,
                                            MachineRepresentation rep,
                                            Node* index, uintptr_t offset,
                                            wasm::WasmCodePosition position) {
  int kAlign = 4;  // Ensure that the LSB is 0, such that this looks like a Smi.
  TNode<RawPtrT> info =
      gasm_->StackSlot(sizeof(wasm::MemoryTracingInfo), kAlign);

  Node* effective_offset = gasm_->IntAdd(gasm_->UintPtrConstant(offset), index);
  auto store = [&](int field_offset, MachineRepresentation rep, Node* data) {
    gasm_->Store(StoreRepresentation(rep, kNoWriteBarrier), info,
                 Int32Constant(field_offset), data);
  };
  // Store effective_offset, is_store, and mem_rep.
  store(offsetof(wasm::MemoryTracingInfo, offset),
        MachineType::PointerRepresentation(), effective_offset);
  store(offsetof(wasm::MemoryTracingInfo, is_store),
        MachineRepresentation::kWord8, Int32Constant(is_store ? 1 : 0));
  store(offsetof(wasm::MemoryTracingInfo, mem_rep),
        MachineRepresentation::kWord8, Int32Constant(static_cast<int>(rep)));

  Node* args[] = {info};
  Node* call =
      BuildCallToRuntime(Runtime::kWasmTraceMemory, args, arraysize(args));
  SetSourcePosition(call, position);
}

namespace {
LoadTransformation GetLoadTransformation(
    MachineType memtype, wasm::LoadTransformationKind transform) {
  switch (transform) {
    case wasm::LoadTransformationKind::kSplat: {
      if (memtype == MachineType::Int8()) {
        return LoadTransformation::kS128Load8Splat;
      } else if (memtype == MachineType::Int16()) {
        return LoadTransformation::kS128Load16Splat;
      } else if (memtype == MachineType::Int32()) {
        return LoadTransformation::kS128Load32Splat;
      } else if (memtype == MachineType::Int64()) {
        return LoadTransformation::kS128Load64Splat;
      }
      break;
    }
    case wasm::LoadTransformationKind::kExtend: {
      if (memtype == MachineType::Int8()) {
        return LoadTransformation::kS128Load8x8S;
      } else if (memtype == MachineType::Uint8()) {
        return LoadTransformation::kS128Load8x8U;
      } else if (memtype == MachineType::Int16()) {
        return LoadTransformation::kS128Load16x4S;
      } else if (memtype == MachineType::Uint16()) {
        return LoadTransformation::kS128Load16x4U;
      } else if (memtype == MachineType::Int32()) {
        return LoadTransformation::kS128Load32x2S;
      } else if (memtype == MachineType::Uint32()) {
        return LoadTransformation::kS128Load32x2U;
      }
      break;
    }
    case wasm::LoadTransformationKind::kZeroExtend: {
      if (memtype == MachineType::Int32()) {
        return LoadTransformation::kS128Load32Zero;
      } else if (memtype == MachineType::Int64()) {
        return LoadTransformation::kS128Load64Zero;
      }
      break;
    }
  }
  UNREACHABLE();
}

MemoryAccessKind GetMemoryAccessKind(MachineGraph* mcgraph, MachineType memtype,
                                     bool use_trap_handler) {
  if (memtype.representation() == MachineRepresentation::kWord8 ||
      mcgraph->machine()->UnalignedLoadSupported(memtype.representation())) {
    if (use_trap_handler) {
      return MemoryAccessKind::kProtected;
    }
    return MemoryAccessKind::kNormal;
  }
  // TODO(eholk): Support unaligned loads with trap handlers.
  DCHECK(!use_trap_handler);
  return MemoryAccessKind::kUnaligned;
}
}  // namespace

// S390 simulator does not execute BE code, hence needs to also check if we are
// running on a LE simulator.
// TODO(miladfar): Remove SIM once V8_TARGET_BIG_ENDIAN includes the Sim.
#if defined(V8_TARGET_BIG_ENDIAN) || defined(V8_TARGET_ARCH_S390_LE_SIM)
Node* WasmGraphBuilder::LoadTransformBigEndian(
    wasm::ValueType type, MachineType memtype,
    wasm::LoadTransformationKind transform, Node* index, uint64_t offset,
    uint32_t alignment, wasm::WasmCodePosition position) {
#define LOAD_EXTEND(num_lanes, bytes_per_load, replace_lane)                   \
  result = graph()->NewNode(mcgraph()->machine()->S128Zero());                 \
  Node* values[num_lanes];                                                     \
  for (int i = 0; i < num_lanes; i++) {                                        \
    values[i] = LoadMem(type, memtype, index, offset + i * bytes_per_load,     \
                        alignment, position);                                  \
    if (memtype.IsSigned()) {                                                  \
      /* sign extend */                                                        \
      values[i] = graph()->NewNode(mcgraph()->machine()->ChangeInt32ToInt64(), \
                                   values[i]);                                 \
    } else {                                                                   \
      /* zero extend */                                                        \
      values[i] = graph()->NewNode(                                            \
          mcgraph()->machine()->ChangeUint32ToUint64(), values[i]);            \
    }                                                                          \
  }                                                                            \
  for (int lane = 0; lane < num_lanes; lane++) {                               \
    result = graph()->NewNode(mcgraph()->machine()->replace_lane(lane),        \
                              result, values[lane]);                           \
  }
  Node* result;
  LoadTransformation transformation = GetLoadTransformation(memtype, transform);

  switch (transformation) {
    case LoadTransformation::kS128Load8Splat: {
      result = LoadMem(type, memtype, index, offset, alignment, position);
      result = graph()->NewNode(mcgraph()->machine()->I8x16Splat(), result);
      break;
    }
    case LoadTransformation::kS128Load8x8S:
    case LoadTransformation::kS128Load8x8U: {
      LOAD_EXTEND(8, 1, I16x8ReplaceLane)
      break;
    }
    case LoadTransformation::kS128Load16Splat: {
      result = LoadMem(type, memtype, index, offset, alignment, position);
      result = graph()->NewNode(mcgraph()->machine()->I16x8Splat(), result);
      break;
    }
    case LoadTransformation::kS128Load16x4S:
    case LoadTransformation::kS128Load16x4U: {
      LOAD_EXTEND(4, 2, I32x4ReplaceLane)
      break;
    }
    case LoadTransformation::kS128Load32Splat: {
      result = LoadMem(type, memtype, index, offset, alignment, position);
      result = graph()->NewNode(mcgraph()->machine()->I32x4Splat(), result);
      break;
    }
    case LoadTransformation::kS128Load32x2S:
    case LoadTransformation::kS128Load32x2U: {
      LOAD_EXTEND(2, 4, I64x2ReplaceLane)
      break;
    }
    case LoadTransformation::kS128Load64Splat: {
      result = LoadMem(type, memtype, index, offset, alignment, position);
      result = graph()->NewNode(mcgraph()->machine()->I64x2Splat(), result);
      break;
    }
    case LoadTransformation::kS128Load32Zero: {
      result = graph()->NewNode(mcgraph()->machine()->S128Zero());
      result = graph()->NewNode(
          mcgraph()->machine()->I32x4ReplaceLane(0), result,
          LoadMem(type, memtype, index, offset, alignment, position));
      break;
    }
    case LoadTransformation::kS128Load64Zero: {
      result = graph()->NewNode(mcgraph()->machine()->S128Zero());
      result = graph()->NewNode(
          mcgraph()->machine()->I64x2ReplaceLane(0), result,
          LoadMem(type, memtype, index, offset, alignment, position));
      break;
    }
    default:
      UNREACHABLE();
  }

  return result;
#undef LOAD_EXTEND
}
#endif

Node* WasmGraphBuilder::LoadLane(wasm::ValueType type, MachineType memtype,
                                 Node* value, Node* index, uint64_t offset,
                                 uint32_t alignment, uint8_t laneidx,
                                 wasm::WasmCodePosition position) {
  has_simd_ = true;
  Node* load;
  uint8_t access_size = memtype.MemSize();
  index =
      BoundsCheckMem(access_size, index, offset, position, kCanOmitBoundsCheck);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
#if defined(V8_TARGET_BIG_ENDIAN) || defined(V8_TARGET_ARCH_S390_LE_SIM)
  load = LoadMem(type, memtype, index, offset, alignment, position);
  if (memtype == MachineType::Int8()) {
    load = graph()->NewNode(mcgraph()->machine()->I8x16ReplaceLane(laneidx),
                            value, load);
  } else if (memtype == MachineType::Int16()) {
    load = graph()->NewNode(mcgraph()->machine()->I16x8ReplaceLane(laneidx),
                            value, load);
  } else if (memtype == MachineType::Int32()) {
    load = graph()->NewNode(mcgraph()->machine()->I32x4ReplaceLane(laneidx),
                            value, load);
  } else if (memtype == MachineType::Int64()) {
    load = graph()->NewNode(mcgraph()->machine()->I64x2ReplaceLane(laneidx),
                            value, load);
  } else {
    UNREACHABLE();
  }
#else
  MemoryAccessKind load_kind =
      GetMemoryAccessKind(mcgraph(), memtype, use_trap_handler());

  load = SetEffect(graph()->NewNode(
      mcgraph()->machine()->LoadLane(load_kind, memtype, laneidx),
      MemBuffer(capped_offset), index, value, effect(), control()));

  if (load_kind == MemoryAccessKind::kProtected) {
    SetSourcePosition(load, position);
  }
#endif
  if (FLAG_trace_wasm_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, capped_offset,
                         position);
  }

  return load;
}

Node* WasmGraphBuilder::LoadTransform(wasm::ValueType type, MachineType memtype,
                                      wasm::LoadTransformationKind transform,
                                      Node* index, uint64_t offset,
                                      uint32_t alignment,
                                      wasm::WasmCodePosition position) {
  has_simd_ = true;

  Node* load;
  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);

#if defined(V8_TARGET_BIG_ENDIAN) || defined(V8_TARGET_ARCH_S390_LE_SIM)
  // LoadTransform cannot efficiently be executed on BE machines as a
  // single operation since loaded bytes need to be reversed first,
  // therefore we divide them into separate "load" and "operation" nodes.
  load = LoadTransformBigEndian(type, memtype, transform, index, offset,
                                alignment, position);
  USE(GetMemoryAccessKind);
#else
  // Wasm semantics throw on OOB. Introduce explicit bounds check and
  // conditioning when not using the trap handler.

  // Load extends always load 8 bytes.
  uint8_t access_size = transform == wasm::LoadTransformationKind::kExtend
                            ? 8
                            : memtype.MemSize();
  index =
      BoundsCheckMem(access_size, index, offset, position, kCanOmitBoundsCheck);

  LoadTransformation transformation = GetLoadTransformation(memtype, transform);
  MemoryAccessKind load_kind =
      GetMemoryAccessKind(mcgraph(), memtype, use_trap_handler());

  load = SetEffect(graph()->NewNode(
      mcgraph()->machine()->LoadTransform(load_kind, transformation),
      MemBuffer(capped_offset), index, effect(), control()));

  if (load_kind == MemoryAccessKind::kProtected) {
    SetSourcePosition(load, position);
  }
#endif

  if (FLAG_trace_wasm_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, capped_offset,
                         position);
  }
  return load;
}

Node* WasmGraphBuilder::LoadMem(wasm::ValueType type, MachineType memtype,
                                Node* index, uint64_t offset,
                                uint32_t alignment,
                                wasm::WasmCodePosition position) {
  Node* load;

  if (memtype.representation() == MachineRepresentation::kSimd128) {
    has_simd_ = true;
  }

  // Wasm semantics throw on OOB. Introduce explicit bounds check and
  // conditioning when not using the trap handler.
  index = BoundsCheckMem(memtype.MemSize(), index, offset, position,
                         kCanOmitBoundsCheck);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  if (memtype.representation() == MachineRepresentation::kWord8 ||
      mcgraph()->machine()->UnalignedLoadSupported(memtype.representation())) {
    if (use_trap_handler()) {
      load = gasm_->ProtectedLoad(memtype, MemBuffer(capped_offset), index);
      SetSourcePosition(load, position);
    } else {
      load = gasm_->Load(memtype, MemBuffer(capped_offset), index);
    }
  } else {
    // TODO(eholk): Support unaligned loads with trap handlers.
    DCHECK(!use_trap_handler());
    load = gasm_->LoadUnaligned(memtype, MemBuffer(capped_offset), index);
  }

#if defined(V8_TARGET_BIG_ENDIAN)
  load = BuildChangeEndiannessLoad(load, memtype, type);
#endif

  if (type == wasm::kWasmI64 &&
      ElementSizeInBytes(memtype.representation()) < 8) {
    // TODO(titzer): TF zeroes the upper bits of 64-bit loads for subword sizes.
    load = memtype.IsSigned()
               ? gasm_->ChangeInt32ToInt64(load)     // sign extend
               : gasm_->ChangeUint32ToUint64(load);  // zero extend
  }

  if (FLAG_trace_wasm_memory) {
    TraceMemoryOperation(false, memtype.representation(), index, capped_offset,
                         position);
  }

  return load;
}

void WasmGraphBuilder::StoreLane(MachineRepresentation mem_rep, Node* index,
                                 uint64_t offset, uint32_t alignment, Node* val,
                                 uint8_t laneidx,
                                 wasm::WasmCodePosition position,
                                 wasm::ValueType type) {
  has_simd_ = true;
  index = BoundsCheckMem(i::ElementSizeInBytes(mem_rep), index, offset,
                         position, kCanOmitBoundsCheck);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
#if defined(V8_TARGET_BIG_ENDIAN) || defined(V8_TARGET_ARCH_S390_LE_SIM)
  Node* output;
  if (mem_rep == MachineRepresentation::kWord8) {
    output =
        graph()->NewNode(mcgraph()->machine()->I8x16ExtractLaneS(laneidx), val);
  } else if (mem_rep == MachineRepresentation::kWord16) {
    output =
        graph()->NewNode(mcgraph()->machine()->I16x8ExtractLaneS(laneidx), val);
  } else if (mem_rep == MachineRepresentation::kWord32) {
    output =
        graph()->NewNode(mcgraph()->machine()->I32x4ExtractLane(laneidx), val);
  } else if (mem_rep == MachineRepresentation::kWord64) {
    output =
        graph()->NewNode(mcgraph()->machine()->I64x2ExtractLane(laneidx), val);
  } else {
    UNREACHABLE();
  }
  StoreMem(mem_rep, index, offset, alignment, output, position, type);
#else
  MachineType memtype = MachineType(mem_rep, MachineSemantic::kNone);
  MemoryAccessKind load_kind =
      GetMemoryAccessKind(mcgraph(), memtype, use_trap_handler());

  Node* store = SetEffect(graph()->NewNode(
      mcgraph()->machine()->StoreLane(load_kind, mem_rep, laneidx),
      MemBuffer(capped_offset), index, val, effect(), control()));

  if (load_kind == MemoryAccessKind::kProtected) {
    SetSourcePosition(store, position);
  }
#endif
  if (FLAG_trace_wasm_memory) {
    TraceMemoryOperation(true, mem_rep, index, capped_offset, position);
  }
}

void WasmGraphBuilder::StoreMem(MachineRepresentation mem_rep, Node* index,
                                uint64_t offset, uint32_t alignment, Node* val,
                                wasm::WasmCodePosition position,
                                wasm::ValueType type) {
  if (mem_rep == MachineRepresentation::kSimd128) {
    has_simd_ = true;
  }

  index = BoundsCheckMem(i::ElementSizeInBytes(mem_rep), index, offset,
                         position, kCanOmitBoundsCheck);

#if defined(V8_TARGET_BIG_ENDIAN)
  val = BuildChangeEndiannessStore(val, mem_rep, type);
#endif

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  if (mem_rep == MachineRepresentation::kWord8 ||
      mcgraph()->machine()->UnalignedStoreSupported(mem_rep)) {
    if (use_trap_handler()) {
      Node* store =
          gasm_->ProtectedStore(mem_rep, MemBuffer(capped_offset), index, val);
      SetSourcePosition(store, position);
    } else {
      gasm_->Store(StoreRepresentation{mem_rep, kNoWriteBarrier},
                   MemBuffer(capped_offset), index, val);
    }
  } else {
    // TODO(eholk): Support unaligned stores with trap handlers.
    DCHECK(!use_trap_handler());
    UnalignedStoreRepresentation rep(mem_rep);
    gasm_->StoreUnaligned(rep, MemBuffer(capped_offset), index, val);
  }

  if (FLAG_trace_wasm_memory) {
    TraceMemoryOperation(true, mem_rep, index, capped_offset, position);
  }
}

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
  Diamond bounds_check(graph(), mcgraph()->common(),
                       gasm_->UintLessThan(index, mem_size), BranchHint::kTrue);
  bounds_check.Chain(control());

  if (untrusted_code_mitigations_) {
    // Condition the index with the memory mask.
    Node* mem_mask = instance_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index = gasm_->WordAnd(index, mem_mask);
  }

  Node* load = graph()->NewNode(mcgraph()->machine()->Load(type), mem_start,
                                index, effect(), bounds_check.if_true);
  SetEffectControl(bounds_check.EffectPhi(load, effect()), bounds_check.merge);

  Node* oob_value;
  switch (type.representation()) {
    case MachineRepresentation::kWord8:
    case MachineRepresentation::kWord16:
    case MachineRepresentation::kWord32:
      oob_value = Int32Constant(0);
      break;
    case MachineRepresentation::kWord64:
      oob_value = Int64Constant(0);
      break;
    case MachineRepresentation::kFloat32:
      oob_value = Float32Constant(std::numeric_limits<float>::quiet_NaN());
      break;
    case MachineRepresentation::kFloat64:
      oob_value = Float64Constant(std::numeric_limits<double>::quiet_NaN());
      break;
    default:
      UNREACHABLE();
  }

  return bounds_check.Phi(type.representation(), load, oob_value);
}

Node* WasmGraphBuilder::Uint32ToUintptr(Node* node) {
  if (mcgraph()->machine()->Is32()) return node;
  // Fold instances of ChangeUint32ToUint64(IntConstant) directly.
  Uint32Matcher matcher(node);
  if (matcher.HasResolvedValue()) {
    uintptr_t value = matcher.ResolvedValue();
    return mcgraph()->IntPtrConstant(bit_cast<intptr_t>(value));
  }
  return gasm_->ChangeUint32ToUint64(node);
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
  Diamond bounds_check(graph(), mcgraph()->common(),
                       gasm_->Uint32LessThan(index, mem_size),
                       BranchHint::kTrue);
  bounds_check.Chain(control());

  if (untrusted_code_mitigations_) {
    // Condition the index with the memory mask.
    Node* mem_mask = instance_cache_->mem_mask;
    DCHECK_NOT_NULL(mem_mask);
    index = gasm_->Word32And(index, mem_mask);
  }

  index = Uint32ToUintptr(index);
  const Operator* store_op = mcgraph()->machine()->Store(StoreRepresentation(
      type.representation(), WriteBarrierKind::kNoWriteBarrier));
  Node* store = graph()->NewNode(store_op, mem_start, index, val, effect(),
                                 bounds_check.if_true);
  SetEffectControl(bounds_check.EffectPhi(store, effect()), bounds_check.merge);
  return val;
}

Node* WasmGraphBuilder::BuildF64x2Ceil(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64x2Floor(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64x2Trunc(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_trunc();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF64x2NearestInt(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f64x2_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4Ceil(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_ceil();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4Floor(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_floor();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4Trunc(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_trunc();
  return BuildCFuncInstruction(ref, type, input);
}

Node* WasmGraphBuilder::BuildF32x4NearestInt(Node* input) {
  MachineType type = MachineType::Simd128();
  ExternalReference ref = ExternalReference::wasm_f32x4_nearest_int();
  return BuildCFuncInstruction(ref, type, input);
}

void WasmGraphBuilder::PrintDebugName(Node* node) {
  PrintF("#%d:%s", node->id(), node->op()->mnemonic());
}

Graph* WasmGraphBuilder::graph() { return mcgraph()->graph(); }

namespace {
Signature<MachineRepresentation>* CreateMachineSignature(
    Zone* zone, const wasm::FunctionSig* sig,
    WasmGraphBuilder::CallOrigin origin) {
  Signature<MachineRepresentation>::Builder builder(zone, sig->return_count(),
                                                    sig->parameter_count());
  for (auto ret : sig->returns()) {
    if (origin == WasmGraphBuilder::kCalledFromJS) {
      builder.AddReturn(MachineRepresentation::kTagged);
    } else {
      builder.AddReturn(ret.machine_representation());
    }
  }

  for (auto param : sig->parameters()) {
    if (origin == WasmGraphBuilder::kCalledFromJS) {
      // Parameters coming from JavaScript are always tagged values. Especially
      // when the signature says that it's an I64 value, then a BigInt object is
      // provided by JavaScript, and not two 32-bit parameters.
      builder.AddParam(MachineRepresentation::kTagged);
    } else {
      builder.AddParam(param.machine_representation());
    }
  }
  return builder.Build();
}

}  // namespace

void WasmGraphBuilder::AddInt64LoweringReplacement(
    CallDescriptor* original, CallDescriptor* replacement) {
  if (!lowering_special_case_) {
    lowering_special_case_ = std::make_unique<Int64LoweringSpecialCase>();
  }
  lowering_special_case_->replacements.insert({original, replacement});
}

CallDescriptor* WasmGraphBuilder::GetI32AtomicWaitCallDescriptor() {
  if (i32_atomic_wait_descriptor_) return i32_atomic_wait_descriptor_;

  i32_atomic_wait_descriptor_ =
      GetBuiltinCallDescriptor(Builtins::kWasmI32AtomicWait64, zone_,
                               StubCallMode::kCallWasmRuntimeStub);

  AddInt64LoweringReplacement(
      i32_atomic_wait_descriptor_,
      GetBuiltinCallDescriptor(Builtins::kWasmI32AtomicWait32, zone_,
                               StubCallMode::kCallWasmRuntimeStub));

  return i32_atomic_wait_descriptor_;
}

CallDescriptor* WasmGraphBuilder::GetI64AtomicWaitCallDescriptor() {
  if (i64_atomic_wait_descriptor_) return i64_atomic_wait_descriptor_;

  i64_atomic_wait_descriptor_ =
      GetBuiltinCallDescriptor(Builtins::kWasmI64AtomicWait64, zone_,
                               StubCallMode::kCallWasmRuntimeStub);

  AddInt64LoweringReplacement(
      i64_atomic_wait_descriptor_,
      GetBuiltinCallDescriptor(Builtins::kWasmI64AtomicWait32, zone_,
                               StubCallMode::kCallWasmRuntimeStub));

  return i64_atomic_wait_descriptor_;
}

void WasmGraphBuilder::LowerInt64(Signature<MachineRepresentation>* sig) {
  if (mcgraph()->machine()->Is64()) return;
  Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(), mcgraph()->common(),
                  gasm_->simplified(), mcgraph()->zone(), sig,
                  std::move(lowering_special_case_));
  r.LowerGraph();
}

void WasmGraphBuilder::LowerInt64(CallOrigin origin) {
  LowerInt64(CreateMachineSignature(mcgraph()->zone(), sig_, origin));
}

void WasmGraphBuilder::SetSourcePosition(Node* node,
                                         wasm::WasmCodePosition position) {
  DCHECK_NE(position, wasm::kNoCodePosition);
  if (source_position_table_) {
    source_position_table_->SetSourcePosition(node, SourcePosition(position));
  }
}

Node* WasmGraphBuilder::S128Zero() {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->S128Zero());
}

Node* WasmGraphBuilder::SimdOp(wasm::WasmOpcode opcode, Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF64x2Splat:
      return graph()->NewNode(mcgraph()->machine()->F64x2Splat(), inputs[0]);
    case wasm::kExprF64x2Abs:
      return graph()->NewNode(mcgraph()->machine()->F64x2Abs(), inputs[0]);
    case wasm::kExprF64x2Neg:
      return graph()->NewNode(mcgraph()->machine()->F64x2Neg(), inputs[0]);
    case wasm::kExprF64x2Sqrt:
      return graph()->NewNode(mcgraph()->machine()->F64x2Sqrt(), inputs[0]);
    case wasm::kExprF64x2Add:
      return graph()->NewNode(mcgraph()->machine()->F64x2Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Sub:
      return graph()->NewNode(mcgraph()->machine()->F64x2Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Mul:
      return graph()->NewNode(mcgraph()->machine()->F64x2Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Div:
      return graph()->NewNode(mcgraph()->machine()->F64x2Div(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Min:
      return graph()->NewNode(mcgraph()->machine()->F64x2Min(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Max:
      return graph()->NewNode(mcgraph()->machine()->F64x2Max(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Eq:
      return graph()->NewNode(mcgraph()->machine()->F64x2Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Ne:
      return graph()->NewNode(mcgraph()->machine()->F64x2Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Lt:
      return graph()->NewNode(mcgraph()->machine()->F64x2Lt(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Le:
      return graph()->NewNode(mcgraph()->machine()->F64x2Le(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Gt:
      return graph()->NewNode(mcgraph()->machine()->F64x2Lt(), inputs[1],
                              inputs[0]);
    case wasm::kExprF64x2Ge:
      return graph()->NewNode(mcgraph()->machine()->F64x2Le(), inputs[1],
                              inputs[0]);
    case wasm::kExprF64x2Qfma:
      return graph()->NewNode(mcgraph()->machine()->F64x2Qfma(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF64x2Qfms:
      return graph()->NewNode(mcgraph()->machine()->F64x2Qfms(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF64x2Pmin:
      return graph()->NewNode(mcgraph()->machine()->F64x2Pmin(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Pmax:
      return graph()->NewNode(mcgraph()->machine()->F64x2Pmax(), inputs[0],
                              inputs[1]);
    case wasm::kExprF64x2Ceil:
      // Architecture support for F64x2Ceil and Float64RoundUp is the same.
      if (!mcgraph()->machine()->Float64RoundUp().IsSupported())
        return BuildF64x2Ceil(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2Ceil(), inputs[0]);
    case wasm::kExprF64x2Floor:
      // Architecture support for F64x2Floor and Float64RoundDown is the same.
      if (!mcgraph()->machine()->Float64RoundDown().IsSupported())
        return BuildF64x2Floor(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2Floor(), inputs[0]);
    case wasm::kExprF64x2Trunc:
      // Architecture support for F64x2Trunc and Float64RoundTruncate is the
      // same.
      if (!mcgraph()->machine()->Float64RoundTruncate().IsSupported())
        return BuildF64x2Trunc(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2Trunc(), inputs[0]);
    case wasm::kExprF64x2NearestInt:
      // Architecture support for F64x2NearestInt and Float64RoundTiesEven is
      // the same.
      if (!mcgraph()->machine()->Float64RoundTiesEven().IsSupported())
        return BuildF64x2NearestInt(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F64x2NearestInt(),
                              inputs[0]);
    case wasm::kExprF64x2ConvertLowI32x4S:
      return graph()->NewNode(mcgraph()->machine()->F64x2ConvertLowI32x4S(),
                              inputs[0]);
    case wasm::kExprF64x2ConvertLowI32x4U:
      return graph()->NewNode(mcgraph()->machine()->F64x2ConvertLowI32x4U(),
                              inputs[0]);
    case wasm::kExprF64x2PromoteLowF32x4:
      return graph()->NewNode(mcgraph()->machine()->F64x2PromoteLowF32x4(),
                              inputs[0]);
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
    case wasm::kExprF32x4Sqrt:
      return graph()->NewNode(mcgraph()->machine()->F32x4Sqrt(), inputs[0]);
    case wasm::kExprF32x4RecipApprox:
      return graph()->NewNode(mcgraph()->machine()->F32x4RecipApprox(),
                              inputs[0]);
    case wasm::kExprF32x4RecipSqrtApprox:
      return graph()->NewNode(mcgraph()->machine()->F32x4RecipSqrtApprox(),
                              inputs[0]);
    case wasm::kExprF32x4Add:
      return graph()->NewNode(mcgraph()->machine()->F32x4Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Sub:
      return graph()->NewNode(mcgraph()->machine()->F32x4Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Mul:
      return graph()->NewNode(mcgraph()->machine()->F32x4Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Div:
      return graph()->NewNode(mcgraph()->machine()->F32x4Div(), inputs[0],
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
    case wasm::kExprF32x4Qfma:
      return graph()->NewNode(mcgraph()->machine()->F32x4Qfma(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF32x4Qfms:
      return graph()->NewNode(mcgraph()->machine()->F32x4Qfms(), inputs[0],
                              inputs[1], inputs[2]);
    case wasm::kExprF32x4Pmin:
      return graph()->NewNode(mcgraph()->machine()->F32x4Pmin(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Pmax:
      return graph()->NewNode(mcgraph()->machine()->F32x4Pmax(), inputs[0],
                              inputs[1]);
    case wasm::kExprF32x4Ceil:
      // Architecture support for F32x4Ceil and Float32RoundUp is the same.
      if (!mcgraph()->machine()->Float32RoundUp().IsSupported())
        return BuildF32x4Ceil(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4Ceil(), inputs[0]);
    case wasm::kExprF32x4Floor:
      // Architecture support for F32x4Floor and Float32RoundDown is the same.
      if (!mcgraph()->machine()->Float32RoundDown().IsSupported())
        return BuildF32x4Floor(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4Floor(), inputs[0]);
    case wasm::kExprF32x4Trunc:
      // Architecture support for F32x4Trunc and Float32RoundTruncate is the
      // same.
      if (!mcgraph()->machine()->Float32RoundTruncate().IsSupported())
        return BuildF32x4Trunc(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4Trunc(), inputs[0]);
    case wasm::kExprF32x4NearestInt:
      // Architecture support for F32x4NearestInt and Float32RoundTiesEven is
      // the same.
      if (!mcgraph()->machine()->Float32RoundTiesEven().IsSupported())
        return BuildF32x4NearestInt(inputs[0]);
      return graph()->NewNode(mcgraph()->machine()->F32x4NearestInt(),
                              inputs[0]);
    case wasm::kExprF32x4DemoteF64x2Zero:
      return graph()->NewNode(mcgraph()->machine()->F32x4DemoteF64x2Zero(),
                              inputs[0]);
    case wasm::kExprI64x2Splat:
      return graph()->NewNode(mcgraph()->machine()->I64x2Splat(), inputs[0]);
    case wasm::kExprI64x2Abs:
      return graph()->NewNode(mcgraph()->machine()->I64x2Abs(), inputs[0]);
    case wasm::kExprI64x2Neg:
      return graph()->NewNode(mcgraph()->machine()->I64x2Neg(), inputs[0]);
    case wasm::kExprI64x2SConvertI32x4Low:
      return graph()->NewNode(mcgraph()->machine()->I64x2SConvertI32x4Low(),
                              inputs[0]);
    case wasm::kExprI64x2SConvertI32x4High:
      return graph()->NewNode(mcgraph()->machine()->I64x2SConvertI32x4High(),
                              inputs[0]);
    case wasm::kExprI64x2UConvertI32x4Low:
      return graph()->NewNode(mcgraph()->machine()->I64x2UConvertI32x4Low(),
                              inputs[0]);
    case wasm::kExprI64x2UConvertI32x4High:
      return graph()->NewNode(mcgraph()->machine()->I64x2UConvertI32x4High(),
                              inputs[0]);
    case wasm::kExprI64x2BitMask:
      return graph()->NewNode(mcgraph()->machine()->I64x2BitMask(), inputs[0]);
    case wasm::kExprI64x2Shl:
      return graph()->NewNode(mcgraph()->machine()->I64x2Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2ShrS:
      return graph()->NewNode(mcgraph()->machine()->I64x2ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Add:
      return graph()->NewNode(mcgraph()->machine()->I64x2Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Sub:
      return graph()->NewNode(mcgraph()->machine()->I64x2Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Mul:
      return graph()->NewNode(mcgraph()->machine()->I64x2Mul(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Eq:
      return graph()->NewNode(mcgraph()->machine()->I64x2Eq(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2Ne:
      return graph()->NewNode(mcgraph()->machine()->I64x2Ne(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2LtS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GtS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI64x2LeS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GeS(), inputs[1],
                              inputs[0]);
    case wasm::kExprI64x2GtS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GtS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2GeS:
      return graph()->NewNode(mcgraph()->machine()->I64x2GeS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2ShrU:
      return graph()->NewNode(mcgraph()->machine()->I64x2ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2ExtMulLowI32x4S:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulLowI32x4S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtMulHighI32x4S:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulHighI32x4S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtMulLowI32x4U:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulLowI32x4U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtMulHighI32x4U:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtMulHighI32x4U(),
                              inputs[0], inputs[1]);
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
    case wasm::kExprI32x4Shl:
      return graph()->NewNode(mcgraph()->machine()->I32x4Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4ShrS:
      return graph()->NewNode(mcgraph()->machine()->I32x4ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4Add:
      return graph()->NewNode(mcgraph()->machine()->I32x4Add(), inputs[0],
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
    case wasm::kExprI32x4ShrU:
      return graph()->NewNode(mcgraph()->machine()->I32x4ShrU(), inputs[0],
                              inputs[1]);
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
    case wasm::kExprI32x4Abs:
      return graph()->NewNode(mcgraph()->machine()->I32x4Abs(), inputs[0]);
    case wasm::kExprI32x4BitMask:
      return graph()->NewNode(mcgraph()->machine()->I32x4BitMask(), inputs[0]);
    case wasm::kExprI32x4DotI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4DotI16x8S(), inputs[0],
                              inputs[1]);
    case wasm::kExprI32x4ExtMulLowI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulLowI16x8S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtMulHighI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulHighI16x8S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtMulLowI16x8U:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulLowI16x8U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtMulHighI16x8U:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtMulHighI16x8U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtAddPairwiseI16x8S:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtAddPairwiseI16x8S(),
                              inputs[0]);
    case wasm::kExprI32x4ExtAddPairwiseI16x8U:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtAddPairwiseI16x8U(),
                              inputs[0]);
    case wasm::kExprI32x4TruncSatF64x2SZero:
      return graph()->NewNode(mcgraph()->machine()->I32x4TruncSatF64x2SZero(),
                              inputs[0]);
    case wasm::kExprI32x4TruncSatF64x2UZero:
      return graph()->NewNode(mcgraph()->machine()->I32x4TruncSatF64x2UZero(),
                              inputs[0]);
    case wasm::kExprI16x8Splat:
      return graph()->NewNode(mcgraph()->machine()->I16x8Splat(), inputs[0]);
    case wasm::kExprI16x8SConvertI8x16Low:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI8x16Low(),
                              inputs[0]);
    case wasm::kExprI16x8SConvertI8x16High:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI8x16High(),
                              inputs[0]);
    case wasm::kExprI16x8Shl:
      return graph()->NewNode(mcgraph()->machine()->I16x8Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8ShrS:
      return graph()->NewNode(mcgraph()->machine()->I16x8ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Neg:
      return graph()->NewNode(mcgraph()->machine()->I16x8Neg(), inputs[0]);
    case wasm::kExprI16x8SConvertI32x4:
      return graph()->NewNode(mcgraph()->machine()->I16x8SConvertI32x4(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Add:
      return graph()->NewNode(mcgraph()->machine()->I16x8Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8AddSatS:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddSatS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8Sub:
      return graph()->NewNode(mcgraph()->machine()->I16x8Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8SubSatS:
      return graph()->NewNode(mcgraph()->machine()->I16x8SubSatS(), inputs[0],
                              inputs[1]);
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
    case wasm::kExprI16x8ShrU:
      return graph()->NewNode(mcgraph()->machine()->I16x8ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8AddSatU:
      return graph()->NewNode(mcgraph()->machine()->I16x8AddSatU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI16x8SubSatU:
      return graph()->NewNode(mcgraph()->machine()->I16x8SubSatU(), inputs[0],
                              inputs[1]);
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
    case wasm::kExprI16x8RoundingAverageU:
      return graph()->NewNode(mcgraph()->machine()->I16x8RoundingAverageU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Q15MulRSatS:
      return graph()->NewNode(mcgraph()->machine()->I16x8Q15MulRSatS(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8Abs:
      return graph()->NewNode(mcgraph()->machine()->I16x8Abs(), inputs[0]);
    case wasm::kExprI16x8BitMask:
      return graph()->NewNode(mcgraph()->machine()->I16x8BitMask(), inputs[0]);
    case wasm::kExprI16x8ExtMulLowI8x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulLowI8x16S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtMulHighI8x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulHighI8x16S(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtMulLowI8x16U:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulLowI8x16U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtMulHighI8x16U:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtMulHighI8x16U(),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtAddPairwiseI8x16S:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtAddPairwiseI8x16S(),
                              inputs[0]);
    case wasm::kExprI16x8ExtAddPairwiseI8x16U:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtAddPairwiseI8x16U(),
                              inputs[0]);
    case wasm::kExprI8x16Splat:
      return graph()->NewNode(mcgraph()->machine()->I8x16Splat(), inputs[0]);
    case wasm::kExprI8x16Neg:
      return graph()->NewNode(mcgraph()->machine()->I8x16Neg(), inputs[0]);
    case wasm::kExprI8x16Shl:
      return graph()->NewNode(mcgraph()->machine()->I8x16Shl(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16ShrS:
      return graph()->NewNode(mcgraph()->machine()->I8x16ShrS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SConvertI16x8:
      return graph()->NewNode(mcgraph()->machine()->I8x16SConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Add:
      return graph()->NewNode(mcgraph()->machine()->I8x16Add(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16AddSatS:
      return graph()->NewNode(mcgraph()->machine()->I8x16AddSatS(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16Sub:
      return graph()->NewNode(mcgraph()->machine()->I8x16Sub(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SubSatS:
      return graph()->NewNode(mcgraph()->machine()->I8x16SubSatS(), inputs[0],
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
    case wasm::kExprI8x16ShrU:
      return graph()->NewNode(mcgraph()->machine()->I8x16ShrU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16UConvertI16x8:
      return graph()->NewNode(mcgraph()->machine()->I8x16UConvertI16x8(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16AddSatU:
      return graph()->NewNode(mcgraph()->machine()->I8x16AddSatU(), inputs[0],
                              inputs[1]);
    case wasm::kExprI8x16SubSatU:
      return graph()->NewNode(mcgraph()->machine()->I8x16SubSatU(), inputs[0],
                              inputs[1]);
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
    case wasm::kExprI8x16RoundingAverageU:
      return graph()->NewNode(mcgraph()->machine()->I8x16RoundingAverageU(),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16Popcnt:
      return graph()->NewNode(mcgraph()->machine()->I8x16Popcnt(), inputs[0]);
    case wasm::kExprI8x16Abs:
      return graph()->NewNode(mcgraph()->machine()->I8x16Abs(), inputs[0]);
    case wasm::kExprI8x16BitMask:
      return graph()->NewNode(mcgraph()->machine()->I8x16BitMask(), inputs[0]);
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
      return graph()->NewNode(mcgraph()->machine()->S128Select(), inputs[2],
                              inputs[0], inputs[1]);
    case wasm::kExprS128AndNot:
      return graph()->NewNode(mcgraph()->machine()->S128AndNot(), inputs[0],
                              inputs[1]);
    case wasm::kExprI64x2AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I64x2AllTrue(), inputs[0]);
    case wasm::kExprI32x4AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I32x4AllTrue(), inputs[0]);
    case wasm::kExprI16x8AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I16x8AllTrue(), inputs[0]);
    case wasm::kExprV128AnyTrue:
      return graph()->NewNode(mcgraph()->machine()->V128AnyTrue(), inputs[0]);
    case wasm::kExprI8x16AllTrue:
      return graph()->NewNode(mcgraph()->machine()->I8x16AllTrue(), inputs[0]);
    case wasm::kExprI8x16Swizzle:
      return graph()->NewNode(mcgraph()->machine()->I8x16Swizzle(), inputs[0],
                              inputs[1]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::SimdLaneOp(wasm::WasmOpcode opcode, uint8_t lane,
                                   Node* const* inputs) {
  has_simd_ = true;
  switch (opcode) {
    case wasm::kExprF64x2ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->F64x2ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF64x2ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->F64x2ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprF32x4ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->F32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprF32x4ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->F32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI64x2ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I64x2ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI64x2ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I64x2ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI32x4ExtractLane:
      return graph()->NewNode(mcgraph()->machine()->I32x4ExtractLane(lane),
                              inputs[0]);
    case wasm::kExprI32x4ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I32x4ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI16x8ExtractLaneS:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtractLaneS(lane),
                              inputs[0]);
    case wasm::kExprI16x8ExtractLaneU:
      return graph()->NewNode(mcgraph()->machine()->I16x8ExtractLaneU(lane),
                              inputs[0]);
    case wasm::kExprI16x8ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I16x8ReplaceLane(lane),
                              inputs[0], inputs[1]);
    case wasm::kExprI8x16ExtractLaneS:
      return graph()->NewNode(mcgraph()->machine()->I8x16ExtractLaneS(lane),
                              inputs[0]);
    case wasm::kExprI8x16ExtractLaneU:
      return graph()->NewNode(mcgraph()->machine()->I8x16ExtractLaneU(lane),
                              inputs[0]);
    case wasm::kExprI8x16ReplaceLane:
      return graph()->NewNode(mcgraph()->machine()->I8x16ReplaceLane(lane),
                              inputs[0], inputs[1]);
    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

Node* WasmGraphBuilder::Simd8x16ShuffleOp(const uint8_t shuffle[16],
                                          Node* const* inputs) {
  has_simd_ = true;
  return graph()->NewNode(mcgraph()->machine()->I8x16Shuffle(shuffle),
                          inputs[0], inputs[1]);
}

Node* WasmGraphBuilder::AtomicOp(wasm::WasmOpcode opcode, Node* const* inputs,
                                 uint32_t alignment, uint64_t offset,
                                 wasm::WasmCodePosition position) {
  struct AtomicOpInfo {
    enum Type : int8_t {
      kNoInput = 0,
      kOneInput = 1,
      kTwoInputs = 2,
      kSpecial
    };

    using OperatorByType =
        const Operator* (MachineOperatorBuilder::*)(MachineType);
    using OperatorByRep =
        const Operator* (MachineOperatorBuilder::*)(MachineRepresentation);

    const Type type;
    const MachineType machine_type;
    const OperatorByType operator_by_type = nullptr;
    const OperatorByRep operator_by_rep = nullptr;

    constexpr AtomicOpInfo(Type t, MachineType m, OperatorByType o)
        : type(t), machine_type(m), operator_by_type(o) {}
    constexpr AtomicOpInfo(Type t, MachineType m, OperatorByRep o)
        : type(t), machine_type(m), operator_by_rep(o) {}

    // Constexpr, hence just a table lookup in most compilers.
    static constexpr AtomicOpInfo Get(wasm::WasmOpcode opcode) {
      switch (opcode) {
#define CASE(Name, Type, MachType, Op) \
  case wasm::kExpr##Name:              \
    return {Type, MachineType::MachType(), &MachineOperatorBuilder::Op};

        // Binops.
        CASE(I32AtomicAdd, kOneInput, Uint32, Word32AtomicAdd)
        CASE(I64AtomicAdd, kOneInput, Uint64, Word64AtomicAdd)
        CASE(I32AtomicAdd8U, kOneInput, Uint8, Word32AtomicAdd)
        CASE(I32AtomicAdd16U, kOneInput, Uint16, Word32AtomicAdd)
        CASE(I64AtomicAdd8U, kOneInput, Uint8, Word64AtomicAdd)
        CASE(I64AtomicAdd16U, kOneInput, Uint16, Word64AtomicAdd)
        CASE(I64AtomicAdd32U, kOneInput, Uint32, Word64AtomicAdd)
        CASE(I32AtomicSub, kOneInput, Uint32, Word32AtomicSub)
        CASE(I64AtomicSub, kOneInput, Uint64, Word64AtomicSub)
        CASE(I32AtomicSub8U, kOneInput, Uint8, Word32AtomicSub)
        CASE(I32AtomicSub16U, kOneInput, Uint16, Word32AtomicSub)
        CASE(I64AtomicSub8U, kOneInput, Uint8, Word64AtomicSub)
        CASE(I64AtomicSub16U, kOneInput, Uint16, Word64AtomicSub)
        CASE(I64AtomicSub32U, kOneInput, Uint32, Word64AtomicSub)
        CASE(I32AtomicAnd, kOneInput, Uint32, Word32AtomicAnd)
        CASE(I64AtomicAnd, kOneInput, Uint64, Word64AtomicAnd)
        CASE(I32AtomicAnd8U, kOneInput, Uint8, Word32AtomicAnd)
        CASE(I32AtomicAnd16U, kOneInput, Uint16, Word32AtomicAnd)
        CASE(I64AtomicAnd8U, kOneInput, Uint8, Word64AtomicAnd)
        CASE(I64AtomicAnd16U, kOneInput, Uint16, Word64AtomicAnd)
        CASE(I64AtomicAnd32U, kOneInput, Uint32, Word64AtomicAnd)
        CASE(I32AtomicOr, kOneInput, Uint32, Word32AtomicOr)
        CASE(I64AtomicOr, kOneInput, Uint64, Word64AtomicOr)
        CASE(I32AtomicOr8U, kOneInput, Uint8, Word32AtomicOr)
        CASE(I32AtomicOr16U, kOneInput, Uint16, Word32AtomicOr)
        CASE(I64AtomicOr8U, kOneInput, Uint8, Word64AtomicOr)
        CASE(I64AtomicOr16U, kOneInput, Uint16, Word64AtomicOr)
        CASE(I64AtomicOr32U, kOneInput, Uint32, Word64AtomicOr)
        CASE(I32AtomicXor, kOneInput, Uint32, Word32AtomicXor)
        CASE(I64AtomicXor, kOneInput, Uint64, Word64AtomicXor)
        CASE(I32AtomicXor8U, kOneInput, Uint8, Word32AtomicXor)
        CASE(I32AtomicXor16U, kOneInput, Uint16, Word32AtomicXor)
        CASE(I64AtomicXor8U, kOneInput, Uint8, Word64AtomicXor)
        CASE(I64AtomicXor16U, kOneInput, Uint16, Word64AtomicXor)
        CASE(I64AtomicXor32U, kOneInput, Uint32, Word64AtomicXor)
        CASE(I32AtomicExchange, kOneInput, Uint32, Word32AtomicExchange)
        CASE(I64AtomicExchange, kOneInput, Uint64, Word64AtomicExchange)
        CASE(I32AtomicExchange8U, kOneInput, Uint8, Word32AtomicExchange)
        CASE(I32AtomicExchange16U, kOneInput, Uint16, Word32AtomicExchange)
        CASE(I64AtomicExchange8U, kOneInput, Uint8, Word64AtomicExchange)
        CASE(I64AtomicExchange16U, kOneInput, Uint16, Word64AtomicExchange)
        CASE(I64AtomicExchange32U, kOneInput, Uint32, Word64AtomicExchange)

        // Compare-exchange.
        CASE(I32AtomicCompareExchange, kTwoInputs, Uint32,
             Word32AtomicCompareExchange)
        CASE(I64AtomicCompareExchange, kTwoInputs, Uint64,
             Word64AtomicCompareExchange)
        CASE(I32AtomicCompareExchange8U, kTwoInputs, Uint8,
             Word32AtomicCompareExchange)
        CASE(I32AtomicCompareExchange16U, kTwoInputs, Uint16,
             Word32AtomicCompareExchange)
        CASE(I64AtomicCompareExchange8U, kTwoInputs, Uint8,
             Word64AtomicCompareExchange)
        CASE(I64AtomicCompareExchange16U, kTwoInputs, Uint16,
             Word64AtomicCompareExchange)
        CASE(I64AtomicCompareExchange32U, kTwoInputs, Uint32,
             Word64AtomicCompareExchange)

        // Load.
        CASE(I32AtomicLoad, kNoInput, Uint32, Word32AtomicLoad)
        CASE(I64AtomicLoad, kNoInput, Uint64, Word64AtomicLoad)
        CASE(I32AtomicLoad8U, kNoInput, Uint8, Word32AtomicLoad)
        CASE(I32AtomicLoad16U, kNoInput, Uint16, Word32AtomicLoad)
        CASE(I64AtomicLoad8U, kNoInput, Uint8, Word64AtomicLoad)
        CASE(I64AtomicLoad16U, kNoInput, Uint16, Word64AtomicLoad)
        CASE(I64AtomicLoad32U, kNoInput, Uint32, Word64AtomicLoad)

        // Store.
        CASE(I32AtomicStore, kOneInput, Uint32, Word32AtomicStore)
        CASE(I64AtomicStore, kOneInput, Uint64, Word64AtomicStore)
        CASE(I32AtomicStore8U, kOneInput, Uint8, Word32AtomicStore)
        CASE(I32AtomicStore16U, kOneInput, Uint16, Word32AtomicStore)
        CASE(I64AtomicStore8U, kOneInput, Uint8, Word64AtomicStore)
        CASE(I64AtomicStore16U, kOneInput, Uint16, Word64AtomicStore)
        CASE(I64AtomicStore32U, kOneInput, Uint32, Word64AtomicStore)

#undef CASE

        case wasm::kExprAtomicNotify:
          return {kSpecial, MachineType::Int32(), OperatorByType{nullptr}};
        case wasm::kExprI32AtomicWait:
          return {kSpecial, MachineType::Int32(), OperatorByType{nullptr}};
        case wasm::kExprI64AtomicWait:
          return {kSpecial, MachineType::Int64(), OperatorByType{nullptr}};
        default:
#if V8_HAS_CXX14_CONSTEXPR
          UNREACHABLE();
#else
          // Return something for older GCC.
          return {kSpecial, MachineType::Int64(), OperatorByType{nullptr}};
#endif
      }
    }
  };

  AtomicOpInfo info = AtomicOpInfo::Get(opcode);

  Node* index = CheckBoundsAndAlignment(info.machine_type.MemSize(), inputs[0],
                                        offset, position);

  // {offset} is validated to be within uintptr_t range in {BoundsCheckMem}.
  uintptr_t capped_offset = static_cast<uintptr_t>(offset);
  if (info.type != AtomicOpInfo::kSpecial) {
    const Operator* op =
        info.operator_by_type
            ? (mcgraph()->machine()->*info.operator_by_type)(info.machine_type)
            : (mcgraph()->machine()->*info.operator_by_rep)(
                  info.machine_type.representation());

    Node* input_nodes[6] = {MemBuffer(capped_offset), index};
    int num_actual_inputs = info.type;
    std::copy_n(inputs + 1, num_actual_inputs, input_nodes + 2);
    input_nodes[num_actual_inputs + 2] = effect();
    input_nodes[num_actual_inputs + 3] = control();
    return gasm_->AddNode(
        graph()->NewNode(op, num_actual_inputs + 4, input_nodes));
  }

  // After we've bounds-checked, compute the effective offset.
  Node* effective_offset =
      gasm_->IntAdd(gasm_->UintPtrConstant(capped_offset), index);

  switch (opcode) {
    case wasm::kExprAtomicNotify:
      return gasm_->CallRuntimeStub(wasm::WasmCode::kWasmAtomicNotify,
                                    effective_offset, inputs[1]);

    case wasm::kExprI32AtomicWait: {
      auto* call_descriptor = GetI32AtomicWaitCallDescriptor();

      intptr_t target = mcgraph()->machine()->Is64()
                            ? wasm::WasmCode::kWasmI32AtomicWait64
                            : wasm::WasmCode::kWasmI32AtomicWait32;
      Node* call_target = mcgraph()->RelocatableIntPtrConstant(
          target, RelocInfo::WASM_STUB_CALL);

      return gasm_->Call(call_descriptor, call_target, effective_offset,
                         inputs[1], inputs[2]);
    }

    case wasm::kExprI64AtomicWait: {
      auto* call_descriptor = GetI64AtomicWaitCallDescriptor();

      intptr_t target = mcgraph()->machine()->Is64()
                            ? wasm::WasmCode::kWasmI64AtomicWait64
                            : wasm::WasmCode::kWasmI64AtomicWait32;
      Node* call_target = mcgraph()->RelocatableIntPtrConstant(
          target, RelocInfo::WASM_STUB_CALL);

      return gasm_->Call(call_descriptor, call_target, effective_offset,
                         inputs[1], inputs[2]);
    }

    default:
      FATAL_UNSUPPORTED_OPCODE(opcode);
  }
}

void WasmGraphBuilder::AtomicFence() {
  SetEffect(graph()->NewNode(mcgraph()->machine()->MemBarrier(), effect(),
                             control()));
}

void WasmGraphBuilder::MemoryInit(uint32_t data_segment_index, Node* dst,
                                  Node* src, Node* size,
                                  wasm::WasmCodePosition position) {
  // The data segment index must be in bounds since it is required by
  // validation.
  DCHECK_LT(data_segment_index, env_->module->num_declared_data_segments);

  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_memory_init());

  Node* stack_slot = StoreArgsInStackSlot(
      {{MachineType::PointerRepresentation(), GetInstance()},
       {MachineRepresentation::kWord32, dst},
       {MachineRepresentation::kWord32, src},
       {MachineRepresentation::kWord32,
        gasm_->Uint32Constant(data_segment_index)},
       {MachineRepresentation::kWord32, size}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);
  Node* call = BuildCCall(&sig, function, stack_slot);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, call, position);
}

void WasmGraphBuilder::DataDrop(uint32_t data_segment_index,
                                wasm::WasmCodePosition position) {
  DCHECK_LT(data_segment_index, env_->module->num_declared_data_segments);

  Node* seg_size_array =
      LOAD_INSTANCE_FIELD(DataSegmentSizes, MachineType::Pointer());
  STATIC_ASSERT(wasm::kV8MaxWasmDataSegments <= kMaxUInt32 >> 2);
  auto access = ObjectAccess(MachineType::Int32(), kNoWriteBarrier);
  gasm_->StoreToObject(access, seg_size_array, data_segment_index << 2,
                       Int32Constant(0));
}

Node* WasmGraphBuilder::StoreArgsInStackSlot(
    std::initializer_list<std::pair<MachineRepresentation, Node*>> args) {
  int slot_size = 0;
  for (auto arg : args) {
    slot_size += ElementSizeInBytes(arg.first);
  }
  DCHECK_LT(0, slot_size);
  Node* stack_slot =
      graph()->NewNode(mcgraph()->machine()->StackSlot(slot_size));

  int offset = 0;
  for (auto arg : args) {
    MachineRepresentation type = arg.first;
    Node* value = arg.second;
    gasm_->Store(StoreRepresentation(type, kNoWriteBarrier), stack_slot,
                 Int32Constant(offset), value);
    offset += ElementSizeInBytes(type);
  }
  return stack_slot;
}

void WasmGraphBuilder::MemoryCopy(Node* dst, Node* src, Node* size,
                                  wasm::WasmCodePosition position) {
  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_memory_copy());

  Node* stack_slot = StoreArgsInStackSlot(
      {{MachineType::PointerRepresentation(), GetInstance()},
       {MachineRepresentation::kWord32, dst},
       {MachineRepresentation::kWord32, src},
       {MachineRepresentation::kWord32, size}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);
  Node* call = BuildCCall(&sig, function, stack_slot);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, call, position);
}

void WasmGraphBuilder::MemoryFill(Node* dst, Node* value, Node* size,
                                  wasm::WasmCodePosition position) {
  Node* function =
      gasm_->ExternalConstant(ExternalReference::wasm_memory_fill());

  Node* stack_slot = StoreArgsInStackSlot(
      {{MachineType::PointerRepresentation(), GetInstance()},
       {MachineRepresentation::kWord32, dst},
       {MachineRepresentation::kWord32, value},
       {MachineRepresentation::kWord32, size}});

  MachineType sig_types[] = {MachineType::Int32(), MachineType::Pointer()};
  MachineSignature sig(1, 1, sig_types);
  Node* call = BuildCCall(&sig, function, stack_slot);
  TrapIfFalse(wasm::kTrapMemOutOfBounds, call, position);
}

void WasmGraphBuilder::TableInit(uint32_t table_index,
                                 uint32_t elem_segment_index, Node* dst,
                                 Node* src, Node* size,
                                 wasm::WasmCodePosition position) {
  gasm_->CallRuntimeStub(wasm::WasmCode::kWasmTableInit, dst, src, size,
                         gasm_->NumberConstant(table_index),
                         gasm_->NumberConstant(elem_segment_index));
}

void WasmGraphBuilder::ElemDrop(uint32_t elem_segment_index,
                                wasm::WasmCodePosition position) {
  // The elem segment index must be in bounds since it is required by
  // validation.
  DCHECK_LT(elem_segment_index, env_->module->elem_segments.size());

  Node* dropped_elem_segments =
      LOAD_INSTANCE_FIELD(DroppedElemSegments, MachineType::Pointer());
  auto store_rep =
      StoreRepresentation(MachineRepresentation::kWord8, kNoWriteBarrier);
  gasm_->Store(store_rep, dropped_elem_segments, elem_segment_index,
               Int32Constant(1));
}

void WasmGraphBuilder::TableCopy(uint32_t table_dst_index,
                                 uint32_t table_src_index, Node* dst, Node* src,
                                 Node* size, wasm::WasmCodePosition position) {
  gasm_->CallRuntimeStub(wasm::WasmCode::kWasmTableCopy, dst, src, size,
                         gasm_->NumberConstant(table_dst_index),
                         gasm_->NumberConstant(table_src_index));
}

Node* WasmGraphBuilder::TableGrow(uint32_t table_index, Node* value,
                                  Node* delta) {
  return BuildChangeSmiToInt32(gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmTableGrow,
      graph()->NewNode(mcgraph()->common()->NumberConstant(table_index)), delta,
      value));
}

Node* WasmGraphBuilder::TableSize(uint32_t table_index) {
  Node* tables = LOAD_INSTANCE_FIELD(Tables, MachineType::TaggedPointer());
  Node* table = gasm_->LoadFixedArrayElementAny(tables, table_index);

  int length_field_size = WasmTableObject::kCurrentLengthOffsetEnd -
                          WasmTableObject::kCurrentLengthOffset + 1;
  Node* length_smi = gasm_->LoadFromObject(
      assert_size(length_field_size, MachineType::TaggedSigned()), table,
      wasm::ObjectAccess::ToTagged(WasmTableObject::kCurrentLengthOffset));

  return BuildChangeSmiToInt32(length_smi);
}

void WasmGraphBuilder::TableFill(uint32_t table_index, Node* start, Node* value,
                                 Node* count) {
  gasm_->CallRuntimeStub(
      wasm::WasmCode::kWasmTableFill,
      graph()->NewNode(mcgraph()->common()->NumberConstant(table_index)), start,
      count, value);
}

Node* WasmGraphBuilder::StructNewWithRtt(uint32_t struct_index,
                                         const wasm::StructType* type,
                                         Node* rtt, Vector<Node*> fields) {
  Node* s = gasm_->CallBuiltin(Builtins::kWasmAllocateStructWithRtt,
                               Operator::kEliminatable, rtt);
  for (uint32_t i = 0; i < type->field_count(); i++) {
    gasm_->StoreStructField(s, type, i, fields[i]);
  }
  return s;
}

Node* WasmGraphBuilder::ArrayNewWithRtt(uint32_t array_index,
                                        const wasm::ArrayType* type,
                                        Node* length, Node* initial_value,
                                        Node* rtt,
                                        wasm::WasmCodePosition position) {
  TrapIfFalse(wasm::kTrapArrayOutOfBounds,
              gasm_->Uint32LessThanOrEqual(
                  length, gasm_->Uint32Constant(wasm::kV8MaxWasmArrayLength)),
              position);
  wasm::ValueType element_type = type->element_type();
  Node* a = gasm_->CallBuiltin(
      Builtins::kWasmAllocateArrayWithRtt, Operator::kEliminatable, rtt, length,
      Int32Constant(element_type.element_size_bytes()));
  auto loop = gasm_->MakeLoopLabel(MachineRepresentation::kWord32);
  auto done = gasm_->MakeLabel();
  Node* start_offset =
      Int32Constant(wasm::ObjectAccess::ToTagged(WasmArray::kHeaderSize));
  Node* element_size = Int32Constant(element_type.element_size_bytes());
  Node* end_offset =
      gasm_->Int32Add(start_offset, gasm_->Int32Mul(element_size, length));
  // Loops need the graph's end to have been set up.
  gasm_->EnsureEnd();
  gasm_->Goto(&loop, start_offset);
  gasm_->Bind(&loop);
  {
    Node* offset = loop.PhiAt(0);
    Node* check = gasm_->Uint32LessThan(offset, end_offset);
    gasm_->GotoIfNot(check, &done);
    gasm_->StoreToObject(ObjectAccessForGCStores(type->element_type()), a,
                         offset, initial_value);
    offset = gasm_->Int32Add(offset, element_size);
    gasm_->Goto(&loop, offset);
  }
  gasm_->Bind(&done);
  return a;
}

Node* WasmGraphBuilder::RttCanon(uint32_t type_index) {
  Node* maps_list =
      LOAD_INSTANCE_FIELD(ManagedObjectMaps, MachineType::TaggedPointer());
  return gasm_->LoadFixedArrayElementPtr(maps_list, type_index);
}

Node* WasmGraphBuilder::RttSub(uint32_t type_index, Node* parent_rtt) {
  return gasm_->CallBuiltin(Builtins::kWasmAllocateRtt, Operator::kEliminatable,
                            Int32Constant(type_index), parent_rtt);
}

WasmGraphBuilder::Callbacks WasmGraphBuilder::TestCallbacks(
    GraphAssemblerLabel<1>* label) {
  return {// succeed_if
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIf(condition, label, hint, Int32Constant(1));
          },
          // fail_if
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIf(condition, label, hint, Int32Constant(0));
          },
          // fail_if_not
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIfNot(condition, label, hint, Int32Constant(0));
          }};
}

WasmGraphBuilder::Callbacks WasmGraphBuilder::CastCallbacks(
    GraphAssemblerLabel<0>* label, wasm::WasmCodePosition position) {
  return {// succeed_if
          [=](Node* condition, BranchHint hint) -> void {
            gasm_->GotoIf(condition, label, hint);
          },
          // fail_if
          [=](Node* condition, BranchHint hint) -> void {
            TrapIfTrue(wasm::kTrapIllegalCast, condition, position);
          },
          // fail_if_not
          [=](Node* condition, BranchHint hint) -> void {
            TrapIfFalse(wasm::kTrapIllegalCast, condition, position);
          }};
}

WasmGraphBuilder::Callbacks WasmGraphBuilder::BranchCallbacks(
    SmallNodeVector& no_match_controls, SmallNodeVector& no_match_effects,
    SmallNodeVector& match_controls, SmallNodeVector& match_effects) {
  return {
      // succeed_if
      [&](Node* condition, BranchHint hint) -> void {
        Node* branch = graph()->NewNode(mcgraph()->common()->Branch(hint),
                                        condition, control());
        match_controls.emplace_back(
            graph()->NewNode(mcgraph()->common()->IfTrue(), branch));
        match_effects.emplace_back(effect());
        SetControl(graph()->NewNode(mcgraph()->common()->IfFalse(), branch));
      },
      // fail_if
      [&](Node* condition, BranchHint hint) -> void {
        Node* branch = graph()->NewNode(mcgraph()->common()->Branch(hint),
                                        condition, control());
        no_match_controls.emplace_back(
            graph()->NewNode(mcgraph()->common()->IfTrue(), branch));
        no_match_effects.emplace_back(effect());
        SetControl(graph()->NewNode(mcgraph()->common()->IfFalse(), branch));
      },
      // fail_if_not
      [&](Node* condition, BranchHint hint) -> void {
        Node* branch = graph()->NewNode(mcgraph()->common()->Branch(hint),
                                        condition, control());
        no_match_controls.emplace_back(
            graph()->NewNode(mcgraph()->common()->IfFalse(), branch));
        no_match_effects.emplace_back(effect());
        SetControl(graph()->NewNode(mcgraph()->common()->IfTrue(), branch));
      }};
}

void WasmGraphBuilder::TypeCheck(
    Node* object, Node* rtt, WasmGraphBuilder::ObjectReferenceKnowledge config,
    bool null_succeeds, Callbacks callbacks) {
  if (config.object_can_be_null) {
    (null_succeeds ? callbacks.succeed_if : callbacks.fail_if)(
        gasm_->WordEqual(object, RefNull()), BranchHint::kFalse);
  }

  Node* map = gasm_->LoadMap(object);

  if (config.reference_kind == kFunction) {
    // Currently, the only way for a function to match an rtt is if its map
    // is equal to that rtt.
    callbacks.fail_if_not(gasm_->TaggedEqual(map, rtt), BranchHint::kTrue);
    return;
  }

  DCHECK(config.reference_kind == kArrayOrStruct);

  callbacks.succeed_if(gasm_->TaggedEqual(map, rtt), BranchHint::kTrue);

  Node* type_info = gasm_->LoadWasmTypeInfo(map);
  Node* supertypes = gasm_->LoadSupertypes(type_info);
  Node* supertypes_length =
      BuildChangeSmiToInt32(gasm_->LoadFixedArrayLengthAsSmi(supertypes));
  Node* rtt_depth =
      config.rtt_depth >= 0
          ? Int32Constant(config.rtt_depth)
          : BuildChangeSmiToInt32(gasm_->LoadFixedArrayLengthAsSmi(
                gasm_->LoadSupertypes(gasm_->LoadWasmTypeInfo(rtt))));
  callbacks.fail_if_not(gasm_->Uint32LessThan(rtt_depth, supertypes_length),
                        BranchHint::kTrue);
  Node* maybe_match = gasm_->LoadFixedArrayElement(
      supertypes, rtt_depth, MachineType::TaggedPointer());

  callbacks.fail_if_not(gasm_->TaggedEqual(maybe_match, rtt),
                        BranchHint::kTrue);
}

void WasmGraphBuilder::DataCheck(Node* object, bool object_can_be_null,
                                 Callbacks callbacks) {
  if (object_can_be_null) {
    callbacks.fail_if(gasm_->WordEqual(object, RefNull()), BranchHint::kFalse);
  }
  callbacks.fail_if(gasm_->IsI31(object), BranchHint::kFalse);
  Node* map = gasm_->LoadMap(object);
  callbacks.fail_if_not(gasm_->IsDataRefMap(map), BranchHint::kTrue);
}

void WasmGraphBuilder::FuncCheck(Node* object, bool object_can_be_null,
                                 Callbacks callbacks) {
  if (object_can_be_null) {
    callbacks.fail_if(gasm_->WordEqual(object, RefNull()), BranchHint::kFalse);
  }
  callbacks.fail_if(gasm_->IsI31(object), BranchHint::kFalse);
  callbacks.fail_if_not(gasm_->HasInstanceType(object, JS_FUNCTION_TYPE),
                        BranchHint::kTrue);
}

void WasmGraphBuilder::BrOnCastAbs(
    Node** match_control, Node** match_effect, Node** no_match_control,
    Node** no_match_effect, std::function<void(Callbacks)> type_checker) {
  SmallNodeVector no_match_controls, no_match_effects, match_controls,
      match_effects;

  type_checker(BranchCallbacks(no_match_controls, no_match_effects,
                               match_controls, match_effects));

  match_controls.emplace_back(control());
  match_effects.emplace_back(effect());

  // Wire up the control/effect nodes.
  unsigned count = static_cast<unsigned>(match_controls.size());
  DCHECK_EQ(match_controls.size(), match_effects.size());
  *match_control = Merge(count, match_controls.data());
  // EffectPhis need their control dependency as an additional input.
  match_effects.emplace_back(*match_control);
  *match_effect = EffectPhi(count, match_effects.data());
  DCHECK_EQ(no_match_controls.size(), no_match_effects.size());
  // Range is 2..4, so casting to unsigned is safe.
  count = static_cast<unsigned>(no_match_controls.size());
  *no_match_control = Merge(count, no_match_controls.data());
  // EffectPhis need their control dependency as an additional input.
  no_match_effects.emplace_back(*no_match_control);
  *no_match_effect = EffectPhi(count, no_match_effects.data());
}

Node* WasmGraphBuilder::RefTest(Node* object, Node* rtt,
                                ObjectReferenceKnowledge config) {
  auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);
  TypeCheck(object, rtt, config, false, TestCallbacks(&done));
  gasm_->Goto(&done, Int32Constant(1));
  gasm_->Bind(&done);
  return done.PhiAt(0);
}

Node* WasmGraphBuilder::RefCast(Node* object, Node* rtt,
                                ObjectReferenceKnowledge config,
                                wasm::WasmCodePosition position) {
  auto done = gasm_->MakeLabel();
  TypeCheck(object, rtt, config, true, CastCallbacks(&done, position));
  gasm_->Goto(&done);
  gasm_->Bind(&done);
  return object;
}

void WasmGraphBuilder::BrOnCast(Node* object, Node* rtt,
                                ObjectReferenceKnowledge config,
                                Node** match_control, Node** match_effect,
                                Node** no_match_control,
                                Node** no_match_effect) {
  BrOnCastAbs(match_control, match_effect, no_match_control, no_match_effect,
              [=](Callbacks callbacks) -> void {
                return TypeCheck(object, rtt, config, false, callbacks);
              });
}

Node* WasmGraphBuilder::RefIsData(Node* object, bool object_can_be_null) {
  auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);
  DataCheck(object, object_can_be_null, TestCallbacks(&done));
  gasm_->Goto(&done, Int32Constant(1));
  gasm_->Bind(&done);
  return done.PhiAt(0);
}

Node* WasmGraphBuilder::RefAsData(Node* object, bool object_can_be_null,
                                  wasm::WasmCodePosition position) {
  auto done = gasm_->MakeLabel();
  DataCheck(object, object_can_be_null, CastCallbacks(&done, position));
  gasm_->Goto(&done);
  gasm_->Bind(&done);
  return object;
}

void WasmGraphBuilder::BrOnData(Node* object, Node* /*rtt*/,
                                ObjectReferenceKnowledge config,
                                Node** match_control, Node** match_effect,
                                Node** no_match_control,
                                Node** no_match_effect) {
  BrOnCastAbs(match_control, match_effect, no_match_control, no_match_effect,
              [=](Callbacks callbacks) -> void {
                return DataCheck(object, config.object_can_be_null, callbacks);
              });
}

Node* WasmGraphBuilder::RefIsFunc(Node* object, bool object_can_be_null) {
  auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);
  FuncCheck(object, object_can_be_null, TestCallbacks(&done));
  gasm_->Goto(&done, Int32Constant(1));
  gasm_->Bind(&done);
  return done.PhiAt(0);
}

Node* WasmGraphBuilder::RefAsFunc(Node* object, bool object_can_be_null,
                                  wasm::WasmCodePosition position) {
  auto done = gasm_->MakeLabel();
  FuncCheck(object, object_can_be_null, CastCallbacks(&done, position));
  gasm_->Goto(&done);
  gasm_->Bind(&done);
  return object;
}

void WasmGraphBuilder::BrOnFunc(Node* object, Node* /*rtt*/,
                                ObjectReferenceKnowledge config,
                                Node** match_control, Node** match_effect,
                                Node** no_match_control,
                                Node** no_match_effect) {
  BrOnCastAbs(match_control, match_effect, no_match_control, no_match_effect,
              [=](Callbacks callbacks) -> void {
                return FuncCheck(object, config.object_can_be_null, callbacks);
              });
}

Node* WasmGraphBuilder::RefIsI31(Node* object) { return gasm_->IsI31(object); }

Node* WasmGraphBuilder::RefAsI31(Node* object,
                                 wasm::WasmCodePosition position) {
  TrapIfFalse(wasm::kTrapIllegalCast, gasm_->IsI31(object), position);
  return object;
}

void WasmGraphBuilder::BrOnI31(Node* object, Node* /* rtt */,
                               ObjectReferenceKnowledge /* config */,
                               Node** match_control, Node** match_effect,
                               Node** no_match_control,
                               Node** no_match_effect) {
  gasm_->Branch(gasm_->IsI31(object), match_control, no_match_control,
                BranchHint::kTrue);

  SetControl(*no_match_control);
  *match_effect = effect();
  *no_match_effect = effect();
}

Node* WasmGraphBuilder::StructGet(Node* struct_object,
                                  const wasm::StructType* struct_type,
                                  uint32_t field_index, CheckForNull null_check,
                                  bool is_signed,
                                  wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    TrapIfTrue(wasm::kTrapNullDereference,
               gasm_->WordEqual(struct_object, RefNull()), position);
  }
  // It is not enough to invoke ValueType::machine_type(), because the
  // signedness has to be determined by {is_signed}.
  MachineType machine_type = MachineType::TypeForRepresentation(
      struct_type->field(field_index).machine_representation(), is_signed);
  Node* offset = gasm_->FieldOffset(struct_type, field_index);
  return gasm_->LoadFromObject(machine_type, struct_object, offset);
}

void WasmGraphBuilder::StructSet(Node* struct_object,
                                 const wasm::StructType* struct_type,
                                 uint32_t field_index, Node* field_value,
                                 CheckForNull null_check,
                                 wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    TrapIfTrue(wasm::kTrapNullDereference,
               gasm_->WordEqual(struct_object, RefNull()), position);
  }
  gasm_->StoreStructField(struct_object, struct_type, field_index, field_value);
}

void WasmGraphBuilder::BoundsCheck(Node* array, Node* index,
                                   wasm::WasmCodePosition position) {
  Node* length = gasm_->LoadWasmArrayLength(array);
  TrapIfFalse(wasm::kTrapArrayOutOfBounds, gasm_->Uint32LessThan(index, length),
              position);
}

Node* WasmGraphBuilder::ArrayGet(Node* array_object,
                                 const wasm::ArrayType* type, Node* index,
                                 CheckForNull null_check, bool is_signed,
                                 wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    TrapIfTrue(wasm::kTrapNullDereference,
               gasm_->WordEqual(array_object, RefNull()), position);
  }
  BoundsCheck(array_object, index, position);
  MachineType machine_type = MachineType::TypeForRepresentation(
      type->element_type().machine_representation(), is_signed);
  Node* offset = gasm_->WasmArrayElementOffset(index, type->element_type());
  return gasm_->LoadFromObject(machine_type, array_object, offset);
}

void WasmGraphBuilder::ArraySet(Node* array_object, const wasm::ArrayType* type,
                                Node* index, Node* value,
                                CheckForNull null_check,
                                wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    TrapIfTrue(wasm::kTrapNullDereference,
               gasm_->WordEqual(array_object, RefNull()), position);
  }
  BoundsCheck(array_object, index, position);
  Node* offset = gasm_->WasmArrayElementOffset(index, type->element_type());
  gasm_->StoreToObject(ObjectAccessForGCStores(type->element_type()),
                       array_object, offset, value);
}

Node* WasmGraphBuilder::ArrayLen(Node* array_object, CheckForNull null_check,
                                 wasm::WasmCodePosition position) {
  if (null_check == kWithNullCheck) {
    TrapIfTrue(wasm::kTrapNullDereference,
               gasm_->WordEqual(array_object, RefNull()), position);
  }
  return gasm_->LoadWasmArrayLength(array_object);
}

// 1 bit V8 Smi tag, 31 bits V8 Smi shift, 1 bit i31ref high-bit truncation.
constexpr int kI31To32BitSmiShift = 33;

Node* WasmGraphBuilder::I31New(Node* input) {
  if (SmiValuesAre31Bits()) {
    return gasm_->Word32Shl(input, BuildSmiShiftBitsConstant32());
  }
  DCHECK(SmiValuesAre32Bits());
  input = BuildChangeInt32ToIntPtr(input);
  return gasm_->WordShl(input, gasm_->IntPtrConstant(kI31To32BitSmiShift));
}

Node* WasmGraphBuilder::I31GetS(Node* input) {
  if (SmiValuesAre31Bits()) {
    input = BuildTruncateIntPtrToInt32(input);
    return gasm_->Word32SarShiftOutZeros(input, BuildSmiShiftBitsConstant32());
  }
  DCHECK(SmiValuesAre32Bits());
  return BuildTruncateIntPtrToInt32(
      gasm_->WordSar(input, gasm_->IntPtrConstant(kI31To32BitSmiShift)));
}

Node* WasmGraphBuilder::I31GetU(Node* input) {
  if (SmiValuesAre31Bits()) {
    input = BuildTruncateIntPtrToInt32(input);
    return gasm_->Word32Shr(input, BuildSmiShiftBitsConstant32());
  }
  DCHECK(SmiValuesAre32Bits());
  return BuildTruncateIntPtrToInt32(
      gasm_->WordShr(input, gasm_->IntPtrConstant(kI31To32BitSmiShift)));
}

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
  decorator_ = graph()->zone()->New<WasmDecorator>(node_origins, decoder);
  graph()->AddDecorator(decorator_);
}

void WasmGraphBuilder::RemoveBytecodePositionDecorator() {
  DCHECK_NOT_NULL(decorator_);
  graph()->RemoveDecorator(decorator_);
  decorator_ = nullptr;
}

namespace {

// A non-null {isolate} signifies that the generated code is treated as being in
// a JS frame for functions like BuildIsolateRoot().
class WasmWrapperGraphBuilder : public WasmGraphBuilder {
 public:
  WasmWrapperGraphBuilder(Zone* zone, MachineGraph* mcgraph,
                          const wasm::FunctionSig* sig,
                          const wasm::WasmModule* module, Isolate* isolate,
                          compiler::SourcePositionTable* spt,
                          StubCallMode stub_mode, wasm::WasmFeatures features)
      : WasmGraphBuilder(nullptr, zone, mcgraph, sig, spt, isolate),
        module_(module),
        stub_mode_(stub_mode),
        enabled_features_(features) {}

  CallDescriptor* GetI64ToBigIntCallDescriptor() {
    if (i64_to_bigint_descriptor_) return i64_to_bigint_descriptor_;

    i64_to_bigint_descriptor_ =
        GetBuiltinCallDescriptor(Builtins::kI64ToBigInt, zone_, stub_mode_);

    AddInt64LoweringReplacement(
        i64_to_bigint_descriptor_,
        GetBuiltinCallDescriptor(Builtins::kI32PairToBigInt, zone_,
                                 stub_mode_));
    return i64_to_bigint_descriptor_;
  }

  CallDescriptor* GetBigIntToI64CallDescriptor(bool needs_frame_state) {
    if (bigint_to_i64_descriptor_) return bigint_to_i64_descriptor_;

    bigint_to_i64_descriptor_ = GetBuiltinCallDescriptor(
        Builtins::kBigIntToI64, zone_, stub_mode_, needs_frame_state);

    AddInt64LoweringReplacement(
        bigint_to_i64_descriptor_,
        GetBuiltinCallDescriptor(Builtins::kBigIntToI32Pair, zone_,
                                 stub_mode_));
    return bigint_to_i64_descriptor_;
  }

  Node* GetTargetForBuiltinCall(wasm::WasmCode::RuntimeStubId wasm_stub,
                                Builtins::Name builtin_id) {
    return (stub_mode_ == StubCallMode::kCallWasmRuntimeStub)
               ? mcgraph()->RelocatableIntPtrConstant(wasm_stub,
                                                      RelocInfo::WASM_STUB_CALL)
               : gasm_->GetBuiltinPointerTarget(builtin_id);
  }

  Node* UndefinedValue() { return LOAD_ROOT(UndefinedValue, undefined_value); }

  Node* BuildChangeInt32ToNumber(Node* value) {
    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    if (SmiValuesAre32Bits()) {
      return BuildChangeInt32ToSmi(value);
    }
    DCHECK(SmiValuesAre31Bits());

    auto builtin = gasm_->MakeDeferredLabel();
    auto done = gasm_->MakeLabel(MachineRepresentation::kTagged);

    // Double value to test if value can be a Smi, and if so, to convert it.
    Node* add = gasm_->Int32AddWithOverflow(value, value);
    Node* ovf = gasm_->Projection(1, add);
    gasm_->GotoIf(ovf, &builtin);

    // If it didn't overflow, the result is {2 * value} as pointer-sized value.
    Node* smi_tagged = BuildChangeInt32ToIntPtr(gasm_->Projection(0, add));
    gasm_->Goto(&done, smi_tagged);

    // Otherwise, call builtin, to convert to a HeapNumber.
    gasm_->Bind(&builtin);
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target =
        GetTargetForBuiltinCall(wasm::WasmCode::kWasmInt32ToHeapNumber,
                                Builtins::kWasmInt32ToHeapNumber);
    if (!int32_to_heapnumber_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmInt32ToHeapNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
      int32_to_heapnumber_operator_.set(common->Call(call_descriptor));
    }
    Node* call =
        gasm_->Call(int32_to_heapnumber_operator_.get(), target, value);
    gasm_->Goto(&done, call);
    gasm_->Bind(&done);
    return done.PhiAt(0);
  }

  Node* BuildChangeTaggedToInt32(Node* value, Node* context,
                                 Node* frame_state) {
    // We expect most integers at runtime to be Smis, so it is important for
    // wrapper performance that Smi conversion be inlined.
    auto builtin = gasm_->MakeDeferredLabel();
    auto done = gasm_->MakeLabel(MachineRepresentation::kWord32);

    gasm_->GotoIfNot(IsSmi(value), &builtin);

    // If Smi, convert to int32.
    Node* smi = BuildChangeSmiToInt32(value);
    gasm_->Goto(&done, smi);

    // Otherwise, call builtin which changes non-Smi to Int32.
    gasm_->Bind(&builtin);
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target =
        GetTargetForBuiltinCall(wasm::WasmCode::kWasmTaggedNonSmiToInt32,
                                Builtins::kWasmTaggedNonSmiToInt32);
    if (!tagged_non_smi_to_int32_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmTaggedNonSmiToInt32Descriptor(), 0,
          frame_state ? CallDescriptor::kNeedsFrameState
                      : CallDescriptor::kNoFlags,
          Operator::kNoProperties, stub_mode_);
      tagged_non_smi_to_int32_operator_.set(common->Call(call_descriptor));
    }
    Node* call = frame_state
                     ? gasm_->Call(tagged_non_smi_to_int32_operator_.get(),
                                   target, value, context, frame_state)
                     : gasm_->Call(tagged_non_smi_to_int32_operator_.get(),
                                   target, value, context);
    SetSourcePosition(call, 1);
    gasm_->Goto(&done, call);
    gasm_->Bind(&done);
    return done.PhiAt(0);
  }

  Node* BuildChangeFloat32ToNumber(Node* value) {
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = GetTargetForBuiltinCall(wasm::WasmCode::kWasmFloat32ToNumber,
                                           Builtins::kWasmFloat32ToNumber);
    if (!float32_to_number_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmFloat32ToNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
      float32_to_number_operator_.set(common->Call(call_descriptor));
    }
    return gasm_->Call(float32_to_number_operator_.get(), target, value);
  }

  Node* BuildChangeFloat64ToNumber(Node* value) {
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = GetTargetForBuiltinCall(wasm::WasmCode::kWasmFloat64ToNumber,
                                           Builtins::kWasmFloat64ToNumber);
    if (!float64_to_number_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmFloat64ToNumberDescriptor(), 0,
          CallDescriptor::kNoFlags, Operator::kNoProperties, stub_mode_);
      float64_to_number_operator_.set(common->Call(call_descriptor));
    }
    return gasm_->Call(float64_to_number_operator_.get(), target, value);
  }

  Node* BuildChangeTaggedToFloat64(Node* value, Node* context,
                                   Node* frame_state) {
    CommonOperatorBuilder* common = mcgraph()->common();
    Node* target = GetTargetForBuiltinCall(wasm::WasmCode::kWasmTaggedToFloat64,
                                           Builtins::kWasmTaggedToFloat64);
    bool needs_frame_state = frame_state != nullptr;
    if (!tagged_to_float64_operator_.is_set()) {
      auto call_descriptor = Linkage::GetStubCallDescriptor(
          mcgraph()->zone(), WasmTaggedToFloat64Descriptor(), 0,
          frame_state ? CallDescriptor::kNeedsFrameState
                      : CallDescriptor::kNoFlags,
          Operator::kNoProperties, stub_mode_);
      tagged_to_float64_operator_.set(common->Call(call_descriptor));
    }
    Node* call = needs_frame_state
                     ? gasm_->Call(tagged_to_float64_operator_.get(), target,
                                   value, context, frame_state)
                     : gasm_->Call(tagged_to_float64_operator_.get(), target,
                                   value, context);
    SetSourcePosition(call, 1);
    return call;
  }

  int AddArgumentNodes(Vector<Node*> args, int pos, int param_count,
                       const wasm::FunctionSig* sig) {
    // Convert wasm numbers to JS values.
    for (int i = 0; i < param_count; ++i) {
      Node* param =
          Param(i + 1);  // Start from index 1 to drop the instance_node.
      args[pos++] = ToJS(param, sig->GetParam(i));
    }
    return pos;
  }

  Node* ToJS(Node* node, wasm::ValueType type) {
    switch (type.kind()) {
      case wasm::kI32:
        return BuildChangeInt32ToNumber(node);
      case wasm::kI64:
        return BuildChangeInt64ToBigInt(node);
      case wasm::kF32:
        return BuildChangeFloat32ToNumber(node);
      case wasm::kF64:
        return BuildChangeFloat64ToNumber(node);
      case wasm::kRef:
      case wasm::kOptRef:
        switch (type.heap_representation()) {
          case wasm::HeapType::kExtern:
          case wasm::HeapType::kFunc:
            return node;
          case wasm::HeapType::kData:
          case wasm::HeapType::kEq:
          case wasm::HeapType::kI31:
            // TODO(7748): Update this when JS interop is settled.
            if (type.kind() == wasm::kOptRef) {
              auto done =
                  gasm_->MakeLabel(MachineRepresentation::kTaggedPointer);
              // Do not wrap {null}.
              gasm_->GotoIf(gasm_->WordEqual(node, RefNull()), &done, node);
              gasm_->Goto(&done, BuildAllocateObjectWrapper(node));
              gasm_->Bind(&done);
              return done.PhiAt(0);
            } else {
              return BuildAllocateObjectWrapper(node);
            }
          case wasm::HeapType::kAny: {
            // Only wrap {node} if it is an array/struct/i31, i.e., do not wrap
            // functions and null.
            // TODO(7748): Update this when JS interop is settled.
            auto done = gasm_->MakeLabel(MachineRepresentation::kTaggedPointer);
            gasm_->GotoIf(IsSmi(node), &done, BuildAllocateObjectWrapper(node));
            // This includes the case where {node == null}.
            gasm_->GotoIfNot(gasm_->IsDataRefMap(gasm_->LoadMap(node)), &done,
                             node);
            gasm_->Goto(&done, BuildAllocateObjectWrapper(node));
            gasm_->Bind(&done);
            return done.PhiAt(0);
          }
          default:
            DCHECK(type.has_index());
            if (module_->has_signature(type.ref_index())) {
              // Typed function
              return node;
            }
            // If this is reached, then IsJSCompatibleSignature() is too
            // permissive.
            // TODO(7748): Figure out a JS interop story for arrays and structs.
            UNREACHABLE();
        }
      case wasm::kRtt:
      case wasm::kRttWithDepth:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kS128:
      case wasm::kVoid:
      case wasm::kBottom:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        // TODO(7748): Figure out what to do for RTTs.
        UNREACHABLE();
    }
  }

  // TODO(7748): Temporary solution to allow round-tripping of Wasm objects
  // through JavaScript, where they show up as opaque boxes. This will disappear
  // once we have a proper WasmGC <-> JS interaction story.
  Node* BuildAllocateObjectWrapper(Node* input) {
    return gasm_->CallBuiltin(
        Builtins::kWasmAllocateObjectWrapper, Operator::kEliminatable, input,
        LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
  }

  // Assumes {input} has been checked for validity against the target wasm type.
  // Returns the value of the property associated with
  // {wasm_wrapped_object_symbol} in {input}, or {input} itself if the property
  // is not found.
  Node* BuildUnpackObjectWrapper(Node* input) {
    Node* obj = gasm_->CallBuiltin(
        Builtins::kWasmGetOwnProperty, Operator::kEliminatable, input,
        LOAD_ROOT(wasm_wrapped_object_symbol, wasm_wrapped_object_symbol),
        LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer()));
    // Invalid object wrappers (i.e. any other JS object that doesn't have the
    // magic hidden property) will return {undefined}. Map that to {null} or
    // {input}, depending on the value of {failure}.
    Node* undefined = UndefinedValue();
    Node* is_undefined = gasm_->WordEqual(obj, undefined);
    Diamond check(graph(), mcgraph()->common(), is_undefined,
                  BranchHint::kFalse);
    check.Chain(control());
    return check.Phi(MachineRepresentation::kTagged, input, obj);
  }

  Node* BuildChangeInt64ToBigInt(Node* input) {
    Node* target;
    if (mcgraph()->machine()->Is64()) {
      target = GetTargetForBuiltinCall(wasm::WasmCode::kI64ToBigInt,
                                       Builtins::kI64ToBigInt);
    } else {
      DCHECK(mcgraph()->machine()->Is32());
      // On 32-bit platforms we already set the target to the
      // I32PairToBigInt builtin here, so that we don't have to replace the
      // target in the int64-lowering.
      target = GetTargetForBuiltinCall(wasm::WasmCode::kI32PairToBigInt,
                                       Builtins::kI32PairToBigInt);
    }
    return gasm_->Call(GetI64ToBigIntCallDescriptor(), target, input);
  }

  Node* BuildChangeBigIntToInt64(Node* input, Node* context,
                                 Node* frame_state) {
    Node* target;
    if (mcgraph()->machine()->Is64()) {
      target = GetTargetForBuiltinCall(wasm::WasmCode::kBigIntToI64,
                                       Builtins::kBigIntToI64);
    } else {
      DCHECK(mcgraph()->machine()->Is32());
      // On 32-bit platforms we already set the target to the
      // BigIntToI32Pair builtin here, so that we don't have to replace the
      // target in the int64-lowering.
      target = GetTargetForBuiltinCall(wasm::WasmCode::kBigIntToI32Pair,
                                       Builtins::kBigIntToI32Pair);
    }

    return frame_state ? gasm_->Call(GetBigIntToI64CallDescriptor(true), target,
                                     input, context, frame_state)
                       : gasm_->Call(GetBigIntToI64CallDescriptor(false),
                                     target, input, context);
  }

  void BuildCheckValidRefValue(Node* input, Node* js_context,
                               wasm::ValueType type) {
    // Make sure ValueType fits in a Smi.
    STATIC_ASSERT(wasm::ValueType::kLastUsedBit + 1 <= kSmiValueSize);
    Node* inputs[] = {GetInstance(), input,
                      mcgraph()->IntPtrConstant(
                          IntToSmi(static_cast<int>(type.raw_bit_field())))};

    Node* check = BuildChangeSmiToInt32(BuildCallToRuntimeWithContext(
        Runtime::kWasmIsValidRefValue, js_context, inputs, 3));

    Diamond type_check(graph(), mcgraph()->common(), check, BranchHint::kTrue);
    type_check.Chain(control());
    SetControl(type_check.if_false);

    Node* old_effect = effect();
    BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, js_context,
                                  nullptr, 0);

    SetEffectControl(type_check.EffectPhi(old_effect, effect()),
                     type_check.merge);
  }

  Node* FromJS(Node* input, Node* js_context, wasm::ValueType type,
               Node* frame_state = nullptr) {
    switch (type.kind()) {
      case wasm::kRef:
      case wasm::kOptRef: {
        switch (type.heap_representation()) {
          case wasm::HeapType::kExtern:
            return input;
          case wasm::HeapType::kAny:
            // If this is a wrapper for arrays/structs/i31s, unpack it.
            // TODO(7748): Update this when JS interop has settled.
            return BuildUnpackObjectWrapper(input);
          case wasm::HeapType::kFunc:
            BuildCheckValidRefValue(input, js_context, type);
            return input;
          case wasm::HeapType::kData:
          case wasm::HeapType::kEq:
          case wasm::HeapType::kI31:
            // TODO(7748): Update this when JS interop has settled.
            BuildCheckValidRefValue(input, js_context, type);
            // This will just return {input} if the object is not wrapped, i.e.
            // if it is null (given the check just above).
            return BuildUnpackObjectWrapper(input);
          default:
            if (module_->has_signature(type.ref_index())) {
              BuildCheckValidRefValue(input, js_context, type);
              return input;
            }
            // If this is reached, then IsJSCompatibleSignature() is too
            // permissive.
            UNREACHABLE();
        }
      }
      case wasm::kF32:
        return gasm_->TruncateFloat64ToFloat32(
            BuildChangeTaggedToFloat64(input, js_context, frame_state));

      case wasm::kF64:
        return BuildChangeTaggedToFloat64(input, js_context, frame_state);

      case wasm::kI32:
        return BuildChangeTaggedToInt32(input, js_context, frame_state);

      case wasm::kI64:
        // i64 values can only come from BigInt.
        return BuildChangeBigIntToInt64(input, js_context, frame_state);

      case wasm::kRtt:
      case wasm::kRttWithDepth:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        // If this is reached, then IsJSCompatibleSignature() is too permissive.
        // TODO(7748): Figure out what to do for RTTs.
        UNREACHABLE();
    }
  }

  Node* SmiToFloat32(Node* input) {
    return gasm_->RoundInt32ToFloat32(BuildChangeSmiToInt32(input));
  }

  Node* SmiToFloat64(Node* input) {
    return gasm_->ChangeInt32ToFloat64(BuildChangeSmiToInt32(input));
  }

  Node* HeapNumberToFloat64(Node* input) {
    return gasm_->LoadFromObject(
        MachineType::Float64(), input,
        wasm::ObjectAccess::ToTagged(HeapNumber::kValueOffset));
  }

  Node* FromJSFast(Node* input, wasm::ValueType type) {
    switch (type.kind()) {
      case wasm::kI32:
        return BuildChangeSmiToInt32(input);
      case wasm::kF32: {
        auto done = gasm_->MakeLabel(MachineRepresentation::kFloat32);
        auto heap_number = gasm_->MakeLabel();
        gasm_->GotoIfNot(IsSmi(input), &heap_number);
        gasm_->Goto(&done, SmiToFloat32(input));
        gasm_->Bind(&heap_number);
        Node* value =
            gasm_->TruncateFloat64ToFloat32(HeapNumberToFloat64(input));
        gasm_->Goto(&done, value);
        gasm_->Bind(&done);
        return done.PhiAt(0);
      }
      case wasm::kF64: {
        auto done = gasm_->MakeLabel(MachineRepresentation::kFloat64);
        auto heap_number = gasm_->MakeLabel();
        gasm_->GotoIfNot(IsSmi(input), &heap_number);
        gasm_->Goto(&done, SmiToFloat64(input));
        gasm_->Bind(&heap_number);
        gasm_->Goto(&done, HeapNumberToFloat64(input));
        gasm_->Bind(&done);
        return done.PhiAt(0);
      }
      case wasm::kRef:
      case wasm::kOptRef:
      case wasm::kI64:
      case wasm::kRtt:
      case wasm::kRttWithDepth:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        UNREACHABLE();
        break;
    }
  }

  void BuildModifyThreadInWasmFlagHelper(Node* thread_in_wasm_flag_address,
                                         bool new_value) {
    if (FLAG_debug_code) {
      Node* flag_value = gasm_->LoadFromObject(MachineType::Pointer(),
                                               thread_in_wasm_flag_address, 0);
      Node* check =
          gasm_->Word32Equal(flag_value, Int32Constant(new_value ? 0 : 1));

      Diamond flag_check(graph(), mcgraph()->common(), check,
                         BranchHint::kTrue);
      flag_check.Chain(control());
      SetControl(flag_check.if_false);
      Node* message_id = gasm_->NumberConstant(static_cast<int32_t>(
          new_value ? AbortReason::kUnexpectedThreadInWasmSet
                    : AbortReason::kUnexpectedThreadInWasmUnset));

      Node* old_effect = effect();
      Node* call = BuildCallToRuntimeWithContext(
          Runtime::kAbort, NoContextConstant(), &message_id, 1);
      flag_check.merge->ReplaceInput(1, call);
      SetEffectControl(flag_check.EffectPhi(old_effect, effect()),
                       flag_check.merge);
    }

    gasm_->StoreToObject(ObjectAccess(MachineType::Int32(), kNoWriteBarrier),
                         thread_in_wasm_flag_address, 0,
                         Int32Constant(new_value ? 1 : 0));
  }

  void BuildModifyThreadInWasmFlag(bool new_value) {
    if (!trap_handler::IsTrapHandlerEnabled()) return;
    Node* isolate_root = BuildLoadIsolateRoot();

    Node* thread_in_wasm_flag_address =
        gasm_->LoadFromObject(MachineType::Pointer(), isolate_root,
                              Isolate::thread_in_wasm_flag_address_offset());

    BuildModifyThreadInWasmFlagHelper(thread_in_wasm_flag_address, new_value);
  }

  class ModifyThreadInWasmFlagScope {
   public:
    ModifyThreadInWasmFlagScope(
        WasmWrapperGraphBuilder* wasm_wrapper_graph_builder,
        WasmGraphAssembler* gasm)
        : wasm_wrapper_graph_builder_(wasm_wrapper_graph_builder) {
      if (!trap_handler::IsTrapHandlerEnabled()) return;
      Node* isolate_root = wasm_wrapper_graph_builder_->BuildLoadIsolateRoot();

      thread_in_wasm_flag_address_ =
          gasm->LoadFromObject(MachineType::Pointer(), isolate_root,
                               Isolate::thread_in_wasm_flag_address_offset());

      wasm_wrapper_graph_builder_->BuildModifyThreadInWasmFlagHelper(
          thread_in_wasm_flag_address_, true);
    }

    ~ModifyThreadInWasmFlagScope() {
      if (!trap_handler::IsTrapHandlerEnabled()) return;

      wasm_wrapper_graph_builder_->BuildModifyThreadInWasmFlagHelper(
          thread_in_wasm_flag_address_, false);
    }

   private:
    WasmWrapperGraphBuilder* wasm_wrapper_graph_builder_;
    Node* thread_in_wasm_flag_address_;
  };

  Node* BuildMultiReturnFixedArrayFromIterable(const wasm::FunctionSig* sig,
                                               Node* iterable, Node* context) {
    Node* length = BuildChangeUint31ToSmi(
        mcgraph()->Uint32Constant(static_cast<uint32_t>(sig->return_count())));
    return gasm_->CallBuiltin(Builtins::kIterableToFixedArrayForWasm,
                              Operator::kEliminatable, iterable, length,
                              context);
  }

  // Generate a call to the AllocateJSArray builtin.
  Node* BuildCallAllocateJSArray(Node* array_length, Node* context) {
    // Since we don't check that args will fit in an array,
    // we make sure this is true based on statically known limits.
    STATIC_ASSERT(wasm::kV8MaxWasmFunctionMultiReturns <=
                  JSArray::kInitialMaxFastElementArray);
    return gasm_->CallBuiltin(Builtins::kWasmAllocateJSArray,
                              Operator::kEliminatable, array_length, context);
  }

  Node* BuildCallAndReturn(bool is_import, Node* js_context,
                           Node* function_data,
                           base::SmallVector<Node*, 16> args,
                           const JSWasmCallData* js_wasm_call_data,
                           Node* frame_state) {
    const int rets_count = static_cast<int>(sig_->return_count());
    base::SmallVector<Node*, 1> rets(rets_count);

    // Set the ThreadInWasm flag before we do the actual call.
    {
      ModifyThreadInWasmFlagScope modify_thread_in_wasm_flag_builder(
          this, gasm_.get());

      if (is_import) {
        // Call to an imported function.
        // Load function index from {WasmExportedFunctionData}.
        Node* function_index = BuildChangeSmiToInt32(
            gasm_->LoadExportedFunctionIndexAsSmi(function_data));
        BuildImportCall(sig_, VectorOf(args), VectorOf(rets),
                        wasm::kNoCodePosition, function_index, kCallContinues);
      } else {
        // Call to a wasm function defined in this module.
        // The call target is the jump table slot for that function.
        Node* jump_table_start =
            LOAD_INSTANCE_FIELD(JumpTableStart, MachineType::Pointer());
        Node* jump_table_offset =
            BuildLoadJumpTableOffsetFromExportedFunctionData(function_data);
        Node* jump_table_slot =
            gasm_->IntAdd(jump_table_start, jump_table_offset);
        args[0] = jump_table_slot;

        BuildWasmCall(sig_, VectorOf(args), VectorOf(rets),
                      wasm::kNoCodePosition, nullptr, kNoRetpoline,
                      frame_state);
      }
    }

    Node* jsval;
    if (sig_->return_count() == 0) {
      jsval = UndefinedValue();
    } else if (sig_->return_count() == 1) {
      jsval = js_wasm_call_data && !js_wasm_call_data->result_needs_conversion()
                  ? rets[0]
                  : ToJS(rets[0], sig_->GetReturn());
    } else {
      int32_t return_count = static_cast<int32_t>(sig_->return_count());
      Node* size = gasm_->NumberConstant(return_count);

      jsval = BuildCallAllocateJSArray(size, js_context);

      Node* fixed_array = gasm_->LoadJSArrayElements(jsval);

      for (int i = 0; i < return_count; ++i) {
        Node* value = ToJS(rets[i], sig_->GetReturn(i));
        gasm_->StoreFixedArrayElementAny(fixed_array, i, value);
      }
    }
    return jsval;
  }

  bool QualifiesForFastTransform(const wasm::FunctionSig*) {
    const int wasm_count = static_cast<int>(sig_->parameter_count());
    for (int i = 0; i < wasm_count; ++i) {
      wasm::ValueType type = sig_->GetParam(i);
      switch (type.kind()) {
        case wasm::kRef:
        case wasm::kOptRef:
        case wasm::kI64:
        case wasm::kRtt:
        case wasm::kRttWithDepth:
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

  Node* IsSmi(Node* input) {
    return gasm_->Word32Equal(
        gasm_->Word32And(BuildTruncateIntPtrToInt32(input),
                         Int32Constant(kSmiTagMask)),
        Int32Constant(kSmiTag));
  }

  void CanTransformFast(
      Node* input, wasm::ValueType type,
      v8::internal::compiler::GraphAssemblerLabel<0>* slow_path) {
    switch (type.kind()) {
      case wasm::kI32: {
        gasm_->GotoIfNot(IsSmi(input), slow_path);
        return;
      }
      case wasm::kF32:
      case wasm::kF64: {
        auto done = gasm_->MakeLabel();
        gasm_->GotoIf(IsSmi(input), &done);
        Node* map = gasm_->LoadFromObject(
            MachineType::TaggedPointer(), input,
            wasm::ObjectAccess::ToTagged(HeapObject::kMapOffset));
        Node* heap_number_map = LOAD_ROOT(HeapNumberMap, heap_number_map);
        Node* is_heap_number = gasm_->WordEqual(heap_number_map, map);
        gasm_->GotoIf(is_heap_number, &done);
        gasm_->Goto(slow_path);
        gasm_->Bind(&done);
        return;
      }
      case wasm::kRef:
      case wasm::kOptRef:
      case wasm::kI64:
      case wasm::kRtt:
      case wasm::kRttWithDepth:
      case wasm::kS128:
      case wasm::kI8:
      case wasm::kI16:
      case wasm::kBottom:
      case wasm::kVoid:
        UNREACHABLE();
        break;
    }
  }

  void BuildJSToWasmWrapper(bool is_import,
                            const JSWasmCallData* js_wasm_call_data = nullptr,
                            Node* frame_state = nullptr) {
    const int wasm_param_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the JS parameter nodes.
    Start(wasm_param_count + 5);

    // Create the js_closure and js_context parameters.
    Node* js_closure = Param(Linkage::kJSCallClosureParamIndex, "%closure");
    Node* js_context = Param(
        Linkage::GetJSCallContextParamIndex(wasm_param_count + 1), "%context");
    Node* function_data = gasm_->LoadFunctionDataFromJSFunction(js_closure);

    if (!wasm::IsJSCompatibleSignature(sig_, module_, enabled_features_)) {
      // Throw a TypeError. Use the js_context of the calling javascript
      // function (passed as a parameter), such that the generated code is
      // js_context independent.
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, js_context,
                                    nullptr, 0);
      TerminateThrow(effect(), control());
      return;
    }

    const int args_count = wasm_param_count + 1;  // +1 for wasm_code.

    // Check whether the signature of the function allows for a fast
    // transformation (if any params exist that need transformation).
    // Create a fast transformation path, only if it does.
    bool include_fast_path = !js_wasm_call_data && wasm_param_count > 0 &&
                             QualifiesForFastTransform(sig_);

    // Prepare Param() nodes. Param() nodes can only be created once,
    // so we need to use the same nodes along all possible transformation paths.
    base::SmallVector<Node*, 16> params(args_count);
    for (int i = 0; i < wasm_param_count; ++i) params[i + 1] = Param(i + 1);

    auto done = gasm_->MakeLabel(MachineRepresentation::kTagged);
    if (include_fast_path) {
      auto slow_path = gasm_->MakeDeferredLabel();
      // Check if the params received on runtime can be actually transformed
      // using the fast transformation. When a param that cannot be transformed
      // fast is encountered, skip checking the rest and fall back to the slow
      // path.
      for (int i = 0; i < wasm_param_count; ++i) {
        CanTransformFast(params[i + 1], sig_->GetParam(i), &slow_path);
      }
      // Convert JS parameters to wasm numbers using the fast transformation
      // and build the call.
      base::SmallVector<Node*, 16> args(args_count);
      for (int i = 0; i < wasm_param_count; ++i) {
        Node* wasm_param = FromJSFast(params[i + 1], sig_->GetParam(i));
        args[i + 1] = wasm_param;
      }
      Node* jsval = BuildCallAndReturn(is_import, js_context, function_data,
                                       args, js_wasm_call_data, frame_state);
      gasm_->Goto(&done, jsval);
      gasm_->Bind(&slow_path);
    }
    // Convert JS parameters to wasm numbers using the default transformation
    // and build the call.
    base::SmallVector<Node*, 16> args(args_count);
    for (int i = 0; i < wasm_param_count; ++i) {
      bool do_conversion =
          !js_wasm_call_data || js_wasm_call_data->arg_needs_conversion(i);
      if (do_conversion) {
        args[i + 1] =
            FromJS(params[i + 1], js_context, sig_->GetParam(i), frame_state);
      } else {
        Node* wasm_param = params[i + 1];

        // For Float32 parameters
        // we set UseInfo::CheckedNumberOrOddballAsFloat64 in
        // simplified-lowering and we need to add here a conversion from Float64
        // to Float32.
        if (sig_->GetParam(i).kind() == wasm::kF32) {
          wasm_param = gasm_->TruncateFloat64ToFloat32(wasm_param);
        }

        args[i + 1] = wasm_param;
      }
    }
    Node* jsval = BuildCallAndReturn(is_import, js_context, function_data, args,
                                     js_wasm_call_data, frame_state);
    // If both the default and a fast transformation paths are present,
    // get the return value based on the path used.
    if (include_fast_path) {
      gasm_->Goto(&done, jsval);
      gasm_->Bind(&done);
      Return(done.PhiAt(0));
    } else {
      Return(jsval);
    }
    if (ContainsInt64(sig_)) LowerInt64(kCalledFromJS);
  }

  Node* BuildReceiverNode(Node* callable_node, Node* native_context,
                          Node* undefined_node) {
    // Check function strict bit.
    Node* shared_function_info = gasm_->LoadSharedFunctionInfo(callable_node);
    Node* flags = gasm_->LoadFromObject(
        MachineType::Int32(), shared_function_info,
        wasm::ObjectAccess::FlagsOffsetInSharedFunctionInfo());
    Node* strict_check =
        Binop(wasm::kExprI32And, flags,
              Int32Constant(SharedFunctionInfo::IsNativeBit::kMask |
                            SharedFunctionInfo::IsStrictBit::kMask));

    // Load global receiver if sloppy else use undefined.
    Diamond strict_d(graph(), mcgraph()->common(), strict_check,
                     BranchHint::kNone);
    Node* old_effect = effect();
    SetControl(strict_d.if_false);
    Node* global_proxy = gasm_->LoadFixedArrayElementPtr(
        native_context, Context::GLOBAL_PROXY_INDEX);
    SetEffectControl(strict_d.EffectPhi(old_effect, global_proxy),
                     strict_d.merge);
    return strict_d.Phi(MachineRepresentation::kTagged, undefined_node,
                        global_proxy);
  }

  bool BuildWasmToJSWrapper(WasmImportCallKind kind, int expected_arity) {
    int wasm_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the parameter nodes.
    Start(wasm_count + 4);

    Node* native_context =
        LOAD_INSTANCE_FIELD(NativeContext, MachineType::TaggedPointer());

    if (kind == WasmImportCallKind::kRuntimeTypeError) {
      // =======================================================================
      // === Runtime TypeError =================================================
      // =======================================================================
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError,
                                    native_context, nullptr, 0);
      TerminateThrow(effect(), control());
      return false;
    }

    // The callable is passed as the last parameter, after Wasm arguments.
    Node* callable_node = Param(wasm_count + 1);

    Node* undefined_node = UndefinedValue();

    Node* call = nullptr;

    // Clear the ThreadInWasm flag.
    BuildModifyThreadInWasmFlag(false);

    switch (kind) {
      // =======================================================================
      // === JS Functions with matching arity ==================================
      // =======================================================================
      case WasmImportCallKind::kJSFunctionArityMatch: {
        base::SmallVector<Node*, 16> args(wasm_count + 7);
        int pos = 0;
        Node* function_context =
            gasm_->LoadContextFromJSFunction(callable_node);
        args[pos++] = callable_node;  // target callable.

        // Determine receiver at runtime.
        args[pos++] =
            BuildReceiverNode(callable_node, native_context, undefined_node);

        auto call_descriptor = Linkage::GetJSCallDescriptor(
            graph()->zone(), false, wasm_count + 1, CallDescriptor::kNoFlags);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(VectorOf(args), pos, wasm_count, sig_);

        args[pos++] = undefined_node;                        // new target
        args[pos++] = Int32Constant(wasm_count);             // argument count
        args[pos++] = function_context;
        args[pos++] = effect();
        args[pos++] = control();

        DCHECK_EQ(pos, args.size());
        call = gasm_->Call(call_descriptor, pos, args.begin());
        break;
      }
      // =======================================================================
      // === JS Functions with mismatching arity ===============================
      // =======================================================================
      case WasmImportCallKind::kJSFunctionArityMismatch: {
        int pushed_count = std::max(expected_arity, wasm_count);
        base::SmallVector<Node*, 16> args(pushed_count + 7);
        int pos = 0;

        args[pos++] = callable_node;  // target callable.
        // Determine receiver at runtime.
        args[pos++] =
            BuildReceiverNode(callable_node, native_context, undefined_node);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(VectorOf(args), pos, wasm_count, sig_);
        for (int i = wasm_count; i < expected_arity; ++i) {
          args[pos++] = undefined_node;
        }
        args[pos++] = undefined_node;                        // new target
        args[pos++] = Int32Constant(wasm_count);             // argument count

        Node* function_context =
            gasm_->LoadContextFromJSFunction(callable_node);
        args[pos++] = function_context;
        args[pos++] = effect();
        args[pos++] = control();
        DCHECK_EQ(pos, args.size());

        auto call_descriptor = Linkage::GetJSCallDescriptor(
            graph()->zone(), false, pushed_count + 1, CallDescriptor::kNoFlags);
        call = gasm_->Call(call_descriptor, pos, args.begin());
        break;
      }
      // =======================================================================
      // === General case of unknown callable ==================================
      // =======================================================================
      case WasmImportCallKind::kUseCallBuiltin: {
        base::SmallVector<Node*, 16> args(wasm_count + 7);
        int pos = 0;
        args[pos++] =
            gasm_->GetBuiltinPointerTarget(Builtins::kCall_ReceiverIsAny);
        args[pos++] = callable_node;
        args[pos++] = Int32Constant(wasm_count);             // argument count
        args[pos++] = undefined_node;                        // receiver

        auto call_descriptor = Linkage::GetStubCallDescriptor(
            graph()->zone(), CallTrampolineDescriptor{}, wasm_count + 1,
            CallDescriptor::kNoFlags, Operator::kNoProperties,
            StubCallMode::kCallBuiltinPointer);

        // Convert wasm numbers to JS values.
        pos = AddArgumentNodes(VectorOf(args), pos, wasm_count, sig_);

        // The native_context is sufficient here, because all kind of callables
        // which depend on the context provide their own context. The context
        // here is only needed if the target is a constructor to throw a
        // TypeError, if the target is a native function, or if the target is a
        // callable JSObject, which can only be constructed by the runtime.
        args[pos++] = native_context;
        args[pos++] = effect();
        args[pos++] = control();

        DCHECK_EQ(pos, args.size());
        call = gasm_->Call(call_descriptor, pos, args.begin());
        break;
      }
      default:
        UNREACHABLE();
    }
    DCHECK_NOT_NULL(call);

    SetSourcePosition(call, 0);

    // Convert the return value(s) back.
    if (sig_->return_count() <= 1) {
      Node* val = sig_->return_count() == 0
                      ? Int32Constant(0)
                      : FromJS(call, native_context, sig_->GetReturn());
      BuildModifyThreadInWasmFlag(true);
      Return(val);
    } else {
      Node* fixed_array =
          BuildMultiReturnFixedArrayFromIterable(sig_, call, native_context);
      base::SmallVector<Node*, 8> wasm_values(sig_->return_count());
      for (unsigned i = 0; i < sig_->return_count(); ++i) {
        wasm_values[i] = FromJS(gasm_->LoadFixedArrayElementAny(fixed_array, i),
                                native_context, sig_->GetReturn(i));
      }
      BuildModifyThreadInWasmFlag(true);
      Return(VectorOf(wasm_values));
    }

    if (ContainsInt64(sig_)) LowerInt64(kCalledFromWasm);
    return true;
  }

  void BuildCapiCallWrapper(Address address) {
    // Store arguments on our stack, then align the stack for calling to C.
    int param_bytes = 0;
    for (wasm::ValueType type : sig_->parameters()) {
      param_bytes += type.element_size_bytes();
    }
    int return_bytes = 0;
    for (wasm::ValueType type : sig_->returns()) {
      return_bytes += type.element_size_bytes();
    }

    int stack_slot_bytes = std::max(param_bytes, return_bytes);
    Node* values = stack_slot_bytes == 0
                       ? mcgraph()->IntPtrConstant(0)
                       : graph()->NewNode(mcgraph()->machine()->StackSlot(
                             stack_slot_bytes, kDoubleAlignment));

    int offset = 0;
    int param_count = static_cast<int>(sig_->parameter_count());
    for (int i = 0; i < param_count; ++i) {
      wasm::ValueType type = sig_->GetParam(i);
      // Start from the parameter with index 1 to drop the instance_node.
      // TODO(jkummerow): When a values is a reference type, we should pass it
      // in a GC-safe way, not just as a raw pointer.
      SetEffect(graph()->NewNode(GetSafeStoreOperator(offset, type), values,
                                 Int32Constant(offset), Param(i + 1), effect(),
                                 control()));
      offset += type.element_size_bytes();
    }
    // The function is passed as the last parameter, after Wasm arguments.
    Node* function_node = Param(param_count + 1);
    Node* sfi_data = gasm_->LoadFunctionDataFromJSFunction(function_node);
    Node* host_data_foreign =
        gasm_->Load(MachineType::AnyTagged(), sfi_data,
                    wasm::ObjectAccess::ToTagged(
                        WasmCapiFunctionData::kEmbedderDataOffset));

    BuildModifyThreadInWasmFlag(false);
    Node* isolate_root = BuildLoadIsolateRoot();
    Node* fp_value = graph()->NewNode(mcgraph()->machine()->LoadFramePointer());
    gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                     kNoWriteBarrier),
                 isolate_root, Isolate::c_entry_fp_offset(), fp_value);

    // TODO(jkummerow): Load the address from the {host_data}, and cache
    // wrappers per signature.
    const ExternalReference ref = ExternalReference::Create(address);
    Node* function = gasm_->ExternalConstant(ref);

    // Parameters: Address host_data_foreign, Address arguments.
    MachineType host_sig_types[] = {
        MachineType::Pointer(), MachineType::Pointer(), MachineType::Pointer()};
    MachineSignature host_sig(1, 2, host_sig_types);
    Node* return_value =
        BuildCCall(&host_sig, function, host_data_foreign, values);

    BuildModifyThreadInWasmFlag(true);

    Node* exception_branch = graph()->NewNode(
        mcgraph()->common()->Branch(BranchHint::kTrue),
        gasm_->WordEqual(return_value, mcgraph()->IntPtrConstant(0)),
        control());
    SetControl(
        graph()->NewNode(mcgraph()->common()->IfFalse(), exception_branch));
    WasmRethrowDescriptor interface_descriptor;
    auto call_descriptor = Linkage::GetStubCallDescriptor(
        mcgraph()->zone(), interface_descriptor,
        interface_descriptor.GetStackParameterCount(), CallDescriptor::kNoFlags,
        Operator::kNoProperties, StubCallMode::kCallWasmRuntimeStub);
    Node* call_target = mcgraph()->RelocatableIntPtrConstant(
        wasm::WasmCode::kWasmRethrow, RelocInfo::WASM_STUB_CALL);
    gasm_->Call(call_descriptor, call_target, return_value);
    TerminateThrow(effect(), control());

    SetEffectControl(
        return_value,
        graph()->NewNode(mcgraph()->common()->IfTrue(), exception_branch));
    DCHECK_LT(sig_->return_count(), wasm::kV8MaxWasmFunctionMultiReturns);
    size_t return_count = sig_->return_count();
    if (return_count == 0) {
      Return(Int32Constant(0));
    } else {
      base::SmallVector<Node*, 8> returns(return_count);
      offset = 0;
      for (size_t i = 0; i < return_count; ++i) {
        wasm::ValueType type = sig_->GetReturn(i);
        Node* val = SetEffect(
            graph()->NewNode(GetSafeLoadOperator(offset, type), values,
                             Int32Constant(offset), effect(), control()));
        returns[i] = val;
        offset += type.element_size_bytes();
      }
      Return(VectorOf(returns));
    }

    if (ContainsInt64(sig_)) LowerInt64(kCalledFromWasm);
  }

  void BuildJSToJSWrapper() {
    int wasm_count = static_cast<int>(sig_->parameter_count());

    // Build the start and the parameter nodes.
    int param_count = 1 /* closure */ + 1 /* receiver */ + wasm_count +
                      1 /* new.target */ + 1 /* #arg */ + 1 /* context */;
    Start(param_count);
    Node* closure = Param(Linkage::kJSCallClosureParamIndex);
    Node* context = Param(Linkage::GetJSCallContextParamIndex(wasm_count + 1));

    // Throw a TypeError if the signature is incompatible with JavaScript.
    if (!wasm::IsJSCompatibleSignature(sig_, module_, enabled_features_)) {
      BuildCallToRuntimeWithContext(Runtime::kWasmThrowJSTypeError, context,
                                    nullptr, 0);
      TerminateThrow(effect(), control());
      return;
    }

    // Load the original callable from the closure.
    Node* func_data = gasm_->LoadFunctionDataFromJSFunction(closure);
    Node* callable = gasm_->LoadFromObject(
        MachineType::AnyTagged(), func_data,
        wasm::ObjectAccess::ToTagged(WasmJSFunctionData::kCallableOffset));

    // Call the underlying closure.
    base::SmallVector<Node*, 16> args(wasm_count + 7);
    int pos = 0;
    args[pos++] = gasm_->GetBuiltinPointerTarget(Builtins::kCall_ReceiverIsAny);
    args[pos++] = callable;
    args[pos++] = Int32Constant(wasm_count);  // argument count
    args[pos++] = UndefinedValue();           // receiver

    auto call_descriptor = Linkage::GetStubCallDescriptor(
        graph()->zone(), CallTrampolineDescriptor{}, wasm_count + 1,
        CallDescriptor::kNoFlags, Operator::kNoProperties,
        StubCallMode::kCallBuiltinPointer);

    // Convert parameter JS values to wasm numbers and back to JS values.
    for (int i = 0; i < wasm_count; ++i) {
      Node* param = Param(i + 1);  // Start from index 1 to skip receiver.
      args[pos++] =
          ToJS(FromJS(param, context, sig_->GetParam(i)), sig_->GetParam(i));
    }

    args[pos++] = context;
    args[pos++] = effect();
    args[pos++] = control();

    DCHECK_EQ(pos, args.size());
    Node* call = gasm_->Call(call_descriptor, pos, args.begin());

    // Convert return JS values to wasm numbers and back to JS values.
    Node* jsval;
    if (sig_->return_count() == 0) {
      jsval = UndefinedValue();
    } else if (sig_->return_count() == 1) {
      jsval = ToJS(FromJS(call, context, sig_->GetReturn()), sig_->GetReturn());
    } else {
      Node* fixed_array =
          BuildMultiReturnFixedArrayFromIterable(sig_, call, context);
      int32_t return_count = static_cast<int32_t>(sig_->return_count());
      Node* size = gasm_->NumberConstant(return_count);
      jsval = BuildCallAllocateJSArray(size, context);
      Node* result_fixed_array = gasm_->LoadJSArrayElements(jsval);
      for (unsigned i = 0; i < sig_->return_count(); ++i) {
        const auto& type = sig_->GetReturn(i);
        Node* elem = gasm_->LoadFixedArrayElementAny(fixed_array, i);
        Node* cast = ToJS(FromJS(elem, context, type), type);
        gasm_->StoreFixedArrayElementAny(result_fixed_array, i, cast);
      }
    }
    Return(jsval);
  }

  void BuildCWasmEntry() {
    // +1 offset for first parameter index being -1.
    Start(CWasmEntryParameters::kNumParameters + 1);

    Node* code_entry = Param(CWasmEntryParameters::kCodeEntry);
    Node* object_ref = Param(CWasmEntryParameters::kObjectRef);
    Node* arg_buffer = Param(CWasmEntryParameters::kArgumentsBuffer);
    Node* c_entry_fp = Param(CWasmEntryParameters::kCEntryFp);

    Node* fp_value = graph()->NewNode(mcgraph()->machine()->LoadFramePointer());
    gasm_->Store(StoreRepresentation(MachineType::PointerRepresentation(),
                                     kNoWriteBarrier),
                 fp_value, TypedFrameConstants::kFirstPushedFrameValueOffset,
                 c_entry_fp);

    int wasm_arg_count = static_cast<int>(sig_->parameter_count());
    base::SmallVector<Node*, 16> args(wasm_arg_count + 4);

    int pos = 0;
    args[pos++] = code_entry;
    args[pos++] = object_ref;

    int offset = 0;
    for (wasm::ValueType type : sig_->parameters()) {
      Node* arg_load = SetEffect(
          graph()->NewNode(GetSafeLoadOperator(offset, type), arg_buffer,
                           Int32Constant(offset), effect(), control()));
      args[pos++] = arg_load;
      offset += type.element_size_bytes();
    }

    args[pos++] = effect();
    args[pos++] = control();

    // Call the wasm code.
    auto call_descriptor = GetWasmCallDescriptor(mcgraph()->zone(), sig_);

    DCHECK_EQ(pos, args.size());
    Node* call = gasm_->Call(call_descriptor, pos, args.begin());

    Node* if_success = graph()->NewNode(mcgraph()->common()->IfSuccess(), call);
    Node* if_exception =
        graph()->NewNode(mcgraph()->common()->IfException(), call, call);

    // Handle exception: return it.
    SetControl(if_exception);
    Return(if_exception);

    // Handle success: store the return value(s).
    SetControl(if_success);
    pos = 0;
    offset = 0;
    for (wasm::ValueType type : sig_->returns()) {
      Node* value = sig_->return_count() == 1
                        ? call
                        : graph()->NewNode(mcgraph()->common()->Projection(pos),
                                           call, control());
      SetEffect(graph()->NewNode(GetSafeStoreOperator(offset, type), arg_buffer,
                                 Int32Constant(offset), value, effect(),
                                 control()));
      offset += type.element_size_bytes();
      pos++;
    }

    Return(mcgraph()->IntPtrConstant(0));

    if (mcgraph()->machine()->Is32() && ContainsInt64(sig_)) {
      // No special lowering should be requested in the C entry.
      DCHECK_NULL(lowering_special_case_);

      MachineRepresentation sig_reps[] = {
          MachineType::PointerRepresentation(),  // return value
          MachineType::PointerRepresentation(),  // target
          MachineRepresentation::kTagged,        // object_ref
          MachineType::PointerRepresentation(),  // argv
          MachineType::PointerRepresentation()   // c_entry_fp
      };
      Signature<MachineRepresentation> c_entry_sig(1, 4, sig_reps);
      Int64Lowering r(mcgraph()->graph(), mcgraph()->machine(),
                      mcgraph()->common(), gasm_->simplified(),
                      mcgraph()->zone(), &c_entry_sig);
      r.LowerGraph();
    }
  }

 private:
  const wasm::WasmModule* module_;
  StubCallMode stub_mode_;
  SetOncePointer<const Operator> int32_to_heapnumber_operator_;
  SetOncePointer<const Operator> tagged_non_smi_to_int32_operator_;
  SetOncePointer<const Operator> float32_to_number_operator_;
  SetOncePointer<const Operator> float64_to_number_operator_;
  SetOncePointer<const Operator> tagged_to_float64_operator_;
  wasm::WasmFeatures enabled_features_;
  CallDescriptor* bigint_to_i64_descriptor_ = nullptr;
  CallDescriptor* i64_to_bigint_descriptor_ = nullptr;
};

}  // namespace

void BuildInlinedJSToWasmWrapper(
    Zone* zone, MachineGraph* mcgraph, const wasm::FunctionSig* signature,
    const wasm::WasmModule* module, Isolate* isolate,
    compiler::SourcePositionTable* spt, StubCallMode stub_mode,
    wasm::WasmFeatures features, const JSWasmCallData* js_wasm_call_data,
    Node* frame_state) {
  WasmWrapperGraphBuilder builder(zone, mcgraph, signature, module, isolate,
                                  spt, stub_mode, features);
  builder.BuildJSToWasmWrapper(false, js_wasm_call_data, frame_state);
}

std::unique_ptr<OptimizedCompilationJob> NewJSToWasmCompilationJob(
    Isolate* isolate, wasm::WasmEngine* wasm_engine,
    const wasm::FunctionSig* sig, const wasm::WasmModule* module,
    bool is_import, const wasm::WasmFeatures& enabled_features) {
  //----------------------------------------------------------------------------
  // Create the Graph.
  //----------------------------------------------------------------------------
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      wasm_engine->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, module, isolate,
                                  nullptr, StubCallMode::kCallBuiltinPointer,
                                  enabled_features);
  builder.BuildJSToWasmWrapper(is_import);

  //----------------------------------------------------------------------------
  // Create the compilation job.
  //----------------------------------------------------------------------------
  std::unique_ptr<char[]> debug_name = WasmExportedFunction::GetDebugName(sig);

  int params = static_cast<int>(sig->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      zone.get(), false, params + 1, CallDescriptor::kNoFlags);

  return Pipeline::NewWasmHeapStubCompilationJob(
      isolate, wasm_engine, incoming, std::move(zone), graph,
      CodeKind::JS_TO_WASM_FUNCTION, std::move(debug_name),
      WasmAssemblerOptions());
}

std::pair<WasmImportCallKind, Handle<JSReceiver>> ResolveWasmImportCall(
    Handle<JSReceiver> callable, const wasm::FunctionSig* expected_sig,
    const wasm::WasmModule* module,
    const wasm::WasmFeatures& enabled_features) {
  if (WasmExportedFunction::IsWasmExportedFunction(*callable)) {
    auto imported_function = Handle<WasmExportedFunction>::cast(callable);
    if (!imported_function->MatchesSignature(module, expected_sig)) {
      return std::make_pair(WasmImportCallKind::kLinkError, callable);
    }
    uint32_t func_index =
        static_cast<uint32_t>(imported_function->function_index());
    if (func_index >=
        imported_function->instance().module()->num_imported_functions) {
      return std::make_pair(WasmImportCallKind::kWasmToWasm, callable);
    }
    Isolate* isolate = callable->GetIsolate();
    // Resolve the shortcut to the underlying callable and continue.
    Handle<WasmInstanceObject> instance(imported_function->instance(), isolate);
    ImportedFunctionEntry entry(instance, func_index);
    callable = handle(entry.callable(), isolate);
  }
  if (WasmJSFunction::IsWasmJSFunction(*callable)) {
    auto js_function = Handle<WasmJSFunction>::cast(callable);
    if (!js_function->MatchesSignature(expected_sig)) {
      return std::make_pair(WasmImportCallKind::kLinkError, callable);
    }
    Isolate* isolate = callable->GetIsolate();
    // Resolve the short-cut to the underlying callable and continue.
    callable = handle(js_function->GetCallable(), isolate);
  }
  if (WasmCapiFunction::IsWasmCapiFunction(*callable)) {
    auto capi_function = Handle<WasmCapiFunction>::cast(callable);
    if (!capi_function->MatchesSignature(expected_sig)) {
      return std::make_pair(WasmImportCallKind::kLinkError, callable);
    }
    return std::make_pair(WasmImportCallKind::kWasmToCapi, callable);
  }
  // Assuming we are calling to JS, check whether this would be a runtime error.
  if (!wasm::IsJSCompatibleSignature(expected_sig, module, enabled_features)) {
    return std::make_pair(WasmImportCallKind::kRuntimeTypeError, callable);
  }
  // For JavaScript calls, determine whether the target has an arity match.
  if (callable->IsJSFunction()) {
    Handle<JSFunction> function = Handle<JSFunction>::cast(callable);
    Handle<SharedFunctionInfo> shared(function->shared(),
                                      function->GetIsolate());

// Check for math intrinsics.
#define COMPARE_SIG_FOR_BUILTIN(name)                                     \
  {                                                                       \
    const wasm::FunctionSig* sig =                                        \
        wasm::WasmOpcodes::Signature(wasm::kExpr##name);                  \
    if (!sig) sig = wasm::WasmOpcodes::AsmjsSignature(wasm::kExpr##name); \
    DCHECK_NOT_NULL(sig);                                                 \
    if (*expected_sig == *sig) {                                          \
      return std::make_pair(WasmImportCallKind::k##name, callable);       \
    }                                                                     \
  }
#define COMPARE_SIG_FOR_BUILTIN_F64(name) \
  case Builtins::kMath##name:             \
    COMPARE_SIG_FOR_BUILTIN(F64##name);   \
    break;
#define COMPARE_SIG_FOR_BUILTIN_F32_F64(name) \
  case Builtins::kMath##name:                 \
    COMPARE_SIG_FOR_BUILTIN(F64##name);       \
    COMPARE_SIG_FOR_BUILTIN(F32##name);       \
    break;

    if (FLAG_wasm_math_intrinsics && shared->HasBuiltinId()) {
      switch (shared->builtin_id()) {
        COMPARE_SIG_FOR_BUILTIN_F64(Acos);
        COMPARE_SIG_FOR_BUILTIN_F64(Asin);
        COMPARE_SIG_FOR_BUILTIN_F64(Atan);
        COMPARE_SIG_FOR_BUILTIN_F64(Cos);
        COMPARE_SIG_FOR_BUILTIN_F64(Sin);
        COMPARE_SIG_FOR_BUILTIN_F64(Tan);
        COMPARE_SIG_FOR_BUILTIN_F64(Exp);
        COMPARE_SIG_FOR_BUILTIN_F64(Log);
        COMPARE_SIG_FOR_BUILTIN_F64(Atan2);
        COMPARE_SIG_FOR_BUILTIN_F64(Pow);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Min);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Max);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Abs);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Ceil);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Floor);
        COMPARE_SIG_FOR_BUILTIN_F32_F64(Sqrt);
        case Builtins::kMathFround:
          COMPARE_SIG_FOR_BUILTIN(F32ConvertF64);
          break;
        default:
          break;
      }
    }

#undef COMPARE_SIG_FOR_BUILTIN
#undef COMPARE_SIG_FOR_BUILTIN_F64
#undef COMPARE_SIG_FOR_BUILTIN_F32_F64

    if (IsClassConstructor(shared->kind())) {
      // Class constructor will throw anyway.
      return std::make_pair(WasmImportCallKind::kUseCallBuiltin, callable);
    }

    if (shared->internal_formal_parameter_count() ==
        expected_sig->parameter_count()) {
      return std::make_pair(WasmImportCallKind::kJSFunctionArityMatch,
                            callable);
    }

    // If function isn't compiled, compile it now.
    Isolate* isolate = callable->GetIsolate();
    IsCompiledScope is_compiled_scope(shared->is_compiled_scope(isolate));
    if (!is_compiled_scope.is_compiled()) {
      Compiler::Compile(isolate, function, Compiler::CLEAR_EXCEPTION,
                        &is_compiled_scope);
    }

    return std::make_pair(WasmImportCallKind::kJSFunctionArityMismatch,
                          callable);
  }
  // Unknown case. Use the call builtin.
  return std::make_pair(WasmImportCallKind::kUseCallBuiltin, callable);
}

namespace {

wasm::WasmOpcode GetMathIntrinsicOpcode(WasmImportCallKind kind,
                                        const char** name_ptr) {
#define CASE(name)                          \
  case WasmImportCallKind::k##name:         \
    *name_ptr = "WasmMathIntrinsic:" #name; \
    return wasm::kExpr##name
  switch (kind) {
    CASE(F64Acos);
    CASE(F64Asin);
    CASE(F64Atan);
    CASE(F64Cos);
    CASE(F64Sin);
    CASE(F64Tan);
    CASE(F64Exp);
    CASE(F64Log);
    CASE(F64Atan2);
    CASE(F64Pow);
    CASE(F64Ceil);
    CASE(F64Floor);
    CASE(F64Sqrt);
    CASE(F64Min);
    CASE(F64Max);
    CASE(F64Abs);
    CASE(F32Min);
    CASE(F32Max);
    CASE(F32Abs);
    CASE(F32Ceil);
    CASE(F32Floor);
    CASE(F32Sqrt);
    CASE(F32ConvertF64);
    default:
      UNREACHABLE();
      return wasm::kExprUnreachable;
  }
#undef CASE
}

wasm::WasmCompilationResult CompileWasmMathIntrinsic(
    wasm::WasmEngine* wasm_engine, WasmImportCallKind kind,
    const wasm::FunctionSig* sig) {
  DCHECK_EQ(1, sig->return_count());

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmMathIntrinsic");

  Zone zone(wasm_engine->allocator(), ZONE_NAME, kCompressGraphZone);

  // Compile a Wasm function with a single bytecode and let TurboFan
  // generate either inlined machine code or a call to a helper.
  SourcePositionTable* source_positions = nullptr;
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  wasm::CompilationEnv env(
      nullptr, wasm::UseTrapHandler::kNoTrapHandler,
      wasm::RuntimeExceptionSupport::kNoRuntimeExceptionSupport,
      wasm::WasmFeatures::All(), wasm::LowerSimd::kNoLowerSimd);

  WasmGraphBuilder builder(&env, mcgraph->zone(), mcgraph, sig,
                           source_positions);

  // Set up the graph start.
  builder.Start(static_cast<int>(sig->parameter_count() + 1 + 1));

  // Generate either a unop or a binop.
  Node* node = nullptr;
  const char* debug_name = "WasmMathIntrinsic";
  auto opcode = GetMathIntrinsicOpcode(kind, &debug_name);
  switch (sig->parameter_count()) {
    case 1:
      node = builder.Unop(opcode, builder.Param(1));
      break;
    case 2:
      node = builder.Binop(opcode, builder.Param(1), builder.Param(2));
      break;
    default:
      UNREACHABLE();
  }

  builder.Return(node);

  // Run the compiler pipeline to generate machine code.
  auto call_descriptor = GetWasmCallDescriptor(&zone, sig);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  wasm::WasmCompilationResult result = Pipeline::GenerateCodeForWasmNativeStub(
      wasm_engine, call_descriptor, mcgraph, CodeKind::WASM_FUNCTION,
      wasm::WasmCode::kFunction, debug_name, WasmStubAssemblerOptions(),
      source_positions);
  return result;
}

}  // namespace

wasm::WasmCompilationResult CompileWasmImportCallWrapper(
    wasm::WasmEngine* wasm_engine, wasm::CompilationEnv* env,
    WasmImportCallKind kind, const wasm::FunctionSig* sig,
    bool source_positions, int expected_arity) {
  DCHECK_NE(WasmImportCallKind::kLinkError, kind);
  DCHECK_NE(WasmImportCallKind::kWasmToWasm, kind);

  // Check for math intrinsics first.
  if (FLAG_wasm_math_intrinsics &&
      kind >= WasmImportCallKind::kFirstMathIntrinsic &&
      kind <= WasmImportCallKind::kLastMathIntrinsic) {
    return CompileWasmMathIntrinsic(wasm_engine, kind, sig);
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmImportCallWrapper");
  //----------------------------------------------------------------------------
  // Create the Graph
  //----------------------------------------------------------------------------
  Zone zone(wasm_engine->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone.New<Graph>(&zone);
  CommonOperatorBuilder* common = zone.New<CommonOperatorBuilder>(&zone);
  MachineOperatorBuilder* machine = zone.New<MachineOperatorBuilder>(
      &zone, MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone.New<MachineGraph>(graph, common, machine);

  SourcePositionTable* source_position_table =
      source_positions ? zone.New<SourcePositionTable>(graph) : nullptr;

  WasmWrapperGraphBuilder builder(
      &zone, mcgraph, sig, env->module, nullptr, source_position_table,
      StubCallMode::kCallWasmRuntimeStub, env->enabled_features);
  builder.BuildWasmToJSWrapper(kind, expected_arity);

  // Build a name in the form "wasm-to-js-<kind>-<signature>".
  constexpr size_t kMaxNameLen = 128;
  char func_name[kMaxNameLen];
  int name_prefix_len = SNPrintF(VectorOf(func_name, kMaxNameLen),
                                 "wasm-to-js-%d-", static_cast<int>(kind));
  PrintSignature(VectorOf(func_name, kMaxNameLen) + name_prefix_len, sig, '-');

  // Schedule and compile to machine code.
  CallDescriptor* incoming =
      GetWasmCallDescriptor(&zone, sig, WasmGraphBuilder::kNoRetpoline,
                            WasmCallKind::kWasmImportWrapper);
  if (machine->Is32()) {
    incoming = GetI32WasmCallDescriptor(&zone, incoming);
  }
  wasm::WasmCompilationResult result = Pipeline::GenerateCodeForWasmNativeStub(
      wasm_engine, incoming, mcgraph, CodeKind::WASM_TO_JS_FUNCTION,
      wasm::WasmCode::kWasmToJsWrapper, func_name, WasmStubAssemblerOptions(),
      source_position_table);
  result.kind = wasm::WasmCompilationResult::kWasmToJsWrapper;
  return result;
}

wasm::WasmCode* CompileWasmCapiCallWrapper(wasm::WasmEngine* wasm_engine,
                                           wasm::NativeModule* native_module,
                                           const wasm::FunctionSig* sig,
                                           Address address) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileWasmCapiFunction");

  Zone zone(wasm_engine->allocator(), ZONE_NAME, kCompressGraphZone);

  // TODO(jkummerow): Extract common code into helper method.
  SourcePositionTable* source_positions = nullptr;
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  WasmWrapperGraphBuilder builder(
      &zone, mcgraph, sig, native_module->module(), nullptr, source_positions,
      StubCallMode::kCallWasmRuntimeStub, native_module->enabled_features());

  // Set up the graph start.
  int param_count = static_cast<int>(sig->parameter_count()) +
                    1 /* offset for first parameter index being -1 */ +
                    1 /* Wasm instance */ + 1 /* kExtraCallableParam */;
  builder.Start(param_count);
  builder.BuildCapiCallWrapper(address);

  // Run the compiler pipeline to generate machine code.
  CallDescriptor* call_descriptor =
      GetWasmCallDescriptor(&zone, sig, WasmGraphBuilder::kNoRetpoline,
                            WasmCallKind::kWasmCapiFunction);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  const char* debug_name = "WasmCapiCall";
  wasm::WasmCompilationResult result = Pipeline::GenerateCodeForWasmNativeStub(
      wasm_engine, call_descriptor, mcgraph, CodeKind::WASM_TO_CAPI_FUNCTION,
      wasm::WasmCode::kWasmToCapiWrapper, debug_name,
      WasmStubAssemblerOptions(), source_positions);
  std::unique_ptr<wasm::WasmCode> wasm_code = native_module->AddCode(
      wasm::kAnonymousFuncIndex, result.code_desc, result.frame_slot_count,
      result.tagged_parameter_slots,
      result.protected_instructions_data.as_vector(),
      result.source_positions.as_vector(), wasm::WasmCode::kWasmToCapiWrapper,
      wasm::ExecutionTier::kNone, wasm::kNoDebugging);
  return native_module->PublishCode(std::move(wasm_code));
}

MaybeHandle<Code> CompileWasmToJSWrapper(Isolate* isolate,
                                         const wasm::FunctionSig* sig,
                                         WasmImportCallKind kind,
                                         int expected_arity) {
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      isolate->allocator(), ZONE_NAME, kCompressGraphZone);

  // Create the Graph
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, nullptr, nullptr,
                                  nullptr, StubCallMode::kCallWasmRuntimeStub,
                                  wasm::WasmFeatures::FromIsolate(isolate));
  builder.BuildWasmToJSWrapper(kind, expected_arity);

  // Build a name in the form "wasm-to-js-<kind>-<signature>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 11;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  base::Memcpy(name_buffer.get(), "wasm-to-js:", kNamePrefixLen);
  PrintSignature(VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen,
                 sig);

  // Generate the call descriptor.
  CallDescriptor* incoming =
      GetWasmCallDescriptor(zone.get(), sig, WasmGraphBuilder::kNoRetpoline,
                            WasmCallKind::kWasmImportWrapper);

  // Run the compilation job synchronously.
  std::unique_ptr<OptimizedCompilationJob> job(
      Pipeline::NewWasmHeapStubCompilationJob(
          isolate, isolate->wasm_engine(), incoming, std::move(zone), graph,
          CodeKind::WASM_TO_JS_FUNCTION, std::move(name_buffer),
          AssemblerOptions::Default(isolate)));

  // Compile the wrapper
  if (job->ExecuteJob(isolate->counters()->runtime_call_stats()) ==
          CompilationJob::FAILED ||
      job->FinalizeJob(isolate) == CompilationJob::FAILED) {
    return Handle<Code>();
  }
  Handle<Code> code = job->compilation_info()->code();
  return code;
}

MaybeHandle<Code> CompileJSToJSWrapper(Isolate* isolate,
                                       const wasm::FunctionSig* sig,
                                       const wasm::WasmModule* module) {
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      isolate->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, module, isolate,
                                  nullptr, StubCallMode::kCallBuiltinPointer,
                                  wasm::WasmFeatures::FromIsolate(isolate));
  builder.BuildJSToJSWrapper();

  int wasm_count = static_cast<int>(sig->parameter_count());
  CallDescriptor* incoming = Linkage::GetJSCallDescriptor(
      zone.get(), false, wasm_count + 1, CallDescriptor::kNoFlags);

  // Build a name in the form "js-to-js:<params>:<returns>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 9;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  base::Memcpy(name_buffer.get(), "js-to-js:", kNamePrefixLen);
  PrintSignature(VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen,
                 sig);

  // Run the compilation job synchronously.
  std::unique_ptr<OptimizedCompilationJob> job(
      Pipeline::NewWasmHeapStubCompilationJob(
          isolate, isolate->wasm_engine(), incoming, std::move(zone), graph,
          CodeKind::JS_TO_JS_FUNCTION, std::move(name_buffer),
          AssemblerOptions::Default(isolate)));

  if (job->ExecuteJob(isolate->counters()->runtime_call_stats()) ==
          CompilationJob::FAILED ||
      job->FinalizeJob(isolate) == CompilationJob::FAILED) {
    return {};
  }
  Handle<Code> code = job->compilation_info()->code();

  return code;
}

Handle<Code> CompileCWasmEntry(Isolate* isolate, const wasm::FunctionSig* sig,
                               const wasm::WasmModule* module) {
  std::unique_ptr<Zone> zone = std::make_unique<Zone>(
      isolate->allocator(), ZONE_NAME, kCompressGraphZone);
  Graph* graph = zone->New<Graph>(zone.get());
  CommonOperatorBuilder* common = zone->New<CommonOperatorBuilder>(zone.get());
  MachineOperatorBuilder* machine = zone->New<MachineOperatorBuilder>(
      zone.get(), MachineType::PointerRepresentation(),
      InstructionSelector::SupportedMachineOperatorFlags(),
      InstructionSelector::AlignmentRequirements());
  MachineGraph* mcgraph = zone->New<MachineGraph>(graph, common, machine);

  WasmWrapperGraphBuilder builder(zone.get(), mcgraph, sig, module, nullptr,
                                  nullptr, StubCallMode::kCallBuiltinPointer,
                                  wasm::WasmFeatures::FromIsolate(isolate));
  builder.BuildCWasmEntry();

  // Schedule and compile to machine code.
  MachineType sig_types[] = {MachineType::Pointer(),    // return
                             MachineType::Pointer(),    // target
                             MachineType::AnyTagged(),  // object_ref
                             MachineType::Pointer(),    // argv
                             MachineType::Pointer()};   // c_entry_fp
  MachineSignature incoming_sig(1, 4, sig_types);
  // Traps need the root register, for TailCallRuntime to call
  // Runtime::kThrowWasmError.
  CallDescriptor::Flags flags = CallDescriptor::kInitializeRootRegister;
  CallDescriptor* incoming =
      Linkage::GetSimplifiedCDescriptor(zone.get(), &incoming_sig, flags);

  // Build a name in the form "c-wasm-entry:<params>:<returns>".
  constexpr size_t kMaxNameLen = 128;
  constexpr size_t kNamePrefixLen = 13;
  auto name_buffer = std::unique_ptr<char[]>(new char[kMaxNameLen]);
  base::Memcpy(name_buffer.get(), "c-wasm-entry:", kNamePrefixLen);
  PrintSignature(VectorOf(name_buffer.get(), kMaxNameLen) + kNamePrefixLen,
                 sig);

  // Run the compilation job synchronously.
  std::unique_ptr<OptimizedCompilationJob> job(
      Pipeline::NewWasmHeapStubCompilationJob(
          isolate, isolate->wasm_engine(), incoming, std::move(zone), graph,
          CodeKind::C_WASM_ENTRY, std::move(name_buffer),
          AssemblerOptions::Default(isolate)));

  CHECK_NE(job->ExecuteJob(isolate->counters()->runtime_call_stats(), nullptr),
           CompilationJob::FAILED);
  CHECK_NE(job->FinalizeJob(isolate), CompilationJob::FAILED);

  return job->compilation_info()->code();
}

namespace {

bool BuildGraphForWasmFunction(AccountingAllocator* allocator,
                               wasm::CompilationEnv* env,
                               const wasm::FunctionBody& func_body,
                               int func_index, wasm::WasmFeatures* detected,
                               MachineGraph* mcgraph,
                               std::vector<compiler::WasmLoopInfo>* loop_infos,
                               NodeOriginTable* node_origins,
                               SourcePositionTable* source_positions) {
  // Create a TF graph during decoding.
  WasmGraphBuilder builder(env, mcgraph->zone(), mcgraph, func_body.sig,
                           source_positions);
  wasm::VoidResult graph_construction_result = wasm::BuildTFGraph(
      allocator, env->enabled_features, env->module, &builder, detected,
      func_body, loop_infos, node_origins);
  if (graph_construction_result.failed()) {
    if (FLAG_trace_wasm_compiler) {
      StdoutStream{} << "Compilation failed: "
                     << graph_construction_result.error().message()
                     << std::endl;
    }
    return false;
  }

  // Lower SIMD first, i64x2 nodes will be lowered to int64 nodes, then int64
  // lowering will take care of them.
  auto sig = CreateMachineSignature(mcgraph->zone(), func_body.sig,
                                    WasmGraphBuilder::kCalledFromWasm);
  if (builder.has_simd() &&
      (!CpuFeatures::SupportsWasmSimd128() || env->lower_simd)) {
    SimplifiedOperatorBuilder simplified(mcgraph->zone());
    SimdScalarLowering(mcgraph, &simplified, sig).LowerGraph();

    // SimdScalarLowering changes all v128 to 4 i32, so update the machine
    // signature for the call to LowerInt64.
    size_t return_count = 0;
    size_t param_count = 0;
    for (auto ret : sig->returns()) {
      return_count += ret == MachineRepresentation::kSimd128 ? 4 : 1;
    }
    for (auto param : sig->parameters()) {
      param_count += param == MachineRepresentation::kSimd128 ? 4 : 1;
    }

    Signature<MachineRepresentation>::Builder sig_builder(
        mcgraph->zone(), return_count, param_count);
    for (auto ret : sig->returns()) {
      if (ret == MachineRepresentation::kSimd128) {
        for (int i = 0; i < 4; ++i) {
          sig_builder.AddReturn(MachineRepresentation::kWord32);
        }
      } else {
        sig_builder.AddReturn(ret);
      }
    }
    for (auto param : sig->parameters()) {
      if (param == MachineRepresentation::kSimd128) {
        for (int i = 0; i < 4; ++i) {
          sig_builder.AddParam(MachineRepresentation::kWord32);
        }
      } else {
        sig_builder.AddParam(param);
      }
    }
    sig = sig_builder.Build();
  }

  builder.LowerInt64(sig);

  if (func_index >= FLAG_trace_wasm_ast_start &&
      func_index < FLAG_trace_wasm_ast_end) {
    PrintRawWasmCode(allocator, func_body, env->module, wasm::kPrintLocals);
  }
  return true;
}

Vector<const char> GetDebugName(Zone* zone, int index) {
  // TODO(herhut): Use name from module if available.
  constexpr int kBufferLength = 24;

  EmbeddedVector<char, kBufferLength> name_vector;
  int name_len = SNPrintF(name_vector, "wasm-function#%d", index);
  DCHECK(name_len > 0 && name_len < name_vector.length());

  char* index_name = zone->NewArray<char>(name_len);
  base::Memcpy(index_name, name_vector.begin(), name_len);
  return Vector<const char>(index_name, name_len);
}

}  // namespace

wasm::WasmCompilationResult ExecuteTurbofanWasmCompilation(
    wasm::WasmEngine* wasm_engine, wasm::CompilationEnv* env,
    const wasm::FunctionBody& func_body, int func_index, Counters* counters,
    wasm::WasmFeatures* detected) {
  TRACE_EVENT2(TRACE_DISABLED_BY_DEFAULT("v8.wasm.detailed"),
               "wasm.CompileTopTier", "func_index", func_index, "body_size",
               func_body.end - func_body.start);
  Zone zone(wasm_engine->allocator(), ZONE_NAME, kCompressGraphZone);
  MachineGraph* mcgraph = zone.New<MachineGraph>(
      zone.New<Graph>(&zone), zone.New<CommonOperatorBuilder>(&zone),
      zone.New<MachineOperatorBuilder>(
          &zone, MachineType::PointerRepresentation(),
          InstructionSelector::SupportedMachineOperatorFlags(),
          InstructionSelector::AlignmentRequirements()));

  OptimizedCompilationInfo info(GetDebugName(&zone, func_index), &zone,
                                CodeKind::WASM_FUNCTION);
  if (env->runtime_exception_support) {
    info.set_wasm_runtime_exception_support();
  }

  if (info.trace_turbo_json()) {
    TurboCfgFile tcf;
    tcf << AsC1VCompilation(&info);
  }

  NodeOriginTable* node_origins =
      info.trace_turbo_json() ? zone.New<NodeOriginTable>(mcgraph->graph())
                              : nullptr;
  SourcePositionTable* source_positions =
      mcgraph->zone()->New<SourcePositionTable>(mcgraph->graph());

  std::vector<WasmLoopInfo> loop_infos;

  if (!BuildGraphForWasmFunction(wasm_engine->allocator(), env, func_body,
                                 func_index, detected, mcgraph, &loop_infos,
                                 node_origins, source_positions)) {
    return wasm::WasmCompilationResult{};
  }

  if (node_origins) {
    node_origins->AddDecorator();
  }

  // Run the compiler pipeline to generate machine code.
  auto call_descriptor = GetWasmCallDescriptor(&zone, func_body.sig);
  if (mcgraph->machine()->Is32()) {
    call_descriptor = GetI32WasmCallDescriptor(&zone, call_descriptor);
  }

  if (ContainsSimd(func_body.sig) &&
      (!CpuFeatures::SupportsWasmSimd128() || env->lower_simd)) {
    call_descriptor = GetI32WasmCallDescriptorForSimd(&zone, call_descriptor);
  }

  Pipeline::GenerateCodeForWasmFunction(
      &info, wasm_engine, mcgraph, call_descriptor, source_positions,
      node_origins, func_body, env->module, func_index, &loop_infos);

  if (counters) {
    counters->wasm_compile_function_peak_memory_bytes()->AddSample(
        static_cast<int>(mcgraph->graph()->zone()->allocation_size()));
  }
  auto result = info.ReleaseWasmCompilationResult();
  CHECK_NOT_NULL(result);  // Compilation expected to succeed.
  DCHECK_EQ(wasm::ExecutionTier::kTurbofan, result->result_tier);
  return std::move(*result);
}

namespace {
// Helper for allocating either an GP or FP reg, or the next stack slot.
class LinkageLocationAllocator {
 public:
  template <size_t kNumGpRegs, size_t kNumFpRegs>
  constexpr LinkageLocationAllocator(const Register (&gp)[kNumGpRegs],
                                     const DoubleRegister (&fp)[kNumFpRegs],
                                     int slot_offset)
      : allocator_(wasm::LinkageAllocator(gp, fp)), slot_offset_(slot_offset) {}

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
    int index = -1 - (slot_offset_ + allocator_.NextStackSlot(rep));
    return LinkageLocation::ForCallerFrameSlot(index, type);
  }

  int NumStackSlots() const { return allocator_.NumStackSlots(); }
  void EndSlotArea() { allocator_.EndSlotArea(); }

 private:
  wasm::LinkageAllocator allocator_;
  // Since params and returns are in different stack frames, we must allocate
  // them separately. Parameter slots don't need an offset, but return slots
  // must be offset to just before the param slots, using this |slot_offset_|.
  int slot_offset_;
};
}  // namespace

// General code uses the above configuration data.
CallDescriptor* GetWasmCallDescriptor(
    Zone* zone, const wasm::FunctionSig* fsig,
    WasmGraphBuilder::UseRetpoline use_retpoline, WasmCallKind call_kind,
    bool need_frame_state) {
  // The extra here is to accomodate the instance object as first parameter
  // and, when specified, the additional callable.
  bool extra_callable_param =
      call_kind == kWasmImportWrapper || call_kind == kWasmCapiFunction;
  int extra_params = extra_callable_param ? 2 : 1;
  LocationSignature::Builder locations(zone, fsig->return_count(),
                                       fsig->parameter_count() + extra_params);

  // Add register and/or stack parameter(s).
  LinkageLocationAllocator params(
      wasm::kGpParamRegisters, wasm::kFpParamRegisters, 0 /* no slot offset */);

  // The instance object.
  locations.AddParam(params.Next(MachineRepresentation::kTaggedPointer));
  const size_t param_offset = 1;  // Actual params start here.

  // Parameters are separated into two groups (first all untagged, then all
  // tagged parameters). This allows for easy iteration of tagged parameters
  // during frame iteration.
  const size_t parameter_count = fsig->parameter_count();
  for (size_t i = 0; i < parameter_count; i++) {
    MachineRepresentation param = fsig->GetParam(i).machine_representation();
    // Skip tagged parameters (e.g. any-ref).
    if (IsAnyTagged(param)) continue;
    auto l = params.Next(param);
    locations.AddParamAt(i + param_offset, l);
  }

  // End the untagged area, so tagged slots come after.
  params.EndSlotArea();

  for (size_t i = 0; i < parameter_count; i++) {
    MachineRepresentation param = fsig->GetParam(i).machine_representation();
    // Skip untagged parameters.
    if (!IsAnyTagged(param)) continue;
    auto l = params.Next(param);
    locations.AddParamAt(i + param_offset, l);
  }

  // Import call wrappers have an additional (implicit) parameter, the callable.
  // For consistency with JS, we use the JSFunction register.
  if (extra_callable_param) {
    locations.AddParam(LinkageLocation::ForRegister(
        kJSFunctionRegister.code(), MachineType::TaggedPointer()));
  }

  int parameter_slots = AddArgumentPaddingSlots(params.NumStackSlots());

  // Add return location(s).
  LinkageLocationAllocator rets(wasm::kGpReturnRegisters,
                                wasm::kFpReturnRegisters, parameter_slots);

  const int return_count = static_cast<int>(locations.return_count_);
  for (int i = 0; i < return_count; i++) {
    MachineRepresentation ret = fsig->GetReturn(i).machine_representation();
    locations.AddReturn(rets.Next(ret));
  }

  int return_slots = rets.NumStackSlots();

  const RegList kCalleeSaveRegisters = 0;
  const RegList kCalleeSaveFPRegisters = 0;

  // The target for wasm calls is always a code object.
  MachineType target_type = MachineType::Pointer();
  LinkageLocation target_loc = LinkageLocation::ForAnyRegister(target_type);

  CallDescriptor::Kind descriptor_kind;
  if (call_kind == kWasmFunction) {
    descriptor_kind = CallDescriptor::kCallWasmFunction;
  } else if (call_kind == kWasmImportWrapper) {
    descriptor_kind = CallDescriptor::kCallWasmImportWrapper;
  } else {
    DCHECK_EQ(call_kind, kWasmCapiFunction);
    descriptor_kind = CallDescriptor::kCallWasmCapiFunction;
  }

  CallDescriptor::Flags flags =
      use_retpoline ? CallDescriptor::kRetpoline
                    : need_frame_state ? CallDescriptor::kNeedsFrameState
                                       : CallDescriptor::kNoFlags;
  return zone->New<CallDescriptor>(       // --
      descriptor_kind,                    // kind
      target_type,                        // target MachineType
      target_loc,                         // target location
      locations.Build(),                  // location_sig
      parameter_slots,                    // parameter slot count
      compiler::Operator::kNoProperties,  // properties
      kCalleeSaveRegisters,               // callee-saved registers
      kCalleeSaveFPRegisters,             // callee-saved fp regs
      flags,                              // flags
      "wasm-call",                        // debug name
      StackArgumentOrder::kDefault,       // order of the arguments in the stack
      0,                                  // allocatable registers
      return_slots);                      // return slot count
}

namespace {
CallDescriptor* ReplaceTypeInCallDescriptorWith(
    Zone* zone, const CallDescriptor* call_descriptor, size_t num_replacements,
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
    return const_cast<CallDescriptor*>(call_descriptor);
  }

  LocationSignature::Builder locations(zone, return_count, parameter_count);

  // The last parameter may be the special callable parameter. In that case we
  // have to preserve it as the last parameter, i.e. we allocate it in the new
  // location signature again in the same register.
  bool has_callable_param =
      (call_descriptor->GetInputLocation(call_descriptor->InputCount() - 1) ==
       LinkageLocation::ForRegister(kJSFunctionRegister.code(),
                                    MachineType::TaggedPointer()));
  LinkageLocationAllocator params(
      wasm::kGpParamRegisters, wasm::kFpParamRegisters, 0 /* no slot offset */);

  for (size_t i = 0, e = call_descriptor->ParameterCount() -
                         (has_callable_param ? 1 : 0);
       i < e; i++) {
    if (call_descriptor->GetParameterType(i) == input_type) {
      for (size_t j = 0; j < num_replacements; j++) {
        locations.AddParam(params.Next(output_type));
      }
    } else {
      locations.AddParam(
          params.Next(call_descriptor->GetParameterType(i).representation()));
    }
  }
  if (has_callable_param) {
    locations.AddParam(LinkageLocation::ForRegister(
        kJSFunctionRegister.code(), MachineType::TaggedPointer()));
  }

  int parameter_slots = AddArgumentPaddingSlots(params.NumStackSlots());

  LinkageLocationAllocator rets(wasm::kGpReturnRegisters,
                                wasm::kFpReturnRegisters, parameter_slots);

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

  int return_slots = rets.NumStackSlots();

  return zone->New<CallDescriptor>(               // --
      call_descriptor->kind(),                    // kind
      call_descriptor->GetInputType(0),           // target MachineType
      call_descriptor->GetInputLocation(0),       // target location
      locations.Build(),                          // location_sig
      parameter_slots,                            // parameter slot count
      call_descriptor->properties(),              // properties
      call_descriptor->CalleeSavedRegisters(),    // callee-saved registers
      call_descriptor->CalleeSavedFPRegisters(),  // callee-saved fp regs
      call_descriptor->flags(),                   // flags
      call_descriptor->debug_name(),              // debug name
      call_descriptor->GetStackArgumentOrder(),   // stack order
      call_descriptor->AllocatableRegisters(),    // allocatable registers
      return_slots);                              // return slot count
}
}  // namespace

CallDescriptor* GetI32WasmCallDescriptor(
    Zone* zone, const CallDescriptor* call_descriptor) {
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
  // Relocation info required to serialize {WasmCode} for proper functions.
  options.record_reloc_info_for_serialization = true;
  options.enable_root_array_delta_access = false;
  return options;
}

AssemblerOptions WasmStubAssemblerOptions() {
  AssemblerOptions options;
  // Relocation info not necessary because stubs are not serialized.
  options.record_reloc_info_for_serialization = false;
  options.enable_root_array_delta_access = false;
  return options;
}

#undef FATAL_UNSUPPORTED_OPCODE
#undef WASM_INSTANCE_OBJECT_SIZE
#undef LOAD_INSTANCE_FIELD
#undef LOAD_MUTABLE_INSTANCE_FIELD
#undef LOAD_ROOT

}  // namespace compiler
}  // namespace internal
}  // namespace v8
