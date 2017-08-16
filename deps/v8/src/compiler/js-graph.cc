// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-graph.h"

#include "src/code-stubs.h"
#include "src/compiler/node-properties.h"
#include "src/compiler/typer.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define CACHED(name, expr) \
  cached_nodes_[name] ? cached_nodes_[name] : (cached_nodes_[name] = (expr))

Node* JSGraph::AllocateInNewSpaceStubConstant() {
  return CACHED(kAllocateInNewSpaceStubConstant,
                HeapConstant(isolate()->builtins()->AllocateInNewSpace()));
}

Node* JSGraph::AllocateInOldSpaceStubConstant() {
  return CACHED(kAllocateInOldSpaceStubConstant,
                HeapConstant(isolate()->builtins()->AllocateInOldSpace()));
}

Node* JSGraph::ToNumberBuiltinConstant() {
  return CACHED(kToNumberBuiltinConstant,
                HeapConstant(isolate()->builtins()->ToNumber()));
}

Node* JSGraph::CEntryStubConstant(int result_size, SaveFPRegsMode save_doubles,
                                  ArgvMode argv_mode, bool builtin_exit_frame) {
  if (save_doubles == kDontSaveFPRegs && argv_mode == kArgvOnStack) {
    DCHECK(result_size >= 1 && result_size <= 3);
    if (!builtin_exit_frame) {
      CachedNode key;
      if (result_size == 1) {
        key = kCEntryStub1Constant;
      } else if (result_size == 2) {
        key = kCEntryStub2Constant;
      } else {
        DCHECK(result_size == 3);
        key = kCEntryStub3Constant;
      }
      return CACHED(
          key, HeapConstant(CEntryStub(isolate(), result_size, save_doubles,
                                       argv_mode, builtin_exit_frame)
                                .GetCode()));
    }
    CachedNode key = builtin_exit_frame
                         ? kCEntryStub1WithBuiltinExitFrameConstant
                         : kCEntryStub1Constant;
    return CACHED(key,
                  HeapConstant(CEntryStub(isolate(), result_size, save_doubles,
                                          argv_mode, builtin_exit_frame)
                                   .GetCode()));
  }
  CEntryStub stub(isolate(), result_size, save_doubles, argv_mode,
                  builtin_exit_frame);
  return HeapConstant(stub.GetCode());
}

Node* JSGraph::EmptyFixedArrayConstant() {
  return CACHED(kEmptyFixedArrayConstant,
                HeapConstant(factory()->empty_fixed_array()));
}

Node* JSGraph::EmptyStringConstant() {
  return CACHED(kEmptyStringConstant, HeapConstant(factory()->empty_string()));
}

Node* JSGraph::FixedArrayMapConstant() {
  return CACHED(kFixedArrayMapConstant,
                HeapConstant(factory()->fixed_array_map()));
}

Node* JSGraph::FixedDoubleArrayMapConstant() {
  return CACHED(kFixedDoubleArrayMapConstant,
                HeapConstant(factory()->fixed_double_array_map()));
}

Node* JSGraph::HeapNumberMapConstant() {
  return CACHED(kHeapNumberMapConstant,
                HeapConstant(factory()->heap_number_map()));
}

Node* JSGraph::OptimizedOutConstant() {
  return CACHED(kOptimizedOutConstant,
                HeapConstant(factory()->optimized_out()));
}

Node* JSGraph::StaleRegisterConstant() {
  return CACHED(kStaleRegisterConstant,
                HeapConstant(factory()->stale_register()));
}

Node* JSGraph::UndefinedConstant() {
  return CACHED(kUndefinedConstant, HeapConstant(factory()->undefined_value()));
}


Node* JSGraph::TheHoleConstant() {
  return CACHED(kTheHoleConstant, HeapConstant(factory()->the_hole_value()));
}


Node* JSGraph::TrueConstant() {
  return CACHED(kTrueConstant, HeapConstant(factory()->true_value()));
}


Node* JSGraph::FalseConstant() {
  return CACHED(kFalseConstant, HeapConstant(factory()->false_value()));
}


Node* JSGraph::NullConstant() {
  return CACHED(kNullConstant, HeapConstant(factory()->null_value()));
}


Node* JSGraph::ZeroConstant() {
  return CACHED(kZeroConstant, NumberConstant(0.0));
}

Node* JSGraph::OneConstant() {
  return CACHED(kOneConstant, NumberConstant(1.0));
}


Node* JSGraph::NaNConstant() {
  return CACHED(kNaNConstant,
                NumberConstant(std::numeric_limits<double>::quiet_NaN()));
}


Node* JSGraph::HeapConstant(Handle<HeapObject> value) {
  Node** loc = cache_.FindHeapConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->HeapConstant(value));
  }
  return *loc;
}


Node* JSGraph::Constant(Handle<Object> value) {
  // Dereference the handle to determine if a number constant or other
  // canonicalized node can be used.
  if (value->IsNumber()) {
    return Constant(value->Number());
  } else if (value->IsUndefined(isolate())) {
    return UndefinedConstant();
  } else if (value->IsTrue(isolate())) {
    return TrueConstant();
  } else if (value->IsFalse(isolate())) {
    return FalseConstant();
  } else if (value->IsNull(isolate())) {
    return NullConstant();
  } else if (value->IsTheHole(isolate())) {
    return TheHoleConstant();
  } else {
    return HeapConstant(Handle<HeapObject>::cast(value));
  }
}


Node* JSGraph::Constant(double value) {
  if (bit_cast<int64_t>(value) == bit_cast<int64_t>(0.0)) return ZeroConstant();
  if (bit_cast<int64_t>(value) == bit_cast<int64_t>(1.0)) return OneConstant();
  return NumberConstant(value);
}


Node* JSGraph::Constant(int32_t value) {
  if (value == 0) return ZeroConstant();
  if (value == 1) return OneConstant();
  return NumberConstant(value);
}

Node* JSGraph::Constant(uint32_t value) {
  if (value == 0) return ZeroConstant();
  if (value == 1) return OneConstant();
  return NumberConstant(value);
}

Node* JSGraph::Int32Constant(int32_t value) {
  Node** loc = cache_.FindInt32Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Int32Constant(value));
  }
  return *loc;
}


Node* JSGraph::Int64Constant(int64_t value) {
  Node** loc = cache_.FindInt64Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Int64Constant(value));
  }
  return *loc;
}

Node* JSGraph::RelocatableInt32Constant(int32_t value, RelocInfo::Mode rmode) {
  Node** loc = cache_.FindRelocatableInt32Constant(
      value, static_cast<RelocInfoMode>(rmode));
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->RelocatableInt32Constant(value, rmode));
  }
  return *loc;
}

Node* JSGraph::RelocatableInt64Constant(int64_t value, RelocInfo::Mode rmode) {
  Node** loc = cache_.FindRelocatableInt64Constant(
      value, static_cast<RelocInfoMode>(rmode));
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->RelocatableInt64Constant(value, rmode));
  }
  return *loc;
}

Node* JSGraph::RelocatableIntPtrConstant(intptr_t value,
                                         RelocInfo::Mode rmode) {
  return kPointerSize == 8
             ? RelocatableInt64Constant(value, rmode)
             : RelocatableInt32Constant(static_cast<int>(value), rmode);
}

Node* JSGraph::NumberConstant(double value) {
  Node** loc = cache_.FindNumberConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->NumberConstant(value));
  }
  return *loc;
}


Node* JSGraph::Float32Constant(float value) {
  Node** loc = cache_.FindFloat32Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Float32Constant(value));
  }
  return *loc;
}


Node* JSGraph::Float64Constant(double value) {
  Node** loc = cache_.FindFloat64Constant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->Float64Constant(value));
  }
  return *loc;
}

Node* JSGraph::PointerConstant(intptr_t value) {
  Node** loc = cache_.FindPointerConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->PointerConstant(value));
  }
  return *loc;
}

Node* JSGraph::ExternalConstant(ExternalReference reference) {
  Node** loc = cache_.FindExternalConstant(reference);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->ExternalConstant(reference));
  }
  return *loc;
}


Node* JSGraph::ExternalConstant(Runtime::FunctionId function_id) {
  return ExternalConstant(ExternalReference(function_id, isolate()));
}

Node* JSGraph::EmptyStateValues() {
  return CACHED(kEmptyStateValues, graph()->NewNode(common()->StateValues(
                                       0, SparseInputMask::Dense())));
}

Node* JSGraph::SingleDeadTypedStateValues() {
  return CACHED(kSingleDeadTypedStateValues,
                graph()->NewNode(common()->TypedStateValues(
                    new (graph()->zone()->New(sizeof(ZoneVector<MachineType>)))
                        ZoneVector<MachineType>(0, graph()->zone()),
                    SparseInputMask(SparseInputMask::kEndMarker << 1))));
}

Node* JSGraph::Dead() {
  return CACHED(kDead, graph()->NewNode(common()->Dead()));
}


void JSGraph::GetCachedNodes(NodeVector* nodes) {
  cache_.GetCachedNodes(nodes);
  for (size_t i = 0; i < arraysize(cached_nodes_); i++) {
    if (Node* node = cached_nodes_[i]) {
      if (!node->IsDead()) nodes->push_back(node);
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
