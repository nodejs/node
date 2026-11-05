// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/snapshot/snapshot-data.h"

#include <algorithm>

#include "src/base/numerics/safe_conversions.h"
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

void SerializedData::AllocateWithPayload(
    uint32_t header_size, const std::vector<uint8_t>* untrusted_payload,
    const std::vector<uint8_t>* external_references_payload,
    const std::vector<uint8_t>* trusted_payload) {
  size_t trusted_length =
      external_references_payload->size() + trusted_payload->size();
  size_t payload_length = trusted_length + untrusted_payload->size();

  AllocateData(base::checked_cast<uint32_t>(header_size + payload_length));
  std::fill_n(data_, header_size, 0);
  SetMagicNumber();
  SetHeaderValue(kPayloadLengthOffset,
                 base::checked_cast<uint32_t>(payload_length));
  SetHeaderValue(kTrustedPayloadLengthOffset,
                 base::checked_cast<uint32_t>(trusted_length));

  uint8_t* end = data_ + header_size;
  auto append = [&end](const std::vector<uint8_t>* bytes) {
    if (bytes->empty()) return;
    CopyBytes(end, bytes->data(), bytes->size());
    end += bytes->size();
  };
  append(external_references_payload);
  append(trusted_payload);
  append(untrusted_payload);
  DCHECK_EQ(end, data_ + size_);
}

SnapshotData::SnapshotData(const Serializer* serializer) {
  DisallowGarbageCollection no_gc;
  // The trusted section is empty.
  const std::vector<uint8_t> empty;
  AllocateWithPayload(kHeaderSize, serializer->Payload(), &empty, &empty);
}

base::Vector<const uint8_t> SnapshotData::Payload() const {
  const uint8_t* payload = data_ + kHeaderSize;
  uint32_t length = GetHeaderValue(kPayloadLengthOffset);
  DCHECK_EQ(data_ + size_, payload + length);
  return base::Vector<const uint8_t>(payload, length);
}

base::Vector<const uint8_t> SnapshotData::UntrustedPayload() const {
  uint32_t trusted_length = GetHeaderValue(kTrustedPayloadLengthOffset);
  const uint8_t* payload = data_ + kHeaderSize + trusted_length;
  uint32_t length = GetHeaderValue(kPayloadLengthOffset) - trusted_length;
  DCHECK_EQ(data_ + size_, payload + length);
  return base::Vector<const uint8_t>(payload, length);
}

base::Vector<const uint8_t> SnapshotData::TrustedPayload() const {
  const uint8_t* payload = data_ + kHeaderSize;
  uint32_t length = GetHeaderValue(kTrustedPayloadLengthOffset);
  return base::Vector<const uint8_t>(payload, length);
}

}  // namespace internal
}  // namespace v8
