// Protocol Buffers - Google's data interchange format
// Copyright 2008 Google Inc.  All rights reserved.
// https://developers.google.com/protocol-buffers/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Author: kenton@google.com (Kenton Varda)
//  Based on original Protocol Buffers design by
//  Sanjay Ghemawat, Jeff Dean, and others.
//
// This implementation is heavily optimized to make reads and writes
// of small values (especially varints) as fast as possible.  In
// particular, we optimize for the common case that a read or a write
// will not cross the end of the buffer, since we can avoid a lot
// of branching in this case.

#include <limits.h>
#include <algorithm>
#include <utility>
#include <google/protobuf/stubs/logging.h>
#include <google/protobuf/stubs/common.h>
#include <google/protobuf/io/coded_stream_inl.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/arena.h>
#include <google/protobuf/stubs/stl_util.h>


#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace io {

namespace {

static const int kMaxVarintBytes = 10;
static const int kMaxVarint32Bytes = 5;


inline bool NextNonEmpty(ZeroCopyInputStream* input, const void** data,
                         int* size) {
  bool success;
  do {
    success = input->Next(data, size);
  } while (success && *size == 0);
  return success;
}

}  // namespace

// CodedInputStream ==================================================

CodedInputStream::~CodedInputStream() {
  if (input_ != NULL) {
    BackUpInputToCurrentPosition();
  }
}

// Static.
int CodedInputStream::default_recursion_limit_ = 100;


void CodedOutputStream::EnableAliasing(bool enabled) {
  aliasing_enabled_ = enabled && output_->AllowsAliasing();
}

void CodedInputStream::BackUpInputToCurrentPosition() {
  int backup_bytes = BufferSize() + buffer_size_after_limit_ + overflow_bytes_;
  if (backup_bytes > 0) {
    input_->BackUp(backup_bytes);

    // total_bytes_read_ doesn't include overflow_bytes_.
    total_bytes_read_ -= BufferSize() + buffer_size_after_limit_;
    buffer_end_ = buffer_;
    buffer_size_after_limit_ = 0;
    overflow_bytes_ = 0;
  }
}

inline void CodedInputStream::RecomputeBufferLimits() {
  buffer_end_ += buffer_size_after_limit_;
  int closest_limit = std::min(current_limit_, total_bytes_limit_);
  if (closest_limit < total_bytes_read_) {
    // The limit position is in the current buffer.  We must adjust
    // the buffer size accordingly.
    buffer_size_after_limit_ = total_bytes_read_ - closest_limit;
    buffer_end_ -= buffer_size_after_limit_;
  } else {
    buffer_size_after_limit_ = 0;
  }
}

CodedInputStream::Limit CodedInputStream::PushLimit(int byte_limit) {
  // Current position relative to the beginning of the stream.
  int current_position = CurrentPosition();

  Limit old_limit = current_limit_;

  // security: byte_limit is possibly evil, so check for negative values
  // and overflow. Also check that the new requested limit is before the
  // previous limit; otherwise we continue to enforce the previous limit.
  if (PROTOBUF_PREDICT_TRUE(byte_limit >= 0 &&
                            byte_limit <= INT_MAX - current_position &&
                            byte_limit < current_limit_ - current_position)) {
    current_limit_ = current_position + byte_limit;
    RecomputeBufferLimits();
  }

  return old_limit;
}

void CodedInputStream::PopLimit(Limit limit) {
  // The limit passed in is actually the *old* limit, which we returned from
  // PushLimit().
  current_limit_ = limit;
  RecomputeBufferLimits();

  // We may no longer be at a legitimate message end.  ReadTag() needs to be
  // called again to find out.
  legitimate_message_end_ = false;
}

std::pair<CodedInputStream::Limit, int>
CodedInputStream::IncrementRecursionDepthAndPushLimit(int byte_limit) {
  return std::make_pair(PushLimit(byte_limit), --recursion_budget_);
}

CodedInputStream::Limit CodedInputStream::ReadLengthAndPushLimit() {
  uint32 length;
  return PushLimit(ReadVarint32(&length) ? length : 0);
}

bool CodedInputStream::DecrementRecursionDepthAndPopLimit(Limit limit) {
  bool result = ConsumedEntireMessage();
  PopLimit(limit);
  GOOGLE_DCHECK_LT(recursion_budget_, recursion_limit_);
  ++recursion_budget_;
  return result;
}

bool CodedInputStream::CheckEntireMessageConsumedAndPopLimit(Limit limit) {
  bool result = ConsumedEntireMessage();
  PopLimit(limit);
  return result;
}

int CodedInputStream::BytesUntilLimit() const {
  if (current_limit_ == INT_MAX) return -1;
  int current_position = CurrentPosition();

  return current_limit_ - current_position;
}

void CodedInputStream::SetTotalBytesLimit(int total_bytes_limit) {
  // Make sure the limit isn't already past, since this could confuse other
  // code.
  int current_position = CurrentPosition();
  total_bytes_limit_ = std::max(current_position, total_bytes_limit);
  RecomputeBufferLimits();
}

int CodedInputStream::BytesUntilTotalBytesLimit() const {
  if (total_bytes_limit_ == INT_MAX) return -1;
  return total_bytes_limit_ - CurrentPosition();
}

void CodedInputStream::PrintTotalBytesLimitError() {
  GOOGLE_LOG(ERROR) << "A protocol message was rejected because it was too "
                "big (more than "
             << total_bytes_limit_
             << " bytes).  To increase the limit (or to disable these "
                "warnings), see CodedInputStream::SetTotalBytesLimit() "
                "in net/proto2/io/public/coded_stream.h.";
}

bool CodedInputStream::SkipFallback(int count, int original_buffer_size) {
  if (buffer_size_after_limit_ > 0) {
    // We hit a limit inside this buffer.  Advance to the limit and fail.
    Advance(original_buffer_size);
    return false;
  }

  count -= original_buffer_size;
  buffer_ = NULL;
  buffer_end_ = buffer_;

  // Make sure this skip doesn't try to skip past the current limit.
  int closest_limit = std::min(current_limit_, total_bytes_limit_);
  int bytes_until_limit = closest_limit - total_bytes_read_;
  if (bytes_until_limit < count) {
    // We hit the limit.  Skip up to it then fail.
    if (bytes_until_limit > 0) {
      total_bytes_read_ = closest_limit;
      input_->Skip(bytes_until_limit);
    }
    return false;
  }

  if (!input_->Skip(count)) {
    total_bytes_read_ = input_->ByteCount();
    return false;
  }
  total_bytes_read_ += count;
  return true;
}

bool CodedInputStream::GetDirectBufferPointer(const void** data, int* size) {
  if (BufferSize() == 0 && !Refresh()) return false;

  *data = buffer_;
  *size = BufferSize();
  return true;
}

bool CodedInputStream::ReadRaw(void* buffer, int size) {
  return InternalReadRawInline(buffer, size);
}

bool CodedInputStream::ReadString(std::string* buffer, int size) {
  if (size < 0) return false;  // security: size is often user-supplied
  return InternalReadStringInline(buffer, size);
}

bool CodedInputStream::ReadStringFallback(std::string* buffer, int size) {
  if (!buffer->empty()) {
    buffer->clear();
  }

  int closest_limit = std::min(current_limit_, total_bytes_limit_);
  if (closest_limit != INT_MAX) {
    int bytes_to_limit = closest_limit - CurrentPosition();
    if (bytes_to_limit > 0 && size > 0 && size <= bytes_to_limit) {
      buffer->reserve(size);
    }
  }

  int current_buffer_size;
  while ((current_buffer_size = BufferSize()) < size) {
    // Some STL implementations "helpfully" crash on buffer->append(NULL, 0).
    if (current_buffer_size != 0) {
      // Note:  string1.append(string2) is O(string2.size()) (as opposed to
      //   O(string1.size() + string2.size()), which would be bad).
      buffer->append(reinterpret_cast<const char*>(buffer_),
                     current_buffer_size);
    }
    size -= current_buffer_size;
    Advance(current_buffer_size);
    if (!Refresh()) return false;
  }

  buffer->append(reinterpret_cast<const char*>(buffer_), size);
  Advance(size);

  return true;
}


bool CodedInputStream::ReadLittleEndian32Fallback(uint32* value) {
  uint8 bytes[sizeof(*value)];

  const uint8* ptr;
  if (BufferSize() >= sizeof(*value)) {
    // Fast path:  Enough bytes in the buffer to read directly.
    ptr = buffer_;
    Advance(sizeof(*value));
  } else {
    // Slow path:  Had to read past the end of the buffer.
    if (!ReadRaw(bytes, sizeof(*value))) return false;
    ptr = bytes;
  }
  ReadLittleEndian32FromArray(ptr, value);
  return true;
}

bool CodedInputStream::ReadLittleEndian64Fallback(uint64* value) {
  uint8 bytes[sizeof(*value)];

  const uint8* ptr;
  if (BufferSize() >= sizeof(*value)) {
    // Fast path:  Enough bytes in the buffer to read directly.
    ptr = buffer_;
    Advance(sizeof(*value));
  } else {
    // Slow path:  Had to read past the end of the buffer.
    if (!ReadRaw(bytes, sizeof(*value))) return false;
    ptr = bytes;
  }
  ReadLittleEndian64FromArray(ptr, value);
  return true;
}

namespace {

// Decodes varint64 with known size, N, and returns next pointer. Knowing N at
// compile time, compiler can generate optimal code. For example, instead of
// subtracting 0x80 at each iteration, it subtracts properly shifted mask once.
template <size_t N>
const uint8* DecodeVarint64KnownSize(const uint8* buffer, uint64* value) {
  GOOGLE_DCHECK_GT(N, 0);
  uint64 result = static_cast<uint64>(buffer[N - 1]) << (7 * (N - 1));
  for (int i = 0, offset = 0; i < N - 1; i++, offset += 7) {
    result += static_cast<uint64>(buffer[i] - 0x80) << offset;
  }
  *value = result;
  return buffer + N;
}

// Read a varint from the given buffer, write it to *value, and return a pair.
// The first part of the pair is true iff the read was successful.  The second
// part is buffer + (number of bytes read).  This function is always inlined,
// so returning a pair is costless.
PROTOBUF_ALWAYS_INLINE
::std::pair<bool, const uint8*> ReadVarint32FromArray(uint32 first_byte,
                                                      const uint8* buffer,
                                                      uint32* value);
inline ::std::pair<bool, const uint8*> ReadVarint32FromArray(
    uint32 first_byte, const uint8* buffer, uint32* value) {
  // Fast path:  We have enough bytes left in the buffer to guarantee that
  // this read won't cross the end, so we can skip the checks.
  GOOGLE_DCHECK_EQ(*buffer, first_byte);
  GOOGLE_DCHECK_EQ(first_byte & 0x80, 0x80) << first_byte;
  const uint8* ptr = buffer;
  uint32 b;
  uint32 result = first_byte - 0x80;
  ++ptr;  // We just processed the first byte.  Move on to the second.
  b = *(ptr++);
  result += b << 7;
  if (!(b & 0x80)) goto done;
  result -= 0x80 << 7;
  b = *(ptr++);
  result += b << 14;
  if (!(b & 0x80)) goto done;
  result -= 0x80 << 14;
  b = *(ptr++);
  result += b << 21;
  if (!(b & 0x80)) goto done;
  result -= 0x80 << 21;
  b = *(ptr++);
  result += b << 28;
  if (!(b & 0x80)) goto done;
  // "result -= 0x80 << 28" is irrevelant.

  // If the input is larger than 32 bits, we still need to read it all
  // and discard the high-order bits.
  for (int i = 0; i < kMaxVarintBytes - kMaxVarint32Bytes; i++) {
    b = *(ptr++);
    if (!(b & 0x80)) goto done;
  }

  // We have overrun the maximum size of a varint (10 bytes).  Assume
  // the data is corrupt.
  return std::make_pair(false, ptr);

done:
  *value = result;
  return std::make_pair(true, ptr);
}

PROTOBUF_ALWAYS_INLINE::std::pair<bool, const uint8*> ReadVarint64FromArray(
    const uint8* buffer, uint64* value);
inline ::std::pair<bool, const uint8*> ReadVarint64FromArray(
    const uint8* buffer, uint64* value) {
  // Assumes varint64 is at least 2 bytes.
  GOOGLE_DCHECK_GE(buffer[0], 128);

  const uint8* next;
  if (buffer[1] < 128) {
    next = DecodeVarint64KnownSize<2>(buffer, value);
  } else if (buffer[2] < 128) {
    next = DecodeVarint64KnownSize<3>(buffer, value);
  } else if (buffer[3] < 128) {
    next = DecodeVarint64KnownSize<4>(buffer, value);
  } else if (buffer[4] < 128) {
    next = DecodeVarint64KnownSize<5>(buffer, value);
  } else if (buffer[5] < 128) {
    next = DecodeVarint64KnownSize<6>(buffer, value);
  } else if (buffer[6] < 128) {
    next = DecodeVarint64KnownSize<7>(buffer, value);
  } else if (buffer[7] < 128) {
    next = DecodeVarint64KnownSize<8>(buffer, value);
  } else if (buffer[8] < 128) {
    next = DecodeVarint64KnownSize<9>(buffer, value);
  } else if (buffer[9] < 128) {
    next = DecodeVarint64KnownSize<10>(buffer, value);
  } else {
    // We have overrun the maximum size of a varint (10 bytes). Assume
    // the data is corrupt.
    return std::make_pair(false, buffer + 11);
  }

  return std::make_pair(true, next);
}

}  // namespace

bool CodedInputStream::ReadVarint32Slow(uint32* value) {
  // Directly invoke ReadVarint64Fallback, since we already tried to optimize
  // for one-byte varints.
  std::pair<uint64, bool> p = ReadVarint64Fallback();
  *value = static_cast<uint32>(p.first);
  return p.second;
}

int64 CodedInputStream::ReadVarint32Fallback(uint32 first_byte_or_zero) {
  if (BufferSize() >= kMaxVarintBytes ||
      // Optimization:  We're also safe if the buffer is non-empty and it ends
      // with a byte that would terminate a varint.
      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    GOOGLE_DCHECK_NE(first_byte_or_zero, 0)
        << "Caller should provide us with *buffer_ when buffer is non-empty";
    uint32 temp;
    ::std::pair<bool, const uint8*> p =
        ReadVarint32FromArray(first_byte_or_zero, buffer_, &temp);
    if (!p.first) return -1;
    buffer_ = p.second;
    return temp;
  } else {
    // Really slow case: we will incur the cost of an extra function call here,
    // but moving this out of line reduces the size of this function, which
    // improves the common case. In micro benchmarks, this is worth about 10-15%
    uint32 temp;
    return ReadVarint32Slow(&temp) ? static_cast<int64>(temp) : -1;
  }
}

int CodedInputStream::ReadVarintSizeAsIntSlow() {
  // Directly invoke ReadVarint64Fallback, since we already tried to optimize
  // for one-byte varints.
  std::pair<uint64, bool> p = ReadVarint64Fallback();
  if (!p.second || p.first > static_cast<uint64>(INT_MAX)) return -1;
  return p.first;
}

int CodedInputStream::ReadVarintSizeAsIntFallback() {
  if (BufferSize() >= kMaxVarintBytes ||
      // Optimization:  We're also safe if the buffer is non-empty and it ends
      // with a byte that would terminate a varint.
      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    uint64 temp;
    ::std::pair<bool, const uint8*> p = ReadVarint64FromArray(buffer_, &temp);
    if (!p.first || temp > static_cast<uint64>(INT_MAX)) return -1;
    buffer_ = p.second;
    return temp;
  } else {
    // Really slow case: we will incur the cost of an extra function call here,
    // but moving this out of line reduces the size of this function, which
    // improves the common case. In micro benchmarks, this is worth about 10-15%
    return ReadVarintSizeAsIntSlow();
  }
}

uint32 CodedInputStream::ReadTagSlow() {
  if (buffer_ == buffer_end_) {
    // Call refresh.
    if (!Refresh()) {
      // Refresh failed.  Make sure that it failed due to EOF, not because
      // we hit total_bytes_limit_, which, unlike normal limits, is not a
      // valid place to end a message.
      int current_position = total_bytes_read_ - buffer_size_after_limit_;
      if (current_position >= total_bytes_limit_) {
        // Hit total_bytes_limit_.  But if we also hit the normal limit,
        // we're still OK.
        legitimate_message_end_ = current_limit_ == total_bytes_limit_;
      } else {
        legitimate_message_end_ = true;
      }
      return 0;
    }
  }

  // For the slow path, just do a 64-bit read. Try to optimize for one-byte tags
  // again, since we have now refreshed the buffer.
  uint64 result = 0;
  if (!ReadVarint64(&result)) return 0;
  return static_cast<uint32>(result);
}

uint32 CodedInputStream::ReadTagFallback(uint32 first_byte_or_zero) {
  const int buf_size = BufferSize();
  if (buf_size >= kMaxVarintBytes ||
      // Optimization:  We're also safe if the buffer is non-empty and it ends
      // with a byte that would terminate a varint.
      (buf_size > 0 && !(buffer_end_[-1] & 0x80))) {
    GOOGLE_DCHECK_EQ(first_byte_or_zero, buffer_[0]);
    if (first_byte_or_zero == 0) {
      ++buffer_;
      return 0;
    }
    uint32 tag;
    ::std::pair<bool, const uint8*> p =
        ReadVarint32FromArray(first_byte_or_zero, buffer_, &tag);
    if (!p.first) {
      return 0;
    }
    buffer_ = p.second;
    return tag;
  } else {
    // We are commonly at a limit when attempting to read tags. Try to quickly
    // detect this case without making another function call.
    if ((buf_size == 0) &&
        ((buffer_size_after_limit_ > 0) ||
         (total_bytes_read_ == current_limit_)) &&
        // Make sure that the limit we hit is not total_bytes_limit_, since
        // in that case we still need to call Refresh() so that it prints an
        // error.
        total_bytes_read_ - buffer_size_after_limit_ < total_bytes_limit_) {
      // We hit a byte limit.
      legitimate_message_end_ = true;
      return 0;
    }
    return ReadTagSlow();
  }
}

bool CodedInputStream::ReadVarint64Slow(uint64* value) {
  // Slow path:  This read might cross the end of the buffer, so we
  // need to check and refresh the buffer if and when it does.

  uint64 result = 0;
  int count = 0;
  uint32 b;

  do {
    if (count == kMaxVarintBytes) {
      *value = 0;
      return false;
    }
    while (buffer_ == buffer_end_) {
      if (!Refresh()) {
        *value = 0;
        return false;
      }
    }
    b = *buffer_;
    result |= static_cast<uint64>(b & 0x7F) << (7 * count);
    Advance(1);
    ++count;
  } while (b & 0x80);

  *value = result;
  return true;
}

std::pair<uint64, bool> CodedInputStream::ReadVarint64Fallback() {
  if (BufferSize() >= kMaxVarintBytes ||
      // Optimization:  We're also safe if the buffer is non-empty and it ends
      // with a byte that would terminate a varint.
      (buffer_end_ > buffer_ && !(buffer_end_[-1] & 0x80))) {
    uint64 temp;
    ::std::pair<bool, const uint8*> p = ReadVarint64FromArray(buffer_, &temp);
    if (!p.first) {
      return std::make_pair(0, false);
    }
    buffer_ = p.second;
    return std::make_pair(temp, true);
  } else {
    uint64 temp;
    bool success = ReadVarint64Slow(&temp);
    return std::make_pair(temp, success);
  }
}

bool CodedInputStream::Refresh() {
  GOOGLE_DCHECK_EQ(0, BufferSize());

  if (buffer_size_after_limit_ > 0 || overflow_bytes_ > 0 ||
      total_bytes_read_ == current_limit_) {
    // We've hit a limit.  Stop.
    int current_position = total_bytes_read_ - buffer_size_after_limit_;

    if (current_position >= total_bytes_limit_ &&
        total_bytes_limit_ != current_limit_) {
      // Hit total_bytes_limit_.
      PrintTotalBytesLimitError();
    }

    return false;
  }

  const void* void_buffer;
  int buffer_size;
  if (NextNonEmpty(input_, &void_buffer, &buffer_size)) {
    buffer_ = reinterpret_cast<const uint8*>(void_buffer);
    buffer_end_ = buffer_ + buffer_size;
    GOOGLE_CHECK_GE(buffer_size, 0);

    if (total_bytes_read_ <= INT_MAX - buffer_size) {
      total_bytes_read_ += buffer_size;
    } else {
      // Overflow.  Reset buffer_end_ to not include the bytes beyond INT_MAX.
      // We can't get that far anyway, because total_bytes_limit_ is guaranteed
      // to be less than it.  We need to keep track of the number of bytes
      // we discarded, though, so that we can call input_->BackUp() to back
      // up over them on destruction.

      // The following line is equivalent to:
      //   overflow_bytes_ = total_bytes_read_ + buffer_size - INT_MAX;
      // except that it avoids overflows.  Signed integer overflow has
      // undefined results according to the C standard.
      overflow_bytes_ = total_bytes_read_ - (INT_MAX - buffer_size);
      buffer_end_ -= overflow_bytes_;
      total_bytes_read_ = INT_MAX;
    }

    RecomputeBufferLimits();
    return true;
  } else {
    buffer_ = NULL;
    buffer_end_ = NULL;
    return false;
  }
}

// CodedOutputStream =================================================

std::atomic<bool> CodedOutputStream::default_serialization_deterministic_{
    false};

CodedOutputStream::CodedOutputStream(ZeroCopyOutputStream* output)
    : CodedOutputStream(output, true) {}

CodedOutputStream::CodedOutputStream(ZeroCopyOutputStream* output,
                                     bool do_eager_refresh)
    : output_(output),
      buffer_(NULL),
      buffer_size_(0),
      total_bytes_(0),
      had_error_(false),
      aliasing_enabled_(false),
      is_serialization_deterministic_(IsDefaultSerializationDeterministic()) {
  if (do_eager_refresh) {
    // Eagerly Refresh() so buffer space is immediately available.
    Refresh();
    // The Refresh() may have failed. If the client doesn't write any data,
    // though, don't consider this an error. If the client does write data, then
    // another Refresh() will be attempted and it will set the error once again.
    had_error_ = false;
  }
}

CodedOutputStream::~CodedOutputStream() { Trim(); }

void CodedOutputStream::Trim() {
  if (buffer_size_ > 0) {
    output_->BackUp(buffer_size_);
    total_bytes_ -= buffer_size_;
    buffer_size_ = 0;
    buffer_ = NULL;
  }
}

bool CodedOutputStream::Skip(int count) {
  if (count < 0) return false;

  while (count > buffer_size_) {
    count -= buffer_size_;
    if (!Refresh()) return false;
  }

  Advance(count);
  return true;
}

bool CodedOutputStream::GetDirectBufferPointer(void** data, int* size) {
  if (buffer_size_ == 0 && !Refresh()) return false;

  *data = buffer_;
  *size = buffer_size_;
  return true;
}

void CodedOutputStream::WriteRaw(const void* data, int size) {
  while (buffer_size_ < size) {
    memcpy(buffer_, data, buffer_size_);
    size -= buffer_size_;
    data = reinterpret_cast<const uint8*>(data) + buffer_size_;
    if (!Refresh()) return;
  }

  memcpy(buffer_, data, size);
  Advance(size);
}

uint8* CodedOutputStream::WriteRawToArray(const void* data, int size,
                                          uint8* target) {
  memcpy(target, data, size);
  return target + size;
}


void CodedOutputStream::WriteAliasedRaw(const void* data, int size) {
  if (size < buffer_size_
  ) {
    WriteRaw(data, size);
  } else {
    Trim();

    total_bytes_ += size;
    had_error_ |= !output_->WriteAliasedRaw(data, size);
  }
}

void CodedOutputStream::WriteLittleEndian32(uint32 value) {
  uint8 bytes[sizeof(value)];

  bool use_fast = buffer_size_ >= sizeof(value);
  uint8* ptr = use_fast ? buffer_ : bytes;

  WriteLittleEndian32ToArray(value, ptr);

  if (use_fast) {
    Advance(sizeof(value));
  } else {
    WriteRaw(bytes, sizeof(value));
  }
}

void CodedOutputStream::WriteLittleEndian64(uint64 value) {
  uint8 bytes[sizeof(value)];

  bool use_fast = buffer_size_ >= sizeof(value);
  uint8* ptr = use_fast ? buffer_ : bytes;

  WriteLittleEndian64ToArray(value, ptr);

  if (use_fast) {
    Advance(sizeof(value));
  } else {
    WriteRaw(bytes, sizeof(value));
  }
}

void CodedOutputStream::WriteVarint32SlowPath(uint32 value) {
  uint8 bytes[kMaxVarint32Bytes];
  uint8* target = &bytes[0];
  uint8* end = WriteVarint32ToArray(value, target);
  int size = end - target;
  WriteRaw(bytes, size);
}

void CodedOutputStream::WriteVarint64SlowPath(uint64 value) {
  uint8 bytes[kMaxVarintBytes];
  uint8* target = &bytes[0];
  uint8* end = WriteVarint64ToArray(value, target);
  int size = end - target;
  WriteRaw(bytes, size);
}

bool CodedOutputStream::Refresh() {
  void* void_buffer;
  if (output_->Next(&void_buffer, &buffer_size_)) {
    buffer_ = reinterpret_cast<uint8*>(void_buffer);
    total_bytes_ += buffer_size_;
    return true;
  } else {
    buffer_ = NULL;
    buffer_size_ = 0;
    had_error_ = true;
    return false;
  }
}

uint8* CodedOutputStream::WriteStringWithSizeToArray(const std::string& str,
                                                     uint8* target) {
  GOOGLE_DCHECK_LE(str.size(), kuint32max);
  target = WriteVarint32ToArray(str.size(), target);
  return WriteStringToArray(str, target);
}

}  // namespace io
}  // namespace protobuf
}  // namespace google
