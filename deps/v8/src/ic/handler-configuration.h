// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_CONFIGURATION_H_
#define V8_IC_HANDLER_CONFIGURATION_H_

#include "src/elements-kind.h"
#include "src/field-index.h"
#include "src/globals.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// A set of bit fields representing Smi handlers for loads.
class LoadHandler {
 public:
  enum Kind { kForElements, kForFields, kForConstants, kForNonExistent };
  class KindBits : public BitField<Kind, 0, 2> {};

  // Defines whether access rights check should be done on receiver object.
  // Applicable to kForFields, kForConstants and kForNonExistent kinds only when
  // loading value from prototype chain. Ignored when loading from holder.
  class DoAccessCheckOnReceiverBits
      : public BitField<bool, KindBits::kNext, 1> {};

  // Defines whether negative lookup check should be done on receiver object.
  // Applicable to kForFields, kForConstants and kForNonExistent kinds only when
  // loading value from prototype chain. Ignored when loading from holder.
  class DoNegativeLookupOnReceiverBits
      : public BitField<bool, DoAccessCheckOnReceiverBits::kNext, 1> {};

  //
  // Encoding when KindBits contains kForConstants.
  //

  class IsAccessorInfoBits
      : public BitField<bool, DoNegativeLookupOnReceiverBits::kNext, 1> {};
  // Index of a value entry in the descriptor array.
  // +2 here is because each descriptor entry occupies 3 slots in array.
  class DescriptorValueIndexBits
      : public BitField<unsigned, IsAccessorInfoBits::kNext,
                        kDescriptorIndexBitCount + 2> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(DescriptorValueIndexBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kForFields.
  //
  class IsInobjectBits
      : public BitField<bool, DoNegativeLookupOnReceiverBits::kNext, 1> {};
  class IsDoubleBits : public BitField<bool, IsInobjectBits::kNext, 1> {};
  // +1 here is to cover all possible JSObject header sizes.
  class FieldOffsetBits
      : public BitField<unsigned, IsDoubleBits::kNext,
                        kDescriptorIndexBitCount + 1 + kPointerSizeLog2> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(FieldOffsetBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kForElements.
  //
  class IsJsArrayBits : public BitField<bool, KindBits::kNext, 1> {};
  class ConvertHoleBits : public BitField<bool, IsJsArrayBits::kNext, 1> {};
  class ElementsKindBits
      : public BitField<ElementsKind, ConvertHoleBits::kNext, 8> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(ElementsKindBits::kNext <= kSmiValueSize);

  // The layout of an Tuple3 handler representing a load of a field from
  // prototype when prototype chain checks do not include non-existing lookups
  // or access checks.
  static const int kHolderCellOffset = Tuple3::kValue1Offset;
  static const int kSmiHandlerOffset = Tuple3::kValue2Offset;
  static const int kValidityCellOffset = Tuple3::kValue3Offset;

  // The layout of an array handler representing a load of a field from
  // prototype when prototype chain checks include non-existing lookups and
  // access checks.
  static const int kSmiHandlerIndex = 0;
  static const int kValidityCellIndex = 1;
  static const int kHolderCellIndex = 2;
  static const int kFirstPrototypeIndex = 3;

  // Creates a Smi-handler for loading a field from fast object.
  static inline Handle<Object> LoadField(Isolate* isolate,
                                         FieldIndex field_index);

  // Creates a Smi-handler for loading a constant from fast object.
  static inline Handle<Object> LoadConstant(Isolate* isolate, int descriptor);

  // Creates a Smi-handler for loading an Api getter property from fast object.
  static inline Handle<Object> LoadApiGetter(Isolate* isolate, int descriptor);

  // Sets DoAccessCheckOnReceiverBits in given Smi-handler. The receiver
  // check is a part of a prototype chain check.
  static inline Handle<Object> EnableAccessCheckOnReceiver(
      Isolate* isolate, Handle<Object> smi_handler);

  // Sets DoNegativeLookupOnReceiverBits in given Smi-handler. The receiver
  // check is a part of a prototype chain check.
  static inline Handle<Object> EnableNegativeLookupOnReceiver(
      Isolate* isolate, Handle<Object> smi_handler);

  // Creates a Smi-handler for loading a non-existent property. Works only as
  // a part of prototype chain check.
  static inline Handle<Object> LoadNonExistent(
      Isolate* isolate, bool do_negative_lookup_on_receiver);

  // Creates a Smi-handler for loading an element.
  static inline Handle<Object> LoadElement(Isolate* isolate,
                                           ElementsKind elements_kind,
                                           bool convert_hole_to_undefined,
                                           bool is_js_array);
};

// A set of bit fields representing Smi handlers for stores.
class StoreHandler {
 public:
  enum Kind {
    kStoreElement,
    kStoreField,
    kStoreConstField,
    kTransitionToField,
    // TODO(ishell): remove once constant field tracking is done.
    kTransitionToConstant = kStoreConstField
  };
  class KindBits : public BitField<Kind, 0, 2> {};

  enum FieldRepresentation { kSmi, kDouble, kHeapObject, kTagged };

  // Applicable to kStoreField, kTransitionToField and kTransitionToConstant
  // kinds.

  // Index of a value entry in the descriptor array.
  // +2 here is because each descriptor entry occupies 3 slots in array.
  class DescriptorValueIndexBits
      : public BitField<unsigned, KindBits::kNext,
                        kDescriptorIndexBitCount + 2> {};
  //
  // Encoding when KindBits contains kTransitionToConstant.
  //

  // Make sure we don't overflow the smi.
  STATIC_ASSERT(DescriptorValueIndexBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kStoreField or kTransitionToField.
  //
  class ExtendStorageBits
      : public BitField<bool, DescriptorValueIndexBits::kNext, 1> {};
  class IsInobjectBits : public BitField<bool, ExtendStorageBits::kNext, 1> {};
  class FieldRepresentationBits
      : public BitField<FieldRepresentation, IsInobjectBits::kNext, 2> {};
  // +1 here is to cover all possible JSObject header sizes.
  class FieldOffsetBits
      : public BitField<unsigned, FieldRepresentationBits::kNext,
                        kDescriptorIndexBitCount + 1 + kPointerSizeLog2> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(FieldOffsetBits::kNext <= kSmiValueSize);

  // The layout of an Tuple3 handler representing a transitioning store
  // when prototype chain checks do not include non-existing lookups or access
  // checks.
  static const int kTransitionCellOffset = Tuple3::kValue1Offset;
  static const int kSmiHandlerOffset = Tuple3::kValue2Offset;
  static const int kValidityCellOffset = Tuple3::kValue3Offset;

  // The layout of an array handler representing a transitioning store
  // when prototype chain checks include non-existing lookups and access checks.
  static const int kSmiHandlerIndex = 0;
  static const int kValidityCellIndex = 1;
  static const int kTransitionCellIndex = 2;
  static const int kFirstPrototypeIndex = 3;

  // Creates a Smi-handler for storing a field to fast object.
  static inline Handle<Object> StoreField(Isolate* isolate, int descriptor,
                                          FieldIndex field_index,
                                          PropertyConstness constness,
                                          Representation representation);

  // Creates a Smi-handler for transitioning store to a field.
  static inline Handle<Object> TransitionToField(Isolate* isolate,
                                                 int descriptor,
                                                 FieldIndex field_index,
                                                 Representation representation,
                                                 bool extend_storage);

  // Creates a Smi-handler for transitioning store to a constant field (in this
  // case the only thing that needs to be done is an update of a map).
  static inline Handle<Object> TransitionToConstant(Isolate* isolate,
                                                    int descriptor);

 private:
  static inline Handle<Object> StoreField(Isolate* isolate, Kind kind,
                                          int descriptor,
                                          FieldIndex field_index,
                                          Representation representation,
                                          bool extend_storage);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_HANDLER_CONFIGURATION_H_
