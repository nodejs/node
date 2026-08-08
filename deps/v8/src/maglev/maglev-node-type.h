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
#define LEAF_NODE_TYPE_LIST(V)                      \
  V(Smi, (1 << 0))                                  \
  V(HeapNumber, (1 << 1))                           \
  V(Null, (1 << 2))                                 \
  V(Undefined, (1 << 3))                            \
  V(Boolean, (1 << 4))                              \
  V(Symbol, (1 << 5))                               \
  /* String Venn diagram:                        */ \
  /* ┌String───────────────────────────────────┐ */ \
  /* │                            OtherString  │ */ \
  /* │┌InternalizedString───┐                  │ */ \
  /* ││                     │                  │ */ \
  /* ││OtherInternalized    │                  │ */ \
  /* ││String               │                  │ */ \
  /* │├─────────────────────┼─SeqOneByteString┐│ */ \
  /* ││                     │                 ││ */ \
  /* ││OtherSeqInternalized │ OtherSeqOneByte ││ */ \
  /* ││OneByteString        │ String          ││ */ \
  /* │├──────────────────┐  │                 ││ */ \
  /* ││ROSeqInternalized │  │                 ││ */ \
  /* ││OneByteString     │  │                 ││ */ \
  /* │└──────────────────┴──┴─────────────────┘│ */ \
  /* └─────────────────────────────────────────┘ */ \
  V(ROSeqInternalizedOneByteString, (1 << 6))       \
  V(OtherSeqInternalizedOneByteString, (1 << 7))    \
  V(OtherInternalizedString, (1 << 8))              \
  V(OtherSeqOneByteString, (1 << 9))                \
  V(OtherString, (1 << 10))                         \
                                                    \
  V(Context, (1 << 11))                             \
  V(StringWrapper, (1 << 12))                       \
  V(JSArray, (1 << 13))                             \
  V(JSFunction, (1 << 14))                          \
  V(OtherCallable, (1 << 15))                       \
  V(JSDataView, (1 << 16))                          \
  V(OtherHeapObject, (1 << 17))                     \
  V(OtherJSReceiver, (1 << 18))

#define COUNT(...) +1
static constexpr int kNumberOfLeafNodeTypes = 0 LEAF_NODE_TYPE_LIST(COUNT);
#undef COUNT

#define COMBINED_NODE_TYPE_LIST(V)                                          \
  /* A value which has all the above bits set */                            \
  V(Unknown, ((1 << kNumberOfLeafNodeTypes) - 1))                           \
  /* All bits cleared, useful as initial value when combining types. */     \
  V(None, 0)                                                                \
  V(Callable, kJSFunction | kOtherCallable)                                 \
  V(NullOrUndefined, kNull | kUndefined)                                    \
  V(Oddball, kNullOrUndefined | kBoolean)                                   \
  V(Number, kSmi | kHeapNumber)                                             \
  V(NumberOrBoolean, kNumber | kBoolean)                                    \
  V(NumberOrUndefined, kNumber | kUndefined)                                \
  V(NumberOrOddball, kNumber | kOddball)                                    \
  V(InternalizedString, kROSeqInternalizedOneByteString |                   \
                            kOtherSeqInternalizedOneByteString |            \
                            kOtherInternalizedString)                       \
  V(SeqOneByteString, kROSeqInternalizedOneByteString |                     \
                          kOtherSeqInternalizedOneByteString |              \
                          kOtherSeqOneByteString)                           \
  V(String, kInternalizedString | kSeqOneByteString | kOtherString)         \
  V(StringOrStringWrapper, kString | kStringWrapper)                        \
  V(StringOrOddball, kString | kOddball)                                    \
  V(Name, kString | kSymbol)                                                \
  /* TODO(jgruber): Add kBigInt and kSymbol once they exist. */             \
  V(JSPrimitive, kNumber | kString | kBoolean | kNullOrUndefined)           \
  V(JSReceiver,                                                             \
    kJSArray | kCallable | kStringWrapper | kJSDataView | kOtherJSReceiver) \
  V(JSReceiverOrNullOrUndefined, kJSReceiver | kNullOrUndefined)            \
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
    case NodeType::kOtherInternalizedString:
    case NodeType::kOtherSeqInternalizedOneByteString:
    case NodeType::kOtherSeqOneByteString:
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

inline constexpr bool NodeTypeIsUnstable(NodeType type) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  // Any type that can be a string might be unstable, if the string part of the
  // type is unstable.
  if (NodeTypeCanBe(type, NodeType::kString)) {
    // Extract out the string part of the node type.
    NodeType string_type = IntersectType(type, NodeType::kString);
    // RO-space strings are ok, since they can't change.
    if (string_type == NodeType::kROSeqInternalizedOneByteString) return false;
    // The generic internalized string type is ok, since it doesn't consider
    // seqness and internalized strings stay internalized.
    if (string_type == NodeType::kInternalizedString) return false;
    // The generic string type is ok, since it defines all strings.
    if (string_type == NodeType::kString) return false;
    // Otherwise, a string can get in-place externalized, or in-place converted
    // to thin if not already internalized, both of which lose seq-ness.
    // TODO(leszeks): We could probably consider byteness of internalized
    // strings to be stable, since we can't change byteness with in-place
    // externalization.
    return true;
  }
  // All other node types are stable.
  return false;
}
// Seq strings are unstable because they could be in-place converted to thin
// strings.
static_assert(NodeTypeIsUnstable(NodeType::kSeqOneByteString));
// Internalized strings are stable because they have to stay internalized.
static_assert(!NodeTypeIsUnstable(NodeType::kInternalizedString));
// RO internalized strings are stable because they are read-only.
static_assert(!NodeTypeIsUnstable(NodeType::kROSeqInternalizedOneByteString));
// The generic string type is stable because we've already erased any
// information about it.
static_assert(!NodeTypeIsUnstable(NodeType::kString));
// A type which contains an unstable string should also be unstable.
static_assert(NodeTypeIsUnstable(UnionType(NodeType::kNumber,
                                           NodeType::kSeqOneByteString)));
// A type which contains a stable string should also be stable.
static_assert(!NodeTypeIsUnstable(UnionType(NodeType::kNumber,
                                            NodeType::kInternalizedString)));

inline constexpr NodeType MakeTypeStable(NodeType type) {
  DCHECK(!NodeTypeIsNeverStandalone(type));
  if (!NodeTypeIsUnstable(type)) return type;
  // Strings can be in-place internalized, turned into thin strings, and
  // in-place externalized, and byteness can change from one->two byte (because
  // of internalized external strings with two-byte encoding of one-byte data)
  // or two->one byte (because of internalizing a two-byte slice with one-byte
  // data). The only invariant that we can preserve is that internalized strings
  // stay internalized.
  DCHECK(NodeTypeCanBe(type, NodeType::kString));
  // Extract out the string part of the node type.
  NodeType string_type = IntersectType(type, NodeType::kString);
  if (NodeTypeIs(string_type, NodeType::kInternalizedString)) {
    // Strings that can't be anything but internalized become generic
    // internalized.
    type = UnionType(type, NodeType::kInternalizedString);
  } else {
    // All other strings become fully generic.
    type = UnionType(type, NodeType::kString);
  }
  DCHECK(!NodeTypeIsUnstable(type));
  return type;
}
// Seq strings become normal strings with unspecified byteness when made stable,
// because they could have been internalized into a two-byte external string.
static_assert(MakeTypeStable(NodeType::kSeqOneByteString) == NodeType::kString);
// Generic internalized strings stay as they are.
static_assert(MakeTypeStable(NodeType::kInternalizedString) ==
              NodeType::kInternalizedString);
// Read-only seq internalized strings become generic.
static_assert(MakeTypeStable(NodeType::kROSeqInternalizedOneByteString) ==
              NodeType::kROSeqInternalizedOneByteString);
// Stabilizing a type which is partially an unstable string should generalize
// the string part of the type
static_assert(MakeTypeStable(UnionType(NodeType::kNumber,
                                       NodeType::kSeqOneByteString)) ==
              UnionType(NodeType::kNumber, NodeType::kString));

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
