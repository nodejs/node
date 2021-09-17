// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_INFO_H_
#define V8_COMPILER_ACCESS_INFO_H_

#include "src/compiler/heap-refs.h"
#include "src/compiler/types.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Factory;

namespace compiler {

// Forward declarations.
class CompilationDependencies;
class CompilationDependency;
class ElementAccessFeedback;
class JSHeapBroker;
class MinimorphicLoadPropertyAccessFeedback;
class TypeCache;
struct ConstFieldInfo;

std::ostream& operator<<(std::ostream&, AccessMode);

// This class encapsulates all information required to access a certain element.
class ElementAccessInfo final {
 public:
  ElementAccessInfo(ZoneVector<MapRef>&& lookup_start_object_maps,
                    ElementsKind elements_kind, Zone* zone);

  ElementsKind elements_kind() const { return elements_kind_; }
  ZoneVector<MapRef> const& lookup_start_object_maps() const {
    return lookup_start_object_maps_;
  }
  ZoneVector<MapRef> const& transition_sources() const {
    return transition_sources_;
  }

  void AddTransitionSource(MapRef map) {
    CHECK_EQ(lookup_start_object_maps_.size(), 1);
    transition_sources_.push_back(map);
  }

 private:
  ElementsKind elements_kind_;
  ZoneVector<MapRef> lookup_start_object_maps_;
  ZoneVector<MapRef> transition_sources_;
};

// This class encapsulates all information required to access a certain
// object property, either on the object itself or on the prototype chain.
class PropertyAccessInfo final {
 public:
  enum Kind {
    kInvalid,
    kNotFound,
    kDataField,
    kFastDataConstant,
    kDictionaryProtoDataConstant,
    kFastAccessorConstant,
    kDictionaryProtoAccessorConstant,
    kModuleExport,
    kStringLength
  };

  static PropertyAccessInfo NotFound(Zone* zone, MapRef receiver_map,
                                     base::Optional<JSObjectRef> holder);
  static PropertyAccessInfo DataField(
      Zone* zone, MapRef receiver_map,
      ZoneVector<CompilationDependency const*>&& unrecorded_dependencies,
      FieldIndex field_index, Representation field_representation,
      Type field_type, MapRef field_owner_map, base::Optional<MapRef> field_map,
      base::Optional<JSObjectRef> holder,
      base::Optional<MapRef> transition_map);
  static PropertyAccessInfo FastDataConstant(
      Zone* zone, MapRef receiver_map,
      ZoneVector<CompilationDependency const*>&& unrecorded_dependencies,
      FieldIndex field_index, Representation field_representation,
      Type field_type, MapRef field_owner_map, base::Optional<MapRef> field_map,
      base::Optional<JSObjectRef> holder,
      base::Optional<MapRef> transition_map);
  static PropertyAccessInfo FastAccessorConstant(
      Zone* zone, MapRef receiver_map, base::Optional<ObjectRef> constant,
      base::Optional<JSObjectRef> holder);
  static PropertyAccessInfo ModuleExport(Zone* zone, MapRef receiver_map,
                                         CellRef cell);
  static PropertyAccessInfo StringLength(Zone* zone, MapRef receiver_map);
  static PropertyAccessInfo Invalid(Zone* zone);
  static PropertyAccessInfo DictionaryProtoDataConstant(
      Zone* zone, MapRef receiver_map, JSObjectRef holder,
      InternalIndex dict_index, NameRef name);
  static PropertyAccessInfo DictionaryProtoAccessorConstant(
      Zone* zone, MapRef receiver_map, base::Optional<JSObjectRef> holder,
      ObjectRef constant, NameRef name);

  bool Merge(PropertyAccessInfo const* that, AccessMode access_mode,
             Zone* zone) V8_WARN_UNUSED_RESULT;

  void RecordDependencies(CompilationDependencies* dependencies);

  bool IsInvalid() const { return kind() == kInvalid; }
  bool IsNotFound() const { return kind() == kNotFound; }
  bool IsDataField() const { return kind() == kDataField; }
  bool IsFastDataConstant() const { return kind() == kFastDataConstant; }
  bool IsFastAccessorConstant() const {
    return kind() == kFastAccessorConstant;
  }
  bool IsModuleExport() const { return kind() == kModuleExport; }
  bool IsStringLength() const { return kind() == kStringLength; }
  bool IsDictionaryProtoDataConstant() const {
    return kind() == kDictionaryProtoDataConstant;
  }
  bool IsDictionaryProtoAccessorConstant() const {
    return kind() == kDictionaryProtoAccessorConstant;
  }

  bool HasTransitionMap() const { return transition_map().has_value(); }
  bool HasDictionaryHolder() const {
    return kind_ == kDictionaryProtoDataConstant ||
           kind_ == kDictionaryProtoAccessorConstant;
  }
  ConstFieldInfo GetConstFieldInfo() const;

  Kind kind() const { return kind_; }
  base::Optional<JSObjectRef> holder() const {
    // TODO(neis): There was a CHECK here that tries to protect against
    // using the access info without recording its dependencies first.
    // Find a more suitable place for it.
    return holder_;
  }
  base::Optional<MapRef> transition_map() const {
    DCHECK(!HasDictionaryHolder());
    return transition_map_;
  }
  base::Optional<ObjectRef> constant() const {
    DCHECK_IMPLIES(constant_.has_value(),
                   IsModuleExport() || IsFastAccessorConstant() ||
                       IsDictionaryProtoAccessorConstant());
    return constant_;
  }
  FieldIndex field_index() const {
    DCHECK(!HasDictionaryHolder());
    return field_index_;
  }

  Type field_type() const {
    DCHECK(!HasDictionaryHolder());
    return field_type_;
  }
  Representation field_representation() const {
    DCHECK(!HasDictionaryHolder());
    return field_representation_;
  }
  base::Optional<MapRef> field_map() const {
    DCHECK(!HasDictionaryHolder());
    return field_map_;
  }
  ZoneVector<MapRef> const& lookup_start_object_maps() const {
    return lookup_start_object_maps_;
  }

  InternalIndex dictionary_index() const {
    DCHECK(HasDictionaryHolder());
    return dictionary_index_;
  }

  NameRef name() const {
    DCHECK(HasDictionaryHolder());
    return name_.value();
  }

 private:
  explicit PropertyAccessInfo(Zone* zone);
  PropertyAccessInfo(Zone* zone, Kind kind, base::Optional<JSObjectRef> holder,
                     ZoneVector<MapRef>&& lookup_start_object_maps);
  PropertyAccessInfo(Zone* zone, Kind kind, base::Optional<JSObjectRef> holder,
                     base::Optional<ObjectRef> constant,
                     base::Optional<NameRef> name,
                     ZoneVector<MapRef>&& lookup_start_object_maps);
  PropertyAccessInfo(Kind kind, base::Optional<JSObjectRef> holder,
                     base::Optional<MapRef> transition_map,
                     FieldIndex field_index,
                     Representation field_representation, Type field_type,
                     MapRef field_owner_map, base::Optional<MapRef> field_map,
                     ZoneVector<MapRef>&& lookup_start_object_maps,
                     ZoneVector<CompilationDependency const*>&& dependencies);
  PropertyAccessInfo(Zone* zone, Kind kind, base::Optional<JSObjectRef> holder,
                     ZoneVector<MapRef>&& lookup_start_object_maps,
                     InternalIndex dictionary_index, NameRef name);

  // Members used for fast and dictionary mode holders:
  Kind kind_;
  ZoneVector<MapRef> lookup_start_object_maps_;
  base::Optional<ObjectRef> constant_;
  base::Optional<JSObjectRef> holder_;

  // Members only used for fast mode holders:
  ZoneVector<CompilationDependency const*> unrecorded_dependencies_;
  base::Optional<MapRef> transition_map_;
  FieldIndex field_index_;
  Representation field_representation_;
  Type field_type_;
  base::Optional<MapRef> field_owner_map_;
  base::Optional<MapRef> field_map_;

  // Members only used for dictionary mode holders:
  InternalIndex dictionary_index_;
  base::Optional<NameRef> name_;
};

// This class encapsulates information required to generate load properties
// by only using the information from handlers. This information is used with
// dynamic map checks.
class MinimorphicLoadPropertyAccessInfo final {
 public:
  enum Kind { kInvalid, kDataField };
  static MinimorphicLoadPropertyAccessInfo DataField(
      int offset, bool is_inobject, Representation field_representation,
      Type field_type);
  static MinimorphicLoadPropertyAccessInfo Invalid();

  bool IsInvalid() const { return kind_ == kInvalid; }
  bool IsDataField() const { return kind_ == kDataField; }
  int offset() const { return offset_; }
  int is_inobject() const { return is_inobject_; }
  Type field_type() const { return field_type_; }
  Representation field_representation() const { return field_representation_; }

 private:
  MinimorphicLoadPropertyAccessInfo(Kind kind, int offset, bool is_inobject,
                                    Representation field_representation,
                                    Type field_type);

  Kind kind_;
  bool is_inobject_;
  int offset_;
  Representation field_representation_;
  Type field_type_;
};

// Factory class for {ElementAccessInfo}s and {PropertyAccessInfo}s.
class AccessInfoFactory final {
 public:
  AccessInfoFactory(JSHeapBroker* broker, CompilationDependencies* dependencies,
                    Zone* zone);

  base::Optional<ElementAccessInfo> ComputeElementAccessInfo(
      MapRef map, AccessMode access_mode) const;
  bool ComputeElementAccessInfos(
      ElementAccessFeedback const& feedback,
      ZoneVector<ElementAccessInfo>* access_infos) const;

  PropertyAccessInfo ComputePropertyAccessInfo(MapRef map, NameRef name,
                                               AccessMode access_mode) const;

  PropertyAccessInfo ComputeDictionaryProtoAccessInfo(
      MapRef receiver_map, NameRef name, JSObjectRef holder,
      InternalIndex dict_index, AccessMode access_mode,
      PropertyDetails details) const;

  MinimorphicLoadPropertyAccessInfo ComputePropertyAccessInfo(
      MinimorphicLoadPropertyAccessFeedback const& feedback) const;

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
      ElementAccessFeedback const& feedback) const;
  PropertyAccessInfo LookupSpecialFieldAccessor(MapRef map, NameRef name) const;
  PropertyAccessInfo LookupTransition(MapRef map, NameRef name,
                                      base::Optional<JSObjectRef> holder) const;
  PropertyAccessInfo ComputeDataFieldAccessInfo(
      MapRef receiver_map, MapRef map, base::Optional<JSObjectRef> holder,
      InternalIndex descriptor, AccessMode access_mode) const;
  PropertyAccessInfo ComputeAccessorDescriptorAccessInfo(
      MapRef receiver_map, NameRef name, MapRef map,
      base::Optional<JSObjectRef> holder, InternalIndex descriptor,
      AccessMode access_mode) const;

  PropertyAccessInfo Invalid() const {
    return PropertyAccessInfo::Invalid(zone());
  }

  void MergePropertyAccessInfos(ZoneVector<PropertyAccessInfo> infos,
                                AccessMode access_mode,
                                ZoneVector<PropertyAccessInfo>* result) const;

  bool TryLoadPropertyDetails(MapRef map,
                              base::Optional<JSObjectRef> maybe_holder,
                              NameRef name, InternalIndex* index_out,
                              PropertyDetails* details_out) const;

  CompilationDependencies* dependencies() const { return dependencies_; }
  JSHeapBroker* broker() const { return broker_; }
  Isolate* isolate() const;
  Zone* zone() const { return zone_; }

  JSHeapBroker* const broker_;
  CompilationDependencies* const dependencies_;
  TypeCache const* const type_cache_;
  Zone* const zone_;

  AccessInfoFactory(const AccessInfoFactory&) = delete;
  AccessInfoFactory& operator=(const AccessInfoFactory&) = delete;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_ACCESS_INFO_H_
