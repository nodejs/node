// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_CROSS_THREAD_PERSISTENT_H_
#define INCLUDE_CPPGC_CROSS_THREAD_PERSISTENT_H_

#include <atomic>

#include "cppgc/internal/persistent-node.h"
#include "cppgc/internal/pointer-policies.h"
#include "cppgc/persistent.h"
#include "cppgc/visitor.h"

namespace cppgc {

namespace internal {

template <typename T, typename WeaknessPolicy, typename LocationPolicy,
          typename CheckingPolicy>
class BasicCrossThreadPersistent final : public PersistentBase,
                                         public LocationPolicy,
                                         private WeaknessPolicy,
                                         private CheckingPolicy {
 public:
  using typename WeaknessPolicy::IsStrongPersistent;
  using PointeeType = T;

  ~BasicCrossThreadPersistent() { Clear(); }

  BasicCrossThreadPersistent(  // NOLINT
      const SourceLocation& loc = SourceLocation::Current())
      : LocationPolicy(loc) {}

  BasicCrossThreadPersistent(  // NOLINT
      std::nullptr_t, const SourceLocation& loc = SourceLocation::Current())
      : LocationPolicy(loc) {}

  BasicCrossThreadPersistent(  // NOLINT
      SentinelPointer s, const SourceLocation& loc = SourceLocation::Current())
      : PersistentBase(s), LocationPolicy(loc) {}

  BasicCrossThreadPersistent(  // NOLINT
      T* raw, const SourceLocation& loc = SourceLocation::Current())
      : PersistentBase(raw), LocationPolicy(loc) {
    if (!IsValid(raw)) return;
    PersistentRegionLock guard;
    PersistentRegion& region = this->GetPersistentRegion(raw);
    SetNode(region.AllocateNode(this, &Trace));
    this->CheckPointer(raw);
  }

  BasicCrossThreadPersistent(  // NOLINT
      T& raw, const SourceLocation& loc = SourceLocation::Current())
      : BasicCrossThreadPersistent(&raw, loc) {}

  template <typename U, typename MemberBarrierPolicy,
            typename MemberWeaknessTag, typename MemberCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicCrossThreadPersistent(  // NOLINT
      internal::BasicMember<U, MemberBarrierPolicy, MemberWeaknessTag,
                            MemberCheckingPolicy>
          member,
      const SourceLocation& loc = SourceLocation::Current())
      : BasicCrossThreadPersistent(member.Get(), loc) {}

  BasicCrossThreadPersistent(
      const BasicCrossThreadPersistent& other,
      const SourceLocation& loc = SourceLocation::Current())
      : BasicCrossThreadPersistent(loc) {
    // Invoke operator=.
    *this = other;
  }

  // Heterogeneous ctor.
  template <typename U, typename OtherWeaknessPolicy,
            typename OtherLocationPolicy, typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicCrossThreadPersistent(  // NOLINT
      const BasicCrossThreadPersistent<U, OtherWeaknessPolicy,
                                       OtherLocationPolicy,
                                       OtherCheckingPolicy>& other,
      const SourceLocation& loc = SourceLocation::Current())
      : BasicCrossThreadPersistent(loc) {
    *this = other;
  }

  BasicCrossThreadPersistent(
      BasicCrossThreadPersistent&& other,
      const SourceLocation& loc = SourceLocation::Current()) noexcept {
    // Invoke operator=.
    *this = std::move(other);
  }

  BasicCrossThreadPersistent& operator=(
      const BasicCrossThreadPersistent& other) {
    PersistentRegionLock guard;
    AssignUnsafe(other.Get());
    return *this;
  }

  template <typename U, typename OtherWeaknessPolicy,
            typename OtherLocationPolicy, typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicCrossThreadPersistent& operator=(
      const BasicCrossThreadPersistent<U, OtherWeaknessPolicy,
                                       OtherLocationPolicy,
                                       OtherCheckingPolicy>& other) {
    PersistentRegionLock guard;
    AssignUnsafe(other.Get());
    return *this;
  }

  BasicCrossThreadPersistent& operator=(BasicCrossThreadPersistent&& other) {
    if (this == &other) return *this;
    Clear();
    PersistentRegionLock guard;
    PersistentBase::operator=(std::move(other));
    LocationPolicy::operator=(std::move(other));
    if (!IsValid(GetValue())) return *this;
    GetNode()->UpdateOwner(this);
    other.SetValue(nullptr);
    other.SetNode(nullptr);
    this->CheckPointer(GetValue());
    return *this;
  }

  BasicCrossThreadPersistent& operator=(T* other) {
    Assign(other);
    return *this;
  }

  // Assignment from member.
  template <typename U, typename MemberBarrierPolicy,
            typename MemberWeaknessTag, typename MemberCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicCrossThreadPersistent& operator=(
      internal::BasicMember<U, MemberBarrierPolicy, MemberWeaknessTag,
                            MemberCheckingPolicy>
          member) {
    return operator=(member.Get());
  }

  BasicCrossThreadPersistent& operator=(std::nullptr_t) {
    Clear();
    return *this;
  }

  BasicCrossThreadPersistent& operator=(SentinelPointer s) {
    Assign(s);
    return *this;
  }

  /**
   * Returns a pointer to the stored object.
   *
   * Note: **Not thread-safe.**
   *
   * \returns a pointer to the stored object.
   */
  // CFI cast exemption to allow passing SentinelPointer through T* and support
  // heterogeneous assignments between different Member and Persistent handles
  // based on their actual types.
  V8_CLANG_NO_SANITIZE("cfi-unrelated-cast") T* Get() const {
    return static_cast<T*>(GetValue());
  }

  /**
   * Clears the stored object.
   */
  void Clear() { Assign(nullptr); }

  /**
   * Returns a pointer to the stored object and releases it.
   *
   * Note: **Not thread-safe.**
   *
   * \returns a pointer to the stored object.
   */
  T* Release() {
    T* result = Get();
    Clear();
    return result;
  }

  /**
   * Conversio to boolean.
   *
   * Note: **Not thread-safe.**
   *
   * \returns true if an actual object has been stored and false otherwise.
   */
  explicit operator bool() const { return Get(); }

  /**
   * Conversion to object of type T.
   *
   * Note: **Not thread-safe.**
   *
   * \returns the object.
   */
  operator T*() const { return Get(); }  // NOLINT

  /**
   * Dereferences the stored object.
   *
   * Note: **Not thread-safe.**
   */
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

 private:
  static bool IsValid(void* ptr) { return ptr && ptr != kSentinelPointer; }

  static void Trace(Visitor* v, const void* ptr) {
    const auto* handle = static_cast<const BasicCrossThreadPersistent*>(ptr);
    v->TraceRoot(*handle, handle->Location());
  }

  void Assign(T* ptr) {
    void* old_value = GetValue();
    if (IsValid(old_value)) {
      PersistentRegionLock guard;
      PersistentRegion& region = this->GetPersistentRegion(old_value);
      if (IsValid(ptr) && (&region == &this->GetPersistentRegion(ptr))) {
        SetValue(ptr);
        this->CheckPointer(ptr);
        return;
      }
      region.FreeNode(GetNode());
      SetNode(nullptr);
    }
    SetValue(ptr);
    if (!IsValid(ptr)) return;
    PersistentRegionLock guard;
    SetNode(this->GetPersistentRegion(ptr).AllocateNode(this, &Trace));
    this->CheckPointer(ptr);
  }

  void AssignUnsafe(T* ptr) {
    void* old_value = GetValue();
    if (IsValid(old_value)) {
      PersistentRegion& region = this->GetPersistentRegion(old_value);
      if (IsValid(ptr) && (&region == &this->GetPersistentRegion(ptr))) {
        SetValue(ptr);
        this->CheckPointer(ptr);
        return;
      }
      region.FreeNode(GetNode());
      SetNode(nullptr);
    }
    SetValue(ptr);
    if (!IsValid(ptr)) return;
    SetNode(this->GetPersistentRegion(ptr).AllocateNode(this, &Trace));
    this->CheckPointer(ptr);
  }

  void ClearFromGC() const {
    if (IsValid(GetValue())) {
      WeaknessPolicy::GetPersistentRegion(GetValue()).FreeNode(GetNode());
      PersistentBase::ClearFromGC();
    }
  }

  friend class cppgc::Visitor;
};

template <typename T, typename LocationPolicy, typename CheckingPolicy>
struct IsWeak<
    BasicCrossThreadPersistent<T, internal::WeakCrossThreadPersistentPolicy,
                               LocationPolicy, CheckingPolicy>>
    : std::true_type {};

}  // namespace internal

namespace subtle {

/**
 * **DO NOT USE: Has known caveats, see below.**
 *
 * CrossThreadPersistent allows retaining objects from threads other than the
 * thread the owning heap is operating on.
 *
 * Known caveats:
 * - Does not protect the heap owning an object from terminating.
 * - Reaching transitively through the graph is unsupported as objects may be
 *   moved concurrently on the thread owning the object.
 */
template <typename T>
using CrossThreadPersistent = internal::BasicCrossThreadPersistent<
    T, internal::StrongCrossThreadPersistentPolicy>;

/**
 * **DO NOT USE: Has known caveats, see below.**
 *
 * CrossThreadPersistent allows weakly retaining objects from threads other than
 * the thread the owning heap is operating on.
 *
 * Known caveats:
 * - Does not protect the heap owning an object from terminating.
 * - Reaching transitively through the graph is unsupported as objects may be
 *   moved concurrently on the thread owning the object.
 */
template <typename T>
using WeakCrossThreadPersistent = internal::BasicCrossThreadPersistent<
    T, internal::WeakCrossThreadPersistentPolicy>;

}  // namespace subtle
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_CROSS_THREAD_PERSISTENT_H_
