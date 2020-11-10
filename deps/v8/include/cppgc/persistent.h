// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_PERSISTENT_H_
#define INCLUDE_CPPGC_PERSISTENT_H_

#include <type_traits>

#include "cppgc/internal/persistent-node.h"
#include "cppgc/internal/pointer-policies.h"
#include "cppgc/source-location.h"
#include "cppgc/type-traits.h"
#include "cppgc/visitor.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

// The basic class from which all Persistent classes are generated.
template <typename T, typename WeaknessPolicy, typename LocationPolicy,
          typename CheckingPolicy>
class BasicPersistent : public LocationPolicy,
                        private WeaknessPolicy,
                        private CheckingPolicy {
 public:
  using typename WeaknessPolicy::IsStrongPersistent;
  using PointeeType = T;

  // Null-state/sentinel constructors.
  BasicPersistent(  // NOLINT
      const SourceLocation& loc = SourceLocation::Current())
      : LocationPolicy(loc) {}

  BasicPersistent(std::nullptr_t,  // NOLINT
                  const SourceLocation& loc = SourceLocation::Current())
      : LocationPolicy(loc) {}

  BasicPersistent(  // NOLINT
      SentinelPointer s, const SourceLocation& loc = SourceLocation::Current())
      : LocationPolicy(loc), raw_(s) {}

  // Raw value contstructors.
  BasicPersistent(T* raw,  // NOLINT
                  const SourceLocation& loc = SourceLocation::Current())
      : LocationPolicy(loc), raw_(raw) {
    if (!IsValid()) return;
    node_ = WeaknessPolicy::GetPersistentRegion(raw_).AllocateNode(
        this, &BasicPersistent::Trace);
    this->CheckPointer(Get());
  }

  BasicPersistent(T& raw,  // NOLINT
                  const SourceLocation& loc = SourceLocation::Current())
      : BasicPersistent(&raw, loc) {}

  // Copy ctor.
  BasicPersistent(const BasicPersistent& other,
                  const SourceLocation& loc = SourceLocation::Current())
      : BasicPersistent(other.Get(), loc) {}

  // Heterogeneous ctor.
  template <typename U, typename OtherWeaknessPolicy,
            typename OtherLocationPolicy, typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicPersistent(  // NOLINT
      const BasicPersistent<U, OtherWeaknessPolicy, OtherLocationPolicy,
                            OtherCheckingPolicy>& other,
      const SourceLocation& loc = SourceLocation::Current())
      : BasicPersistent(other.Get(), loc) {}

  // Move ctor. The heterogeneous move ctor is not supported since e.g.
  // persistent can't reuse persistent node from weak persistent.
  BasicPersistent(
      BasicPersistent&& other,
      const SourceLocation& loc = SourceLocation::Current()) noexcept
      : LocationPolicy(std::move(other)),
        raw_(std::move(other.raw_)),
        node_(std::move(other.node_)) {
    if (!IsValid()) return;
    node_->UpdateOwner(this);
    other.raw_ = nullptr;
    other.node_ = nullptr;
    this->CheckPointer(Get());
  }

  // Constructor from member.
  template <typename U, typename MemberBarrierPolicy,
            typename MemberWeaknessTag, typename MemberCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicPersistent(internal::BasicMember<U, MemberBarrierPolicy,  // NOLINT
                                        MemberWeaknessTag, MemberCheckingPolicy>
                      member,
                  const SourceLocation& loc = SourceLocation::Current())
      : BasicPersistent(member.Get(), loc) {}

  ~BasicPersistent() { Clear(); }

  // Copy assignment.
  BasicPersistent& operator=(const BasicPersistent& other) {
    return operator=(other.Get());
  }

  template <typename U, typename OtherWeaknessPolicy,
            typename OtherLocationPolicy, typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicPersistent& operator=(
      const BasicPersistent<U, OtherWeaknessPolicy, OtherLocationPolicy,
                            OtherCheckingPolicy>& other) {
    return operator=(other.Get());
  }

  // Move assignment.
  BasicPersistent& operator=(BasicPersistent&& other) {
    if (this == &other) return *this;
    Clear();
    LocationPolicy::operator=(std::move(other));
    raw_ = std::move(other.raw_);
    node_ = std::move(other.node_);
    if (!IsValid()) return *this;
    node_->UpdateOwner(this);
    other.raw_ = nullptr;
    other.node_ = nullptr;
    this->CheckPointer(Get());
    return *this;
  }

  // Assignment from member.
  template <typename U, typename MemberBarrierPolicy,
            typename MemberWeaknessTag, typename MemberCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicPersistent& operator=(
      internal::BasicMember<U, MemberBarrierPolicy, MemberWeaknessTag,
                            MemberCheckingPolicy>
          member) {
    return operator=(member.Get());
  }

  BasicPersistent& operator=(T* other) {
    Assign(other);
    return *this;
  }

  BasicPersistent& operator=(std::nullptr_t) {
    Clear();
    return *this;
  }

  BasicPersistent& operator=(SentinelPointer s) {
    Assign(s);
    return *this;
  }

  explicit operator bool() const { return Get(); }
  operator T*() const { return Get(); }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  T* Get() const { return raw_; }

  void Clear() { Assign(nullptr); }

  T* Release() {
    T* result = Get();
    Clear();
    return result;
  }

 private:
  static void Trace(Visitor* v, const void* ptr) {
    const auto* persistent = static_cast<const BasicPersistent*>(ptr);
    v->TraceRoot(*persistent, persistent->Location());
  }

  bool IsValid() const {
    // Ideally, handling kSentinelPointer would be done by the embedder. On the
    // other hand, having Persistent aware of it is beneficial since no node
    // gets wasted.
    return raw_ != nullptr && raw_ != kSentinelPointer;
  }

  void Assign(T* ptr) {
    if (IsValid()) {
      if (ptr && ptr != kSentinelPointer) {
        // Simply assign the pointer reusing the existing node.
        raw_ = ptr;
        this->CheckPointer(ptr);
        return;
      }
      WeaknessPolicy::GetPersistentRegion(raw_).FreeNode(node_);
      node_ = nullptr;
    }
    raw_ = ptr;
    if (!IsValid()) return;
    node_ = WeaknessPolicy::GetPersistentRegion(raw_).AllocateNode(
        this, &BasicPersistent::Trace);
    this->CheckPointer(Get());
  }

  T* raw_ = nullptr;
  PersistentNode* node_ = nullptr;
};

template <typename T1, typename WeaknessPolicy1, typename LocationPolicy1,
          typename CheckingPolicy1, typename T2, typename WeaknessPolicy2,
          typename LocationPolicy2, typename CheckingPolicy2>
bool operator==(const BasicPersistent<T1, WeaknessPolicy1, LocationPolicy1,
                                      CheckingPolicy1>& p1,
                const BasicPersistent<T2, WeaknessPolicy2, LocationPolicy2,
                                      CheckingPolicy2>& p2) {
  return p1.Get() == p2.Get();
}

template <typename T1, typename WeaknessPolicy1, typename LocationPolicy1,
          typename CheckingPolicy1, typename T2, typename WeaknessPolicy2,
          typename LocationPolicy2, typename CheckingPolicy2>
bool operator!=(const BasicPersistent<T1, WeaknessPolicy1, LocationPolicy1,
                                      CheckingPolicy1>& p1,
                const BasicPersistent<T2, WeaknessPolicy2, LocationPolicy2,
                                      CheckingPolicy2>& p2) {
  return !(p1 == p2);
}

template <typename T1, typename PersistentWeaknessPolicy,
          typename PersistentLocationPolicy, typename PersistentCheckingPolicy,
          typename T2, typename MemberWriteBarrierPolicy,
          typename MemberWeaknessTag, typename MemberCheckingPolicy>
bool operator==(const BasicPersistent<T1, PersistentWeaknessPolicy,
                                      PersistentLocationPolicy,
                                      PersistentCheckingPolicy>& p,
                BasicMember<T2, MemberWeaknessTag, MemberWriteBarrierPolicy,
                            MemberCheckingPolicy>
                    m) {
  return p.Get() == m.Get();
}

template <typename T1, typename PersistentWeaknessPolicy,
          typename PersistentLocationPolicy, typename PersistentCheckingPolicy,
          typename T2, typename MemberWriteBarrierPolicy,
          typename MemberWeaknessTag, typename MemberCheckingPolicy>
bool operator!=(const BasicPersistent<T1, PersistentWeaknessPolicy,
                                      PersistentLocationPolicy,
                                      PersistentCheckingPolicy>& p,
                BasicMember<T2, MemberWeaknessTag, MemberWriteBarrierPolicy,
                            MemberCheckingPolicy>
                    m) {
  return !(p == m);
}

template <typename T1, typename MemberWriteBarrierPolicy,
          typename MemberWeaknessTag, typename MemberCheckingPolicy,
          typename T2, typename PersistentWeaknessPolicy,
          typename PersistentLocationPolicy, typename PersistentCheckingPolicy>
bool operator==(BasicMember<T2, MemberWeaknessTag, MemberWriteBarrierPolicy,
                            MemberCheckingPolicy>
                    m,
                const BasicPersistent<T1, PersistentWeaknessPolicy,
                                      PersistentLocationPolicy,
                                      PersistentCheckingPolicy>& p) {
  return m.Get() == p.Get();
}

template <typename T1, typename MemberWriteBarrierPolicy,
          typename MemberWeaknessTag, typename MemberCheckingPolicy,
          typename T2, typename PersistentWeaknessPolicy,
          typename PersistentLocationPolicy, typename PersistentCheckingPolicy>
bool operator!=(BasicMember<T2, MemberWeaknessTag, MemberWriteBarrierPolicy,
                            MemberCheckingPolicy>
                    m,
                const BasicPersistent<T1, PersistentWeaknessPolicy,
                                      PersistentLocationPolicy,
                                      PersistentCheckingPolicy>& p) {
  return !(m == p);
}

template <typename T, typename LocationPolicy, typename CheckingPolicy>
struct IsWeak<BasicPersistent<T, internal::WeakPersistentPolicy, LocationPolicy,
                              CheckingPolicy>> : std::true_type {};
}  // namespace internal

/**
 * Persistent is a way to create a strong pointer from an off-heap object to
 * another on-heap object. As long as the Persistent handle is alive the GC will
 * keep the object pointed to alive. The Persistent handle is always a GC root
 * from the point of view of the GC. Persistent must be constructed and
 * destructed in the same thread.
 */
template <typename T>
using Persistent =
    internal::BasicPersistent<T, internal::StrongPersistentPolicy>;

/**
 * WeakPersistent is a way to create a weak pointer from an off-heap object to
 * an on-heap object. The pointer is automatically cleared when the pointee gets
 * collected. WeakPersistent must be constructed and destructed in the same
 * thread.
 */
template <typename T>
using WeakPersistent =
    internal::BasicPersistent<T, internal::WeakPersistentPolicy>;

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_PERSISTENT_H_
