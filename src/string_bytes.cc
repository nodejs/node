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

#include "node.h"
#include "node_buffer.h"
#include "v8.h"

#include <assert.h>
#include <limits.h>
#include <string.h>  // memcpy

// When creating strings >= this length v8's gc spins up and consumes
// most of the execution time. For these cases it's more performant to
// use external string resources.
#define EXTERN_APEX 0xFBEE9

namespace node {

using v8::Handle;
using v8::HandleScope;
using v8::Local;
using v8::String;
using v8::Value;


template <typename ResourceType, typename TypeName>
class ExternString: public ResourceType {
  public:
    ~ExternString() {
      delete[] data_;
      node_isolate->AdjustAmountOfExternalAllocatedMemory(-length_);
    }

    const TypeName* data() const {
      return data_;
    }

    size_t length() const {
      return length_;
    }

    static Local<String> NewFromCopy(const TypeName* data, size_t length) {
      HandleScope scope(node_isolate);

      if (length == 0)
        return scope.Close(String::Empty(node_isolate));

      TypeName* new_data = new TypeName[length];
      memcpy(new_data, data, length * sizeof(*new_data));

      return scope.Close(ExternString<ResourceType, TypeName>::New(new_data,
                                                                   length));
    }

    // uses "data" for external resource, and will be free'd on gc
    static Local<String> New(const TypeName* data, size_t length) {
      HandleScope scope(node_isolate);

      if (length == 0)
        return scope.Close(String::Empty(node_isolate));

      ExternString* h_str = new ExternString<ResourceType, TypeName>(data,
                                                                     length);
      Local<String> str = String::NewExternal(h_str);
      node_isolate->AdjustAmountOfExternalAllocatedMemory(length);

      return scope.Close(str);
    }

  private:
    ExternString(const TypeName* data, size_t length)
      : data_(data), length_(length) { }
    const TypeName* data_;
    size_t length_;
};


typedef ExternString<String::ExternalAsciiStringResource,
                     char> ExternOneByteString;
typedef ExternString<String::ExternalStringResource,
                     uint16_t> ExternTwoByteString;


//// Base 64 ////

#define base64_encoded_size(size) ((size + 2 - ((size + 2) % 3)) / 3 * 4)


// Doesn't check for padding at the end.  Can be 1-2 bytes over.
static inline size_t base64_decoded_size_fast(size_t size) {
  size_t remainder = size % 4;

  size = (size / 4) * 3;
  if (remainder) {
    if (size == 0 && remainder == 1) {
      // special case: 1-byte input cannot be decoded
      size = 0;
    } else {
      // non-padded input, add 1 or 2 extra bytes
      size += 1 + (remainder == 3);
    }
  }

  return size;
}

template <typename TypeName>
size_t base64_decoded_size(const TypeName* src, size_t size) {
  if (size == 0)
    return 0;

  if (src[size - 1] == '=')
    size--;
  if (size > 0 && src[size - 1] == '=')
    size--;

  return base64_decoded_size_fast(size);
}


// supports regular and URL-safe base64
static const int unbase64_table[] =
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
#define unbase64(x) unbase64_table[(uint8_t)(x)]


template <typename TypeName>
size_t base64_decode(char* buf,
                     size_t len,
                     const TypeName* src,
                     const size_t srcLen) {
  char a, b, c, d;
  char* dst = buf;
  char* dstEnd = buf + len;
  const TypeName* srcEnd = src + srcLen;

  while (src < srcEnd && dst < dstEnd) {
    int remaining = srcEnd - src;

    while (unbase64(*src) < 0 && src < srcEnd)
      src++, remaining--;
    if (remaining == 0 || *src == '=')
      break;
    a = unbase64(*src++);

    while (unbase64(*src) < 0 && src < srcEnd)
      src++, remaining--;
    if (remaining <= 1 || *src == '=')
      break;
    b = unbase64(*src++);

    *dst++ = (a << 2) | ((b & 0x30) >> 4);
    if (dst == dstEnd)
      break;

    while (unbase64(*src) < 0 && src < srcEnd)
      src++, remaining--;
    if (remaining <= 2 || *src == '=')
      break;
    c = unbase64(*src++);

    *dst++ = ((b & 0x0F) << 4) | ((c & 0x3C) >> 2);
    if (dst == dstEnd)
      break;

    while (unbase64(*src) < 0 && src < srcEnd)
      src++, remaining--;
    if (remaining <= 3 || *src == '=')
      break;
    d = unbase64(*src++);

    *dst++ = ((c & 0x03) << 6) | (d & 0x3F);
  }

  return dst - buf;
}


//// HEX ////

template <typename TypeName>
unsigned hex2bin(TypeName c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'A' && c <= 'F')
    return 10 + (c - 'A');
  if (c >= 'a' && c <= 'f')
    return 10 + (c - 'a');
  return static_cast<unsigned>(-1);
}


template <typename TypeName>
size_t hex_decode(char* buf,
                  size_t len,
                  const TypeName* src,
                  const size_t srcLen) {
  size_t i;
  for (i = 0; i < len && i * 2 + 1 < srcLen; ++i) {
    unsigned a = hex2bin(src[i * 2 + 0]);
    unsigned b = hex2bin(src[i * 2 + 1]);
    if (!~a || !~b)
      return i;
    buf[i] = a * 16 + b;
  }

  return i;
}


bool StringBytes::GetExternalParts(Handle<Value> val,
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

  if (str->IsExternalAscii()) {
    const String::ExternalAsciiStringResource* ext;
    ext = str->GetExternalAsciiStringResource();
    *data = ext->data();
    *len = ext->length();
    return true;

  } else if (str->IsExternal()) {
    const String::ExternalStringResource* ext;
    ext = str->GetExternalStringResource();
    *data = reinterpret_cast<const char*>(ext->data());
    *len = ext->length();
    return true;
  }

  return false;
}


size_t StringBytes::Write(char* buf,
                          size_t buflen,
                          Handle<Value> val,
                          enum encoding encoding,
                          int* chars_written) {
  HandleScope scope(node_isolate);
  const char* data;
  size_t len = 0;
  bool is_extern = GetExternalParts(val, &data, &len);

  Local<String> str = val.As<String>();
  len = len < buflen ? len : buflen;

  int flags = String::NO_NULL_TERMINATION |
              String::HINT_MANY_WRITES_EXPECTED;

  switch (encoding) {
    case ASCII:
    case BINARY:
    case BUFFER:
      if (is_extern)
        memcpy(buf, data, len);
      else
        len = str->WriteOneByte(reinterpret_cast<uint8_t*>(buf),
                                0,
                                buflen,
                                flags);
      if (chars_written != NULL)
        *chars_written = len;
      break;

    case UTF8:
      if (is_extern)
        memcpy(buf, data, len);
      else
        len = str->WriteUtf8(buf, buflen, chars_written, flags);
      break;

    case UCS2:
      if (is_extern)
        memcpy(buf, data, len * 2);
      else
        len = str->Write(reinterpret_cast<uint16_t*>(buf), 0, buflen, flags);
      if (chars_written != NULL)
        *chars_written = len;
      len = len * sizeof(uint16_t);
      break;

    case BASE64:
      if (is_extern) {
        base64_decode(buf, buflen, data, len);
      } else {
        String::Value value(str);
        len = base64_decode(buf, buflen, *value, value.length());
      }
      if (chars_written != NULL) {
        *chars_written = len;
      }
      break;

    case HEX:
      if (is_extern) {
        hex_decode(buf, buflen, data, len);
      } else {
        String::Value value(str);
        len = hex_decode(buf, buflen, *value, value.length());
      }
      if (chars_written != NULL) {
        *chars_written = len * 2;
      }
      break;

    default:
      assert(0 && "unknown encoding");
      break;
  }

  return len;
}


bool StringBytes::IsValidString(Handle<String> string, enum encoding enc) {
  if (enc == HEX && string->Length() % 2 != 0)
    return false;
  // TODO(bnoordhuis) Add BASE64 check?
  return true;
}


// Quick and dirty size calculation
// Will always be at least big enough, but may have some extra
// UTF8 can be as much as 3x the size, Base64 can have 1-2 extra bytes
size_t StringBytes::StorageSize(Handle<Value> val, enum encoding encoding) {
  HandleScope scope(node_isolate);
  size_t data_size = 0;
  bool is_buffer = Buffer::HasInstance(val);

  if (is_buffer && (encoding == BUFFER || encoding == BINARY)) {
    return Buffer::Length(val);
  }

  Local<String> str = val->ToString();

  switch (encoding) {
    case BINARY:
    case BUFFER:
    case ASCII:
      data_size = str->Length();
      break;

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
      assert(str->Length() % 2 == 0 && "invalid hex string length");
      data_size = str->Length() / 2;
      break;

    default:
      assert(0 && "unknown encoding");
      break;
  }

  return data_size;
}


size_t StringBytes::Size(Handle<Value> val, enum encoding encoding) {
  HandleScope scope(node_isolate);
  size_t data_size = 0;
  bool is_buffer = Buffer::HasInstance(val);

  if (is_buffer && (encoding == BUFFER || encoding == BINARY))
    return Buffer::Length(val);

  const char* data;
  if (GetExternalParts(val, &data, &data_size))
    return data_size;

  Local<String> str = val->ToString();

  switch (encoding) {
    case BINARY:
    case BUFFER:
    case ASCII:
      data_size = str->Length();
      break;

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
      assert(0 && "unknown encoding");
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


#if defined(__x86_64__) || defined(_WIN64)
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

#if defined(__x86_64__) || defined(_WIN64)
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


static size_t base64_encode(const char* src,
                            size_t slen,
                            char* dst,
                            size_t dlen) {
  // We know how much we'll write, just make sure that there's space.
  assert(dlen >= base64_encoded_size(slen) &&
      "not enough space provided for base64 encode");

  dlen = base64_encoded_size(slen);

  unsigned a;
  unsigned b;
  unsigned c;
  unsigned i;
  unsigned k;
  unsigned n;

  static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                              "abcdefghijklmnopqrstuvwxyz"
                              "0123456789+/";

  i = 0;
  k = 0;
  n = slen / 3 * 3;

  while (i < n) {
    a = src[i + 0] & 0xff;
    b = src[i + 1] & 0xff;
    c = src[i + 2] & 0xff;

    dst[k + 0] = table[a >> 2];
    dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
    dst[k + 2] = table[((b & 0x0f) << 2) | (c >> 6)];
    dst[k + 3] = table[c & 0x3f];

    i += 3;
    k += 4;
  }

  if (n != slen) {
    switch (slen - n) {
      case 1:
        a = src[i + 0] & 0xff;
        dst[k + 0] = table[a >> 2];
        dst[k + 1] = table[(a & 3) << 4];
        dst[k + 2] = '=';
        dst[k + 3] = '=';
        break;

      case 2:
        a = src[i + 0] & 0xff;
        b = src[i + 1] & 0xff;
        dst[k + 0] = table[a >> 2];
        dst[k + 1] = table[((a & 3) << 4) | (b >> 4)];
        dst[k + 2] = table[(b & 0x0f) << 2];
        dst[k + 3] = '=';
        break;
    }
  }

  return dlen;
}


static size_t hex_encode(const char* src, size_t slen, char* dst, size_t dlen) {
  // We know how much we'll write, just make sure that there's space.
  assert(dlen >= slen * 2 &&
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



Local<Value> StringBytes::Encode(const char* buf,
                                 size_t buflen,
                                 enum encoding encoding) {
  HandleScope scope(node_isolate);

  assert(buflen <= Buffer::kMaxLength);
  if (!buflen && encoding != BUFFER)
    return scope.Close(String::Empty(node_isolate));

  Local<String> val;
  switch (encoding) {
    case BUFFER:
      return scope.Close(Buffer::New(buf, buflen));

    case ASCII:
      if (contains_non_ascii(buf, buflen)) {
        char* out = new char[buflen];
        force_ascii(buf, out, buflen);
        if (buflen < EXTERN_APEX) {
          val = OneByteString(node_isolate, out, buflen);
          delete[] out;
        } else {
          val = ExternOneByteString::New(out, buflen);
        }
      } else {
        if (buflen < EXTERN_APEX)
          val = OneByteString(node_isolate, buf, buflen);
        else
          val = ExternOneByteString::NewFromCopy(buf, buflen);
      }
      break;

    case UTF8:
      val = String::NewFromUtf8(node_isolate,
                                buf,
                                String::kNormalString,
                                buflen);
      break;

    case BINARY:
      if (buflen < EXTERN_APEX)
        val = OneByteString(node_isolate, buf, buflen);
      else
        val = ExternOneByteString::NewFromCopy(buf, buflen);
      break;

    case BASE64: {
      size_t dlen = base64_encoded_size(buflen);
      char* dst = new char[dlen];

      size_t written = base64_encode(buf, buflen, dst, dlen);
      assert(written == dlen);

      if (dlen < EXTERN_APEX) {
        val = OneByteString(node_isolate, dst, dlen);
        delete[] dst;
      } else {
        val = ExternOneByteString::New(dst, dlen);
      }
      break;
    }

    case UCS2: {
      const uint16_t* out = reinterpret_cast<const uint16_t*>(buf);
      if (buflen < EXTERN_APEX)
        val = String::NewFromTwoByte(node_isolate,
                                     out,
                                     String::kNormalString,
                                     buflen / 2);
      else
        val = ExternTwoByteString::NewFromCopy(out, buflen / 2);
      break;
    }

    case HEX: {
      size_t dlen = buflen * 2;
      char* dst = new char[dlen];
      size_t written = hex_encode(buf, buflen, dst, dlen);
      assert(written == dlen);

      if (dlen < EXTERN_APEX) {
        val = OneByteString(node_isolate, dst, dlen);
        delete[] dst;
      } else {
        val = ExternOneByteString::New(dst, dlen);
      }
      break;
    }

    default:
      assert(0 && "unknown encoding");
      break;
  }

  return scope.Close(val);
}

}  // namespace node
