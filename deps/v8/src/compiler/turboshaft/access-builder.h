// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_ACCESS_BUILDER_H_
#define V8_COMPILER_TURBOSHAFT_ACCESS_BUILDER_H_

#include "src/base/compiler-specific.h"
#include "src/common/globals.h"
#include "src/compiler/access-builder.h"
#include "src/compiler/globals.h"
#include "src/compiler/turboshaft/index.h"
#include "src/compiler/type-cache.h"
#include "src/compiler/write-barrier-kind.h"
#include "src/objects/js-objects.h"

namespace v8::internal::compiler::turboshaft {

class AccessBuilderTS;
using JSReceiverOrNull = UnionOf<JSReceiver, Null>;

// TODO(nicohartmann): Rename this to `FieldAccess` and rely on proper
// namespaces.
template <typename Class, typename T>
struct FieldAccessTS : public compiler::FieldAccess {
  using type = T;

 private:
  friend class AccessBuilderTS;
  explicit FieldAccessTS(const compiler::FieldAccess& base)
      : compiler::FieldAccess(base) {}
};

// TODO(nicohartmann): Rename this to `ElementAccess` and rely on proper
// namespaces.
template <typename Class, typename T>
struct ElementAccessTS : public compiler::ElementAccess {
  using type = T;

  const bool is_array_buffer_load;

 private:
  friend class AccessBuilderTS;
  explicit ElementAccessTS(const compiler::ElementAccess& base,
                           bool is_array_buffer_load)
      : compiler::ElementAccess(base),
        is_array_buffer_load(is_array_buffer_load) {}
};

// TODO(nicohartmann): Rename this to `AccessBuilder` and rely on proper
// namespaces.
class AccessBuilderTS : public AllStatic {
 public:
  template <typename Class>
  static constexpr bool is_array_buffer_v = std::is_same_v<Class, ArrayBuffer>;

#define TF_FIELD_ACCESS(Class, T, name)                              \
  static FieldAccessTS<Class, T> name() {                            \
    return FieldAccessTS<Class, T>(compiler::AccessBuilder::name()); \
  }
  TF_FIELD_ACCESS(String, Word32, ForStringLength)
  TF_FIELD_ACCESS(Name, Word32, ForNameRawHashField)
  TF_FIELD_ACCESS(HeapNumber, Float64, ForHeapNumberValue)
  using HeapNumberOrOddball = Union<HeapNumber, Oddball>;
  TF_FIELD_ACCESS(HeapNumberOrOddball, Float64, ForHeapNumberOrOddballValue)
  TF_FIELD_ACCESS(BigInt, Word32, ForBigIntBitfield)
  TF_FIELD_ACCESS(BigInt, Word64, ForBigIntLeastSignificantDigit64)
  TF_FIELD_ACCESS(Map, JSReceiverOrNull, ForMapPrototype)
  TF_FIELD_ACCESS(Map, Word32, ForMapBitField)
  TF_FIELD_ACCESS(Map, Word32, ForMapBitField3)
  TF_FIELD_ACCESS(Map, NativeContext, ForMapNativeContext)
  TF_FIELD_ACCESS(JSAny, Object, ForJSObjectPropertiesOrHash)
#undef TF_ACCESS
  static FieldAccessTS<Object, Map> ForMap(
      WriteBarrierKind write_barrier = kMapWriteBarrier) {
    return FieldAccessTS<Object, Map>(
        compiler::AccessBuilder::ForMap(write_barrier));
  }
  static FieldAccessTS<FeedbackVector, Word32> ForFeedbackVectorLength() {
    return FieldAccessTS<FeedbackVector, Word32>(compiler::FieldAccess{
        BaseTaggedness::kTaggedBase, FeedbackVector::kLengthOffset,
        Handle<Name>(), OptionalMapRef(), TypeCache::Get()->kInt32,
        MachineType::Int32(), WriteBarrierKind::kNoWriteBarrier});
  }
  static FieldAccessTS<Cell, MaybeObject> ForCellMaybeValue() {
    return FieldAccessTS<Cell, MaybeObject>(
        compiler::AccessBuilder::ForCellValue());
  }
  static FieldAccessTS<PropertyCell, Object> ForPropertyCellValue() {
    return FieldAccessTS<PropertyCell, Object>(compiler::FieldAccess{
        BaseTaggedness::kTaggedBase, PropertyCell::kValueOffset, Handle<Name>(),
        OptionalMapRef(), compiler::Type::Any(), MachineType::AnyTagged(),
        WriteBarrierKind::kFullWriteBarrier});
  }
  static FieldAccessTS<JSPrimitiveWrapper, Object>
  ForJSPrimitiveWrapperValue() {
    return FieldAccessTS<JSPrimitiveWrapper, Object>(compiler::FieldAccess{
        BaseTaggedness::kTaggedBase, JSPrimitiveWrapper::kValueOffset,
        Handle<Name>(), OptionalMapRef(), compiler::Type::Any(),
        MachineType::AnyTagged(), WriteBarrierKind::kFullWriteBarrier});
  }
  static FieldAccessTS<JSArray, Number> ForJSArrayLength() {
    // NOTE: The only difference between elements kinds is the TF type of the
    // field, which we don't care about in TS.
    return FieldAccessTS<JSArray, Number>(
        compiler::AccessBuilder::ForJSArrayLength(ElementsKind::NO_ELEMENTS));
  }
  static FieldAccessTS<Context, Object> ForContextSlot(size_t index) {
    return FieldAccessTS<Context, Object>(
        compiler::AccessBuilder::ForContextSlot(index));
  }
  static FieldAccessTS<Symbol, Word32> ForSymbolFlags() {
    return FieldAccessTS<Symbol, Word32>(compiler::FieldAccess{
        BaseTaggedness::kTaggedBase, offsetof(Symbol, flags_), Handle<Name>(),
        OptionalMapRef(), compiler::Type::Any(), MachineType::Uint32(),
        WriteBarrierKind::kNoWriteBarrier});
  }

#define TF_ELEMENT_ACCESS(Class, T, name)                                     \
  static ElementAccessTS<Class, T> name() {                                   \
    return ElementAccessTS<Class, T>{compiler::AccessBuilder::name(), false}; \
  }
  TF_ELEMENT_ACCESS(SeqOneByteString, Word32, ForSeqOneByteStringCharacter)
  TF_ELEMENT_ACCESS(SeqTwoByteString, Word32, ForSeqTwoByteStringCharacter)
  TF_ELEMENT_ACCESS(Object, Object, ForOrderedHashMapEntryValue)
#undef TF_ELEMENT_ACCESS

  template <IsTagged T>
  static ElementAccessTS<FixedArray, T> ForFixedArrayElement() {
    static_assert(!is_array_buffer_v<FixedArray>);
    return ElementAccessTS<FixedArray, T>{
        compiler::AccessBuilder::ForFixedArrayElement(), false};
  }
};

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_ACCESS_BUILDER_H_
