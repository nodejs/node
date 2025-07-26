// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef V8_ENABLE_MAGLEV

#include "src/maglev/maglev-ir.h"
#include "test/unittests/maglev/maglev-test.h"

namespace v8 {
namespace internal {
namespace maglev {

static std::initializer_list<NodeType> kAllNodeTypes{
#define TYPE(Name, _) NodeType::k##Name,
    NODE_TYPE_LIST(TYPE)
#undef TYPE
};

TEST_F(MaglevTest, NodeTypeSmokeTests) {
  // A couple of quick sanity checks that the type inference works.
  CHECK(NodeTypeIs(NodeType::kNumberOrBoolean, NodeType::kNumberOrOddball));
  CHECK(!NodeTypeIs(NodeType::kNumberOrOddball, NodeType::kNumberOrBoolean));

  CHECK_EQ(CombineType(NodeType::kNumberOrBoolean, NodeType::kNumberOrOddball),
           NodeType::kNumberOrBoolean);
  CHECK_EQ(
      IntersectType(NodeType::kNumberOrBoolean, NodeType::kNumberOrOddball),
      NodeType::kNumberOrOddball);

  CHECK(!NodeTypeIs(NodeType::kStringWrapper, NodeType::kName));
}

TEST_F(MaglevTest, EmptyTypeIsAnything) {
  for (NodeType a : kAllNodeTypes) {
    CHECK(NodeTypeIs(EmptyNodeType(), a));
  }
}

// Allow-list of types we have consciously omitted from the combined types since
// they are not needed often.
std::unordered_set<NodeType> kMissingEntries{
    // HeapNumberOrOddball
    CombineType(NodeType::kNumberOrOddball, NodeType::kAnyHeapObject),
    // BooleanOrHeapNumber
    CombineType(NodeType::kNumberOrBoolean, NodeType::kAnyHeapObject),
};

// The missing node types must be inhabited.
TEST_F(MaglevTest, NodeTypeMissingEntriesExist) {
  for (NodeType missing : kMissingEntries) {
    CHECK(!IsEmptyNodeType(missing));
  }
}

// Ensure StaticTypeForConstant is consistent with actual objects.
TEST_F(MaglevTest, ConstantNodeTypeApproximationIsConsistent) {
  for (auto idx = RootIndex::kFirstRoot; idx <= RootIndex::kLastRoot; ++idx) {
    Tagged<Object> obj = isolate()->roots_table().slot(idx).load(isolate());
    if (obj.ptr() == kNullAddress || !obj.IsHeapObject()) continue;
    compiler::HeapObjectRef ref = MakeRef(broker(), Cast<HeapObject>(obj));
    NodeType t = StaticTypeForConstant(broker(), ref);
    CHECK(!IsEmptyNodeType(t));
    for (NodeType a : kAllNodeTypes) {
      bool is_instance = IsInstanceOfNodeType(ref.map(broker()), a, broker());
      bool is_subtype = NodeTypeIs(t, a);
      CHECK_IMPLIES(is_subtype, is_instance);
      CHECK_IMPLIES(!is_instance, !is_subtype);
    }
  }
}

// Ensure StaticTypeForMap is consistent with actual maps.
TEST_F(MaglevTest, NodeTypeApproximationIsConsistent) {
  for (auto idx = RootIndex::kFirstRoot; idx <= RootIndex::kLastRoot; ++idx) {
    Tagged<Object> obj = isolate()->roots_table().slot(idx).load(isolate());
    if (obj.ptr() == kNullAddress || !IsMap(obj)) continue;
    Tagged<Map> map = Cast<Map>(obj);
    compiler::MapRef map_ref = MakeRef(broker(), map);

    for (NodeType a : kAllNodeTypes) {
      bool is_instance = IsInstanceOfNodeType(map_ref, a, broker());
      bool is_subtype = NodeTypeIs(StaticTypeForMap(map_ref, broker()), a);
      CHECK_IMPLIES(is_subtype, is_instance);
      CHECK_IMPLIES(!is_instance, !is_subtype);
    }
  }
}

// Ensure CombineType is consistent with actual maps.
TEST_F(MaglevTest, NodeTypeCombineIsConsistent) {
  for (auto idx = RootIndex::kFirstRoot; idx <= RootIndex::kLastRoot; ++idx) {
    Tagged<Object> obj = isolate()->roots_table().slot(idx).load(isolate());
    if (obj.ptr() == kNullAddress || !IsMap(obj)) continue;
    Tagged<Map> map = Cast<Map>(obj);
    compiler::MapRef map_ref = MakeRef(broker(), map);

    for (NodeType a : kAllNodeTypes) {
      for (NodeType b : kAllNodeTypes) {
        NodeType combined_type = CombineType(a, b);
        bool map_is_a = IsInstanceOfNodeType(map_ref, a, broker());
        bool map_is_b = IsInstanceOfNodeType(map_ref, b, broker());
        bool is_instance =
            IsInstanceOfNodeType(map_ref, combined_type, broker());
        CHECK_EQ(is_instance, map_is_a && map_is_b);
        DCHECK_IMPLIES(IsEmptyNodeType(combined_type), !is_instance);
        DCHECK_IMPLIES(is_instance, !IsEmptyNodeType(combined_type));
      }
    }
  }
}

// Ensure IntersectType is consistent with actual maps.
TEST_F(MaglevTest, NodeTypeIntersectIsConsistent) {
  for (auto idx = RootIndex::kFirstRoot; idx <= RootIndex::kLastRoot; ++idx) {
    Tagged<Object> obj = isolate()->roots_table().slot(idx).load(isolate());
    if (obj.ptr() == kNullAddress || !IsMap(obj)) continue;
    Tagged<Map> map = Cast<Map>(obj);
    compiler::MapRef map_ref = MakeRef(broker(), map);

    for (NodeType a : kAllNodeTypes) {
      for (NodeType b : kAllNodeTypes) {
        NodeType join_type = IntersectType(a, b);
        bool map_is_a = IsInstanceOfNodeType(map_ref, a, broker());
        bool map_is_b = IsInstanceOfNodeType(map_ref, b, broker());
        CHECK_IMPLIES(map_is_a || map_is_b,
                      IsInstanceOfNodeType(map_ref, join_type, broker()));
        CHECK_IMPLIES(!IsInstanceOfNodeType(map_ref, join_type, broker()),
                      !(map_is_a && map_is_b));
      }
    }
  }
}

}  // namespace maglev
}  // namespace internal
}  // namespace v8

#endif  // V8_ENABLE_MAGLEV
