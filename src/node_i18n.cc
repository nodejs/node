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
#include "env.h"
#include "env-inl.h"
#include "util.h"
#include "util-inl.h"
#include "v8.h"

#include <unicode/putil.h>
#include <unicode/uchar.h>
#include <unicode/udata.h>
#include <unicode/uidna.h>

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
using v8::FunctionCallbackInfo;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

bool flag_icu_data_dir = false;

namespace i18n {

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

int32_t ToUnicode(MaybeStackBuffer<char>* buf,
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

int32_t ToASCII(MaybeStackBuffer<char>* buf,
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

// This is similar to wcwidth except that it takes the current unicode
// character properties database into consideration, allowing it to
// correctly calculate the column widths of things like emoji's and
// newer wide characters. wcwidth, on the other hand, uses a fixed
// algorithm that does not take things like emoji into proper
// consideration.
static int GetColumnWidth(UChar32 codepoint,
                          bool ambiguous_as_full_width = false) {
  if (!u_isdefined(codepoint) ||
      u_iscntrl(codepoint) ||
      u_getCombiningClass(codepoint) > 0 ||
      u_hasBinaryProperty(codepoint, UCHAR_EMOJI_MODIFIER)) {
    return 0;
  }
  // UCHAR_EAST_ASIAN_WIDTH is the Unicode property that identifies a
  // codepoint as being full width, wide, ambiguous, neutral, narrow,
  // or halfwidth.
  const int eaw = u_getIntPropertyValue(codepoint, UCHAR_EAST_ASIAN_WIDTH);
  switch (eaw) {
    case U_EA_FULLWIDTH:
    case U_EA_WIDE:
      return 2;
    case U_EA_AMBIGUOUS:
      // See: http://www.unicode.org/reports/tr11/#Ambiguous for details
      if (ambiguous_as_full_width) {
        return 2;
      }
      // Fall through if ambiguous_as_full_width if false.
    case U_EA_NEUTRAL:
      if (u_hasBinaryProperty(codepoint, UCHAR_EMOJI_PRESENTATION)) {
        return 2;
      }
      // Fall through
    case U_EA_HALFWIDTH:
    case U_EA_NARROW:
    default:
      return 1;
  }
}

// Returns the column width for the given String.
static void GetStringWidth(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  if (args.Length() < 1)
    return;

  bool ambiguous_as_full_width = args[1]->BooleanValue();
  bool expand_emoji_sequence = args[2]->BooleanValue();

  if (args[0]->IsNumber()) {
    args.GetReturnValue().Set(
        GetColumnWidth(args[0]->Uint32Value(),
                       ambiguous_as_full_width));
    return;
  }

  TwoByteValue value(env->isolate(), args[0]);
  // reinterpret_cast is required by windows to compile
  UChar* str = reinterpret_cast<UChar*>(*value);
  UChar32 c;
  UChar32 p;
  size_t n = 0;
  uint32_t width = 0;

  while (n < value.length()) {
    p = c;
    U16_NEXT(str, n, value.length(), c);
    // Don't count individual emoji codepoints that occur within an
    // emoji sequence. This is not necessarily foolproof. Some
    // environments display emoji sequences in the appropriate
    // condensed form (as a single emoji glyph), other environments
    // may not understand an emoji sequence and will display each
    // individual emoji separately. When this happens, the width
    // calculated will be off, and there's no reliable way of knowing
    // in advance if a particular sequence is going to be supported.
    // The expand_emoji_sequence option allows the caller to skip this
    // check and count each code within an emoji sequence separately.
    if (!expand_emoji_sequence &&
        n > 0 && p == 0x200d &&  // 0x200d == ZWJ (zero width joiner)
        (u_hasBinaryProperty(c, UCHAR_EMOJI_PRESENTATION) ||
         u_hasBinaryProperty(c, UCHAR_EMOJI_MODIFIER))) {
      continue;
    }
    width += GetColumnWidth(c, ambiguous_as_full_width);
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
  env->SetMethod(target, "getStringWidth", GetStringWidth);
}

}  // namespace i18n
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(icu, node::i18n::Init)

#endif  // NODE_HAVE_I18N_SUPPORT
