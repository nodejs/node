// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_EXTERNAL_POINTER_H_
#define V8_SANDBOX_EXTERNAL_POINTER_H_

#include "src/codegen/external-reference.h"
#include "src/common/globals.h"
#include "src/sandbox/isolate.h"
#include "v8-internal.h"

namespace v8 {
namespace internal {

V8_INLINE constexpr std::optional<ExternalReference::Type>
TryGetCallbackRedirectionType(ExternalPointerTag tag) {
  switch (tag) {
    case kFunctionTemplateInfoCallbackTag:
      return ExternalReference::DIRECT_API_CALL;
    case kAccessorInfoGetterTag:
    case kApiNamedPropertyGetterCallbackTag:
      return ExternalReference::DIRECT_GETTER_CALL;
    case kApiNamedPropertySetterCallbackTag:
      return ExternalReference::DIRECT_SETTER_CALL;
    default:
      return std::nullopt;
  }
}

template <ExternalPointerTagRange kTagRange>
class ExternalPointerMember {
 public:
  ExternalPointerMember() = default;

  template <ExternalPointerTag tag>
  void Init(Address host_address, IsolateForSandbox isolate, Address value);
  void Init(Address host_address, IsolateForSandbox isolate, Address value)
    requires(kTagRange.Size() == 1)
  {
    Init<kTagRange.first>(host_address, isolate, value);
  }

  template <ExternalPointerTagRange tag_range>
  inline Address load(const IsolateForSandbox isolate) const;
  inline Address load(const IsolateForSandbox isolate) const {
    return load<kTagRange>(isolate);
  }

  template <ExternalPointerTag tag>
  inline void store(IsolateForSandbox isolate, Address value);
  inline void store(IsolateForSandbox isolate, Address value)
    requires(kTagRange.Size() == 1)
  {
    store<kTagRange.first>(isolate, value);
  }

  inline ExternalPointer_t load_encoded() const;
  inline void store_encoded(ExternalPointer_t value);

  Address storage_address() const {
    return reinterpret_cast<Address>(storage_);
  }

 private:
  static_assert(
      ([]() {
        for (int tag = kTagRange.first; tag <= kTagRange.last; ++tag) {
          if (TryGetCallbackRedirectionType(
                  static_cast<ExternalPointerTag>(tag))) {
            return false;
          }
        }
        return true;
      })(),
      "Do not use generic ExternalPointerMember for tags that need callback "
      "redirection. You may need to use a single tag instead of a tag range, "
      "or update the NeedsCallbackRedirection specialization to support tag "
      "ranges.");

  alignas(alignof(Tagged_t)) char storage_[sizeof(ExternalPointer_t)];
};

template <ExternalPointerTagRange kTagRange>
concept TagWithRedirection =
    (kTagRange.Size() == 1 &&
     TryGetCallbackRedirectionType(kTagRange.first).has_value());

// Specialization of ExternalPointerMember for tags that need callback
// redirection on the simulator.
//
// Currently only supports a single tag instead of a tag range, since different
// tags need different redirection types.
template <ExternalPointerTagRange kTagRange>
  requires TagWithRedirection<kTagRange>
class ExternalPointerMember<kTagRange> {
 public:
  static constexpr ExternalPointerTag kTag = kTagRange.first;
  static constexpr ExternalReference::Type kRedirectionType =
      TryGetCallbackRedirectionType(kTag).value();

  ExternalPointerMember() = default;

  void Init(Address host_address, IsolateForSandbox isolate, Address value);

  inline Address load(const IsolateForSandbox isolate) const;
  inline void store(IsolateForSandbox isolate, Address value);

  inline Address load_raw(const IsolateForSandbox isolate) const;
  inline void store_raw(const IsolateForSandbox isolate, Address value);

  inline ExternalPointer_t load_encoded() const;
  inline void store_encoded(ExternalPointer_t value);

  Address storage_address() { return reinterpret_cast<Address>(storage_); }

  inline void RemoveCallbackRedirectionForSerialization(
      IsolateForSandbox isolate);
  inline void RestoreCallbackRedirectionAfterDeserialization(
      IsolateForSandbox isolate);

 private:
  static Address RedirectValue(IsolateForSandbox isolate, Address value);

  alignas(alignof(Tagged_t)) char storage_[sizeof(ExternalPointer_t)];
};

// Writes the null handle or kNullAddress into the external pointer field.
V8_INLINE void InitLazyExternalPointerField(Address field_address);

// Creates and initializes an entry in the external pointer table and writes the
// handle for that entry to the field.
template <ExternalPointerTag tag>
V8_INLINE void InitExternalPointerField(Address host_address,
                                        Address field_address,
                                        IsolateForSandbox isolate,
                                        Address value);
V8_INLINE void InitExternalPointerField(Address host_address,
                                        Address field_address,
                                        IsolateForSandbox isolate,
                                        ExternalPointerTag tag, Address value);

// If the sandbox is enabled: reads the ExternalPointerHandle from the field and
// loads the corresponding external pointer from the external pointer table. If
// the sandbox is disabled: load the external pointer from the field.
//
// This can be used for both regular and lazily-initialized external pointer
// fields since lazily-initialized field will initially contain
// kNullExternalPointerHandle, which is guaranteed to result in kNullAddress
// being returned from the external pointer table.
template <ExternalPointerTagRange tag_range>
V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           IsolateForSandbox isolate);

V8_INLINE Address ReadExternalPointerField(Address field_address,
                                           IsolateForSandbox isolate,
                                           ExternalPointerTagRange tag_range);

// If the sandbox is enabled: reads the ExternalPointerHandle from the field and
// stores the external pointer to the corresponding entry in the external
// pointer table. If the sandbox is disabled: stores the external pointer to the
// field.
template <ExternalPointerTag tag>
V8_INLINE void WriteExternalPointerField(Address field_address,
                                         IsolateForSandbox isolate,
                                         Address value);
V8_INLINE void WriteExternalPointerField(Address field_address,
                                         IsolateForSandbox isolate,
                                         ExternalPointerTag tag, Address value);

}  // namespace internal
}  // namespace v8

#endif  // V8_SANDBOX_EXTERNAL_POINTER_H_
