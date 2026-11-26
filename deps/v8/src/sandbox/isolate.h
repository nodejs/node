// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SANDBOX_ISOLATE_H_
#define V8_SANDBOX_ISOLATE_H_

#include "src/sandbox/cppheap-pointer-table.h"
#include "src/sandbox/external-pointer-table.h"
#include "src/sandbox/js-dispatch-table.h"
#include "src/sandbox/trusted-pointer-table.h"
#include "src/sandbox/trusted-pointer-tag.h"

namespace v8::internal {

class Isolate;

// A reference to an Isolate that only exposes the pointer-compression- and
// sandbox-related parts of an isolate, in particular the various pointer
// tables. Can be used off-thread and implicitly constructed from both an
// Isolate* and a LocalIsolate*.
#ifdef V8_COMPRESS_POINTERS

class V8_EXPORT_PRIVATE IsolateForSandbox final {
 public:
  template <typename IsolateT>
  IsolateForSandbox(IsolateT* isolate)  // NOLINT(runtime/explicit)
      : isolate_(isolate->ForSandbox()) {}

  inline ExternalPointerTable& GetExternalPointerTableFor(
      ExternalPointerTagRange tag_range);
  inline ExternalPointerTable::Space* GetExternalPointerTableSpaceFor(
      ExternalPointerTagRange tag_range, Address host);

#ifdef V8_ENABLE_SANDBOX
  inline TrustedPointerTable& GetTrustedPointerTableFor(
      TrustedPointerTagRange tag_range);
  inline TrustedPointerTable::Space* GetTrustedPointerTableSpaceFor(
      TrustedPointerTagRange tag_range, Address host);
#endif  // V8_ENABLE_SANDBOX

  inline JSDispatchTable& GetJSDispatchTable();

  // Object is needed in case it comes from shared space.
  inline ExternalPointerTag GetExternalPointerTableTagFor(
      Tagged<HeapObject> witness, ExternalPointerHandle handle);

  // Check whether the shared pointer tables of two IsolateForSandbox objects
  // are the same.
  inline bool SharesPointerTablesWith(IsolateForSandbox other) const;

  inline Isolate* GetIsolate() const { return isolate_; }

 private:
  friend class IsolateForPointerCompression;
  Isolate* const isolate_;
};

V8_INLINE IsolateForSandbox GetCurrentIsolateForSandbox();

class V8_EXPORT_PRIVATE IsolateForPointerCompression final {
 public:
  template <typename IsolateT>
  IsolateForPointerCompression(IsolateT* isolate)  // NOLINT(runtime/explicit)
      : isolate_(isolate->ForSandbox()) {}
  IsolateForPointerCompression(
      IsolateForSandbox isolate)  // NOLINT(runtime/explicit)
      : isolate_(isolate.isolate_) {}

  inline ExternalPointerTable& GetExternalPointerTableFor(
      ExternalPointerTagRange tag_range);
  inline ExternalPointerTable::Space* GetExternalPointerTableSpaceFor(
      ExternalPointerTagRange tag_range, Address host);

  inline CppHeapPointerTable& GetCppHeapPointerTable();
  inline CppHeapPointerTable::Space* GetCppHeapPointerTableSpace();

  inline Isolate* GetIsolate() const { return isolate_; }

 private:
  Isolate* const isolate_;
};

#else  // V8_COMPRESS_POINTERS

class V8_EXPORT_PRIVATE IsolateForSandbox final {
 public:
  template <typename IsolateT>
  constexpr IsolateForSandbox(IsolateT*) {}  // NOLINT(runtime/explicit)

  constexpr IsolateForSandbox() = default;
};

V8_INLINE IsolateForSandbox GetCurrentIsolateForSandbox() { return {}; }

class V8_EXPORT_PRIVATE IsolateForPointerCompression final {
 public:
  template <typename IsolateT>
  constexpr IsolateForPointerCompression(IsolateT*)  // NOLINT(runtime/explicit)
  {}
  constexpr IsolateForPointerCompression(
      IsolateForSandbox)  // NOLINT(runtime/explicit)
  {}
};

#endif  // V8_COMPRESS_POINTERS

}  // namespace v8::internal

#endif  // V8_SANDBOX_ISOLATE_H_
