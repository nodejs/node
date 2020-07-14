// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/off-thread-factory.h"

#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/fixed-array.h"
#include "src/objects/heap-object.h"
#include "src/objects/string.h"
#include "src/roots/roots-inl.h"
#include "src/strings/string-hasher.h"

namespace v8 {
namespace internal {

OffThreadFactory::OffThreadFactory(Isolate* isolate) : roots_(isolate) {}

// Hacky method for creating a simple object with a slot pointing to a string.
// TODO(leszeks): Remove once we have full FixedArray support.
Handle<FixedArray> OffThreadFactory::StringWrapperForTest(
    Handle<String> string) {
  HeapObject wrapper =
      AllocateRaw(FixedArray::SizeFor(1), AllocationType::kOld);
  wrapper.set_map_after_allocation(read_only_roots().fixed_array_map());
  FixedArray array = FixedArray::cast(wrapper);
  array.set_length(1);
  array.data_start().Relaxed_Store(*string);
  return handle(array, isolate());
}

Handle<String> OffThreadFactory::MakeOrFindTwoCharacterString(uint16_t c1,
                                                              uint16_t c2) {
  // TODO(leszeks): Do some real caching here. Also, these strings should be
  // internalized.
  if ((c1 | c2) <= unibrow::Latin1::kMaxChar) {
    Handle<SeqOneByteString> ret =
        NewRawOneByteString(2, AllocationType::kOld).ToHandleChecked();
    ret->SeqOneByteStringSet(0, c1);
    ret->SeqOneByteStringSet(1, c2);
    return ret;
  }
  Handle<SeqTwoByteString> ret =
      NewRawTwoByteString(2, AllocationType::kOld).ToHandleChecked();
  ret->SeqTwoByteStringSet(0, c1);
  ret->SeqTwoByteStringSet(1, c2);
  return ret;
}

Handle<String> OffThreadFactory::InternalizeString(
    const Vector<const uint8_t>& string) {
  uint32_t hash = StringHasher::HashSequentialString(
      string.begin(), string.length(), HashSeed(read_only_roots()));
  return NewOneByteInternalizedString(string, hash);
}

Handle<String> OffThreadFactory::InternalizeString(
    const Vector<const uint16_t>& string) {
  uint32_t hash = StringHasher::HashSequentialString(
      string.begin(), string.length(), HashSeed(read_only_roots()));
  return NewTwoByteInternalizedString(string, hash);
}

void OffThreadFactory::AddToScriptList(Handle<Script> shared) {
  isolate()->heap()->AddToScriptList(shared);
}

HeapObject OffThreadFactory::AllocateRaw(int size, AllocationType allocation,
                                         AllocationAlignment alignment) {
  return isolate()->heap()->AllocateRaw(size, allocation, alignment);
}

}  // namespace internal
}  // namespace v8
