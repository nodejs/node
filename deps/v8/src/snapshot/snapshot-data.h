// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_DATA_H_
#define V8_SNAPSHOT_SNAPSHOT_DATA_H_

#include "src/base/bit-field.h"
#include "src/base/memory.h"
#include "src/base/vector.h"
#include "src/codegen/external-reference-table.h"
#include "src/utils/memcopy.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Isolate;
class Serializer;

class SerializedData {
 public:
  SerializedData(uint8_t* data, int size)
      : data_(data), size_(size), owns_data_(false) {}
  SerializedData() : data_(nullptr), size_(0), owns_data_(false) {}
  SerializedData(SerializedData&& other) V8_NOEXCEPT
      : data_(other.data_),
        size_(other.size_),
        owns_data_(other.owns_data_) {
    // Ensure |other| will not attempt to destroy our data in destructor.
    other.owns_data_ = false;
  }
  SerializedData(const SerializedData&) = delete;
  SerializedData& operator=(const SerializedData&) = delete;

  virtual ~SerializedData() {
    if (owns_data_) DeleteArray<uint8_t>(data_);
  }

  uint32_t GetMagicNumber() const { return GetHeaderValue(kMagicNumberOffset); }

  using ChunkSizeBits = base::BitField<uint32_t, 0, 31>;
  using IsLastChunkBits = base::BitField<bool, 31, 1>;

  static constexpr uint32_t kMagicNumberOffset = 0;
  static constexpr uint32_t kMagicNumber =
      0xC0DE0000 ^ ExternalReferenceTable::kSize;

 protected:
  void SetHeaderValue(uint32_t offset, uint32_t value) {
    base::WriteLittleEndianValue(reinterpret_cast<Address>(data_) + offset,
                                 value);
  }

  uint32_t GetHeaderValue(uint32_t offset) const {
    return base::ReadLittleEndianValue<uint32_t>(
        reinterpret_cast<Address>(data_) + offset);
  }

  void AllocateData(uint32_t size);

  void SetMagicNumber() { SetHeaderValue(kMagicNumberOffset, kMagicNumber); }

  uint8_t* data_;
  uint32_t size_;
  bool owns_data_;
};

// Wrapper around reservation sizes and the serialization payload.
class V8_EXPORT_PRIVATE SnapshotData : public SerializedData {
 public:
  // Used when producing.
  explicit SnapshotData(const Serializer* serializer);

  // Used when consuming.
  explicit SnapshotData(const base::Vector<const uint8_t> snapshot)
      : SerializedData(const_cast<uint8_t*>(snapshot.begin()),
                       snapshot.length()) {}

  virtual base::Vector<const uint8_t> Payload() const;

  base::Vector<const uint8_t> RawData() const {
    return base::Vector<const uint8_t>(data_, size_);
  }

 protected:
  // Empty constructor used by SnapshotCompression so it can manually allocate
  // memory.
  SnapshotData() : SerializedData() {}
  friend class SnapshotCompression;

  // Resize used by SnapshotCompression so it can shrink the compressed
  // SnapshotData.
  void Resize(uint32_t size) { size_ = size; }

  // The data header consists of uint32_t-sized entries:
  // [0] magic number and (internal) external reference count
  // [1] payload length
  // ... serialized payload
  static const uint32_t kPayloadLengthOffset = kMagicNumberOffset + kUInt32Size;
  static const uint32_t kHeaderSize = kPayloadLengthOffset + kUInt32Size;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_DATA_H_
