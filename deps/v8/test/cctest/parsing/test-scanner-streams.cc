// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/factory.h"  // for i::Factory::NewExternalStringFrom*Byte
#include "src/objects-inl.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/parsing/scanner.h"
#include "src/type-feedback-vector-inl.h"  // for include "src/factory.h"
#include "test/cctest/cctest.h"

namespace {

// Implement ExternalSourceStream based on const char**.
// This will take each string as one chunk. The last chunk must be empty.
class ChunkSource : public v8::ScriptCompiler::ExternalSourceStream {
 public:
  explicit ChunkSource(const char** chunks) : current_(0) {
    do {
      chunks_.push_back(
          {reinterpret_cast<const uint8_t*>(*chunks), strlen(*chunks)});
      chunks++;
    } while (chunks_.back().len > 0);
  }
  ChunkSource(const uint8_t* data, size_t len, bool extra_chunky)
      : current_(0) {
    // If extra_chunky, we'll use increasingly large chunk sizes.
    // If not, we'll have a single chunk of full length.
    size_t chunk_size = extra_chunky ? 1 : len;
    for (size_t i = 0; i < len; i += chunk_size, chunk_size *= 2) {
      chunks_.push_back({data + i, i::Min(chunk_size, len - i)});
    }
    chunks_.push_back({nullptr, 0});
  }
  ~ChunkSource() {}
  bool SetBookmark() override { return false; }
  void ResetToBookmark() override {}
  size_t GetMoreData(const uint8_t** src) override {
    DCHECK_LT(current_, chunks_.size());
    Chunk& next = chunks_[current_++];
    uint8_t* chunk = new uint8_t[next.len];
    i::MemMove(chunk, next.ptr, next.len);
    *src = chunk;
    return next.len;
  }

 private:
  struct Chunk {
    const uint8_t* ptr;
    size_t len;
  };
  std::vector<Chunk> chunks_;
  size_t current_;
};

class TestExternalResource : public v8::String::ExternalStringResource {
 public:
  explicit TestExternalResource(uint16_t* data, int length)
      : data_(data), length_(static_cast<size_t>(length)) {}

  ~TestExternalResource() {}

  const uint16_t* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  uint16_t* data_;
  size_t length_;
};

class TestExternalOneByteResource
    : public v8::String::ExternalOneByteStringResource {
 public:
  TestExternalOneByteResource(const char* data, size_t length)
      : data_(data), length_(length) {}

  const char* data() const { return data_; }
  size_t length() const { return length_; }

 private:
  const char* data_;
  size_t length_;
};

// A test string with all lengths of utf-8 encodings.
const char unicode_utf8[] =
    "abc"               // 3x ascii
    "\xc3\xa4"          // a Umlaut, code point 228
    "\xe2\xa8\xa0"      // >> (math symbol), code point 10784
    "\xf0\x9f\x92\xa9"  // best character, code point 128169,
                        //     as utf-16 surrogates: 55357 56489
    "def";              // 3x ascii again.
const uint16_t unicode_ucs2[] = {97,    98,  99,  228, 10784, 55357,
                                 56489, 100, 101, 102, 0};

}  // anonymous namespace

TEST(Utf8StreamAsciiOnly) {
  const char* chunks[] = {"abc", "def", "ghi", ""};
  ChunkSource chunk_source(chunks);
  std::unique_ptr<v8::internal::Utf16CharacterStream> stream(
      v8::internal::ScannerStream::For(
          &chunk_source, v8::ScriptCompiler::StreamedSource::UTF8));

  // Read the data without dying.
  v8::internal::uc32 c;
  do {
    c = stream->Advance();
  } while (c != v8::internal::Utf16CharacterStream::kEndOfInput);
}

TEST(Utf8StreamBOM) {
  // Construct test string w/ UTF-8 BOM (byte order mark)
  char data[3 + arraysize(unicode_utf8)] = {"\xef\xbb\xbf"};
  strncpy(data + 3, unicode_utf8, arraysize(unicode_utf8));

  const char* chunks[] = {data, "\0"};
  ChunkSource chunk_source(chunks);
  std::unique_ptr<v8::internal::Utf16CharacterStream> stream(
      v8::internal::ScannerStream::For(
          &chunk_source, v8::ScriptCompiler::StreamedSource::UTF8));

  // Read the data without tripping over the BOM.
  for (size_t i = 0; unicode_ucs2[i]; i++) {
    CHECK_EQ(unicode_ucs2[i], stream->Advance());
  }
  CHECK_EQ(v8::internal::Utf16CharacterStream::kEndOfInput, stream->Advance());

  // Make sure seek works.
  stream->Seek(0);
  CHECK_EQ(unicode_ucs2[0], stream->Advance());

  stream->Seek(5);
  CHECK_EQ(unicode_ucs2[5], stream->Advance());
}

TEST(Utf8SplitBOM) {
  // Construct chunks with a BOM split into two chunks.
  char partial_bom[] = "\xef\xbb";
  char data[1 + arraysize(unicode_utf8)] = {"\xbf"};
  strncpy(data + 1, unicode_utf8, arraysize(unicode_utf8));

  {
    const char* chunks[] = {partial_bom, data, "\0"};
    ChunkSource chunk_source(chunks);
    std::unique_ptr<v8::internal::Utf16CharacterStream> stream(
        v8::internal::ScannerStream::For(
            &chunk_source, v8::ScriptCompiler::StreamedSource::UTF8));

    // Read the data without tripping over the BOM.
    for (size_t i = 0; unicode_ucs2[i]; i++) {
      CHECK_EQ(unicode_ucs2[i], stream->Advance());
    }
  }

  // And now with single-byte BOM chunks.
  char bom_byte_1[] = "\xef";
  char bom_byte_2[] = "\xbb";
  {
    const char* chunks[] = {bom_byte_1, bom_byte_2, data, "\0"};
    ChunkSource chunk_source(chunks);
    std::unique_ptr<v8::internal::Utf16CharacterStream> stream(
        v8::internal::ScannerStream::For(
            &chunk_source, v8::ScriptCompiler::StreamedSource::UTF8));

    // Read the data without tripping over the BOM.
    for (size_t i = 0; unicode_ucs2[i]; i++) {
      CHECK_EQ(unicode_ucs2[i], stream->Advance());
    }
  }
}

TEST(Utf8ChunkBoundaries) {
  // Test utf-8 parsing at chunk boundaries.

  // Split the test string at each byte and pass it to the stream. This way,
  // we'll have a split at each possible boundary.
  size_t len = strlen(unicode_utf8);
  char buffer[arraysize(unicode_utf8) + 3];
  for (size_t i = 1; i < len; i++) {
    // Copy source string into buffer, splitting it at i.
    // Then add three chunks, 0..i-1, i..strlen-1, empty.
    strncpy(buffer, unicode_utf8, i);
    strncpy(buffer + i + 1, unicode_utf8 + i, len - i);
    buffer[i] = '\0';
    buffer[len + 1] = '\0';
    buffer[len + 2] = '\0';
    const char* chunks[] = {buffer, buffer + i + 1, buffer + len + 2};

    ChunkSource chunk_source(chunks);
    std::unique_ptr<v8::internal::Utf16CharacterStream> stream(
        v8::internal::ScannerStream::For(
            &chunk_source, v8::ScriptCompiler::StreamedSource::UTF8));

    for (size_t i = 0; unicode_ucs2[i]; i++) {
      CHECK_EQ(unicode_ucs2[i], stream->Advance());
    }
    CHECK_EQ(v8::internal::Utf16CharacterStream::kEndOfInput,
             stream->Advance());
  }
}

TEST(Utf8SingleByteChunks) {
  // Have each byte as a single-byte chunk.
  size_t len = strlen(unicode_utf8);
  char buffer[arraysize(unicode_utf8) + 4];
  for (size_t i = 1; i < len - 1; i++) {
    // Copy source string into buffer, make a single-byte chunk at i.
    strncpy(buffer, unicode_utf8, i);
    strncpy(buffer + i + 3, unicode_utf8 + i + 1, len - i - 1);
    buffer[i] = '\0';
    buffer[i + 1] = unicode_utf8[i];
    buffer[i + 2] = '\0';
    buffer[len + 2] = '\0';
    buffer[len + 3] = '\0';
    const char* chunks[] = {buffer, buffer + i + 1, buffer + i + 3,
                            buffer + len + 3};

    ChunkSource chunk_source(chunks);
    std::unique_ptr<v8::internal::Utf16CharacterStream> stream(
        v8::internal::ScannerStream::For(
            &chunk_source, v8::ScriptCompiler::StreamedSource::UTF8));

    for (size_t j = 0; unicode_ucs2[j]; j++) {
      CHECK_EQ(unicode_ucs2[j], stream->Advance());
    }
    CHECK_EQ(v8::internal::Utf16CharacterStream::kEndOfInput,
             stream->Advance());
  }
}

#define CHECK_EQU(v1, v2) CHECK_EQ(static_cast<int>(v1), static_cast<int>(v2))

void TestCharacterStream(const char* reference, i::Utf16CharacterStream* stream,
                         unsigned length, unsigned start, unsigned end) {
  // Read streams one char at a time
  unsigned i;
  for (i = start; i < end; i++) {
    CHECK_EQU(i, stream->pos());
    CHECK_EQU(reference[i], stream->Advance());
  }
  CHECK_EQU(end, stream->pos());

  // Pushback, re-read, pushback again.
  while (i > end / 4) {
    int32_t c0 = reference[i - 1];
    CHECK_EQU(i, stream->pos());
    stream->Back();
    i--;
    CHECK_EQU(i, stream->pos());
    int32_t c1 = stream->Advance();
    i++;
    CHECK_EQU(i, stream->pos());
    CHECK_EQ(c0, c1);
    stream->Back();
    i--;
    CHECK_EQU(i, stream->pos());
  }

  // Seek + read streams one char at a time.
  unsigned halfway = end / 2;
  stream->Seek(stream->pos() + halfway - i);
  for (i = halfway; i < end; i++) {
    CHECK_EQU(i, stream->pos());
    CHECK_EQU(reference[i], stream->Advance());
  }
  CHECK_EQU(i, stream->pos());
  CHECK_LT(stream->Advance(), 0);

  // Seek back, then seek beyond end of stream.
  stream->Seek(start);
  if (start < length) {
    CHECK_EQU(stream->Advance(), reference[start]);
  } else {
    CHECK_LT(stream->Advance(), 0);
  }
  stream->Seek(length + 5);
  CHECK_LT(stream->Advance(), 0);
}

#undef CHECK_EQU

void TestCharacterStreams(const char* one_byte_source, unsigned length,
                          unsigned start = 0, unsigned end = 0) {
  if (end == 0) end = length;

  i::Isolate* isolate = CcTest::i_isolate();
  i::Factory* factory = isolate->factory();

  // 2-byte external string
  std::unique_ptr<i::uc16[]> uc16_buffer(new i::uc16[length]);
  i::Vector<const i::uc16> two_byte_vector(uc16_buffer.get(),
                                           static_cast<int>(length));
  {
    for (unsigned i = 0; i < length; i++) {
      uc16_buffer[i] = static_cast<i::uc16>(one_byte_source[i]);
    }
    TestExternalResource resource(uc16_buffer.get(), length);
    i::Handle<i::String> uc16_string(
        factory->NewExternalStringFromTwoByte(&resource).ToHandleChecked());
    std::unique_ptr<i::Utf16CharacterStream> uc16_stream(
        i::ScannerStream::For(uc16_string, start, end));
    TestCharacterStream(one_byte_source, uc16_stream.get(), length, start, end);
  }

  // 1-byte external string
  i::Vector<const char> one_byte_vector(one_byte_source,
                                        static_cast<int>(length));
  i::Handle<i::String> one_byte_string =
      factory->NewStringFromAscii(one_byte_vector).ToHandleChecked();
  {
    TestExternalOneByteResource one_byte_resource(one_byte_source, length);
    i::Handle<i::String> ext_one_byte_string(
        factory->NewExternalStringFromOneByte(&one_byte_resource)
            .ToHandleChecked());
    std::unique_ptr<i::Utf16CharacterStream> one_byte_stream(
        i::ScannerStream::For(ext_one_byte_string, start, end));
    TestCharacterStream(one_byte_source, one_byte_stream.get(), length, start,
                        end);
  }

  // 1-byte generic i::String
  {
    std::unique_ptr<i::Utf16CharacterStream> string_stream(
        i::ScannerStream::For(one_byte_string, start, end));
    TestCharacterStream(one_byte_source, string_stream.get(), length, start,
                        end);
  }

  // 2-byte generic i::String
  {
    i::Handle<i::String> two_byte_string =
        factory->NewStringFromTwoByte(two_byte_vector).ToHandleChecked();
    std::unique_ptr<i::Utf16CharacterStream> two_byte_string_stream(
        i::ScannerStream::For(two_byte_string, start, end));
    TestCharacterStream(one_byte_source, two_byte_string_stream.get(), length,
                        start, end);
  }

  // Streaming has no notion of start/end, so let's skip streaming tests for
  // these cases.
  if (start != 0 || end != length) return;

  // 1-byte streaming stream, single + many chunks.
  {
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(one_byte_vector.begin());
    const uint8_t* data_end =
        reinterpret_cast<const uint8_t*>(one_byte_vector.end());

    ChunkSource single_chunk(data, data_end - data, false);
    std::unique_ptr<i::Utf16CharacterStream> one_byte_streaming_stream(
        i::ScannerStream::For(&single_chunk,
                              v8::ScriptCompiler::StreamedSource::ONE_BYTE));
    TestCharacterStream(one_byte_source, one_byte_streaming_stream.get(),
                        length, start, end);

    ChunkSource many_chunks(data, data_end - data, true);
    one_byte_streaming_stream.reset(i::ScannerStream::For(
        &many_chunks, v8::ScriptCompiler::StreamedSource::ONE_BYTE));
    TestCharacterStream(one_byte_source, one_byte_streaming_stream.get(),
                        length, start, end);
  }

  // UTF-8 streaming stream, single + many chunks.
  {
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(one_byte_vector.begin());
    const uint8_t* data_end =
        reinterpret_cast<const uint8_t*>(one_byte_vector.end());
    ChunkSource chunks(data, data_end - data, false);
    std::unique_ptr<i::Utf16CharacterStream> utf8_streaming_stream(
        i::ScannerStream::For(&chunks,
                              v8::ScriptCompiler::StreamedSource::UTF8));
    TestCharacterStream(one_byte_source, utf8_streaming_stream.get(), length,
                        start, end);

    ChunkSource many_chunks(data, data_end - data, true);
    utf8_streaming_stream.reset(i::ScannerStream::For(
        &many_chunks, v8::ScriptCompiler::StreamedSource::UTF8));
    TestCharacterStream(one_byte_source, utf8_streaming_stream.get(), length,
                        start, end);
  }

  // 2-byte streaming stream, single + many chunks.
  {
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(two_byte_vector.begin());
    const uint8_t* data_end =
        reinterpret_cast<const uint8_t*>(two_byte_vector.end());
    ChunkSource chunks(data, data_end - data, false);
    std::unique_ptr<i::Utf16CharacterStream> two_byte_streaming_stream(
        i::ScannerStream::For(&chunks,
                              v8::ScriptCompiler::StreamedSource::TWO_BYTE));
    TestCharacterStream(one_byte_source, two_byte_streaming_stream.get(),
                        length, start, end);

    ChunkSource many_chunks(data, data_end - data, true);
    two_byte_streaming_stream.reset(i::ScannerStream::For(
        &many_chunks, v8::ScriptCompiler::StreamedSource::TWO_BYTE));
    TestCharacterStream(one_byte_source, two_byte_streaming_stream.get(),
                        length, start, end);
  }
}

TEST(CharacterStreams) {
  v8::Isolate* isolate = CcTest::isolate();
  v8::HandleScope handles(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  TestCharacterStreams("abcdefghi", 9);
  TestCharacterStreams("abc\0\n\r\x7f", 7);
  TestCharacterStreams("\0", 1);
  TestCharacterStreams("", 0);

  // 4k large buffer.
  char buffer[4096 + 1];
  for (unsigned i = 0; i < arraysize(buffer); i++) {
    buffer[i] = static_cast<char>(i & 0x7F);
  }
  buffer[arraysize(buffer) - 1] = '\0';
  TestCharacterStreams(buffer, arraysize(buffer) - 1);
  TestCharacterStreams(buffer, arraysize(buffer) - 1, 576, 3298);
}

// Regression test for crbug.com/651333. Read invalid utf-8.
TEST(Regress651333) {
  const uint8_t bytes[] =
      "A\xf1"
      "ad";  // Anad, with n == n-with-tilde.
  const uint16_t unicode[] = {65, 65533, 97, 100};

  // Run the test for all sub-strings 0..N of bytes, to make sure we hit the
  // error condition in and at chunk boundaries.
  for (size_t len = 0; len < arraysize(bytes); len++) {
    // Read len bytes from bytes, and compare against the expected unicode
    // characters. Expect kBadChar ( == Unicode replacement char == code point
    // 65533) instead of the incorrectly coded Latin1 char.
    ChunkSource chunks(bytes, len, false);
    std::unique_ptr<i::Utf16CharacterStream> stream(i::ScannerStream::For(
        &chunks, v8::ScriptCompiler::StreamedSource::UTF8));
    for (size_t i = 0; i < len; i++) {
      CHECK_EQ(unicode[i], stream->Advance());
    }
    CHECK_EQ(i::Utf16CharacterStream::kEndOfInput, stream->Advance());
  }
}
