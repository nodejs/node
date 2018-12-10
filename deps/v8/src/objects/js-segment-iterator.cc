// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTL_SUPPORT
#error Internationalization is expected to be enabled.
#endif  // V8_INTL_SUPPORT

#include "src/objects/js-segment-iterator.h"

#include <map>
#include <memory>
#include <string>

#include "src/heap/factory.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/objects/intl-objects.h"
#include "src/objects/js-segment-iterator-inl.h"
#include "src/objects/managed.h"
#include "unicode/brkiter.h"

namespace v8 {
namespace internal {

MaybeHandle<String> JSSegmentIterator::GetSegment(Isolate* isolate,
                                                  int32_t start,
                                                  int32_t end) const {
  return Intl::ToString(isolate, *(unicode_string()->raw()), start, end);
}

Handle<String> JSSegmentIterator::GranularityAsString() const {
  switch (granularity()) {
    case JSSegmenter::Granularity::GRAPHEME:
      return GetReadOnlyRoots().grapheme_string_handle();
    case JSSegmenter::Granularity::WORD:
      return GetReadOnlyRoots().word_string_handle();
    case JSSegmenter::Granularity::SENTENCE:
      return GetReadOnlyRoots().sentence_string_handle();
    case JSSegmenter::Granularity::LINE:
      return GetReadOnlyRoots().line_string_handle();
    case JSSegmenter::Granularity::COUNT:
      UNREACHABLE();
  }
}

MaybeHandle<JSSegmentIterator> JSSegmentIterator::Create(
    Isolate* isolate, icu::BreakIterator* break_iterator,
    JSSegmenter::Granularity granularity, Handle<String> text) {
  CHECK_NOT_NULL(break_iterator);
  // 1. Let iterator be ObjectCreate(%SegmentIteratorPrototype%).
  Handle<Map> map = Handle<Map>(
      isolate->native_context()->intl_segment_iterator_map(), isolate);
  Handle<JSObject> result = isolate->factory()->NewJSObjectFromMap(map);

  Handle<JSSegmentIterator> segment_iterator =
      Handle<JSSegmentIterator>::cast(result);

  segment_iterator->set_flags(0);
  segment_iterator->set_granularity(granularity);
  // 2. Let iterator.[[SegmentIteratorSegmenter]] be segmenter.
  Handle<Managed<icu::BreakIterator>> managed_break_iterator =
      Managed<icu::BreakIterator>::FromRawPtr(isolate, 0, break_iterator);
  segment_iterator->set_icu_break_iterator(*managed_break_iterator);

  // 3. Let iterator.[[SegmentIteratorString]] be string.
  Managed<icu::UnicodeString>* unicode_string =
      Intl::SetTextToBreakIterator(isolate, text, break_iterator);
  segment_iterator->set_unicode_string(unicode_string);

  // 4. Let iterator.[[SegmentIteratorPosition]] be 0.
  // 5. Let iterator.[[SegmentIteratorBreakType]] be an implementation-dependent
  // string representing a break at the edge of a string.
  // step 4 and 5 are stored inside break_iterator.

  return segment_iterator;
}

// ecma402 #sec-segment-iterator-prototype-breakType
Handle<Object> JSSegmentIterator::BreakType() const {
  icu::BreakIterator* break_iterator = icu_break_iterator()->raw();
  int32_t rule_status = break_iterator->getRuleStatus();
  switch (granularity()) {
    case JSSegmenter::Granularity::GRAPHEME:
      return GetReadOnlyRoots().undefined_value_handle();
    case JSSegmenter::Granularity::WORD:
      if (rule_status >= UBRK_WORD_NONE && rule_status < UBRK_WORD_NONE_LIMIT) {
        // "words" that do not fit into any of other categories. Includes spaces
        // and most punctuation.
        return GetReadOnlyRoots().none_string_handle();
      }
      if ((rule_status >= UBRK_WORD_NUMBER &&
           rule_status < UBRK_WORD_NUMBER_LIMIT) ||
          (rule_status >= UBRK_WORD_LETTER &&
           rule_status < UBRK_WORD_LETTER_LIMIT) ||
          (rule_status >= UBRK_WORD_KANA &&
           rule_status < UBRK_WORD_KANA_LIMIT) ||
          (rule_status >= UBRK_WORD_IDEO &&
           rule_status < UBRK_WORD_IDEO_LIMIT)) {
        // words that appear to be numbers, letters, kana characters,
        // ideographic characters, etc
        return GetReadOnlyRoots().word_string_handle();
      }
      return GetReadOnlyRoots().undefined_value_handle();
    case JSSegmenter::Granularity::LINE:
      if (rule_status >= UBRK_LINE_SOFT && rule_status < UBRK_LINE_SOFT_LIMIT) {
        // soft line breaks, positions at which a line break is acceptable but
        // not required
        return GetReadOnlyRoots().soft_string_handle();
      }
      if ((rule_status >= UBRK_LINE_HARD &&
           rule_status < UBRK_LINE_HARD_LIMIT)) {
        // hard, or mandatory line breaks
        return GetReadOnlyRoots().hard_string_handle();
      }
      return GetReadOnlyRoots().undefined_value_handle();
    case JSSegmenter::Granularity::SENTENCE:
      if (rule_status >= UBRK_SENTENCE_TERM &&
          rule_status < UBRK_SENTENCE_TERM_LIMIT) {
        // sentences ending with a sentence terminator ('.', '?', '!', etc.)
        // character, possibly followed by a hard separator (CR, LF, PS, etc.)
        return GetReadOnlyRoots().term_string_handle();
      }
      if ((rule_status >= UBRK_SENTENCE_SEP &&
           rule_status < UBRK_SENTENCE_SEP_LIMIT)) {
        // sentences that do not contain an ending sentence terminator ('.',
        // '?', '!', etc.) character, but are ended only by a hard separator
        // (CR, LF, PS, etc.) hard, or mandatory line breaks
        return GetReadOnlyRoots().sep_string_handle();
      }
      return GetReadOnlyRoots().undefined_value_handle();
    case JSSegmenter::Granularity::COUNT:
      UNREACHABLE();
  }
}

// ecma402 #sec-segment-iterator-prototype-position
Handle<Object> JSSegmentIterator::Position(
    Isolate* isolate, Handle<JSSegmentIterator> segment_iterator) {
  icu::BreakIterator* icu_break_iterator =
      segment_iterator->icu_break_iterator()->raw();
  CHECK_NOT_NULL(icu_break_iterator);
  return isolate->factory()->NewNumberFromInt(icu_break_iterator->current());
}

// ecma402 #sec-segment-iterator-prototype-next
MaybeHandle<JSReceiver> JSSegmentIterator::Next(
    Isolate* isolate, Handle<JSSegmentIterator> segment_iterator) {
  Factory* factory = isolate->factory();
  icu::BreakIterator* icu_break_iterator =
      segment_iterator->icu_break_iterator()->raw();
  // 3. Let _previousPosition be iterator.[[SegmentIteratorPosition]].
  int32_t prev = icu_break_iterator->current();
  // 4. Let done be AdvanceSegmentIterator(iterator, forwards).
  int32_t position = icu_break_iterator->next();
  if (position == icu::BreakIterator::DONE) {
    // 5. If done is true, return CreateIterResultObject(undefined, true).
    return factory->NewJSIteratorResult(isolate->factory()->undefined_value(),
                                        true);
  }
  // 6. Let newPosition be iterator.[[SegmentIteratorPosition]].
  Handle<Object> new_position = factory->NewNumberFromInt(position);

  // 8. Let segment be the substring of string from previousPosition to
  // newPosition, inclusive of previousPosition and exclusive of newPosition.
  Handle<String> segment;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, segment, segment_iterator->GetSegment(isolate, prev, position),
      JSReceiver);

  // 9. Let breakType be iterator.[[SegmentIteratorBreakType]].
  Handle<Object> break_type = segment_iterator->BreakType();

  // 10. Let result be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());

  // 11. Perform ! CreateDataProperty(result "segment", segment).
  CHECK(JSReceiver::CreateDataProperty(
            isolate, result, factory->segment_string(), segment, kDontThrow)
            .FromJust());

  // 12. Perform ! CreateDataProperty(result, "breakType", breakType).
  CHECK(JSReceiver::CreateDataProperty(isolate, result,
                                       factory->breakType_string(), break_type,
                                       kDontThrow)
            .FromJust());

  // 13. Perform ! CreateDataProperty(result, "position", newPosition).
  CHECK(JSReceiver::CreateDataProperty(isolate, result,
                                       factory->position_string(), new_position,
                                       kDontThrow)
            .FromJust());

  // 14. Return CreateIterResultObject(result, false).
  return factory->NewJSIteratorResult(result, false);
}

// ecma402 #sec-segment-iterator-prototype-following
Maybe<bool> JSSegmentIterator::Following(
    Isolate* isolate, Handle<JSSegmentIterator> segment_iterator,
    Handle<Object> from_obj) {
  Factory* factory = isolate->factory();
  icu::BreakIterator* icu_break_iterator =
      segment_iterator->icu_break_iterator()->raw();
  // 3. If from is not undefined,
  if (!from_obj->IsUndefined()) {
    // a. Let from be ? ToIndex(from).
    uint32_t from;
    if (!from_obj->ToArrayIndex(&from)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("following"),
                        from_obj),
          Nothing<bool>());
    }
    // b. If from ≥ iterator.[[SegmentIteratorString]], throw a RangeError
    // exception.
    // c. Let iterator.[[SegmentIteratorPosition]] be from.
    if (icu_break_iterator->following(from) == icu::BreakIterator::DONE) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("following"),
                        from_obj),
          Nothing<bool>());
    }
    return Just(false);
  }
  // 4. return AdvanceSegmentIterator(iterator, forward).
  // 4. .... or if direction is backwards and position is 0, return true.
  // 4. If direction is forwards and position is the length of string ... return
  // true.
  return Just(icu_break_iterator->next() == icu::BreakIterator::DONE);
}

// ecma402 #sec-segment-iterator-prototype-preceding
Maybe<bool> JSSegmentIterator::Preceding(
    Isolate* isolate, Handle<JSSegmentIterator> segment_iterator,
    Handle<Object> from_obj) {
  Factory* factory = isolate->factory();
  icu::BreakIterator* icu_break_iterator =
      segment_iterator->icu_break_iterator()->raw();
  // 3. If from is not undefined,
  if (!from_obj->IsUndefined()) {
    // a. Let from be ? ToIndex(from).
    uint32_t from;
    if (!from_obj->ToArrayIndex(&from)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("following"),
                        from_obj),
          Nothing<bool>());
    }
    // b. If from > iterator.[[SegmentIteratorString]] or from = 0, throw a
    // RangeError exception.
    // c. Let iterator.[[SegmentIteratorPosition]] be from.
    uint32_t text_len =
        static_cast<uint32_t>(icu_break_iterator->getText().getLength());
    if (from > text_len ||
        icu_break_iterator->preceding(from) == icu::BreakIterator::DONE) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("preceding"),
                        from_obj),
          Nothing<bool>());
    }
    return Just(false);
  }
  // 4. return AdvanceSegmentIterator(iterator, backwards).
  // 4. .... or if direction is backwards and position is 0, return true.
  return Just(icu_break_iterator->previous() == icu::BreakIterator::DONE);
}

}  // namespace internal
}  // namespace v8
