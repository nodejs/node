#include "string_bytes.h"

#include "node.h"
#include "node_buffer.h"
#include "v8.h"

#include <limits.h>
#include <string.h>  // memcpy

// When creating strings >= this length v8's gc spins up and consumes
// most of the execution time. For these cases it's more performant to
// use external string resources.
#define EXTERN_APEX 0xFBEE9

namespace node {

using v8::EscapableHandleScope;
using v8::HandleScope;
using v8::Isolate;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;
using v8::MaybeLocal;


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

    static Local<String> NewFromCopy(Isolate* isolate,
                                     const TypeName* data,
                                     size_t length) {
      EscapableHandleScope scope(isolate);

      if (length == 0)
        return scope.Escape(String::Empty(isolate));

      TypeName* new_data =
          static_cast<TypeName*>(malloc(length * sizeof(*new_data)));
      if (new_data == nullptr) {
        return Local<String>();
      }
      memcpy(new_data, data, length * sizeof(*new_data));

      return scope.Escape(ExternString<ResourceType, TypeName>::New(isolate,
                                                                    new_data,
                                                                    length));
    }

    // uses "data" for external resource, and will be free'd on gc
    static Local<String> New(Isolate* isolate,
                             const TypeName* data,
                             size_t length) {
      EscapableHandleScope scope(isolate);

      if (length == 0)
        return scope.Escape(String::Empty(isolate));

      ExternString* h_str = new ExternString<ResourceType, TypeName>(isolate,
                                                                     data,
                                                                     length);
      MaybeLocal<String> str = String::NewExternal(isolate, h_str);
      isolate->AdjustAmountOfExternalAllocatedMemory(h_str->byte_length());

      if (str.IsEmpty()) {
        delete h_str;
        return Local<String>();
      }

      return scope.Escape(str.ToLocalChecked());
    }

    inline Isolate* isolate() const { return isolate_; }

  private:
    ExternString(Isolate* isolate, const TypeName* data, size_t length)
      : isolate_(isolate), data_(data), length_(length) { }
    Isolate* isolate_;
    const TypeName* data_;
    size_t length_;
};


typedef ExternString<String::ExternalOneByteStringResource,
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
static const int8_t unbase64_table[] =
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
#define unbase64(x)                                                           \
  static_cast<uint8_t>(unbase64_table[static_cast<uint8_t>(x)])


template <typename TypeName>
size_t base64_decode_slow(char* dst, size_t dstlen,
                          const TypeName* src, size_t srclen) {
  uint8_t hi;
  uint8_t lo;
  size_t i = 0;
  size_t k = 0;
  for (;;) {
#define V(expr)                                                               \
    while (i < srclen) {                                                      \
      const uint8_t c = src[i];                                               \
      lo = unbase64(c);                                                       \
      i += 1;                                                                 \
      if (lo < 64)                                                            \
        break;  /* Legal character. */                                        \
      if (c == '=')                                                           \
        return k;                                                             \
    }                                                                         \
    expr;                                                                     \
    if (i >= srclen)                                                          \
      return k;                                                               \
    if (k >= dstlen)                                                          \
      return k;                                                               \
    hi = lo;
    V(/* Nothing. */);
    V(dst[k++] = ((hi & 0x3F) << 2) | ((lo & 0x30) >> 4));
    V(dst[k++] = ((hi & 0x0F) << 4) | ((lo & 0x3C) >> 2));
    V(dst[k++] = ((hi & 0x03) << 6) | ((lo & 0x3F) >> 0));
#undef V
  }
  UNREACHABLE();
}


template <typename TypeName>
size_t base64_decode_fast(char* const dst, const size_t dstlen,
                          const TypeName* const src, const size_t srclen,
                          const size_t decoded_size) {
  const size_t available = dstlen < decoded_size ? dstlen : decoded_size;
  const size_t max_i = srclen / 4 * 4;
  const size_t max_k = available / 3 * 3;
  size_t i = 0;
  size_t k = 0;
  while (i < max_i && k < max_k) {
    const uint32_t v =
        unbase64(src[i + 0]) << 24 |
        unbase64(src[i + 1]) << 16 |
        unbase64(src[i + 2]) << 8 |
        unbase64(src[i + 3]);
    // If MSB is set, input contains whitespace or is not valid base64.
    if (v & 0x80808080) {
      break;
    }
    dst[k + 0] = ((v >> 22) & 0xFC) | ((v >> 20) & 0x03);
    dst[k + 1] = ((v >> 12) & 0xF0) | ((v >> 10) & 0x0F);
    dst[k + 2] = ((v >>  2) & 0xC0) | ((v >>  0) & 0x3F);
    i += 4;
    k += 3;
  }
  if (i < srclen && k < dstlen) {
    return k + base64_decode_slow(dst + k, dstlen - k, src + i, srclen - i);
  }
  return k;
}


template <typename TypeName>
size_t base64_decode(char* const dst, const size_t dstlen,
                     const TypeName* const src, const size_t srclen) {
  const size_t decoded_size = base64_decoded_size(src, srclen);
  return base64_decode_fast(dst, dstlen, src, srclen, decoded_size);
}


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


bool StringBytes::GetExternalParts(Isolate* isolate,
                                   Local<Value> val,
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
                              size_t nbytes,
                              const char* data,
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
  ASSERT_EQ(reinterpret_cast<uintptr_t>(aligned_dst) % sizeof(*dst), 0);

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
  const bool is_extern = GetExternalParts(isolate, val, &data, &nbytes);
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
    case BINARY:
    case BUFFER:
      if (is_extern && str->IsOneByte()) {
        memcpy(buf, data, nbytes);
      } else {
        uint8_t* const dst = reinterpret_cast<uint8_t*>(buf);
        nbytes = str->WriteOneByte(dst, 0, buflen, flags);
      }
      if (chars_written != nullptr)
        *chars_written = nbytes;
      break;

    case UTF8:
      nbytes = str->WriteUtf8(buf, buflen, chars_written, flags);
      break;

    case UCS2: {
      size_t nchars;

      if (is_extern && !str->IsOneByte()) {
        memcpy(buf, data, nbytes);
        nchars = nbytes / sizeof(uint16_t);
      } else {
        nbytes = WriteUCS2(buf, buflen, nbytes, data, str, flags, &nchars);
      }
      if (chars_written != nullptr)
        *chars_written = nchars;

      if (!IsBigEndian())
        break;

      // Node's "ucs2" encoding wants LE character data stored in
      // the Buffer, so we need to reorder on BE platforms.  See
      // http://nodejs.org/api/buffer.html regarding Node's "ucs2"
      // encoding specification

      const bool is_aligned =
          reinterpret_cast<uintptr_t>(buf) % sizeof(uint16_t);
      if (is_aligned) {
        uint16_t* const dst = reinterpret_cast<uint16_t*>(buf);
        for (size_t i = 0; i < nchars; i++)
          dst[i] = dst[i] << 8 | dst[i] >> 8;
        break;
      }

      ASSERT_EQ(sizeof(uint16_t), 2);
      for (size_t i = 0; i < nchars; i++) {
        char tmp = buf[i * 2];
        buf[i * 2] = buf[i * 2 + 1];
        buf[i * 2 + 1] = tmp;
      }
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


bool StringBytes::IsValidString(Isolate* isolate,
                                Local<String> string,
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

  if (is_buffer && (encoding == BUFFER || encoding == BINARY)) {
    return Buffer::Length(val);
  }

  Local<String> str = val->ToString(isolate);

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

  if (is_buffer && (encoding == BUFFER || encoding == BINARY))
    return Buffer::Length(val);

  const char* data;
  if (GetExternalParts(isolate, val, &data, &data_size))
    return data_size;

  Local<String> str = val->ToString(isolate);

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


static size_t base64_encode(const char* src,
                            size_t slen,
                            char* dst,
                            size_t dlen) {
  // We know how much we'll write, just make sure that there's space.
  CHECK(dlen >= base64_encoded_size(slen) &&
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



Local<Value> StringBytes::Encode(Isolate* isolate,
                                 const char* buf,
                                 size_t buflen,
                                 enum encoding encoding) {
  EscapableHandleScope scope(isolate);

  CHECK_NE(encoding, UCS2);
  CHECK_LE(buflen, Buffer::kMaxLength);
  if (!buflen && encoding != BUFFER)
    return scope.Escape(String::Empty(isolate));

  Local<String> val;
  switch (encoding) {
    case BUFFER:
      {
        Local<Object> vbuf =
            Buffer::Copy(isolate, buf, buflen).ToLocalChecked();
        return scope.Escape(vbuf);
      }

    case ASCII:
      if (contains_non_ascii(buf, buflen)) {
        char* out = static_cast<char*>(malloc(buflen));
        if (out == nullptr) {
          return Local<String>();
        }
        force_ascii(buf, out, buflen);
        if (buflen < EXTERN_APEX) {
          val = OneByteString(isolate, out, buflen);
          free(out);
        } else {
          val = ExternOneByteString::New(isolate, out, buflen);
        }
      } else {
        if (buflen < EXTERN_APEX)
          val = OneByteString(isolate, buf, buflen);
        else
          val = ExternOneByteString::NewFromCopy(isolate, buf, buflen);
      }
      break;

    case UTF8:
      val = String::NewFromUtf8(isolate,
                                buf,
                                String::kNormalString,
                                buflen);
      break;

    case BINARY:
      if (buflen < EXTERN_APEX)
        val = OneByteString(isolate, buf, buflen);
      else
        val = ExternOneByteString::NewFromCopy(isolate, buf, buflen);
      break;

    case BASE64: {
      size_t dlen = base64_encoded_size(buflen);
      char* dst = static_cast<char*>(malloc(dlen));
      if (dst == nullptr) {
        return Local<String>();
      }

      size_t written = base64_encode(buf, buflen, dst, dlen);
      CHECK_EQ(written, dlen);

      if (dlen < EXTERN_APEX) {
        val = OneByteString(isolate, dst, dlen);
        free(dst);
      } else {
        val = ExternOneByteString::New(isolate, dst, dlen);
      }
      break;
    }

    case HEX: {
      size_t dlen = buflen * 2;
      char* dst = static_cast<char*>(malloc(dlen));
      if (dst == nullptr) {
        return Local<String>();
      }
      size_t written = hex_encode(buf, buflen, dst, dlen);
      CHECK_EQ(written, dlen);

      if (dlen < EXTERN_APEX) {
        val = OneByteString(isolate, dst, dlen);
        free(dst);
      } else {
        val = ExternOneByteString::New(isolate, dst, dlen);
      }
      break;
    }

    default:
      CHECK(0 && "unknown encoding");
      break;
  }

  return scope.Escape(val);
}


Local<Value> StringBytes::Encode(Isolate* isolate,
                                 const uint16_t* buf,
                                 size_t buflen) {
  Local<String> val;

  if (buflen < EXTERN_APEX) {
    val = String::NewFromTwoByte(isolate,
                                 buf,
                                 String::kNormalString,
                                 buflen);
  } else {
    val = ExternTwoByteString::NewFromCopy(isolate, buf, buflen);
  }

  return val;
}

}  // namespace node
