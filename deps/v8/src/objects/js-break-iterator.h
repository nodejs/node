// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_JS_BREAK_ITERATOR_H_
#define V8_OBJECTS_JS_BREAK_ITERATOR_H_

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include <set>
#include <string>

#include "src/objects/intl-objects.h"
#include "src/objects/managed.h"
#include "src/objects/objects.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace U_ICU_NAMESPACE {
class BreakIterator;
}  // namespace U_ICU_NAMESPACE

namespace v8 {
namespace internal {

#include "torque-generated/src/objects/js-break-iterator-tq.inc"

V8_OBJECT class JSV8BreakIterator : public JSObject {
 public:
  V8_WARN_UNUSED_RESULT static MaybeDirectHandle<JSV8BreakIterator> New(
      Isolate* isolate, DirectHandle<Map> map,
      DirectHandle<Object> input_locales, DirectHandle<Object> input_options,
      const char* service);

  static DirectHandle<JSObject> ResolvedOptions(
      Isolate* isolate, DirectHandle<JSV8BreakIterator> break_iterator);

  V8_EXPORT_PRIVATE static const std::set<std::string>& GetAvailableLocales();

  static void AdoptText(Isolate* isolate,
                        DirectHandle<JSV8BreakIterator> break_iterator,
                        DirectHandle<String> text);

  static DirectHandle<Object> Current(
      Isolate* isolate, DirectHandle<JSV8BreakIterator> break_iterator);
  static DirectHandle<Object> First(
      Isolate* isolate, DirectHandle<JSV8BreakIterator> break_iterator);
  static DirectHandle<Object> Next(
      Isolate* isolate, DirectHandle<JSV8BreakIterator> break_iterator);
  static Tagged<String> BreakType(
      Isolate* isolate, DirectHandle<JSV8BreakIterator> break_iterator);

  inline Tagged<String> locale() const;
  inline void set_locale(Tagged<String> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<Managed<IcuBreakIteratorWithText>> icu_iterator_with_text()
      const;
  inline void set_icu_iterator_with_text(
      Tagged<Managed<IcuBreakIteratorWithText>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, JSFunction>> bound_adopt_text() const;
  inline void set_bound_adopt_text(
      Tagged<UnionOf<Undefined, JSFunction>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, JSFunction>> bound_first() const;
  inline void set_bound_first(Tagged<UnionOf<Undefined, JSFunction>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, JSFunction>> bound_next() const;
  inline void set_bound_next(Tagged<UnionOf<Undefined, JSFunction>> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, JSFunction>> bound_current() const;
  inline void set_bound_current(Tagged<UnionOf<Undefined, JSFunction>> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Undefined, JSFunction>> bound_break_type() const;
  inline void set_bound_break_type(
      Tagged<UnionOf<Undefined, JSFunction>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSV8BreakIterator)
  DECL_VERIFIER(JSV8BreakIterator)

  static const int kHeaderSize;

 public:
  TaggedMember<String> locale_;
  TaggedMember<Foreign> icu_iterator_with_text_;
  TaggedMember<UnionOf<Undefined, JSFunction>> bound_adopt_text_;
  TaggedMember<UnionOf<Undefined, JSFunction>> bound_first_;
  TaggedMember<UnionOf<Undefined, JSFunction>> bound_next_;
  TaggedMember<UnionOf<Undefined, JSFunction>> bound_current_;
  TaggedMember<UnionOf<Undefined, JSFunction>> bound_break_type_;
} V8_OBJECT_END;

inline constexpr int JSV8BreakIterator::kHeaderSize = sizeof(JSV8BreakIterator);

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_JS_BREAK_ITERATOR_H_
