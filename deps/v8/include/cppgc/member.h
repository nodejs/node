// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_MEMBER_H_
#define INCLUDE_CPPGC_MEMBER_H_

#include <atomic>
#include <cstddef>
#include <type_traits>

#include "cppgc/internal/pointer-policies.h"
#include "cppgc/type-traits.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {

class Visitor;

namespace internal {

class MemberBase {
 protected:
  MemberBase() = default;
  explicit MemberBase(void* value) : raw_(value) {}

  void* const* GetRawSlot() const { return &raw_; }
  void* GetRaw() const { return raw_; }
  void SetRaw(void* value) { raw_ = value; }

  void* GetRawAtomic() const {
    return reinterpret_cast<const std::atomic<void*>*>(&raw_)->load(
        std::memory_order_relaxed);
  }
  void SetRawAtomic(void* value) {
    reinterpret_cast<std::atomic<void*>*>(&raw_)->store(
        value, std::memory_order_relaxed);
  }

  void ClearFromGC() const { raw_ = nullptr; }

 private:
  mutable void* raw_ = nullptr;
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
  BasicMember(const BasicMember& other) : BasicMember(other.Get()) {}
  // Allow heterogeneous construction.
  template <typename U, typename OtherBarrierPolicy, typename OtherWeaknessTag,
            typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember(  // NOLINT
      const BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other)
      : BasicMember(other.Get()) {}
  // Construction from Persistent.
  template <typename U, typename PersistentWeaknessPolicy,
            typename PersistentLocationPolicy,
            typename PersistentCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember(  // NOLINT
      const BasicPersistent<U, PersistentWeaknessPolicy,
                            PersistentLocationPolicy, PersistentCheckingPolicy>&
          p)
      : BasicMember(p.Get()) {}

  BasicMember& operator=(const BasicMember& other) {
    return operator=(other.Get());
  }
  // Allow heterogeneous assignment.
  template <typename U, typename OtherWeaknessTag, typename OtherBarrierPolicy,
            typename OtherCheckingPolicy,
            typename = std::enable_if_t<std::is_base_of<T, U>::value>>
  BasicMember& operator=(
      const BasicMember<U, OtherWeaknessTag, OtherBarrierPolicy,
                        OtherCheckingPolicy>& other) {
    return operator=(other.Get());
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
  operator T*() const { return Get(); }  // NOLINT
  T* operator->() const { return Get(); }
  T& operator*() const { return *Get(); }

  // CFI cast exemption to allow passing SentinelPointer through T* and support
  // heterogeneous assignments between different Member and Persistent handles
  // based on their actual types.
  V8_CLANG_NO_SANITIZE("cfi-unrelated-cast") T* Get() const {
    // Executed by the mutator, hence non atomic load.
    return static_cast<T*>(MemberBase::GetRaw());
  }

  void Clear() { SetRawAtomic(nullptr); }

  T* Release() {
    T* result = Get();
    Clear();
    return result;
  }

 private:
  T* GetRawAtomic() const {
    return static_cast<T*>(MemberBase::GetRawAtomic());
  }

  void InitializingWriteBarrier() const {
    WriteBarrierPolicy::InitializingBarrier(GetRawSlot(), GetRaw());
  }
  void AssigningWriteBarrier() const {
    WriteBarrierPolicy::AssigningBarrier(GetRawSlot(), GetRaw());
  }

  void ClearFromGC() const { MemberBase::ClearFromGC(); }

  friend class cppgc::Visitor;
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
