// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_MAGLEV

#include "src/compiler/js-heap-broker.h"
#include "src/execution/isolate.h"
#include "src/handles/handles.h"
#include "src/maglev/maglev-ir.h"
#include "test/unittests/test-utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace v8 {
namespace internal {
namespace maglev {

class MaglevTest : public TestWithNativeContextAndZone {
 public:
  MaglevTest();
  ~MaglevTest() override;

  compiler::JSHeapBroker* broker() { return &broker_; }

 private:
  compiler::JSHeapBroker broker_;
  compiler::JSHeapBrokerScopeForTesting broker_scope_;
  std::unique_ptr<PersistentHandlesScope> persistent_scope_;
  compiler::CurrentHeapBrokerScope current_broker_;
};

MaglevTest::MaglevTest()
    : TestWithNativeContextAndZone(kCompressGraphZone),
      broker_(isolate(), zone(), v8_flags.trace_heap_broker, CodeKind::MAGLEV),
      broker_scope_(&broker_, isolate(), zone()),
      current_broker_(&broker_) {
  // PersistentHandlesScope currently requires an active handle before it can
  // be opened and they can't be nested.
  // TODO(v8:13897): Remove once PersistentHandlesScopes can be opened
  // uncontionally.
  if (!PersistentHandlesScope::IsActive(isolate())) {
    Handle<Object> dummy(ReadOnlyRoots(isolate()->heap()).empty_string(),
                         isolate());
    persistent_scope_ = std::make_unique<PersistentHandlesScope>(isolate());
  }
  broker()->SetTargetNativeContextRef(isolate()->native_context());
}

MaglevTest::~MaglevTest() {
  if (persistent_scope_ != nullptr) {
    persistent_scope_->Detach();
  }
}

static std::initializer_list<NodeType> kAllNodeTypes{
#define TYPE(Name, _) NodeType::k##Name,
    NODE_TYPE_LIST(TYPE)
#undef TYPE
};

inline constexpr bool IsKnownNodeTypeConstant(NodeType type) {
  switch (type) {
#define CASE(Name, _) case NodeType::k##Name:
    NODE_TYPE_LIST(CASE)
#undef CASE
    return true;
  }
  return false;
}

// Ensures NodeTypes are closed under intersection modulo heap object bit.
TEST_F(MaglevTest, NodeTypeIsClosedUnderIntersect) {
  for (NodeType a : kAllNodeTypes) {
    CHECK(IsKnownNodeTypeConstant(a));
    for (NodeType b : kAllNodeTypes) {
      NodeType combined = IntersectType(a, b);
      // Somtimes the intersection keeps more information, e.g.
      // Intersect(HeapNumber,Boolean) is a NumberOrOddball|HeapObject
      NodeType combined_not_obj =
          NodeType(static_cast<int>(combined) &
                   ~static_cast<int>(NodeType::kAnyHeapObject));
      CHECK(IsKnownNodeTypeConstant(combined) ||
            IsKnownNodeTypeConstant(combined_not_obj));
    }
  }
}

// Ensure StaticTypeForMap is consistent with actual maps.
TEST_F(MaglevTest, NodeTypeApproximationIsConsistent) {
  for (auto idx = RootIndex::kFirstRoot; idx <= RootIndex::kLastRoot; ++idx) {
    Tagged<Object> obj = isolate()->roots_table().slot(idx).load(isolate());
    if (obj.ptr() == kNullAddress || !IsMap(obj)) continue;
    Tagged<Map> map = Map::cast(obj);
    compiler::MapRef map_ref = MakeRef(broker(), map);

    for (NodeType a : kAllNodeTypes) {
      bool isInstance = IsInstanceOfNodeType(map_ref, a, broker());
      bool isSubtype = NodeTypeIs(StaticTypeForMap(map_ref), a);
      CHECK_IMPLIES(isSubtype, isInstance);
      CHECK_IMPLIES(!isInstance, !isSubtype);
    }
  }
}

// Ensure CombineType is consistent with actual maps.
TEST_F(MaglevTest, NodeTypeCombineIsConsistent) {
  for (auto idx = RootIndex::kFirstRoot; idx <= RootIndex::kLastRoot; ++idx) {
    Tagged<Object> obj = isolate()->roots_table().slot(idx).load(isolate());
    if (obj.ptr() == kNullAddress || !IsMap(obj)) continue;
    Tagged<Map> map = Map::cast(obj);
    compiler::MapRef map_ref = MakeRef(broker(), map);

    for (NodeType a : kAllNodeTypes) {
      for (NodeType b : kAllNodeTypes) {
        NodeType combined_type = CombineType(a, b);

        bool mapIsA = IsInstanceOfNodeType(map_ref, a, broker());
        bool mapIsB = IsInstanceOfNodeType(map_ref, b, broker());
        CHECK_EQ(IsInstanceOfNodeType(map_ref, combined_type, broker()),
                 mapIsA && mapIsB);
      }
    }
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
