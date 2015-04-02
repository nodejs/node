#include "util.h"
#include "string_bytes.h"

#define UNI_SUR_HIGH_START    0xD800UL
#define UNI_SUR_LOW_END       0xDFFFUL
#define UNI_REPLACEMENT_CHAR  0x0000FFFDUL
#define UNI_MAX_LEGAL_UTF32   0x0010FFFFUL

inline uint32_t log2(uint8_t v) {
  const uint32_t r = (v > 15) << 2;
  v >>= r;
  const uint32_t s = (v > 3) << 1;
  v >>= s;
  v >>= 1;
  return r | s | v;
}

inline uint32_t clz(uint8_t v) {
  return 7 - log2(v);
}

namespace node {

typedef size_t (*ErrorStrategy)(size_t, const uint8_t*, size_t);
typedef void (*GlyphStrategy)(const uint8_t*, size_t, uint32_t, size_t);


inline size_t Skip(
    const size_t remaining,
    const uint8_t* input,
    const size_t glyph_size) {
  if (remaining > glyph_size) {
    return 1;
  }
  return 0;
}


inline size_t Halt(
    const size_t remaining,
    const uint8_t* input,
    const size_t glyph_size) {
  return 0;
}


inline void DiscardGlyph(
    const uint8_t* glyph,
    const size_t glyph_size,
    const uint32_t glyph_value,
    const size_t pos) {
}


inline bool IsLegalUtf8Glyph(const uint8_t* input, const size_t length) {
  uint8_t acc;
  const uint8_t* srcptr = input + length;
  switch (length) {
    default: return false;
    case 4:
      acc = (*--srcptr);
      if (acc < 0x80 || acc > 0xBF)
        return false;
      // fall-through
    case 3:
      acc = (*--srcptr);
      if (acc < 0x80 || acc > 0xBF)
        return false;
      // fall-through
    case 2:
      acc = (*--srcptr);
      if (acc > 0xBF)
        return false;
      switch (*input) {
        case 0xE0:
          if (acc < 0xA0)
            return false;
          break;
        case 0xED:
          if (acc > 0x9F)
            return false;
          break;
        case 0xF0:
          if (acc < 0x90)
            return false;
          break;
        case 0xF4:
          if (acc > 0x8F)
            return false;
          break;
        default:
          if (acc < 0x80)
            return false;
      }
      // fall-through
    case 1:
      if (*input >= 0x80 && *input < 0xC2) {
        return false;
      }
  }
  return *input <= 0xF4;
}


static const uint32_t offsets_from_utf8[6] = {
  0x00000000, 0x00003080, 0x000E2080,
  0x03C82080, 0xFA082080, 0x82082080
};


template <ErrorStrategy OnError, typename GlyphStrategy>
inline size_t Utf8Consume(
    const uint8_t* const input,
    const size_t length,
    const GlyphStrategy OnGlyph) {
  size_t idx = 0;
  while (idx < length) {
    size_t advance = 0;
    uint32_t glyph = 0;
    uint8_t extrabytes = input[idx] ? clz(~input[idx]) : 0;
    size_t i = idx;

    if (extrabytes + idx > length) {
      advance = OnError(length - idx, input, extrabytes);
    } else if (!IsLegalUtf8Glyph(input + idx, extrabytes + 1)) {
      advance = OnError(length - idx, input, extrabytes);
    } else {
      ASSERT(extrabytes < 4);
      switch (extrabytes) {
        case 3:
          glyph += input[i++];
          glyph <<= 6;
          // fall-through
        case 2:
          glyph += input[i++];
          glyph <<= 6;
          // fall-through
        case 1:
          glyph += input[i++];
          glyph <<= 6;
          // fall-through
        case 0:
          glyph += input[i];
      }

      glyph -= offsets_from_utf8[extrabytes];

      if (glyph > UNI_MAX_LEGAL_UTF32 ||
          (glyph >= UNI_SUR_HIGH_START && glyph <= UNI_SUR_LOW_END)) {
        advance = OnError(length - idx, input, extrabytes);
      } else {
        advance = extrabytes + 1;
        OnGlyph(input + idx, extrabytes + 1, glyph, idx);
      }
    }

    if (advance == 0) {
      break;
    }
    idx += advance;
  }
  return idx;
}


size_t Utf8Value::StripInvalidUtf8Glyphs(uint8_t* const input, const size_t size) {
  size_t idx = 0;
  auto on_glyph = [input, &idx](
      const uint8_t* data, size_t size, uint32_t glyph, size_t pos) {
    size_t old_idx = idx;
    idx += size;
    if (old_idx == pos) return;
    memmove(input + old_idx, data, size);
  };

  return Utf8Consume<Skip>(input, size, on_glyph);
}


bool Utf8Value::IsValidUtf8(const uint8_t * const input, const size_t size) {
  return Utf8Consume<Halt>(input, size, DiscardGlyph) == size;
}


Utf8Value::Utf8Value(v8::Isolate* isolate, v8::Handle<v8::Value> value)
    : length_(0), str_(str_st_) {
  if (value.IsEmpty())
    return;

  v8::Local<v8::String> string = value->ToString(isolate);
  if (string.IsEmpty())
    return;

  // Allocate enough space to include the null terminator
  size_t len = StringBytes::StorageSize(string, UTF8) + 1;
  if (len > sizeof(str_st_)) {
    str_ = static_cast<char*>(malloc(len));
    CHECK_NE(str_, nullptr);
  }

  const int flags =
      v8::String::NO_NULL_TERMINATION | v8::String::REPLACE_INVALID_UTF8;
  length_ = string->WriteUtf8(str_, len, 0, flags);
  str_[length_] = '\0';
}

}  // namespace node
