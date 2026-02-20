// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/maglev/maglev-node-type.h"

namespace v8::internal::maglev {

// Assert that the Unknown type is constructed correctly.
#define ADD_STATIC_ASSERT(Name, Value)                          \
  static_assert(NodeTypeIsNeverStandalone(NodeType::k##Name) || \
                NodeTypeIs(NodeType::k##Name, NodeType::kUnknown));
LEAF_NODE_TYPE_LIST(ADD_STATIC_ASSERT)
#undef ADD_STATIC_ASSERT

NodeType StaticTypeForMap(compiler::MapRef map,
                          compiler::JSHeapBroker* broker) {
  if (map.IsHeapNumberMap()) return NodeType::kHeapNumber;
  if (map.IsStringMap()) {
    if (map.IsInternalizedStringMap()) {
      return NodeType::kInternalizedString;
    }
    if (map.IsSeqStringMap() && map.IsOneByteStringMap()) {
      return NodeType::kSeqOneByteString;
    }
    return NodeType::kString;
  }
  if (map.IsStringWrapperMap()) return NodeType::kStringWrapper;
  if (map.IsSymbolMap()) return NodeType::kSymbol;
  if (map.IsBooleanMap(broker)) return NodeType::kBoolean;
  if (map.IsNullMap(broker)) return NodeType::kNull;
  if (map.IsUndefinedMap(broker)) return NodeType::kUndefined;
  DCHECK(!map.IsOddballMap());
  if (map.IsContextMap()) return NodeType::kContext;
  if (map.IsJSArrayMap()) return NodeType::kJSArray;
  if (map.IsJSFunctionMap()) return NodeType::kJSFunction;
  if (map.is_callable()) {
    return NodeType::kCallable;
  }
  if (map.IsJSDataViewMap()) return NodeType::kJSDataView;
  if (map.IsJSReceiverMap()) {
    // JSReceiver but not any of the above.
    return NodeType::kOtherJSReceiver;
  }
  return NodeType::kOtherHeapObject;
}

NodeType StaticTypeForConstant(compiler::JSHeapBroker* broker,
                               compiler::ObjectRef ref) {
  if (ref.IsSmi()) return NodeType::kSmi;
  if (ref.HoleType() != compiler::HoleType::kNone)
    return NodeType::kOtherHeapObject;
  NodeType type = StaticTypeForMap(ref.AsHeapObject().map(broker), broker);
  DCHECK(!IsEmptyNodeType(type));
  if (type == NodeType::kInternalizedString && ref.is_read_only()) {
    if (ref.AsString().IsSeqString() &&
        ref.AsString().IsOneByteRepresentation()) {
      type = NodeType::kROSeqInternalizedOneByteString;
    }
  }
  return type;
}

bool IsInstanceOfLeafNodeType(compiler::MapRef map, NodeType type,
                              compiler::JSHeapBroker* broker) {
  switch (type) {
    case NodeType::kSmi:
      return false;
    case NodeType::kHeapNumber:
      return map.IsHeapNumberMap();
    case NodeType::kNull:
      return map.IsNullMap(broker);
    case NodeType::kUndefined:
      return map.IsUndefinedMap(broker);
    case NodeType::kBoolean:
      return map.IsBooleanMap(broker);
    case NodeType::kSymbol:
      return map.IsSymbolMap();
    case NodeType::kOtherString:
      // This doesn't exclude other string leaf types, which means one should
      // never test for this node type alone.
      return map.IsStringMap();
    case NodeType::kOtherSeqOneByteString:
      return map.IsSeqStringMap() && map.IsOneByteStringMap();
    // We can't prove with a map alone that an object is in RO-space, but
    // these maps will be potential candidates.
    case NodeType::kROSeqInternalizedOneByteString:
    case NodeType::kOtherSeqInternalizedOneByteString:
      return map.IsInternalizedStringMap() && map.IsSeqStringMap() &&
             map.IsOneByteStringMap();
    case NodeType::kOtherInternalizedString:
      return map.IsInternalizedStringMap();
    case NodeType::kStringWrapper:
      return map.IsStringWrapperMap();
    case NodeType::kContext:
      return map.IsContextMap();
    case NodeType::kJSArray:
      return map.IsJSArrayMap();
    case NodeType::kJSFunction:
      return map.IsJSFunctionMap();
    case NodeType::kCallable:
      return map.is_callable();
    case NodeType::kOtherCallable:
      return map.is_callable() && !map.IsJSFunctionMap();
    case NodeType::kJSDataView:
      return map.IsJSDataViewMap();
    case NodeType::kOtherJSReceiver:
      return map.IsJSReceiverMap() && !map.IsJSArrayMap() &&
             !map.is_callable() && !map.IsStringWrapperMap();
    case NodeType::kOtherHeapObject:
      return !map.IsHeapNumberMap() && !map.IsOddballMap() &&
             !map.IsContextMap() && !map.IsSymbolMap() && !map.IsStringMap() &&
             !map.IsJSReceiverMap();
    default:
      UNREACHABLE();
  }
}

bool IsInstanceOfNodeType(compiler::MapRef map, NodeType type,
                          compiler::JSHeapBroker* broker) {
  DCHECK(!NodeTypeIsNeverStandalone(type));

  // Early return for any heap object.
  if (NodeTypeIs(NodeType::kAnyHeapObject, type)) {
    // Unknown types will be handled here too.
    static_assert(NodeTypeIs(NodeType::kAnyHeapObject, NodeType::kUnknown));
    return true;
  }

  // Iterate over each leaf type bit in the type bitmask, and check if the map
  // matches it.
  NodeTypeInt type_bits = static_cast<NodeTypeInt>(type);
  while (type_bits != 0) {
    NodeTypeInt current_bit =
        1 << base::bits::CountTrailingZerosNonZero(type_bits);
    NodeType leaf_type = static_cast<NodeType>(current_bit);
    if (IsInstanceOfLeafNodeType(map, leaf_type, broker)) return true;
    type_bits = base::bits::ClearLsb(type_bits);
  }
  return false;
}

std::ostream& operator<<(std::ostream& out, const NodeType& type) {
  if (IsEmptyNodeType(type)) {
    out << "Empty";
    return out;
  }
  switch (type) {
#define CASE(Name, _)     \
  case NodeType::k##Name: \
    out << #Name;         \
    break;
    NODE_TYPE_LIST(CASE)
#undef CASE
    default:
#define CASE(Name, _)                                        \
  if (NodeTypeIsForPrinting(NodeType::k##Name, type)) {      \
    if constexpr (NodeType::k##Name != NodeType::kUnknown) { \
      out << #Name "|";                                      \
    }                                                        \
  }
      LEAF_NODE_TYPE_LIST(CASE)
#undef CASE
  }
  return out;
}

}  // namespace v8::internal::maglev
