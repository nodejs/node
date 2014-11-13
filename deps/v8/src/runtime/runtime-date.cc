// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/arguments.h"
#include "src/date.h"
#include "src/dateparser-inl.h"
#include "src/runtime/runtime-utils.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_DateMakeDay) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);

  CONVERT_SMI_ARG_CHECKED(year, 0);
  CONVERT_SMI_ARG_CHECKED(month, 1);

  int days = isolate->date_cache()->DaysFromYearMonth(year, month);
  RUNTIME_ASSERT(Smi::IsValid(days));
  return Smi::FromInt(days);
}


RUNTIME_FUNCTION(Runtime_DateSetValue) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 3);

  CONVERT_ARG_HANDLE_CHECKED(JSDate, date, 0);
  CONVERT_DOUBLE_ARG_CHECKED(time, 1);
  CONVERT_SMI_ARG_CHECKED(is_utc, 2);

  DateCache* date_cache = isolate->date_cache();

  Handle<Object> value;
  ;
  bool is_value_nan = false;
  if (std::isnan(time)) {
    value = isolate->factory()->nan_value();
    is_value_nan = true;
  } else if (!is_utc && (time < -DateCache::kMaxTimeBeforeUTCInMs ||
                         time > DateCache::kMaxTimeBeforeUTCInMs)) {
    value = isolate->factory()->nan_value();
    is_value_nan = true;
  } else {
    time = is_utc ? time : date_cache->ToUTC(static_cast<int64_t>(time));
    if (time < -DateCache::kMaxTimeInMs || time > DateCache::kMaxTimeInMs) {
      value = isolate->factory()->nan_value();
      is_value_nan = true;
    } else {
      value = isolate->factory()->NewNumber(DoubleToInteger(time));
    }
  }
  date->SetValue(*value, is_value_nan);
  return *value;
}


RUNTIME_FUNCTION(Runtime_ThrowNotDateError) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  THROW_NEW_ERROR_RETURN_FAILURE(
      isolate, NewTypeError("not_date_object", HandleVector<Object>(NULL, 0)));
}


RUNTIME_FUNCTION(Runtime_DateCurrentTime) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 0);
  if (FLAG_log_timer_events) LOG(isolate, CurrentTimeEvent());

  // According to ECMA-262, section 15.9.1, page 117, the precision of
  // the number in a Date object representing a particular instant in
  // time is milliseconds. Therefore, we floor the result of getting
  // the OS time.
  double millis;
  if (FLAG_verify_predictable) {
    millis = 1388534400000.0;  // Jan 1 2014 00:00:00 GMT+0000
    millis += Floor(isolate->heap()->synthetic_time());
  } else {
    millis = Floor(base::OS::TimeCurrentMillis());
  }
  return *isolate->factory()->NewNumber(millis);
}


RUNTIME_FUNCTION(Runtime_DateParseString) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_HANDLE_CHECKED(String, str, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, output, 1);

  RUNTIME_ASSERT(output->HasFastElements());
  JSObject::EnsureCanContainHeapObjectElements(output);
  RUNTIME_ASSERT(output->HasFastObjectElements());
  Handle<FixedArray> output_array(FixedArray::cast(output->elements()));
  RUNTIME_ASSERT(output_array->length() >= DateParser::OUTPUT_SIZE);

  str = String::Flatten(str);
  DisallowHeapAllocation no_gc;

  bool result;
  String::FlatContent str_content = str->GetFlatContent();
  if (str_content.IsOneByte()) {
    result = DateParser::Parse(str_content.ToOneByteVector(), *output_array,
                               isolate->unicode_cache());
  } else {
    DCHECK(str_content.IsTwoByte());
    result = DateParser::Parse(str_content.ToUC16Vector(), *output_array,
                               isolate->unicode_cache());
  }

  if (result) {
    return *output;
  } else {
    return isolate->heap()->null_value();
  }
}


RUNTIME_FUNCTION(Runtime_DateLocalTimezone) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  RUNTIME_ASSERT(x >= -DateCache::kMaxTimeBeforeUTCInMs &&
                 x <= DateCache::kMaxTimeBeforeUTCInMs);
  const char* zone =
      isolate->date_cache()->LocalTimezone(static_cast<int64_t>(x));
  Handle<String> result =
      isolate->factory()->NewStringFromUtf8(CStrVector(zone)).ToHandleChecked();
  return *result;
}


RUNTIME_FUNCTION(Runtime_DateToUTC) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);

  CONVERT_DOUBLE_ARG_CHECKED(x, 0);
  RUNTIME_ASSERT(x >= -DateCache::kMaxTimeBeforeUTCInMs &&
                 x <= DateCache::kMaxTimeBeforeUTCInMs);
  int64_t time = isolate->date_cache()->ToUTC(static_cast<int64_t>(x));

  return *isolate->factory()->NewNumber(static_cast<double>(time));
}


RUNTIME_FUNCTION(Runtime_DateCacheVersion) {
  HandleScope hs(isolate);
  DCHECK(args.length() == 0);
  if (!isolate->eternal_handles()->Exists(EternalHandles::DATE_CACHE_VERSION)) {
    Handle<FixedArray> date_cache_version =
        isolate->factory()->NewFixedArray(1, TENURED);
    date_cache_version->set(0, Smi::FromInt(0));
    isolate->eternal_handles()->CreateSingleton(
        isolate, *date_cache_version, EternalHandles::DATE_CACHE_VERSION);
  }
  Handle<FixedArray> date_cache_version =
      Handle<FixedArray>::cast(isolate->eternal_handles()->GetSingleton(
          EternalHandles::DATE_CACHE_VERSION));
  // Return result as a JS array.
  Handle<JSObject> result =
      isolate->factory()->NewJSObject(isolate->array_function());
  JSArray::SetContent(Handle<JSArray>::cast(result), date_cache_version);
  return *result;
}


RUNTIME_FUNCTION(RuntimeReference_DateField) {
  SealHandleScope shs(isolate);
  DCHECK(args.length() == 2);
  CONVERT_ARG_CHECKED(Object, obj, 0);
  CONVERT_SMI_ARG_CHECKED(index, 1);
  if (!obj->IsJSDate()) {
    HandleScope scope(isolate);
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate,
        NewTypeError("not_date_object", HandleVector<Object>(NULL, 0)));
  }
  JSDate* date = JSDate::cast(obj);
  if (index == 0) return date->value();
  return JSDate::GetField(date, Smi::FromInt(index));
}
}
}  // namespace v8::internal
