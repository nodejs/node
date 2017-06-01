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
#include "env-inl.h"
#include "util-inl.h"
#include "v8.h"

#include <unicode/putil.h>
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

bool InitializeICUDirectory(const std::string& path) {
  if (!path.empty()) {
    flag_icu_data_dir = true;
    u_setDataDirectory(path.c_str());
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
  uint32_t options = UIDNA_NONTRANSITIONAL_TO_UNICODE;
  UIDNA* uidna = uidna_openUTS46(options, &status);
  if (U_FAILURE(status))
    return -1;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;

  int32_t len = uidna_nameToUnicodeUTF8(uidna,
                                        input, length,
                                        **buf, buf->capacity(),
                                        &info,
                                        &status);

  // Do not check info.errors like we do with ToASCII since ToUnicode always
  // returns a string, despite any possible errors that may have occurred.

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    buf->AllocateSufficientStorage(len);
    len = uidna_nameToUnicodeUTF8(uidna,
                                  input, length,
                                  **buf, buf->capacity(),
                                  &info,
                                  &status);
  }

  // info.errors is ignored as UTS #46 ToUnicode always produces a Unicode
  // string, regardless of whether an error occurred.

  if (U_FAILURE(status)) {
    len = -1;
    buf->SetLength(0);
  } else {
    buf->SetLength(len);
  }

  uidna_close(uidna);
  return len;
}

int32_t ToASCII(MaybeStackBuffer<char>* buf,
                const char* input,
                size_t length,
                enum idna_mode mode) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t options =                  // CheckHyphens = false; handled later
    UIDNA_CHECK_BIDI |                // CheckBidi = true
    UIDNA_CHECK_CONTEXTJ |            // CheckJoiners = true
    UIDNA_NONTRANSITIONAL_TO_ASCII;   // Nontransitional_Processing
  if (mode == IDNA_STRICT) {
    options |= UIDNA_USE_STD3_RULES;  // UseSTD3ASCIIRules = beStrict
                                      // VerifyDnsLength = beStrict;
                                      //   handled later
  }

  UIDNA* uidna = uidna_openUTS46(options, &status);
  if (U_FAILURE(status))
    return -1;
  UIDNAInfo info = UIDNA_INFO_INITIALIZER;

  int32_t len = uidna_nameToASCII_UTF8(uidna,
                                       input, length,
                                       **buf, buf->capacity(),
                                       &info,
                                       &status);

  if (status == U_BUFFER_OVERFLOW_ERROR) {
    status = U_ZERO_ERROR;
    buf->AllocateSufficientStorage(len);
    len = uidna_nameToASCII_UTF8(uidna,
                                 input, length,
                                 **buf, buf->capacity(),
                                 &info,
                                 &status);
  }

  // In UTS #46 which specifies ToASCII, certain error conditions are
  // configurable through options, and the WHATWG URL Standard promptly elects
  // to disable some of them to accommodate for real-world use cases.
  // Unfortunately, ICU4C's IDNA module does not support disabling some of
  // these options through `options` above, and thus continues throwing
  // unnecessary errors. To counter this situation, we just filter out the
  // errors that may have happened afterwards, before deciding whether to
  // return an error from this function.

  // CheckHyphens = false
  // (Specified in the current UTS #46 draft rev. 18.)
  // Refs:
  // - https://github.com/whatwg/url/issues/53
  // - https://github.com/whatwg/url/pull/309
  // - http://www.unicode.org/review/pri317/
  // - http://www.unicode.org/reports/tr46/tr46-18.html
  // - https://www.icann.org/news/announcement-2000-01-07-en
  info.errors &= ~UIDNA_ERROR_HYPHEN_3_4;
  info.errors &= ~UIDNA_ERROR_LEADING_HYPHEN;
  info.errors &= ~UIDNA_ERROR_TRAILING_HYPHEN;

  if (mode != IDNA_STRICT) {
    // VerifyDnsLength = beStrict
    info.errors &= ~UIDNA_ERROR_EMPTY_LABEL;
    info.errors &= ~UIDNA_ERROR_LABEL_TOO_LONG;
    info.errors &= ~UIDNA_ERROR_DOMAIN_NAME_TOO_LONG;
  }

  if (U_FAILURE(status) || (mode != IDNA_LENIENT && info.errors != 0)) {
    len = -1;
    buf->SetLength(0);
  } else {
    buf->SetLength(len);
  }

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
  // optional arg
  bool lenient = args[1]->BooleanValue(env->context()).FromJust();
  enum idna_mode mode = lenient ? IDNA_LENIENT : IDNA_DEFAULT;

  MaybeStackBuffer<char> buf;
  int32_t len = ToASCII(&buf, *val, val.length(), mode);

  if (len < 0) {
    return env->ThrowError("Cannot convert name to ASCII");
  }

  args.GetReturnValue().Set(
      String::NewFromUtf8(env->isolate(),
                          *buf,
                          v8::NewStringType::kNormal,
                          len).ToLocalChecked());
}

void Init(Local<Object> target,
          Local<Value> unused,
          Local<Context> context,
          void* priv) {
  Environment* env = Environment::GetCurrent(context);
  env->SetMethod(target, "toUnicode", ToUnicode);
  env->SetMethod(target, "toASCII", ToASCII);
}

}  // namespace i18n
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(icu, node::i18n::Init)

#endif  // NODE_HAVE_I18N_SUPPORT
