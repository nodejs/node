// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MAP_WORD_INL_H_
#define V8_OBJECTS_MAP_WORD_INL_H_

#include "src/objects/map-word.h"
// Include the non-inl header before the rest of the headers.

#include "src/common/globals.h"
#include "src/objects/casting.h"
#include "src/objects/heap-object.h"
#include "src/objects/smi-inl.h"
#include "src/objects/tagged.h"

namespace v8::internal {

MapWord MapWord::FromMap(const Tagged<Map> map) {
  DCHECK(map.is_null() || !MapWord::IsPacked(map.ptr()));
#ifdef V8_MAP_PACKING
  return MapWord(Pack(map.ptr()));
#else
  return MapWord(map.ptr());
#endif
}

Tagged<Map> MapWord::ToMap() const {
#ifdef V8_MAP_PACKING
  return UncheckedCast<Map>(Tagged<Object>(Unpack(value_)));
#else
  return UncheckedCast<Map>(Tagged<Object>(value_));
#endif
}

bool MapWord::IsForwardingAddress() const {
#ifdef V8_EXTERNAL_CODE_SPACE
  // When external code space is enabled forwarding pointers are encoded as
  // Smi representing a diff from the source object address in kObjectAlignment
  // chunks.
  return HAS_SMI_TAG(value_);
#else
  return (value_ & kForwardingTagMask) == kForwardingTag;
#endif  // V8_EXTERNAL_CODE_SPACE
}

MapWord MapWord::FromForwardingAddress(Tagged<HeapObject> map_word_host,
                                       Tagged<HeapObject> object) {
#ifdef V8_EXTERNAL_CODE_SPACE
  // When external code space is enabled forwarding pointers are encoded as
  // Smi representing a diff from the source object address in kObjectAlignment
  // chunks.
  intptr_t diff = static_cast<intptr_t>(object.ptr() - map_word_host.ptr());
  DCHECK(IsAligned(diff, kObjectAlignment));
  MapWord map_word(Smi::FromIntptr(diff / kObjectAlignment).ptr());
  DCHECK(map_word.IsForwardingAddress());
  return map_word;
#else
  return MapWord(object.ptr() - kHeapObjectTag);
#endif  // V8_EXTERNAL_CODE_SPACE
}

Tagged<HeapObject> MapWord::ToForwardingAddress(
    Tagged<HeapObject> map_word_host) const {
  DCHECK(IsForwardingAddress());
#ifdef V8_EXTERNAL_CODE_SPACE
  // When the sandbox or the external code space is enabled, forwarding
  // pointers are encoded as Smi representing a diff from the source object
  // address in kObjectAlignment chunks. This is required as we are using
  // multiple pointer compression cages in these scenarios.
  intptr_t diff =
      static_cast<intptr_t>(Tagged<Smi>(value_).value()) * kObjectAlignment;
  Address address = map_word_host.address() + diff;
  return HeapObject::FromAddress(address);
#else
  // The sandbox requires the external code space.
  DCHECK(!V8_ENABLE_SANDBOX_BOOL);
  return HeapObject::FromAddress(value_);
#endif  // V8_EXTERNAL_CODE_SPACE
}

}  // namespace v8::internal

#endif  // V8_OBJECTS_MAP_WORD_INL_H_
