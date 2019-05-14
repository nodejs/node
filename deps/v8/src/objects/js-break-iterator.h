// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#ifndef V8_OBJECTS_JS_BREAK_ITERATOR_H_
#define V8_OBJECTS_JS_BREAK_ITERATOR_H_

#include <set>
#include <string>

#include "src/objects.h"
#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

class JSV8BreakIterator : public JSObject {
 public:
  V8_WARN_UNUSED_RESULT static MaybeHandle<JSV8BreakIterator> Initialize(
      Isolate* isolate, Handle<JSV8BreakIterator> break_iterator,
      Handle<Object> input_locales, Handle<Object> input_options);

  static Handle<JSObject> ResolvedOptions(
      Isolate* isolate, Handle<JSV8BreakIterator> break_iterator);

  static const std::set<std::string>& GetAvailableLocales();

  static void AdoptText(Isolate* isolate,
                        Handle<JSV8BreakIterator> break_iterator,
                        Handle<String> text);

  static Handle<Object> Current(Isolate* isolate,
                                Handle<JSV8BreakIterator> break_iterator);
  static Handle<Object> First(Isolate* isolate,
                              Handle<JSV8BreakIterator> break_iterator);
  static Handle<Object> Next(Isolate* isolate,
                             Handle<JSV8BreakIterator> break_iterator);
  static String BreakType(Isolate* isolate,
                          Handle<JSV8BreakIterator> break_iterator);

  enum class Type { CHARACTER, WORD, SENTENCE, LINE, COUNT };
  inline void set_type(Type type);
  inline Type type() const;

  Handle<String> TypeAsString() const;

  DECL_CAST(JSV8BreakIterator)
  DECL_PRINTER(JSV8BreakIterator)
  DECL_VERIFIER(JSV8BreakIterator)

  DECL_ACCESSORS(locale, String)
  DECL_ACCESSORS(break_iterator, Managed<icu::BreakIterator>)
  DECL_ACCESSORS(unicode_string, Managed<icu::UnicodeString>)
  DECL_ACCESSORS(bound_adopt_text, Object)
  DECL_ACCESSORS(bound_first, Object)
  DECL_ACCESSORS(bound_next, Object)
  DECL_ACCESSORS(bound_current, Object)
  DECL_ACCESSORS(bound_break_type, Object)

// Layout description.
#define BREAK_ITERATOR_FIELDS(V)        \
  /* Pointer fields. */                 \
  V(kLocaleOffset, kTaggedSize)         \
  V(kTypeOffset, kTaggedSize)           \
  V(kBreakIteratorOffset, kTaggedSize)  \
  V(kUnicodeStringOffset, kTaggedSize)  \
  V(kBoundAdoptTextOffset, kTaggedSize) \
  V(kBoundFirstOffset, kTaggedSize)     \
  V(kBoundNextOffset, kTaggedSize)      \
  V(kBoundCurrentOffset, kTaggedSize)   \
  V(kBoundBreakTypeOffset, kTaggedSize) \
  /* Total Size */                      \
  V(kSize, 0)

  DEFINE_FIELD_OFFSET_CONSTANTS(JSObject::kHeaderSize, BREAK_ITERATOR_FIELDS)
#undef BREAK_ITERATOR_FIELDS

  OBJECT_CONSTRUCTORS(JSV8BreakIterator, JSObject);
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_BREAK_ITERATOR_H_
