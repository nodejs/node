// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_MAGLEV_MAGLEV_NODE_TYPE_H_
#define V8_MAGLEV_MAGLEV_NODE_TYPE_H_

#include <ostream>
#include <type_traits>

#include "src/base/bits.h"
#include "src/base/logging.h"
#include "src/compiler/heap-refs.h"

namespace v8::internal::maglev {

// TODO(olivf): Rename Unknown to Any.

/* Every object should belong to exactly one of these.*/
#define LEAF_NODE_TYPE_LIST(V)                                          \
  V(Smi, (1 << 0))                                                      \
  V(HeapNumber, (1 << 1))                                               \
  V(Null, (1 << 2))                                                     \
  V(Undefined, (1 << 3))                                                \
  V(Boolean, (1 << 4))                                                  \
  V(Symbol, (1 << 5))                                                   \
  V(InternalizedString, (1 << 6))                                       \
  V(OtherString, (1 << 7))                                              \
                                                                        \
  V(Context, (1 << 8))                                                  \
  V(StringWrapper, (1 << 9))                                            \
  V(JSArray, (1 << 10))                                                 \
  V(JSFunction, (1 << 11))                                              \
  V(OtherCallable, (1 << 12))                                           \
  V(JSDataView, (1 << 13))                                              \
  /* Only "normal" generator objects. Async generator objects and async \
   * functions are not JSGeneratorObject. */                            \
  V(JSGeneratorObject, (1 << 14))                                       \
  V(OtherHeapObject, (1 << 15))                                         \
  V(OtherJSReceiver, (1 << 16))                                         \
  V(BigInt, (1 << 17))

#define COUNT(...) +1
static constexpr int kNumberOfLeafNodeTypes = 0 LEAF_NODE_TYPE_LIST(COUNT);
#undef COUNT

#define COMBINED_NODE_TYPE_LIST(V)                                       \
  /* A value which has all the above bits set */                         \
  V(Unknown, ((1 << kNumberOfLeafNodeTypes) - 1))                        \
  /* All bits cleared, useful as initial value when combining types. */  \
  V(None, 0)                                                             \
  V(Callable, kJSFunction | kOtherCallable)                              \
  V(NullOrUndefined, kNull | kUndefined)                                 \
  V(Oddball, kNullOrUndefined | kBoolean)                                \
  V(Number, kSmi | kHeapNumber)                                          \
  V(Numeric, kNumber | kBigInt)                                          \
  V(NumberOrBoolean, kNumber | kBoolean)                                 \
  V(NumberOrUndefined, kNumber | kUndefined)                             \
  V(NumberOrOddball, kNumber | kOddball)                                 \
  V(String, kInternalizedString | kOtherString)                          \
  V(StringOrStringWrapper, kString | kStringWrapper)                     \
  V(StringOrOddball, kString | kOddball)                                 \
  V(Name, kString | kSymbol)                                             \
  V(JSPrimitive,                                                         \
    kNumber | kString | kBoolean | kNullOrUndefined | kSymbol | kBigInt) \
  V(JSReceiver, kJSArray | kCallable | kStringWrapper | kJSDataView |    \
                    kJSGeneratorObject | kOtherJSReceiver)               \
  V(JSReceiverOrNull, kJSReceiver | kNull)                               \
  V(JSReceiverOrNullOrUndefined, kJSReceiver | kNullOrUndefined)         \
  V(NotUndetectable, kNumeric | kName | kBoolean)                        \
  V(AnyHeapObject, kUnknown - kSmi)

#define NODE_TYPE_LIST(V) \
  LEAF_NODE_TYPE_LIST(V)  \
  COMBINED_NODE_TYPE_LIST(V)

enum class NodeType : uint32_t {
#define DEFINE_NODE_TYPE(Name, Value) k##Name = Value,
  NODE_TYPE_LIST(DEFINE_NODE_TYPE)
#undef DEFINE_NODE_TYPE
};
using NodeTypeInt = std::underlying_type_t<NodeType>;

// Some leaf node types only exist to complement other leaf node types in a
// combined type. We never expect to see these as standalone types.
inline constexpr bool NodeTypeIsNeverStandalone(NodeType type) {
  switch (type) {
    // "Other" string types should be considered internal and never appear as
    // standalone leaf types.
    case NodeType::kOtherCallable:
    case NodeType::kOtherString:
      return true;
    default:
      return false;
  }
}

inline constexpr NodeType EmptyNodeType() { return static_cast<NodeType>(0); }

enum class NodeTypeIsVariant {
  kDefault,
  // Allows the lhs of `NodeTypeIs` to be kNone, in which case the result is
  // always true. Usually this is unexpected and caused by dead code.
  kAllowNone,
};

inline constexpr NodeType IntersectType(NodeType left, NodeType right) {
  DCHECK(!NodeTypeIsNeverStandalone(left));
  DCHECK(!NodeTypeIsNeverStandalone(right));
  return static_cast<NodeType>(static_cast<NodeTypeInt>(left) &
                               static_cast<NodeTypeInt>(right));
}
inline constexpr NodeType UnionType(NodeType left, NodeType right) {
  DCHECK(!NodeTypeIsNeverStandalone(left));
  DCHECK(!NodeTypeIsNeverStandalone(right));
  return static_cast<NodeType>(static_cast<NodeTypeInt>(left) |
                               static_cast<NodeTypeInt>(right));
}
inline constexpr NodeType RemoveType(NodeType left, NodeType right) {
  DCHECK(!NodeTypeIsNeverStandalone(left));
  DCHECK(!NodeTypeIsNeverStandalone(right));
  return static_cast<NodeType>(static_cast<NodeTypeInt>(left) &
                               ~static_cast<NodeTypeInt>(right));
}
// TODO(jgruber): Switch the default value back to kDefault once
// BranchResult/BuildBranchIfFoo can signal an Abort.
inline constexpr bool NodeTypeIs(
    NodeType type, NodeType to_check,
    NodeTypeIsVariant variant = NodeTypeIsVariant::kAllowNone) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  DCHECK(!NodeTypeIsNeverStandalone(to_check));
  if (variant != NodeTypeIsVariant::kAllowNone) {
    DCHECK_NE(type, NodeType::kNone);
  }
  NodeTypeInt right = static_cast<NodeTypeInt>(to_check);
  return (static_cast<NodeTypeInt>(type) & (~right)) == 0;
}
inline constexpr bool NodeTypeIsForPrinting(NodeType type, NodeType to_check) {
  // Like NodeTypeIs, but without the DCHECKs, since non-standalone types can be
  // part of larger types and we still need to print them individually, which
  // will trigger the DCHECKs of NodeTypeIs.
  NodeTypeInt right = static_cast<NodeTypeInt>(to_check);
  return (static_cast<NodeTypeInt>(type) & (~right)) == 0;
}
// TODO(dmercadier): similarly to NodeTypeIs, try to disallow kNone here because
// it can lead to confusing results.
inline constexpr bool NodeTypeCanBe(NodeType type, NodeType to_check,
                                    bool allow_standalone = false) {
  DCHECK_IMPLIES(!allow_standalone, !NodeTypeIsNeverStandalone(type));
  DCHECK_IMPLIES(!allow_standalone, !NodeTypeIsNeverStandalone(to_check));
  NodeTypeInt right = static_cast<NodeTypeInt>(to_check);
  return (static_cast<NodeTypeInt>(type) & (right)) != 0;
}

inline constexpr bool IsEmptyNodeType(NodeType type) {
  // No bits are set.
  return static_cast<int>(type) == 0;
}

static_assert(!NodeTypeCanBe(NodeType::kJSPrimitive, NodeType::kJSReceiver));


V8_EXPORT_PRIVATE NodeType StaticTypeForMap(compiler::MapRef map,
                                            compiler::JSHeapBroker* broker);

V8_EXPORT_PRIVATE NodeType StaticTypeForConstant(compiler::JSHeapBroker* broker,
                                                 compiler::ObjectRef ref);

V8_EXPORT_PRIVATE bool IsInstanceOfLeafNodeType(compiler::MapRef map,
                                                NodeType type,
                                                compiler::JSHeapBroker* broker);

V8_EXPORT_PRIVATE bool IsInstanceOfNodeType(compiler::MapRef map, NodeType type,
                                            compiler::JSHeapBroker* broker);

V8_EXPORT_PRIVATE std::ostream& operator<<(std::ostream& out,
                                           const NodeType& type);

// TODO(jgruber): Switch the default value back to kDefault once
// BranchResult/BuildBranchIfFoo can signal an Abort.
#define DEFINE_NODE_TYPE_CHECK(Type, _)                            \
  inline bool NodeTypeIs##Type(                                    \
      NodeType type,                                               \
      NodeTypeIsVariant variant = NodeTypeIsVariant::kAllowNone) { \
    return NodeTypeIs(type, NodeType::k##Type, variant);           \
  }
NODE_TYPE_LIST(DEFINE_NODE_TYPE_CHECK)
#undef DEFINE_NODE_TYPE_CHECK

inline bool NodeTypeMayBeNullOrUndefined(NodeType type) {
  return (static_cast<int>(type) &
          static_cast<int>(NodeType::kNullOrUndefined)) != 0;
}

}  // namespace v8::internal::maglev

#endif  // V8_MAGLEV_MAGLEV_NODE_TYPE_H_
