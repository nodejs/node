// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/snapshot-data.h"

#include "src/common/assert-scope.h"
#include "src/snapshot/serializer.h"

namespace v8 {
namespace internal {

void SerializedData::AllocateData(uint32_t size) {
  DCHECK(!owns_data_);
  data_ = NewArray<uint8_t>(size);
  size_ = size;
  owns_data_ = true;
}

// static
constexpr uint32_t SerializedData::kMagicNumber;

SnapshotData::SnapshotData(const Serializer* serializer) {
  DisallowGarbageCollection no_gc;
  const std::vector<uint8_t>* payload = serializer->Payload();

  // Calculate sizes.
  uint32_t size = kHeaderSize + static_cast<uint32_t>(payload->size());

  // Allocate backing store and create result data.
  AllocateData(size);

  // Zero out pre-payload data. Part of that is only used for padding.
  memset(data_, 0, kHeaderSize);

  // Set header values.
  SetMagicNumber();
  SetHeaderValue(kPayloadLengthOffset, static_cast<int>(payload->size()));

  // Copy serialized data.
  CopyBytes(data_ + kHeaderSize, payload->data(),
            static_cast<size_t>(payload->size()));
}

base::Vector<const uint8_t> SnapshotData::Payload() const {
  const uint8_t* payload = data_ + kHeaderSize;
  uint32_t length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return base::Vector<const uint8_t>(payload, length);
}

}  // namespace internal
}  // namespace v8
