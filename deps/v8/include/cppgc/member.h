// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_MEMBER_H_
#define INCLUDE_CPPGC_MEMBER_H_

#include <atomic>
#include <cstddef>
#include <type_traits>

#include "cppgc/internal/pointer-policies.h"
#include "cppgc/sentinel-pointer.h"
#include "cppgc/type-traits.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

class Visitor;

namespace internal {

// MemberBase always refers to the object as const object and defers to
// BasicMember on casting to the right type as needed.
class MemberBase {
 protected:
  struct AtomicInitializerTag {};

  MemberBase() = default;
  explicit MemberBase(const void* value) : raw_(value) {}
  MemberBase(const void* value, AtomicInitializerTag) { SetRawAtomic(value); }

  const void** GetRawSlot() const { return &raw_; }
  const void* GetRaw() const { return raw_; }
  void SetRaw(void* value) { raw_ = value; }

  const void* GetRawAtomic() const {
    return reinterpret_cast<const std::atomic<const void*>*>(&raw_)->load(
        std::memory_order_relaxed);
  }
  void SetRawAtomic(const void* value) {
    reinterpret_cast<std::atomic<const void*>*>(&raw_)->store(
        value, std::memory_order_relaxed);
  }

  void ClearFromGC() const { raw_ = nullptr; }

 private:
  mutable const void* raw_ = nullptr;
};

// The basic class from which all Member classes are 'generated'.
template <typename T, typename WeaknessTag, typename WriteBarrierPolicy,
          typename CheckingPolicy>
class BasicMember final : private MemberBase, private CheckingPolicy {
 public:
  using PointeeType = T;

  constexpr BasicMember() = default;
  constexpr BasicMember(std::nullptr_t) {}     // NOLINT
  BasicMember(SentinelPointer s) : MemberBase(s) {}  // NOLINT
  BasicMember(T* raw) : MemberBase(raw) {            // NOLINT
    InitializingWriteBarrier();
    this->CheckPointer(Get());
  }
  BasicMember(T& raw) : BasicMember(&raw) {}  // NOLINT
  // Atomic ctor. Using the AtomicInitializerTag forces BasicMember to
  // initialize using atomic assignments. This is required for preventing
  // data races with concurrent marking.
  using AtomicInitializerTag = MemberBase::AtomicInitializerTag;
  BasicMember(std::nullptr_t, AtomicInitializerTag atomic)
      : MemberBase(nullptr, atomic) {}
  BasicMember(SentinelPointer s, AtomicInitializerTag atomic)
      : MemberBase(s, atomic) {}
  BasicMember(T* raw, AtomicInitializerTag atomic) : MemberBase(raw, atomic) {
    InitializingWriteBarrier();
    this->CheckPointer(Get());
  }
  BasicMember(T& raw, AtomicInitializerTag atomic)
      : BasicMember(&raw, atomic) {}
  // Copy ctor.
  BasicMember(const BasicMember& other) : BasicMember(other.Get()) {}
  // Allow heterogeneous construction.
  template <typename U, typename OtherBarrierPolicy, typename OtherWeaknessTag,
            typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember(  // NOLINT
      const BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other)
      : BasicMember(other.Get()) {}
  // Move ctor.
  BasicMember(BasicMember&& other) noexcept : BasicMember(other.Get()) {
    other.Clear();
  }
  // Allow heterogeneous move construction.
  template <typename U, typename OtherBarrierPolicy, typename OtherWeaknessTag,
            typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember(BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                          OtherCheckingPolicy>&& other) noexcept
      : BasicMember(other.Get()) {
    other.Clear();
  }
  // Construction from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember(const BasicPersistent<U, PersistentWeaknessPolicy,
                                    PersistentLocationPolicy,
                                    PersistentCheckingPolicy>& p)
      : BasicMember(p.Get()) {}

  // Copy assignment.
  BasicMember& operator=(const BasicMember& other) {
    return operator=(other.Get());
  }
  // Allow heterogeneous copy assignment.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember& operator=(
      const BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other) {
    return operator=(other.Get());
  }
  // Move assignment.
  BasicMember& operator=(BasicMember&& other) noexcept {
    operator=(other.Get());
    other.Clear();
    return *this;
  }
  // Heterogeneous move assignment.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember& operator=(BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                                     OtherCheckingPolicy>&& other) noexcept {
    operator=(other.Get());
    other.Clear();
    return *this;
  }
  // Assignment from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember& operator=(
      const BasicPersistent<U, PersistentWeaknessPolicy,
                            PersistentLocationPolicy, PersistentCheckingPolicy>&
          other) {
    return operator=(other.Get());
  }
  BasicMember& operator=(T* other) {
    SetRawAtomic(other);
    AssigningWriteBarrier();
    this->CheckPointer(Get());
    return *this;
  }
  BasicMember& operator=(std::nullptr_t) {
    Clear();
    return *this;
  }
  BasicMember& operator=(SentinelPointer s) {
    SetRawAtomic(s);
    return *this;
  }

  template <typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy>
  void Swap(BasicMember<T, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other) {
    T* tmp = Get();
    *this = other;
    other = tmp;
  }

  explicit operator bool() const { return Get(); }
  operator T*() const { return Get(); }
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  // CFI cast exemption to allow passing SentinelPointer through T* and support
  // heterogeneous assignments between different Member and Persistent handles
  // based on their actual types.
  V8_CLANG_NO_SANITIZE("cfi-unrelated-cast") T* Get() const {
    // Executed by the mutator, hence non atomic load.
    //
    // The const_cast below removes the constness from MemberBase storage. The
    // following static_cast re-adds any constness if specified through the
    // user-visible template parameter T.
    return static_cast<T*>(const_cast<void*>(MemberBase::GetRaw()));
  }

  void Clear() { SetRawAtomic(nullptr); }

  T* Release() {
    T* result = Get();
    Clear();
    return result;
  }

  const T** GetSlotForTesting() const {
    return reinterpret_cast<const T**>(GetRawSlot());
  }

 private:
  const T* GetRawAtomic() const {
    return static_cast<const T*>(MemberBase::GetRawAtomic());
  }

  void InitializingWriteBarrier() const {
    WriteBarrierPolicy::InitializingBarrier(GetRawSlot(), GetRaw());
  }
  void AssigningWriteBarrier() const {
    WriteBarrierPolicy::AssigningBarrier(GetRawSlot(), GetRaw());
  }

  void ClearFromGC() const { MemberBase::ClearFromGC(); }

  friend class cppgc::Visitor;
  template <typename U>
  friend struct cppgc::TraceTrait;
};

template <typename T1, typename WeaknessTag1, typename WriteBarrierPolicy1,
          typename CheckingPolicy1, typename T2, typename WeaknessTag2,
          typename WriteBarrierPolicy2, typename CheckingPolicy2>
bool operator==(
    BasicMember<T1, WeaknessTag1, WriteBarrierPolicy1, CheckingPolicy1> member1,
    BasicMember<T2, WeaknessTag2, WriteBarrierPolicy2, CheckingPolicy2>
        member2) {
  return member1.Get() == member2.Get();
}

template <typename T1, typename WeaknessTag1, typename WriteBarrierPolicy1,
          typename CheckingPolicy1, typename T2, typename WeaknessTag2,
          typename WriteBarrierPolicy2, typename CheckingPolicy2>
bool operator!=(
    BasicMember<T1, WeaknessTag1, WriteBarrierPolicy1, CheckingPolicy1> member1,
    BasicMember<T2, WeaknessTag2, WriteBarrierPolicy2, CheckingPolicy2>
        member2) {
  return !(member1 == member2);
}

template <typename T, typename WriteBarrierPolicy, typename CheckingPolicy>
struct IsWeak<
    internal::BasicMember<T, WeakMemberTag, WriteBarrierPolicy, CheckingPolicy>>
    : std::true_type {};

}  // namespace internal

/**
 * Members are used in classes to contain strong pointers to other garbage
 * collected objects. All Member fields of a class must be traced in the class'
 * trace method.
 */
template <typename T>
using Member = internal::BasicMember<T, internal::StrongMemberTag,
                                     internal::DijkstraWriteBarrierPolicy>;

/**
 * WeakMember is similar to Member in that it is used to point to other garbage
 * collected objects. However instead of creating a strong pointer to the
 * object, the WeakMember creates a weak pointer, which does not keep the
 * pointee alive. Hence if all pointers to to a heap allocated object are weak
 * the object will be garbage collected. At the time of GC the weak pointers
 * will automatically be set to null.
 */
template <typename T>
using WeakMember = internal::BasicMember<T, internal::WeakMemberTag,
                                         internal::DijkstraWriteBarrierPolicy>;

/**
 * UntracedMember is a pointer to an on-heap object that is not traced for some
 * reason. Do not use this unless you know what you are doing. Keeping raw
 * pointers to on-heap objects is prohibited unless used from stack. Pointee
 * must be kept alive through other means.
 */
template <typename T>
using UntracedMember = internal::BasicMember<T, internal::UntracedMemberTag,
                                             internal::NoWriteBarrierPolicy>;

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_MEMBER_H_
