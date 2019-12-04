// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/scanner-character-streams.h"

#include <memory>
#include <vector>

#include "include/v8.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/scanner.h"
#include "src/strings/unicode-inl.h"

namespace v8 {
namespace internal {

class ScopedExternalStringLock {
 public:
  explicit ScopedExternalStringLock(ExternalString string) {
    DCHECK(!string.is_null());
    if (string.IsExternalOneByteString()) {
      resource_ = ExternalOneByteString::cast(string).resource();
    } else {
      DCHECK(string.IsExternalTwoByteString());
      resource_ = ExternalTwoByteString::cast(string).resource();
    }
    DCHECK(resource_);
    resource_->Lock();
  }

  // Copying a lock increases the locking depth.
  ScopedExternalStringLock(const ScopedExternalStringLock& other) V8_NOEXCEPT
      : resource_(other.resource_) {
    resource_->Lock();
  }

  ~ScopedExternalStringLock() { resource_->Unlock(); }

 private:
  // Not nullptr.
  const v8::String::ExternalStringResourceBase* resource_;
};

namespace {
const unibrow::uchar kUtf8Bom = 0xFEFF;
}  // namespace

template <typename Char>
struct Range {
  const Char* start;
  const Char* end;

  size_t length() { return static_cast<size_t>(end - start); }
  bool unaligned_start() const {
    return reinterpret_cast<intptr_t>(start) % sizeof(Char) == 1;
  }
};

// A Char stream backed by an on-heap SeqOneByteString or SeqTwoByteString.
template <typename Char>
class OnHeapStream {
 public:
  using String = typename CharTraits<Char>::String;

  OnHeapStream(Handle<String> string, size_t start_offset, size_t end)
      : string_(string), start_offset_(start_offset), length_(end) {}

  OnHeapStream(const OnHeapStream&) V8_NOEXCEPT : start_offset_(0), length_(0) {
    UNREACHABLE();
  }

  // The no_gc argument is only here because of the templated way this class
  // is used along with other implementations that require V8 heap access.
  Range<Char> GetDataAt(size_t pos, RuntimeCallStats* stats,
                        DisallowHeapAllocation* no_gc) {
    return {&string_->GetChars(*no_gc)[start_offset_ + Min(length_, pos)],
            &string_->GetChars(*no_gc)[start_offset_ + length_]};
  }

  static const bool kCanBeCloned = false;
  static const bool kCanAccessHeap = true;

 private:
  Handle<String> string_;
  const size_t start_offset_;
  const size_t length_;
};

// A Char stream backed by an off-heap ExternalOneByteString or
// ExternalTwoByteString.
template <typename Char>
class ExternalStringStream {
  using ExternalString = typename CharTraits<Char>::ExternalString;

 public:
  ExternalStringStream(ExternalString string, size_t start_offset,
                       size_t length)
      : lock_(string),
        data_(string.GetChars() + start_offset),
        length_(length) {}

  ExternalStringStream(const ExternalStringStream& other) V8_NOEXCEPT
      : lock_(other.lock_),
        data_(other.data_),
        length_(other.length_) {}

  // The no_gc argument is only here because of the templated way this class
  // is used along with other implementations that require V8 heap access.
  Range<Char> GetDataAt(size_t pos, RuntimeCallStats* stats,
                        DisallowHeapAllocation* no_gc = nullptr) {
    return {&data_[Min(length_, pos)], &data_[length_]};
  }

  static const bool kCanBeCloned = true;
  static const bool kCanAccessHeap = false;

 private:
  ScopedExternalStringLock lock_;
  const Char* const data_;
  const size_t length_;
};

// A Char stream backed by a C array. Testing only.
template <typename Char>
class TestingStream {
 public:
  TestingStream(const Char* data, size_t length)
      : data_(data), length_(length) {}
  // The no_gc argument is only here because of the templated way this class
  // is used along with other implementations that require V8 heap access.
  Range<Char> GetDataAt(size_t pos, RuntimeCallStats* stats,
                        DisallowHeapAllocation* no_gc = nullptr) {
    return {&data_[Min(length_, pos)], &data_[length_]};
  }

  static const bool kCanBeCloned = true;
  static const bool kCanAccessHeap = false;

 private:
  const Char* const data_;
  const size_t length_;
};

// A Char stream backed by multiple source-stream provided off-heap chunks.
template <typename Char>
class ChunkedStream {
 public:
  explicit ChunkedStream(ScriptCompiler::ExternalSourceStream* source)
      : source_(source) {}

  ChunkedStream(const ChunkedStream&) V8_NOEXCEPT {
    // TODO(rmcilroy): Implement cloning for chunked streams.
    UNREACHABLE();
  }

  // The no_gc argument is only here because of the templated way this class
  // is used along with other implementations that require V8 heap access.
  Range<Char> GetDataAt(size_t pos, RuntimeCallStats* stats,
                        DisallowHeapAllocation* no_gc = nullptr) {
    Chunk chunk = FindChunk(pos, stats);
    size_t buffer_end = chunk.length;
    size_t buffer_pos = Min(buffer_end, pos - chunk.position);
    return {&chunk.data[buffer_pos], &chunk.data[buffer_end]};
  }

  ~ChunkedStream() {
    for (Chunk& chunk : chunks_) delete[] chunk.data;
  }

  static const bool kCanBeCloned = false;
  static const bool kCanAccessHeap = false;

 private:
  struct Chunk {
    Chunk(const Char* const data, size_t position, size_t length)
        : data(data), position(position), length(length) {}
    const Char* const data;
    // The logical position of data.
    const size_t position;
    const size_t length;
    size_t end_position() const { return position + length; }
  };

  Chunk FindChunk(size_t position, RuntimeCallStats* stats) {
    while (V8_UNLIKELY(chunks_.empty())) FetchChunk(size_t{0}, stats);

    // Walk forwards while the position is in front of the current chunk.
    while (position >= chunks_.back().end_position() &&
           chunks_.back().length > 0) {
      FetchChunk(chunks_.back().end_position(), stats);
    }

    // Walk backwards.
    for (auto reverse_it = chunks_.rbegin(); reverse_it != chunks_.rend();
         ++reverse_it) {
      if (reverse_it->position <= position) return *reverse_it;
    }

    UNREACHABLE();
  }

  virtual void ProcessChunk(const uint8_t* data, size_t position,
                            size_t length) {
    // Incoming data has to be aligned to Char size.
    DCHECK_EQ(0, length % sizeof(Char));
    chunks_.emplace_back(reinterpret_cast<const Char*>(data), position,
                         length / sizeof(Char));
  }

  void FetchChunk(size_t position, RuntimeCallStats* stats) {
    const uint8_t* data = nullptr;
    size_t length;
    {
      RuntimeCallTimerScope scope(stats,
                                  RuntimeCallCounterId::kGetMoreDataCallback);
      length = source_->GetMoreData(&data);
    }
    ProcessChunk(data, position, length);
  }

  ScriptCompiler::ExternalSourceStream* source_;

 protected:
  std::vector<struct Chunk> chunks_;
};

// Provides a buffered utf-16 view on the bytes from the underlying ByteStream.
// Chars are buffered if either the underlying stream isn't utf-16 or the
// underlying utf-16 stream might move (is on-heap).
template <template <typename T> class ByteStream>
class BufferedCharacterStream : public Utf16CharacterStream {
 public:
  template <class... TArgs>
  BufferedCharacterStream(size_t pos, TArgs... args) : byte_stream_(args...) {
    buffer_pos_ = pos;
  }

  bool can_be_cloned() const final {
    return ByteStream<uint16_t>::kCanBeCloned;
  }

  std::unique_ptr<Utf16CharacterStream> Clone() const override {
    CHECK(can_be_cloned());
    return std::unique_ptr<Utf16CharacterStream>(
        new BufferedCharacterStream<ByteStream>(*this));
  }

 protected:
  bool ReadBlock() final {
    size_t position = pos();
    buffer_pos_ = position;
    buffer_start_ = &buffer_[0];
    buffer_cursor_ = buffer_start_;

    DisallowHeapAllocation no_gc;
    Range<uint8_t> range =
        byte_stream_.GetDataAt(position, runtime_call_stats(), &no_gc);
    if (range.length() == 0) {
      buffer_end_ = buffer_start_;
      return false;
    }

    size_t length = Min(kBufferSize, range.length());
    i::CopyCharsUnsigned(buffer_, range.start, length);
    buffer_end_ = &buffer_[length];
    return true;
  }

  bool can_access_heap() const final {
    return ByteStream<uint8_t>::kCanAccessHeap;
  }

 private:
  BufferedCharacterStream(const BufferedCharacterStream<ByteStream>& other)
      : byte_stream_(other.byte_stream_) {}

  static const size_t kBufferSize = 512;
  uc16 buffer_[kBufferSize];
  ByteStream<uint8_t> byte_stream_;
};

// Provides a unbuffered utf-16 view on the bytes from the underlying
// ByteStream.
template <template <typename T> class ByteStream>
class UnbufferedCharacterStream : public Utf16CharacterStream {
 public:
  template <class... TArgs>
  UnbufferedCharacterStream(size_t pos, TArgs... args) : byte_stream_(args...) {
    buffer_pos_ = pos;
  }

  bool can_access_heap() const final {
    return ByteStream<uint16_t>::kCanAccessHeap;
  }

  bool can_be_cloned() const final {
    return ByteStream<uint16_t>::kCanBeCloned;
  }

  std::unique_ptr<Utf16CharacterStream> Clone() const override {
    return std::unique_ptr<Utf16CharacterStream>(
        new UnbufferedCharacterStream<ByteStream>(*this));
  }

 protected:
  bool ReadBlock() final {
    size_t position = pos();
    buffer_pos_ = position;
    DisallowHeapAllocation no_gc;
    Range<uint16_t> range =
        byte_stream_.GetDataAt(position, runtime_call_stats(), &no_gc);
    buffer_start_ = range.start;
    buffer_end_ = range.end;
    buffer_cursor_ = buffer_start_;
    if (range.length() == 0) return false;

    DCHECK(!range.unaligned_start());
    DCHECK_LE(buffer_start_, buffer_end_);
    return true;
  }

  UnbufferedCharacterStream(const UnbufferedCharacterStream<ByteStream>& other)
      : byte_stream_(other.byte_stream_) {}

  ByteStream<uint16_t> byte_stream_;
};

// Provides a unbuffered utf-16 view on the bytes from the underlying
// ByteStream.
class RelocatingCharacterStream
    : public UnbufferedCharacterStream<OnHeapStream> {
 public:
  template <class... TArgs>
  RelocatingCharacterStream(Isolate* isolate, size_t pos, TArgs... args)
      : UnbufferedCharacterStream<OnHeapStream>(pos, args...),
        isolate_(isolate) {
    isolate->heap()->AddGCEpilogueCallback(UpdateBufferPointersCallback,
                                           v8::kGCTypeAll, this);
  }

 private:
  ~RelocatingCharacterStream() final {
    isolate_->heap()->RemoveGCEpilogueCallback(UpdateBufferPointersCallback,
                                               this);
  }

  static void UpdateBufferPointersCallback(v8::Isolate* v8_isolate,
                                           v8::GCType type,
                                           v8::GCCallbackFlags flags,
                                           void* stream) {
    reinterpret_cast<RelocatingCharacterStream*>(stream)
        ->UpdateBufferPointers();
  }

  void UpdateBufferPointers() {
    DisallowHeapAllocation no_gc;
    Range<uint16_t> range =
        byte_stream_.GetDataAt(buffer_pos_, runtime_call_stats(), &no_gc);
    if (range.start != buffer_start_) {
      buffer_cursor_ = (buffer_cursor_ - buffer_start_) + range.start;
      buffer_start_ = range.start;
      buffer_end_ = range.end;
    }
  }

  Isolate* isolate_;
};

// ----------------------------------------------------------------------------
// BufferedUtf16CharacterStreams
//
// A buffered character stream based on a random access character
// source (ReadBlock can be called with pos() pointing to any position,
// even positions before the current).
//
// TODO(verwaest): Remove together with Utf8 external streaming streams.
class BufferedUtf16CharacterStream : public Utf16CharacterStream {
 public:
  BufferedUtf16CharacterStream();

 protected:
  static const size_t kBufferSize = 512;

  bool ReadBlock() final;

  // FillBuffer should read up to kBufferSize characters at position and store
  // them into buffer_[0..]. It returns the number of characters stored.
  virtual size_t FillBuffer(size_t position) = 0;

  // Fixed sized buffer that this class reads from.
  // The base class' buffer_start_ should always point to buffer_.
  uc16 buffer_[kBufferSize];
};

BufferedUtf16CharacterStream::BufferedUtf16CharacterStream()
    : Utf16CharacterStream(buffer_, buffer_, buffer_, 0) {}

bool BufferedUtf16CharacterStream::ReadBlock() {
  DCHECK_EQ(buffer_start_, buffer_);

  size_t position = pos();
  buffer_pos_ = position;
  buffer_cursor_ = buffer_;
  buffer_end_ = buffer_ + FillBuffer(position);
  DCHECK_EQ(pos(), position);
  DCHECK_LE(buffer_end_, buffer_start_ + kBufferSize);
  return buffer_cursor_ < buffer_end_;
}

// ----------------------------------------------------------------------------
// Utf8ExternalStreamingStream - chunked streaming of Utf-8 data.
//
// This implementation is fairly complex, since data arrives in chunks which
// may 'cut' arbitrarily into utf-8 characters. Also, seeking to a given
// character position is tricky because the byte position cannot be derived
// from the character position.
//
// TODO(verwaest): Decode utf8 chunks into utf16 chunks on the blink side
// instead so we don't need to buffer.

class Utf8ExternalStreamingStream : public BufferedUtf16CharacterStream {
 public:
  Utf8ExternalStreamingStream(
      ScriptCompiler::ExternalSourceStream* source_stream)
      : current_({0, {0, 0, 0, unibrow::Utf8::State::kAccept}}),
        source_stream_(source_stream) {}
  ~Utf8ExternalStreamingStream() final {
    for (const Chunk& chunk : chunks_) delete[] chunk.data;
  }

  bool can_access_heap() const final { return false; }

  bool can_be_cloned() const final { return false; }

  std::unique_ptr<Utf16CharacterStream> Clone() const override {
    UNREACHABLE();
  }

 protected:
  size_t FillBuffer(size_t position) final;

 private:
  // A position within the data stream. It stores:
  // - The 'physical' position (# of bytes in the stream),
  // - the 'logical' position (# of ucs-2 characters, also within the stream),
  // - a possibly incomplete utf-8 char at the current 'physical' position.
  struct StreamPosition {
    size_t bytes;
    size_t chars;
    uint32_t incomplete_char;
    unibrow::Utf8::State state;
  };

  // Position contains a StreamPosition and the index of the chunk the position
  // points into. (The chunk_no could be derived from pos, but that'd be
  // an expensive search through all chunks.)
  struct Position {
    size_t chunk_no;
    StreamPosition pos;
  };

  // A chunk in the list of chunks, containing:
  // - The chunk data (data pointer and length), and
  // - the position at the first byte of the chunk.
  struct Chunk {
    const uint8_t* data;
    size_t length;
    StreamPosition start;
  };

  // Within the current chunk, skip forward from current_ towards position.
  bool SkipToPosition(size_t position);
  // Within the current chunk, fill the buffer_ (while it has capacity).
  void FillBufferFromCurrentChunk();
  // Fetch a new chunk (assuming current_ is at the end of the current data).
  bool FetchChunk();
  // Search through the chunks and set current_ to point to the given position.
  // (This call is potentially expensive.)
  void SearchPosition(size_t position);

  std::vector<Chunk> chunks_;
  Position current_;
  ScriptCompiler::ExternalSourceStream* source_stream_;
};

bool Utf8ExternalStreamingStream::SkipToPosition(size_t position) {
  DCHECK_LE(current_.pos.chars, position);  // We can only skip forward.

  // Already there? Then return immediately.
  if (current_.pos.chars == position) return true;

  const Chunk& chunk = chunks_[current_.chunk_no];
  DCHECK(current_.pos.bytes >= chunk.start.bytes);

  unibrow::Utf8::State state = chunk.start.state;
  uint32_t incomplete_char = chunk.start.incomplete_char;
  size_t it = current_.pos.bytes - chunk.start.bytes;
  const uint8_t* cursor = &chunk.data[it];
  const uint8_t* end = &chunk.data[chunk.length];

  size_t chars = current_.pos.chars;

  if (V8_UNLIKELY(current_.pos.bytes < 3 && chars == 0)) {
    while (cursor < end) {
      unibrow::uchar t =
          unibrow::Utf8::ValueOfIncremental(&cursor, &state, &incomplete_char);
      if (t == unibrow::Utf8::kIncomplete) continue;
      if (t != kUtf8Bom) {
        chars++;
        if (t > unibrow::Utf16::kMaxNonSurrogateCharCode) chars++;
      }
      break;
    }
  }

  while (cursor < end && chars < position) {
    unibrow::uchar t =
        unibrow::Utf8::ValueOfIncremental(&cursor, &state, &incomplete_char);
    if (t != unibrow::Utf8::kIncomplete) {
      chars++;
      if (t > unibrow::Utf16::kMaxNonSurrogateCharCode) chars++;
    }
  }

  current_.pos.bytes = chunk.start.bytes + (cursor - chunk.data);
  current_.pos.chars = chars;
  current_.pos.incomplete_char = incomplete_char;
  current_.pos.state = state;
  current_.chunk_no += (cursor == end);

  return current_.pos.chars == position;
}

void Utf8ExternalStreamingStream::FillBufferFromCurrentChunk() {
  DCHECK_LT(current_.chunk_no, chunks_.size());
  DCHECK_EQ(buffer_start_, buffer_cursor_);
  DCHECK_LT(buffer_end_ + 1, buffer_start_ + kBufferSize);

  const Chunk& chunk = chunks_[current_.chunk_no];

  // The buffer_ is writable, but buffer_*_ members are const. So we get a
  // non-const pointer into buffer that points to the same char as buffer_end_.
  uint16_t* output_cursor = buffer_ + (buffer_end_ - buffer_start_);
  DCHECK_EQ(output_cursor, buffer_end_);

  unibrow::Utf8::State state = current_.pos.state;
  uint32_t incomplete_char = current_.pos.incomplete_char;

  // If the current chunk is the last (empty) chunk we'll have to process
  // any left-over, partial characters.
  if (chunk.length == 0) {
    unibrow::uchar t = unibrow::Utf8::ValueOfIncrementalFinish(&state);
    if (t != unibrow::Utf8::kBufferEmpty) {
      DCHECK_EQ(t, unibrow::Utf8::kBadChar);
      *output_cursor = static_cast<uc16>(t);
      buffer_end_++;
      current_.pos.chars++;
      current_.pos.incomplete_char = 0;
      current_.pos.state = state;
    }
    return;
  }

  size_t it = current_.pos.bytes - chunk.start.bytes;
  const uint8_t* cursor = chunk.data + it;
  const uint8_t* end = chunk.data + chunk.length;

  // Deal with possible BOM.
  if (V8_UNLIKELY(current_.pos.bytes < 3 && current_.pos.chars == 0)) {
    while (cursor < end) {
      unibrow::uchar t =
          unibrow::Utf8::ValueOfIncremental(&cursor, &state, &incomplete_char);
      if (V8_LIKELY(t < kUtf8Bom)) {
        *(output_cursor++) = static_cast<uc16>(t);  // The most frequent case.
      } else if (t == unibrow::Utf8::kIncomplete) {
        continue;
      } else if (t == kUtf8Bom) {
        // BOM detected at beginning of the stream. Don't copy it.
      } else if (t <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
        *(output_cursor++) = static_cast<uc16>(t);
      } else {
        *(output_cursor++) = unibrow::Utf16::LeadSurrogate(t);
        *(output_cursor++) = unibrow::Utf16::TrailSurrogate(t);
      }
      break;
    }
  }

  const uint16_t* max_buffer_end = buffer_start_ + kBufferSize;
  while (cursor < end && output_cursor + 1 < max_buffer_end) {
    unibrow::uchar t =
        unibrow::Utf8::ValueOfIncremental(&cursor, &state, &incomplete_char);
    if (V8_LIKELY(t <= unibrow::Utf16::kMaxNonSurrogateCharCode)) {
      *(output_cursor++) = static_cast<uc16>(t);  // The most frequent case.
    } else if (t == unibrow::Utf8::kIncomplete) {
      continue;
    } else {
      *(output_cursor++) = unibrow::Utf16::LeadSurrogate(t);
      *(output_cursor++) = unibrow::Utf16::TrailSurrogate(t);
    }
    // Fast path for ascii sequences.
    size_t remaining = end - cursor;
    size_t max_buffer = max_buffer_end - output_cursor;
    int max_length = static_cast<int>(Min(remaining, max_buffer));
    DCHECK_EQ(state, unibrow::Utf8::State::kAccept);
    int ascii_length = NonAsciiStart(cursor, max_length);
    CopyChars(output_cursor, cursor, ascii_length);
    cursor += ascii_length;
    output_cursor += ascii_length;
  }

  current_.pos.bytes = chunk.start.bytes + (cursor - chunk.data);
  current_.pos.chars += (output_cursor - buffer_end_);
  current_.pos.incomplete_char = incomplete_char;
  current_.pos.state = state;
  current_.chunk_no += (cursor == end);

  buffer_end_ = output_cursor;
}

bool Utf8ExternalStreamingStream::FetchChunk() {
  RuntimeCallTimerScope scope(runtime_call_stats(),
                              RuntimeCallCounterId::kGetMoreDataCallback);
  DCHECK_EQ(current_.chunk_no, chunks_.size());
  DCHECK(chunks_.empty() || chunks_.back().length != 0);

  const uint8_t* chunk = nullptr;
  size_t length = source_stream_->GetMoreData(&chunk);
  chunks_.push_back({chunk, length, current_.pos});
  return length > 0;
}

void Utf8ExternalStreamingStream::SearchPosition(size_t position) {
  // If current_ already points to the right position, we're done.
  //
  // This is expected to be the common case, since we typically call
  // FillBuffer right after the current buffer.
  if (current_.pos.chars == position) return;

  // No chunks. Fetch at least one, so we can assume !chunks_.empty() below.
  if (chunks_.empty()) {
    DCHECK_EQ(current_.chunk_no, 0u);
    DCHECK_EQ(current_.pos.bytes, 0u);
    DCHECK_EQ(current_.pos.chars, 0u);
    FetchChunk();
  }

  // Search for the last chunk whose start position is less or equal to
  // position.
  size_t chunk_no = chunks_.size() - 1;
  while (chunk_no > 0 && chunks_[chunk_no].start.chars > position) {
    chunk_no--;
  }

  // Did we find the terminating (zero-length) chunk? Then we're seeking
  // behind the end of the data, and position does not exist.
  // Set current_ to point to the terminating chunk.
  if (chunks_[chunk_no].length == 0) {
    current_ = {chunk_no, chunks_[chunk_no].start};
    return;
  }

  // Did we find the non-last chunk? Then our position must be within chunk_no.
  if (chunk_no + 1 < chunks_.size()) {
    // Fancy-pants optimization for ASCII chunks within a utf-8 stream.
    // (Many web sites declare utf-8 encoding, but use only (or almost only) the
    //  ASCII subset for their JavaScript sources. We can exploit this, by
    //  checking whether the # bytes in a chunk are equal to the # chars, and if
    //  so avoid the expensive SkipToPosition.)
    bool ascii_only_chunk =
        chunks_[chunk_no].start.incomplete_char == 0 &&
        (chunks_[chunk_no + 1].start.bytes - chunks_[chunk_no].start.bytes) ==
            (chunks_[chunk_no + 1].start.chars - chunks_[chunk_no].start.chars);
    if (ascii_only_chunk) {
      size_t skip = position - chunks_[chunk_no].start.chars;
      current_ = {chunk_no,
                  {chunks_[chunk_no].start.bytes + skip,
                   chunks_[chunk_no].start.chars + skip, 0,
                   unibrow::Utf8::State::kAccept}};
    } else {
      current_ = {chunk_no, chunks_[chunk_no].start};
      SkipToPosition(position);
    }

    // Since position was within the chunk, SkipToPosition should have found
    // something.
    DCHECK_EQ(position, current_.pos.chars);
    return;
  }

  // What's left: We're in the last, non-terminating chunk. Our position
  // may be in the chunk, but it may also be in 'future' chunks, which we'll
  // have to obtain.
  DCHECK_EQ(chunk_no, chunks_.size() - 1);
  current_ = {chunk_no, chunks_[chunk_no].start};
  bool have_more_data = true;
  bool found = SkipToPosition(position);
  while (have_more_data && !found) {
    DCHECK_EQ(current_.chunk_no, chunks_.size());
    have_more_data = FetchChunk();
    found = have_more_data && SkipToPosition(position);
  }

  // We'll return with a postion != the desired position only if we're out
  // of data. In that case, we'll point to the terminating chunk.
  DCHECK_EQ(found, current_.pos.chars == position);
  DCHECK_EQ(have_more_data, chunks_.back().length != 0);
  DCHECK_IMPLIES(!found, !have_more_data);
  DCHECK_IMPLIES(!found, current_.chunk_no == chunks_.size() - 1);
}

size_t Utf8ExternalStreamingStream::FillBuffer(size_t position) {
  buffer_cursor_ = buffer_;
  buffer_end_ = buffer_;

  SearchPosition(position);
  bool out_of_data = current_.chunk_no != chunks_.size() &&
                     chunks_[current_.chunk_no].length == 0 &&
                     current_.pos.incomplete_char == 0;

  if (out_of_data) return 0;

  // Fill the buffer, until we have at least one char (or are out of data).
  // (The embedder might give us 1-byte blocks within a utf-8 char, so we
  //  can't guarantee progress with one chunk. Thus we iterate.)
  while (!out_of_data && buffer_cursor_ == buffer_end_) {
    // At end of current data, but there might be more? Then fetch it.
    if (current_.chunk_no == chunks_.size()) {
      out_of_data = !FetchChunk();
    }
    FillBufferFromCurrentChunk();
  }

  DCHECK_EQ(current_.pos.chars - position,
            static_cast<size_t>(buffer_end_ - buffer_cursor_));
  return buffer_end_ - buffer_cursor_;
}

// ----------------------------------------------------------------------------
// ScannerStream: Create stream instances.

Utf16CharacterStream* ScannerStream::For(Isolate* isolate,
                                         Handle<String> data) {
  return ScannerStream::For(isolate, data, 0, data->length());
}

Utf16CharacterStream* ScannerStream::For(Isolate* isolate, Handle<String> data,
                                         int start_pos, int end_pos) {
  DCHECK_GE(start_pos, 0);
  DCHECK_LE(start_pos, end_pos);
  DCHECK_LE(end_pos, data->length());
  size_t start_offset = 0;
  if (data->IsSlicedString()) {
    SlicedString string = SlicedString::cast(*data);
    start_offset = string.offset();
    String parent = string.parent();
    if (parent.IsThinString()) parent = ThinString::cast(parent).actual();
    data = handle(parent, isolate);
  } else {
    data = String::Flatten(isolate, data);
  }
  if (data->IsExternalOneByteString()) {
    return new BufferedCharacterStream<ExternalStringStream>(
        static_cast<size_t>(start_pos), ExternalOneByteString::cast(*data),
        start_offset, static_cast<size_t>(end_pos));
  } else if (data->IsExternalTwoByteString()) {
    return new UnbufferedCharacterStream<ExternalStringStream>(
        static_cast<size_t>(start_pos), ExternalTwoByteString::cast(*data),
        start_offset, static_cast<size_t>(end_pos));
  } else if (data->IsSeqOneByteString()) {
    return new BufferedCharacterStream<OnHeapStream>(
        static_cast<size_t>(start_pos), Handle<SeqOneByteString>::cast(data),
        start_offset, static_cast<size_t>(end_pos));
  } else if (data->IsSeqTwoByteString()) {
    return new RelocatingCharacterStream(
        isolate, static_cast<size_t>(start_pos),
        Handle<SeqTwoByteString>::cast(data), start_offset,
        static_cast<size_t>(end_pos));
  } else {
    UNREACHABLE();
  }
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const char* data) {
  return ScannerStream::ForTesting(data, strlen(data));
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const char* data, size_t length) {
  return std::unique_ptr<Utf16CharacterStream>(
      new BufferedCharacterStream<TestingStream>(
          static_cast<size_t>(0), reinterpret_cast<const uint8_t*>(data),
          static_cast<size_t>(length)));
}

Utf16CharacterStream* ScannerStream::For(
    ScriptCompiler::ExternalSourceStream* source_stream,
    v8::ScriptCompiler::StreamedSource::Encoding encoding) {
  switch (encoding) {
    case v8::ScriptCompiler::StreamedSource::TWO_BYTE:
      return new UnbufferedCharacterStream<ChunkedStream>(
          static_cast<size_t>(0), source_stream);
    case v8::ScriptCompiler::StreamedSource::ONE_BYTE:
      return new BufferedCharacterStream<ChunkedStream>(static_cast<size_t>(0),
                                                        source_stream);
    case v8::ScriptCompiler::StreamedSource::UTF8:
      return new Utf8ExternalStreamingStream(source_stream);
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
