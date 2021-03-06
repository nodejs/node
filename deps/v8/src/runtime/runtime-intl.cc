// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <cmath>
#include <memory>

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/date/date.h"
#include "src/execution/arguments-inl.h"
#include "src/execution/isolate-inl.h"
#include "src/handles/global-handles.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
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
#include "src/utils/utils.h"

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

RUNTIME_FUNCTION(Runtime_StringToLowerCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  RETURN_RESULT_OR_FAILURE(isolate, Intl::ConvertToLower(isolate, s));
}

RUNTIME_FUNCTION(Runtime_StringToUpperCaseIntl) {
  HandleScope scope(isolate);
  DCHECK_EQ(args.length(), 1);
  CONVERT_ARG_HANDLE_CHECKED(String, s, 0);
  s = String::Flatten(isolate, s);
  RETURN_RESULT_OR_FAILURE(isolate, Intl::ConvertToUpper(isolate, s));
}

}  // namespace internal
}  // namespace v8
