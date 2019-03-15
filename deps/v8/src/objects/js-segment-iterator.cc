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
  Managed<icu::UnicodeString> unicode_string =
      Intl::SetTextToBreakIterator(isolate, text, break_iterator);
  segment_iterator->set_unicode_string(unicode_string);

  // 4. Let iterator.[[SegmentIteratorIndex]] be 0.
  // step 4 is stored inside break_iterator.

  // 5. Let iterator.[[SegmentIteratorBreakType]] be undefined.
  segment_iterator->set_is_break_type_set(false);

  return segment_iterator;
}

// ecma402 #sec-segment-iterator-prototype-breakType
Handle<Object> JSSegmentIterator::BreakType() const {
  if (!is_break_type_set()) {
    return GetReadOnlyRoots().undefined_value_handle();
  }
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

// ecma402 #sec-segment-iterator-prototype-index
Handle<Object> JSSegmentIterator::Index(
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
  // 3. Let _previousIndex be iterator.[[SegmentIteratorIndex]].
  int32_t prev = icu_break_iterator->current();
  // 4. Let done be AdvanceSegmentIterator(iterator, forwards).
  int32_t index = icu_break_iterator->next();
  segment_iterator->set_is_break_type_set(true);
  if (index == icu::BreakIterator::DONE) {
    // 5. If done is true, return CreateIterResultObject(undefined, true).
    return factory->NewJSIteratorResult(isolate->factory()->undefined_value(),
                                        true);
  }
  // 6. Let newIndex be iterator.[[SegmentIteratorIndex]].
  Handle<Object> new_index = factory->NewNumberFromInt(index);

  // 8. Let segment be the substring of string from previousIndex to
  // newIndex, inclusive of previousIndex and exclusive of newIndex.
  Handle<String> segment;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, segment,
                             segment_iterator->GetSegment(isolate, prev, index),
                             JSReceiver);

  // 9. Let breakType be iterator.[[SegmentIteratorBreakType]].
  Handle<Object> break_type = segment_iterator->BreakType();

  // 10. Let result be ! ObjectCreate(%ObjectPrototype%).
  Handle<JSObject> result = factory->NewJSObject(isolate->object_function());

  // 11. Perform ! CreateDataProperty(result "segment", segment).
  CHECK(JSReceiver::CreateDataProperty(isolate, result,
                                       factory->segment_string(), segment,
                                       Just(kDontThrow))
            .FromJust());

  // 12. Perform ! CreateDataProperty(result, "breakType", breakType).
  CHECK(JSReceiver::CreateDataProperty(isolate, result,
                                       factory->breakType_string(), break_type,
                                       Just(kDontThrow))
            .FromJust());

  // 13. Perform ! CreateDataProperty(result, "index", newIndex).
  CHECK(JSReceiver::CreateDataProperty(isolate, result, factory->index_string(),
                                       new_index, Just(kDontThrow))
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
    Handle<Object> index;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, index,
        Object::ToIndex(isolate, from_obj, MessageTemplate::kInvalidIndex),
        Nothing<bool>());
    if (!index->ToArrayIndex(&from)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("following"), index),
          Nothing<bool>());
    }
    // b. Let length be the length of iterator.[[SegmentIteratorString]].
    uint32_t length =
        static_cast<uint32_t>(icu_break_iterator->getText().getLength());

    // c. If from ≥ length, throw a RangeError exception.
    if (from >= length) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("following"),
                        from_obj),
          Nothing<bool>());
    }

    // d. Let iterator.[[SegmentIteratorPosition]] be from.
    segment_iterator->set_is_break_type_set(true);
    icu_break_iterator->following(from);
    return Just(false);
  }
  // 4. return AdvanceSegmentIterator(iterator, forward).
  // 4. .... or if direction is backwards and position is 0, return true.
  // 4. If direction is forwards and position is the length of string ... return
  // true.
  segment_iterator->set_is_break_type_set(true);
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
    Handle<Object> index;
    ASSIGN_RETURN_ON_EXCEPTION_VALUE(
        isolate, index,
        Object::ToIndex(isolate, from_obj, MessageTemplate::kInvalidIndex),
        Nothing<bool>());

    if (!index->ToArrayIndex(&from)) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("preceding"), index),
          Nothing<bool>());
    }
    // b. Let length be the length of iterator.[[SegmentIteratorString]].
    uint32_t length =
        static_cast<uint32_t>(icu_break_iterator->getText().getLength());
    // c. If from > length or from = 0, throw a RangeError exception.
    if (from > length || from == 0) {
      THROW_NEW_ERROR_RETURN_VALUE(
          isolate,
          NewRangeError(MessageTemplate::kParameterOfFunctionOutOfRange,
                        factory->NewStringFromStaticChars("from"),
                        factory->NewStringFromStaticChars("preceding"),
                        from_obj),
          Nothing<bool>());
    }
    // d. Let iterator.[[SegmentIteratorIndex]] be from.
    segment_iterator->set_is_break_type_set(true);
    icu_break_iterator->preceding(from);
    return Just(false);
  }
  // 4. return AdvanceSegmentIterator(iterator, backwards).
  // 4. .... or if direction is backwards and position is 0, return true.
  segment_iterator->set_is_break_type_set(true);
  return Just(icu_break_iterator->previous() == icu::BreakIterator::DONE);
}

}  // namespace internal
}  // namespace v8
