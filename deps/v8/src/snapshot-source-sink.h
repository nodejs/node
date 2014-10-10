// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SNAPSHOT_SOURCE_SINK_H_
#define V8_SNAPSHOT_SOURCE_SINK_H_

#include "src/base/logging.h"
#include "src/utils.h"

namespace v8 {
namespace internal {


/**
 * Source to read snapshot and builtins files from.
 *
 * Note: Memory ownership remains with callee.
 */
class SnapshotByteSource FINAL {
 public:
  SnapshotByteSource(const byte* array, int length);
  ~SnapshotByteSource();

  bool HasMore() { return position_ < length_; }

  int Get() {
    DCHECK(position_ < length_);
    return data_[position_++];
  }

  int32_t GetUnalignedInt();

  void Advance(int by) { position_ += by; }

  void CopyRaw(byte* to, int number_of_bytes);

  inline int GetInt() {
    // This way of variable-length encoding integers does not suffer from branch
    // mispredictions.
    uint32_t answer = GetUnalignedInt();
    int bytes = (answer & 3) + 1;
    Advance(bytes);
    uint32_t mask = 0xffffffffu;
    mask >>= 32 - (bytes << 3);
    answer &= mask;
    answer >>= 2;
    return answer;
  }

  bool GetBlob(const byte** data, int* number_of_bytes);

  bool AtEOF();

  int position() { return position_; }

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
  virtual ~SnapshotByteSink() { }
  virtual void Put(byte b, const char* description) = 0;
  virtual void PutSection(int b, const char* description) {
    DCHECK_LE(b, kMaxUInt8);
    Put(static_cast<byte>(b), description);
  }
  void PutInt(uintptr_t integer, const char* description);
  void PutRaw(byte* data, int number_of_bytes, const char* description);
  void PutBlob(byte* data, int number_of_bytes, const char* description);
  virtual int Position() = 0;
};


class DummySnapshotSink : public SnapshotByteSink {
 public:
  DummySnapshotSink() : length_(0) {}
  virtual ~DummySnapshotSink() {}
  virtual void Put(byte b, const char* description) { length_++; }
  virtual int Position() { return length_; }

 private:
  int length_;
};


// Wrap a SnapshotByteSink into a DebugSnapshotSink to get debugging output.
class DebugSnapshotSink : public SnapshotByteSink {
 public:
  explicit DebugSnapshotSink(SnapshotByteSink* chained) : sink_(chained) {}
  virtual void Put(byte b, const char* description) OVERRIDE;
  virtual int Position() OVERRIDE { return sink_->Position(); }

 private:
  SnapshotByteSink* sink_;
};


class ListSnapshotSink : public i::SnapshotByteSink {
 public:
  explicit ListSnapshotSink(i::List<byte>* data) : data_(data) {}
  virtual void Put(byte b, const char* description) OVERRIDE {
    data_->Add(b);
  }
  virtual int Position() OVERRIDE { return data_->length(); }

 private:
  i::List<byte>* data_;
};

}  // namespace v8::internal
}  // namespace v8

#endif  // V8_SNAPSHOT_SOURCE_SINK_H_
