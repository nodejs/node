// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SNAPSHOT_SOURCE_SINK_H_
#define V8_SNAPSHOT_SNAPSHOT_SOURCE_SINK_H_

#include "src/base/logging.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


/**
 * Source to read snapshot and builtins files from.
 *
 * Note: Memory ownership remains with callee.
 */
class SnapshotByteSource final {
 public:
  SnapshotByteSource(const char* data, int length)
      : data_(reinterpret_cast<const byte*>(data)),
        length_(length),
        position_(0) {}

  explicit SnapshotByteSource(Vector<const byte> payload)
      : data_(payload.start()), length_(payload.length()), position_(0) {}

  ~SnapshotByteSource() = default;

  bool HasMore() { return position_ < length_; }

  byte Get() {
    DCHECK(position_ < length_);
    return data_[position_++];
  }

  void Advance(int by) { position_ += by; }

  void CopyRaw(byte* to, int number_of_bytes) {
    memcpy(to, data_ + position_, number_of_bytes);
    position_ += number_of_bytes;
  }

  inline int GetInt() {
    // This way of decoding variable-length encoded integers does not
    // suffer from branch mispredictions.
    DCHECK(position_ + 3 < length_);
    uint32_t answer = data_[position_];
    answer |= data_[position_ + 1] << 8;
    answer |= data_[position_ + 2] << 16;
    answer |= data_[position_ + 3] << 24;
    int bytes = (answer & 3) + 1;
    Advance(bytes);
    uint32_t mask = 0xffffffffu;
    mask >>= 32 - (bytes << 3);
    answer &= mask;
    answer >>= 2;
    return answer;
  }

  // Returns length.
  int GetBlob(const byte** data);

  int position() { return position_; }
  void set_position(int position) { position_ = position; }

 private:
  const byte* data_;
  int length_;
  int position_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotByteSource);
};


/**
 * Sink to write snapshot files to.
 *
 * Subclasses must implement actual storage or i/o.
 */
class SnapshotByteSink {
 public:
  SnapshotByteSink() = default;
  explicit SnapshotByteSink(int initial_size) : data_(initial_size) {}

  ~SnapshotByteSink() = default;

  void Put(byte b, const char* description) { data_.push_back(b); }

  void PutSection(int b, const char* description) {
    DCHECK_LE(b, kMaxUInt8);
    Put(static_cast<byte>(b), description);
  }

  void PutInt(uintptr_t integer, const char* description);
  void PutRaw(const byte* data, int number_of_bytes, const char* description);

  void Append(const SnapshotByteSink& other);
  int Position() const { return static_cast<int>(data_.size()); }

  const std::vector<byte>* data() const { return &data_; }

 private:
  std::vector<byte> data_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SNAPSHOT_SOURCE_SINK_H_
