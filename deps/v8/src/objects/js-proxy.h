// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_PROXY_H_
#define V8_OBJECTS_JS_PROXY_H_

#include "src/objects/js-objects.h"
#include "src/objects/oddball.h"
#include "torque-generated/builtin-definitions.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class KeyAccumulator;

#include "torque-generated/src/objects/js-proxy-tq.inc"

// The JSProxy describes ECMAScript Harmony proxies
V8_OBJECT class JSProxy : public JSReceiver {
 public:
  inline Tagged<UnionOf<JSReceiver, Null>> target() const;
  inline void set_target(Tagged<UnionOf<JSReceiver, Null>> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSReceiver, Null>> handler() const;
  inline void set_handler(Tagged<UnionOf<JSReceiver, Null>> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSProxy> New(
      Isolate* isolate, DirectHandle<Object>, DirectHandle<Object>,
      bool revocable);

  V8_INLINE bool is_revocable() const;
  V8_INLINE bool IsRevoked() const;
  static void Revoke(DirectHandle<JSProxy> proxy);

  // ES6 9.5.1
  static MaybeDirectHandle<JSPrototype> GetPrototype(
      DirectHandle<JSProxy> receiver);

  // ES6 9.5.2
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetPrototype(
      Isolate* isolate, DirectHandle<JSProxy> proxy,
      DirectHandle<JSPrototype> value, bool from_javascript,
      ShouldThrow should_throw);
  // ES6 9.5.3
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsExtensible(
      DirectHandle<JSProxy> proxy);

  // https://tc39.es/ecma262/#sec-isarray.  NOT to be confused with %_IsArray.
  V8_WARN_UNUSED_RESULT static Maybe<bool> IsArray(DirectHandle<JSProxy> proxy);

  // ES6 9.5.4 (when passed kDontThrow)
  V8_WARN_UNUSED_RESULT static Maybe<bool> PreventExtensions(
      DirectHandle<JSProxy> proxy, ShouldThrow should_throw);

  // ES6 9.5.5
  V8_WARN_UNUSED_RESULT static Maybe<bool> GetOwnPropertyDescriptor(
      Isolate* isolate, DirectHandle<JSProxy> proxy, DirectHandle<Name> name,
      PropertyDescriptor* desc);

  // ES6 9.5.6
  V8_WARN_UNUSED_RESULT static Maybe<bool> DefineOwnProperty(
      Isolate* isolate, DirectHandle<JSProxy> object, DirectHandle<Object> key,
      PropertyDescriptor* desc, Maybe<ShouldThrow> should_throw);

  // ES6 9.5.7
  V8_WARN_UNUSED_RESULT static Maybe<bool> HasProperty(
      Isolate* isolate, DirectHandle<JSProxy> proxy, DirectHandle<Name> name);

  // This function never returns false.
  // It returns either true or throws.
  V8_WARN_UNUSED_RESULT static Maybe<bool> CheckHasTrap(
      Isolate* isolate, DirectHandle<Name> name,
      DirectHandle<JSReceiver> target);

  // ES6 9.5.10
  V8_WARN_UNUSED_RESULT static Maybe<bool> CheckDeleteTrap(
      Isolate* isolate, DirectHandle<Name> name,
      DirectHandle<JSReceiver> target);

  // ES6 9.5.8
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSAny> GetProperty(
      Isolate* isolate, DirectHandle<JSProxy> proxy, DirectHandle<Name> name,
      DirectHandle<JSAny> receiver, bool* was_found);

  enum AccessKind { kGet, kSet };

  static MaybeHandle<JSAny> CheckGetSetTrapResult(
      Isolate* isolate, DirectHandle<Name> name,
      DirectHandle<JSReceiver> target, DirectHandle<Object> trap_result,
      AccessKind access_kind);

  // ES6 9.5.9
  V8_WARN_UNUSED_RESULT static Maybe<bool> SetProperty(
      DirectHandle<JSProxy> proxy, DirectHandle<Name> name,
      DirectHandle<Object> value, DirectHandle<JSAny> receiver,
      Maybe<ShouldThrow> should_throw);

  // ES6 9.5.10 (when passed LanguageMode::kSloppy)
  V8_WARN_UNUSED_RESULT static Maybe<bool> DeletePropertyOrElement(
      DirectHandle<JSProxy> proxy, DirectHandle<Name> name,
      LanguageMode language_mode);

  V8_WARN_UNUSED_RESULT static Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  // Dispatched behavior.
  DECL_PRINTER(JSProxy)
  DECL_VERIFIER(JSProxy)

  static const int kMaxIterationLimit = 100 * 1024;

  class BodyDescriptor;

  static Maybe<bool> SetPrivateSymbol(Isolate* isolate,
                                      DirectHandle<JSProxy> proxy,
                                      DirectHandle<Symbol> private_name,
                                      PropertyDescriptor* desc,
                                      Maybe<ShouldThrow> should_throw);

  using IsRevocableBit = base::BitField<bool, 0, 1>;

  static constexpr int kIsRevocableBit = IsRevocableBit::encode(true);

  // Defined out-of-line below the class so `offsetof` / `sizeof` on the
  // still-incomplete type can appear in an initializer.
  static const int kTargetOffset;
  static const int kHandlerOffset;
  static const int kFlagsOffset;
#if TAGGED_SIZE_8_BYTES
  static const int kPaddingOffset;
#endif
  static const int kHeaderSize;
  static const int kSize;

 public:
  TaggedMember<UnionOf<JSReceiver, Null>> target_;
  TaggedMember<UnionOf<JSReceiver, Null>> handler_;
  int32_t flags_;
#if TAGGED_SIZE_8_BYTES
  uint32_t padding_;
#endif
} V8_OBJECT_END;

inline constexpr int JSProxy::kTargetOffset = offsetof(JSProxy, target_);
inline constexpr int JSProxy::kHandlerOffset = offsetof(JSProxy, handler_);
inline constexpr int JSProxy::kFlagsOffset = offsetof(JSProxy, flags_);
#if TAGGED_SIZE_8_BYTES
inline constexpr int JSProxy::kPaddingOffset = offsetof(JSProxy, padding_);
#endif
inline constexpr int JSProxy::kHeaderSize = sizeof(JSProxy);
inline constexpr int JSProxy::kSize = JSProxy::kHeaderSize;

// kTargetOffset aliases with the elements of JSObject. The fact that
// JSProxy::target is a Javascript value which cannot be confused with an
// elements backing store is exploited by loading from this offset from an
// unknown JSReceiver.
static_assert(static_cast<int>(JSObject::kElementsOffset) ==
              static_cast<int>(JSProxy::kTargetOffset));

// JSProxyRevocableResult is just a JSObject with a specific initial map.
// This initial map adds in-object properties for "proxy" and "revoke".
// See https://tc39.es/ecma262/#sec-proxy.revocable
// Shape-style: no own C++ storage; fields live in the parent JSObject's
// in-object property slots at fixed offsets past JSObject::kHeaderSize.
class JSProxyRevocableResult : public JSObject {
 public:
  static constexpr int kProxySlotIndex = 0;
  static constexpr int kRevokeSlotIndex = 1;
  static constexpr int kInObjectPropertyCount = 2;

  static constexpr int kProxyOffset =
      JSObject::kHeaderSize + kProxySlotIndex * kTaggedSize;
  static constexpr int kRevokeOffset =
      JSObject::kHeaderSize + kRevokeSlotIndex * kTaggedSize;
  static constexpr int kSize =
      JSObject::kHeaderSize + kInObjectPropertyCount * kTaggedSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSProxyRevocableResult);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_PROXY_H_
