// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_LIST_FORMAT_H_
#define V8_OBJECTS_JS_LIST_FORMAT_H_

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects.h"
#include "src/objects/managed.h"
#include "unicode/uversion.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class ListFormatter;
}

namespace v8 {
namespace internal {

class JSListFormat : public JSObject {
 public:
  // Initializes relative time format object with properties derived from input
  // locales and options.
  static MaybeHandle<JSListFormat> Initialize(
      Isolate* isolate, Handle<JSListFormat> list_format_holder,
      Handle<Object> locales, Handle<Object> options);

  static Handle<JSObject> ResolvedOptions(Isolate* isolate,
                                          Handle<JSListFormat> format_holder);

  // ecma402 #sec-formatlist
  V8_WARN_UNUSED_RESULT static MaybeHandle<String> FormatList(
      Isolate* isolate, Handle<JSListFormat> format_holder,
      Handle<JSArray> list);

  // ecma42 #sec-formatlisttoparts
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSArray> FormatListToParts(
      Isolate* isolate, Handle<JSListFormat> format_holder,
      Handle<JSArray> list);

  Handle<String> StyleAsString() const;
  Handle<String> TypeAsString() const;

  DECL_CAST(JSListFormat)

  // ListFormat accessors.
  DECL_ACCESSORS(locale, String)
  DECL_ACCESSORS(icu_formatter, Managed<icu::ListFormatter>)

  // Style: identifying the relative time format style used.
  //
  // ecma402/#sec-properties-of-intl-listformat-instances
  enum class Style {
    LONG,    // Everything spelled out.
    SHORT,   // Abbreviations used when possible.
    NARROW,  // Use the shortest possible form.
    COUNT
  };
  inline void set_style(Style style);
  inline Style style() const;

  // Type: identifying the list of types used.
  //
  // ecma402/#sec-properties-of-intl-listformat-instances
  enum class Type {
    CONJUNCTION,  // for "and"-based lists (e.g., "A, B and C")
    DISJUNCTION,  // for "or"-based lists (e.g., "A, B or C"),
    UNIT,  // for lists of values with units (e.g., "5 pounds, 12 ounces").
    COUNT
  };
  inline void set_type(Type type);
  inline Type type() const;

// Bit positions in |flags|.
#define FLAGS_BIT_FIELDS(V, _) \
  V(StyleBits, Style, 2, _)    \
  V(TypeBits, Type, 2, _)
  DEFINE_BIT_FIELDS(FLAGS_BIT_FIELDS)
#undef FLAGS_BIT_FIELDS

  STATIC_ASSERT(Style::LONG <= StyleBits::kMax);
  STATIC_ASSERT(Style::SHORT <= StyleBits::kMax);
  STATIC_ASSERT(Style::NARROW <= StyleBits::kMax);
  STATIC_ASSERT(Type::CONJUNCTION <= TypeBits::kMax);
  STATIC_ASSERT(Type::DISJUNCTION <= TypeBits::kMax);
  STATIC_ASSERT(Type::UNIT <= TypeBits::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  DECL_PRINTER(JSListFormat)
  DECL_VERIFIER(JSListFormat)

  // Layout description.
  static const int kJSListFormatOffset = JSObject::kHeaderSize;
  static const int kLocaleOffset = kJSListFormatOffset + kPointerSize;
  static const int kICUFormatterOffset = kLocaleOffset + kPointerSize;
  static const int kFlagsOffset = kICUFormatterOffset + kPointerSize;
  static const int kSize = kFlagsOffset + kPointerSize;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(JSListFormat);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_LIST_FORMAT_H_
