// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <cmath>
#include <memory>

#include "src/api-inl.h"
#include "src/api-natives.h"
#include "src/arguments-inl.h"
#include "src/date.h"
#include "src/global-handles.h"
#include "src/heap/factory.h"
#include "src/intl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/objects/intl-objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-collator-inl.h"
#include "src/objects/js-date-time-format-inl.h"
#include "src/objects/js-list-format-inl.h"
#include "src/objects/js-list-format.h"
#include "src/objects/js-number-format-inl.h"
#include "src/objects/js-plural-rules-inl.h"
#include "src/objects/managed.h"
#include "src/runtime/runtime-utils.h"
#include "src/utils.h"

#include "unicode/brkiter.h"
#include "unicode/calendar.h"
#include "unicode/coll.h"
#include "unicode/curramt.h"
#include "unicode/datefmt.h"
#include "unicode/dcfmtsym.h"
#include "unicode/decimfmt.h"
#include "unicode/dtfmtsym.h"
#include "unicode/dtptngen.h"
#include "unicode/locid.h"
#include "unicode/numfmt.h"
#include "unicode/numsys.h"
#include "unicode/plurrule.h"
#include "unicode/smpdtfmt.h"
#include "unicode/timezone.h"
#include "unicode/uchar.h"
#include "unicode/ucol.h"
#include "unicode/ucurr.h"
#include "unicode/uloc.h"
#include "unicode/unistr.h"
#include "unicode/unum.h"
#include "unicode/uversion.h"


namespace v8 {
namespace internal {

// ecma402 #sec-formatlist
RUNTIME_FUNCTION(Runtime_FormatList) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSListFormat, list_format, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, list, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSListFormat::FormatList(isolate, list_format, list));
}

// ecma402 #sec-formatlisttoparts
RUNTIME_FUNCTION(Runtime_FormatListToParts) {
  HandleScope scope(isolate);
  DCHECK_EQ(2, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSListFormat, list_format, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, list, 1);
  RETURN_RESULT_OR_FAILURE(
      isolate, JSListFormat::FormatListToParts(isolate, list_format, list));
}

// ECMA 402 6.2.3
RUNTIME_FUNCTION(Runtime_CanonicalizeLanguageTag) {
  HandleScope scope(isolate);

  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, locale, 0);

  std::string canonicalized;
  if (!Intl::CanonicalizeLanguageTag(isolate, locale).To(&canonicalized)) {
    return ReadOnlyRoots(isolate).exception();
  }
  return *isolate->factory()->NewStringFromAsciiChecked(canonicalized.c_str());
}

RUNTIME_FUNCTION(Runtime_AvailableLocalesOf) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, service, 0);
  Handle<JSObject> locales;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, locales, Intl::AvailableLocalesOf(isolate, service));
  return *locales;
}

RUNTIME_FUNCTION(Runtime_GetDefaultICULocale) {
  HandleScope scope(isolate);

  DCHECK_EQ(0, args.length());
  return *isolate->factory()->NewStringFromAsciiChecked(
      Intl::DefaultLocale(isolate).c_str());
}

RUNTIME_FUNCTION(Runtime_StringToLowerCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  RETURN_RESULT_OR_FAILURE(isolate, ConvertToLower(s, isolate));
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  RETURN_RESULT_OR_FAILURE(isolate, ConvertToUpper(s, isolate));
}

RUNTIME_FUNCTION(Runtime_DateCacheVersion) {
  HandleScope scope(isolate);
  DCHECK_EQ(0, args.length());
  if (isolate->serializer_enabled())
    return ReadOnlyRoots(isolate).undefined_value();
  if (!isolate->eternal_handles()->Exists(EternalHandles::DATE_CACHE_VERSION)) {
    Handle<FixedArray> date_cache_version =
        isolate->factory()->NewFixedArray(1, TENURED);
    date_cache_version->set(0, Smi::kZero);
    isolate->eternal_handles()->CreateSingleton(
        isolate, *date_cache_version, EternalHandles::DATE_CACHE_VERSION);
  }
  Handle<FixedArray> date_cache_version =
      Handle<FixedArray>::cast(isolate->eternal_handles()->GetSingleton(
          EternalHandles::DATE_CACHE_VERSION));
  return date_cache_version->get(0);
}

}  // namespace internal
}  // namespace v8
