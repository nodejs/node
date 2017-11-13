#include "node_url.h"
#include "node_internals.h"
#include "base-object-inl.h"
#include "node_i18n.h"

#include <string>
#include <vector>
#include <stdio.h>
#include <cmath>

namespace node {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Null;
using v8::Object;
using v8::String;
using v8::TryCatch;
using v8::Undefined;
using v8::Value;

#define GET(env, obj, name)                                                   \
  obj->Get(env->context(),                                                    \
           OneByteString(env->isolate(), name)).ToLocalChecked()

#define GET_AND_SET(env, obj, name, data, flag)                               \
  {                                                                           \
    Local<Value> val = GET(env, obj, #name);                                  \
    if (val->IsString()) {                                                    \
      Utf8Value value(env->isolate(), val.As<String>());                      \
      data->name = *value;                                                    \
      data->flags |= flag;                                                    \
    }                                                                         \
  }

#define UTF8STRING(isolate, str)                                              \
  String::NewFromUtf8(isolate, str.c_str(), v8::NewStringType::kNormal)       \
    .ToLocalChecked()

namespace url {

// https://url.spec.whatwg.org/#eof-code-point
static const char kEOL = -1;

// Used in ToUSVString().
static const char16_t kUnicodeReplacementCharacter = 0xFFFD;

// https://url.spec.whatwg.org/#concept-host
union url_host_value {
  std::string domain;
  uint32_t ipv4;
  uint16_t ipv6[8];
  std::string opaque;
  ~url_host_value() {}
};

enum url_host_type {
  HOST_TYPE_FAILED = -1,
  HOST_TYPE_DOMAIN = 0,
  HOST_TYPE_IPV4 = 1,
  HOST_TYPE_IPV6 = 2,
  HOST_TYPE_OPAQUE = 3,
};

struct url_host {
  url_host_value value;
  enum url_host_type type;
};

#define ARGS(XX)                                                              \
  XX(ARG_FLAGS)                                                               \
  XX(ARG_PROTOCOL)                                                            \
  XX(ARG_USERNAME)                                                            \
  XX(ARG_PASSWORD)                                                            \
  XX(ARG_HOST)                                                                \
  XX(ARG_PORT)                                                                \
  XX(ARG_PATH)                                                                \
  XX(ARG_QUERY)                                                               \
  XX(ARG_FRAGMENT)

#define ERR_ARGS(XX)                                                          \
  XX(ERR_ARG_FLAGS)                                                           \
  XX(ERR_ARG_INPUT)                                                           \

enum url_cb_args {
#define XX(name) name,
  ARGS(XX)
#undef XX
};

enum url_error_cb_args {
#define XX(name) name,
  ERR_ARGS(XX)
#undef XX
};

#define CHAR_TEST(bits, name, expr)                                           \
  template <typename T>                                                       \
  static inline bool name(const T ch) {                                       \
    static_assert(sizeof(ch) >= (bits) / 8,                                   \
                  "Character must be wider than " #bits " bits");             \
    return (expr);                                                            \
  }

#define TWO_CHAR_STRING_TEST(bits, name, expr)                                \
  template <typename T>                                                       \
  static inline bool name(const T ch1, const T ch2) {                         \
    static_assert(sizeof(ch1) >= (bits) / 8,                                  \
                  "Character must be wider than " #bits " bits");             \
    return (expr);                                                            \
  }                                                                           \
  template <typename T>                                                       \
  static inline bool name(const std::basic_string<T>& str) {                  \
    static_assert(sizeof(str[0]) >= (bits) / 8,                               \
                  "Character must be wider than " #bits " bits");             \
    return str.length() >= 2 && name(str[0], str[1]);                         \
  }

// https://infra.spec.whatwg.org/#ascii-tab-or-newline
CHAR_TEST(8, IsASCIITabOrNewline, (ch == '\t' || ch == '\n' || ch == '\r'))

// https://infra.spec.whatwg.org/#c0-control-or-space
CHAR_TEST(8, IsC0ControlOrSpace, (ch >= '\0' && ch <= ' '))

// https://infra.spec.whatwg.org/#ascii-digit
CHAR_TEST(8, IsASCIIDigit, (ch >= '0' && ch <= '9'))

// https://infra.spec.whatwg.org/#ascii-hex-digit
CHAR_TEST(8, IsASCIIHexDigit, (IsASCIIDigit(ch) ||
                               (ch >= 'A' && ch <= 'F') ||
                               (ch >= 'a' && ch <= 'f')))

// https://infra.spec.whatwg.org/#ascii-alpha
CHAR_TEST(8, IsASCIIAlpha, ((ch >= 'A' && ch <= 'Z') ||
                            (ch >= 'a' && ch <= 'z')))

// https://infra.spec.whatwg.org/#ascii-alphanumeric
CHAR_TEST(8, IsASCIIAlphanumeric, (IsASCIIDigit(ch) || IsASCIIAlpha(ch)))

// https://infra.spec.whatwg.org/#ascii-lowercase
template <typename T>
static inline T ASCIILowercase(T ch) {
  return IsASCIIAlpha(ch) ? (ch | 0x20) : ch;
}

// https://url.spec.whatwg.org/#forbidden-host-code-point
CHAR_TEST(8, IsForbiddenHostCodePoint,
          ch == '\0' || ch == '\t' || ch == '\n' || ch == '\r' ||
          ch == ' ' || ch == '#' || ch == '%' || ch == '/' ||
          ch == ':' || ch == '?' || ch == '@' || ch == '[' ||
          ch == '\\' || ch == ']')

// https://url.spec.whatwg.org/#windows-drive-letter
TWO_CHAR_STRING_TEST(8, IsWindowsDriveLetter,
                     (IsASCIIAlpha(ch1) && (ch2 == ':' || ch2 == '|')))

// https://url.spec.whatwg.org/#normalized-windows-drive-letter
TWO_CHAR_STRING_TEST(8, IsNormalizedWindowsDriveLetter,
                     (IsASCIIAlpha(ch1) && ch2 == ':'))

// If a UTF-16 character is a low/trailing surrogate.
CHAR_TEST(16, IsUnicodeTrail, (ch & 0xFC00) == 0xDC00)

// If a UTF-16 character is a surrogate.
CHAR_TEST(16, IsUnicodeSurrogate, (ch & 0xF800) == 0xD800)

// If a UTF-16 surrogate is a low/trailing one.
CHAR_TEST(16, IsUnicodeSurrogateTrail, (ch & 0x400) != 0)

#undef CHAR_TEST
#undef TWO_CHAR_STRING_TEST

static const char* hex[256] = {
  "%00", "%01", "%02", "%03", "%04", "%05", "%06", "%07",
  "%08", "%09", "%0A", "%0B", "%0C", "%0D", "%0E", "%0F",
  "%10", "%11", "%12", "%13", "%14", "%15", "%16", "%17",
  "%18", "%19", "%1A", "%1B", "%1C", "%1D", "%1E", "%1F",
  "%20", "%21", "%22", "%23", "%24", "%25", "%26", "%27",
  "%28", "%29", "%2A", "%2B", "%2C", "%2D", "%2E", "%2F",
  "%30", "%31", "%32", "%33", "%34", "%35", "%36", "%37",
  "%38", "%39", "%3A", "%3B", "%3C", "%3D", "%3E", "%3F",
  "%40", "%41", "%42", "%43", "%44", "%45", "%46", "%47",
  "%48", "%49", "%4A", "%4B", "%4C", "%4D", "%4E", "%4F",
  "%50", "%51", "%52", "%53", "%54", "%55", "%56", "%57",
  "%58", "%59", "%5A", "%5B", "%5C", "%5D", "%5E", "%5F",
  "%60", "%61", "%62", "%63", "%64", "%65", "%66", "%67",
  "%68", "%69", "%6A", "%6B", "%6C", "%6D", "%6E", "%6F",
  "%70", "%71", "%72", "%73", "%74", "%75", "%76", "%77",
  "%78", "%79", "%7A", "%7B", "%7C", "%7D", "%7E", "%7F",
  "%80", "%81", "%82", "%83", "%84", "%85", "%86", "%87",
  "%88", "%89", "%8A", "%8B", "%8C", "%8D", "%8E", "%8F",
  "%90", "%91", "%92", "%93", "%94", "%95", "%96", "%97",
  "%98", "%99", "%9A", "%9B", "%9C", "%9D", "%9E", "%9F",
  "%A0", "%A1", "%A2", "%A3", "%A4", "%A5", "%A6", "%A7",
  "%A8", "%A9", "%AA", "%AB", "%AC", "%AD", "%AE", "%AF",
  "%B0", "%B1", "%B2", "%B3", "%B4", "%B5", "%B6", "%B7",
  "%B8", "%B9", "%BA", "%BB", "%BC", "%BD", "%BE", "%BF",
  "%C0", "%C1", "%C2", "%C3", "%C4", "%C5", "%C6", "%C7",
  "%C8", "%C9", "%CA", "%CB", "%CC", "%CD", "%CE", "%CF",
  "%D0", "%D1", "%D2", "%D3", "%D4", "%D5", "%D6", "%D7",
  "%D8", "%D9", "%DA", "%DB", "%DC", "%DD", "%DE", "%DF",
  "%E0", "%E1", "%E2", "%E3", "%E4", "%E5", "%E6", "%E7",
  "%E8", "%E9", "%EA", "%EB", "%EC", "%ED", "%EE", "%EF",
  "%F0", "%F1", "%F2", "%F3", "%F4", "%F5", "%F6", "%F7",
  "%F8", "%F9", "%FA", "%FB", "%FC", "%FD", "%FE", "%FF"
};

static const uint8_t C0_CONTROL_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static const uint8_t PATH_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x80,
  // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x00 | 0x20 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static const uint8_t USERINFO_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 40     41     42     43     44     45     46     47
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x40 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x01 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x08 | 0x10 | 0x20 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static const uint8_t QUERY_ENCODE_SET[32] = {
  // 00     01     02     03     04     05     06     07
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 08     09     0A     0B     0C     0D     0E     0F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 10     11     12     13     14     15     16     17
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 18     19     1A     1B     1C     1D     1E     1F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 20     21     22     23     24     25     26     27
    0x01 | 0x00 | 0x04 | 0x08 | 0x00 | 0x00 | 0x00 | 0x00,
  // 28     29     2A     2B     2C     2D     2E     2F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 30     31     32     33     34     35     36     37
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 38     39     3A     3B     3C     3D     3E     3F
    0x00 | 0x00 | 0x00 | 0x00 | 0x10 | 0x00 | 0x40 | 0x00,
  // 40     41     42     43     44     45     46     47
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 48     49     4A     4B     4C     4D     4E     4F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 50     51     52     53     54     55     56     57
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 58     59     5A     5B     5C     5D     5E     5F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 60     61     62     63     64     65     66     67
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 68     69     6A     6B     6C     6D     6E     6F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 70     71     72     73     74     75     76     77
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00,
  // 78     79     7A     7B     7C     7D     7E     7F
    0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x00 | 0x80,
  // 80     81     82     83     84     85     86     87
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 88     89     8A     8B     8C     8D     8E     8F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 90     91     92     93     94     95     96     97
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // 98     99     9A     9B     9C     9D     9E     9F
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A0     A1     A2     A3     A4     A5     A6     A7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // A8     A9     AA     AB     AC     AD     AE     AF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B0     B1     B2     B3     B4     B5     B6     B7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // B8     B9     BA     BB     BC     BD     BE     BF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C0     C1     C2     C3     C4     C5     C6     C7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // C8     C9     CA     CB     CC     CD     CE     CF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D0     D1     D2     D3     D4     D5     D6     D7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // D8     D9     DA     DB     DC     DD     DE     DF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E0     E1     E2     E3     E4     E5     E6     E7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // E8     E9     EA     EB     EC     ED     EE     EF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F0     F1     F2     F3     F4     F5     F6     F7
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80,
  // F8     F9     FA     FB     FC     FD     FE     FF
    0x01 | 0x02 | 0x04 | 0x08 | 0x10 | 0x20 | 0x40 | 0x80
};

static inline bool BitAt(const uint8_t a[], const uint8_t i) {
  return !!(a[i >> 3] & (1 << (i & 7)));
}

// Appends ch to str. If ch position in encode_set is set, the ch will
// be percent-encoded then appended.
static inline void AppendOrEscape(std::string* str,
                                  const unsigned char ch,
                                  const uint8_t encode_set[]) {
  if (BitAt(encode_set, ch))
    *str += hex[ch];
  else
    *str += ch;
}

template <typename T>
static inline unsigned hex2bin(const T ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return 10 + (ch - 'A');
  if (ch >= 'a' && ch <= 'f')
    return 10 + (ch - 'a');
  return static_cast<unsigned>(-1);
}

static inline void PercentDecode(const char* input,
                                 size_t len,
                                 std::string* dest) {
  if (len == 0)
    return;
  dest->reserve(len);
  const char* pointer = input;
  const char* end = input + len;

  while (pointer < end) {
    const char ch = pointer[0];
    const size_t remaining = end - pointer - 1;
    if (ch != '%' || remaining < 2 ||
        (ch == '%' &&
         (!IsASCIIHexDigit(pointer[1]) ||
          !IsASCIIHexDigit(pointer[2])))) {
      *dest += ch;
      pointer++;
      continue;
    } else {
      unsigned a = hex2bin(pointer[1]);
      unsigned b = hex2bin(pointer[2]);
      char c = static_cast<char>(a * 16 + b);
      *dest += c;
      pointer += 3;
    }
  }
}

#define SPECIALS(XX)                                                          \
  XX("ftp:", 21)                                                              \
  XX("file:", -1)                                                             \
  XX("gopher:", 70)                                                           \
  XX("http:", 80)                                                             \
  XX("https:", 443)                                                           \
  XX("ws:", 80)                                                               \
  XX("wss:", 443)

static inline bool IsSpecial(std::string scheme) {
#define XX(name, _) if (scheme == name) return true;
  SPECIALS(XX);
#undef XX
  return false;
}

// https://url.spec.whatwg.org/#start-with-a-windows-drive-letter
static inline bool StartsWithWindowsDriveLetter(const char* p,
                                                const char* end) {
  const size_t length = end - p;
  return length >= 2 &&
    IsWindowsDriveLetter(p[0], p[1]) &&
    (length == 2 ||
      p[2] == '/' ||
      p[2] == '\\' ||
      p[2] == '?' ||
      p[2] == '#');
}

static inline int NormalizePort(std::string scheme, int p) {
#define XX(name, port) if (scheme == name && p == port) return -1;
  SPECIALS(XX);
#undef XX
  return p;
}

#if defined(NODE_HAVE_I18N_SUPPORT)
static inline bool ToUnicode(const std::string& input, std::string* output) {
  MaybeStackBuffer<char> buf;
  if (i18n::ToUnicode(&buf, input.c_str(), input.length()) < 0)
    return false;
  output->assign(*buf, buf.length());
  return true;
}

static inline bool ToASCII(const std::string& input, std::string* output) {
  MaybeStackBuffer<char> buf;
  if (i18n::ToASCII(&buf, input.c_str(), input.length()) < 0)
    return false;
  output->assign(*buf, buf.length());
  return true;
}
#else
// Intentional non-ops if ICU is not present.
static inline bool ToUnicode(const std::string& input, std::string* output) {
  *output = input;
  return true;
}

static inline bool ToASCII(const std::string& input, std::string* output) {
  *output = input;
  return true;
}
#endif

static url_host_type ParseIPv6Host(url_host* host,
                                   const char* input,
                                   size_t length) {
  url_host_type type = HOST_TYPE_FAILED;
  for (unsigned n = 0; n < 8; n++)
    host->value.ipv6[n] = 0;
  uint16_t* piece_pointer = &host->value.ipv6[0];
  uint16_t* last_piece = piece_pointer + 8;
  uint16_t* compress_pointer = nullptr;
  const char* pointer = input;
  const char* end = pointer + length;
  unsigned value, len, swaps, numbers_seen;
  char ch = pointer < end ? pointer[0] : kEOL;
  if (ch == ':') {
    if (length < 2 || pointer[1] != ':')
      goto end;
    pointer += 2;
    ch = pointer < end ? pointer[0] : kEOL;
    piece_pointer++;
    compress_pointer = piece_pointer;
  }
  while (ch != kEOL) {
    if (piece_pointer > last_piece)
      goto end;
    if (ch == ':') {
      if (compress_pointer != nullptr)
        goto end;
      pointer++;
      ch = pointer < end ? pointer[0] : kEOL;
      piece_pointer++;
      compress_pointer = piece_pointer;
      continue;
    }
    value = 0;
    len = 0;
    while (len < 4 && IsASCIIHexDigit(ch)) {
      value = value * 0x10 + hex2bin(ch);
      pointer++;
      ch = pointer < end ? pointer[0] : kEOL;
      len++;
    }
    switch (ch) {
      case '.':
        if (len == 0)
          goto end;
        pointer -= len;
        ch = pointer < end ? pointer[0] : kEOL;
        if (piece_pointer > last_piece - 2)
          goto end;
        numbers_seen = 0;
        while (ch != kEOL) {
          value = 0xffffffff;
          if (numbers_seen > 0) {
            if (ch == '.' && numbers_seen < 4) {
              pointer++;
              ch = pointer < end ? pointer[0] : kEOL;
            } else {
              goto end;
            }
          }
          if (!IsASCIIDigit(ch))
            goto end;
          while (IsASCIIDigit(ch)) {
            unsigned number = ch - '0';
            if (value == 0xffffffff) {
              value = number;
            } else if (value == 0) {
              goto end;
            } else {
              value = value * 10 + number;
            }
            if (value > 255)
              goto end;
            pointer++;
            ch = pointer < end ? pointer[0] : kEOL;
          }
          *piece_pointer = *piece_pointer * 0x100 + value;
          numbers_seen++;
          if (numbers_seen == 2 || numbers_seen == 4)
            piece_pointer++;
        }
        if (numbers_seen != 4)
          goto end;
        continue;
      case ':':
        pointer++;
        ch = pointer < end ? pointer[0] : kEOL;
        if (ch == kEOL)
          goto end;
        break;
      case kEOL:
        break;
      default:
        goto end;
    }
    *piece_pointer = value;
    piece_pointer++;
  }

  if (compress_pointer != nullptr) {
    swaps = piece_pointer - compress_pointer;
    piece_pointer = last_piece - 1;
    while (piece_pointer != &host->value.ipv6[0] && swaps > 0) {
      uint16_t temp = *piece_pointer;
      uint16_t* swap_piece = compress_pointer + swaps - 1;
      *piece_pointer = *swap_piece;
      *swap_piece = temp;
       piece_pointer--;
       swaps--;
    }
  } else if (compress_pointer == nullptr &&
             piece_pointer != last_piece) {
    goto end;
  }
  type = HOST_TYPE_IPV6;
 end:
  host->type = type;
  return type;
}

static inline int64_t ParseNumber(const char* start, const char* end) {
  unsigned R = 10;
  if (end - start >= 2 && start[0] == '0' && (start[1] | 0x20) == 'x') {
    start += 2;
    R = 16;
  }
  if (end - start == 0) {
    return 0;
  } else if (R == 10 && end - start > 1 && start[0] == '0') {
    start++;
    R = 8;
  }
  const char* p = start;

  while (p < end) {
    const char ch = p[0];
    switch (R) {
      case 8:
        if (ch < '0' || ch > '7')
          return -1;
        break;
      case 10:
        if (!IsASCIIDigit(ch))
          return -1;
        break;
      case 16:
        if (!IsASCIIHexDigit(ch))
          return -1;
        break;
    }
    p++;
  }
  return strtoll(start, NULL, R);
}

static url_host_type ParseIPv4Host(url_host* host,
                                   const char* input,
                                   size_t length) {
  url_host_type type = HOST_TYPE_DOMAIN;
  const char* pointer = input;
  const char* mark = input;
  const char* end = pointer + length;
  int parts = 0;
  uint32_t val = 0;
  uint64_t numbers[4];
  int tooBigNumbers = 0;
  if (length == 0)
    goto end;

  while (pointer <= end) {
    const char ch = pointer < end ? pointer[0] : kEOL;
    const int remaining = end - pointer - 1;
    if (ch == '.' || ch == kEOL) {
      if (++parts > 4)
        goto end;
      if (pointer == mark)
        goto end;
      int64_t n = ParseNumber(mark, pointer);
      if (n < 0)
        goto end;

      if (n > 255) {
        tooBigNumbers++;
      }
      numbers[parts - 1] = n;
      mark = pointer + 1;
      if (ch == '.' && remaining == 0)
        break;
    }
    pointer++;
  }
  CHECK_GT(parts, 0);

  // If any but the last item in numbers is greater than 255, return failure.
  // If the last item in numbers is greater than or equal to
  // 256^(5 - the number of items in numbers), return failure.
  if (tooBigNumbers > 1 ||
      (tooBigNumbers == 1 && numbers[parts - 1] <= 255) ||
      numbers[parts - 1] >= pow(256, static_cast<double>(5 - parts))) {
    type = HOST_TYPE_FAILED;
    goto end;
  }

  type = HOST_TYPE_IPV4;
  val = numbers[parts - 1];
  for (int n = 0; n < parts - 1; n++) {
    double b = 3 - n;
    val += numbers[n] * pow(256, b);
  }

  host->value.ipv4 = val;
 end:
  host->type = type;
  return type;
}

static url_host_type ParseOpaqueHost(url_host* host,
                                     const char* input,
                                     size_t length) {
  url_host_type type = HOST_TYPE_OPAQUE;
  std::string output;
  output.reserve(length * 3);
  for (size_t i = 0; i < length; i++) {
    const char ch = input[i];
    if (ch != '%' && IsForbiddenHostCodePoint(ch)) {
      type = HOST_TYPE_FAILED;
      goto end;
    } else {
      AppendOrEscape(&output, ch, C0_CONTROL_ENCODE_SET);
    }
  }

  host->value.opaque = output;
 end:
  host->type = type;
  return type;
}

static url_host_type ParseHost(url_host* host,
                               const char* input,
                               size_t length,
                               bool is_special,
                               bool unicode = false) {
  url_host_type type = HOST_TYPE_FAILED;
  const char* pointer = input;
  std::string decoded;

  if (length == 0)
    goto end;

  if (pointer[0] == '[') {
    if (pointer[length - 1] != ']')
      goto end;
    return ParseIPv6Host(host, ++pointer, length - 2);
  }

  if (!is_special)
    return ParseOpaqueHost(host, input, length);

  // First, we have to percent decode
  PercentDecode(input, length, &decoded);

  // Then we have to punycode toASCII
  if (!ToASCII(decoded, &decoded))
    goto end;

  // If any of the following characters are still present, we have to fail
  for (size_t n = 0; n < decoded.size(); n++) {
    const char ch = decoded[n];
    if (IsForbiddenHostCodePoint(ch)) {
      goto end;
    }
  }

  // Check to see if it's an IPv4 IP address
  type = ParseIPv4Host(host, decoded.c_str(), decoded.length());
  if (type == HOST_TYPE_IPV4 || type == HOST_TYPE_FAILED)
    goto end;

  // If the unicode flag is set, run the result through punycode ToUnicode
  if (unicode && !ToUnicode(decoded, &decoded))
    goto end;

  // It's not an IPv4 or IPv6 address, it must be a domain
  type = HOST_TYPE_DOMAIN;
  host->value.domain = decoded;

 end:
  host->type = type;
  return type;
}

// Locates the longest sequence of 0 segments in an IPv6 address
// in order to use the :: compression when serializing
static inline uint16_t* FindLongestZeroSequence(uint16_t* values,
                                                size_t len) {
  uint16_t* start = values;
  uint16_t* end = start + len;
  uint16_t* result = nullptr;

  uint16_t* current = nullptr;
  unsigned counter = 0, longest = 1;

  while (start < end) {
    if (*start == 0) {
      if (current == nullptr)
        current = start;
      counter++;
    } else {
      if (counter > longest) {
        longest = counter;
        result = current;
      }
      counter = 0;
      current = nullptr;
    }
    start++;
  }
  if (counter > longest)
    result = current;
  return result;
}

static url_host_type WriteHost(url_host* host, std::string* dest) {
  dest->clear();
  switch (host->type) {
    case HOST_TYPE_DOMAIN:
      *dest = host->value.domain;
      break;
    case HOST_TYPE_IPV4: {
      dest->reserve(15);
      uint32_t value = host->value.ipv4;
      for (int n = 0; n < 4; n++) {
        char buf[4];
        char* buffer = buf;
        snprintf(buffer, sizeof(buf), "%d", value % 256);
        dest->insert(0, buf);
        if (n < 3)
          dest->insert(0, 1, '.');
        value /= 256;
      }
      break;
    }
    case HOST_TYPE_IPV6: {
      dest->reserve(41);
      *dest+= '[';
      uint16_t* start = &host->value.ipv6[0];
      uint16_t* compress_pointer =
          FindLongestZeroSequence(start, 8);
      bool ignore0 = false;
      for (int n = 0; n <= 7; n++) {
        uint16_t* piece = &host->value.ipv6[n];
        if (ignore0 && *piece == 0)
          continue;
        else if (ignore0)
          ignore0 = false;
        if (compress_pointer == piece) {
          *dest += n == 0 ? "::" : ":";
          ignore0 = true;
          continue;
        }
        char buf[5];
        char* buffer = buf;
        snprintf(buffer, sizeof(buf), "%x", *piece);
        *dest += buf;
        if (n < 7)
          *dest += ':';
      }
      *dest += ']';
      break;
    }
    case HOST_TYPE_OPAQUE:
      *dest = host->value.opaque;
      break;
    case HOST_TYPE_FAILED:
      break;
  }
  return host->type;
}

static bool ParseHost(std::string* input,
                      std::string* output,
                      bool is_special,
                      bool unicode = false) {
  if (input->length() == 0) {
    output->clear();
    return true;
  }
  url_host host{{""}, HOST_TYPE_DOMAIN};
  ParseHost(&host, input->c_str(), input->length(), is_special, unicode);
  if (host.type == HOST_TYPE_FAILED)
    return false;
  WriteHost(&host, output);
  return true;
}

static inline void Copy(Environment* env,
                        Local<Array> ary,
                        std::vector<std::string>* vec) {
  const int32_t len = ary->Length();
  if (len == 0)
    return;  // nothing to copy
  vec->reserve(len);
  for (int32_t n = 0; n < len; n++) {
    Local<Value> val = ary->Get(env->context(), n).ToLocalChecked();
    if (val->IsString()) {
      Utf8Value value(env->isolate(), val.As<String>());
      vec->push_back(std::string(*value, value.length()));
    }
  }
}

static inline Local<Array> Copy(Environment* env,
                                const std::vector<std::string>& vec) {
  Isolate* isolate = env->isolate();
  Local<Array> ary = Array::New(isolate, vec.size());
  for (size_t n = 0; n < vec.size(); n++)
    ary->Set(env->context(), n, UTF8STRING(isolate, vec[n])).FromJust();
  return ary;
}

static inline void HarvestBase(Environment* env,
                               struct url_data* base,
                               Local<Object> base_obj) {
  Local<Context> context = env->context();
  Local<Value> flags = GET(env, base_obj, "flags");
  if (flags->IsInt32())
    base->flags = flags->Int32Value(context).FromJust();

  Local<Value> scheme = GET(env, base_obj, "scheme");
  base->scheme = Utf8Value(env->isolate(), scheme).out();

  GET_AND_SET(env, base_obj, username, base, URL_FLAGS_HAS_USERNAME);
  GET_AND_SET(env, base_obj, password, base, URL_FLAGS_HAS_PASSWORD);
  GET_AND_SET(env, base_obj, host, base, URL_FLAGS_HAS_HOST);
  GET_AND_SET(env, base_obj, query, base, URL_FLAGS_HAS_QUERY);
  GET_AND_SET(env, base_obj, fragment, base, URL_FLAGS_HAS_FRAGMENT);
  Local<Value> port = GET(env, base_obj, "port");
  if (port->IsInt32())
    base->port = port->Int32Value(context).FromJust();
  Local<Value> path = GET(env, base_obj, "path");
  if (path->IsArray()) {
    base->flags |= URL_FLAGS_HAS_PATH;
    Copy(env, path.As<Array>(), &(base->path));
  }
}

static inline void HarvestContext(Environment* env,
                                  struct url_data* context,
                                  Local<Object> context_obj) {
  Local<Value> flags = GET(env, context_obj, "flags");
  if (flags->IsInt32()) {
    int32_t _flags = flags->Int32Value(env->context()).FromJust();
    if (_flags & URL_FLAGS_SPECIAL)
      context->flags |= URL_FLAGS_SPECIAL;
    if (_flags & URL_FLAGS_CANNOT_BE_BASE)
      context->flags |= URL_FLAGS_CANNOT_BE_BASE;
    if (_flags & URL_FLAGS_HAS_USERNAME)
      context->flags |= URL_FLAGS_HAS_USERNAME;
    if (_flags & URL_FLAGS_HAS_PASSWORD)
      context->flags |= URL_FLAGS_HAS_PASSWORD;
    if (_flags & URL_FLAGS_HAS_HOST)
      context->flags |= URL_FLAGS_HAS_HOST;
  }
  Local<Value> scheme = GET(env, context_obj, "scheme");
  if (scheme->IsString()) {
    Utf8Value value(env->isolate(), scheme);
    context->scheme.assign(*value, value.length());
  }
  Local<Value> port = GET(env, context_obj, "port");
  if (port->IsInt32())
    context->port = port->Int32Value(env->context()).FromJust();
  if (context->flags & URL_FLAGS_HAS_USERNAME) {
    Local<Value> username = GET(env, context_obj, "username");
    CHECK(username->IsString());
    Utf8Value value(env->isolate(), username);
    context->username.assign(*value, value.length());
  }
  if (context->flags & URL_FLAGS_HAS_PASSWORD) {
    Local<Value> password = GET(env, context_obj, "password");
    CHECK(password->IsString());
    Utf8Value value(env->isolate(), password);
    context->password.assign(*value, value.length());
  }
  Local<Value> host = GET(env, context_obj, "host");
  if (host->IsString()) {
    Utf8Value value(env->isolate(), host);
    context->host.assign(*value, value.length());
  }
}

// Single dot segment can be ".", "%2e", or "%2E"
static inline bool IsSingleDotSegment(std::string str) {
  switch (str.size()) {
    case 1:
      return str == ".";
    case 3:
      return str[0] == '%' &&
             str[1] == '2' &&
             ASCIILowercase(str[2]) == 'e';
    default:
      return false;
  }
}

// Double dot segment can be:
//   "..", ".%2e", ".%2E", "%2e.", "%2E.",
//   "%2e%2e", "%2E%2E", "%2e%2E", or "%2E%2e"
static inline bool IsDoubleDotSegment(std::string str) {
  switch (str.size()) {
    case 2:
      return str == "..";
    case 4:
      if (str[0] != '.' && str[0] != '%')
        return false;
      return ((str[0] == '.' &&
               str[1] == '%' &&
               str[2] == '2' &&
               ASCIILowercase(str[3]) == 'e') ||
              (str[0] == '%' &&
               str[1] == '2' &&
               ASCIILowercase(str[2]) == 'e' &&
               str[3] == '.'));
    case 6:
      return (str[0] == '%' &&
              str[1] == '2' &&
              ASCIILowercase(str[2]) == 'e' &&
              str[3] == '%' &&
              str[4] == '2' &&
              ASCIILowercase(str[5]) == 'e');
    default:
      return false;
  }
}

static inline void ShortenUrlPath(struct url_data* url) {
  if (url->path.empty()) return;
  if (url->path.size() == 1 && url->scheme == "file:" &&
      IsNormalizedWindowsDriveLetter(url->path[0])) return;
  url->path.pop_back();
}

void URL::Parse(const char* input,
                size_t len,
                enum url_parse_state state_override,
                struct url_data* url,
                bool has_url,
                const struct url_data* base,
                bool has_base) {
  const char* p = input;
  const char* end = input + len;

  if (!has_url) {
    for (const char* ptr = p; ptr < end; ptr++) {
      if (IsC0ControlOrSpace(*ptr))
        p++;
      else
        break;
    }
    for (const char* ptr = end - 1; ptr >= p; ptr--) {
      if (IsC0ControlOrSpace(*ptr))
        end--;
      else
        break;
    }
    len = end - p;
  }

  std::string whitespace_stripped;
  whitespace_stripped.reserve(len);
  for (const char* ptr = p; ptr < end; ptr++)
    if (!IsASCIITabOrNewline(*ptr))
      whitespace_stripped += *ptr;

  input = whitespace_stripped.c_str();
  len = whitespace_stripped.size();
  p = input;
  end = input + len;

  bool atflag = false;
  bool sbflag = false;
  bool uflag = false;

  std::string buffer;
  url->scheme.reserve(len);
  url->username.reserve(len);
  url->password.reserve(len);
  url->host.reserve(len);
  url->path.reserve(len);
  url->query.reserve(len);
  url->fragment.reserve(len);
  buffer.reserve(len);

  // Set the initial parse state.
  const bool has_state_override = state_override != kUnknownState;
  enum url_parse_state state = has_state_override ? state_override :
                                                    kSchemeStart;

  if (state < kSchemeStart || state > kFragment) {
    url->flags |= URL_FLAGS_INVALID_PARSE_STATE;
    return;
  }

  while (p <= end) {
    const char ch = p < end ? p[0] : kEOL;
    bool special = (url->flags & URL_FLAGS_SPECIAL);
    bool cannot_be_base;
    const bool special_back_slash = (special && ch == '\\');

    switch (state) {
      case kSchemeStart:
        if (IsASCIIAlpha(ch)) {
          buffer += ASCIILowercase(ch);
          state = kScheme;
        } else if (!has_state_override) {
          state = kNoScheme;
          continue;
        } else {
          url->flags |= URL_FLAGS_FAILED;
          return;
        }
        break;
      case kScheme:
        if (IsASCIIAlphanumeric(ch) || ch == '+' || ch == '-' || ch == '.') {
          buffer += ASCIILowercase(ch);
        } else if (ch == ':' || (has_state_override && ch == kEOL)) {
          if (has_state_override && buffer.size() == 0) {
            url->flags |= URL_FLAGS_TERMINATED;
            return;
          }
          buffer += ':';

          bool new_is_special = IsSpecial(buffer);

          if (has_state_override) {
            if ((special != new_is_special) ||
                ((buffer == "file:") &&
                 ((url->flags & URL_FLAGS_HAS_USERNAME) ||
                  (url->flags & URL_FLAGS_HAS_PASSWORD) ||
                  (url->port != -1)))) {
              url->flags |= URL_FLAGS_TERMINATED;
              return;
            }

            // File scheme && (host == empty or null) check left to JS-land
            // as it can be done before even entering C++ binding.
          }

          url->scheme = buffer;
          url->port = NormalizePort(url->scheme, url->port);
          if (new_is_special) {
            url->flags |= URL_FLAGS_SPECIAL;
            special = true;
          } else {
            url->flags &= ~URL_FLAGS_SPECIAL;
            special = false;
          }
          buffer.clear();
          if (has_state_override)
            return;
          if (url->scheme == "file:") {
            state = kFile;
          } else if (special &&
                     has_base &&
                     url->scheme == base->scheme) {
            state = kSpecialRelativeOrAuthority;
          } else if (special) {
            state = kSpecialAuthoritySlashes;
          } else if (p[1] == '/') {
            state = kPathOrAuthority;
            p++;
          } else {
            url->flags |= URL_FLAGS_CANNOT_BE_BASE;
            url->flags |= URL_FLAGS_HAS_PATH;
            url->path.push_back("");
            state = kCannotBeBase;
          }
        } else if (!has_state_override) {
          buffer.clear();
          state = kNoScheme;
          p = input;
          continue;
        } else {
          url->flags |= URL_FLAGS_FAILED;
          return;
        }
        break;
      case kNoScheme:
        cannot_be_base = has_base && (base->flags & URL_FLAGS_CANNOT_BE_BASE);
        if (!has_base || (cannot_be_base && ch != '#')) {
          url->flags |= URL_FLAGS_FAILED;
          return;
        } else if (cannot_be_base && ch == '#') {
          url->scheme = base->scheme;
          if (IsSpecial(url->scheme)) {
            url->flags |= URL_FLAGS_SPECIAL;
            special = true;
          } else {
            url->flags &= ~URL_FLAGS_SPECIAL;
            special = false;
          }
          if (base->flags & URL_FLAGS_HAS_PATH) {
            url->flags |= URL_FLAGS_HAS_PATH;
            url->path = base->path;
          }
          if (base->flags & URL_FLAGS_HAS_QUERY) {
            url->flags |= URL_FLAGS_HAS_QUERY;
            url->query = base->query;
          }
          if (base->flags & URL_FLAGS_HAS_FRAGMENT) {
            url->flags |= URL_FLAGS_HAS_FRAGMENT;
            url->fragment = base->fragment;
          }
          url->flags |= URL_FLAGS_CANNOT_BE_BASE;
          state = kFragment;
        } else if (has_base &&
                   base->scheme != "file:") {
          state = kRelative;
          continue;
        } else {
          url->scheme = "file:";
          url->flags |= URL_FLAGS_SPECIAL;
          special = true;
          state = kFile;
          continue;
        }
        break;
      case kSpecialRelativeOrAuthority:
        if (ch == '/' && p[1] == '/') {
          state = kSpecialAuthorityIgnoreSlashes;
          p++;
        } else {
          state = kRelative;
          continue;
        }
        break;
      case kPathOrAuthority:
        if (ch == '/') {
          state = kAuthority;
        } else {
          state = kPath;
          continue;
        }
        break;
      case kRelative:
        url->scheme = base->scheme;
        if (IsSpecial(url->scheme)) {
          url->flags |= URL_FLAGS_SPECIAL;
          special = true;
        } else {
          url->flags &= ~URL_FLAGS_SPECIAL;
          special = false;
        }
        switch (ch) {
          case kEOL:
            if (base->flags & URL_FLAGS_HAS_USERNAME) {
              url->flags |= URL_FLAGS_HAS_USERNAME;
              url->username = base->username;
            }
            if (base->flags & URL_FLAGS_HAS_PASSWORD) {
              url->flags |= URL_FLAGS_HAS_PASSWORD;
              url->password = base->password;
            }
            if (base->flags & URL_FLAGS_HAS_HOST) {
              url->flags |= URL_FLAGS_HAS_HOST;
              url->host = base->host;
            }
            if (base->flags & URL_FLAGS_HAS_QUERY) {
              url->flags |= URL_FLAGS_HAS_QUERY;
              url->query = base->query;
            }
            if (base->flags & URL_FLAGS_HAS_PATH) {
              url->flags |= URL_FLAGS_HAS_PATH;
              url->path = base->path;
            }
            url->port = base->port;
            break;
          case '/':
            state = kRelativeSlash;
            break;
          case '?':
            if (base->flags & URL_FLAGS_HAS_USERNAME) {
              url->flags |= URL_FLAGS_HAS_USERNAME;
              url->username = base->username;
            }
            if (base->flags & URL_FLAGS_HAS_PASSWORD) {
              url->flags |= URL_FLAGS_HAS_PASSWORD;
              url->password = base->password;
            }
            if (base->flags & URL_FLAGS_HAS_HOST) {
              url->flags |= URL_FLAGS_HAS_HOST;
              url->host = base->host;
            }
            if (base->flags & URL_FLAGS_HAS_PATH) {
              url->flags |= URL_FLAGS_HAS_PATH;
              url->path = base->path;
            }
            url->port = base->port;
            state = kQuery;
            break;
          case '#':
            if (base->flags & URL_FLAGS_HAS_USERNAME) {
              url->flags |= URL_FLAGS_HAS_USERNAME;
              url->username = base->username;
            }
            if (base->flags & URL_FLAGS_HAS_PASSWORD) {
              url->flags |= URL_FLAGS_HAS_PASSWORD;
              url->password = base->password;
            }
            if (base->flags & URL_FLAGS_HAS_HOST) {
              url->flags |= URL_FLAGS_HAS_HOST;
              url->host = base->host;
            }
            if (base->flags & URL_FLAGS_HAS_QUERY) {
              url->flags |= URL_FLAGS_HAS_QUERY;
              url->query = base->query;
            }
            if (base->flags & URL_FLAGS_HAS_PATH) {
              url->flags |= URL_FLAGS_HAS_PATH;
              url->path = base->path;
            }
            url->port = base->port;
            state = kFragment;
            break;
          default:
            if (special_back_slash) {
              state = kRelativeSlash;
            } else {
              if (base->flags & URL_FLAGS_HAS_USERNAME) {
                url->flags |= URL_FLAGS_HAS_USERNAME;
                url->username = base->username;
              }
              if (base->flags & URL_FLAGS_HAS_PASSWORD) {
                url->flags |= URL_FLAGS_HAS_PASSWORD;
                url->password = base->password;
              }
              if (base->flags & URL_FLAGS_HAS_HOST) {
                url->flags |= URL_FLAGS_HAS_HOST;
                url->host = base->host;
              }
              if (base->flags & URL_FLAGS_HAS_PATH) {
                url->flags |= URL_FLAGS_HAS_PATH;
                url->path = base->path;
                ShortenUrlPath(url);
              }
              url->port = base->port;
              state = kPath;
              continue;
            }
        }
        break;
      case kRelativeSlash:
        if (IsSpecial(url->scheme) && (ch == '/' || ch == '\\')) {
          state = kSpecialAuthorityIgnoreSlashes;
        } else if (ch == '/') {
          state = kAuthority;
        } else {
          if (base->flags & URL_FLAGS_HAS_USERNAME) {
            url->flags |= URL_FLAGS_HAS_USERNAME;
            url->username = base->username;
          }
          if (base->flags & URL_FLAGS_HAS_PASSWORD) {
            url->flags |= URL_FLAGS_HAS_PASSWORD;
            url->password = base->password;
          }
          if (base->flags & URL_FLAGS_HAS_HOST) {
            url->flags |= URL_FLAGS_HAS_HOST;
            url->host = base->host;
          }
          url->port = base->port;
          state = kPath;
          continue;
        }
        break;
      case kSpecialAuthoritySlashes:
        state = kSpecialAuthorityIgnoreSlashes;
        if (ch == '/' && p[1] == '/') {
          p++;
        } else {
          continue;
        }
        break;
      case kSpecialAuthorityIgnoreSlashes:
        if (ch != '/' && ch != '\\') {
          state = kAuthority;
          continue;
        }
        break;
      case kAuthority:
        if (ch == '@') {
          if (atflag) {
            buffer.reserve(buffer.size() + 3);
            buffer.insert(0, "%40");
          }
          atflag = true;
          const size_t blen = buffer.size();
          if (blen > 0 && buffer[0] != ':') {
            url->flags |= URL_FLAGS_HAS_USERNAME;
          }
          for (size_t n = 0; n < blen; n++) {
            const char bch = buffer[n];
            if (bch == ':') {
              url->flags |= URL_FLAGS_HAS_PASSWORD;
              if (!uflag) {
                uflag = true;
                continue;
              }
            }
            if (uflag) {
              AppendOrEscape(&url->password, bch, USERINFO_ENCODE_SET);
            } else {
              AppendOrEscape(&url->username, bch, USERINFO_ENCODE_SET);
            }
          }
          buffer.clear();
        } else if (ch == kEOL ||
                   ch == '/' ||
                   ch == '?' ||
                   ch == '#' ||
                   special_back_slash) {
          if (atflag && buffer.size() == 0) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          p -= buffer.size() + 1;
          buffer.clear();
          state = kHost;
        } else {
          buffer += ch;
        }
        break;
      case kHost:
      case kHostname:
        if (has_state_override && url->scheme == "file:") {
          state = kFileHost;
          continue;
        } else if (ch == ':' && !sbflag) {
          if (buffer.size() == 0) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          url->flags |= URL_FLAGS_HAS_HOST;
          if (!ParseHost(&buffer, &url->host, special)) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          buffer.clear();
          state = kPort;
          if (state_override == kHostname) {
            return;
          }
        } else if (ch == kEOL ||
                   ch == '/' ||
                   ch == '?' ||
                   ch == '#' ||
                   special_back_slash) {
          p--;
          if (special && buffer.size() == 0) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          if (has_state_override &&
              buffer.size() == 0 &&
              ((url->username.size() > 0 || url->password.size() > 0) ||
               url->port != -1)) {
            url->flags |= URL_FLAGS_TERMINATED;
            return;
          }
          url->flags |= URL_FLAGS_HAS_HOST;
          if (!ParseHost(&buffer, &url->host, special)) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          buffer.clear();
          state = kPathStart;
          if (has_state_override) {
            return;
          }
        } else {
          if (ch == '[')
            sbflag = true;
          if (ch == ']')
            sbflag = false;
          buffer += ch;
        }
        break;
      case kPort:
        if (IsASCIIDigit(ch)) {
          buffer += ch;
        } else if (has_state_override ||
                   ch == kEOL ||
                   ch == '/' ||
                   ch == '?' ||
                   ch == '#' ||
                   special_back_slash) {
          if (buffer.size() > 0) {
            unsigned port = 0;
            // the condition port <= 0xffff prevents integer overflow
            for (size_t i = 0; port <= 0xffff && i < buffer.size(); i++)
              port = port * 10 + buffer[i] - '0';
            if (port > 0xffff) {
              // TODO(TimothyGu): This hack is currently needed for the host
              // setter since it needs access to hostname if it is valid, and
              // if the FAILED flag is set the entire response to JS layer
              // will be empty.
              if (state_override == kHost)
                url->port = -1;
              else
                url->flags |= URL_FLAGS_FAILED;
              return;
            }
            // the port is valid
            url->port = NormalizePort(url->scheme, static_cast<int>(port));
            buffer.clear();
          } else if (has_state_override) {
            // TODO(TimothyGu): Similar case as above.
            if (state_override == kHost)
              url->port = -1;
            else
              url->flags |= URL_FLAGS_TERMINATED;
            return;
          }
          state = kPathStart;
          continue;
        } else {
          url->flags |= URL_FLAGS_FAILED;
          return;
        }
        break;
      case kFile:
        url->scheme = "file:";
        if (ch == '/' || ch == '\\') {
          state = kFileSlash;
        } else if (has_base && base->scheme == "file:") {
          switch (ch) {
            case kEOL:
              if (base->flags & URL_FLAGS_HAS_HOST) {
                url->flags |= URL_FLAGS_HAS_HOST;
                url->host = base->host;
              }
              if (base->flags & URL_FLAGS_HAS_PATH) {
                url->flags |= URL_FLAGS_HAS_PATH;
                url->path = base->path;
              }
              if (base->flags & URL_FLAGS_HAS_QUERY) {
                url->flags |= URL_FLAGS_HAS_QUERY;
                url->query = base->query;
              }
              break;
            case '?':
              if (base->flags & URL_FLAGS_HAS_HOST) {
                url->flags |= URL_FLAGS_HAS_HOST;
                url->host = base->host;
              }
              if (base->flags & URL_FLAGS_HAS_PATH) {
                url->flags |= URL_FLAGS_HAS_PATH;
                url->path = base->path;
              }
              url->flags |= URL_FLAGS_HAS_QUERY;
              url->query.clear();
              state = kQuery;
              break;
            case '#':
              if (base->flags & URL_FLAGS_HAS_HOST) {
                url->flags |= URL_FLAGS_HAS_HOST;
                url->host = base->host;
              }
              if (base->flags & URL_FLAGS_HAS_PATH) {
                url->flags |= URL_FLAGS_HAS_PATH;
                url->path = base->path;
              }
              if (base->flags & URL_FLAGS_HAS_QUERY) {
                url->flags |= URL_FLAGS_HAS_QUERY;
                url->query = base->query;
              }
              url->flags |= URL_FLAGS_HAS_FRAGMENT;
              url->fragment.clear();
              state = kFragment;
              break;
            default:
              if (!StartsWithWindowsDriveLetter(p, end)) {
                if (base->flags & URL_FLAGS_HAS_HOST) {
                  url->flags |= URL_FLAGS_HAS_HOST;
                  url->host = base->host;
                }
                if (base->flags & URL_FLAGS_HAS_PATH) {
                  url->flags |= URL_FLAGS_HAS_PATH;
                  url->path = base->path;
                }
                ShortenUrlPath(url);
              }
              state = kPath;
              continue;
          }
        } else {
          state = kPath;
          continue;
        }
        break;
      case kFileSlash:
        if (ch == '/' || ch == '\\') {
          state = kFileHost;
        } else {
          if (has_base &&
              base->scheme == "file:" &&
              !StartsWithWindowsDriveLetter(p, end)) {
            if (IsNormalizedWindowsDriveLetter(base->path[0])) {
              url->flags |= URL_FLAGS_HAS_PATH;
              url->path.push_back(base->path[0]);
            } else {
              if (base->flags & URL_FLAGS_HAS_HOST) {
                url->flags |= URL_FLAGS_HAS_HOST;
                url->host = base->host;
              } else {
                url->flags &= ~URL_FLAGS_HAS_HOST;
                url->host.clear();
              }
            }
          }
          state = kPath;
          continue;
        }
        break;
      case kFileHost:
        if (ch == kEOL ||
            ch == '/' ||
            ch == '\\' ||
            ch == '?' ||
            ch == '#') {
          if (!has_state_override &&
              buffer.size() == 2 &&
              IsWindowsDriveLetter(buffer)) {
            state = kPath;
          } else if (buffer.size() == 0) {
            url->flags |= URL_FLAGS_HAS_HOST;
            url->host.clear();
            if (has_state_override)
              return;
            state = kPathStart;
          } else {
            std::string host;
            if (!ParseHost(&buffer, &host, special)) {
              url->flags |= URL_FLAGS_FAILED;
              return;
            }
            if (host == "localhost")
              host.clear();
            url->flags |= URL_FLAGS_HAS_HOST;
            url->host = host;
            if (has_state_override)
              return;
            buffer.clear();
            state = kPathStart;
          }
          continue;
        } else {
          buffer += ch;
        }
        break;
      case kPathStart:
        if (IsSpecial(url->scheme)) {
          state = kPath;
          if (ch != '/' && ch != '\\') {
            continue;
          }
        } else if (!has_state_override && ch == '?') {
          url->flags |= URL_FLAGS_HAS_QUERY;
          url->query.clear();
          state = kQuery;
        } else if (!has_state_override && ch == '#') {
          url->flags |= URL_FLAGS_HAS_FRAGMENT;
          url->fragment.clear();
          state = kFragment;
        } else if (ch != kEOL) {
          state = kPath;
          if (ch != '/') {
            continue;
          }
        }
        break;
      case kPath:
        if (ch == kEOL ||
            ch == '/' ||
            special_back_slash ||
            (!has_state_override && (ch == '?' || ch == '#'))) {
          if (IsDoubleDotSegment(buffer)) {
            ShortenUrlPath(url);
            if (ch != '/' && !special_back_slash) {
              url->flags |= URL_FLAGS_HAS_PATH;
              url->path.push_back("");
            }
          } else if (IsSingleDotSegment(buffer) &&
                     ch != '/' && !special_back_slash) {
            url->flags |= URL_FLAGS_HAS_PATH;
            url->path.push_back("");
          } else if (!IsSingleDotSegment(buffer)) {
            if (url->scheme == "file:" &&
                url->path.empty() &&
                buffer.size() == 2 &&
                IsWindowsDriveLetter(buffer)) {
              if ((url->flags & URL_FLAGS_HAS_HOST) &&
                  !url->host.empty()) {
                url->host.clear();
                url->flags |= URL_FLAGS_HAS_HOST;
              }
              buffer[1] = ':';
            }
            url->flags |= URL_FLAGS_HAS_PATH;
            std::string segment(buffer.c_str(), buffer.size());
            url->path.push_back(segment);
          }
          buffer.clear();
          if (url->scheme == "file:" &&
              (ch == kEOL ||
               ch == '?' ||
               ch == '#')) {
            while (url->path.size() > 1 && url->path[0].length() == 0) {
              url->path.erase(url->path.begin());
            }
          }
          if (ch == '?') {
            url->flags |= URL_FLAGS_HAS_QUERY;
            state = kQuery;
          } else if (ch == '#') {
            state = kFragment;
          }
        } else {
          AppendOrEscape(&buffer, ch, PATH_ENCODE_SET);
        }
        break;
      case kCannotBeBase:
        switch (ch) {
          case '?':
            state = kQuery;
            break;
          case '#':
            state = kFragment;
            break;
          default:
            if (url->path.size() == 0)
              url->path.push_back("");
            if (url->path.size() > 0 && ch != kEOL)
              AppendOrEscape(&url->path[0], ch, C0_CONTROL_ENCODE_SET);
        }
        break;
      case kQuery:
        if (ch == kEOL || (!has_state_override && ch == '#')) {
          url->flags |= URL_FLAGS_HAS_QUERY;
          url->query = buffer;
          buffer.clear();
          if (ch == '#')
            state = kFragment;
        } else {
          AppendOrEscape(&buffer, ch, QUERY_ENCODE_SET);
        }
        break;
      case kFragment:
        switch (ch) {
          case kEOL:
            url->flags |= URL_FLAGS_HAS_FRAGMENT;
            url->fragment = buffer;
            break;
          case 0:
            break;
          default:
            AppendOrEscape(&buffer, ch, C0_CONTROL_ENCODE_SET);
        }
        break;
      default:
        url->flags |= URL_FLAGS_INVALID_PARSE_STATE;
        return;
    }

    p++;
  }
}  // NOLINT(readability/fn_size)

static inline void SetArgs(Environment* env,
                           Local<Value> argv[],
                           const struct url_data* url) {
  Isolate* isolate = env->isolate();
  argv[ARG_FLAGS] = Integer::NewFromUnsigned(isolate, url->flags);
  argv[ARG_PROTOCOL] = OneByteString(isolate, url->scheme.c_str());
  if (url->flags & URL_FLAGS_HAS_USERNAME)
    argv[ARG_USERNAME] = UTF8STRING(isolate, url->username);
  if (url->flags & URL_FLAGS_HAS_PASSWORD)
    argv[ARG_PASSWORD] = UTF8STRING(isolate, url->password);
  if (url->flags & URL_FLAGS_HAS_HOST)
    argv[ARG_HOST] = UTF8STRING(isolate, url->host);
  if (url->flags & URL_FLAGS_HAS_QUERY)
    argv[ARG_QUERY] = UTF8STRING(isolate, url->query);
  if (url->flags & URL_FLAGS_HAS_FRAGMENT)
    argv[ARG_FRAGMENT] = UTF8STRING(isolate, url->fragment);
  if (url->port > -1)
    argv[ARG_PORT] = Integer::New(isolate, url->port);
  if (url->flags & URL_FLAGS_HAS_PATH)
    argv[ARG_PATH] = Copy(env, url->path);
}

static void Parse(Environment* env,
                  Local<Value> recv,
                  const char* input,
                  const size_t len,
                  enum url_parse_state state_override,
                  Local<Value> base_obj,
                  Local<Value> context_obj,
                  Local<Function> cb,
                  Local<Value> error_cb) {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  HandleScope handle_scope(isolate);
  Context::Scope context_scope(context);

  const bool has_context = context_obj->IsObject();
  const bool has_base = base_obj->IsObject();

  struct url_data base;
  struct url_data url;
  if (has_context)
    HarvestContext(env, &url, context_obj.As<Object>());
  if (has_base)
    HarvestBase(env, &base, base_obj.As<Object>());

  URL::Parse(input, len, state_override, &url, has_context, &base, has_base);
  if ((url.flags & URL_FLAGS_INVALID_PARSE_STATE) ||
      ((state_override != kUnknownState) &&
       (url.flags & URL_FLAGS_TERMINATED)))
    return;

  // Define the return value placeholders
  const Local<Value> undef = Undefined(isolate);
  const Local<Value> null = Null(isolate);
  if (!(url.flags & URL_FLAGS_FAILED)) {
    Local<Value> argv[9] = {
      undef,
      undef,
      undef,
      undef,
      null,  // host defaults to null
      null,  // port defaults to null
      undef,
      null,  // query defaults to null
      null,  // fragment defaults to null
    };
    SetArgs(env, argv, &url);
    cb->Call(context, recv, arraysize(argv), argv).FromMaybe(Local<Value>());
  } else if (error_cb->IsFunction()) {
    Local<Value> argv[2] = { undef, undef };
    argv[ERR_ARG_FLAGS] = Integer::NewFromUnsigned(isolate, url.flags);
    argv[ERR_ARG_INPUT] =
      String::NewFromUtf8(env->isolate(),
                          input,
                          v8::NewStringType::kNormal).ToLocalChecked();
    error_cb.As<Function>()->Call(context, recv, arraysize(argv), argv)
        .FromMaybe(Local<Value>());
  }
}

static void Parse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 5);
  CHECK(args[0]->IsString());  // input
  CHECK(args[2]->IsUndefined() ||  // base context
        args[2]->IsNull() ||
        args[2]->IsObject());
  CHECK(args[3]->IsUndefined() ||  // context
        args[3]->IsNull() ||
        args[3]->IsObject());
  CHECK(args[4]->IsFunction());  // complete callback
  CHECK(args[5]->IsUndefined() || args[5]->IsFunction());  // error callback

  Utf8Value input(env->isolate(), args[0]);
  enum url_parse_state state_override = kUnknownState;
  if (args[1]->IsNumber()) {
    state_override = static_cast<enum url_parse_state>(
        args[1]->Uint32Value(env->context()).FromJust());
  }

  Parse(env, args.This(),
        *input, input.length(),
        state_override,
        args[2],
        args[3],
        args[4].As<Function>(),
        args[5]);
}

static void EncodeAuthSet(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value value(env->isolate(), args[0]);
  std::string output;
  const size_t len = value.length();
  output.reserve(len);
  for (size_t n = 0; n < len; n++) {
    const char ch = (*value)[n];
    AppendOrEscape(&output, ch, USERINFO_ENCODE_SET);
  }
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          output.c_str(),
                          v8::NewStringType::kNormal).ToLocalChecked());
}

static void ToUSVString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 2);
  CHECK(args[0]->IsString());
  CHECK(args[1]->IsNumber());

  TwoByteValue value(env->isolate(), args[0]);
  const size_t n = value.length();

  const int64_t start = args[1]->IntegerValue(env->context()).FromJust();
  CHECK_GE(start, 0);

  for (size_t i = start; i < n; i++) {
    char16_t c = value[i];
    if (!IsUnicodeSurrogate(c)) {
      continue;
    } else if (IsUnicodeSurrogateTrail(c) || i == n - 1) {
      value[i] = kUnicodeReplacementCharacter;
    } else {
      char16_t d = value[i + 1];
      if (IsUnicodeTrail(d)) {
        i++;
      } else {
        value[i] = kUnicodeReplacementCharacter;
      }
    }
  }

  args.GetReturnValue().Set(
      String::NewFromTwoByte(env->isolate(),
                             *value,
                             v8::NewStringType::kNormal,
                             n).ToLocalChecked());
}

static void DomainToASCII(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value value(env->isolate(), args[0]);

  url_host host{{""}, HOST_TYPE_DOMAIN};
  // Assuming the host is used for a special scheme.
  ParseHost(&host, *value, value.length(), true);
  if (host.type == HOST_TYPE_FAILED) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(env->isolate(), ""));
    return;
  }
  std::string out;
  WriteHost(&host, &out);
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          out.c_str(),
                          v8::NewStringType::kNormal).ToLocalChecked());
}

static void DomainToUnicode(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value value(env->isolate(), args[0]);

  url_host host{{""}, HOST_TYPE_DOMAIN};
  // Assuming the host is used for a special scheme.
  ParseHost(&host, *value, value.length(), true, true);
  if (host.type == HOST_TYPE_FAILED) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(env->isolate(), ""));
    return;
  }
  std::string out;
  WriteHost(&host, &out);
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          out.c_str(),
                          v8::NewStringType::kNormal).ToLocalChecked());
}

std::string URL::ToFilePath() const {
  if (context_.scheme != "file:") {
    return "";
  }

#ifdef _WIN32
  const char* slash = "\\";
  auto is_slash = [] (char ch) {
    return ch == '/' || ch == '\\';
  };
#else
  const char* slash = "/";
  auto is_slash = [] (char ch) {
    return ch == '/';
  };
  if ((context_.flags & URL_FLAGS_HAS_HOST) &&
      context_.host.length() > 0) {
    return "";
  }
#endif
  std::string decoded_path;
  for (const std::string& part : context_.path) {
    std::string decoded;
    PercentDecode(part.c_str(), part.length(), &decoded);
    for (char& ch : decoded) {
      if (is_slash(ch)) {
        return "";
      }
    }
    decoded_path += slash + decoded;
  }

#ifdef _WIN32
  // TODO(TimothyGu): Use "\\?\" long paths on Windows.

  // If hostname is set, then we have a UNC path. Pass the hostname through
  // ToUnicode just in case it is an IDN using punycode encoding. We do not
  // need to worry about percent encoding because the URL parser will have
  // already taken care of that for us. Note that this only causes IDNs with an
  // appropriate `xn--` prefix to be decoded.
  if ((context_.flags & URL_FLAGS_HAS_HOST) &&
      context_.host.length() > 0) {
    std::string unicode_host;
    if (!ToUnicode(context_.host, &unicode_host)) {
      return "";
    }
    return "\\\\" + unicode_host + decoded_path;
  }
  // Otherwise, it's a local path that requires a drive letter.
  if (decoded_path.length() < 3) {
    return "";
  }
  if (decoded_path[2] != ':' ||
      !IsASCIIAlpha(decoded_path[1])) {
    return "";
  }
  // Strip out the leading '\'.
  return decoded_path.substr(1);
#else
  return decoded_path;
#endif
}

// This function works by calling out to a JS function that creates and
// returns the JS URL object. Be mindful of the JS<->Native boundary
// crossing that is required.
const Local<Value> URL::ToObject(Environment* env) const {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  const Local<Value> undef = Undefined(isolate);
  const Local<Value> null = Null(isolate);

  if (context_.flags & URL_FLAGS_FAILED)
    return Local<Value>();

  Local<Value> argv[9] = {
    undef,
    undef,
    undef,
    undef,
    null,  // host defaults to null
    null,  // port defaults to null
    undef,
    null,  // query defaults to null
    null,  // fragment defaults to null
  };
  SetArgs(env, argv, &context_);

  TryCatch try_catch(isolate);

  // The SetURLConstructor method must have been called already to
  // set the constructor function used below. SetURLConstructor is
  // called automatically when the internal/url.js module is loaded
  // during the internal/bootstrap_node.js processing.
  MaybeLocal<Value> ret =
      env->url_constructor_function()
          ->Call(env->context(), undef, 9, argv);

  if (ret.IsEmpty()) {
    ClearFatalExceptionHandlers(env);
    FatalException(isolate, try_catch);
  }

  return ret.ToLocalChecked();
}

static void SetURLConstructor(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  env->set_url_constructor_function(args[0].As<Function>());
}

static void Init(Local<Object> target,
                 Local<Value> unused,
                 Local<Context> context,
                 void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "parse", Parse);
  env->SetMethod(target, "encodeAuth", EncodeAuthSet);
  env->SetMethod(target, "toUSVString", ToUSVString);
  env->SetMethod(target, "domainToASCII", DomainToASCII);
  env->SetMethod(target, "domainToUnicode", DomainToUnicode);
  env->SetMethod(target, "setURLConstructor", SetURLConstructor);

#define XX(name, _) NODE_DEFINE_CONSTANT(target, name);
  FLAGS(XX)
#undef XX

#define XX(name) NODE_DEFINE_CONSTANT(target, name);
  PARSESTATES(XX)
#undef XX
}
}  // namespace url
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(url, node::url::Init)
