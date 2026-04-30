// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_H_
#define V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_H_

#include "src/common/globals.h"
#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <set>
#include <string>

#include "src/base/bit-field.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class RelativeDateTimeFormatter;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-relative-time-format-tq.inc"

V8_OBJECT class JSRelativeTimeFormat : public JSObject {
 public:
  // Creates relative time format object with properties derived from input
  // locales and options.
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSRelativeTimeFormat> New(
      Isolate* isolate, DirectHandle<Map> map, DirectHandle<Object> locales,
      DirectHandle<Object> options, const char* method_name);

  V8_WARN_UNUSED_RESULT static DirectHandle<JSObject> ResolvedOptions(
      Isolate* isolate, DirectHandle<JSRelativeTimeFormat> format_holder);

  Handle<String> NumericAsString(Isolate* isolate) const;

  // https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat.prototype.format
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<String> Format(
      Isolate* isolate, Handle<Object> value_obj, Handle<Object> unit_obj,
      DirectHandle<JSRelativeTimeFormat> format);

  // https://tc39.es/ecma402/#sec-Intl.RelativeTimeFormat.prototype.formatToParts
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSArray> FormatToParts(
      Isolate* isolate, Handle<Object> value_obj, Handle<Object> unit_obj,
      DirectHandle<JSRelativeTimeFormat> format);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  // RelativeTimeFormat accessors.
  inline Tagged<String> locale() const;
  inline void set_locale(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<String> numberingSystem() const;
  inline void set_numberingSystem(Tagged<String> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Managed<icu::RelativeDateTimeFormatter>> icu_formatter() const;
  inline void set_icu_formatter(
      Tagged<Managed<icu::RelativeDateTimeFormatter>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int flags() const;
  inline void set_flags(int value);

  // Numeric: identifying whether numerical descriptions are always used, or
  // used only when no more specific version is available (e.g., "1 day ago" vs
  // "yesterday").
  //
  // https://tc39.es/ecma402/#sec-properties-of-intl-relativetimeformat-instances
  enum class Numeric {
    ALWAYS,  // numerical descriptions are always used ("1 day ago")
    AUTO     // numerical descriptions are used only when no more specific
             // version is available ("yesterday")
  };
  inline void set_numeric(Numeric numeric);
  inline Numeric numeric() const;

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_JS_RELATIVE_TIME_FORMAT_FLAGS()

  static_assert(NumericBit::is_valid(Numeric::AUTO));
  static_assert(NumericBit::is_valid(Numeric::ALWAYS));

  DECL_PRINTER(JSRelativeTimeFormat)
  DECL_VERIFIER(JSRelativeTimeFormat)

  // Back-compat offset/size constants.
  static const int kLocaleOffset;
  static const int kNumberingSystemOffset;
  static const int kIcuFormatterOffset;
  static const int kFlagsOffset;
  static const int kHeaderSize;

 public:
  TaggedMember<String> locale_;
  TaggedMember<String> numberingSystem_;
  TaggedMember<Foreign> icu_formatter_;
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

inline constexpr int JSRelativeTimeFormat::kLocaleOffset =
    offsetof(JSRelativeTimeFormat, locale_);
inline constexpr int JSRelativeTimeFormat::kNumberingSystemOffset =
    offsetof(JSRelativeTimeFormat, numberingSystem_);
inline constexpr int JSRelativeTimeFormat::kIcuFormatterOffset =
    offsetof(JSRelativeTimeFormat, icu_formatter_);
inline constexpr int JSRelativeTimeFormat::kFlagsOffset =
    offsetof(JSRelativeTimeFormat, flags_);
inline constexpr int JSRelativeTimeFormat::kHeaderSize =
    sizeof(JSRelativeTimeFormat);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_RELATIVE_TIME_FORMAT_H_
