// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_DISPLAY_NAMES_H_
#define V8_OBJECTS_JS_DISPLAY_NAMES_H_

#include <set>
#include <string>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class DisplayNamesInternal;

class JSDisplayNames : public JSObject {
 public:
  // Creates display names object with properties derived from input
  // locales and options.
  static MaybeHandle<JSDisplayNames> New(Isolate* isolate, Handle<Map> map,
                                         Handle<Object> locales,
                                         Handle<Object> options);

  static Handle<JSObject> ResolvedOptions(Isolate* isolate,
                                          Handle<JSDisplayNames> format_holder);

  static MaybeHandle<Object> Of(Isolate* isolate, Handle<JSDisplayNames> holder,
                                Handle<Object> code_obj);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  Handle<String> StyleAsString() const;
  Handle<String> FallbackAsString() const;

  // Style: identifying the display names style used.
  //
  // ecma402/#sec-properties-of-intl-displaynames-instances
  enum class Style {
    kLong,   // Everything spelled out.
    kShort,  // Abbreviations used when possible.
    kNarrow  // Use the shortest possible form.
  };
  inline void set_style(Style style);
  inline Style style() const;

  // Type: identifying the fallback of the display names.
  //
  // ecma402/#sec-properties-of-intl-displaynames-instances
  enum class Fallback {
    kCode,
    kNone,
  };
  inline void set_fallback(Fallback fallback);
  inline Fallback fallback() const;

  DECL_CAST(JSDisplayNames)

  // Bit positions in |flags|.
  DEFINE_TORQUE_GENERATED_JS_DISPLAY_NAMES_FLAGS()

  STATIC_ASSERT(Style::kLong <= StyleBits::kMax);
  STATIC_ASSERT(Style::kShort <= StyleBits::kMax);
  STATIC_ASSERT(Style::kNarrow <= StyleBits::kMax);
  STATIC_ASSERT(Fallback::kCode <= FallbackBit::kMax);
  STATIC_ASSERT(Fallback::kNone <= FallbackBit::kMax);

  // [flags] Bit field containing various flags about the function.
  DECL_INT_ACCESSORS(flags)

  DECL_ACCESSORS(internal, Managed<DisplayNamesInternal>)

  DECL_PRINTER(JSDisplayNames)
  DECL_VERIFIER(JSDisplayNames)

  // Layout description.
  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize,
                                TORQUE_GENERATED_JS_DISPLAY_NAMES_FIELDS)

  OBJECT_CONSTRUCTORS(JSDisplayNames, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_DISPLAY_NAMES_H_
