// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_INTERNAL_POINTER_POLICIES_H_
#define INCLUDE_CPPGC_INTERNAL_POINTER_POLICIES_H_

#include <cstdint>
#include <type_traits>

#include "cppgc/source-location.h"
#include "v8config.h"  // NOLINT(build/include_directory)

namespace cppgc {
namespace internal {

class PersistentRegion;

// Tags to distinguish between strong and weak member types.
class StrongMemberTag;
class WeakMemberTag;
class UntracedMemberTag;

struct DijkstraWriteBarrierPolicy {
  static void InitializingBarrier(const void*, const void*) {
    // Since in initializing writes the source object is always white, having no
    // barrier doesn't break the tri-color invariant.
  }
  static void AssigningBarrier(const void*, const void*) {
    // TODO(chromium:1056170): Add actual implementation.
  }
};

struct NoWriteBarrierPolicy {
  static void InitializingBarrier(const void*, const void*) {}
  static void AssigningBarrier(const void*, const void*) {}
};

class V8_EXPORT EnabledCheckingPolicy {
 protected:
  EnabledCheckingPolicy();
  void CheckPointer(const void* ptr);

 private:
  void* impl_;
};

class DisabledCheckingPolicy {
 protected:
  void CheckPointer(const void* raw) {}
};

#if V8_ENABLE_CHECKS
using DefaultCheckingPolicy = EnabledCheckingPolicy;
#else
using DefaultCheckingPolicy = DisabledCheckingPolicy;
#endif

class KeepLocationPolicy {
 public:
  constexpr const SourceLocation& Location() const { return location_; }

 protected:
  constexpr explicit KeepLocationPolicy(const SourceLocation& location)
      : location_(location) {}

  // KeepLocationPolicy must not copy underlying source locations.
  KeepLocationPolicy(const KeepLocationPolicy&) = delete;
  KeepLocationPolicy& operator=(const KeepLocationPolicy&) = delete;

  // Location of the original moved from object should be preserved.
  KeepLocationPolicy(KeepLocationPolicy&&) = default;
  KeepLocationPolicy& operator=(KeepLocationPolicy&&) = default;

 private:
  SourceLocation location_;
};

class IgnoreLocationPolicy {
 public:
  constexpr SourceLocation Location() const { return {}; }

 protected:
  constexpr explicit IgnoreLocationPolicy(const SourceLocation&) {}
};

#if CPPGC_SUPPORTS_OBJECT_NAMES
using DefaultLocationPolicy = KeepLocationPolicy;
#else
using DefaultLocationPolicy = IgnoreLocationPolicy;
#endif

struct StrongPersistentPolicy {
  using IsStrongPersistent = std::true_type;

  static V8_EXPORT PersistentRegion& GetPersistentRegion(void* object);
};

struct WeakPersistentPolicy {
  using IsStrongPersistent = std::false_type;

  static V8_EXPORT PersistentRegion& GetPersistentRegion(void* object);
};

// Persistent/Member forward declarations.
template <typename T, typename WeaknessPolicy,
          typename LocationPolicy = DefaultLocationPolicy,
          typename CheckingPolicy = DefaultCheckingPolicy>
class BasicPersistent;
template <typename T, typename WeaknessTag, typename WriteBarrierPolicy,
          typename CheckingPolicy = DefaultCheckingPolicy>
class BasicMember;

// Special tag type used to denote some sentinel member. The semantics of the
// sentinel is defined by the embedder.
struct SentinelPointer {
  template <typename T>
  operator T*() const {  // NOLINT
    static constexpr intptr_t kSentinelValue = -1;
    return reinterpret_cast<T*>(kSentinelValue);
  }
  // Hidden friends.
  friend bool operator==(SentinelPointer, SentinelPointer) { return true; }
  friend bool operator!=(SentinelPointer, SentinelPointer) { return false; }
};

}  // namespace internal

constexpr internal::SentinelPointer kSentinelPointer;

}  // namespace cppgc

#endif  // INCLUDE_CPPGC_INTERNAL_POINTER_POLICIES_H_
