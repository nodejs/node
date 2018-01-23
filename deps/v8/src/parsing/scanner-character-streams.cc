// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/scanner-character-streams.h"

#include "include/v8.h"
#include "src/counters.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/objects-inl.h"
#include "src/parsing/scanner.h"
#include "src/unicode-inl.h"

namespace v8 {
namespace internal {

namespace {
const unibrow::uchar kUtf8Bom = 0xFEFF;
}  // namespace

// ----------------------------------------------------------------------------
// BufferedUtf16CharacterStreams
//
// A buffered character stream based on a random access character
// source (ReadBlock can be called with pos() pointing to any position,
// even positions before the current).
class BufferedUtf16CharacterStream : public Utf16CharacterStream {
 public:
  BufferedUtf16CharacterStream();

 protected:
  static const size_t kBufferSize = 512;

  bool ReadBlock() override;

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
// GenericStringUtf16CharacterStream.
//
// A stream w/ a data source being a (flattened) Handle<String>.

class GenericStringUtf16CharacterStream : public BufferedUtf16CharacterStream {
 public:
  GenericStringUtf16CharacterStream(Handle<String> data, size_t start_position,
                                    size_t end_position);

  bool can_access_heap() override { return true; }

 protected:
  size_t FillBuffer(size_t position) override;

  Handle<String> string_;
  size_t length_;
};

GenericStringUtf16CharacterStream::GenericStringUtf16CharacterStream(
    Handle<String> data, size_t start_position, size_t end_position)
    : string_(data), length_(end_position) {
  DCHECK_GE(end_position, start_position);
  DCHECK_GE(static_cast<size_t>(string_->length()),
            end_position - start_position);
  buffer_pos_ = start_position;
}

size_t GenericStringUtf16CharacterStream::FillBuffer(size_t from_pos) {
  if (from_pos >= length_) return 0;

  size_t length = i::Min(kBufferSize, length_ - from_pos);
  String::WriteToFlat<uc16>(*string_, buffer_, static_cast<int>(from_pos),
                            static_cast<int>(from_pos + length));
  return length;
}

// ----------------------------------------------------------------------------
// ExternalTwoByteStringUtf16CharacterStream.
//
// A stream whose data source is a Handle<ExternalTwoByteString>. It avoids
// all data copying.

class ExternalTwoByteStringUtf16CharacterStream : public Utf16CharacterStream {
 public:
  ExternalTwoByteStringUtf16CharacterStream(Handle<ExternalTwoByteString> data,
                                            size_t start_position,
                                            size_t end_position);

  bool can_access_heap() override { return false; }

 private:
  bool ReadBlock() override;

  const uc16* raw_data_;  // Pointer to the actual array of characters.
  size_t start_pos_;
  size_t end_pos_;
};

ExternalTwoByteStringUtf16CharacterStream::
    ExternalTwoByteStringUtf16CharacterStream(
        Handle<ExternalTwoByteString> data, size_t start_position,
        size_t end_position)
    : raw_data_(data->GetTwoByteData(static_cast<int>(start_position))),
      start_pos_(start_position),
      end_pos_(end_position) {
  buffer_start_ = raw_data_;
  buffer_cursor_ = raw_data_;
  buffer_end_ = raw_data_ + (end_pos_ - start_pos_);
  buffer_pos_ = start_pos_;
}

bool ExternalTwoByteStringUtf16CharacterStream::ReadBlock() {
  size_t position = pos();
  bool have_data = start_pos_ <= position && position < end_pos_;
  if (have_data) {
    buffer_pos_ = start_pos_;
    buffer_cursor_ = raw_data_ + (position - start_pos_),
    buffer_end_ = raw_data_ + (end_pos_ - start_pos_);
  } else {
    buffer_pos_ = position;
    buffer_cursor_ = raw_data_;
    buffer_end_ = raw_data_;
  }
  return have_data;
}

// ----------------------------------------------------------------------------
// ExternalOneByteStringUtf16CharacterStream
//
// A stream whose data source is a Handle<ExternalOneByteString>.

class ExternalOneByteStringUtf16CharacterStream
    : public BufferedUtf16CharacterStream {
 public:
  ExternalOneByteStringUtf16CharacterStream(Handle<ExternalOneByteString> data,
                                            size_t start_position,
                                            size_t end_position);

  // For testing:
  ExternalOneByteStringUtf16CharacterStream(const char* data, size_t length);

  bool can_access_heap() override { return false; }

 protected:
  size_t FillBuffer(size_t position) override;

  const uint8_t* raw_data_;  // Pointer to the actual array of characters.
  size_t length_;
};

ExternalOneByteStringUtf16CharacterStream::
    ExternalOneByteStringUtf16CharacterStream(
        Handle<ExternalOneByteString> data, size_t start_position,
        size_t end_position)
    : raw_data_(data->GetChars()), length_(end_position) {
  DCHECK(end_position >= start_position);
  buffer_pos_ = start_position;
}

ExternalOneByteStringUtf16CharacterStream::
    ExternalOneByteStringUtf16CharacterStream(const char* data, size_t length)
    : raw_data_(reinterpret_cast<const uint8_t*>(data)), length_(length) {}

size_t ExternalOneByteStringUtf16CharacterStream::FillBuffer(size_t from_pos) {
  if (from_pos >= length_) return 0;

  size_t length = Min(kBufferSize, length_ - from_pos);
  i::CopyCharsUnsigned(buffer_, raw_data_ + from_pos, length);
  return length;
}

// ----------------------------------------------------------------------------
// Utf8ExternalStreamingStream - chunked streaming of Utf-8 data.
//
// This implementation is fairly complex, since data arrives in chunks which
// may 'cut' arbitrarily into utf-8 characters. Also, seeking to a given
// character position is tricky because the byte position cannot be dericed
// from the character position.

class Utf8ExternalStreamingStream : public BufferedUtf16CharacterStream {
 public:
  Utf8ExternalStreamingStream(
      ScriptCompiler::ExternalSourceStream* source_stream,
      RuntimeCallStats* stats)
      : current_({0, {0, 0, 0, unibrow::Utf8::State::kAccept}}),
        source_stream_(source_stream),
        stats_(stats) {}
  ~Utf8ExternalStreamingStream() override {
    for (size_t i = 0; i < chunks_.size(); i++) delete[] chunks_[i].data;
  }

  bool can_access_heap() override { return false; }

 protected:
  size_t FillBuffer(size_t position) override;

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
  RuntimeCallStats* stats_;
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
  size_t chars = chunk.start.chars;
  while (it < chunk.length && chars < position) {
    unibrow::uchar t = unibrow::Utf8::ValueOfIncremental(
        chunk.data[it], &it, &state, &incomplete_char);
    if (t == kUtf8Bom && current_.pos.chars == 0) {
      // BOM detected at beginning of the stream. Don't copy it.
    } else if (t != unibrow::Utf8::kIncomplete) {
      chars++;
      if (t > unibrow::Utf16::kMaxNonSurrogateCharCode) chars++;
    }
  }

  current_.pos.bytes += it;
  current_.pos.chars = chars;
  current_.pos.incomplete_char = incomplete_char;
  current_.pos.state = state;
  current_.chunk_no += (it == chunk.length);

  return current_.pos.chars == position;
}

void Utf8ExternalStreamingStream::FillBufferFromCurrentChunk() {
  DCHECK_LT(current_.chunk_no, chunks_.size());
  DCHECK_EQ(buffer_start_, buffer_cursor_);
  DCHECK_LT(buffer_end_ + 1, buffer_start_ + kBufferSize);

  const Chunk& chunk = chunks_[current_.chunk_no];

  // The buffer_ is writable, but buffer_*_ members are const. So we get a
  // non-const pointer into buffer that points to the same char as buffer_end_.
  uint16_t* cursor = buffer_ + (buffer_end_ - buffer_start_);
  DCHECK_EQ(cursor, buffer_end_);

  unibrow::Utf8::State state = current_.pos.state;
  uint32_t incomplete_char = current_.pos.incomplete_char;

  // If the current chunk is the last (empty) chunk we'll have to process
  // any left-over, partial characters.
  if (chunk.length == 0) {
    unibrow::uchar t = unibrow::Utf8::ValueOfIncrementalFinish(&state);
    if (t != unibrow::Utf8::kBufferEmpty) {
      DCHECK_EQ(t, unibrow::Utf8::kBadChar);
      *cursor = static_cast<uc16>(t);
      buffer_end_++;
      current_.pos.chars++;
      current_.pos.incomplete_char = 0;
      current_.pos.state = state;
    }
    return;
  }

  size_t it = current_.pos.bytes - chunk.start.bytes;
  while (it < chunk.length && cursor + 1 < buffer_start_ + kBufferSize) {
    unibrow::uchar t = unibrow::Utf8::ValueOfIncremental(
        chunk.data[it], &it, &state, &incomplete_char);
    if (V8_LIKELY(t < kUtf8Bom)) {
      *(cursor++) = static_cast<uc16>(t);  // The by most frequent case.
    } else if (t == unibrow::Utf8::kIncomplete) {
      continue;
    } else if (t == kUtf8Bom && current_.pos.bytes + it == 3) {
      // BOM detected at beginning of the stream. Don't copy it.
    } else if (t <= unibrow::Utf16::kMaxNonSurrogateCharCode) {
      *(cursor++) = static_cast<uc16>(t);
    } else {
      *(cursor++) = unibrow::Utf16::LeadSurrogate(t);
      *(cursor++) = unibrow::Utf16::TrailSurrogate(t);
    }
  }

  current_.pos.bytes = chunk.start.bytes + it;
  current_.pos.chars += (cursor - buffer_end_);
  current_.pos.incomplete_char = incomplete_char;
  current_.pos.state = state;
  current_.chunk_no += (it == chunk.length);

  buffer_end_ = cursor;
}

bool Utf8ExternalStreamingStream::FetchChunk() {
  RuntimeCallTimerScope scope(stats_,
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
// Chunks - helper for One- + TwoByteExternalStreamingStream
namespace {

struct Chunk {
  const uint8_t* data;
  size_t byte_length;
  size_t byte_pos;
};

typedef std::vector<struct Chunk> Chunks;

void DeleteChunks(Chunks& chunks) {
  for (size_t i = 0; i < chunks.size(); i++) delete[] chunks[i].data;
}

// Return the chunk index for the chunk containing position.
// If position is behind the end of the stream, the index of the last,
// zero-length chunk is returned.
size_t FindChunk(Chunks& chunks, ScriptCompiler::ExternalSourceStream* source,
                 size_t position, RuntimeCallStats* stats) {
  size_t end_pos =
      chunks.empty() ? 0 : (chunks.back().byte_pos + chunks.back().byte_length);

  // Get more data if needed. We usually won't enter the loop body.
  bool out_of_data = !chunks.empty() && chunks.back().byte_length == 0;
  {
    RuntimeCallTimerScope scope(stats,
                                RuntimeCallCounterId::kGetMoreDataCallback);
    while (!out_of_data && end_pos <= position + 1) {
      const uint8_t* chunk = nullptr;
      size_t len = source->GetMoreData(&chunk);

      chunks.push_back({chunk, len, end_pos});
      end_pos += len;
      out_of_data = (len == 0);
    }
  }

  // Here, we should always have at least one chunk, and we either have the
  // chunk we were looking for, or we're out of data. Also, out_of_data and
  // end_pos are current (and designate whether we have exhausted the stream,
  // and the length of data received so far, respectively).
  DCHECK(!chunks.empty());
  DCHECK_EQ(end_pos, chunks.back().byte_pos + chunks.back().byte_length);
  DCHECK_EQ(out_of_data, chunks.back().byte_length == 0);
  DCHECK(position < end_pos || out_of_data);

  // Edge case: position is behind the end of stream: Return the last (length 0)
  // chunk to indicate the end of the stream.
  if (position >= end_pos) {
    DCHECK(out_of_data);
    return chunks.size() - 1;
  }

  // We almost always 'stream', meaning we want data from the last chunk, so
  // let's look at chunks back-to-front.
  size_t chunk_no = chunks.size() - 1;
  while (chunks[chunk_no].byte_pos > position) {
    DCHECK_NE(chunk_no, 0u);
    chunk_no--;
  }
  DCHECK_LE(chunks[chunk_no].byte_pos, position);
  DCHECK_LT(position, chunks[chunk_no].byte_pos + chunks[chunk_no].byte_length);
  return chunk_no;
}

}  // anonymous namespace

// ----------------------------------------------------------------------------
// OneByteExternalStreamingStream
//
// A stream of latin-1 encoded, chunked data.

class OneByteExternalStreamingStream : public BufferedUtf16CharacterStream {
 public:
  explicit OneByteExternalStreamingStream(
      ScriptCompiler::ExternalSourceStream* source, RuntimeCallStats* stats)
      : source_(source), stats_(stats) {}
  ~OneByteExternalStreamingStream() override { DeleteChunks(chunks_); }

  bool can_access_heap() override { return false; }

 protected:
  size_t FillBuffer(size_t position) override;

 private:
  Chunks chunks_;
  ScriptCompiler::ExternalSourceStream* source_;
  RuntimeCallStats* stats_;
};

size_t OneByteExternalStreamingStream::FillBuffer(size_t position) {
  const Chunk& chunk = chunks_[FindChunk(chunks_, source_, position, stats_)];
  if (chunk.byte_length == 0) return 0;

  size_t start_pos = position - chunk.byte_pos;
  size_t len = i::Min(kBufferSize, chunk.byte_length - start_pos);
  i::CopyCharsUnsigned(buffer_, chunk.data + start_pos, len);
  return len;
}

#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64)
// ----------------------------------------------------------------------------
// TwoByteExternalStreamingStream
//
// A stream of ucs-2 data, delivered in chunks. Chunks may be 'cut' into the
// middle of characters (or even contain only one byte), which adds a bit
// of complexity. This stream avoid all data copying, except for characters
// that cross chunk boundaries.

class TwoByteExternalStreamingStream : public Utf16CharacterStream {
 public:
  explicit TwoByteExternalStreamingStream(
      ScriptCompiler::ExternalSourceStream* source, RuntimeCallStats* stats);
  ~TwoByteExternalStreamingStream() override;

  bool can_access_heap() override { return false; }

 protected:
  bool ReadBlock() override;

  Chunks chunks_;
  ScriptCompiler::ExternalSourceStream* source_;
  RuntimeCallStats* stats_;
  uc16 one_char_buffer_;
};

TwoByteExternalStreamingStream::TwoByteExternalStreamingStream(
    ScriptCompiler::ExternalSourceStream* source, RuntimeCallStats* stats)
    : Utf16CharacterStream(&one_char_buffer_, &one_char_buffer_,
                           &one_char_buffer_, 0),
      source_(source),
      stats_(stats),
      one_char_buffer_(0) {}

TwoByteExternalStreamingStream::~TwoByteExternalStreamingStream() {
  DeleteChunks(chunks_);
}

bool TwoByteExternalStreamingStream::ReadBlock() {
  size_t position = pos();

  // We'll search for the 2nd byte of our character, to make sure we
  // have enough data for at least one character.
  size_t chunk_no = FindChunk(chunks_, source_, 2 * position + 1, stats_);

  // Out of data? Return 0.
  if (chunks_[chunk_no].byte_length == 0) {
    buffer_pos_ = position;
    buffer_cursor_ = buffer_start_;
    buffer_end_ = buffer_start_;
    return false;
  }

  Chunk& current = chunks_[chunk_no];

  // Annoying edge case: Chunks may not be 2-byte aligned, meaning that a
  // character may be split between the previous and the current chunk.
  // If we find such a lonely byte at the beginning of the chunk, we'll use
  // one_char_buffer_ to hold the full character.
  bool lonely_byte = (chunks_[chunk_no].byte_pos == (2 * position + 1));
  if (lonely_byte) {
    DCHECK_NE(chunk_no, 0u);
    Chunk& previous_chunk = chunks_[chunk_no - 1];
#ifdef V8_TARGET_BIG_ENDIAN
    uc16 character = current.data[0] |
                     previous_chunk.data[previous_chunk.byte_length - 1] << 8;
#else
    uc16 character = previous_chunk.data[previous_chunk.byte_length - 1] |
                     current.data[0] << 8;
#endif

    one_char_buffer_ = character;
    buffer_pos_ = position;
    buffer_start_ = &one_char_buffer_;
    buffer_cursor_ = &one_char_buffer_;
    buffer_end_ = &one_char_buffer_ + 1;
    return true;
  }

  // Common case: character is in current chunk.
  DCHECK_LE(current.byte_pos, 2 * position);
  DCHECK_LT(2 * position + 1, current.byte_pos + current.byte_length);

  // Determine # of full ucs-2 chars in stream, and whether we started on an odd
  // byte boundary.
  bool odd_start = (current.byte_pos % 2) == 1;
  size_t number_chars = (current.byte_length - odd_start) / 2;

  // Point the buffer_*_ members into the current chunk and set buffer_cursor_
  // to point to position. Be careful when converting the byte positions (in
  // Chunk) to the ucs-2 character positions (in buffer_*_ members).
  buffer_start_ = reinterpret_cast<const uint16_t*>(current.data + odd_start);
  buffer_end_ = buffer_start_ + number_chars;
  buffer_pos_ = (current.byte_pos + odd_start) / 2;
  buffer_cursor_ = buffer_start_ + (position - buffer_pos_);
  DCHECK_EQ(position, pos());
  return true;
}

#else

// ----------------------------------------------------------------------------
// TwoByteExternalBufferedStream
//
// This class is made specifically to address unaligned access to 16-bit data
// in MIPS and ARM architectures. It replaces class
// TwoByteExternalStreamingStream which in some cases does have unaligned
// accesse to 16-bit data

class TwoByteExternalBufferedStream : public Utf16CharacterStream {
 public:
  explicit TwoByteExternalBufferedStream(
      ScriptCompiler::ExternalSourceStream* source, RuntimeCallStats* stats);
  ~TwoByteExternalBufferedStream();

  bool can_access_heap() override { return false; }

 protected:
  static const size_t kBufferSize = 512;

  bool ReadBlock() override;

  // FillBuffer should read up to kBufferSize characters at position and store
  // them into buffer_[0..]. It returns the number of characters stored.
  size_t FillBuffer(size_t position, size_t chunk_no);

  // Fixed sized buffer that this class reads from.
  // The base class' buffer_start_ should always point to buffer_.
  uc16 buffer_[kBufferSize];

  Chunks chunks_;
  ScriptCompiler::ExternalSourceStream* source_;
  RuntimeCallStats* stats_;
};

TwoByteExternalBufferedStream::TwoByteExternalBufferedStream(
    ScriptCompiler::ExternalSourceStream* source, RuntimeCallStats* stats)
    : Utf16CharacterStream(buffer_, buffer_, buffer_, 0),
      source_(source),
      stats_(stats) {}

TwoByteExternalBufferedStream::~TwoByteExternalBufferedStream() {
  DeleteChunks(chunks_);
}

bool TwoByteExternalBufferedStream::ReadBlock() {
  size_t position = pos();
  // Find chunk in which the position belongs
  size_t chunk_no = FindChunk(chunks_, source_, 2 * position + 1, stats_);

  // Out of data? Return 0.
  if (chunks_[chunk_no].byte_length == 0) {
    buffer_pos_ = position;
    buffer_cursor_ = buffer_start_;
    buffer_end_ = buffer_start_;
    return false;
  }

  Chunk& current = chunks_[chunk_no];

  bool odd_start = current.byte_pos % 2;
  // Common case: character is in current chunk.
  DCHECK_LE(current.byte_pos, 2 * position + odd_start);
  DCHECK_LT(2 * position + 1, current.byte_pos + current.byte_length);

  // If character starts on odd address copy text in buffer so there is always
  // aligned access to characters. This is important on MIPS and ARM
  // architectures. Otherwise read characters from memory directly.
  if (!odd_start) {
    buffer_start_ = reinterpret_cast<const uint16_t*>(current.data);
    size_t number_chars = current.byte_length / 2;
    buffer_end_ = buffer_start_ + number_chars;
    buffer_pos_ = current.byte_pos / 2;
    buffer_cursor_ = buffer_start_ + (position - buffer_pos_);
    DCHECK_EQ(position, pos());
    return true;
  } else {
    buffer_start_ = buffer_;
    buffer_pos_ = position;
    buffer_cursor_ = buffer_;
    buffer_end_ = buffer_ + FillBuffer(position, chunk_no);
    DCHECK_EQ(pos(), position);
    DCHECK_LE(buffer_end_, buffer_start_ + kBufferSize);
    return buffer_cursor_ < buffer_end_;
  }
}

size_t TwoByteExternalBufferedStream::FillBuffer(size_t position,
                                                 size_t chunk_no) {
  DCHECK_EQ(chunks_[chunk_no].byte_pos % 2, 1u);
  bool odd_start = true;
  // Align buffer_pos_ to the size of the buffer.
  {
    size_t new_pos = position / kBufferSize * kBufferSize;
    if (new_pos != position) {
      chunk_no = FindChunk(chunks_, source_, 2 * new_pos + 1, stats_);
      buffer_pos_ = new_pos;
      buffer_cursor_ = buffer_start_ + (position - buffer_pos_);
      position = new_pos;
      odd_start = chunks_[chunk_no].byte_pos % 2;
    }
  }

  Chunk* current = &chunks_[chunk_no];

  // Annoying edge case: Chunks may not be 2-byte aligned, meaning that a
  // character may be split between the previous and the current chunk.
  // If we find such a lonely byte at the beginning of the chunk, we'll copy
  // it to the first byte in buffer_.
  size_t totalLength = 0;
  bool lonely_byte = (current->byte_pos == (2 * position + 1));
  if (lonely_byte) {
    DCHECK_NE(chunk_no, 0u);
    Chunk& previous_chunk = chunks_[chunk_no - 1];
    *reinterpret_cast<uint8_t*>(buffer_) =
        previous_chunk.data[previous_chunk.byte_length - 1];
    totalLength++;
  }

  // Common case: character is in current chunk.
  DCHECK_LE(current->byte_pos, 2 * position + odd_start);
  DCHECK_LT(2 * position + 1, current->byte_pos + current->byte_length);

  // Copy characters from current chunk starting from chunk_pos to the end of
  // buffer or chunk.
  size_t chunk_pos = position - current->byte_pos / 2;
  size_t start_offset = odd_start && chunk_pos != 0;
  size_t bytes_to_move =
      i::Min(2 * kBufferSize - lonely_byte,
             current->byte_length - 2 * chunk_pos + start_offset);
  i::MemMove(reinterpret_cast<uint8_t*>(buffer_) + lonely_byte,
             current->data + 2 * chunk_pos - start_offset, bytes_to_move);

  // Fill up the rest of the buffer if there is space and data left.
  totalLength += bytes_to_move;
  position = (current->byte_pos + current->byte_length) / 2;
  if (position - buffer_pos_ < kBufferSize) {
    chunk_no = FindChunk(chunks_, source_, 2 * position + 1, stats_);
    current = &chunks_[chunk_no];
    odd_start = current->byte_pos % 2;
    bytes_to_move = i::Min(2 * kBufferSize - totalLength, current->byte_length);
    while (bytes_to_move) {
      // Common case: character is in current chunk.
      DCHECK_LE(current->byte_pos, 2 * position + odd_start);
      DCHECK_LT(2 * position + 1, current->byte_pos + current->byte_length);

      i::MemMove(reinterpret_cast<uint8_t*>(buffer_) + totalLength,
                 current->data, bytes_to_move);
      totalLength += bytes_to_move;
      position = (current->byte_pos + current->byte_length) / 2;
      chunk_no = FindChunk(chunks_, source_, 2 * position + 1, stats_);
      current = &chunks_[chunk_no];
      odd_start = current->byte_pos % 2;
      bytes_to_move =
          i::Min(2 * kBufferSize - totalLength, current->byte_length);
    }
  }
  return totalLength / 2;
}
#endif

// ----------------------------------------------------------------------------
// ScannerStream: Create stream instances.

Utf16CharacterStream* ScannerStream::For(Handle<String> data) {
  return ScannerStream::For(data, 0, data->length());
}

Utf16CharacterStream* ScannerStream::For(Handle<String> data, int start_pos,
                                         int end_pos) {
  DCHECK_GE(start_pos, 0);
  DCHECK_LE(start_pos, end_pos);
  DCHECK_LE(end_pos, data->length());
  if (data->IsExternalOneByteString()) {
    return new ExternalOneByteStringUtf16CharacterStream(
        Handle<ExternalOneByteString>::cast(data),
        static_cast<size_t>(start_pos), static_cast<size_t>(end_pos));
  } else if (data->IsExternalTwoByteString()) {
    return new ExternalTwoByteStringUtf16CharacterStream(
        Handle<ExternalTwoByteString>::cast(data),
        static_cast<size_t>(start_pos), static_cast<size_t>(end_pos));
  } else {
    // TODO(vogelheim): Maybe call data.Flatten() first?
    return new GenericStringUtf16CharacterStream(
        data, static_cast<size_t>(start_pos), static_cast<size_t>(end_pos));
  }
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const char* data) {
  return ScannerStream::ForTesting(data, strlen(data));
}

std::unique_ptr<Utf16CharacterStream> ScannerStream::ForTesting(
    const char* data, size_t length) {
  return std::unique_ptr<Utf16CharacterStream>(
      new ExternalOneByteStringUtf16CharacterStream(data, length));
}

Utf16CharacterStream* ScannerStream::For(
    ScriptCompiler::ExternalSourceStream* source_stream,
    v8::ScriptCompiler::StreamedSource::Encoding encoding,
    RuntimeCallStats* stats) {
  switch (encoding) {
    case v8::ScriptCompiler::StreamedSource::TWO_BYTE:
#if !(V8_TARGET_ARCH_MIPS || V8_TARGET_ARCH_MIPS64)
      return new TwoByteExternalStreamingStream(source_stream, stats);
#else
      return new TwoByteExternalBufferedStream(source_stream, stats);
#endif
    case v8::ScriptCompiler::StreamedSource::ONE_BYTE:
      return new OneByteExternalStreamingStream(source_stream, stats);
    case v8::ScriptCompiler::StreamedSource::UTF8:
      return new Utf8ExternalStreamingStream(source_stream, stats);
  }
  UNREACHABLE();
  return nullptr;
}

}  // namespace internal
}  // namespace v8
