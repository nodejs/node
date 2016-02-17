// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/snapshot/snapshot-source-sink.h"

#include "src/base/logging.h"
#include "src/handles-inl.h"
#include "src/snapshot/serialize.h"  // for SerializerDeserializer::nop()


namespace v8 {
namespace internal {

void SnapshotByteSource::CopyRaw(byte* to, int number_of_bytes) {
  memcpy(to, data_ + position_, number_of_bytes);
  position_ += number_of_bytes;
}


void SnapshotByteSink::PutInt(uintptr_t integer, const char* description) {
  DCHECK(integer < 1 << 30);
  integer <<= 2;
  int bytes = 1;
  if (integer > 0xff) bytes = 2;
  if (integer > 0xffff) bytes = 3;
  if (integer > 0xffffff) bytes = 4;
  integer |= (bytes - 1);
  Put(static_cast<int>(integer & 0xff), "IntPart1");
  if (bytes > 1) Put(static_cast<int>((integer >> 8) & 0xff), "IntPart2");
  if (bytes > 2) Put(static_cast<int>((integer >> 16) & 0xff), "IntPart3");
  if (bytes > 3) Put(static_cast<int>((integer >> 24) & 0xff), "IntPart4");
}


void SnapshotByteSink::PutRaw(const byte* data, int number_of_bytes,
                              const char* description) {
  data_.AddAll(Vector<byte>(const_cast<byte*>(data), number_of_bytes));
}


bool SnapshotByteSource::AtEOF() {
  if (0u + length_ - position_ > 2 * sizeof(uint32_t)) return false;
  for (int x = position_; x < length_; x++) {
    if (data_[x] != SerializerDeserializer::nop()) return false;
  }
  return true;
}


bool SnapshotByteSource::GetBlob(const byte** data, int* number_of_bytes) {
  int size = GetInt();
  *number_of_bytes = size;

  if (position_ + size <= length_) {
    *data = &data_[position_];
    Advance(size);
    return true;
  } else {
    Advance(length_ - position_);  // proceed until end.
    return false;
  }
}
}  // namespace internal
}  // namespace v8
