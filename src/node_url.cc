#include "node_url.h"
#include "base_object-inl.h"
#include "node_errors.h"
#include "node_external_reference.h"
#include "node_i18n.h"
#include "util-inl.h"

#include <cmath>
#include <cstdio>
#include <numeric>
#include <string>
#include <vector>

namespace node {

using errors::TryCatchScope;

using url::table_data::hex;
using url::table_data::C0_CONTROL_ENCODE_SET;
using url::table_data::FRAGMENT_ENCODE_SET;
using url::table_data::PATH_ENCODE_SET;
using url::table_data::USERINFO_ENCODE_SET;
using url::table_data::QUERY_ENCODE_SET_NONSPECIAL;
using url::table_data::QUERY_ENCODE_SET_SPECIAL;

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Null;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

Local<String> Utf8String(Isolate* isolate, const std::string& str) {
  return String::NewFromUtf8(isolate,
                             str.data(),
                             NewStringType::kNormal,
                             str.length()).ToLocalChecked();
}

namespace url {
namespace {

// https://url.spec.whatwg.org/#eof-code-point
constexpr char kEOL = -1;

// https://url.spec.whatwg.org/#concept-host
class URLHost {
 public:
  ~URLHost();

  void ParseIPv4Host(const char* input, size_t length, bool* is_ipv4);
  void ParseIPv6Host(const char* input, size_t length);
  void ParseOpaqueHost(const char* input, size_t length);
  void ParseHost(const char* input,
                 size_t length,
                 bool is_special,
                 bool unicode = false);

  bool ParsingFailed() const { return type_ == HostType::H_FAILED; }
  std::string ToString() const;
  // Like ToString(), but avoids a copy in exchange for invalidating `*this`.
  std::string ToStringMove();

 private:
  enum class HostType {
    H_FAILED,
    H_DOMAIN,
    H_IPV4,
    H_IPV6,
    H_OPAQUE,
  };

  union Value {
    std::string domain_or_opaque;
    uint32_t ipv4;
    uint16_t ipv6[8];

    ~Value() {}
    Value() : ipv4(0) {}
  };

  Value value_;
  HostType type_ = HostType::H_FAILED;

  void Reset() {
    using string = std::string;
    switch (type_) {
      case HostType::H_DOMAIN:
      case HostType::H_OPAQUE:
        value_.domain_or_opaque.~string();
        break;
      default:
        break;
    }
    type_ = HostType::H_FAILED;
  }

  // Setting the string members of the union with = is brittle because
  // it relies on them being initialized to a state that requires no
  // destruction of old data.
  // For a long time, that worked well enough because ParseIPv6Host() happens
  // to zero-fill `value_`, but that really is relying on standard library
  // internals too much.
  // These helpers are the easiest solution but we might want to consider
  // just not forcing strings into an union.
  void SetOpaque(std::string&& string) {
    Reset();
    type_ = HostType::H_OPAQUE;
    new(&value_.domain_or_opaque) std::string(std::move(string));
  }

  void SetDomain(std::string&& string) {
    Reset();
    type_ = HostType::H_DOMAIN;
    new(&value_.domain_or_opaque) std::string(std::move(string));
  }
};

URLHost::~URLHost() {
  Reset();
}

#define ARGS(XX)                                                              \
  XX(ARG_FLAGS)                                                               \
  XX(ARG_PROTOCOL)                                                            \
  XX(ARG_USERNAME)                                                            \
  XX(ARG_PASSWORD)                                                            \
  XX(ARG_HOST)                                                                \
  XX(ARG_PORT)                                                                \
  XX(ARG_PATH)                                                                \
  XX(ARG_QUERY)                                                               \
  XX(ARG_FRAGMENT)                                                            \
  XX(ARG_COUNT)  // This one has to be last.

enum url_cb_args {
#define XX(name) name,
  ARGS(XX)
#undef XX
};

#define TWO_CHAR_STRING_TEST(bits, name, expr)                                \
  template <typename T>                                                       \
  bool name(const T ch1, const T ch2) {                                \
    static_assert(sizeof(ch1) >= (bits) / 8,                                  \
                  "Character must be wider than " #bits " bits");             \
    return (expr);                                                            \
  }                                                                           \
  template <typename T>                                                       \
  bool name(const std::basic_string<T>& str) {                         \
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
T ASCIILowercase(T ch) {
  return IsASCIIAlpha(ch) ? (ch | 0x20) : ch;
}

// https://url.spec.whatwg.org/#forbidden-host-code-point
CHAR_TEST(8, IsForbiddenHostCodePoint,
          ch == '\0' || ch == '\t' || ch == '\n' || ch == '\r' ||
          ch == ' ' || ch == '#' || ch == '%' || ch == '/' ||
          ch == ':' || ch == '?' || ch == '@' || ch == '[' ||
          ch == '<' || ch == '>' || ch == '\\' || ch == ']' ||
          ch == '^' || ch == '|')

// https://url.spec.whatwg.org/#windows-drive-letter
TWO_CHAR_STRING_TEST(8, IsWindowsDriveLetter,
                     (IsASCIIAlpha(ch1) && (ch2 == ':' || ch2 == '|')))

// https://url.spec.whatwg.org/#normalized-windows-drive-letter
TWO_CHAR_STRING_TEST(8, IsNormalizedWindowsDriveLetter,
                     (IsASCIIAlpha(ch1) && ch2 == ':'))

#undef TWO_CHAR_STRING_TEST

bool BitAt(const uint8_t a[], const uint8_t i) {
  return !!(a[i >> 3] & (1 << (i & 7)));
}

// Appends ch to str. If ch position in encode_set is set, the ch will
// be percent-encoded then appended.
void AppendOrEscape(std::string* str,
                    const unsigned char ch,
                    const uint8_t encode_set[]) {
  if (BitAt(encode_set, ch))
    *str += hex + ch * 4;  // "%XX\0" has a length of 4
  else
    *str += ch;
}

template <typename T>
unsigned hex2bin(const T ch) {
  if (ch >= '0' && ch <= '9')
    return ch - '0';
  if (ch >= 'A' && ch <= 'F')
    return 10 + (ch - 'A');
  if (ch >= 'a' && ch <= 'f')
    return 10 + (ch - 'a');
  UNREACHABLE();
}

std::string PercentDecode(const char* input, size_t len) {
  std::string dest;
  if (len == 0)
    return dest;
  dest.reserve(len);
  const char* pointer = input;
  const char* end = input + len;

  while (pointer < end) {
    const char ch = pointer[0];
    size_t remaining = end - pointer - 1;
    if (ch != '%' || remaining < 2 ||
        (ch == '%' &&
         (!IsASCIIHexDigit(pointer[1]) ||
          !IsASCIIHexDigit(pointer[2])))) {
      dest += ch;
      pointer++;
      continue;
    } else {
      unsigned a = hex2bin(pointer[1]);
      unsigned b = hex2bin(pointer[2]);
      char c = static_cast<char>(a * 16 + b);
      dest += c;
      pointer += 3;
    }
  }
  return dest;
}

#define SPECIALS(XX)                                                          \
  XX(ftp, 21, "ftp:")                                                         \
  XX(file, -1, "file:")                                                       \
  XX(http, 80, "http:")                                                       \
  XX(https, 443, "https:")                                                    \
  XX(ws, 80, "ws:")                                                           \
  XX(wss, 443, "wss:")

bool IsSpecial(const std::string& scheme) {
#define V(_, __, name) if (scheme == name) return true;
  SPECIALS(V);
#undef V
  return false;
}

Local<String> GetSpecial(Environment* env, const std::string& scheme) {
#define V(key, _, name) if (scheme == name)                                  \
    return env->url_special_##key##_string();
  SPECIALS(V)
#undef V
  UNREACHABLE();
}

int NormalizePort(const std::string& scheme, int p) {
#define V(_, port, name) if (scheme == name && p == port) return -1;
  SPECIALS(V);
#undef V
  return p;
}

// https://url.spec.whatwg.org/#start-with-a-windows-drive-letter
bool StartsWithWindowsDriveLetter(const char* p, const char* end) {
  size_t length = end - p;
  return length >= 2 &&
    IsWindowsDriveLetter(p[0], p[1]) &&
    (length == 2 ||
      p[2] == '/' ||
      p[2] == '\\' ||
      p[2] == '?' ||
      p[2] == '#');
}

#if defined(NODE_HAVE_I18N_SUPPORT)
bool ToUnicode(const std::string& input, std::string* output) {
  MaybeStackBuffer<char> buf;
  if (i18n::ToUnicode(&buf, input.c_str(), input.length()) < 0)
    return false;
  output->assign(*buf, buf.length());
  return true;
}

bool ToASCII(const std::string& input, std::string* output) {
  MaybeStackBuffer<char> buf;
  if (i18n::ToASCII(&buf, input.c_str(), input.length()) < 0)
    return false;
  if (buf.length() == 0)
    return false;
  output->assign(*buf, buf.length());
  return true;
}
#else  // !defined(NODE_HAVE_I18N_SUPPORT)
// Intentional non-ops if ICU is not present.
bool ToUnicode(const std::string& input, std::string* output) {
  *output = input;
  return true;
}

bool ToASCII(const std::string& input, std::string* output) {
  *output = input;
  return true;
}
#endif  // !defined(NODE_HAVE_I18N_SUPPORT)

#define NS_IN6ADDRSZ 16

void URLHost::ParseIPv6Host(const char* input, size_t length) {
  CHECK_EQ(type_, HostType::H_FAILED);

  unsigned char buf[sizeof(struct in6_addr)];
  MaybeStackBuffer<char> ipv6(length + 1);
  *(*ipv6 + length) = 0;
  memset(buf, 0, sizeof(buf));
  memcpy(*ipv6, input, sizeof(const char) * length);

  int ret = uv_inet_pton(AF_INET6, *ipv6, buf);

  if (ret != 0) {
    return;
  }

  // Ref: https://sourceware.org/git/?p=glibc.git;a=blob;f=resolv/inet_ntop.c;h=c4d38c0f951013e51a4fc6eaa8a9b82e146abe5a;hb=HEAD#l119
  for (int i = 0; i < NS_IN6ADDRSZ; i += 2) {
    value_.ipv6[i >> 1] = (buf[i] << 8) | buf[i + 1];
  }

  type_ = HostType::H_IPV6;
}

int64_t ParseNumber(const char* start, const char* end) {
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
  return strtoll(start, nullptr, R);
}

void URLHost::ParseIPv4Host(const char* input, size_t length, bool* is_ipv4) {
  CHECK_EQ(type_, HostType::H_FAILED);
  *is_ipv4 = false;
  const char* pointer = input;
  const char* mark = input;
  const char* end = pointer + length;
  int parts = 0;
  uint32_t val = 0;
  uint64_t numbers[4];
  int tooBigNumbers = 0;
  if (length == 0)
    return;

  while (pointer <= end) {
    const char ch = pointer < end ? pointer[0] : kEOL;
    int64_t remaining = end - pointer - 1;
    if (ch == '.' || ch == kEOL) {
      if (++parts > static_cast<int>(arraysize(numbers)))
        return;
      if (pointer == mark)
        return;
      int64_t n = ParseNumber(mark, pointer);
      if (n < 0)
        return;

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
  *is_ipv4 = true;

  // If any but the last item in numbers is greater than 255, return failure.
  // If the last item in numbers is greater than or equal to
  // 256^(5 - the number of items in numbers), return failure.
  if (tooBigNumbers > 1 ||
      (tooBigNumbers == 1 && numbers[parts - 1] <= 255) ||
      numbers[parts - 1] >= pow(256, static_cast<double>(5 - parts))) {
    return;
  }

  type_ = HostType::H_IPV4;
  val = static_cast<uint32_t>(numbers[parts - 1]);
  for (int n = 0; n < parts - 1; n++) {
    double b = 3 - n;
    val +=
        static_cast<uint32_t>(numbers[n]) * static_cast<uint32_t>(pow(256, b));
  }

  value_.ipv4 = val;
}

void URLHost::ParseOpaqueHost(const char* input, size_t length) {
  CHECK_EQ(type_, HostType::H_FAILED);
  std::string output;
  output.reserve(length);
  for (size_t i = 0; i < length; i++) {
    const char ch = input[i];
    if (ch != '%' && IsForbiddenHostCodePoint(ch)) {
      return;
    } else {
      AppendOrEscape(&output, ch, C0_CONTROL_ENCODE_SET);
    }
  }

  SetOpaque(std::move(output));
}

void URLHost::ParseHost(const char* input,
                        size_t length,
                        bool is_special,
                        bool unicode) {
  CHECK_EQ(type_, HostType::H_FAILED);
  const char* pointer = input;

  if (length == 0)
    return;

  if (pointer[0] == '[') {
    if (pointer[length - 1] != ']')
      return;
    return ParseIPv6Host(++pointer, length - 2);
  }

  if (!is_special)
    return ParseOpaqueHost(input, length);

  // First, we have to percent decode
  std::string decoded = PercentDecode(input, length);

  // Then we have to punycode toASCII
  if (!ToASCII(decoded, &decoded))
    return;

  // If any of the following characters are still present, we have to fail
  for (size_t n = 0; n < decoded.size(); n++) {
    const char ch = decoded[n];
    if (IsForbiddenHostCodePoint(ch)) {
      return;
    }
  }

  // Check to see if it's an IPv4 IP address
  bool is_ipv4;
  ParseIPv4Host(decoded.c_str(), decoded.length(), &is_ipv4);
  if (is_ipv4)
    return;

  // If the unicode flag is set, run the result through punycode ToUnicode
  if (unicode && !ToUnicode(decoded, &decoded))
    return;

  // It's not an IPv4 or IPv6 address, it must be a domain
  SetDomain(std::move(decoded));
}

// Locates the longest sequence of 0 segments in an IPv6 address
// in order to use the :: compression when serializing
template <typename T>
T* FindLongestZeroSequence(T* values, size_t len) {
  T* start = values;
  T* end = start + len;
  T* result = nullptr;

  T* current = nullptr;
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

std::string URLHost::ToStringMove() {
  std::string return_value;
  switch (type_) {
    case HostType::H_DOMAIN:
    case HostType::H_OPAQUE:
      return_value = std::move(value_.domain_or_opaque);
      break;
    default:
      return_value = ToString();
      break;
  }
  Reset();
  return return_value;
}

std::string URLHost::ToString() const {
  std::string dest;
  switch (type_) {
    case HostType::H_DOMAIN:
    case HostType::H_OPAQUE:
      return value_.domain_or_opaque;
    case HostType::H_IPV4: {
      dest.reserve(15);
      uint32_t value = value_.ipv4;
      for (int n = 0; n < 4; n++) {
        dest.insert(0, std::to_string(value % 256));
        if (n < 3)
          dest.insert(0, 1, '.');
        value /= 256;
      }
      break;
    }
    case HostType::H_IPV6: {
      dest.reserve(41);
      dest += '[';
      const uint16_t* start = &value_.ipv6[0];
      const uint16_t* compress_pointer =
          FindLongestZeroSequence(start, 8);
      bool ignore0 = false;
      for (int n = 0; n <= 7; n++) {
        const uint16_t* piece = &value_.ipv6[n];
        if (ignore0 && *piece == 0)
          continue;
        else if (ignore0)
          ignore0 = false;
        if (compress_pointer == piece) {
          dest += n == 0 ? "::" : ":";
          ignore0 = true;
          continue;
        }
        char buf[5];
        snprintf(buf, sizeof(buf), "%x", *piece);
        dest += buf;
        if (n < 7)
          dest += ':';
      }
      dest += ']';
      break;
    }
    case HostType::H_FAILED:
      break;
  }
  return dest;
}

bool ParseHost(const std::string& input,
               std::string* output,
               bool is_special,
               bool unicode = false) {
  if (input.empty()) {
    output->clear();
    return true;
  }
  URLHost host;
  host.ParseHost(input.c_str(), input.length(), is_special, unicode);
  if (host.ParsingFailed())
    return false;
  *output = host.ToStringMove();
  return true;
}

std::vector<std::string> FromJSStringArray(Environment* env,
                                           Local<Array> array) {
  std::vector<std::string> vec;
  if (array->Length() > 0)
    vec.reserve(array->Length());
  for (size_t n = 0; n < array->Length(); n++) {
    Local<Value> val = array->Get(env->context(), n).ToLocalChecked();
    if (val->IsString()) {
      Utf8Value value(env->isolate(), val.As<String>());
      vec.emplace_back(*value, value.length());
    }
  }
  return vec;
}

url_data HarvestBase(Environment* env, Local<Object> base_obj) {
  url_data base;
  Local<Context> context = env->context();

  Local<Value> flags =
      base_obj->Get(env->context(), env->flags_string()).ToLocalChecked();
  if (flags->IsInt32())
    base.flags = flags->Int32Value(context).FromJust();

  Local<Value> port =
      base_obj->Get(env->context(), env->port_string()).ToLocalChecked();
  if (port->IsInt32())
    base.port = port->Int32Value(context).FromJust();

  Local<Value> scheme =
      base_obj->Get(env->context(), env->scheme_string()).ToLocalChecked();
  base.scheme = Utf8Value(env->isolate(), scheme).out();

  auto GetStr = [&](std::string url_data::*member,
                    int flag,
                    Local<String> name,
                    bool empty_as_present) {
    Local<Value> value = base_obj->Get(env->context(), name).ToLocalChecked();
    if (value->IsString()) {
      Utf8Value utf8value(env->isolate(), value.As<String>());
      (base.*member).assign(*utf8value, utf8value.length());
      if (empty_as_present || value.As<String>()->Length() != 0) {
        base.flags |= flag;
      }
    }
  };
  GetStr(&url_data::username,
         URL_FLAGS_HAS_USERNAME,
         env->username_string(),
         false);
  GetStr(&url_data::password,
         URL_FLAGS_HAS_PASSWORD,
         env->password_string(),
         false);
  GetStr(&url_data::host, URL_FLAGS_HAS_HOST, env->host_string(), true);
  GetStr(&url_data::query, URL_FLAGS_HAS_QUERY, env->query_string(), true);
  GetStr(&url_data::fragment,
         URL_FLAGS_HAS_FRAGMENT,
         env->fragment_string(),
         true);

  Local<Value>
      path = base_obj->Get(env->context(), env->path_string()).ToLocalChecked();
  if (path->IsArray()) {
    base.flags |= URL_FLAGS_HAS_PATH;
    base.path = FromJSStringArray(env, path.As<Array>());
  }
  return base;
}

url_data HarvestContext(Environment* env, Local<Object> context_obj) {
  url_data context;
  Local<Value> flags =
      context_obj->Get(env->context(), env->flags_string()).ToLocalChecked();
  if (flags->IsInt32()) {
    static constexpr int32_t kCopyFlagsMask =
        URL_FLAGS_SPECIAL |
        URL_FLAGS_CANNOT_BE_BASE |
        URL_FLAGS_HAS_USERNAME |
        URL_FLAGS_HAS_PASSWORD |
        URL_FLAGS_HAS_HOST;
    context.flags |= flags.As<Int32>()->Value() & kCopyFlagsMask;
  }
  Local<Value> scheme =
      context_obj->Get(env->context(), env->scheme_string()).ToLocalChecked();
  if (scheme->IsString()) {
    Utf8Value value(env->isolate(), scheme);
    context.scheme.assign(*value, value.length());
  }
  Local<Value> port =
      context_obj->Get(env->context(), env->port_string()).ToLocalChecked();
  if (port->IsInt32())
    context.port = port.As<Int32>()->Value();
  if (context.flags & URL_FLAGS_HAS_USERNAME) {
    Local<Value> username =
        context_obj->Get(env->context(),
                         env->username_string()).ToLocalChecked();
    CHECK(username->IsString());
    Utf8Value value(env->isolate(), username);
    context.username.assign(*value, value.length());
  }
  if (context.flags & URL_FLAGS_HAS_PASSWORD) {
    Local<Value> password =
        context_obj->Get(env->context(),
                         env->password_string()).ToLocalChecked();
    CHECK(password->IsString());
    Utf8Value value(env->isolate(), password);
    context.password.assign(*value, value.length());
  }
  Local<Value> host =
      context_obj->Get(env->context(),
                       env->host_string()).ToLocalChecked();
  if (host->IsString()) {
    Utf8Value value(env->isolate(), host);
    context.host.assign(*value, value.length());
  }
  return context;
}

// Single dot segment can be ".", "%2e", or "%2E"
bool IsSingleDotSegment(const std::string& str) {
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
bool IsDoubleDotSegment(const std::string& str) {
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

void ShortenUrlPath(struct url_data* url) {
  if (url->path.empty()) return;
  if (url->path.size() == 1 && url->scheme == "file:" &&
      IsNormalizedWindowsDriveLetter(url->path[0])) return;
  url->path.pop_back();
}

}  // anonymous namespace

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
    input = p;
    len = end - p;
  }

  // The spec says we should strip out any ASCII tabs or newlines.
  // In those cases, we create another std::string instance with the filtered
  // contents, but in the general case we avoid the overhead.
  std::string whitespace_stripped;
  for (const char* ptr = p; ptr < end; ptr++) {
    if (!IsASCIITabOrNewline(*ptr))
      continue;
    // Hit tab or newline. Allocate storage, copy what we have until now,
    // and then iterate and filter all similar characters out.
    whitespace_stripped.reserve(len - 1);
    whitespace_stripped.assign(p, ptr - p);
    // 'ptr + 1' skips the current char, which we know to be tab or newline.
    for (ptr = ptr + 1; ptr < end; ptr++) {
      if (!IsASCIITabOrNewline(*ptr))
        whitespace_stripped += *ptr;
    }

    // Update variables like they should have looked like if the string
    // had been stripped of whitespace to begin with.
    input = whitespace_stripped.c_str();
    len = whitespace_stripped.size();
    p = input;
    end = input + len;
    break;
  }

  bool atflag = false;  // Set when @ has been seen.
  bool square_bracket_flag = false;  // Set inside of [...]
  bool password_token_seen_flag = false;  // Set after a : after an username.

  std::string buffer;

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
    bool special_back_slash = (special && ch == '\\');

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
                  (url->port != -1))) ||
                  (url->scheme == "file:" && url->host.empty())) {
              url->flags |= URL_FLAGS_TERMINATED;
              return;
            }
          }

          url->scheme = std::move(buffer);
          url->port = NormalizePort(url->scheme, url->port);
          if (new_is_special) {
            url->flags |= URL_FLAGS_SPECIAL;
            special = true;
          } else {
            url->flags &= ~URL_FLAGS_SPECIAL;
            special = false;
          }
          // `special_back_slash` equals to `(special && ch == '\\')` and `ch`
          // here always not equals to `\\`. So `special_back_slash` here always
          // equals to `false`.
          special_back_slash = false;
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
          } else if (p + 1 < end && p[1] == '/') {
            state = kPathOrAuthority;
            p++;
          } else {
            url->flags |= URL_FLAGS_CANNOT_BE_BASE;
            url->flags |= URL_FLAGS_HAS_PATH;
            url->path.emplace_back("");
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
          special_back_slash = (special && ch == '\\');
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
          special_back_slash = (special && ch == '\\');
          continue;
        }
        break;
      case kSpecialRelativeOrAuthority:
        if (ch == '/' && p + 1 < end && p[1] == '/') {
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
        special_back_slash = (special && ch == '\\');
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
        if (ch == '/' && p + 1 < end && p[1] == '/') {
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
          size_t blen = buffer.size();
          if (blen > 0 && buffer[0] != ':') {
            url->flags |= URL_FLAGS_HAS_USERNAME;
          }
          for (size_t n = 0; n < blen; n++) {
            const char bch = buffer[n];
            if (bch == ':') {
              url->flags |= URL_FLAGS_HAS_PASSWORD;
              if (!password_token_seen_flag) {
                password_token_seen_flag = true;
                continue;
              }
            }
            if (password_token_seen_flag) {
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
        } else if (ch == ':' && !square_bracket_flag) {
          if (buffer.size() == 0) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          if (state_override == kHostname) {
            return;
          }
          url->flags |= URL_FLAGS_HAS_HOST;
          if (!ParseHost(buffer, &url->host, special)) {
            url->flags |= URL_FLAGS_FAILED;
            return;
          }
          buffer.clear();
          state = kPort;
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
          if (!ParseHost(buffer, &url->host, special)) {
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
            square_bracket_flag = true;
          if (ch == ']')
            square_bracket_flag = false;
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
            if (url->port == -1)
              url->flags |= URL_FLAGS_IS_DEFAULT_SCHEME_PORT;
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
        url->host.clear();
        url->flags |= URL_FLAGS_HAS_HOST;
        if (ch == '/' || ch == '\\') {
          state = kFileSlash;
        } else if (has_base && base->scheme == "file:") {
          switch (ch) {
            case kEOL:
              if (base->flags & URL_FLAGS_HAS_HOST) {
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
              url->query.clear();
              if (base->flags & URL_FLAGS_HAS_HOST) {
                url->host = base->host;
              }
              if (base->flags & URL_FLAGS_HAS_PATH) {
                url->flags |= URL_FLAGS_HAS_PATH;
                url->path = base->path;
              }
              if (!StartsWithWindowsDriveLetter(p, end)) {
                ShortenUrlPath(url);
              } else {
                url->path.clear();
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
          if (has_base && base->scheme == "file:") {
            url->flags |= URL_FLAGS_HAS_HOST;
            url->host = base->host;
            if (!StartsWithWindowsDriveLetter(p, end) &&
                IsNormalizedWindowsDriveLetter(base->path[0])) {
              url->flags |= URL_FLAGS_HAS_PATH;
              url->path.push_back(base->path[0]);
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
            if (!ParseHost(buffer, &host, special)) {
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
        } else if (has_state_override && !(url->flags & URL_FLAGS_HAS_HOST)) {
          url->flags |= URL_FLAGS_HAS_PATH;
          url->path.emplace_back("");
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
              url->path.emplace_back("");
            }
          } else if (IsSingleDotSegment(buffer) &&
                     ch != '/' && !special_back_slash) {
            url->flags |= URL_FLAGS_HAS_PATH;
            url->path.emplace_back("");
          } else if (!IsSingleDotSegment(buffer)) {
            if (url->scheme == "file:" &&
                url->path.empty() &&
                buffer.size() == 2 &&
                IsWindowsDriveLetter(buffer)) {
              buffer[1] = ':';
            }
            url->flags |= URL_FLAGS_HAS_PATH;
            url->path.emplace_back(std::move(buffer));
          }
          buffer.clear();
          if (ch == '?') {
            url->flags |= URL_FLAGS_HAS_QUERY;
            url->query.clear();
            state = kQuery;
          } else if (ch == '#') {
            url->flags |= URL_FLAGS_HAS_FRAGMENT;
            url->fragment.clear();
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
            if (url->path.empty())
              url->path.emplace_back("");
            else if (ch != kEOL)
              AppendOrEscape(&url->path[0], ch, C0_CONTROL_ENCODE_SET);
        }
        break;
      case kQuery:
        if (ch == kEOL || (!has_state_override && ch == '#')) {
          url->flags |= URL_FLAGS_HAS_QUERY;
          url->query = std::move(buffer);
          buffer.clear();
          if (ch == '#')
            state = kFragment;
        } else {
          AppendOrEscape(&buffer, ch, special ? QUERY_ENCODE_SET_SPECIAL :
                                                QUERY_ENCODE_SET_NONSPECIAL);
        }
        break;
      case kFragment:
        switch (ch) {
          case kEOL:
            url->flags |= URL_FLAGS_HAS_FRAGMENT;
            url->fragment = std::move(buffer);
            break;
          default:
            AppendOrEscape(&buffer, ch, FRAGMENT_ENCODE_SET);
        }
        break;
      default:
        url->flags |= URL_FLAGS_INVALID_PARSE_STATE;
        return;
    }

    p++;
  }
}  // NOLINT(readability/fn_size)

// https://url.spec.whatwg.org/#url-serializing
std::string URL::SerializeURL(const url_data& url,
                              bool exclude = false) {
  std::string output;
  output.reserve(
    10 +  // We generally insert < 10 separator characters between URL parts
    url.scheme.size() +
    url.username.size() +
    url.password.size() +
    url.host.size() +
    url.query.size() +
    url.fragment.size() +
    url.href.size() +
    std::accumulate(
        url.path.begin(),
        url.path.end(),
        0,
        [](size_t sum, const auto& str) { return sum + str.size(); }));

  output += url.scheme;
  if (url.flags & URL_FLAGS_HAS_HOST) {
    output += "//";
    if (url.flags & URL_FLAGS_HAS_USERNAME ||
        url.flags & URL_FLAGS_HAS_PASSWORD) {
      if (url.flags & URL_FLAGS_HAS_USERNAME) {
        output += url.username;
      }
      if (url.flags & URL_FLAGS_HAS_PASSWORD) {
        output += ":" + url.password;
      }
      output += "@";
    }
    output += url.host;
    if (url.port != -1) {
      output += ":" + std::to_string(url.port);
    }
  }
  if (url.flags & URL_FLAGS_CANNOT_BE_BASE) {
    output += url.path[0];
  } else {
    if (!(url.flags & URL_FLAGS_HAS_HOST) &&
          url.path.size() > 1 &&
          url.path[0].empty()) {
      output += "/.";
    }
    for (size_t i = 1; i < url.path.size(); i++) {
      output += "/" + url.path[i];
    }
  }
  if (url.flags & URL_FLAGS_HAS_QUERY) {
    output += "?" + url.query;
  }
  if (!exclude && (url.flags & URL_FLAGS_HAS_FRAGMENT)) {
    output += "#" + url.fragment;
  }
  output.shrink_to_fit();
  return output;
}

namespace {
void SetArgs(Environment* env,
             Local<Value> argv[ARG_COUNT],
             const struct url_data& url) {
  Isolate* isolate = env->isolate();
  argv[ARG_FLAGS] = Integer::NewFromUnsigned(isolate, url.flags);
  argv[ARG_PROTOCOL] =
      url.flags & URL_FLAGS_SPECIAL ?
          GetSpecial(env, url.scheme) :
          OneByteString(isolate, url.scheme.c_str());
  if (url.flags & URL_FLAGS_HAS_USERNAME)
    argv[ARG_USERNAME] = Utf8String(isolate, url.username);
  if (url.flags & URL_FLAGS_HAS_PASSWORD)
    argv[ARG_PASSWORD] = Utf8String(isolate, url.password);
  if (url.flags & URL_FLAGS_HAS_HOST)
    argv[ARG_HOST] = Utf8String(isolate, url.host);
  if (url.flags & URL_FLAGS_HAS_QUERY)
    argv[ARG_QUERY] = Utf8String(isolate, url.query);
  if (url.flags & URL_FLAGS_HAS_FRAGMENT)
    argv[ARG_FRAGMENT] = Utf8String(isolate, url.fragment);
  if (url.port > -1)
    argv[ARG_PORT] = Integer::New(isolate, url.port);
  if (url.flags & URL_FLAGS_HAS_PATH)
    argv[ARG_PATH] = ToV8Value(env->context(), url.path).ToLocalChecked();
}

void Parse(Environment* env,
           Local<Value> recv,
           const char* input,
           size_t len,
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

  url_data base;
  url_data url;
  if (has_context)
    url = HarvestContext(env, context_obj.As<Object>());
  if (has_base)
    base = HarvestBase(env, base_obj.As<Object>());

  URL::Parse(input, len, state_override, &url, has_context, &base, has_base);
  if ((url.flags & URL_FLAGS_INVALID_PARSE_STATE) ||
      ((state_override != kUnknownState) &&
       (url.flags & URL_FLAGS_TERMINATED)))
    return;

  // Define the return value placeholders
  const Local<Value> undef = Undefined(isolate);
  const Local<Value> null = Null(isolate);
  if (!(url.flags & URL_FLAGS_FAILED)) {
    Local<Value> argv[] = {
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
    SetArgs(env, argv, url);
    cb->Call(context, recv, arraysize(argv), argv).FromMaybe(Local<Value>());
  } else if (error_cb->IsFunction()) {
    Local<Value> flags = Integer::NewFromUnsigned(isolate, url.flags);
    USE(error_cb.As<Function>()->Call(context, recv, 1, &flags));
  }
}

void Parse(const FunctionCallbackInfo<Value>& args) {
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

void EncodeAuthSet(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value value(env->isolate(), args[0]);
  std::string output;
  size_t len = value.length();
  output.reserve(len);
  for (size_t n = 0; n < len; n++) {
    const char ch = (*value)[n];
    AppendOrEscape(&output, ch, USERINFO_ENCODE_SET);
  }
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), output.c_str()).ToLocalChecked());
}

void DomainToASCII(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value value(env->isolate(), args[0]);

  URLHost host;
  // Assuming the host is used for a special scheme.
  host.ParseHost(*value, value.length(), true);
  if (host.ParsingFailed()) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(env->isolate(), ""));
    return;
  }
  std::string out = host.ToStringMove();
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), out.c_str()).ToLocalChecked());
}

void DomainToUnicode(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value value(env->isolate(), args[0]);

  URLHost host;
  // Assuming the host is used for a special scheme.
  host.ParseHost(*value, value.length(), true, true);
  if (host.ParsingFailed()) {
    args.GetReturnValue().Set(FIXED_ONE_BYTE_STRING(env->isolate(), ""));
    return;
  }
  std::string out = host.ToStringMove();
  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(), out.c_str()).ToLocalChecked());
}

void SetURLConstructor(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_EQ(args.Length(), 1);
  CHECK(args[0]->IsFunction());
  env->set_url_constructor_function(args[0].As<Function>());
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "parse", Parse);
  env->SetMethodNoSideEffect(target, "encodeAuth", EncodeAuthSet);
  env->SetMethodNoSideEffect(target, "domainToASCII", DomainToASCII);
  env->SetMethodNoSideEffect(target, "domainToUnicode", DomainToUnicode);
  env->SetMethod(target, "setURLConstructor", SetURLConstructor);

#define XX(name, _) NODE_DEFINE_CONSTANT(target, name);
  FLAGS(XX)
#undef XX

#define XX(name) NODE_DEFINE_CONSTANT(target, name);
  PARSESTATES(XX)
#undef XX
}
}  // namespace

void RegisterExternalReferences(ExternalReferenceRegistry* registry) {
  registry->Register(Parse);
  registry->Register(EncodeAuthSet);
  registry->Register(DomainToASCII);
  registry->Register(DomainToUnicode);
  registry->Register(SetURLConstructor);
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
    std::string decoded = PercentDecode(part.c_str(), part.length());
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

URL URL::FromFilePath(const std::string& file_path) {
  URL url("file://");
  std::string escaped_file_path;
  for (size_t i = 0; i < file_path.length(); ++i) {
    escaped_file_path += file_path[i];
    if (file_path[i] == '%')
      escaped_file_path += "25";
  }
  URL::Parse(escaped_file_path.c_str(), escaped_file_path.length(), kPathStart,
             &url.context_, true, nullptr, false);
  return url;
}

// This function works by calling out to a JS function that creates and
// returns the JS URL object. Be mindful of the JS<->Native boundary
// crossing that is required.
MaybeLocal<Value> URL::ToObject(Environment* env) const {
  Isolate* isolate = env->isolate();
  Local<Context> context = env->context();
  Context::Scope context_scope(context);

  const Local<Value> undef = Undefined(isolate);
  const Local<Value> null = Null(isolate);

  if (context_.flags & URL_FLAGS_FAILED)
    return Local<Value>();

  Local<Value> argv[] = {
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
  SetArgs(env, argv, context_);

  MaybeLocal<Value> ret;
  {
    TryCatchScope try_catch(env, TryCatchScope::CatchMode::kFatal);

    // The SetURLConstructor method must have been called already to
    // set the constructor function used below. SetURLConstructor is
    // called automatically when the internal/url.js module is loaded
    // during the internal/bootstrap/node.js processing.
    ret = env->url_constructor_function()
        ->Call(env->context(), undef, arraysize(argv), argv);
  }

  return ret;
}

}  // namespace url
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(url, node::url::Initialize)
NODE_MODULE_EXTERNAL_REFERENCE(url, node::url::RegisterExternalReferences)
