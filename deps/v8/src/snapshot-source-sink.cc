// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "src/snapshot-source-sink.h"

#include "src/base/logging.h"
#include "src/handles-inl.h"
#include "src/serialize.h"  // for SerializerDeserializer::nop() in AtEOF()


namespace v8 {
namespace internal {


SnapshotByteSource::SnapshotByteSource(const byte* array, int length)
    : data_(array), length_(length), position_(0) {
}


SnapshotByteSource::~SnapshotByteSource() { }


int32_t SnapshotByteSource::GetUnalignedInt() {
  DCHECK(position_ < length_);  // Require at least one byte left.
  int32_t answer = data_[position_];
  answer |= data_[position_ + 1] << 8;
  answer |= data_[position_ + 2] << 16;
  answer |= data_[position_ + 3] << 24;
  return answer;
}


void SnapshotByteSource::CopyRaw(byte* to, int number_of_bytes) {
  MemCopy(to, data_ + position_, number_of_bytes);
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

void SnapshotByteSink::PutRaw(byte* data, int number_of_bytes,
                              const char* description) {
  data_.AddAll(Vector<byte>(data, number_of_bytes));
}

void SnapshotByteSink::PutBlob(byte* data, int number_of_bytes,
                               const char* description) {
  PutInt(number_of_bytes, description);
  PutRaw(data, number_of_bytes, description);
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

  if (position_ + size < length_) {
    *data = &data_[position_];
    Advance(size);
    return true;
  } else {
    Advance(length_ - position_);  // proceed until end.
    return false;
  }
}

}  // namespace v8::internal
}  // namespace v8
