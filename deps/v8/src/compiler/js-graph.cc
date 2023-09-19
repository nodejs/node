// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/js-graph.h"

#include "src/codegen/code-factory.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {
namespace compiler {

#define GET_CACHED_FIELD(ptr, expr) (*(ptr)) ? *(ptr) : (*(ptr) = (expr))

#define DEFINE_GETTER(name, expr) \
  Node* JSGraph::name() { return GET_CACHED_FIELD(&name##_, expr); }

Node* JSGraph::CEntryStubConstant(int result_size, ArgvMode argv_mode,
                                  bool builtin_exit_frame) {
  if (argv_mode == ArgvMode::kStack) {
    DCHECK(result_size >= 1 && result_size <= 3);
    if (!builtin_exit_frame) {
      Node** ptr = nullptr;
      if (result_size == 1) {
        ptr = &CEntryStub1Constant_;
      } else if (result_size == 2) {
        ptr = &CEntryStub2Constant_;
      } else {
        DCHECK_EQ(3, result_size);
        ptr = &CEntryStub3Constant_;
      }
      return GET_CACHED_FIELD(
          ptr, HeapConstant(CodeFactory::CEntry(
                   isolate(), result_size, argv_mode, builtin_exit_frame)));
    }
    Node** ptr = builtin_exit_frame ? &CEntryStub1WithBuiltinExitFrameConstant_
                                    : &CEntryStub1Constant_;
    return GET_CACHED_FIELD(
        ptr, HeapConstant(CodeFactory::CEntry(isolate(), result_size, argv_mode,
                                              builtin_exit_frame)));
  }
  return HeapConstant(CodeFactory::CEntry(isolate(), result_size, argv_mode,
                                          builtin_exit_frame));
}

Node* JSGraph::Constant(const ObjectRef& ref, JSHeapBroker* broker) {
  if (ref.IsSmi()) return Constant(ref.AsSmi());
  if (ref.IsHeapNumber()) {
    return Constant(ref.AsHeapNumber().value());
  }
  OddballType oddball_type =
      ref.AsHeapObject().GetHeapObjectType(broker).oddball_type();
  ReadOnlyRoots roots(isolate());
  if (oddball_type == OddballType::kUndefined) {
    DCHECK(ref.object()->IsUndefined(roots));
    return UndefinedConstant();
  } else if (oddball_type == OddballType::kNull) {
    DCHECK(ref.object()->IsNull(roots));
    return NullConstant();
  } else if (oddball_type == OddballType::kHole) {
    DCHECK(ref.object()->IsTheHole(roots));
    return TheHoleConstant();
  } else if (oddball_type == OddballType::kBoolean) {
    if (ref.object()->IsTrue(roots)) {
      return TrueConstant();
    } else {
      DCHECK(ref.object()->IsFalse(roots));
      return FalseConstant();
    }
  } else {
    return HeapConstant(ref.AsHeapObject().object());
  }
}

Node* JSGraph::Constant(double value) {
  if (base::bit_cast<int64_t>(value) == base::bit_cast<int64_t>(0.0))
    return ZeroConstant();
  if (base::bit_cast<int64_t>(value) == base::bit_cast<int64_t>(1.0))
    return OneConstant();
  return NumberConstant(value);
}

Node* JSGraph::NumberConstant(double value) {
  Node** loc = cache_.FindNumberConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->NumberConstant(value));
  }
  return *loc;
}

Node* JSGraph::HeapConstant(Handle<HeapObject> value) {
  Node** loc = cache_.FindHeapConstant(value);
  if (*loc == nullptr) {
    *loc = graph()->NewNode(common()->HeapConstant(value));
  }
  return *loc;
}

void JSGraph::GetCachedNodes(NodeVector* nodes) {
  cache_.GetCachedNodes(nodes);
#define DO_CACHED_FIELD(name) \
  if (name##_) nodes->push_back(name##_);

  CACHED_GLOBAL_LIST(DO_CACHED_FIELD)
  CACHED_CENTRY_LIST(DO_CACHED_FIELD)
#undef DO_CACHED_FIELD
}

DEFINE_GETTER(AllocateInYoungGenerationStubConstant,
              HeapConstant(BUILTIN_CODE(isolate(), AllocateInYoungGeneration)))

DEFINE_GETTER(AllocateRegularInYoungGenerationStubConstant,
              HeapConstant(BUILTIN_CODE(isolate(),
                                        AllocateRegularInYoungGeneration)))

DEFINE_GETTER(AllocateInOldGenerationStubConstant,
              HeapConstant(BUILTIN_CODE(isolate(), AllocateInOldGeneration)))

DEFINE_GETTER(AllocateRegularInOldGenerationStubConstant,
              HeapConstant(BUILTIN_CODE(isolate(),
                                        AllocateRegularInOldGeneration)))

DEFINE_GETTER(ArrayConstructorStubConstant,
              HeapConstant(BUILTIN_CODE(isolate(), ArrayConstructorImpl)))

DEFINE_GETTER(BigIntMapConstant, HeapConstant(factory()->bigint_map()))

DEFINE_GETTER(BooleanMapConstant, HeapConstant(factory()->boolean_map()))

DEFINE_GETTER(ToNumberBuiltinConstant,
              HeapConstant(BUILTIN_CODE(isolate(), ToNumber)))

DEFINE_GETTER(PlainPrimitiveToNumberBuiltinConstant,
              HeapConstant(BUILTIN_CODE(isolate(), PlainPrimitiveToNumber)))

DEFINE_GETTER(EmptyFixedArrayConstant,
              HeapConstant(factory()->empty_fixed_array()))

DEFINE_GETTER(EmptyStringConstant, HeapConstant(factory()->empty_string()))

DEFINE_GETTER(FixedArrayMapConstant, HeapConstant(factory()->fixed_array_map()))

DEFINE_GETTER(PropertyArrayMapConstant,
              HeapConstant(factory()->property_array_map()))

DEFINE_GETTER(FixedDoubleArrayMapConstant,
              HeapConstant(factory()->fixed_double_array_map()))

DEFINE_GETTER(WeakFixedArrayMapConstant,
              HeapConstant(factory()->weak_fixed_array_map()))

DEFINE_GETTER(HeapNumberMapConstant, HeapConstant(factory()->heap_number_map()))

DEFINE_GETTER(OptimizedOutConstant, HeapConstant(factory()->optimized_out()))

DEFINE_GETTER(StaleRegisterConstant, HeapConstant(factory()->stale_register()))

DEFINE_GETTER(UndefinedConstant, HeapConstant(factory()->undefined_value()))

DEFINE_GETTER(TheHoleConstant, HeapConstant(factory()->the_hole_value()))

DEFINE_GETTER(TrueConstant, HeapConstant(factory()->true_value()))

DEFINE_GETTER(FalseConstant, HeapConstant(factory()->false_value()))

DEFINE_GETTER(NullConstant, HeapConstant(factory()->null_value()))

DEFINE_GETTER(ZeroConstant, NumberConstant(0.0))

DEFINE_GETTER(MinusZeroConstant, NumberConstant(-0.0))

DEFINE_GETTER(OneConstant, NumberConstant(1.0))

DEFINE_GETTER(MinusOneConstant, NumberConstant(-1.0))

DEFINE_GETTER(NaNConstant,
              NumberConstant(std::numeric_limits<double>::quiet_NaN()))

DEFINE_GETTER(EmptyStateValues,
              graph()->NewNode(common()->StateValues(0,
                                                     SparseInputMask::Dense())))

DEFINE_GETTER(
    SingleDeadTypedStateValues,
    graph()->NewNode(common()->TypedStateValues(
        graph()->zone()->New<ZoneVector<MachineType>>(0, graph()->zone()),
        SparseInputMask(SparseInputMask::kEndMarker << 1))))

DEFINE_GETTER(ExternalObjectMapConstant,
              HeapConstant(factory()->external_map()))

#undef DEFINE_GETTER
#undef GET_CACHED_FIELD

}  // namespace compiler
}  // namespace internal
}  // namespace v8
