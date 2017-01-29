#include "node_url.h"
#include "node.h"
#include "node_internals.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"
#include "base-object.h"
#include "base-object-inl.h"
#include "node_i18n.h"

#include <string>
#include <vector>
#include <stdio.h>
#include <cmath>

#if defined(NODE_HAVE_I18N_SUPPORT)
#include <unicode/utf8.h>
#include <unicode/utf.h>
#endif

namespace node {

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Null;
using v8::Object;
using v8::String;
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

#define CANNOT_BE_BASE() url.flags |= URL_FLAGS_CANNOT_BE_BASE;
#define INVALID_PARSE_STATE() url.flags |= URL_FLAGS_INVALID_PARSE_STATE;
#define SPECIAL()                                                             \
  {                                                                           \
    url.flags |= URL_FLAGS_SPECIAL;                                           \
    special = true;                                                           \
  }
#define TERMINATE()                                                           \
  {                                                                           \
    url.flags |= URL_FLAGS_TERMINATED;                                        \
    goto done;                                                                \
  }
#define URL_FAILED()                                                          \
  {                                                                           \
    url.flags |= URL_FLAGS_FAILED;                                            \
    goto done;                                                                \
  }

#define CHECK_FLAG(flags, name) (flags & URL_FLAGS_##name) /* NOLINT */

#define IS_CANNOT_BE_BASE(flags) CHECK_FLAG(flags, CANNOT_BE_BASE)
#define IS_FAILED(flags) CHECK_FLAG(flags, FAILED)

#define DOES_HAVE_SCHEME(url) CHECK_FLAG(url.flags, HAS_SCHEME)
#define DOES_HAVE_USERNAME(url) CHECK_FLAG(url.flags, HAS_USERNAME)
#define DOES_HAVE_PASSWORD(url) CHECK_FLAG(url.flags, HAS_PASSWORD)
#define DOES_HAVE_HOST(url) CHECK_FLAG(url.flags, HAS_HOST)
#define DOES_HAVE_PATH(url) CHECK_FLAG(url.flags, HAS_PATH)
#define DOES_HAVE_QUERY(url) CHECK_FLAG(url.flags, HAS_QUERY)
#define DOES_HAVE_FRAGMENT(url) CHECK_FLAG(url.flags, HAS_FRAGMENT)

#define SET_HAVE_SCHEME() url.flags |= URL_FLAGS_HAS_SCHEME;
#define SET_HAVE_USERNAME() url.flags |= URL_FLAGS_HAS_USERNAME;
#define SET_HAVE_PASSWORD() url.flags |= URL_FLAGS_HAS_PASSWORD;
#define SET_HAVE_HOST() url.flags |= URL_FLAGS_HAS_HOST;
#define SET_HAVE_PATH() url.flags |= URL_FLAGS_HAS_PATH;
#define SET_HAVE_QUERY() url.flags |= URL_FLAGS_HAS_QUERY;
#define SET_HAVE_FRAGMENT() url.flags |= URL_FLAGS_HAS_FRAGMENT;

#define UTF8STRING(isolate, str)                                              \
  String::NewFromUtf8(isolate, str.c_str(), v8::NewStringType::kNormal)       \
    .ToLocalChecked()

namespace url {

#if defined(NODE_HAVE_I18N_SUPPORT)
  static bool ToUnicode(std::string* input, std::string* output) {
    MaybeStackBuffer<char> buf;
    if (i18n::ToUnicode(&buf, input->c_str(), input->length()) < 0)
      return false;
    output->assign(*buf, buf.length());
    return true;
  }

  static bool ToASCII(std::string* input, std::string* output) {
    MaybeStackBuffer<char> buf;
    if (i18n::ToASCII(&buf, input->c_str(), input->length()) < 0)
      return false;
    output->assign(*buf, buf.length());
    return true;
  }

  // Unfortunately there's not really a better way to do this.
  // Iterate through each encoded codepoint and verify that
  // it is a valid unicode codepoint.
  static bool IsValidUTF8(std::string* input) {
    const char* p = input->c_str();
    int32_t len = input->length();
    for (int32_t i = 0; i < len;) {
      UChar32 c;
      U8_NEXT_UNSAFE(p, i, c);
      if (!U_IS_UNICODE_CHAR(c))
        return false;
    }
    return true;
  }
#else
  // Intentional non-ops if ICU is not present.
  static bool ToUnicode(std::string* input, std::string* output) {
    *output = *input;
    return true;
  }

  static bool ToASCII(std::string* input, std::string* output) {
    *output = *input;
    return true;
  }

  static bool IsValidUTF8(std::string* input) {
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
    unsigned value, len, swaps, dots;
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
      while (len < 4 && ASCII_HEX_DIGIT(ch)) {
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
          dots = 0;
          while (ch != kEOL) {
            value = 0xffffffff;
            if (!ASCII_DIGIT(ch))
              goto end;
            while (ASCII_DIGIT(ch)) {
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
            if (dots < 3 && ch != '.')
              goto end;
            *piece_pointer = *piece_pointer * 0x100 + value;
            if (dots & 0x1)
              piece_pointer++;
            if (ch != kEOL) {
              pointer++;
              ch = pointer < end ? pointer[0] : kEOL;
            }
            if (dots == 3 && ch != kEOL)
              goto end;
            dots++;
          }
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
          if (!ASCII_DIGIT(ch))
            return -1;
          break;
        case 16:
          if (!ASCII_HEX_DIGIT(ch))
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
        if (pointer - mark == 0)
          break;
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

  static url_host_type ParseHost(url_host* host,
                                 const char* input,
                                 size_t length,
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

    // First, we have to percent decode
    if (PercentDecode(input, length, &decoded) < 0)
      goto end;

    // If there are any invalid UTF8 byte sequences, we have to fail.
    // Unfortunately this means iterating through the string and checking
    // each decoded codepoint.
    if (!IsValidUTF8(&decoded))
      goto end;

    // Then we have to punycode toASCII
    if (!ToASCII(&decoded, &decoded))
      goto end;

    // If any of the following characters are still present, we have to fail
    for (size_t n = 0; n < decoded.size(); n++) {
      const char ch = decoded[n];
      if (ch == 0x00 || ch == 0x09 || ch == 0x0a || ch == 0x0d ||
          ch == 0x20 || ch == '#' || ch == '%' || ch == '/' ||
          ch == '?' || ch == '@' || ch == '[' || ch == '\\' ||
          ch == ']') {
        goto end;
      }
    }

    // Check to see if it's an IPv4 IP address
    type = ParseIPv4Host(host, decoded.c_str(), decoded.length());
    if (type == HOST_TYPE_IPV4 || type == HOST_TYPE_FAILED)
      goto end;

    // If the unicode flag is set, run the result through punycode ToUnicode
    if (unicode && !ToUnicode(&decoded, &decoded))
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
        for (int n = 0; n <= 7; n++) {
          uint16_t* piece = &host->value.ipv6[n];
          if (compress_pointer == piece) {
            *dest += n == 0 ? "::" : ":";
            while (*piece == 0 && ++n < 8)
              piece = &host->value.ipv6[n];
            if (n == 8)
              break;
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
      case HOST_TYPE_FAILED:
        break;
    }
    return host->type;
  }

  static bool ParseHost(std::string* input,
                        std::string* output,
                        bool unicode = false) {
    if (input->length() == 0)
      return true;
    url_host host{{""}, HOST_TYPE_DOMAIN};
    ParseHost(&host, input->c_str(), input->length(), unicode);
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
                                  std::vector<std::string> vec) {
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

    GET_AND_SET(env, base_obj, scheme, base, URL_FLAGS_HAS_SCHEME);
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
    }
    Local<Value> scheme = GET(env, context_obj, "scheme");
    if (scheme->IsString()) {
      Utf8Value value(env->isolate(), scheme);
      context->scheme.assign(*value, value.length());
    }
    Local<Value> port = GET(env, context_obj, "port");
    if (port->IsInt32())
      context->port = port->Int32Value(env->context()).FromJust();
  }

  // Single dot segment can be ".", "%2e", or "%2E"
  static inline bool IsSingleDotSegment(std::string str) {
    switch (str.size()) {
      case 1:
        return str == ".";
      case 3:
        return str[0] == '%' &&
               str[1] == '2' &&
               TO_LOWER(str[2]) == 'e';
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
                 TO_LOWER(str[3]) == 'e') ||
                (str[0] == '%' &&
                 str[1] == '2' &&
                 TO_LOWER(str[2]) == 'e' &&
                 str[3] == '.'));
      case 6:
        return (str[0] == '%' &&
                str[1] == '2' &&
                TO_LOWER(str[2]) == 'e' &&
                str[3] == '%' &&
                str[4] == '2' &&
                TO_LOWER(str[5]) == 'e');
      default:
        return false;
    }
  }

  static inline void ShortenUrlPath(struct url_data* url) {
    if (url->path.empty()) return;
    if (url->path.size() == 1 && url->scheme == "file:" &&
        NORMALIZED_WINDOWS_DRIVE_LETTER(url->path[0])) return;
    url->path.pop_back();
  }

  static void Parse(Environment* env,
                    Local<Value> recv,
                    const char* input,
                    const size_t len,
                    enum url_parse_state state_override,
                    Local<Value> base_obj,
                    Local<Value> context_obj,
                    Local<Function> cb) {
    Isolate* isolate = env->isolate();
    Local<Context> context = env->context();
    HandleScope handle_scope(isolate);
    Context::Scope context_scope(context);

    const bool has_base = base_obj->IsObject();
    bool atflag = false;
    bool sbflag = false;
    bool uflag = false;
    bool base_is_file = false;
    int wskip = 0;

    struct url_data base;
    struct url_data url;
    if (context_obj->IsObject())
      HarvestContext(env, &url, context_obj.As<Object>());
    if (has_base)
      HarvestBase(env, &base, base_obj.As<Object>());

    std::string buffer;
    url.scheme.reserve(len);
    url.username.reserve(len);
    url.password.reserve(len);
    url.host.reserve(len);
    url.path.reserve(len);
    url.query.reserve(len);
    url.fragment.reserve(len);
    buffer.reserve(len);

    // Set the initial parse state.
    const bool has_state_override = state_override != kUnknownState;
    enum url_parse_state state = has_state_override ? state_override :
                                                      kSchemeStart;

    const char* p = input;
    const char* end = input + len;

    if (state < kSchemeStart || state > kFragment) {
      INVALID_PARSE_STATE();
      goto done;
    }

    while (p <= end) {
      const char ch = p < end ? p[0] : kEOL;

      if (TAB_AND_NEWLINE(ch)) {
        if (state == kAuthority) {
          // It's necessary to keep track of how much whitespace
          // is being ignored when in kAuthority state because of
          // how the buffer is managed. TODO: See if there's a better
          // way
          wskip++;
        }
        p++;
        continue;
      }

      bool special = url.flags & URL_FLAGS_SPECIAL;
      const bool special_back_slash = (special && ch == '\\');
      switch (state) {
        case kSchemeStart:
          if (ASCII_ALPHA(ch)) {
            buffer += TO_LOWER(ch);
            state = kScheme;
          } else if (!has_state_override) {
            state = kNoScheme;
            continue;
          } else {
            TERMINATE()
          }
          break;
        case kScheme:
          if (SCHEME_CHAR(ch)) {
            buffer += TO_LOWER(ch);
            p++;
            continue;
          } else if (ch == ':' || (has_state_override && ch == kEOL)) {
            buffer += ':';
            if (buffer.size() > 0) {
              SET_HAVE_SCHEME()
              url.scheme = buffer;
            }
            if (IsSpecial(url.scheme)) {
              SPECIAL()
            } else {
              url.flags &= ~URL_FLAGS_SPECIAL;
            }
            if (has_state_override)
              goto done;
            buffer.clear();
            if (url.scheme == "file:") {
              state = kFile;
            } else if (special &&
                       has_base &&
                       DOES_HAVE_SCHEME(base) &&
                       url.scheme == base.scheme) {
              state = kSpecialRelativeOrAuthority;
            } else if (special) {
              state = kSpecialAuthoritySlashes;
            } else if (p[1] == '/') {
              state = kPathOrAuthority;
              p++;
            } else {
              CANNOT_BE_BASE()
              SET_HAVE_PATH()
              url.path.push_back("");
              state = kCannotBeBase;
            }
          } else if (!has_state_override) {
            buffer.clear();
            state = kNoScheme;
            p = input;
            continue;
          } else {
            TERMINATE()
          }
          break;
        case kNoScheme:
          if (!has_base || (IS_CANNOT_BE_BASE(base.flags) && ch != '#')) {
            URL_FAILED()
          } else if (IS_CANNOT_BE_BASE(base.flags) && ch == '#') {
            SET_HAVE_SCHEME()
            url.scheme = base.scheme;
            if (IsSpecial(url.scheme)) {
              SPECIAL()
            } else {
              url.flags &= ~URL_FLAGS_SPECIAL;
            }
            if (DOES_HAVE_PATH(base)) {
              SET_HAVE_PATH()
              url.path = base.path;
            }
            if (DOES_HAVE_QUERY(base)) {
              SET_HAVE_QUERY()
              url.query = base.query;
            }
            if (DOES_HAVE_FRAGMENT(base)) {
              SET_HAVE_FRAGMENT()
              url.fragment = base.fragment;
            }
            CANNOT_BE_BASE()
            state = kFragment;
          } else if (has_base &&
                     DOES_HAVE_SCHEME(base) &&
                     base.scheme != "file:") {
            state = kRelative;
            continue;
          } else {
            SET_HAVE_SCHEME()
            url.scheme = "file:";
            SPECIAL()
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
          SET_HAVE_SCHEME()
          url.scheme = base.scheme;
          if (IsSpecial(url.scheme)) {
            SPECIAL()
          } else {
            url.flags &= ~URL_FLAGS_SPECIAL;
          }
          switch (ch) {
            case kEOL:
              if (DOES_HAVE_USERNAME(base)) {
                SET_HAVE_USERNAME()
                url.username = base.username;
              }
              if (DOES_HAVE_PASSWORD(base)) {
                SET_HAVE_PASSWORD()
                url.password = base.password;
              }
              if (DOES_HAVE_HOST(base)) {
                SET_HAVE_HOST()
                url.host = base.host;
              }
              if (DOES_HAVE_QUERY(base)) {
                SET_HAVE_QUERY()
                url.query = base.query;
              }
              if (DOES_HAVE_PATH(base)) {
                SET_HAVE_PATH()
                url.path = base.path;
              }
              url.port = base.port;
              break;
            case '/':
              state = kRelativeSlash;
              break;
            case '?':
              if (DOES_HAVE_USERNAME(base)) {
                SET_HAVE_USERNAME()
                url.username = base.username;
              }
              if (DOES_HAVE_PASSWORD(base)) {
                SET_HAVE_PASSWORD()
                url.password = base.password;
              }
              if (DOES_HAVE_HOST(base)) {
                SET_HAVE_HOST()
                url.host = base.host;
              }
              if (DOES_HAVE_PATH(base)) {
                SET_HAVE_PATH()
                url.path = base.path;
              }
              url.port = base.port;
              state = kQuery;
              break;
            case '#':
              if (DOES_HAVE_USERNAME(base)) {
                SET_HAVE_USERNAME()
                url.username = base.username;
              }
              if (DOES_HAVE_PASSWORD(base)) {
                SET_HAVE_PASSWORD()
                url.password = base.password;
              }
              if (DOES_HAVE_HOST(base)) {
                SET_HAVE_HOST()
                url.host = base.host;
              }
              if (DOES_HAVE_QUERY(base)) {
                SET_HAVE_QUERY()
                url.query = base.query;
              }
              if (DOES_HAVE_PATH(base)) {
                SET_HAVE_PATH()
                url.path = base.path;
              }
              url.port = base.port;
              state = kFragment;
              break;
            default:
              if (special_back_slash) {
                state = kRelativeSlash;
              } else {
                if (DOES_HAVE_USERNAME(base)) {
                  SET_HAVE_USERNAME()
                  url.username = base.username;
                }
                if (DOES_HAVE_PASSWORD(base)) {
                  SET_HAVE_PASSWORD()
                  url.password = base.password;
                }
                if (DOES_HAVE_HOST(base)) {
                  SET_HAVE_HOST()
                  url.host = base.host;
                }
                if (DOES_HAVE_PATH(base)) {
                  SET_HAVE_PATH()
                  url.path = base.path;
                  ShortenUrlPath(&url);
                }
                url.port = base.port;
                state = kPath;
                continue;
              }
          }
          break;
        case kRelativeSlash:
          if (ch == '/' || special_back_slash) {
            state = kSpecialAuthorityIgnoreSlashes;
          } else {
            if (DOES_HAVE_USERNAME(base)) {
              SET_HAVE_USERNAME()
              url.username = base.username;
            }
            if (DOES_HAVE_PASSWORD(base)) {
              SET_HAVE_PASSWORD()
              url.password = base.password;
            }
            if (DOES_HAVE_HOST(base)) {
              SET_HAVE_HOST()
              url.host = base.host;
            }
            url.port = base.port;
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
              SET_HAVE_USERNAME()
            }
            for (size_t n = 0; n < blen; n++) {
              const char bch = buffer[n];
              if (bch == ':') {
                SET_HAVE_PASSWORD()
                if (!uflag) {
                  uflag = true;
                  continue;
                }
              }
              if (uflag) {
                AppendOrEscape(&url.password, bch, UserinfoEncodeSet);
              } else {
                AppendOrEscape(&url.username, bch, UserinfoEncodeSet);
              }
            }
            buffer.clear();
          } else if (ch == kEOL ||
                     ch == '/' ||
                     ch == '?' ||
                     ch == '#' ||
                     special_back_slash) {
            p -= buffer.size() + 1 + wskip;
            buffer.clear();
            state = kHost;
          } else {
            buffer += ch;
          }
          break;
        case kHost:
        case kHostname:
          if (ch == ':' && !sbflag) {
            if (special && buffer.size() == 0)
              URL_FAILED()
            SET_HAVE_HOST()
            if (!ParseHost(&buffer, &url.host))
              URL_FAILED()
            buffer.clear();
            state = kPort;
            if (state_override == kHostname)
              TERMINATE()
          } else if (ch == kEOL ||
                     ch == '/' ||
                     ch == '?' ||
                     ch == '#' ||
                     special_back_slash) {
            p--;
            if (special && buffer.size() == 0)
              URL_FAILED()
            SET_HAVE_HOST()
            if (!ParseHost(&buffer, &url.host))
              URL_FAILED()
            buffer.clear();
            state = kPathStart;
            if (has_state_override)
              TERMINATE()
          } else {
            if (ch == '[')
              sbflag = true;
            if (ch == ']')
              sbflag = false;
            buffer += TO_LOWER(ch);
          }
          break;
        case kPort:
          if (ASCII_DIGIT(ch)) {
            buffer += ch;
          } else if (has_state_override ||
                     ch == kEOL ||
                     ch == '/' ||
                     ch == '?' ||
                     ch == '#' ||
                     special_back_slash) {
            if (buffer.size() > 0) {
              int port = 0;
              for (size_t i = 0; i < buffer.size(); i++)
                port = port * 10 + buffer[i] - '0';
              if (port >= 0 && port <= 0xffff) {
                url.port = NormalizePort(url.scheme, port);
              } else if (!has_state_override) {
                URL_FAILED()
              }
              buffer.clear();
            }
            state = kPathStart;
            continue;
          } else {
            URL_FAILED();
          }
          break;
        case kFile:
          base_is_file = (
              has_base &&
              DOES_HAVE_SCHEME(base) &&
              base.scheme == "file:");
          switch (ch) {
            case kEOL:
              if (base_is_file) {
                if (DOES_HAVE_HOST(base)) {
                  SET_HAVE_HOST()
                  url.host = base.host;
                }
                if (DOES_HAVE_PATH(base)) {
                  SET_HAVE_PATH()
                  url.path = base.path;
                }
                if (DOES_HAVE_QUERY(base)) {
                  SET_HAVE_QUERY()
                  url.query = base.query;
                }
              }
              break;
            case '\\':
            case '/':
              state = kFileSlash;
              break;
            case '?':
              if (base_is_file) {
                if (DOES_HAVE_HOST(base)) {
                  SET_HAVE_HOST()
                  url.host = base.host;
                }
                if (DOES_HAVE_PATH(base)) {
                  SET_HAVE_PATH()
                  url.path = base.path;
                }
                SET_HAVE_QUERY()
                state = kQuery;
              }
              break;
            case '#':
              if (base_is_file) {
                if (DOES_HAVE_HOST(base)) {
                  SET_HAVE_HOST()
                  url.host = base.host;
                }
                if (DOES_HAVE_PATH(base)) {
                  SET_HAVE_PATH()
                  url.path = base.path;
                }
                if (DOES_HAVE_QUERY(base)) {
                  SET_HAVE_QUERY()
                  url.query = base.query;
                }
                state = kFragment;
              }
              break;
            default:
              if (base_is_file &&
                  (!WINDOWS_DRIVE_LETTER(ch, p[1]) ||
                   end - p == 1 ||
                   (p[2] != '/' &&
                    p[2] != '\\' &&
                    p[2] != '?' &&
                    p[2] != '#'))) {
                if (DOES_HAVE_HOST(base)) {
                  SET_HAVE_HOST()
                  url.host = base.host;
                }
                if (DOES_HAVE_PATH(base)) {
                  SET_HAVE_PATH()
                  url.path = base.path;
                }
                ShortenUrlPath(&url);
              }
              state = kPath;
              continue;
          }
          break;
        case kFileSlash:
          if (ch == '/' || ch == '\\') {
            state = kFileHost;
          } else {
            if (has_base &&
                DOES_HAVE_SCHEME(base) &&
                base.scheme == "file:" &&
                DOES_HAVE_PATH(base) &&
                base.path.size() > 0 &&
                NORMALIZED_WINDOWS_DRIVE_LETTER(base.path[0])) {
              SET_HAVE_PATH()
              url.path.push_back(base.path[0]);
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
            if (buffer.size() == 2 &&
                WINDOWS_DRIVE_LETTER(buffer[0], buffer[1])) {
              state = kPath;
            } else if (buffer.size() == 0) {
              state = kPathStart;
            } else {
              if (buffer != "localhost") {
                SET_HAVE_HOST()
                if (!ParseHost(&buffer, &url.host))
                  URL_FAILED()
              }
              buffer.clear();
              state = kPathStart;
            }
            continue;
          } else {
            buffer += ch;
          }
          break;
        case kPathStart:
          state = kPath;
          if (ch != '/' && !special_back_slash)
            continue;
          break;
        case kPath:
          if (ch == kEOL ||
              ch == '/' ||
              special_back_slash ||
              (!has_state_override && (ch == '?' || ch == '#'))) {
            if (IsDoubleDotSegment(buffer)) {
              ShortenUrlPath(&url);
              if (ch != '/' && !special_back_slash) {
                SET_HAVE_PATH()
                url.path.push_back("");
              }
            } else if (IsSingleDotSegment(buffer)) {
              if (ch != '/' && !special_back_slash) {
                SET_HAVE_PATH();
                url.path.push_back("");
              }
            } else {
              if (DOES_HAVE_SCHEME(url) &&
                  url.scheme == "file:" &&
                  url.path.empty() &&
                  buffer.size() == 2 &&
                  WINDOWS_DRIVE_LETTER(buffer[0], buffer[1])) {
                url.flags &= ~URL_FLAGS_HAS_HOST;
                buffer[1] = ':';
              }
              SET_HAVE_PATH()
              std::string segment(buffer.c_str(), buffer.size());
              url.path.push_back(segment);
            }
            buffer.clear();
            if (ch == '?') {
              SET_HAVE_QUERY()
              state = kQuery;
            } else if (ch == '#') {
              state = kFragment;
            }
          } else {
            AppendOrEscape(&buffer, ch, DefaultEncodeSet);
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
              if (url.path.size() == 0)
                url.path.push_back("");
              if (url.path.size() > 0 && ch != kEOL)
                AppendOrEscape(&url.path[0], ch, SimpleEncodeSet);
          }
          break;
        case kQuery:
          if (ch == kEOL || (!has_state_override && ch == '#')) {
            SET_HAVE_QUERY()
            url.query = buffer;
            buffer.clear();
            if (ch == '#')
              state = kFragment;
          } else {
            AppendOrEscape(&buffer, ch, QueryEncodeSet);
          }
          break;
        case kFragment:
          switch (ch) {
            case kEOL:
              SET_HAVE_FRAGMENT()
              url.fragment = buffer;
              break;
            case 0:
              break;
            default:
              AppendOrEscape(&buffer, ch, SimpleEncodeSet);
          }
          break;
        default:
          INVALID_PARSE_STATE()
          goto done;
      }

      p++;
    }

   done:

    // Define the return value placeholders
    const Local<Value> undef = Undefined(isolate);
    Local<Value> argv[9] = {
      undef,
      undef,
      undef,
      undef,
      undef,
      undef,
      undef,
      undef,
      undef,
    };

    argv[ARG_FLAGS] = Integer::NewFromUnsigned(isolate, url.flags);
    if (!IS_FAILED(url.flags)) {
      if (DOES_HAVE_SCHEME(url))
        argv[ARG_PROTOCOL] = OneByteString(isolate, url.scheme.c_str());
      if (DOES_HAVE_USERNAME(url))
        argv[ARG_USERNAME] = UTF8STRING(isolate, url.username);
      if (DOES_HAVE_PASSWORD(url))
        argv[ARG_PASSWORD] = UTF8STRING(isolate, url.password);
      if (DOES_HAVE_HOST(url))
        argv[ARG_HOST] = UTF8STRING(isolate, url.host);
      if (DOES_HAVE_QUERY(url))
        argv[ARG_QUERY] = UTF8STRING(isolate, url.query);
      if (DOES_HAVE_FRAGMENT(url))
        argv[ARG_FRAGMENT] = UTF8STRING(isolate, url.fragment);
      if (url.port > -1)
        argv[ARG_PORT] = Integer::New(isolate, url.port);
      if (DOES_HAVE_PATH(url))
        argv[ARG_PATH] = Copy(env, url.path);
    }

    (void)cb->Call(context, recv, 9, argv);
  }

  static void Parse(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK_GE(args.Length(), 5);
    CHECK(args[0]->IsString());
    CHECK(args[2]->IsUndefined() ||
          args[2]->IsNull() ||
          args[2]->IsObject());
    CHECK(args[3]->IsUndefined() ||
          args[3]->IsNull() ||
          args[3]->IsObject());
    CHECK(args[4]->IsFunction());
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
          args[4].As<Function>());
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
      AppendOrEscape(&output, ch, UserinfoEncodeSet);
    }
    args.GetReturnValue().Set(
        String::NewFromUtf8(env->isolate(),
                            output.c_str(),
                            v8::NewStringType::kNormal).ToLocalChecked());
  }

  static void DomainToASCII(const FunctionCallbackInfo<Value>& args) {
    Environment* env = Environment::GetCurrent(args);
    CHECK_GE(args.Length(), 1);
    CHECK(args[0]->IsString());
    Utf8Value value(env->isolate(), args[0]);

    url_host host{{""}, HOST_TYPE_DOMAIN};
    ParseHost(&host, *value, value.length());
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

  static void Init(Local<Object> target,
                   Local<Value> unused,
                   Local<Context> context,
                   void* priv) {
    Environment* env = Environment::GetCurrent(context);
    env->SetMethod(target, "parse", Parse);
    env->SetMethod(target, "encodeAuth", EncodeAuthSet);
    env->SetMethod(target, "domainToASCII", DomainToASCII);
    env->SetMethod(target, "domainToUnicode", DomainToUnicode);

#define XX(name, _) NODE_DEFINE_CONSTANT(target, name);
    FLAGS(XX)
#undef XX

#define XX(name) NODE_DEFINE_CONSTANT(target, name);
    ARGS(XX)
    PARSESTATES(XX)
#undef XX
  }
}  // namespace url
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(url, node::url::Init)
