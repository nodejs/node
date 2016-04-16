// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_ACCESS_INFO_H_
#define V8_COMPILER_ACCESS_INFO_H_

#include <iosfwd>

#include "src/field-index.h"
#include "src/objects.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

// Forward declarations.
class CompilationDependencies;
class Factory;
class TypeCache;


namespace compiler {

// Whether we are loading a property or storing to a property.
enum class AccessMode { kLoad, kStore };

std::ostream& operator<<(std::ostream&, AccessMode);


// Mapping of transition source to transition target.
typedef std::vector<std::pair<Handle<Map>, Handle<Map>>> MapTransitionList;


// This class encapsulates all information required to access a certain element.
class ElementAccessInfo final {
 public:
  ElementAccessInfo();
  ElementAccessInfo(Type* receiver_type, ElementsKind elements_kind,
                    MaybeHandle<JSObject> holder);

  MaybeHandle<JSObject> holder() const { return holder_; }
  ElementsKind elements_kind() const { return elements_kind_; }
  Type* receiver_type() const { return receiver_type_; }
  MapTransitionList& transitions() { return transitions_; }
  MapTransitionList const& transitions() const { return transitions_; }

 private:
  ElementsKind elements_kind_;
  MaybeHandle<JSObject> holder_;
  Type* receiver_type_;
  MapTransitionList transitions_;
};


// Additional checks that need to be perform for data field accesses.
enum class FieldCheck : uint8_t {
  // No additional checking needed.
  kNone,
  // Check that the [[ViewedArrayBuffer]] of {JSArrayBufferView}s
  // was not neutered.
  kJSArrayBufferViewBufferNotNeutered,
};


// This class encapsulates all information required to access a certain
// object property, either on the object itself or on the prototype chain.
class PropertyAccessInfo final {
 public:
  enum Kind { kInvalid, kNotFound, kDataConstant, kDataField };

  static PropertyAccessInfo NotFound(Type* receiver_type,
                                     MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataConstant(Type* receiver_type,
                                         Handle<Object> constant,
                                         MaybeHandle<JSObject> holder);
  static PropertyAccessInfo DataField(
      Type* receiver_type, FieldIndex field_index, Type* field_type,
      FieldCheck field_check = FieldCheck::kNone,
      MaybeHandle<JSObject> holder = MaybeHandle<JSObject>(),
      MaybeHandle<Map> transition_map = MaybeHandle<Map>());

  PropertyAccessInfo();

  bool IsNotFound() const { return kind() == kNotFound; }
  bool IsDataConstant() const { return kind() == kDataConstant; }
  bool IsDataField() const { return kind() == kDataField; }

  bool HasTransitionMap() const { return !transition_map().is_null(); }

  Kind kind() const { return kind_; }
  MaybeHandle<JSObject> holder() const { return holder_; }
  MaybeHandle<Map> transition_map() const { return transition_map_; }
  Handle<Object> constant() const { return constant_; }
  FieldCheck field_check() const { return field_check_; }
  FieldIndex field_index() const { return field_index_; }
  Type* field_type() const { return field_type_; }
  Type* receiver_type() const { return receiver_type_; }

 private:
  PropertyAccessInfo(MaybeHandle<JSObject> holder, Type* receiver_type);
  PropertyAccessInfo(MaybeHandle<JSObject> holder, Handle<Object> constant,
                     Type* receiver_type);
  PropertyAccessInfo(MaybeHandle<JSObject> holder,
                     MaybeHandle<Map> transition_map, FieldIndex field_index,
                     FieldCheck field_check, Type* field_type,
                     Type* receiver_type);

  Kind kind_;
  Type* receiver_type_;
  Handle<Object> constant_;
  MaybeHandle<Map> transition_map_;
  MaybeHandle<JSObject> holder_;
  FieldIndex field_index_;
  FieldCheck field_check_;
  Type* field_type_;
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
