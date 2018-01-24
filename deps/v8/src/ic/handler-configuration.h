// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_HANDLER_CONFIGURATION_H_
#define V8_IC_HANDLER_CONFIGURATION_H_

#include "src/elements-kind.h"
#include "src/field-index.h"
#include "src/globals.h"
#include "src/objects.h"
#include "src/utils.h"

namespace v8 {
namespace internal {

// A set of bit fields representing Smi handlers for loads.
class LoadHandler {
 public:
  enum Kind {
    kElement,
    kIndexedString,
    kNormal,
    kGlobal,
    kField,
    kConstant,
    kAccessor,
    kNativeDataProperty,
    kApiGetter,
    kApiGetterHolderIsPrototype,
    kInterceptor,
    kProxy,
    kNonExistent,
    kModuleExport
  };
  class KindBits : public BitField<Kind, 0, 4> {};

  // Defines whether access rights check should be done on receiver object.
  // Applicable to named property kinds only when loading value from prototype
  // chain. Ignored when loading from holder.
  class DoAccessCheckOnReceiverBits
      : public BitField<bool, KindBits::kNext, 1> {};

  // Defines whether a lookup should be done on receiver object before
  // proceeding to the prototype chain. Applicable to named property kinds only
  // when loading value from prototype chain. Ignored when loading from holder.
  class LookupOnReceiverBits
      : public BitField<bool, DoAccessCheckOnReceiverBits::kNext, 1> {};

  //
  // Encoding when KindBits contains kForConstants.
  //

  // Index of a value entry in the descriptor array.
  class DescriptorBits : public BitField<unsigned, LookupOnReceiverBits::kNext,
                                         kDescriptorIndexBitCount> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(DescriptorBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kField.
  //
  class IsInobjectBits : public BitField<bool, LookupOnReceiverBits::kNext, 1> {
  };
  class IsDoubleBits : public BitField<bool, IsInobjectBits::kNext, 1> {};
  // +1 here is to cover all possible JSObject header sizes.
  class FieldIndexBits : public BitField<unsigned, IsDoubleBits::kNext,
                                         kDescriptorIndexBitCount + 1> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(FieldIndexBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kElement or kIndexedString.
  //
  class AllowOutOfBoundsBits : public BitField<bool, KindBits::kNext, 1> {};

  //
  // Encoding when KindBits contains kElement.
  //
  class IsJsArrayBits : public BitField<bool, AllowOutOfBoundsBits::kNext, 1> {
  };
  class ConvertHoleBits : public BitField<bool, IsJsArrayBits::kNext, 1> {};
  class ElementsKindBits
      : public BitField<ElementsKind, ConvertHoleBits::kNext, 8> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(ElementsKindBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kModuleExport.
  //
  class ExportsIndexBits : public BitField<unsigned, KindBits::kNext,
                                           kSmiValueSize - KindBits::kNext> {};

  // The layout of an Tuple3 handler representing a load of a field from
  // prototype when prototype chain checks do not include non-existing lookups
  // or access checks.
  static const int kDataOffset = Tuple3::kValue1Offset;
  static const int kSmiHandlerOffset = Tuple3::kValue2Offset;
  static const int kValidityCellOffset = Tuple3::kValue3Offset;

  // The layout of an array handler representing a load of a field from
  // prototype when prototype chain checks include non-existing lookups and
  // access checks.
  static const int kSmiHandlerIndex = 0;
  static const int kValidityCellIndex = 1;
  static const int kDataIndex = 2;
  static const int kFirstPrototypeIndex = 3;

  // Decodes kind from Smi-handler.
  static inline Kind GetHandlerKind(Smi* smi_handler);

  // Creates a Smi-handler for loading a property from a slow object.
  static inline Handle<Smi> LoadNormal(Isolate* isolate);

  // Creates a Smi-handler for loading a property from a global object.
  static inline Handle<Smi> LoadGlobal(Isolate* isolate);

  // Creates a Smi-handler for loading a property from an object with an
  // interceptor.
  static inline Handle<Smi> LoadInterceptor(Isolate* isolate);

  // Creates a Smi-handler for loading a field from fast object.
  static inline Handle<Smi> LoadField(Isolate* isolate, FieldIndex field_index);

  // Creates a Smi-handler for loading a constant from fast object.
  static inline Handle<Smi> LoadConstant(Isolate* isolate, int descriptor);

  // Creates a Smi-handler for calling a getter on a fast object.
  static inline Handle<Smi> LoadAccessor(Isolate* isolate, int descriptor);

  // Creates a Smi-handler for calling a getter on a proxy.
  static inline Handle<Smi> LoadProxy(Isolate* isolate);

  // Creates a Smi-handler for loading a native data property from fast object.
  static inline Handle<Smi> LoadNativeDataProperty(Isolate* isolate,
                                                   int descriptor);

  // Creates a Smi-handler for calling a native getter on a fast object.
  static inline Handle<Smi> LoadApiGetter(Isolate* isolate,
                                          bool holder_is_receiver);

  // Creates a Smi-handler for loading a Module export.
  // |index| is the index to the "value" slot in the Module's "exports"
  // dictionary.
  static inline Handle<Smi> LoadModuleExport(Isolate* isolate, int index);

  // Creates a data handler that represents a load of a non-existent property.
  // {holder} is the object from which the property is loaded. If no holder is
  // needed (e.g., for "nonexistent"), null_value() may be passed in.
  static Handle<Object> LoadFullChain(Isolate* isolate,
                                      Handle<Map> receiver_map,
                                      Handle<Object> holder, Handle<Name> name,
                                      Handle<Smi> smi_handler);

  // Creates a data handler that represents a prototype chain check followed
  // by given Smi-handler that encoded a load from the holder.
  // Can be used only if GetPrototypeCheckCount() returns non negative value.
  static Handle<Object> LoadFromPrototype(
      Isolate* isolate, Handle<Map> receiver_map, Handle<JSReceiver> holder,
      Handle<Name> name, Handle<Smi> smi_handler,
      MaybeHandle<Object> maybe_data = MaybeHandle<Object>());

  // Creates a Smi-handler for loading a non-existent property. Works only as
  // a part of prototype chain check.
  static inline Handle<Smi> LoadNonExistent(Isolate* isolate);

  // Creates a Smi-handler for loading an element.
  static inline Handle<Smi> LoadElement(Isolate* isolate,
                                        ElementsKind elements_kind,
                                        bool convert_hole_to_undefined,
                                        bool is_js_array,
                                        KeyedAccessLoadMode load_mode);

  // Creates a Smi-handler for loading from a String.
  static inline Handle<Smi> LoadIndexedString(Isolate* isolate,
                                              KeyedAccessLoadMode load_mode);

  // Decodes the KeyedAccessLoadMode from a {handler}.
  static KeyedAccessLoadMode GetKeyedAccessLoadMode(Object* handler);

 private:
  // Sets DoAccessCheckOnReceiverBits in given Smi-handler. The receiver
  // check is a part of a prototype chain check.
  static inline Handle<Smi> EnableAccessCheckOnReceiver(
      Isolate* isolate, Handle<Smi> smi_handler);

  // Sets LookupOnReceiverBits in given Smi-handler. The receiver
  // check is a part of a prototype chain check.
  static inline Handle<Smi> EnableLookupOnReceiver(Isolate* isolate,
                                                   Handle<Smi> smi_handler);
};

// A set of bit fields representing Smi handlers for stores.
class StoreHandler {
 public:
  enum Kind {
    kElement,
    kField,
    kConstField,
    // TODO(ishell): remove once constant field tracking is done.
    kTransitionToConstant = kConstField,
    kTransitionToField,
    kAccessor,
    kNativeDataProperty,
    kApiSetter,
    kApiSetterHolderIsPrototype,
    kGlobalProxy,
    kNormal,
    kProxy,
    kKindsNumber  // Keep last
  };
  class KindBits : public BitField<Kind, 0, 4> {};

  enum FieldRepresentation { kSmi, kDouble, kHeapObject, kTagged };

  static inline bool IsHandler(Object* maybe_handler);

  // Applicable to kGlobalProxy, kProxy kinds.

  // Defines whether access rights check should be done on receiver object.
  class DoAccessCheckOnReceiverBits
      : public BitField<bool, KindBits::kNext, 1> {};

  // Applicable to kField, kTransitionToField and kTransitionToConstant
  // kinds.

  // Index of a value entry in the descriptor array.
  class DescriptorBits
      : public BitField<unsigned, DoAccessCheckOnReceiverBits::kNext,
                        kDescriptorIndexBitCount> {};
  //
  // Encoding when KindBits contains kTransitionToConstant.
  //

  // Make sure we don't overflow the smi.
  STATIC_ASSERT(DescriptorBits::kNext <= kSmiValueSize);

  //
  // Encoding when KindBits contains kField or kTransitionToField.
  //
  class ExtendStorageBits : public BitField<bool, DescriptorBits::kNext, 1> {};
  class IsInobjectBits : public BitField<bool, ExtendStorageBits::kNext, 1> {};
  class FieldRepresentationBits
      : public BitField<FieldRepresentation, IsInobjectBits::kNext, 2> {};
  // +1 here is to cover all possible JSObject header sizes.
  class FieldIndexBits
      : public BitField<unsigned, FieldRepresentationBits::kNext,
                        kDescriptorIndexBitCount + 1> {};
  // Make sure we don't overflow the smi.
  STATIC_ASSERT(FieldIndexBits::kNext <= kSmiValueSize);

  // The layout of an Tuple3 handler representing a transitioning store
  // when prototype chain checks do not include non-existing lookups or access
  // checks.
  static const int kDataOffset = Tuple3::kValue1Offset;
  static const int kSmiHandlerOffset = Tuple3::kValue2Offset;
  static const int kValidityCellOffset = Tuple3::kValue3Offset;

  static inline WeakCell* GetTransitionCell(Object* handler);
  static Object* ValidHandlerOrNull(Object* handler, Name* name,
                                    Handle<Map>* out_transition);

  // The layout of an array handler representing a transitioning store
  // when prototype chain checks include non-existing lookups and access checks.
  static const int kSmiHandlerIndex = 0;
  static const int kValidityCellIndex = 1;
  static const int kDataIndex = 2;
  static const int kFirstPrototypeIndex = 3;

  // Creates a Smi-handler for storing a field to fast object.
  static inline Handle<Smi> StoreField(Isolate* isolate, int descriptor,
                                       FieldIndex field_index,
                                       PropertyConstness constness,
                                       Representation representation);

  static Handle<Smi> StoreTransition(Isolate* isolate,
                                     Handle<Map> transition_map);

  // Creates a Smi-handler for storing a native data property on a fast object.
  static inline Handle<Smi> StoreNativeDataProperty(Isolate* isolate,
                                                    int descriptor);

  // Creates a Smi-handler for calling a setter on a fast object.
  static inline Handle<Smi> StoreAccessor(Isolate* isolate, int descriptor);

  // Creates a Smi-handler for calling a native setter on a fast object.
  static inline Handle<Smi> StoreApiSetter(Isolate* isolate,
                                           bool holder_is_receiver);

  static Handle<Object> StoreThroughPrototype(
      Isolate* isolate, Handle<Map> receiver_map, Handle<JSReceiver> holder,
      Handle<Name> name, Handle<Smi> smi_handler,
      MaybeHandle<Object> data = MaybeHandle<Object>());

  static Handle<Object> StoreElementTransition(Isolate* isolate,
                                               Handle<Map> receiver_map,
                                               Handle<Map> transition,
                                               KeyedAccessStoreMode store_mode);

  static Handle<Object> StoreProxy(Isolate* isolate, Handle<Map> receiver_map,
                                   Handle<JSProxy> proxy,
                                   Handle<JSReceiver> receiver,
                                   Handle<Name> name);

  // Creates a handler for storing a property to the property cell of a global
  // object.
  static Handle<Object> StoreGlobal(Isolate* isolate,
                                    Handle<PropertyCell> cell);

  // Creates a Smi-handler for storing a property to a global proxy object.
  static inline Handle<Smi> StoreGlobalProxy(Isolate* isolate);

  // Creates a Smi-handler for storing a property to a slow object.
  static inline Handle<Smi> StoreNormal(Isolate* isolate);

  // Creates a Smi-handler for storing a property on a proxy.
  static inline Handle<Smi> StoreProxy(Isolate* isolate);

 private:
  // Sets DoAccessCheckOnReceiverBits in given Smi-handler. The receiver
  // check is a part of a prototype chain check.
  static inline Handle<Smi> EnableAccessCheckOnReceiver(
      Isolate* isolate, Handle<Smi> smi_handler);

  static inline Handle<Smi> StoreField(Isolate* isolate, Kind kind,
                                       int descriptor, FieldIndex field_index,
                                       Representation representation,
                                       bool extend_storage);

  // Creates a Smi-handler for transitioning store to a field.
  static inline Handle<Smi> TransitionToField(Isolate* isolate, int descriptor,
                                              FieldIndex field_index,
                                              Representation representation,
                                              bool extend_storage);

  // Creates a Smi-handler for transitioning store to a constant field (in this
  // case the only thing that needs to be done is an update of a map).
  static inline Handle<Smi> TransitionToConstant(Isolate* isolate,
                                                 int descriptor);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_IC_HANDLER_CONFIGURATION_H_
