// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_INFO_H_
#define V8_COMPILER_ACCESS_INFO_H_

#include <iosfwd>

#include "src/field-index.h"
#include "src/machine-type.h"
#include "src/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;

namespace compiler {

// Forward declarations.
class Type;
class TypeCache;

// Whether we are loading a property or storing to a property.
// For a store during literal creation, do not walk up the prototype chain.
enum class AccessMode { kLoad, kStore, kStoreInLiteral };

std::ostream& operator<<(std::ostream&, AccessMode);

typedef std::vector<Handle<Map>> MapList;

// Mapping of transition source to transition target.
typedef std::vector<std::pair<Handle<Map>, Handle<Map>>> MapTransitionList;

// This class encapsulates all information required to access a certain element.
class ElementAccessInfo final {
 public:
  ElementAccessInfo();
  ElementAccessInfo(MapList const& receiver_maps, ElementsKind elements_kind);

  ElementsKind elements_kind() const { return elements_kind_; }
  MapList const& receiver_maps() const { return receiver_maps_; }
  MapTransitionList& transitions() { return transitions_; }
  MapTransitionList const& transitions() const { return transitions_; }

 private:
  ElementsKind elements_kind_;
  MapList receiver_maps_;
  MapTransitionList transitions_;
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
    kAccessorConstant,
    kGeneric
  };

  static PropertyAccessInfo NotFound(MapList const& receiver_maps,
                                     MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataConstant(MapList const& receiver_maps,
                                         Handle<Object> constant,
                                         MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataField(
      MapList const& receiver_maps, FieldIndex field_index,
      MachineRepresentation field_representation, Type* field_type,
      MaybeHandle<Map> field_map = MaybeHandle<Map>(),
      MaybeHandle<JSObject> holder = MaybeHandle<JSObject>(),
      MaybeHandle<Map> transition_map = MaybeHandle<Map>());
  static PropertyAccessInfo AccessorConstant(MapList const& receiver_maps,
                                             Handle<Object> constant,
                                             MaybeHandle<JSObject> holder);
  static PropertyAccessInfo Generic(MapList const& receiver_maps);

  PropertyAccessInfo();

  bool Merge(PropertyAccessInfo const* that) WARN_UNUSED_RESULT;

  bool IsNotFound() const { return kind() == kNotFound; }
  bool IsDataConstant() const { return kind() == kDataConstant; }
  bool IsDataField() const { return kind() == kDataField; }
  bool IsAccessorConstant() const { return kind() == kAccessorConstant; }
  bool IsGeneric() const { return kind() == kGeneric; }

  bool HasTransitionMap() const { return !transition_map().is_null(); }

  Kind kind() const { return kind_; }
  MaybeHandle<JSObject> holder() const { return holder_; }
  MaybeHandle<Map> transition_map() const { return transition_map_; }
  Handle<Object> constant() const { return constant_; }
  FieldIndex field_index() const { return field_index_; }
  Type* field_type() const { return field_type_; }
  MachineRepresentation field_representation() const {
    return field_representation_;
  }
  MaybeHandle<Map> field_map() const { return field_map_; }
  MapList const& receiver_maps() const { return receiver_maps_; }

 private:
  PropertyAccessInfo(MaybeHandle<JSObject> holder,
                     MapList const& receiver_maps);
  PropertyAccessInfo(Kind kind, MaybeHandle<JSObject> holder,
                     Handle<Object> constant, MapList const& receiver_maps);
  PropertyAccessInfo(MaybeHandle<JSObject> holder,
                     MaybeHandle<Map> transition_map, FieldIndex field_index,
                     MachineRepresentation field_representation,
                     Type* field_type, MaybeHandle<Map> field_map,
                     MapList const& receiver_maps);

  Kind kind_;
  MapList receiver_maps_;
  Handle<Object> constant_;
  MaybeHandle<Map> transition_map_;
  MaybeHandle<JSObject> holder_;
  FieldIndex field_index_;
  MachineRepresentation field_representation_;
  Type* field_type_;
  MaybeHandle<Map> field_map_;
};


// Factory class for {ElementAccessInfo}s and {PropertyAccessInfo}s.
class AccessInfoFactory final {
 public:
  AccessInfoFactory(CompilationDependencies* dependencies,
                    Handle<Context> native_context, Zone* zone);

  bool ComputeElementAccessInfo(Handle<Map> map, AccessMode access_mode,
                                ElementAccessInfo* access_info);
  bool ComputeElementAccessInfos(MapHandleList const& maps,
                                 AccessMode access_mode,
                                 ZoneVector<ElementAccessInfo>* access_infos);
  bool ComputePropertyAccessInfo(Handle<Map> map, Handle<Name> name,
                                 AccessMode access_mode,
                                 PropertyAccessInfo* access_info);
  bool ComputePropertyAccessInfos(MapHandleList const& maps, Handle<Name> name,
                                  AccessMode access_mode,
                                  ZoneVector<PropertyAccessInfo>* access_infos);

 private:
  bool LookupSpecialFieldAccessor(Handle<Map> map, Handle<Name> name,
                                  PropertyAccessInfo* access_info);
  bool LookupTransition(Handle<Map> map, Handle<Name> name,
                        MaybeHandle<JSObject> holder,
                        PropertyAccessInfo* access_info);

  CompilationDependencies* dependencies() const { return dependencies_; }
  Factory* factory() const;
  Isolate* isolate() const { return isolate_; }
  Handle<Context> native_context() const { return native_context_; }
  Zone* zone() const { return zone_; }

  CompilationDependencies* const dependencies_;
  Handle<Context> const native_context_;
  Isolate* const isolate_;
  TypeCache const& type_cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(AccessInfoFactory);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ACCESS_INFO_H_
