// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_INFO_H_
#define V8_COMPILER_ACCESS_INFO_H_

#include <iosfwd>

#include "src/codegen/machine-type.h"
#include "src/compiler/compilation-dependencies.h"
#include "src/compiler/types.h"
#include "src/objects/feedback-vector.h"
#include "src/objects/field-index.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;

namespace compiler {

// Forward declarations.
class ElementAccessFeedback;
class Type;
class TypeCache;

std::ostream& operator<<(std::ostream&, AccessMode);

// This class encapsulates all information required to access a certain element.
class ElementAccessInfo final {
 public:
  ElementAccessInfo(ZoneVector<Handle<Map>>&& receiver_maps,
                    ElementsKind elements_kind, Zone* zone);

  ElementsKind elements_kind() const { return elements_kind_; }
  ZoneVector<Handle<Map>> const& receiver_maps() const {
    return receiver_maps_;
  }
  ZoneVector<Handle<Map>> const& transition_sources() const {
    return transition_sources_;
  }

  void AddTransitionSource(Handle<Map> map) {
    CHECK_EQ(receiver_maps_.size(), 1);
    transition_sources_.push_back(map);
  }

 private:
  ElementsKind elements_kind_;
  ZoneVector<Handle<Map>> receiver_maps_;
  ZoneVector<Handle<Map>> transition_sources_;
};

// This class encapsulates all information required to access a certain
// object property, either on the object itself or on the prototype chain.
class PropertyAccessInfo final {
 public:
  enum Kind {
    kInvalid,
    kNotFound,
    kDataField,
    kDataConstant,
    kAccessorConstant,
    kModuleExport,
    kStringLength
  };

  static PropertyAccessInfo NotFound(Zone* zone, Handle<Map> receiver_map,
                                     MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataField(
      Zone* zone, Handle<Map> receiver_map,
      ZoneVector<CompilationDependencies::Dependency const*>&&
          unrecorded_dependencies,
      FieldIndex field_index, Representation field_representation,
      Type field_type, MaybeHandle<Map> field_map = MaybeHandle<Map>(),
      MaybeHandle<JSObject> holder = MaybeHandle<JSObject>(),
      MaybeHandle<Map> transition_map = MaybeHandle<Map>());
  static PropertyAccessInfo DataConstant(
      Zone* zone, Handle<Map> receiver_map,
      ZoneVector<CompilationDependencies::Dependency const*>&&
          unrecorded_dependencies,
      FieldIndex field_index, Representation field_representation,
      Type field_type, MaybeHandle<Map> field_map, MaybeHandle<JSObject> holder,
      MaybeHandle<Map> transition_map = MaybeHandle<Map>());
  static PropertyAccessInfo AccessorConstant(Zone* zone,
                                             Handle<Map> receiver_map,
                                             Handle<Object> constant,
                                             MaybeHandle<JSObject> holder);
  static PropertyAccessInfo ModuleExport(Zone* zone, Handle<Map> receiver_map,
                                         Handle<Cell> cell);
  static PropertyAccessInfo StringLength(Zone* zone, Handle<Map> receiver_map);
  static PropertyAccessInfo Invalid(Zone* zone);

  bool Merge(PropertyAccessInfo const* that, AccessMode access_mode,
             Zone* zone) V8_WARN_UNUSED_RESULT;

  void RecordDependencies(CompilationDependencies* dependencies);

  bool IsInvalid() const { return kind() == kInvalid; }
  bool IsNotFound() const { return kind() == kNotFound; }
  bool IsDataField() const { return kind() == kDataField; }
  bool IsDataConstant() const { return kind() == kDataConstant; }
  bool IsAccessorConstant() const { return kind() == kAccessorConstant; }
  bool IsModuleExport() const { return kind() == kModuleExport; }
  bool IsStringLength() const { return kind() == kStringLength; }

  bool HasTransitionMap() const { return !transition_map().is_null(); }

  Kind kind() const { return kind_; }
  MaybeHandle<JSObject> holder() const {
    // This CHECK tries to protect against using the access info without
    // recording its dependencies first.
    CHECK(unrecorded_dependencies_.empty());
    return holder_;
  }
  MaybeHandle<Map> transition_map() const { return transition_map_; }
  Handle<Object> constant() const { return constant_; }
  FieldIndex field_index() const { return field_index_; }
  Type field_type() const { return field_type_; }
  Representation field_representation() const { return field_representation_; }
  MaybeHandle<Map> field_map() const { return field_map_; }
  ZoneVector<Handle<Map>> const& receiver_maps() const {
    return receiver_maps_;
  }
  Handle<Cell> export_cell() const;

 private:
  explicit PropertyAccessInfo(Zone* zone);
  PropertyAccessInfo(Zone* zone, Kind kind, MaybeHandle<JSObject> holder,
                     ZoneVector<Handle<Map>>&& receiver_maps);
  PropertyAccessInfo(Zone* zone, Kind kind, MaybeHandle<JSObject> holder,
                     Handle<Object> constant,
                     ZoneVector<Handle<Map>>&& receiver_maps);
  PropertyAccessInfo(
      Kind kind, MaybeHandle<JSObject> holder, MaybeHandle<Map> transition_map,
      FieldIndex field_index, Representation field_representation,
      Type field_type, MaybeHandle<Map> field_map,
      ZoneVector<Handle<Map>>&& receiver_maps,
      ZoneVector<CompilationDependencies::Dependency const*>&& dependencies);

  Kind kind_;
  ZoneVector<Handle<Map>> receiver_maps_;
  ZoneVector<CompilationDependencies::Dependency const*>
      unrecorded_dependencies_;
  Handle<Object> constant_;
  MaybeHandle<Map> transition_map_;
  MaybeHandle<JSObject> holder_;
  FieldIndex field_index_;
  Representation field_representation_;
  Type field_type_;
  MaybeHandle<Map> field_map_;
};


// Factory class for {ElementAccessInfo}s and {PropertyAccessInfo}s.
class AccessInfoFactory final {
 public:
  AccessInfoFactory(JSHeapBroker* broker, CompilationDependencies* dependencies,
                    Zone* zone);

  base::Optional<ElementAccessInfo> ComputeElementAccessInfo(
      Handle<Map> map, AccessMode access_mode) const;
  bool ComputeElementAccessInfos(
      ElementAccessFeedback const& processed, AccessMode access_mode,
      ZoneVector<ElementAccessInfo>* access_infos) const;

  PropertyAccessInfo ComputePropertyAccessInfo(Handle<Map> map,
                                               Handle<Name> name,
                                               AccessMode access_mode) const;

  // Convenience wrapper around {ComputePropertyAccessInfo} for multiple maps.
  void ComputePropertyAccessInfos(
      MapHandles const& maps, Handle<Name> name, AccessMode access_mode,
      ZoneVector<PropertyAccessInfo>* access_infos) const;

  // Merge as many of the given {infos} as possible and record any dependencies.
  // Return false iff any of them was invalid, in which case no dependencies are
  // recorded.
  // TODO(neis): Make access_mode part of access info?
  bool FinalizePropertyAccessInfos(
      ZoneVector<PropertyAccessInfo> infos, AccessMode access_mode,
      ZoneVector<PropertyAccessInfo>* result) const;

  // Merge the given {infos} to a single one and record any dependencies. If the
  // merge is not possible, the result has kind {kInvalid} and no dependencies
  // are recorded.
  PropertyAccessInfo FinalizePropertyAccessInfosAsOne(
      ZoneVector<PropertyAccessInfo> infos, AccessMode access_mode) const;

 private:
  base::Optional<ElementAccessInfo> ConsolidateElementLoad(
      ElementAccessFeedback const& processed) const;
  PropertyAccessInfo LookupSpecialFieldAccessor(Handle<Map> map,
                                                Handle<Name> name) const;
  PropertyAccessInfo LookupTransition(Handle<Map> map, Handle<Name> name,
                                      MaybeHandle<JSObject> holder) const;
  PropertyAccessInfo ComputeDataFieldAccessInfo(Handle<Map> receiver_map,
                                                Handle<Map> map,
                                                MaybeHandle<JSObject> holder,
                                                int descriptor,
                                                AccessMode access_mode) const;
  PropertyAccessInfo ComputeAccessorDescriptorAccessInfo(
      Handle<Map> receiver_map, Handle<Name> name, Handle<Map> map,
      MaybeHandle<JSObject> holder, int descriptor,
      AccessMode access_mode) const;

  void MergePropertyAccessInfos(ZoneVector<PropertyAccessInfo> infos,
                                AccessMode access_mode,
                                ZoneVector<PropertyAccessInfo>* result) const;

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
