// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_ISOLATE_H_
#define V8_SANDBOX_ISOLATE_H_

#include "src/sandbox/code-pointer-table.h"
#include "src/sandbox/cppheap-pointer-table.h"
#include "src/sandbox/external-pointer-table.h"
#include "src/sandbox/indirect-pointer-tag.h"
#include "src/sandbox/js-dispatch-table.h"
#include "src/sandbox/trusted-pointer-table.h"

namespace v8::internal {

class Isolate;

// A reference to an Isolate that only exposes the sandbox-related parts of an
// isolate, in particular the various pointer tables. Can be used off-thread
// and implicitly constructed from both an Isolate* and a LocalIsolate*.
#ifdef V8_ENABLE_SANDBOX

class V8_EXPORT_PRIVATE IsolateForSandbox final {
 public:
  template <typename IsolateT>
  IsolateForSandbox(IsolateT* isolate)  // NOLINT(runtime/explicit)
      : isolate_(isolate->ForSandbox()) {}

  inline ExternalPointerTable& GetExternalPointerTableFor(
      ExternalPointerTagRange tag_range);
  inline ExternalPointerTable::Space* GetExternalPointerTableSpaceFor(
      ExternalPointerTagRange tag_range, Address host);

  inline CodePointerTable::Space* GetCodePointerTableSpaceFor(
      Address owning_slot);

  inline TrustedPointerTable& GetTrustedPointerTableFor(IndirectPointerTag tag);
  inline TrustedPointerTable::Space* GetTrustedPointerTableSpaceFor(
      IndirectPointerTag tag);

  // Object is needed as a witness that this handle does not come from the
  // shared space.
  inline ExternalPointerTag GetExternalPointerTableTagFor(
      Tagged<HeapObject> witness, ExternalPointerHandle handle);

  // Check whether the shared pointer tables of two IsolateForSandbox objects
  // are the same.
  inline bool SharesPointerTablesWith(IsolateForSandbox other) const;

 private:
  Isolate* const isolate_;
};

// Use this function instead of `Internals::GetIsolateForSandbox` for internal
// code, as this function is fully inlinable.
// Note that this method might return an isolate which is not the "current" one
// as returned by `Isolate::Current()`. Use `GetCurrentIsolateForSandbox`
// instead where possible.
// TODO(396607238): Replace all callers with `GetCurrentIsolateForSandbox()`.
V8_INLINE IsolateForSandbox GetIsolateForSandbox(Tagged<HeapObject> object);

V8_INLINE IsolateForSandbox GetCurrentIsolateForSandbox();

#else  // V8_ENABLE_SANDBOX

class V8_EXPORT_PRIVATE IsolateForSandbox final {
 public:
  template <typename IsolateT>
  constexpr IsolateForSandbox(IsolateT*) {}  // NOLINT(runtime/explicit)

  constexpr IsolateForSandbox() = default;
};

V8_INLINE IsolateForSandbox GetIsolateForSandbox(Tagged<HeapObject>) {
  return {};
}
V8_INLINE IsolateForSandbox GetCurrentIsolateForSandbox() { return {}; }

#endif  // V8_ENABLE_SANDBOX

#ifdef V8_COMPRESS_POINTERS
class V8_EXPORT_PRIVATE IsolateForPointerCompression final {
 public:
  template <typename IsolateT>
  IsolateForPointerCompression(IsolateT* isolate)  // NOLINT(runtime/explicit)
      : isolate_(isolate->ForSandbox()) {}

  inline ExternalPointerTable& GetExternalPointerTableFor(
      ExternalPointerTagRange tag_range);
  inline ExternalPointerTable::Space* GetExternalPointerTableSpaceFor(
      ExternalPointerTagRange tag_range, Address host);

  inline CppHeapPointerTable& GetCppHeapPointerTable();
  inline CppHeapPointerTable::Space* GetCppHeapPointerTableSpace();

 private:
  Isolate* const isolate_;
};
#else   // V8_COMPRESS_POINTERS
class V8_EXPORT_PRIVATE IsolateForPointerCompression final {
 public:
  template <typename IsolateT>
  constexpr IsolateForPointerCompression(IsolateT*)  // NOLINT(runtime/explicit)
  {}
};
#endif  // V8_COMPRESS_POINTERS

}  // namespace v8::internal

#endif  // V8_SANDBOX_ISOLATE_H_
