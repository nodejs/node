// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/tagged-impl.h"

#include <sstream>

#include "src/objects/objects.h"
#include "src/strings/string-stream.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

template <HeapObjectReferenceType kRefType, typename StorageType>
void TaggedImpl<kRefType, StorageType>::ShortPrint(FILE* out) {
  OFStream os(out);
  os << Brief(*this);
}

template <HeapObjectReferenceType kRefType, typename StorageType>
void TaggedImpl<kRefType, StorageType>::ShortPrint(StringStream* accumulator) {
  std::ostringstream os;
  os << Brief(*this);
  accumulator->Add(os.str().c_str());
}

template <HeapObjectReferenceType kRefType, typename StorageType>
void TaggedImpl<kRefType, StorageType>::ShortPrint(std::ostream& os) {
  os << Brief(*this);
}

// Explicit instantiation declarations.
template class TaggedImpl<HeapObjectReferenceType::STRONG, Address>;
template class TaggedImpl<HeapObjectReferenceType::WEAK, Address>;

}  // namespace internal
}  // namespace v8
