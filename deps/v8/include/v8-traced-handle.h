// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_TRACED_HANDLE_H_
#define INCLUDE_V8_TRACED_HANDLE_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <atomic>
#include <memory>
#include <type_traits>
#include <utility>

#include "v8-internal.h"            // NOLINT(build/include_directory)
#include "v8-local-handle.h"        // NOLINT(build/include_directory)
#include "v8-weak-callback-info.h"  // NOLINT(build/include_directory)
#include "v8config.h"               // NOLINT(build/include_directory)

namespace v8 {

class Value;

namespace internal {

class BasicTracedReferenceExtractor;

enum class GlobalHandleStoreMode {
  kInitializingStore,
  kAssigningStore,
};

V8_EXPORT internal::Address* GlobalizeTracedReference(
    internal::Isolate* isolate, internal::Address value,
    internal::Address* slot, GlobalHandleStoreMode store_mode);
V8_EXPORT void MoveTracedReference(internal::Address** from,
                                   internal::Address** to);
V8_EXPORT void CopyTracedReference(const internal::Address* const* from,
                                   internal::Address** to);
V8_EXPORT void DisposeTracedReference(internal::Address* global_handle);

}  // namespace internal

/**
 * An indirect handle, where the indirect pointer points to a GlobalHandles
 * node.
 */
class TracedReferenceBase : public IndirectHandleBase {
 public:
  /**
   * If non-empty, destroy the underlying storage cell. |IsEmpty| will return
   * true after this call.
   */
  V8_INLINE void Reset();

  /**
   * Construct a Local<Value> from this handle.
   */
  V8_INLINE Local<Value> Get(Isolate* isolate) const {
    if (IsEmpty()) return Local<Value>();
    return Local<Value>::New(isolate, this->value<Value>());
  }

  /**
   * Returns true if this TracedReference is empty, i.e., has not been
   * assigned an object. This version of IsEmpty is thread-safe.
   */
  bool IsEmptyThreadSafe() const {
    return this->GetSlotThreadSafe() == nullptr;
  }

  /**
   * Assigns a wrapper class ID to the handle.
   */
  V8_INLINE void SetWrapperClassId(uint16_t class_id);

  /**
   * Returns the class ID previously assigned to this handle or 0 if no class ID
   * was previously assigned.
   */
  V8_INLINE uint16_t WrapperClassId() const;

 protected:
  V8_INLINE TracedReferenceBase() = default;

  /**
   * Update this reference in a thread-safe way.
   */
  void SetSlotThreadSafe(void* new_val) {
    reinterpret_cast<std::atomic<void*>*>(&slot())->store(
        new_val, std::memory_order_relaxed);
  }

  /**
   * Get this reference in a thread-safe way
   */
  const void* GetSlotThreadSafe() const {
    return reinterpret_cast<std::atomic<const void*> const*>(&slot())->load(
        std::memory_order_relaxed);
  }

  V8_EXPORT void CheckValue() const;

  friend class internal::BasicTracedReferenceExtractor;
  template <typename F>
  friend class Local;
  template <typename U>
  friend bool operator==(const TracedReferenceBase&, const Local<U>&);
  friend bool operator==(const TracedReferenceBase&,
                         const TracedReferenceBase&);
};

/**
 * A traced handle with copy and move semantics. The handle is to be used
 * together as part of GarbageCollected objects (see v8-cppgc.h) or from stack
 * and specifies edges from C++ objects to JavaScript.
 *
 * The exact semantics are:
 * - Tracing garbage collections using CppHeap.
 * - Non-tracing garbage collections refer to
 *   |v8::EmbedderRootsHandler::IsRoot()| whether the handle should
 * be treated as root or not.
 *
 * Note that the base class cannot be instantiated itself, use |TracedReference|
 * instead.
 */
template <typename T>
class BasicTracedReference : public TracedReferenceBase {
 public:
  /**
   * Construct a Local<T> from this handle.
   */
  Local<T> Get(Isolate* isolate) const { return Local<T>::New(isolate, *this); }

  template <class S>
  V8_INLINE BasicTracedReference<S>& As() const {
    return reinterpret_cast<BasicTracedReference<S>&>(
        const_cast<BasicTracedReference<T>&>(*this));
  }

  V8_DEPRECATE_SOON("Use Get to convert to Local instead")
  V8_INLINE T* operator->() const {
#ifdef V8_ENABLE_CHECKS
    CheckValue();
#endif  // V8_ENABLE_CHECKS
    return this->template value<T>();
  }

  V8_DEPRECATE_SOON("Use Get to convert to Local instead")
  V8_INLINE T* operator*() const { return this->operator->(); }

 private:
  /**
   * An empty BasicTracedReference without storage cell.
   */
  BasicTracedReference() = default;

  V8_INLINE static internal::Address* New(
      Isolate* isolate, T* that, internal::Address** slot,
      internal::GlobalHandleStoreMode store_mode);

  template <typename F>
  friend class Local;
  friend class Object;
  template <typename F>
  friend class TracedReference;
  template <typename F>
  friend class BasicTracedReference;
  template <typename F>
  friend class ReturnValue;
};

/**
 * A traced handle without destructor that clears the handle. The embedder needs
 * to ensure that the handle is not accessed once the V8 object has been
 * reclaimed. For more details see BasicTracedReference.
 */
template <typename T>
class TracedReference : public BasicTracedReference<T> {
 public:
  using BasicTracedReference<T>::Reset;

  /**
   * An empty TracedReference without storage cell.
   */
  V8_INLINE TracedReference() = default;

  /**
   * Construct a TracedReference from a Local.
   *
   * When the Local is non-empty, a new storage cell is created
   * pointing to the same object.
   */
  template <class S>
  TracedReference(Isolate* isolate, Local<S> that) : BasicTracedReference<T>() {
    this->slot() =
        this->New(isolate, *that, &this->slot(),
                  internal::GlobalHandleStoreMode::kInitializingStore);
    static_assert(std::is_base_of<T, S>::value, "type check");
  }

  /**
   * Move constructor initializing TracedReference from an
   * existing one.
   */
  V8_INLINE TracedReference(TracedReference&& other) noexcept {
    // Forward to operator=.
    *this = std::move(other);
  }

  /**
   * Move constructor initializing TracedReference from an
   * existing one.
   */
  template <typename S>
  V8_INLINE TracedReference(TracedReference<S>&& other) noexcept {
    // Forward to operator=.
    *this = std::move(other);
  }

  /**
   * Copy constructor initializing TracedReference from an
   * existing one.
   */
  V8_INLINE TracedReference(const TracedReference& other) {
    // Forward to operator=;
    *this = other;
  }

  /**
   * Copy constructor initializing TracedReference from an
   * existing one.
   */
  template <typename S>
  V8_INLINE TracedReference(const TracedReference<S>& other) {
    // Forward to operator=;
    *this = other;
  }

  /**
   * Move assignment operator initializing TracedReference from an existing one.
   */
  V8_INLINE TracedReference& operator=(TracedReference&& rhs) noexcept;

  /**
   * Move assignment operator initializing TracedReference from an existing one.
   */
  template <class S>
  V8_INLINE TracedReference& operator=(TracedReference<S>&& rhs) noexcept;

  /**
   * Copy assignment operator initializing TracedReference from an existing one.
   */
  V8_INLINE TracedReference& operator=(const TracedReference& rhs);

  /**
   * Copy assignment operator initializing TracedReference from an existing one.
   */
  template <class S>
  V8_INLINE TracedReference& operator=(const TracedReference<S>& rhs);

  /**
   * If non-empty, destroy the underlying storage cell and create a new one with
   * the contents of other if other is non empty
   */
  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const Local<S>& other);

  template <class S>
  V8_INLINE TracedReference<S>& As() const {
    return reinterpret_cast<TracedReference<S>&>(
        const_cast<TracedReference<T>&>(*this));
  }
};

// --- Implementation ---
template <class T>
internal::Address* BasicTracedReference<T>::New(
    Isolate* isolate, T* that, internal::Address** slot,
    internal::GlobalHandleStoreMode store_mode) {
  if (internal::ValueHelper::IsEmpty(that)) return nullptr;
  return internal::GlobalizeTracedReference(
      reinterpret_cast<internal::Isolate*>(isolate),
      internal::ValueHelper::ValueAsAddress(that),
      reinterpret_cast<internal::Address*>(slot), store_mode);
}

void TracedReferenceBase::Reset() {
  if (IsEmpty()) return;
  internal::DisposeTracedReference(slot());
  SetSlotThreadSafe(nullptr);
}

V8_INLINE bool operator==(const TracedReferenceBase& lhs,
                          const TracedReferenceBase& rhs) {
  return internal::HandleHelper::EqualHandles(lhs, rhs);
}

template <typename U>
V8_INLINE bool operator==(const TracedReferenceBase& lhs,
                          const v8::Local<U>& rhs) {
  return internal::HandleHelper::EqualHandles(lhs, rhs);
}

template <typename U>
V8_INLINE bool operator==(const v8::Local<U>& lhs,
                          const TracedReferenceBase& rhs) {
  return rhs == lhs;
}

V8_INLINE bool operator!=(const TracedReferenceBase& lhs,
                          const TracedReferenceBase& rhs) {
  return !(lhs == rhs);
}

template <typename U>
V8_INLINE bool operator!=(const TracedReferenceBase& lhs,
                          const v8::Local<U>& rhs) {
  return !(lhs == rhs);
}

template <typename U>
V8_INLINE bool operator!=(const v8::Local<U>& lhs,
                          const TracedReferenceBase& rhs) {
  return !(rhs == lhs);
}

template <class T>
template <class S>
void TracedReference<T>::Reset(Isolate* isolate, const Local<S>& other) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  this->Reset();
  if (other.IsEmpty()) return;
  this->SetSlotThreadSafe(
      this->New(isolate, *other, &this->slot(),
                internal::GlobalHandleStoreMode::kAssigningStore));
}

template <class T>
template <class S>
TracedReference<T>& TracedReference<T>::operator=(
    TracedReference<S>&& rhs) noexcept {
  static_assert(std::is_base_of<T, S>::value, "type check");
  *this = std::move(rhs.template As<T>());
  return *this;
}

template <class T>
template <class S>
TracedReference<T>& TracedReference<T>::operator=(
    const TracedReference<S>& rhs) {
  static_assert(std::is_base_of<T, S>::value, "type check");
  *this = rhs.template As<T>();
  return *this;
}

template <class T>
TracedReference<T>& TracedReference<T>::operator=(
    TracedReference&& rhs) noexcept {
  if (this != &rhs) {
    internal::MoveTracedReference(&rhs.slot(), &this->slot());
  }
  return *this;
}

template <class T>
TracedReference<T>& TracedReference<T>::operator=(const TracedReference& rhs) {
  if (this != &rhs) {
    this->Reset();
    if (!rhs.IsEmpty()) {
      internal::CopyTracedReference(&rhs.slot(), &this->slot());
    }
  }
  return *this;
}

void TracedReferenceBase::SetWrapperClassId(uint16_t class_id) {
  using I = internal::Internals;
  if (IsEmpty()) return;
  uint8_t* addr =
      reinterpret_cast<uint8_t*>(slot()) + I::kTracedNodeClassIdOffset;
  *reinterpret_cast<uint16_t*>(addr) = class_id;
}

uint16_t TracedReferenceBase::WrapperClassId() const {
  using I = internal::Internals;
  if (IsEmpty()) return 0;
  uint8_t* addr =
      reinterpret_cast<uint8_t*>(slot()) + I::kTracedNodeClassIdOffset;
  return *reinterpret_cast<uint16_t*>(addr);
}

}  // namespace v8

#endif  // INCLUDE_V8_TRACED_HANDLE_H_
