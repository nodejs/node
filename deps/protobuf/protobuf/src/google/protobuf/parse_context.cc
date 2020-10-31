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

#include <google/protobuf/parse_context.h>

#include <google/protobuf/stubs/stringprintf.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/arenastring.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/repeated_field.h>
#include <google/protobuf/wire_format_lite.h>
#include <google/protobuf/stubs/strutil.h>

#include <google/protobuf/port_def.inc>

namespace google {
namespace protobuf {
namespace internal {

namespace {

// Only call if at start of tag.
bool ParseEndsInSlopRegion(const char* begin, int overrun, int d) {
  constexpr int kSlopBytes = EpsCopyInputStream::kSlopBytes;
  GOOGLE_DCHECK(overrun >= 0);
  GOOGLE_DCHECK(overrun <= kSlopBytes);
  auto ptr = begin + overrun;
  auto end = begin + kSlopBytes;
  while (ptr < end) {
    uint32 tag;
    ptr = ReadTag(ptr, &tag);
    if (ptr == nullptr || ptr > end) return false;
    // ending on 0 tag is allowed and is the major reason for the necessity of
    // this function.
    if (tag == 0) return true;
    switch (tag & 7) {
      case 0: {  // Varint
        uint64 val;
        ptr = ParseVarint64(ptr, &val);
        if (ptr == nullptr) return false;
        break;
      }
      case 1: {  // fixed64
        ptr += 8;
        break;
      }
      case 2: {  // len delim
        int32 size = ReadSize(&ptr);
        if (ptr == nullptr || size > end - ptr) return false;
        ptr += size;
        break;
      }
      case 3: {  // start group
        d++;
        break;
      }
      case 4: {                    // end group
        if (--d < 0) return true;  // We exit early
        break;
      }
      case 5: {  // fixed32
        ptr += 4;
        break;
      }
      default:
        return false;  // Unknown wireformat
    }
  }
  return false;
}

}  // namespace

const char* EpsCopyInputStream::Next(int overrun, int d) {
  if (next_chunk_ == nullptr) return nullptr;  // We've reached end of stream.
  if (next_chunk_ != buffer_) {
    GOOGLE_DCHECK(size_ > kSlopBytes);
    // The chunk is large enough to be used directly
    buffer_end_ = next_chunk_ + size_ - kSlopBytes;
    auto res = next_chunk_;
    next_chunk_ = buffer_;
    if (aliasing_ == kOnPatch) aliasing_ = kNoDelta;
    return res;
  }
  // Move the slop bytes of previous buffer to start of the patch buffer.
  // Note we must use memmove because the previous buffer could be part of
  // buffer_.
  std::memmove(buffer_, buffer_end_, kSlopBytes);
  if (overall_limit_ > 0 &&
      (d < 0 || !ParseEndsInSlopRegion(buffer_, overrun, d))) {
    const void* data;
    // ZeroCopyInputStream indicates Next may return 0 size buffers. Hence
    // we loop.
    while (zcis_->Next(&data, &size_)) {
      overall_limit_ -= size_;
      if (size_ > kSlopBytes) {
        // We got a large chunk
        std::memcpy(buffer_ + kSlopBytes, data, kSlopBytes);
        next_chunk_ = static_cast<const char*>(data);
        buffer_end_ = buffer_ + kSlopBytes;
        if (aliasing_ >= kNoDelta) aliasing_ = kOnPatch;
        return buffer_;
      } else if (size_ > 0) {
        std::memcpy(buffer_ + kSlopBytes, data, size_);
        next_chunk_ = buffer_;
        buffer_end_ = buffer_ + size_;
        if (aliasing_ >= kNoDelta) aliasing_ = kOnPatch;
        return buffer_;
      }
      GOOGLE_DCHECK(size_ == 0) << size_;
    }
    overall_limit_ = 0;  // Next failed, no more needs for next
  }
  // End of stream or array
  if (aliasing_ == kNoDelta) {
    // If there is no more block and aliasing is true, the previous block
    // is still valid and we can alias. We have users relying on string_view's
    // obtained from protos to outlive the proto, when the parse was from an
    // array. This guarantees string_view's are always aliased if parsed from
    // an array.
    aliasing_ = reinterpret_cast<std::uintptr_t>(buffer_end_) -
                reinterpret_cast<std::uintptr_t>(buffer_);
  }
  next_chunk_ = nullptr;
  buffer_end_ = buffer_ + kSlopBytes;
  size_ = 0;
  return buffer_;
}

std::pair<const char*, bool> EpsCopyInputStream::DoneFallback(const char* ptr,
                                                              int d) {
  GOOGLE_DCHECK(ptr >= limit_end_);
  int overrun = ptr - buffer_end_;
  GOOGLE_DCHECK(overrun <= kSlopBytes);  // Guaranteed by parse loop.
  // Did we exceeded the limit (parse error).
  if (PROTOBUF_PREDICT_FALSE(overrun > limit_)) return {nullptr, true};
  GOOGLE_DCHECK(overrun != limit_);  // Guaranteed by caller.
  GOOGLE_DCHECK(overrun < limit_);   // Follows from above
  // TODO(gerbens) Instead of this dcheck we could just assign, and remove
  // updating the limit_end from PopLimit, ie.
  // limit_end_ = buffer_end_ + (std::min)(0, limit_);
  // if (ptr < limit_end_) return {ptr, false};
  GOOGLE_DCHECK(limit_end_ == buffer_end_ + (std::min)(0, limit_));
  // At this point we know the following assertion holds.
  GOOGLE_DCHECK(limit_ > 0);
  GOOGLE_DCHECK(limit_end_ == buffer_end_);  // because limit_ > 0
  do {
    // We are past the end of buffer_end_, in the slop region.
    GOOGLE_DCHECK(overrun >= 0);
    auto p = Next(overrun, d);
    if (p == nullptr) {
      // We are at the end of the stream
      if (PROTOBUF_PREDICT_FALSE(overrun != 0)) return {nullptr, true};
      GOOGLE_DCHECK(limit_ > 0);
      limit_end_ = buffer_end_;
      // Distinquish ending on a pushed limit or ending on end-of-stream.
      SetEndOfStream();
      return {ptr, true};
    }
    limit_ -= buffer_end_ - p;  // Adjust limit_ relative to new anchor
    ptr = p + overrun;
    overrun = ptr - buffer_end_;
  } while (overrun >= 0);
  limit_end_ = buffer_end_ + std::min(0, limit_);
  return {ptr, false};
}

const char* EpsCopyInputStream::SkipFallback(const char* ptr, int size) {
  return AppendSize(ptr, size, [](const char* p, int s) {});
}

const char* EpsCopyInputStream::ReadStringFallback(const char* ptr, int size,
                                                   std::string* s) {
  s->clear();
  // TODO(gerbens) assess security. At the moment its parity with
  // CodedInputStream but it allows a payload to reserve large memory.
  if (PROTOBUF_PREDICT_TRUE(size <= buffer_end_ - ptr + limit_)) {
    s->reserve(size);
  }
  return AppendStringFallback(ptr, size, s);
}

const char* EpsCopyInputStream::AppendStringFallback(const char* ptr, int size,
                                                     std::string* str) {
  // TODO(gerbens) assess security. At the moment its parity with
  // CodedInputStream but it allows a payload to reserve large memory.
  if (PROTOBUF_PREDICT_TRUE(size <= buffer_end_ - ptr + limit_)) {
    str->reserve(size);
  }
  return AppendSize(ptr, size,
                    [str](const char* p, int s) { str->append(p, s); });
}


template <typename Tag, typename T>
const char* EpsCopyInputStream::ReadRepeatedFixed(const char* ptr,
                                                  Tag expected_tag,
                                                  RepeatedField<T>* out) {
  do {
    out->Add(UnalignedLoad<T>(ptr));
    ptr += sizeof(T);
    if (PROTOBUF_PREDICT_FALSE(ptr >= limit_end_)) return ptr;
  } while (UnalignedLoad<Tag>(ptr) == expected_tag&& ptr += sizeof(Tag));
  return ptr;
}

template <typename T>
const char* EpsCopyInputStream::ReadPackedFixed(const char* ptr, int size,
                                                RepeatedField<T>* out) {
  int nbytes = buffer_end_ + kSlopBytes - ptr;
  while (size > nbytes) {
    int num = nbytes / sizeof(T);
    int old_entries = out->size();
    out->Reserve(old_entries + num);
    int block_size = num * sizeof(T);
    std::memcpy(out->AddNAlreadyReserved(num), ptr, block_size);
    ptr += block_size;
    size -= block_size;
    if (DoneWithCheck(&ptr, -1)) return nullptr;
    nbytes = buffer_end_ + kSlopBytes - ptr;
  }
  int num = size / sizeof(T);
  int old_entries = out->size();
  out->Reserve(old_entries + num);
  int block_size = num * sizeof(T);
  std::memcpy(out->AddNAlreadyReserved(num), ptr, block_size);
  ptr += block_size;
  if (size != block_size) return nullptr;
  return ptr;
}

const char* EpsCopyInputStream::InitFrom(io::ZeroCopyInputStream* zcis) {
  zcis_ = zcis;
  const void* data;
  int size;
  limit_ = INT_MAX;
  if (zcis->Next(&data, &size)) {
    overall_limit_ -= size;
    if (size > kSlopBytes) {
      auto ptr = static_cast<const char*>(data);
      limit_ -= size - kSlopBytes;
      limit_end_ = buffer_end_ = ptr + size - kSlopBytes;
      next_chunk_ = buffer_;
      if (aliasing_ == kOnPatch) aliasing_ = kNoDelta;
      return ptr;
    } else {
      limit_end_ = buffer_end_ = buffer_ + kSlopBytes;
      next_chunk_ = buffer_;
      auto ptr = buffer_ + 2 * kSlopBytes - size;
      std::memcpy(ptr, data, size);
      return ptr;
    }
  }
  overall_limit_ = 0;
  next_chunk_ = nullptr;
  size_ = 0;
  limit_end_ = buffer_end_ = buffer_;
  return buffer_;
}

#if GOOGLE_PROTOBUF_ENABLE_EXPERIMENTAL_PARSER
const char* ParseContext::ParseMessage(MessageLite* msg, const char* ptr) {
  return ParseMessage<MessageLite>(msg, ptr);
}
const char* ParseContext::ParseMessage(Message* msg, const char* ptr) {
  // Use reinterptret case to prevent inclusion of non lite header
  return ParseMessage(reinterpret_cast<MessageLite*>(msg), ptr);
}
#endif

inline void WriteVarint(uint64 val, std::string* s) {
  while (val >= 128) {
    uint8 c = val | 0x80;
    s->push_back(c);
    val >>= 7;
  }
  s->push_back(val);
}

void WriteVarint(uint32 num, uint64 val, std::string* s) {
  WriteVarint(num << 3, s);
  WriteVarint(val, s);
}

void WriteLengthDelimited(uint32 num, StringPiece val, std::string* s) {
  WriteVarint((num << 3) + 2, s);
  WriteVarint(val.size(), s);
  s->append(val.data(), val.size());
}

std::pair<const char*, uint32> ReadTagFallback(const char* p, uint32 res) {
  for (std::uint32_t i = 0; i < 3; i++) {
    std::uint32_t byte = static_cast<uint8>(p[i]);
    res += (byte - 1) << (7 * (i + 2));
    if (PROTOBUF_PREDICT_TRUE(byte < 128)) {
      return {p + i + 1, res};
    }
  }
  return {nullptr, 0};
}

std::pair<const char*, uint64> ParseVarint64Fallback(const char* p,
                                                     uint64 res) {
  return ParseVarint64FallbackInline(p, res);
}

std::pair<const char*, int32> ReadSizeFallback(const char* p, uint32 first) {
  uint32 tmp;
  auto res = VarintParse<4>(p + 1, &tmp);
  if (tmp >= (1 << 24) - ParseContext::kSlopBytes) return {nullptr, 0};
  return {res, (tmp << 7) + first - 0x80};
}

const char* StringParser(const char* begin, const char* end, void* object,
                         ParseContext*) {
  auto str = static_cast<std::string*>(object);
  str->append(begin, end - begin);
  return end;
}

// Defined in wire_format_lite.cc
void PrintUTF8ErrorLog(const char* field_name, const char* operation_str,
                       bool emit_stacktrace);

bool VerifyUTF8(StringPiece str, const char* field_name) {
  if (!IsStructurallyValidUTF8(str)) {
    PrintUTF8ErrorLog(field_name, "parsing", false);
    return false;
  }
  return true;
}

const char* InlineGreedyStringParser(std::string* s, const char* ptr,
                                     ParseContext* ctx) {
  int size = ReadSize(&ptr);
  if (!ptr) return nullptr;
  return ctx->ReadString(ptr, size, s);
}

const char* InlineGreedyStringParserUTF8(std::string* s, const char* ptr,
                                         ParseContext* ctx,
                                         const char* field_name) {
  auto p = InlineGreedyStringParser(s, ptr, ctx);
  GOOGLE_PROTOBUF_PARSER_ASSERT(VerifyUTF8(*s, field_name));
  return p;
}


template <typename T, bool sign>
const char* VarintParser(void* object, const char* ptr, ParseContext* ctx) {
  return ctx->ReadPackedVarint(ptr, [object](uint64 varint) {
    T val;
    if (sign) {
      if (sizeof(T) == 8) {
        val = WireFormatLite::ZigZagDecode64(varint);
      } else {
        val = WireFormatLite::ZigZagDecode32(varint);
      }
    } else {
      val = varint;
    }
    static_cast<RepeatedField<T>*>(object)->Add(val);
  });
}

const char* PackedInt32Parser(void* object, const char* ptr,
                              ParseContext* ctx) {
  return VarintParser<int32, false>(object, ptr, ctx);
}
const char* PackedUInt32Parser(void* object, const char* ptr,
                               ParseContext* ctx) {
  return VarintParser<uint32, false>(object, ptr, ctx);
}
const char* PackedInt64Parser(void* object, const char* ptr,
                              ParseContext* ctx) {
  return VarintParser<int64, false>(object, ptr, ctx);
}
const char* PackedUInt64Parser(void* object, const char* ptr,
                               ParseContext* ctx) {
  return VarintParser<uint64, false>(object, ptr, ctx);
}
const char* PackedSInt32Parser(void* object, const char* ptr,
                               ParseContext* ctx) {
  return VarintParser<int32, true>(object, ptr, ctx);
}
const char* PackedSInt64Parser(void* object, const char* ptr,
                               ParseContext* ctx) {
  return VarintParser<int64, true>(object, ptr, ctx);
}

const char* PackedEnumParser(void* object, const char* ptr, ParseContext* ctx) {
  return VarintParser<int, false>(object, ptr, ctx);
}

const char* PackedEnumParser(void* object, const char* ptr, ParseContext* ctx,
                             bool (*is_valid)(int),
                             InternalMetadataWithArenaLite* metadata,
                             int field_num) {
  return ctx->ReadPackedVarint(
      ptr, [object, is_valid, metadata, field_num](uint64 val) {
        if (is_valid(val)) {
          static_cast<RepeatedField<int>*>(object)->Add(val);
        } else {
          WriteVarint(field_num, val, metadata->mutable_unknown_fields());
        }
      });
}

const char* PackedEnumParserArg(void* object, const char* ptr,
                                ParseContext* ctx,
                                bool (*is_valid)(const void*, int),
                                const void* data,
                                InternalMetadataWithArenaLite* metadata,
                                int field_num) {
  return ctx->ReadPackedVarint(
      ptr, [object, is_valid, data, metadata, field_num](uint64 val) {
        if (is_valid(data, val)) {
          static_cast<RepeatedField<int>*>(object)->Add(val);
        } else {
          WriteVarint(field_num, val, metadata->mutable_unknown_fields());
        }
      });
}

const char* PackedBoolParser(void* object, const char* ptr, ParseContext* ctx) {
  return VarintParser<bool, false>(object, ptr, ctx);
}

template <typename T>
const char* FixedParser(void* object, const char* ptr, ParseContext* ctx) {
  int size = ReadSize(&ptr);
  GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
  return ctx->ReadPackedFixed(ptr, size,
                              static_cast<RepeatedField<T>*>(object));
}

const char* PackedFixed32Parser(void* object, const char* ptr,
                                ParseContext* ctx) {
  return FixedParser<uint32>(object, ptr, ctx);
}
const char* PackedSFixed32Parser(void* object, const char* ptr,
                                 ParseContext* ctx) {
  return FixedParser<int32>(object, ptr, ctx);
}
const char* PackedFixed64Parser(void* object, const char* ptr,
                                ParseContext* ctx) {
  return FixedParser<uint64>(object, ptr, ctx);
}
const char* PackedSFixed64Parser(void* object, const char* ptr,
                                 ParseContext* ctx) {
  return FixedParser<int64>(object, ptr, ctx);
}
const char* PackedFloatParser(void* object, const char* ptr,
                              ParseContext* ctx) {
  return FixedParser<float>(object, ptr, ctx);
}
const char* PackedDoubleParser(void* object, const char* ptr,
                               ParseContext* ctx) {
  return FixedParser<double>(object, ptr, ctx);
}

class UnknownFieldLiteParserHelper {
 public:
  explicit UnknownFieldLiteParserHelper(std::string* unknown)
      : unknown_(unknown) {}

  void AddVarint(uint32 num, uint64 value) {
    if (unknown_ == nullptr) return;
    WriteVarint(num * 8, unknown_);
    WriteVarint(value, unknown_);
  }
  void AddFixed64(uint32 num, uint64 value) {
    if (unknown_ == nullptr) return;
    WriteVarint(num * 8 + 1, unknown_);
    char buffer[8];
    std::memcpy(buffer, &value, 8);
    unknown_->append(buffer, 8);
  }
  const char* ParseLengthDelimited(uint32 num, const char* ptr,
                                   ParseContext* ctx) {
    int size = ReadSize(&ptr);
    GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
    if (unknown_ == nullptr) return ctx->Skip(ptr, size);
    WriteVarint(num * 8 + 2, unknown_);
    WriteVarint(size, unknown_);
    return ctx->AppendString(ptr, size, unknown_);
  }
  const char* ParseGroup(uint32 num, const char* ptr, ParseContext* ctx) {
    if (unknown_) WriteVarint(num * 8 + 3, unknown_);
    ptr = ctx->ParseGroup(this, ptr, num * 8 + 3);
    GOOGLE_PROTOBUF_PARSER_ASSERT(ptr);
    if (unknown_) WriteVarint(num * 8 + 4, unknown_);
    return ptr;
  }
  void AddFixed32(uint32 num, uint32 value) {
    if (unknown_ == nullptr) return;
    WriteVarint(num * 8 + 5, unknown_);
    char buffer[4];
    std::memcpy(buffer, &value, 4);
    unknown_->append(buffer, 4);
  }

  const char* _InternalParse(const char* ptr, ParseContext* ctx) {
    return WireFormatParser(*this, ptr, ctx);
  }

 private:
  std::string* unknown_;
};

const char* UnknownGroupLiteParse(std::string* unknown, const char* ptr,
                                  ParseContext* ctx) {
  UnknownFieldLiteParserHelper field_parser(unknown);
  return WireFormatParser(field_parser, ptr, ctx);
}

const char* UnknownFieldParse(uint32 tag, std::string* unknown, const char* ptr,
                              ParseContext* ctx) {
  UnknownFieldLiteParserHelper field_parser(unknown);
  return FieldParser(tag, field_parser, ptr, ctx);
}

const char* UnknownFieldParse(uint32 tag,
                              InternalMetadataWithArenaLite* metadata,
                              const char* ptr, ParseContext* ctx) {
  return UnknownFieldParse(tag, metadata->mutable_unknown_fields(), ptr, ctx);
}

}  // namespace internal
}  // namespace protobuf
}  // namespace google
