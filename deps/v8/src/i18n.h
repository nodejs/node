// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// limitations under the License.

#ifndef V8_I18N_H_
#define V8_I18N_H_

#include "src/base/timezone-cache.h"
#include "src/objects.h"
#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
class Collator;
class DecimalFormat;
class SimpleDateFormat;
class TimeZone;
}

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class DateFormat {
 public:
  // Create a formatter for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::SimpleDateFormat* InitializeDateTimeFormat(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks date format object from corresponding JavaScript object.
  static icu::SimpleDateFormat* UnpackDateFormat(Isolate* isolate,
                                                 Handle<JSObject> obj);

  // Release memory we allocated for the DateFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteDateFormat(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kSimpleDateFormat = JSObject::kHeaderSize;
  static const int kSize = kSimpleDateFormat + kPointerSize;

 private:
  DateFormat();
};


class NumberFormat {
 public:
  // Create a formatter for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::DecimalFormat* InitializeNumberFormat(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks number format object from corresponding JavaScript object.
  static icu::DecimalFormat* UnpackNumberFormat(Isolate* isolate,
                                                Handle<JSObject> obj);

  // Release memory we allocated for the NumberFormat once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteNumberFormat(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kDecimalFormat = JSObject::kHeaderSize;
  static const int kSize = kDecimalFormat + kPointerSize;

 private:
  NumberFormat();
};


class Collator {
 public:
  // Create a collator for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::Collator* InitializeCollator(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks collator object from corresponding JavaScript object.
  static icu::Collator* UnpackCollator(Isolate* isolate, Handle<JSObject> obj);

  // Release memory we allocated for the Collator once the JS object that holds
  // the pointer gets garbage collected.
  static void DeleteCollator(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kCollator = JSObject::kHeaderSize;
  static const int kSize = kCollator + kPointerSize;

 private:
  Collator();
};

class V8BreakIterator {
 public:
  // Create a BreakIterator for the specificied locale and options. Returns the
  // resolved settings for the locale / options.
  static icu::BreakIterator* InitializeBreakIterator(
      Isolate* isolate,
      Handle<String> locale,
      Handle<JSObject> options,
      Handle<JSObject> resolved);

  // Unpacks break iterator object from corresponding JavaScript object.
  static icu::BreakIterator* UnpackBreakIterator(Isolate* isolate,
                                                 Handle<JSObject> obj);

  // Release memory we allocated for the BreakIterator once the JS object that
  // holds the pointer gets garbage collected.
  static void DeleteBreakIterator(const v8::WeakCallbackInfo<void>& data);

  // Layout description.
  static const int kBreakIterator = JSObject::kHeaderSize;
  static const int kUnicodeString = kBreakIterator + kPointerSize;
  static const int kSize = kUnicodeString + kPointerSize;

 private:
  V8BreakIterator();
};

const UChar* GetUCharBufferFromFlat(const String::FlatContent& flat,
                                    std::unique_ptr<uc16[]>* dest,
                                    int32_t length);
MUST_USE_RESULT Object* LocaleConvertCase(Handle<String> s, Isolate* isolate,
                                          bool is_to_upper, const char* lang);
MUST_USE_RESULT Object* ConvertToLower(Handle<String> s, Isolate* isolate);
MUST_USE_RESULT Object* ConvertToUpper(Handle<String> s, Isolate* isolate);
MUST_USE_RESULT Object* ConvertCase(Handle<String> s, bool is_upper,
                                    Isolate* isolate);

// ICUTimezoneCache calls out to ICU for TimezoneCache
// functionality in a straightforward way.
class ICUTimezoneCache : public base::TimezoneCache {
 public:
  ICUTimezoneCache();

  ~ICUTimezoneCache() override;

  const char* LocalTimezone(double time_ms) override;

  double DaylightSavingsOffset(double time_ms) override;

  double LocalTimeOffset() override;

  void Clear() override;

 private:
  icu::TimeZone* GetTimeZone();

  bool GetOffsets(double time_ms, int32_t* raw_offset, int32_t* dst_offset);

  icu::TimeZone* timezone_;

  static const int32_t kMaxTimezoneChars = 100;
  char timezone_name_[kMaxTimezoneChars];
  char dst_timezone_name_[kMaxTimezoneChars];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_I18N_H_
