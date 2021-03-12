// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Only including the -inl.h file directly makes the linter complain.
#include "src/objects/swiss-name-dictionary.h"

#include "src/objects/swiss-name-dictionary-inl.h"

namespace v8 {
namespace internal {

// The largest value we ever have to store in the enumeration table is
// Capacity() - 1. The largest value we ever have to store for the present or
// deleted element count is MaxUsableCapacity(Capacity()). All data in the
// meta table is unsigned. Using this, we verify the values of the constants
// |kMax1ByteMetaTableCapacity| and |kMax2ByteMetaTableCapacity|.
STATIC_ASSERT(SwissNameDictionary::kMax1ByteMetaTableCapacity - 1 <=
              std::numeric_limits<uint8_t>::max());
STATIC_ASSERT(SwissNameDictionary::MaxUsableCapacity(
                  SwissNameDictionary::kMax1ByteMetaTableCapacity) <=
              std::numeric_limits<uint8_t>::max());
STATIC_ASSERT(SwissNameDictionary::kMax2ByteMetaTableCapacity - 1 <=
              std::numeric_limits<uint16_t>::max());
STATIC_ASSERT(SwissNameDictionary::MaxUsableCapacity(
                  SwissNameDictionary::kMax2ByteMetaTableCapacity) <=
              std::numeric_limits<uint16_t>::max());

template void SwissNameDictionary::Initialize(Isolate* isolate,
                                              ByteArray meta_table,
                                              int capacity);
template void SwissNameDictionary::Initialize(LocalIsolate* isolate,
                                              ByteArray meta_table,
                                              int capacity);

}  // namespace internal
}  // namespace v8
