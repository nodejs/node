// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/scanner-character-streams.h"

#include "include/v8.h"
#include "src/handles.h"
#include "src/unicode-inl.h"

namespace v8 {
namespace internal {

namespace {

unsigned CopyCharsHelper(uint16_t* dest, unsigned length, const uint8_t* src,
                         unsigned* src_pos, unsigned src_length,
                         ScriptCompiler::StreamedSource::Encoding encoding) {
  if (encoding == ScriptCompiler::StreamedSource::UTF8) {
    return v8::internal::Utf8ToUtf16CharacterStream::CopyChars(
        dest, length, src, src_pos, src_length);
  }

  unsigned to_fill = length;
  if (to_fill > src_length - *src_pos) to_fill = src_length - *src_pos;

  if (encoding == ScriptCompiler::StreamedSource::ONE_BYTE) {
    v8::internal::CopyChars<uint8_t, uint16_t>(dest, src + *src_pos, to_fill);
  } else {
    DCHECK(encoding == ScriptCompiler::StreamedSource::TWO_BYTE);
    v8::internal::CopyChars<uint16_t, uint16_t>(
        dest, reinterpret_cast<const uint16_t*>(src + *src_pos), to_fill);
  }
  *src_pos += to_fill;
  return to_fill;
}

}  // namespace


// ----------------------------------------------------------------------------
// BufferedUtf16CharacterStreams

BufferedUtf16CharacterStream::BufferedUtf16CharacterStream()
    : Utf16CharacterStream(),
      pushback_limit_(NULL) {
  // Initialize buffer as being empty. First read will fill the buffer.
  buffer_cursor_ = buffer_;
  buffer_end_ = buffer_;
}


BufferedUtf16CharacterStream::~BufferedUtf16CharacterStream() { }

void BufferedUtf16CharacterStream::PushBack(uc32 character) {
  if (character == kEndOfInput) {
    pos_--;
    return;
  }
  if (pushback_limit_ == NULL && buffer_cursor_ > buffer_) {
    // buffer_ is writable, buffer_cursor_ is const pointer.
    buffer_[--buffer_cursor_ - buffer_] = static_cast<uc16>(character);
    pos_--;
    return;
  }
  SlowPushBack(static_cast<uc16>(character));
}


void BufferedUtf16CharacterStream::SlowPushBack(uc16 character) {
  // In pushback mode, the end of the buffer contains pushback,
  // and the start of the buffer (from buffer start to pushback_limit_)
  // contains valid data that comes just after the pushback.
  // We NULL the pushback_limit_ if pushing all the way back to the
  // start of the buffer.

  if (pushback_limit_ == NULL) {
    // Enter pushback mode.
    pushback_limit_ = buffer_end_;
    buffer_end_ = buffer_ + kBufferSize;
    buffer_cursor_ = buffer_end_;
  }
  // Ensure that there is room for at least one pushback.
  DCHECK(buffer_cursor_ > buffer_);
  DCHECK(pos_ > 0);
  buffer_[--buffer_cursor_ - buffer_] = character;
  if (buffer_cursor_ == buffer_) {
    pushback_limit_ = NULL;
  } else if (buffer_cursor_ < pushback_limit_) {
    pushback_limit_ = buffer_cursor_;
  }
  pos_--;
}


bool BufferedUtf16CharacterStream::ReadBlock() {
  buffer_cursor_ = buffer_;
  if (pushback_limit_ != NULL) {
    // Leave pushback mode.
    buffer_end_ = pushback_limit_;
    pushback_limit_ = NULL;
    // If there were any valid characters left at the
    // start of the buffer, use those.
    if (buffer_cursor_ < buffer_end_) return true;
    // Otherwise read a new block.
  }
  unsigned length = FillBuffer(pos_);
  buffer_end_ = buffer_ + length;
  return length > 0;
}


unsigned BufferedUtf16CharacterStream::SlowSeekForward(unsigned delta) {
  // Leave pushback mode (i.e., ignore that there might be valid data
  // in the buffer before the pushback_limit_ point).
  pushback_limit_ = NULL;
  return BufferSeekForward(delta);
}


// ----------------------------------------------------------------------------
// GenericStringUtf16CharacterStream


GenericStringUtf16CharacterStream::GenericStringUtf16CharacterStream(
    Handle<String> data,
    unsigned start_position,
    unsigned end_position)
    : string_(data),
      length_(end_position) {
  DCHECK(end_position >= start_position);
  pos_ = start_position;
}


GenericStringUtf16CharacterStream::~GenericStringUtf16CharacterStream() { }


unsigned GenericStringUtf16CharacterStream::BufferSeekForward(unsigned delta) {
  unsigned old_pos = pos_;
  pos_ = Min(pos_ + delta, length_);
  ReadBlock();
  return pos_ - old_pos;
}


unsigned GenericStringUtf16CharacterStream::FillBuffer(unsigned from_pos) {
  if (from_pos >= length_) return 0;
  unsigned length = kBufferSize;
  if (from_pos + length > length_) {
    length = length_ - from_pos;
  }
  String::WriteToFlat<uc16>(*string_, buffer_, from_pos, from_pos + length);
  return length;
}


// ----------------------------------------------------------------------------
// Utf8ToUtf16CharacterStream
Utf8ToUtf16CharacterStream::Utf8ToUtf16CharacterStream(const byte* data,
                                                       unsigned length)
    : BufferedUtf16CharacterStream(),
      raw_data_(data),
      raw_data_length_(length),
      raw_data_pos_(0),
      raw_character_position_(0) {
  ReadBlock();
}


Utf8ToUtf16CharacterStream::~Utf8ToUtf16CharacterStream() { }


unsigned Utf8ToUtf16CharacterStream::CopyChars(uint16_t* dest, unsigned length,
                                               const byte* src,
                                               unsigned* src_pos,
                                               unsigned src_length) {
  static const unibrow::uchar kMaxUtf16Character = 0xffff;
  unsigned i = 0;
  // Because of the UTF-16 lead and trail surrogates, we stop filling the buffer
  // one character early (in the normal case), because we need to have at least
  // two free spaces in the buffer to be sure that the next character will fit.
  while (i < length - 1) {
    if (*src_pos == src_length) break;
    unibrow::uchar c = src[*src_pos];
    if (c <= unibrow::Utf8::kMaxOneByteChar) {
      *src_pos = *src_pos + 1;
    } else {
      c = unibrow::Utf8::CalculateValue(src + *src_pos, src_length - *src_pos,
                                        src_pos);
    }
    if (c > kMaxUtf16Character) {
      dest[i++] = unibrow::Utf16::LeadSurrogate(c);
      dest[i++] = unibrow::Utf16::TrailSurrogate(c);
    } else {
      dest[i++] = static_cast<uc16>(c);
    }
  }
  return i;
}


unsigned Utf8ToUtf16CharacterStream::BufferSeekForward(unsigned delta) {
  unsigned old_pos = pos_;
  unsigned target_pos = pos_ + delta;
  SetRawPosition(target_pos);
  pos_ = raw_character_position_;
  ReadBlock();
  return pos_ - old_pos;
}


unsigned Utf8ToUtf16CharacterStream::FillBuffer(unsigned char_position) {
  SetRawPosition(char_position);
  if (raw_character_position_ != char_position) {
    // char_position was not a valid position in the stream (hit the end
    // while spooling to it).
    return 0u;
  }
  unsigned i = CopyChars(buffer_, kBufferSize, raw_data_, &raw_data_pos_,
                         raw_data_length_);
  raw_character_position_ = char_position + i;
  return i;
}


static const byte kUtf8MultiByteMask = 0xC0;
static const byte kUtf8MultiByteCharFollower = 0x80;


#ifdef DEBUG
static const byte kUtf8MultiByteCharStart = 0xC0;
static bool IsUtf8MultiCharacterStart(byte first_byte) {
  return (first_byte & kUtf8MultiByteMask) == kUtf8MultiByteCharStart;
}
#endif


static bool IsUtf8MultiCharacterFollower(byte later_byte) {
  return (later_byte & kUtf8MultiByteMask) == kUtf8MultiByteCharFollower;
}


// Move the cursor back to point at the preceding UTF-8 character start
// in the buffer.
static inline void Utf8CharacterBack(const byte* buffer, unsigned* cursor) {
  byte character = buffer[--*cursor];
  if (character > unibrow::Utf8::kMaxOneByteChar) {
    DCHECK(IsUtf8MultiCharacterFollower(character));
    // Last byte of a multi-byte character encoding. Step backwards until
    // pointing to the first byte of the encoding, recognized by having the
    // top two bits set.
    while (IsUtf8MultiCharacterFollower(buffer[--*cursor])) { }
    DCHECK(IsUtf8MultiCharacterStart(buffer[*cursor]));
  }
}


// Move the cursor forward to point at the next following UTF-8 character start
// in the buffer.
static inline void Utf8CharacterForward(const byte* buffer, unsigned* cursor) {
  byte character = buffer[(*cursor)++];
  if (character > unibrow::Utf8::kMaxOneByteChar) {
    // First character of a multi-byte character encoding.
    // The number of most-significant one-bits determines the length of the
    // encoding:
    //  110..... - (0xCx, 0xDx) one additional byte (minimum).
    //  1110.... - (0xEx) two additional bytes.
    //  11110... - (0xFx) three additional bytes (maximum).
    DCHECK(IsUtf8MultiCharacterStart(character));
    // Additional bytes is:
    // 1 if value in range 0xC0 .. 0xDF.
    // 2 if value in range 0xE0 .. 0xEF.
    // 3 if value in range 0xF0 .. 0xF7.
    // Encode that in a single value.
    unsigned additional_bytes =
        ((0x3211u) >> (((character - 0xC0) >> 2) & 0xC)) & 0x03;
    *cursor += additional_bytes;
    DCHECK(!IsUtf8MultiCharacterFollower(buffer[1 + additional_bytes]));
  }
}


// This can't set a raw position between two surrogate pairs, since there
// is no position in the UTF8 stream that corresponds to that.  This assumes
// that the surrogate pair is correctly coded as a 4 byte UTF-8 sequence.  If
// it is illegally coded as two 3 byte sequences then there is no problem here.
void Utf8ToUtf16CharacterStream::SetRawPosition(unsigned target_position) {
  if (raw_character_position_ > target_position) {
    // Spool backwards in utf8 buffer.
    do {
      int old_pos = raw_data_pos_;
      Utf8CharacterBack(raw_data_, &raw_data_pos_);
      raw_character_position_--;
      DCHECK(old_pos - raw_data_pos_ <= 4);
      // Step back over both code units for surrogate pairs.
      if (old_pos - raw_data_pos_ == 4) raw_character_position_--;
    } while (raw_character_position_ > target_position);
    // No surrogate pair splitting.
    DCHECK(raw_character_position_ == target_position);
    return;
  }
  // Spool forwards in the utf8 buffer.
  while (raw_character_position_ < target_position) {
    if (raw_data_pos_ == raw_data_length_) return;
    int old_pos = raw_data_pos_;
    Utf8CharacterForward(raw_data_, &raw_data_pos_);
    raw_character_position_++;
    DCHECK(raw_data_pos_ - old_pos <= 4);
    if (raw_data_pos_ - old_pos == 4) raw_character_position_++;
  }
  // No surrogate pair splitting.
  DCHECK(raw_character_position_ == target_position);
}


unsigned ExternalStreamingStream::FillBuffer(unsigned position) {
  // Ignore "position" which is the position in the decoded data. Instead,
  // ExternalStreamingStream keeps track of the position in the raw data.
  unsigned data_in_buffer = 0;
  // Note that the UTF-8 decoder might not be able to fill the buffer
  // completely; it will typically leave the last character empty (see
  // Utf8ToUtf16CharacterStream::CopyChars).
  while (data_in_buffer < kBufferSize - 1) {
    if (current_data_ == NULL) {
      // GetSomeData will wait until the embedder has enough data. Here's an
      // interface between the API which uses size_t (which is the correct type
      // here) and the internal parts which use unsigned. TODO(marja): make the
      // internal parts use size_t too.
      current_data_length_ =
          static_cast<unsigned>(source_stream_->GetMoreData(&current_data_));
      current_data_offset_ = 0;
      bool data_ends = current_data_length_ == 0;

      // A caveat: a data chunk might end with bytes from an incomplete UTF-8
      // character (the rest of the bytes will be in the next chunk).
      if (encoding_ == ScriptCompiler::StreamedSource::UTF8) {
        HandleUtf8SplitCharacters(&data_in_buffer);
        if (!data_ends && current_data_offset_ == current_data_length_) {
          // The data stream didn't end, but we used all the data in the
          // chunk. This will only happen when the chunk was really small. We
          // don't handle the case where a UTF-8 character is split over several
          // chunks; in that case V8 won't crash, but it will be a parse error.
          delete[] current_data_;
          current_data_ = NULL;
          current_data_length_ = 0;
          current_data_offset_ = 0;
          continue;  // Request a new chunk.
        }
      }

      // Did the data stream end?
      if (data_ends) {
        DCHECK(utf8_split_char_buffer_length_ == 0);
        return data_in_buffer;
      }
    }

    // Fill the buffer from current_data_.
    unsigned new_offset = 0;
    unsigned new_chars_in_buffer =
        CopyCharsHelper(buffer_ + data_in_buffer, kBufferSize - data_in_buffer,
                        current_data_ + current_data_offset_, &new_offset,
                        current_data_length_ - current_data_offset_, encoding_);
    data_in_buffer += new_chars_in_buffer;
    current_data_offset_ += new_offset;
    DCHECK(data_in_buffer <= kBufferSize);

    // Did we use all the data in the data chunk?
    if (current_data_offset_ == current_data_length_) {
      delete[] current_data_;
      current_data_ = NULL;
      current_data_length_ = 0;
      current_data_offset_ = 0;
    }
  }
  return data_in_buffer;
}

void ExternalStreamingStream::HandleUtf8SplitCharacters(
    unsigned* data_in_buffer) {
  // First check if we have leftover data from the last chunk.
  unibrow::uchar c;
  if (utf8_split_char_buffer_length_ > 0) {
    // Move the bytes which are part of the split character (which started in
    // the previous chunk) into utf8_split_char_buffer_.
    while (current_data_offset_ < current_data_length_ &&
           utf8_split_char_buffer_length_ < 4 &&
           (c = current_data_[current_data_offset_]) >
               unibrow::Utf8::kMaxOneByteChar) {
      utf8_split_char_buffer_[utf8_split_char_buffer_length_] = c;
      ++utf8_split_char_buffer_length_;
      ++current_data_offset_;
    }

    // Convert the data in utf8_split_char_buffer_.
    unsigned new_offset = 0;
    unsigned new_chars_in_buffer =
        CopyCharsHelper(buffer_ + *data_in_buffer,
                        kBufferSize - *data_in_buffer, utf8_split_char_buffer_,
                        &new_offset, utf8_split_char_buffer_length_, encoding_);
    *data_in_buffer += new_chars_in_buffer;
    // Make sure we used all the data.
    DCHECK(new_offset == utf8_split_char_buffer_length_);
    DCHECK(*data_in_buffer <= kBufferSize);

    utf8_split_char_buffer_length_ = 0;
  }

  // Move bytes which are part of an incomplete character from the end of the
  // current chunk to utf8_split_char_buffer_. They will be converted when the
  // next data chunk arrives. Note that all valid UTF-8 characters are at most 4
  // bytes long, but if the data is invalid, we can have character values bigger
  // than unibrow::Utf8::kMaxOneByteChar for more than 4 consecutive bytes.
  while (current_data_length_ > current_data_offset_ &&
         (c = current_data_[current_data_length_ - 1]) >
             unibrow::Utf8::kMaxOneByteChar &&
         utf8_split_char_buffer_length_ < 4) {
    --current_data_length_;
    ++utf8_split_char_buffer_length_;
  }
  CHECK(utf8_split_char_buffer_length_ <= 4);
  for (unsigned i = 0; i < utf8_split_char_buffer_length_; ++i) {
    utf8_split_char_buffer_[i] = current_data_[current_data_length_ + i];
  }
}


// ----------------------------------------------------------------------------
// ExternalTwoByteStringUtf16CharacterStream

ExternalTwoByteStringUtf16CharacterStream::
    ~ExternalTwoByteStringUtf16CharacterStream() { }


ExternalTwoByteStringUtf16CharacterStream
    ::ExternalTwoByteStringUtf16CharacterStream(
        Handle<ExternalTwoByteString> data,
        int start_position,
        int end_position)
    : Utf16CharacterStream(),
      source_(data),
      raw_data_(data->GetTwoByteData(start_position)) {
  buffer_cursor_ = raw_data_,
  buffer_end_ = raw_data_ + (end_position - start_position);
  pos_ = start_position;
}

} }  // namespace v8::internal
