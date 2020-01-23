// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debug-helper-internal.h"
#include "src/common/ptr-compr-inl.h"
#include "torque-generated/class-debug-readers-tq.h"

namespace i = v8::internal;

namespace v8_debug_helper_internal {

bool IsPointerCompressed(uintptr_t address) {
#if COMPRESS_POINTERS_BOOL
  return address < i::kPtrComprHeapReservationSize;
#else
  return false;
#endif
}

uintptr_t EnsureDecompressed(uintptr_t address,
                             uintptr_t any_uncompressed_ptr) {
  if (!COMPRESS_POINTERS_BOOL || !IsPointerCompressed(address)) return address;
  return i::DecompressTaggedAny(any_uncompressed_ptr,
                                static_cast<i::Tagged_t>(address));
}

d::PropertyKind GetArrayKind(d::MemoryAccessResult mem_result) {
  d::PropertyKind indexed_field_kind{};
  switch (mem_result) {
    case d::MemoryAccessResult::kOk:
      indexed_field_kind = d::PropertyKind::kArrayOfKnownSize;
      break;
    case d::MemoryAccessResult::kAddressNotValid:
      indexed_field_kind =
          d::PropertyKind::kArrayOfUnknownSizeDueToInvalidMemory;
      break;
    default:
      indexed_field_kind =
          d::PropertyKind::kArrayOfUnknownSizeDueToValidButInaccessibleMemory;
      break;
  }
  return indexed_field_kind;
}

std::vector<std::unique_ptr<ObjectProperty>> TqObject::GetProperties(
    d::MemoryAccessor accessor) const {
  return std::vector<std::unique_ptr<ObjectProperty>>();
}

const char* TqObject::GetName() const { return "v8::internal::Object"; }

void TqObject::Visit(TqObjectVisitor* visitor) const {
  visitor->VisitObject(this);
}

bool TqObject::IsSuperclassOf(const TqObject* other) const {
  return GetName() != other->GetName();
}

}  // namespace v8_debug_helper_internal
