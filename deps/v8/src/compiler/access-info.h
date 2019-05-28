// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_INFO_H_
#define V8_COMPILER_ACCESS_INFO_H_

#include <iosfwd>

#include "src/compiler/types.h"
#include "src/feedback-vector.h"
#include "src/field-index.h"
#include "src/machine-type.h"
#include "src/objects.h"
#include "src/objects/map.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;

namespace compiler {

// Forward declarations.
class CompilationDependencies;
class ElementAccessFeedback;
class Type;
class TypeCache;

// Whether we are loading a property or storing to a property.
// For a store during literal creation, do not walk up the prototype chain.
enum class AccessMode { kLoad, kStore, kStoreInLiteral, kHas };

std::ostream& operator<<(std::ostream&, AccessMode);

// This class encapsulates all information required to access a certain element.
class ElementAccessInfo final {
 public:
  ElementAccessInfo();
  ElementAccessInfo(MapHandles const& receiver_maps,
                    ElementsKind elements_kind);

  ElementsKind elements_kind() const { return elements_kind_; }
  MapHandles const& receiver_maps() const { return receiver_maps_; }
  MapHandles const& transition_sources() const { return transition_sources_; }

  void AddTransitionSource(Handle<Map> map) {
    CHECK_EQ(receiver_maps_.size(), 1);
    transition_sources_.push_back(map);
  }

 private:
  ElementsKind elements_kind_;
  MapHandles receiver_maps_;
  MapHandles transition_sources_;
};

// This class encapsulates all information required to access a certain
// object property, either on the object itself or on the prototype chain.
class PropertyAccessInfo final {
 public:
  enum Kind {
    kInvalid,
    kNotFound,
    kDataConstant,
    kDataField,
    kDataConstantField,
    kAccessorConstant,
    kModuleExport,
    kStringLength
  };

  static PropertyAccessInfo NotFound(MapHandles const& receiver_maps,
                                     MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataConstant(MapHandles const& receiver_maps,
                                         Handle<Object> constant,
                                         MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataField(
      PropertyConstness constness, MapHandles const& receiver_maps,
      FieldIndex field_index, MachineRepresentation field_representation,
      Type field_type, MaybeHandle<Map> field_map = MaybeHandle<Map>(),
      MaybeHandle<JSObject> holder = MaybeHandle<JSObject>(),
      MaybeHandle<Map> transition_map = MaybeHandle<Map>());
  static PropertyAccessInfo AccessorConstant(MapHandles const& receiver_maps,
                                             Handle<Object> constant,
                                             MaybeHandle<JSObject> holder);
  static PropertyAccessInfo ModuleExport(MapHandles const& receiver_maps,
                                         Handle<Cell> cell);
  static PropertyAccessInfo StringLength(MapHandles const& receiver_maps);

  PropertyAccessInfo();

  bool Merge(PropertyAccessInfo const* that, AccessMode access_mode,
             Zone* zone) V8_WARN_UNUSED_RESULT;

  bool IsInvalid() const { return kind() == kInvalid; }
  bool IsNotFound() const { return kind() == kNotFound; }
  bool IsDataConstant() const { return kind() == kDataConstant; }
  bool IsDataField() const { return kind() == kDataField; }
  // TODO(ishell): rename to IsDataConstant() once constant field tracking
  // is done.
  bool IsDataConstantField() const { return kind() == kDataConstantField; }
  bool IsAccessorConstant() const { return kind() == kAccessorConstant; }
  bool IsModuleExport() const { return kind() == kModuleExport; }
  bool IsStringLength() const { return kind() == kStringLength; }

  bool HasTransitionMap() const { return !transition_map().is_null(); }

  Kind kind() const { return kind_; }
  MaybeHandle<JSObject> holder() const { return holder_; }
  MaybeHandle<Map> transition_map() const { return transition_map_; }
  Handle<Object> constant() const { return constant_; }
  FieldIndex field_index() const { return field_index_; }
  Type field_type() const { return field_type_; }
  MachineRepresentation field_representation() const {
    return field_representation_;
  }
  MaybeHandle<Map> field_map() const { return field_map_; }
  MapHandles const& receiver_maps() const { return receiver_maps_; }
  Handle<Cell> export_cell() const;

 private:
  PropertyAccessInfo(Kind kind, MaybeHandle<JSObject> holder,
                     MapHandles const& receiver_maps);
  PropertyAccessInfo(Kind kind, MaybeHandle<JSObject> holder,
                     Handle<Object> constant, MapHandles const& receiver_maps);
  PropertyAccessInfo(Kind kind, MaybeHandle<JSObject> holder,
                     MaybeHandle<Map> transition_map, FieldIndex field_index,
                     MachineRepresentation field_representation,
                     Type field_type, MaybeHandle<Map> field_map,
                     MapHandles const& receiver_maps);

  Kind kind_;
  MapHandles receiver_maps_;
  Handle<Object> constant_;
  MaybeHandle<Map> transition_map_;
  MaybeHandle<JSObject> holder_;
  FieldIndex field_index_;
  MachineRepresentation field_representation_;
  Type field_type_;
  MaybeHandle<Map> field_map_;
};


// Factory class for {ElementAccessInfo}s and {PropertyAccessInfo}s.
class AccessInfoFactory final {
 public:
  AccessInfoFactory(JSHeapBroker* broker, CompilationDependencies* dependencies,
                    Zone* zone);

  bool ComputeElementAccessInfo(Handle<Map> map, AccessMode access_mode,
                                ElementAccessInfo* access_info) const;
  bool ComputeElementAccessInfos(
      FeedbackNexus nexus, MapHandles const& maps, AccessMode access_mode,
      ZoneVector<ElementAccessInfo>* access_infos) const;

  PropertyAccessInfo ComputePropertyAccessInfo(Handle<Map> map,
                                               Handle<Name> name,
                                               AccessMode access_mode) const;
  PropertyAccessInfo ComputePropertyAccessInfo(MapHandles const& maps,
                                               Handle<Name> name,
                                               AccessMode access_mode) const;
  void ComputePropertyAccessInfos(
      MapHandles const& maps, Handle<Name> name, AccessMode access_mode,
      ZoneVector<PropertyAccessInfo>* access_infos) const;

  // Merge as many of the given {infos} as possible. Return false iff
  // any of them was invalid.
  bool FinalizePropertyAccessInfos(
      ZoneVector<PropertyAccessInfo> infos, AccessMode access_mode,
      ZoneVector<PropertyAccessInfo>* result) const;

 private:
  bool ConsolidateElementLoad(ElementAccessFeedback const& processed,
                              ElementAccessInfo* access_info) const;
  PropertyAccessInfo LookupSpecialFieldAccessor(Handle<Map> map,
                                                Handle<Name> name) const;
  PropertyAccessInfo LookupTransition(Handle<Map> map, Handle<Name> name,
                                      MaybeHandle<JSObject> holder) const;
  PropertyAccessInfo ComputeDataFieldAccessInfo(Handle<Map> receiver_map,
                                                Handle<Map> map,
                                                MaybeHandle<JSObject> holder,
                                                int number,
                                                AccessMode access_mode) const;
  PropertyAccessInfo ComputeAccessorDescriptorAccessInfo(
      Handle<Map> receiver_map, Handle<Name> name, Handle<Map> map,
      MaybeHandle<JSObject> holder, int number, AccessMode access_mode) const;

  CompilationDependencies* dependencies() const { return dependencies_; }
  JSHeapBroker* broker() const { return broker_; }
  Isolate* isolate() const { return broker()->isolate(); }
  Zone* zone() const { return zone_; }

  JSHeapBroker* const broker_;
  CompilationDependencies* const dependencies_;
  TypeCache const* const type_cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(AccessInfoFactory);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ACCESS_INFO_H_
