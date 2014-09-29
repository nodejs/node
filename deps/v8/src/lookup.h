// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_LOOKUP_H_
#define V8_LOOKUP_H_

#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class LookupIterator V8_FINAL BASE_EMBEDDED {
 public:
  enum Configuration {
    CHECK_OWN_REAL     = 0,
    CHECK_HIDDEN       = 1 << 0,
    CHECK_DERIVED      = 1 << 1,
    CHECK_INTERCEPTOR  = 1 << 2,
    CHECK_ACCESS_CHECK = 1 << 3,
    CHECK_ALL          = CHECK_HIDDEN | CHECK_DERIVED |
                         CHECK_INTERCEPTOR | CHECK_ACCESS_CHECK,
    SKIP_INTERCEPTOR   = CHECK_ALL ^ CHECK_INTERCEPTOR,
    CHECK_OWN          = CHECK_ALL ^ CHECK_DERIVED
  };

  enum State {
    NOT_FOUND,
    PROPERTY,
    INTERCEPTOR,
    ACCESS_CHECK,
    JSPROXY
  };

  enum PropertyKind {
    DATA,
    ACCESSOR
  };

  enum PropertyEncoding {
    DICTIONARY,
    DESCRIPTOR
  };

  LookupIterator(Handle<Object> receiver,
                 Handle<Name> name,
                 Configuration configuration = CHECK_ALL)
      : configuration_(ComputeConfiguration(configuration, name)),
        state_(NOT_FOUND),
        property_kind_(DATA),
        property_encoding_(DESCRIPTOR),
        property_details_(NONE, NONEXISTENT, Representation::None()),
        isolate_(name->GetIsolate()),
        name_(name),
        maybe_receiver_(receiver),
        number_(DescriptorArray::kNotFound) {
    Handle<JSReceiver> root = GetRoot();
    holder_map_ = handle(root->map());
    maybe_holder_ = root;
    Next();
  }

  LookupIterator(Handle<Object> receiver,
                 Handle<Name> name,
                 Handle<JSReceiver> holder,
                 Configuration configuration = CHECK_ALL)
      : configuration_(ComputeConfiguration(configuration, name)),
        state_(NOT_FOUND),
        property_kind_(DATA),
        property_encoding_(DESCRIPTOR),
        property_details_(NONE, NONEXISTENT, Representation::None()),
        isolate_(name->GetIsolate()),
        name_(name),
        holder_map_(holder->map()),
        maybe_receiver_(receiver),
        maybe_holder_(holder),
        number_(DescriptorArray::kNotFound) {
    Next();
  }

  Isolate* isolate() const { return isolate_; }
  State state() const { return state_; }
  Handle<Name> name() const { return name_; }

  bool IsFound() const { return state_ != NOT_FOUND; }
  void Next();

  Heap* heap() const { return isolate_->heap(); }
  Factory* factory() const { return isolate_->factory(); }
  Handle<Object> GetReceiver() const {
    return Handle<Object>::cast(maybe_receiver_.ToHandleChecked());
  }
  Handle<Map> holder_map() const { return holder_map_; }
  template <class T>
  Handle<T> GetHolder() const {
    DCHECK(IsFound());
    return Handle<T>::cast(maybe_holder_.ToHandleChecked());
  }
  Handle<JSReceiver> GetRoot() const;
  bool HolderIsReceiverOrHiddenPrototype() const;

  /* Dynamically reduce the trapped types. */
  void skip_interceptor() {
    configuration_ = static_cast<Configuration>(
        configuration_ & ~CHECK_INTERCEPTOR);
  }
  void skip_access_check() {
    configuration_ = static_cast<Configuration>(
        configuration_ & ~CHECK_ACCESS_CHECK);
  }

  /* ACCESS_CHECK */
  bool HasAccess(v8::AccessType access_type) const;

  /* PROPERTY */
  // HasProperty needs to be called before any of the other PROPERTY methods
  // below can be used. It ensures that we are able to provide a definite
  // answer, and loads extra information about the property.
  bool HasProperty();
  void PrepareForDataProperty(Handle<Object> value);
  void TransitionToDataProperty(Handle<Object> value,
                                PropertyAttributes attributes,
                                Object::StoreFromKeyed store_mode);
  PropertyKind property_kind() const {
    DCHECK(has_property_);
    return property_kind_;
  }
  PropertyEncoding property_encoding() const {
    DCHECK(has_property_);
    return property_encoding_;
  }
  PropertyDetails property_details() const {
    DCHECK(has_property_);
    return property_details_;
  }
  bool IsConfigurable() const { return !property_details().IsDontDelete(); }
  Representation representation() const {
    return property_details().representation();
  }
  FieldIndex GetFieldIndex() const;
  int GetConstantIndex() const;
  Handle<PropertyCell> GetPropertyCell() const;
  Handle<Object> GetAccessors() const;
  Handle<Object> GetDataValue() const;
  void WriteDataValue(Handle<Object> value);

  void InternalizeName();

 private:
  Handle<Map> GetReceiverMap() const;

  MUST_USE_RESULT inline JSReceiver* NextHolder(Map* map);
  inline State LookupInHolder(Map* map);
  Handle<Object> FetchValue() const;

  bool IsBootstrapping() const;

  // Methods that fetch data from the holder ensure they always have a holder.
  // This means the receiver needs to be present as opposed to just the receiver
  // map. Other objects in the prototype chain are transitively guaranteed to be
  // present via the receiver map.
  bool is_guaranteed_to_have_holder() const {
    return !maybe_receiver_.is_null();
  }
  bool check_interceptor() const {
    return !IsBootstrapping() && (configuration_ & CHECK_INTERCEPTOR) != 0;
  }
  bool check_derived() const {
    return (configuration_ & CHECK_DERIVED) != 0;
  }
  bool check_hidden() const {
    return (configuration_ & CHECK_HIDDEN) != 0;
  }
  bool check_access_check() const {
    return (configuration_ & CHECK_ACCESS_CHECK) != 0;
  }
  int descriptor_number() const {
    DCHECK(has_property_);
    DCHECK_EQ(DESCRIPTOR, property_encoding_);
    return number_;
  }
  int dictionary_entry() const {
    DCHECK(has_property_);
    DCHECK_EQ(DICTIONARY, property_encoding_);
    return number_;
  }

  static Configuration ComputeConfiguration(
      Configuration configuration, Handle<Name> name) {
    if (name->IsOwn()) {
      return static_cast<Configuration>(configuration & CHECK_OWN);
    } else {
      return configuration;
    }
  }

  Configuration configuration_;
  State state_;
  bool has_property_;
  PropertyKind property_kind_;
  PropertyEncoding property_encoding_;
  PropertyDetails property_details_;
  Isolate* isolate_;
  Handle<Name> name_;
  Handle<Map> holder_map_;
  MaybeHandle<Object> maybe_receiver_;
  MaybeHandle<JSReceiver> maybe_holder_;

  int number_;
};


} }  // namespace v8::internal

#endif  // V8_LOOKUP_H_
