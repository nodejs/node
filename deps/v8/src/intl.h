// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_INTL_H_
#define V8_INTL_H_

#include "src/base/timezone-cache.h"
#include "src/objects.h"
#include "src/objects/string.h"
#include "unicode/uversion.h"

namespace U_ICU_NAMESPACE {
class TimeZone;
}

namespace v8 {
namespace internal {

const UChar* GetUCharBufferFromFlat(const String::FlatContent& flat,
                                    std::unique_ptr<uc16[]>* dest,
                                    int32_t length);
V8_WARN_UNUSED_RESULT Object* LocaleConvertCase(Handle<String> s,
                                                Isolate* isolate,
                                                bool is_to_upper,
                                                const char* lang);
V8_WARN_UNUSED_RESULT Object* ConvertToLower(Handle<String> s,
                                             Isolate* isolate);
V8_WARN_UNUSED_RESULT Object* ConvertToUpper(Handle<String> s,
                                             Isolate* isolate);
V8_WARN_UNUSED_RESULT Object* ConvertCase(Handle<String> s, bool is_upper,
                                          Isolate* isolate);

V8_WARN_UNUSED_RESULT Object* ConvertOneByteToLower(String* src, String* dst,
                                                    Isolate* isolate);

const uint8_t* ToLatin1LowerTable();

// ICUTimezoneCache calls out to ICU for TimezoneCache
// functionality in a straightforward way.
class ICUTimezoneCache : public base::TimezoneCache {
 public:
  ICUTimezoneCache();

  ~ICUTimezoneCache() override;

  const char* LocalTimezone(double time_ms) override;

  double DaylightSavingsOffset(double time_ms) override;

  double LocalTimeOffset(double time_ms, bool is_utc) override;

  void Clear() override;

 private:
  icu::TimeZone* GetTimeZone();

  bool GetOffsets(double time_ms, bool is_utc, int32_t* raw_offset,
                  int32_t* dst_offset);

  icu::TimeZone* timezone_;

  static const int32_t kMaxTimezoneChars = 100;
  char timezone_name_[kMaxTimezoneChars];
  char dst_timezone_name_[kMaxTimezoneChars];
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INTL_H_
