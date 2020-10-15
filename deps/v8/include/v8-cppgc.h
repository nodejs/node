// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_CPPGC_H_
#define INCLUDE_V8_CPPGC_H_

#include "cppgc/visitor.h"
#include "v8-internal.h"  // NOLINT(build/include_directory)
#include "v8.h"  // NOLINT(build/include_directory)

namespace v8 {

class Isolate;
template <typename T>
class JSMember;

namespace internal {

class JSMemberBaseExtractor;

class V8_EXPORT JSMemberBase {
 public:
  /**
   * Returns true if the reference is empty, i.e., has not been assigned
   * object.
   */
  bool IsEmpty() const { return val_ == nullptr; }

  /**
   * Clears the reference. IsEmpty() will return true after this call.
   */
  inline void Reset();

 private:
  static internal::Address* New(v8::Isolate* isolate,
                                internal::Address* object_slot,
                                internal::Address** this_slot);
  static void Delete(internal::Address* object);
  static void Copy(const internal::Address* const* from_slot,
                   internal::Address** to_slot);
  static void Move(internal::Address** from_slot, internal::Address** to_slot);

  JSMemberBase() = default;

  JSMemberBase(v8::Isolate* isolate, internal::Address* object_slot)
      : val_(New(isolate, object_slot, &val_)) {}

  inline JSMemberBase& CopyImpl(const JSMemberBase& other);
  inline JSMemberBase& MoveImpl(JSMemberBase&& other);

  // val_ points to a GlobalHandles node.
  internal::Address* val_ = nullptr;

  template <typename T>
  friend class v8::JSMember;
  friend class v8::internal::JSMemberBaseExtractor;
};

JSMemberBase& JSMemberBase::CopyImpl(const JSMemberBase& other) {
  if (this != &other) {
    Reset();
    if (!other.IsEmpty()) {
      Copy(&other.val_, &val_);
    }
  }
  return *this;
}

JSMemberBase& JSMemberBase::MoveImpl(JSMemberBase&& other) {
  if (this != &other) {
    // No call to Reset() as Move() will conditionally reset itself when needed,
    // and otherwise reuse the internal meta data.
    Move(&other.val_, &val_);
  }
  return *this;
}

void JSMemberBase::Reset() {
  if (IsEmpty()) return;
  Delete(val_);
  val_ = nullptr;
}

}  // namespace internal

/**
 * A traced handle without destructor that clears the handle. The handle may
 * only be used in GarbageCollected objects and must be processed in a Trace()
 * method.
 */
template <typename T>
class V8_EXPORT JSMember : public internal::JSMemberBase {
  static_assert(std::is_base_of<v8::Value, T>::value,
                "JSMember only supports references to v8::Value");

 public:
  JSMember() = default;

  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  JSMember(Isolate* isolate, Local<U> that)
      : internal::JSMemberBase(isolate,
                               reinterpret_cast<internal::Address*>(*that)) {}

  JSMember(const JSMember& other) { CopyImpl(other); }

  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  JSMember(const JSMember<U>& other) {  // NOLINT
    CopyImpl(other);
  }

  JSMember(JSMember&& other) { MoveImpl(std::move(other)); }

  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  JSMember(JSMember<U>&& other) {  // NOLINT
    MoveImpl(std::move(other));
  }

  JSMember& operator=(const JSMember& other) { return CopyImpl(other); }

  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  JSMember& operator=(const JSMember<U>& other) {
    return CopyImpl(other);
  }

  JSMember& operator=(JSMember&& other) { return MoveImpl(other); }

  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  JSMember& operator=(JSMember<U>&& other) {
    return MoveImpl(other);
  }

  T* operator->() const { return reinterpret_cast<T*>(val_); }
  T* operator*() const { return reinterpret_cast<T*>(val_); }

  using internal::JSMemberBase::Reset;

  template <typename U,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  void Set(v8::Isolate* isolate, Local<U> that) {
    Reset();
    val_ = New(isolate, reinterpret_cast<internal::Address*>(*that), &val_);
  }
};

template <typename T1, typename T2,
          typename = std::enable_if_t<std::is_base_of<T2, T1>::value ||
                                      std::is_base_of<T1, T2>::value>>
inline bool operator==(const JSMember<T1>& lhs, const JSMember<T2>& rhs) {
  v8::internal::Address* a = reinterpret_cast<v8::internal::Address*>(*lhs);
  v8::internal::Address* b = reinterpret_cast<v8::internal::Address*>(*rhs);
  if (a == nullptr) return b == nullptr;
  if (b == nullptr) return false;
  return *a == *b;
}

template <typename T1, typename T2,
          typename = std::enable_if_t<std::is_base_of<T2, T1>::value ||
                                      std::is_base_of<T1, T2>::value>>
inline bool operator!=(const JSMember<T1>& lhs, const JSMember<T2>& rhs) {
  return !(lhs == rhs);
}

template <typename T1, typename T2,
          typename = std::enable_if_t<std::is_base_of<T2, T1>::value ||
                                      std::is_base_of<T1, T2>::value>>
inline bool operator==(const JSMember<T1>& lhs, const Local<T2>& rhs) {
  v8::internal::Address* a = reinterpret_cast<v8::internal::Address*>(*lhs);
  v8::internal::Address* b = reinterpret_cast<v8::internal::Address*>(*rhs);
  if (a == nullptr) return b == nullptr;
  if (b == nullptr) return false;
  return *a == *b;
}

template <typename T1, typename T2,
          typename = std::enable_if_t<std::is_base_of<T2, T1>::value ||
                                      std::is_base_of<T1, T2>::value>>
inline bool operator==(const Local<T1>& lhs, const JSMember<T2> rhs) {
  return rhs == lhs;
}

template <typename T1, typename T2>
inline bool operator!=(const JSMember<T1>& lhs, const T2& rhs) {
  return !(lhs == rhs);
}

template <typename T1, typename T2>
inline bool operator!=(const T1& lhs, const JSMember<T2>& rhs) {
  return !(lhs == rhs);
}

class JSVisitor : public cppgc::Visitor {
 public:
  explicit JSVisitor(cppgc::Visitor::Key key) : cppgc::Visitor(key) {}

  template <typename T>
  void Trace(const JSMember<T>& ref) {
    if (ref.IsEmpty()) return;
    Visit(ref);
  }

 protected:
  using cppgc::Visitor::Visit;

  virtual void Visit(const internal::JSMemberBase& ref) {}
};

}  // namespace v8

namespace cppgc {

template <typename T>
struct TraceTrait<v8::JSMember<T>> {
  static void Trace(Visitor* visitor, const v8::JSMember<T>* self) {
    static_cast<v8::JSVisitor*>(visitor)->Trace(*self);
  }
};

}  // namespace cppgc

#endif  // INCLUDE_V8_CPPGC_H_
