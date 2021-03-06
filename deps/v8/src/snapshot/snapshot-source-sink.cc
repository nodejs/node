// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/snapshot/snapshot-source-sink.h"

#include "src/base/logging.h"
#include "src/handles/handles-inl.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

void SnapshotByteSink::PutInt(uintptr_t integer, const char* description) {
  DCHECK_LT(integer, 1 << 30);
  integer <<= 2;
  int bytes = 1;
  if (integer > 0xFF) bytes = 2;
  if (integer > 0xFFFF) bytes = 3;
  if (integer > 0xFFFFFF) bytes = 4;
  integer |= (bytes - 1);
  Put(static_cast<byte>(integer & 0xFF), "IntPart1");
  if (bytes > 1) Put(static_cast<byte>((integer >> 8) & 0xFF), "IntPart2");
  if (bytes > 2) Put(static_cast<byte>((integer >> 16) & 0xFF), "IntPart3");
  if (bytes > 3) Put(static_cast<byte>((integer >> 24) & 0xFF), "IntPart4");
}

void SnapshotByteSink::PutRaw(const byte* data, int number_of_bytes,
                              const char* description) {
  data_.insert(data_.end(), data, data + number_of_bytes);
}

void SnapshotByteSink::Append(const SnapshotByteSink& other) {
  data_.insert(data_.end(), other.data_.begin(), other.data_.end());
}

int SnapshotByteSource::GetBlob(const byte** data) {
  int size = GetInt();
  CHECK(position_ + size <= length_);
  *data = &data_[position_];
  Advance(size);
  return size;
}
}  // namespace internal
}  // namespace v8
