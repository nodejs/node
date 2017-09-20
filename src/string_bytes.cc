// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "string_bytes.h"

#include "base64.h"
#include "node.h"
#include "node_buffer.h"
#include "v8.h"

#include <limits.h>
#include <string.h>  // memcpy
#include <vector>

// When creating strings >= this length v8's gc spins up and consumes
// most of the execution time. For these cases it's more performant to
// use external string resources.
#define EXTERN_APEX 0xFBEE9

// TODO(addaleax): These should all have better error messages. In particular,
// they should mention what the actual limits are.
#define SB_MALLOC_FAILED_ERROR \
  v8::Exception::Error(OneByteString(isolate, "\"toString()\" failed"))

#define SB_STRING_TOO_LONG_ERROR \
  v8::Exception::Error(OneByteString(isolate, "\"toString()\" failed"))

#define SB_BUFFER_CREATION_ERROR \
  v8::Exception::Error(OneByteString(isolate, "\"toString()\" failed"))

#define SB_BUFFER_SIZE_EXCEEDED_ERROR \
  v8::Exception::Error(OneByteString(isolate, "\"toString()\" failed"))

namespace node {

using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::String;
using v8::Value;

namespace {

template <typename ResourceType, typename TypeName>
class ExternString: public ResourceType {
 public:
  ~ExternString() override {
    free(const_cast<TypeName*>(data_));
    isolate()->AdjustAmountOfExternalAllocatedMemory(-byte_length());
  }

  const TypeName* data() const override {
    return data_;
  }

  size_t length() const override {
    return length_;
  }

  int64_t byte_length() const {
    return length() * sizeof(*data());
  }

  static MaybeLocal<Value> NewFromCopy(Isolate* isolate,
                                       const TypeName* data,
                                       size_t length,
                                       Local<Value>* error) {
    if (length == 0)
      return String::Empty(isolate);

    if (length < EXTERN_APEX)
      return NewSimpleFromCopy(isolate, data, length, error);

    TypeName* new_data = node::UncheckedMalloc<TypeName>(length);
    if (new_data == nullptr) {
      *error = SB_MALLOC_FAILED_ERROR;
      return MaybeLocal<Value>();
    }
    memcpy(new_data, data, length * sizeof(*new_data));

    return ExternString<ResourceType, TypeName>::New(isolate,
                                                     new_data,
                                                     length,
                                                     error);
  }

  // uses "data" for external resource, and will be free'd on gc
  static MaybeLocal<Value> New(Isolate* isolate,
                               TypeName* data,
                               size_t length,
                               Local<Value>* error) {
    if (length == 0)
      return String::Empty(isolate);

    if (length < EXTERN_APEX) {
      MaybeLocal<Value> str = NewSimpleFromCopy(isolate, data, length, error);
      free(data);
      return str;
    }

    ExternString* h_str = new ExternString<ResourceType, TypeName>(isolate,
                                                                   data,
                                                                   length);
    MaybeLocal<Value> str = NewExternal(isolate, h_str);
    isolate->AdjustAmountOfExternalAllocatedMemory(h_str->byte_length());

    if (str.IsEmpty()) {
      delete h_str;
      *error = SB_STRING_TOO_LONG_ERROR;
      return MaybeLocal<Value>();
    }

    return str.ToLocalChecked();
  }

  inline Isolate* isolate() const { return isolate_; }

 private:
  ExternString(Isolate* isolate, const TypeName* data, size_t length)
    : isolate_(isolate), data_(data), length_(length) { }
  static MaybeLocal<Value> NewExternal(Isolate* isolate,
                                       ExternString* h_str);

  // This method does not actually create ExternString instances.
  static MaybeLocal<Value> NewSimpleFromCopy(Isolate* isolate,
                                             const TypeName* data,
                                             size_t length,
                                             Local<Value>* error);

  Isolate* isolate_;
  const TypeName* data_;
  size_t length_;
};


typedef ExternString<String::ExternalOneByteStringResource,
                     char> ExternOneByteString;
typedef ExternString<String::ExternalStringResource,
                     uint16_t> ExternTwoByteString;


template <>
MaybeLocal<Value> ExternOneByteString::NewExternal(
    Isolate* isolate, ExternOneByteString* h_str) {
  return String::NewExternalOneByte(isolate, h_str).FromMaybe(Local<Value>());
}


template <>
MaybeLocal<Value> ExternTwoByteString::NewExternal(
    Isolate* isolate, ExternTwoByteString* h_str) {
  return String::NewExternalTwoByte(isolate, h_str).FromMaybe(Local<Value>());
}

template <>
MaybeLocal<Value> ExternOneByteString::NewSimpleFromCopy(Isolate* isolate,
                                                         const char* data,
                                                         size_t length,
                                                         Local<Value>* error) {
  MaybeLocal<String> str =
      String::NewFromOneByte(isolate,
                             reinterpret_cast<const uint8_t*>(data),
                             v8::NewStringType::kNormal,
                             length);
  if (str.IsEmpty()) {
    *error = SB_STRING_TOO_LONG_ERROR;
    return MaybeLocal<Value>();
  }
  return str.ToLocalChecked();
}


template <>
MaybeLocal<Value> ExternTwoByteString::NewSimpleFromCopy(Isolate* isolate,
                                                         const uint16_t* data,
                                                         size_t length,
                                                         Local<Value>* error) {
  MaybeLocal<String> str =
      String::NewFromTwoByte(isolate,
                             data,
                             v8::NewStringType::kNormal,
                             length);
  if (str.IsEmpty()) {
    *error = SB_STRING_TOO_LONG_ERROR;
    return MaybeLocal<Value>();
  }
  return str.ToLocalChecked();
}

}  // anonymous namespace

// supports regular and URL-safe base64
const int8_t unbase64_table[256] =
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -2, -1, -1, -2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, 62, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, 63,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
  };


static const int8_t unhex_table[256] =
  { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
     0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
  };

static inline unsigned unhex(uint8_t x) {
  return unhex_table[x];
}

template <typename TypeName>
static size_t hex_decode(char* buf,
                         size_t len,
                         const TypeName* src,
                         const size_t srcLen) {
  size_t i;
  for (i = 0; i < len && i * 2 + 1 < srcLen; ++i) {
    unsigned a = unhex(src[i * 2 + 0]);
    unsigned b = unhex(src[i * 2 + 1]);
    if (!~a || !~b)
      return i;
    buf[i] = (a << 4) | b;
  }

  return i;
}


bool StringBytes::GetExternalParts(Local<Value> val,
                                   const char** data,
                                   size_t* len) {
  if (Buffer::HasInstance(val)) {
    *data = Buffer::Data(val);
    *len = Buffer::Length(val);
    return true;
  }

  if (!val->IsString())
    return false;

  Local<String> str = val.As<String>();

  if (str->IsExternalOneByte()) {
    const String::ExternalOneByteStringResource* ext;
    ext = str->GetExternalOneByteStringResource();
    *data = ext->data();
    *len = ext->length();
    return true;

  } else if (str->IsExternal()) {
    const String::ExternalStringResource* ext;
    ext = str->GetExternalStringResource();
    *data = reinterpret_cast<const char*>(ext->data());
    *len = ext->length() * sizeof(*ext->data());
    return true;
  }

  return false;
}


size_t StringBytes::WriteUCS2(char* buf,
                              size_t buflen,
                              Local<String> str,
                              int flags,
                              size_t* chars_written) {
  uint16_t* const dst = reinterpret_cast<uint16_t*>(buf);

  size_t max_chars = (buflen / sizeof(*dst));
  size_t nchars;
  size_t alignment = reinterpret_cast<uintptr_t>(dst) % sizeof(*dst);
  if (alignment == 0) {
    nchars = str->Write(dst, 0, max_chars, flags);
    *chars_written = nchars;
    return nchars * sizeof(*dst);
  }

  uint16_t* aligned_dst =
      reinterpret_cast<uint16_t*>(buf + sizeof(*dst) - alignment);
  CHECK_EQ(reinterpret_cast<uintptr_t>(aligned_dst) % sizeof(*dst), 0);

  // Write all but the last char
  nchars = str->Write(aligned_dst, 0, max_chars - 1, flags);

  // Shift everything to unaligned-left
  memmove(dst, aligned_dst, nchars * sizeof(*dst));

  // One more char to be written
  uint16_t last;
  if (nchars == max_chars - 1 && str->Write(&last, nchars, 1, flags) != 0) {
    memcpy(buf + nchars * sizeof(*dst), &last, sizeof(last));
    nchars++;
  }

  *chars_written = nchars;
  return nchars * sizeof(*dst);
}


size_t StringBytes::Write(Isolate* isolate,
                          char* buf,
                          size_t buflen,
                          Local<Value> val,
                          enum encoding encoding,
                          int* chars_written) {
  HandleScope scope(isolate);
  const char* data = nullptr;
  size_t nbytes = 0;
  const bool is_extern = GetExternalParts(val, &data, &nbytes);
  const size_t external_nbytes = nbytes;

  CHECK(val->IsString() == true);
  Local<String> str = val.As<String>();

  if (nbytes > buflen)
    nbytes = buflen;

  int flags = String::HINT_MANY_WRITES_EXPECTED |
              String::NO_NULL_TERMINATION |
              String::REPLACE_INVALID_UTF8;

  switch (encoding) {
    case ASCII:
    case LATIN1:
      if (is_extern && str->IsOneByte()) {
        memcpy(buf, data, nbytes);
      } else {
        uint8_t* const dst = reinterpret_cast<uint8_t*>(buf);
        nbytes = str->WriteOneByte(dst, 0, buflen, flags);
      }
      if (chars_written != nullptr)
        *chars_written = nbytes;
      break;

    case BUFFER:
    case UTF8:
      nbytes = str->WriteUtf8(buf, buflen, chars_written, flags);
      break;

    case UCS2: {
      size_t nchars;

      if (is_extern && !str->IsOneByte()) {
        memcpy(buf, data, nbytes);
        nchars = nbytes / sizeof(uint16_t);
      } else {
        nbytes = WriteUCS2(buf, buflen, str, flags, &nchars);
      }
      if (chars_written != nullptr)
        *chars_written = nchars;

      // Node's "ucs2" encoding wants LE character data stored in
      // the Buffer, so we need to reorder on BE platforms.  See
      // http://nodejs.org/api/buffer.html regarding Node's "ucs2"
      // encoding specification
      if (IsBigEndian())
        SwapBytes16(buf, nbytes);

      break;
    }

    case BASE64:
      if (is_extern) {
        nbytes = base64_decode(buf, buflen, data, external_nbytes);
      } else {
        String::Value value(str);
        nbytes = base64_decode(buf, buflen, *value, value.length());
      }
      if (chars_written != nullptr) {
        *chars_written = nbytes;
      }
      break;

    case HEX:
      if (is_extern) {
        nbytes = hex_decode(buf, buflen, data, external_nbytes);
      } else {
        String::Value value(str);
        nbytes = hex_decode(buf, buflen, *value, value.length());
      }
      if (chars_written != nullptr) {
        *chars_written = nbytes;
      }
      break;

    default:
      CHECK(0 && "unknown encoding");
      break;
  }

  return nbytes;
}


bool StringBytes::IsValidString(Local<String> string,
                                enum encoding enc) {
  if (enc == HEX && string->Length() % 2 != 0)
    return false;
  // TODO(bnoordhuis) Add BASE64 check?
  return true;
}


// Quick and dirty size calculation
// Will always be at least big enough, but may have some extra
// UTF8 can be as much as 3x the size, Base64 can have 1-2 extra bytes
size_t StringBytes::StorageSize(Isolate* isolate,
                                Local<Value> val,
                                enum encoding encoding) {
  HandleScope scope(isolate);
  size_t data_size = 0;
  bool is_buffer = Buffer::HasInstance(val);

  if (is_buffer && (encoding == BUFFER || encoding == LATIN1)) {
    return Buffer::Length(val);
  }

  Local<String> str = val->ToString(isolate);

  switch (encoding) {
    case ASCII:
    case LATIN1:
      data_size = str->Length();
      break;

    case BUFFER:
    case UTF8:
      // A single UCS2 codepoint never takes up more than 3 utf8 bytes.
      // It is an exercise for the caller to decide when a string is
      // long enough to justify calling Size() instead of StorageSize()
      data_size = 3 * str->Length();
      break;

    case UCS2:
      data_size = str->Length() * sizeof(uint16_t);
      break;

    case BASE64:
      data_size = base64_decoded_size_fast(str->Length());
      break;

    case HEX:
      CHECK(str->Length() % 2 == 0 && "invalid hex string length");
      data_size = str->Length() / 2;
      break;

    default:
      CHECK(0 && "unknown encoding");
      break;
  }

  return data_size;
}


size_t StringBytes::Size(Isolate* isolate,
                         Local<Value> val,
                         enum encoding encoding) {
  HandleScope scope(isolate);
  size_t data_size = 0;
  bool is_buffer = Buffer::HasInstance(val);

  if (is_buffer && (encoding == BUFFER || encoding == LATIN1))
    return Buffer::Length(val);

  const char* data;
  if (GetExternalParts(val, &data, &data_size))
    return data_size;

  Local<String> str = val->ToString(isolate);

  switch (encoding) {
    case ASCII:
    case LATIN1:
      data_size = str->Length();
      break;

    case BUFFER:
    case UTF8:
      data_size = str->Utf8Length();
      break;

    case UCS2:
      data_size = str->Length() * sizeof(uint16_t);
      break;

    case BASE64: {
      String::Value value(str);
      data_size = base64_decoded_size(*value, value.length());
      break;
    }

    case HEX:
      data_size = str->Length() / 2;
      break;

    default:
      CHECK(0 && "unknown encoding");
      break;
  }

  return data_size;
}




static bool contains_non_ascii_slow(const char* buf, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    if (buf[i] & 0x80)
      return true;
  }
  return false;
}


static bool contains_non_ascii(const char* src, size_t len) {
  if (len < 16) {
    return contains_non_ascii_slow(src, len);
  }

  const unsigned bytes_per_word = sizeof(uintptr_t);
  const unsigned align_mask = bytes_per_word - 1;
  const unsigned unaligned = reinterpret_cast<uintptr_t>(src) & align_mask;

  if (unaligned > 0) {
    const unsigned n = bytes_per_word - unaligned;
    if (contains_non_ascii_slow(src, n))
      return true;
    src += n;
    len -= n;
  }


#if defined(_WIN64) || defined(_LP64)
  const uintptr_t mask = 0x8080808080808080ll;
#else
  const uintptr_t mask = 0x80808080l;
#endif

  const uintptr_t* srcw = reinterpret_cast<const uintptr_t*>(src);

  for (size_t i = 0, n = len / bytes_per_word; i < n; ++i) {
    if (srcw[i] & mask)
      return true;
  }

  const unsigned remainder = len & align_mask;
  if (remainder > 0) {
    const size_t offset = len - remainder;
    if (contains_non_ascii_slow(src + offset, remainder))
      return true;
  }

  return false;
}


static void force_ascii_slow(const char* src, char* dst, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    dst[i] = src[i] & 0x7f;
  }
}


static void force_ascii(const char* src, char* dst, size_t len) {
  if (len < 16) {
    force_ascii_slow(src, dst, len);
    return;
  }

  const unsigned bytes_per_word = sizeof(uintptr_t);
  const unsigned align_mask = bytes_per_word - 1;
  const unsigned src_unalign = reinterpret_cast<uintptr_t>(src) & align_mask;
  const unsigned dst_unalign = reinterpret_cast<uintptr_t>(dst) & align_mask;

  if (src_unalign > 0) {
    if (src_unalign == dst_unalign) {
      const unsigned unalign = bytes_per_word - src_unalign;
      force_ascii_slow(src, dst, unalign);
      src += unalign;
      dst += unalign;
      len -= src_unalign;
    } else {
      force_ascii_slow(src, dst, len);
      return;
    }
  }

#if defined(_WIN64) || defined(_LP64)
  const uintptr_t mask = ~0x8080808080808080ll;
#else
  const uintptr_t mask = ~0x80808080l;
#endif

  const uintptr_t* srcw = reinterpret_cast<const uintptr_t*>(src);
  uintptr_t* dstw = reinterpret_cast<uintptr_t*>(dst);

  for (size_t i = 0, n = len / bytes_per_word; i < n; ++i) {
    dstw[i] = srcw[i] & mask;
  }

  const unsigned remainder = len & align_mask;
  if (remainder > 0) {
    const size_t offset = len - remainder;
    force_ascii_slow(src + offset, dst + offset, remainder);
  }
}


static size_t hex_encode(const char* src, size_t slen, char* dst, size_t dlen) {
  // We know how much we'll write, just make sure that there's space.
  CHECK(dlen >= slen * 2 &&
      "not enough space provided for hex encode");

  dlen = slen * 2;
  for (uint32_t i = 0, k = 0; k < dlen; i += 1, k += 2) {
    static const char hex[] = "0123456789abcdef";
    uint8_t val = static_cast<uint8_t>(src[i]);
    dst[k + 0] = hex[val >> 4];
    dst[k + 1] = hex[val & 15];
  }

  return dlen;
}


#define CHECK_BUFLEN_IN_RANGE(len)                                    \
  do {                                                                \
    if ((len) > Buffer::kMaxLength) {                                 \
      *error = SB_BUFFER_SIZE_EXCEEDED_ERROR;                         \
      return MaybeLocal<Value>();                                     \
    }                                                                 \
  } while (0)


MaybeLocal<Value> StringBytes::Encode(Isolate* isolate,
                                      const char* buf,
                                      size_t buflen,
                                      enum encoding encoding,
                                      Local<Value>* error) {
  CHECK_NE(encoding, UCS2);
  CHECK_BUFLEN_IN_RANGE(buflen);

  if (!buflen && encoding != BUFFER) {
    return String::Empty(isolate);
  }

  MaybeLocal<String> val;

  switch (encoding) {
    case BUFFER:
      {
        auto maybe_buf = Buffer::Copy(isolate, buf, buflen);
        if (maybe_buf.IsEmpty()) {
          *error = SB_BUFFER_CREATION_ERROR;
          return MaybeLocal<Value>();
        }
        return maybe_buf.ToLocalChecked();
      }

    case ASCII:
      if (contains_non_ascii(buf, buflen)) {
        char* out = node::UncheckedMalloc(buflen);
        if (out == nullptr) {
          *error = SB_MALLOC_FAILED_ERROR;
          return MaybeLocal<Value>();
        }
        force_ascii(buf, out, buflen);
        return ExternOneByteString::New(isolate, out, buflen, error);
      } else {
        return ExternOneByteString::NewFromCopy(isolate, buf, buflen, error);
      }

    case UTF8:
      val = String::NewFromUtf8(isolate,
                                buf,
                                v8::NewStringType::kNormal,
                                buflen);
      if (val.IsEmpty()) {
        *error = SB_STRING_TOO_LONG_ERROR;
        return MaybeLocal<Value>();
      }
      return val.ToLocalChecked();

    case LATIN1:
      return ExternOneByteString::NewFromCopy(isolate, buf, buflen, error);

    case BASE64: {
      size_t dlen = base64_encoded_size(buflen);
      char* dst = node::UncheckedMalloc(dlen);
      if (dst == nullptr) {
        *error = SB_MALLOC_FAILED_ERROR;
        return MaybeLocal<Value>();
      }

      size_t written = base64_encode(buf, buflen, dst, dlen);
      CHECK_EQ(written, dlen);

      return ExternOneByteString::New(isolate, dst, dlen, error);
    }

    case HEX: {
      size_t dlen = buflen * 2;
      char* dst = node::UncheckedMalloc(dlen);
      if (dst == nullptr) {
        *error = SB_MALLOC_FAILED_ERROR;
        return MaybeLocal<Value>();
      }
      size_t written = hex_encode(buf, buflen, dst, dlen);
      CHECK_EQ(written, dlen);

      return ExternOneByteString::New(isolate, dst, dlen, error);
    }

    default:
      CHECK(0 && "unknown encoding");
      break;
  }

  UNREACHABLE();
}


MaybeLocal<Value> StringBytes::Encode(Isolate* isolate,
                                      const uint16_t* buf,
                                      size_t buflen,
                                      Local<Value>* error) {
  CHECK_BUFLEN_IN_RANGE(buflen);

  // Node's "ucs2" encoding expects LE character data inside a
  // Buffer, so we need to reorder on BE platforms.  See
  // http://nodejs.org/api/buffer.html regarding Node's "ucs2"
  // encoding specification
  std::vector<uint16_t> dst;
  if (IsBigEndian()) {
    dst.assign(buf, buf + buflen);
    size_t nbytes = buflen * sizeof(dst[0]);
    SwapBytes16(reinterpret_cast<char*>(&dst[0]), nbytes);
    buf = &dst[0];
  }

  return ExternTwoByteString::NewFromCopy(isolate, buf, buflen, error);
}

MaybeLocal<Value> StringBytes::Encode(Isolate* isolate,
                                      const char* buf,
                                      enum encoding encoding,
                                      Local<Value>* error) {
  const size_t len = strlen(buf);
  MaybeLocal<Value> ret;
  if (encoding == UCS2) {
    // In Node, UCS2 means utf16le. The data must be in little-endian
    // order and must be aligned on 2-bytes. This returns an empty
    // value if it's not aligned and ensures the appropriate byte order
    // on big endian architectures.
    const bool be = IsBigEndian();
    if (len % 2 != 0)
      return ret;
    std::vector<uint16_t> vec(len / 2);
    for (size_t i = 0, k = 0; i < len; i += 2, k += 1) {
      const uint8_t hi = static_cast<uint8_t>(buf[i + 0]);
      const uint8_t lo = static_cast<uint8_t>(buf[i + 1]);
      vec[k] = be ?
          static_cast<uint16_t>(hi) << 8 | lo
          : static_cast<uint16_t>(lo) << 8 | hi;
    }
    ret = vec.empty() ?
        static_cast< Local<Value> >(String::Empty(isolate))
        : StringBytes::Encode(isolate, &vec[0], vec.size(), error);
  } else {
    ret = StringBytes::Encode(isolate, buf, len, encoding, error);
  }
  return ret;
}

}  // namespace node
