// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROTOTYPE_H_
#define V8_PROTOTYPE_H_

#include "src/isolate.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

/**
 * A class to uniformly access the prototype of any Object and walk its
 * prototype chain.
 *
 * The PrototypeIterator can either start at the prototype (default), or
 * include the receiver itself. If a PrototypeIterator is constructed for a
 * Map, it will always start at the prototype.
 *
 * The PrototypeIterator can either run to the null_value(), the first
 * non-hidden prototype, or a given object.
 */
class PrototypeIterator {
 public:
  enum WhereToStart { START_AT_RECEIVER, START_AT_PROTOTYPE };

  enum WhereToEnd { END_AT_NULL, END_AT_NON_HIDDEN };

  const int kProxyPrototypeLimit = 100 * 1000;

  PrototypeIterator(Isolate* isolate, Handle<Object> receiver,
                    WhereToStart where_to_start = START_AT_PROTOTYPE)
      : did_jump_to_prototype_chain_(false),
        object_(NULL),
        handle_(receiver),
        isolate_(isolate),
        seen_proxies_(0) {
    CHECK(!handle_.is_null());
    if (where_to_start == START_AT_PROTOTYPE) {
      Advance();
    }
  }

  PrototypeIterator(Isolate* isolate, Object* receiver,
                    WhereToStart where_to_start = START_AT_PROTOTYPE)
      : did_jump_to_prototype_chain_(false),
        object_(receiver),
        isolate_(isolate),
        seen_proxies_(0) {
    if (where_to_start == START_AT_PROTOTYPE) {
      Advance();
    }
  }

  explicit PrototypeIterator(Map* receiver_map)
      : did_jump_to_prototype_chain_(true),
        object_(receiver_map->prototype()),
        isolate_(receiver_map->GetIsolate()) {}

  explicit PrototypeIterator(Handle<Map> receiver_map)
      : did_jump_to_prototype_chain_(true),
        object_(NULL),
        handle_(handle(receiver_map->prototype(), receiver_map->GetIsolate())),
        isolate_(receiver_map->GetIsolate()) {}

  ~PrototypeIterator() {}

  bool HasAccess() const {
    // We can only perform access check in the handlified version of the
    // PrototypeIterator.
    DCHECK(!handle_.is_null());
    if (handle_->IsAccessCheckNeeded()) {
      return isolate_->MayAccess(handle(isolate_->context()),
                                 Handle<JSObject>::cast(handle_));
    }
    return true;
  }

  template <typename T = Object>
  T* GetCurrent() const {
    DCHECK(handle_.is_null());
    return T::cast(object_);
  }

  template <typename T = Object>
  static Handle<T> GetCurrent(const PrototypeIterator& iterator) {
    DCHECK(!iterator.handle_.is_null());
    DCHECK(iterator.object_ == NULL);
    return Handle<T>::cast(iterator.handle_);
  }

  void Advance() {
    if (handle_.is_null() && object_->IsJSProxy()) {
      did_jump_to_prototype_chain_ = true;
      object_ = isolate_->heap()->null_value();
      return;
    } else if (!handle_.is_null() && handle_->IsJSProxy()) {
      did_jump_to_prototype_chain_ = true;
      handle_ = handle(isolate_->heap()->null_value(), isolate_);
      return;
    }
    AdvanceIgnoringProxies();
  }

  void AdvanceIgnoringProxies() {
    if (!did_jump_to_prototype_chain_) {
      did_jump_to_prototype_chain_ = true;
      if (handle_.is_null()) {
        object_ = object_->GetRootMap(isolate_)->prototype();
      } else {
        handle_ = handle(handle_->GetRootMap(isolate_)->prototype(), isolate_);
      }
    } else {
      if (handle_.is_null()) {
        object_ = HeapObject::cast(object_)->map()->prototype();
      } else {
        handle_ =
            handle(HeapObject::cast(*handle_)->map()->prototype(), isolate_);
      }
    }
  }

  // Returns false iff a call to JSProxy::GetPrototype throws.
  // TODO(neis): This should probably replace Advance().
  bool AdvanceFollowingProxies() {
    DCHECK(!(handle_.is_null() && object_->IsJSProxy()));
    if (!HasAccess()) {
      // Abort the lookup if we do not have access to the current object.
      handle_ = isolate_->factory()->null_value();
      return true;
    }
    if (handle_.is_null() || !handle_->IsJSProxy()) {
      AdvanceIgnoringProxies();
      return true;
    }
    // Due to possible __proto__ recursion limit the number of Proxies
    // we visit to an arbitrarily chosen large number.
    seen_proxies_++;
    if (seen_proxies_ > kProxyPrototypeLimit) {
      isolate_->Throw(
          *isolate_->factory()->NewRangeError(MessageTemplate::kStackOverflow));
      return false;
    }
    did_jump_to_prototype_chain_ = true;
    MaybeHandle<Object> proto =
        JSProxy::GetPrototype(Handle<JSProxy>::cast(handle_));
    return proto.ToHandle(&handle_);
  }

  bool IsAtEnd(WhereToEnd where_to_end = END_AT_NULL) const {
    if (handle_.is_null()) {
      return object_->IsNull() ||
             (did_jump_to_prototype_chain_ &&
              where_to_end == END_AT_NON_HIDDEN &&
              !HeapObject::cast(object_)->map()->is_hidden_prototype());
    } else {
      return handle_->IsNull() ||
             (did_jump_to_prototype_chain_ &&
              where_to_end == END_AT_NON_HIDDEN &&
              !Handle<HeapObject>::cast(handle_)->map()->is_hidden_prototype());
    }
  }

  bool IsAtEnd(Object* final_object) {
    DCHECK(handle_.is_null());
    return object_->IsNull() || object_ == final_object;
  }

  bool IsAtEnd(Handle<Object> final_object) {
    DCHECK(!handle_.is_null());
    return handle_->IsNull() || *handle_ == *final_object;
  }

 private:
  bool did_jump_to_prototype_chain_;
  Object* object_;
  Handle<Object> handle_;
  Isolate* isolate_;
  int seen_proxies_;

  DISALLOW_COPY_AND_ASSIGN(PrototypeIterator);
};


}  // namespace internal

}  // namespace v8

#endif  // V8_PROTOTYPE_H_
