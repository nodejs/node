/*
 * notes: by srl295
 *  - When in NODE_HAVE_SMALL_ICU mode, ICU is linked against "stub" (null) data
 *     ( stubdata/libicudata.a ) containing nothing, no data, and it's also
 *    linked against a "small" data file which the SMALL_ICUDATA_ENTRY_POINT
 *    macro names. That's the "english+root" data.
 *
 *    If icu_data_path is non-null, the user has provided a path and we assume
 *    it goes somewhere useful. We set that path in ICU, and exit.
 *    If icu_data_path is null, they haven't set a path and we want the
 *    "english+root" data.  We call
 *       udata_setCommonData(SMALL_ICUDATA_ENTRY_POINT,...)
 *    to load up the english+root data.
 *
 *  - when NOT in NODE_HAVE_SMALL_ICU mode, ICU is linked directly with its full
 *    data. All of the variables and command line options for changing data at
 *    runtime are disabled, as they wouldn't fully override the internal data.
 *    See:  http://bugs.icu-project.org/trac/ticket/10924
 */


#include "node_i18n.h"

#if defined(NODE_HAVE_I18N_SUPPORT)

#include "node.h"
#include "node_buffer.h"
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

#include <unicode/putil.h>
#include <unicode/udata.h>
#include <unicode/uidna.h>
#include <unicode/utypes.h>
#include <unicode/ustring.h>
#include <unicode/ucnv.h>
#include <unicode/utf8.h>
#include <unicode/utf16.h>
#include <unicode/normalizer2.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>

#ifdef NODE_HAVE_SMALL_ICU
/* if this is defined, we have a 'secondary' entry point.
   compare following to utypes.h defs for U_ICUDATA_ENTRY_POINT */
#define SMALL_ICUDATA_ENTRY_POINT \
  SMALL_DEF2(U_ICU_VERSION_MAJOR_NUM, U_LIB_SUFFIX_C_NAME)
#define SMALL_DEF2(major, suff) SMALL_DEF(major, suff)
#ifndef U_LIB_SUFFIX_C_NAME
#define SMALL_DEF(major, suff) icusmdt##major##_dat
#else
#define SMALL_DEF(major, suff) icusmdt##suff##major##_dat
#endif

extern "C" const char U_DATA_API SMALL_ICUDATA_ENTRY_POINT[];
#endif

namespace node {

using v8::Context;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Object;
using v8::String;
using v8::Uint32;
using v8::Value;

bool flag_icu_data_dir = false;

namespace i18n {

Local<Value> ICUException(Environment* env,
                          UErrorCode status,
                          const char* msg) {
  Local<String> estring = OneByteString(env->isolate(), u_errorName(status));
  if (msg == nullptr || msg[0] == '\0')
    msg = "Unspecified ICU Exception";
  Local<String> cons =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(env->isolate(), ", "));
  cons = String::Concat(cons, OneByteString(env->isolate(), msg));

  Local<Value> e = Exception::Error(cons);
  Local<Object> obj = e->ToObject(env->isolate());
  obj->Set(env->errno_string(), Integer::New(env->isolate(), status));
  obj->Set(env->code_string(), estring);
  return e;
}

#define THROW_ICU_ERROR(env, status, msg)                                     \
  do {                                                                        \
    v8::HandleScope scope(env->isolate());                                    \
    env->isolate()->ThrowException(ICUException(env, status, msg));           \
  }                                                                           \
  while (0)

MaybeLocal<Object> AsBuffer(Isolate* isolate,
                            MaybeStackBuffer<char>* buf,
                            size_t len) {
  if (buf->IsAllocated()) {
    MaybeLocal<Object> ret = Buffer::New(isolate, buf->out(), len);
    buf->Release();
    return ret;
  }
  return Buffer::Copy(isolate, buf->out(), len);
}

MaybeLocal<Object> AsBuffer(Isolate* isolate,
                            MaybeStackBuffer<UChar>* buf,
                            size_t len) {
  if (IsBigEndian()) {
    uint16_t* dst = reinterpret_cast<uint16_t*>(buf->out());
    SwapBytes(dst, dst, len);
  }
  if (buf->IsAllocated()) {
    MaybeLocal<Object> ret =
        Buffer::New(isolate, reinterpret_cast<char*>(buf->out()), len);
    buf->Release();
    return ret;
  }
  return Buffer::Copy(isolate, reinterpret_cast<char*>(buf->out()), len);
}

struct Converter {
  Converter(Environment* env, const char* name)
      : conv(nullptr) {
    UErrorCode status = U_ZERO_ERROR;
    conv = ucnv_open(name, &status);
    if (U_FAILURE(status)) {
      THROW_ICU_ERROR(env, status, "Unable to create converter");
    }
  }

  ~Converter() {
    if (conv != nullptr)
      ucnv_close(conv);
  }

  UConverter* conv;
};

bool InitializeICUDirectory(const char* icu_data_path) {
  if (icu_data_path != nullptr) {
    flag_icu_data_dir = true;
    u_setDataDirectory(icu_data_path);
    return true;  // no error
  } else {
    UErrorCode status = U_ZERO_ERROR;
#ifdef NODE_HAVE_SMALL_ICU
    // install the 'small' data.
    udata_setCommonData(&SMALL_ICUDATA_ENTRY_POINT, &status);
#else  // !NODE_HAVE_SMALL_ICU
    // no small data, so nothing to do.
#endif  // !NODE_HAVE_SMALL_ICU
    return (status == U_ZERO_ERROR);
  }
}

static int32_t ToUnicode(MaybeStackBuffer<char>* buf,
                         const char* input,
                         size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t options = UIDNA_DEFAULT;
  options |= UIDNA_NONTRANSITIONAL_TO_UNICODE;
  UIDNA* uidna = uidna_openUTS46(options, &status);
  if (U_FAILURE(status))
    return -1;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;

  int32_t len = uidna_nameToUnicodeUTF8(uidna,
                                        input, length,
                                        **buf, buf->length(),
                                        &info,
                                        &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    buf->AllocateSufficientStorage(len);
    len = uidna_nameToUnicodeUTF8(uidna,
                                  input, length,
                                  **buf, buf->length(),
                                  &info,
                                  &status);
  }

  if (U_FAILURE(status))
    len = -1;

  uidna_close(uidna);
  return len;
}

static int32_t ToASCII(MaybeStackBuffer<char>* buf,
                       const char* input,
                       size_t length) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t options = UIDNA_DEFAULT;
  options |= UIDNA_NONTRANSITIONAL_TO_ASCII;
  UIDNA* uidna = uidna_openUTS46(options, &status);
  if (U_FAILURE(status))
    return -1;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;

  int32_t len = uidna_nameToASCII_UTF8(uidna,
                                       input, length,
                                       **buf, buf->length(),
                                       &info,
                                       &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    buf->AllocateSufficientStorage(len);
    len = uidna_nameToASCII_UTF8(uidna,
                                 input, length,
                                 **buf, buf->length(),
                                 &info,
                                 &status);
  }

  if (U_FAILURE(status))
    len = -1;

  uidna_close(uidna);
  return len;
}

static void ToUnicode(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value val(env->isolate(), args[0]);
  MaybeStackBuffer<char> buf;
  int32_t len = ToUnicode(&buf, *val, val.length());

  if (len < 0) {
    return env->ThrowError("Cannot convert name to Unicode");
  }

  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          *buf,
                          v8::NewStringType::kNormal,
                          len).ToLocalChecked());
}

static void ToASCII(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  CHECK_GE(args.Length(), 1);
  CHECK(args[0]->IsString());
  Utf8Value val(env->isolate(), args[0]);
  MaybeStackBuffer<char> buf;
  int32_t len = ToASCII(&buf, *val, val.length());

  if (len < 0) {
    return env->ThrowError("Cannot convert name to ASCII");
  }

  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          *buf,
                          v8::NewStringType::kNormal,
                          len).ToLocalChecked());
}

// One-Shot Converters

// Converts the Buffer from one named encoding to another.
// args[0] is a string identifying the encoding we're converting to.
// args[1] is a string identifying the encoding we're converting from.
// args[2] must be a buffer instance
void Convert(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());
  CHECK(args[1]->IsString());
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[2]);

  Utf8Value to_name(env->isolate(), args[0]);
  Utf8Value from_name(env->isolate(), args[1]);

  SPREAD_ARG(args[2], ts_obj);

  UErrorCode status = U_ZERO_ERROR;

  MaybeStackBuffer<char> buf;
  uint32_t len, limit;
  char* target;
  const char* data = ts_obj_data;

  Converter to(env, *to_name);
  Converter from(env, *from_name);
  if (to.conv == nullptr || from.conv == nullptr)
    return;

  ucnv_setSubstChars(to.conv, "?", 1, &status);
  status = U_ZERO_ERROR;

  limit = ts_obj_length * ucnv_getMaxCharSize(to.conv);
  buf.AllocateSufficientStorage(ts_obj_length * ucnv_getMaxCharSize(to.conv));
  target = *buf;

  ucnv_convertEx(to.conv, from.conv,
                 &target, target + limit,
                 &data, ts_obj_data + ts_obj_length,
                 NULL, NULL, NULL, NULL,
                 true, true,
                 &status);

  if (U_SUCCESS(status)) {
    len = target - *buf;
    args.GetReturnValue().Set(
      AsBuffer(env->isolate(), &buf, len).ToLocalChecked());
  } else {
    THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
  }
}

// Converts to UCS2 from ISO-8859-1 and US-ASCII
// args[0] is the encoding to convert from
// args[1] must be a buffer instance
void ConvertToUcs2(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[1]);

  Utf8Value name(env->isolate(), args[0]);

  SPREAD_ARG(args[1], ts_obj);

  UErrorCode status = U_ZERO_ERROR;
  size_t len;

  MaybeStackBuffer<UChar> buf(ts_obj_length);

  Converter from(env, *name);
  if (from.conv == nullptr)
    return;

  len = ucnv_toUChars(from.conv,
                      *buf, ts_obj_length << 1,
                      ts_obj_data, ts_obj_length,
                      &status);

  if (U_FAILURE(status)) {
    THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
    return;
  }
  args.GetReturnValue().Set(
    AsBuffer(env->isolate(), &buf, ts_obj_length << 1).ToLocalChecked());
}

// Convert to a named encoding from UCS2
// args[0] is the name of the encoding being converted to
// args[1] must be a buffer instance
void ConvertFromUcs2(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());
  THROW_AND_RETURN_UNLESS_BUFFER(env, args[1]);

  Utf8Value name(env->isolate(), args[0]);
  SPREAD_ARG(args[1], ts_obj);

  UErrorCode status = U_ZERO_ERROR;

  UChar* source = nullptr;
  MaybeStackBuffer<UChar> swapspace;
  const size_t length = ts_obj_length >> 1;

  if (IsLittleEndian()) {
    source = reinterpret_cast<UChar*>(ts_obj_data);
  } else {
    swapspace.AllocateSufficientStorage(length);
    for (size_t n = 0, i = 0; i < length; n += 2, i += 1) {
      const uint8_t hi = static_cast<uint8_t>(ts_obj_data[n + 0]);
      const uint8_t lo = static_cast<uint8_t>(ts_obj_data[n + 1]);
      swapspace[i] = (hi << 8) | lo;
    }
    source = *swapspace;
  }

  uint32_t len;
  MaybeStackBuffer<char> buf(length);

  Converter to(env, *name);
  if (to.conv == nullptr)
    return;
  ucnv_setSubstChars(to.conv, "?", 1, &status);
  status = U_ZERO_ERROR;

  len = ucnv_fromUChars(to.conv, *buf, length, source, length, &status);

  if (U_SUCCESS(status)) {
    args.GetReturnValue().Set(
      AsBuffer(env->isolate(), &buf, len).ToLocalChecked());
  } else {
    THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
  }
}

// Converts from UTF-8 to UCS2
// args[0] must be a buffer instance
void Ucs2FromUtf8(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_ARG(args[0], ts_obj);

  UErrorCode status = U_ZERO_ERROR;

  MaybeStackBuffer<UChar> buf;
  int32_t len;

  u_strFromUTF8(*buf, 1024, &len, ts_obj_data, ts_obj_length, &status);

  if (U_SUCCESS(status)) {
    args.GetReturnValue().Set(
        AsBuffer(env->isolate(), &buf, len * sizeof(UChar)).ToLocalChecked());
  } else if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    buf.AllocateSufficientStorage(len);
    u_strFromUTF8(*buf, len, &len, ts_obj_data, ts_obj_length, &status);
    if (U_FAILURE(status)) {
      THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
      return;
    }
    args.GetReturnValue().Set(
      AsBuffer(env->isolate(), &buf, len << 1).ToLocalChecked());
  } else {
    THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
  }
}

// Converts UCS2 into UTF-8
// args[0] must be a buffer instance
void Utf8FromUcs2(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_ARG(args[0], ts_obj);

  const size_t length = ts_obj_length >> 1;

  UChar* source = nullptr;
  MaybeStackBuffer<UChar> swapspace;
  if (IsLittleEndian()) {
    source = reinterpret_cast<UChar*>(ts_obj_data);
  } else {
    swapspace.AllocateSufficientStorage(length);
    for (size_t n = 0, i = 0; i < length; n += 2, i += 1) {
      const uint8_t hi = static_cast<uint8_t>(ts_obj_data[n + 0]);
      const uint8_t lo = static_cast<uint8_t>(ts_obj_data[n + 1]);
      swapspace[i] = (hi << 8) | lo;
    }
    source = *swapspace;
  }

  UErrorCode status = U_ZERO_ERROR;

  MaybeStackBuffer<char> buf;
  int32_t len;

  u_strToUTF8(*buf, 1024, &len, source, length, &status);
  if (U_SUCCESS(status)) {
    // Used the static buf, just copy it into the new Buffer instance.
    args.GetReturnValue().Set(
      AsBuffer(env->isolate(), &buf, len).ToLocalChecked());
  } else if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    buf.AllocateSufficientStorage(len);
    u_strToUTF8(*buf, len, &len, source, length, &status);
    if (U_FAILURE(status)) {
      THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
      return;
    }
    args.GetReturnValue().Set(
      Buffer::New(env->isolate(), *buf, len).ToLocalChecked());
    buf.Release();
  } else {
    THROW_ICU_ERROR(env, status, "Unable to transcode buffer");
  }
}

void NormalizeBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  THROW_AND_RETURN_UNLESS_BUFFER(env, args[0]);
  SPREAD_ARG(args[0], ts_obj);

  Utf8Value encoding(env->isolate(), args[1]);
  Utf8Value form(env->isolate(), args[2]);

  icu::UnicodeString input(ts_obj_data, ts_obj_length, *encoding);
  icu::UnicodeString output;

  UErrorCode status = U_ZERO_ERROR;
  const icu::Normalizer2* normalizer = nullptr;

  if (strcmp(*form, "NFC") == 0) {
    normalizer = icu::Normalizer2::getNFCInstance(status);
  } else if (strcmp(*form, "NFD") == 0) {
    normalizer = icu::Normalizer2::getNFDInstance(status);
  } else if (strcmp(*form, "NFKC") == 0) {
    normalizer = icu::Normalizer2::getNFKCInstance(status);
  } else if (strcmp(*form, "NFKD") == 0) {
    normalizer = icu::Normalizer2::getNFKDInstance(status);
  } else {
    return env->ThrowTypeError("Unknown Unicode normalization form");
  }
  if (U_FAILURE(status)) {
    THROW_ICU_ERROR(env, status, "Unable to initialize normalizer");
    return;
  }
  status = U_ZERO_ERROR;
  normalizer->normalize(input, output, status);
  if (U_FAILURE(status)) {
    THROW_ICU_ERROR(env, status, "Unable to normalize buffer");
    return;
  }
  status = U_ZERO_ERROR;

  MaybeStackBuffer<char> res;
  int32_t len;
  u_strToUTF8(*res, 1024, &len,
              output.getTerminatedBuffer(),
              output.length(),
              &status);
  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    res.AllocateSufficientStorage(len);
    u_strToUTF8(*res, len, &len,
                output.getTerminatedBuffer(),
                output.length(),
                &status);
  }
  if (U_FAILURE(status)) {
    THROW_ICU_ERROR(env, status, "Unable to process normalized output");
    return;
  }

  args.GetReturnValue().Set(
    AsBuffer(env->isolate(), &res, len).ToLocalChecked());
}

static void GetCharacterProperty(const FunctionCallbackInfo<Value>& args) {
  if (args.Length() < 2)
    return;
  UChar32 codepoint = args[0]->Uint32Value();
  UProperty property = static_cast<UProperty>(args[1]->Int32Value());
  if (!u_isdefined(codepoint))
    return;
  if (property < UCHAR_BINARY_LIMIT) {
    args.GetReturnValue().Set(
      static_cast<bool>(u_hasBinaryProperty(codepoint, property)));
  } else if (property >= UCHAR_INT_START && property < UCHAR_MASK_LIMIT) {
    args.GetReturnValue().Set(u_getIntPropertyValue(codepoint, property));
  } else if (property == UCHAR_NUMERIC_VALUE) {
    args.GetReturnValue().Set(u_getNumericValue(codepoint));
  } else if (property >= UCHAR_STRING_START && property < UCHAR_STRING_LIMIT) {
    switch (property) {
      case UCHAR_BIDI_MIRRORING_GLYPH:
        args.GetReturnValue().Set(u_charMirror(codepoint));
        break;
      case UCHAR_SIMPLE_LOWERCASE_MAPPING:
        args.GetReturnValue().Set(u_tolower(codepoint));
        break;
      case UCHAR_SIMPLE_TITLECASE_MAPPING:
        args.GetReturnValue().Set(u_totitle(codepoint));
        break;
      case UCHAR_SIMPLE_UPPERCASE_MAPPING:
        args.GetReturnValue().Set(u_toupper(codepoint));
        break;
      case UCHAR_BIDI_PAIRED_BRACKET:
        args.GetReturnValue().Set(u_getBidiPairedBracket(codepoint));
        break;
      default:
        // Do nothing and fallthrough
        break;
    }
  }
}

// This is similar to wcwidth except that it takes the current unicode
// character properties database into consideration, allowing it to
// correctly calculate the column widths of things like emoji's and
// newer wide characters. wcwidth, on the other hand, uses a fixed
// algorithm that does not take things like emoji into proper
// consideration.
static int GetColumnWidth(UChar32 codepoint, bool ambiguousFull = false) {
  const int eaw = u_getIntPropertyValue(codepoint, UCHAR_EAST_ASIAN_WIDTH);
  switch (eaw) {
    case U_EA_FULLWIDTH:
    case U_EA_WIDE:
      return 2;
    case U_EA_AMBIGUOUS:
      if (ambiguousFull)
        return 2;
    case U_EA_NEUTRAL:
      if (u_iscntrl(codepoint) ||
          u_hasBinaryProperty(codepoint, UCHAR_EMOJI_MODIFIER) ||
          u_getCombiningClass(codepoint) > 0)
        return 0;
      if (u_hasBinaryProperty(codepoint, UCHAR_EMOJI_PRESENTATION))
        return 2;
    case U_EA_HALFWIDTH:
    case U_EA_NARROW:
    default:
      return 1;
  }
}

// Returns the column width for the given String
static void GetColumnWidth(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1)
    return;

  if (args[0]->IsNumber()) {
    args.GetReturnValue().Set(GetColumnWidth(args[0]->Uint32Value()));
    return;
  }

  bool ambiguousFull = args.Length() > 1 ? args[1]->BooleanValue() : false;

  TwoByteValue value(env->isolate(), args[0].As<String>());
  UChar* str = static_cast<UChar*>(*value);
  UChar32 c;
  size_t n = 0;
  int width = 0;
  while (n < value.length()) {
    U16_NEXT(str, n, value.length(), c);
    const int cols = GetColumnWidth(c, ambiguousFull);
    if (cols < 0) {
      width = -1;
      break;
    }
    width += cols;
  }
  args.GetReturnValue().Set(width);
}

void Init(Local<Object> target,
          Local<Value> unused,
          Local<Context> context,
          void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "toUnicode", ToUnicode);
  env->SetMethod(target, "toASCII", ToASCII);

  // One-Shot Converters.
  env->SetMethod(target, "convert", Convert);
  env->SetMethod(target, "convertFromUcs2", ConvertFromUcs2);
  env->SetMethod(target, "convertToUcs2", ConvertToUcs2);
  env->SetMethod(target, "convertToUcs2FromUtf8", Ucs2FromUtf8);
  env->SetMethod(target, "convertToUtf8FromUcs2", Utf8FromUcs2);

  env->SetMethod(target, "normalize", NormalizeBuffer);
  env->SetMethod(target, "getCharacterProperty", GetCharacterProperty);
  env->SetMethod(target, "getColumnWidth", GetColumnWidth);
}

}  // namespace i18n
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(icu, node::i18n::Init)

#endif  // NODE_HAVE_I18N_SUPPORT
