// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/conversions-inl.h"
#include "src/isolate-inl.h"
#include "src/messages.h"
#include "src/regexp/jsregexp-inl.h"
#include "src/regexp/jsregexp.h"
#include "src/regexp/regexp-utils.h"
#include "src/string-builder.h"
#include "src/string-search.h"

namespace v8 {
namespace internal {

class CompiledReplacement {
 public:
  explicit CompiledReplacement(Zone* zone)
      : parts_(1, zone), replacement_substrings_(0, zone), zone_(zone) {}

  // Return whether the replacement is simple.
  bool Compile(Handle<String> replacement, int capture_count,
               int subject_length);

  // Use Apply only if Compile returned false.
  void Apply(ReplacementStringBuilder* builder, int match_from, int match_to,
             int32_t* match);

  // Number of distinct parts of the replacement pattern.
  int parts() { return parts_.length(); }

  Zone* zone() const { return zone_; }

 private:
  enum PartType {
    SUBJECT_PREFIX = 1,
    SUBJECT_SUFFIX,
    SUBJECT_CAPTURE,
    REPLACEMENT_SUBSTRING,
    REPLACEMENT_STRING,
    NUMBER_OF_PART_TYPES
  };

  struct ReplacementPart {
    static inline ReplacementPart SubjectMatch() {
      return ReplacementPart(SUBJECT_CAPTURE, 0);
    }
    static inline ReplacementPart SubjectCapture(int capture_index) {
      return ReplacementPart(SUBJECT_CAPTURE, capture_index);
    }
    static inline ReplacementPart SubjectPrefix() {
      return ReplacementPart(SUBJECT_PREFIX, 0);
    }
    static inline ReplacementPart SubjectSuffix(int subject_length) {
      return ReplacementPart(SUBJECT_SUFFIX, subject_length);
    }
    static inline ReplacementPart ReplacementString() {
      return ReplacementPart(REPLACEMENT_STRING, 0);
    }
    static inline ReplacementPart ReplacementSubString(int from, int to) {
      DCHECK(from >= 0);
      DCHECK(to > from);
      return ReplacementPart(-from, to);
    }

    // If tag <= 0 then it is the negation of a start index of a substring of
    // the replacement pattern, otherwise it's a value from PartType.
    ReplacementPart(int tag, int data) : tag(tag), data(data) {
      // Must be non-positive or a PartType value.
      DCHECK(tag < NUMBER_OF_PART_TYPES);
    }
    // Either a value of PartType or a non-positive number that is
    // the negation of an index into the replacement string.
    int tag;
    // The data value's interpretation depends on the value of tag:
    // tag == SUBJECT_PREFIX ||
    // tag == SUBJECT_SUFFIX:  data is unused.
    // tag == SUBJECT_CAPTURE: data is the number of the capture.
    // tag == REPLACEMENT_SUBSTRING ||
    // tag == REPLACEMENT_STRING:    data is index into array of substrings
    //                               of the replacement string.
    // tag <= 0: Temporary representation of the substring of the replacement
    //           string ranging over -tag .. data.
    //           Is replaced by REPLACEMENT_{SUB,}STRING when we create the
    //           substring objects.
    int data;
  };

  template <typename Char>
  bool ParseReplacementPattern(ZoneList<ReplacementPart>* parts,
                               Vector<Char> characters, int capture_count,
                               int subject_length, Zone* zone) {
    int length = characters.length();
    int last = 0;
    for (int i = 0; i < length; i++) {
      Char c = characters[i];
      if (c == '$') {
        int next_index = i + 1;
        if (next_index == length) {  // No next character!
          break;
        }
        Char c2 = characters[next_index];
        switch (c2) {
          case '$':
            if (i > last) {
              // There is a substring before. Include the first "$".
              parts->Add(
                  ReplacementPart::ReplacementSubString(last, next_index),
                  zone);
              last = next_index + 1;  // Continue after the second "$".
            } else {
              // Let the next substring start with the second "$".
              last = next_index;
            }
            i = next_index;
            break;
          case '`':
            if (i > last) {
              parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
            }
            parts->Add(ReplacementPart::SubjectPrefix(), zone);
            i = next_index;
            last = i + 1;
            break;
          case '\'':
            if (i > last) {
              parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
            }
            parts->Add(ReplacementPart::SubjectSuffix(subject_length), zone);
            i = next_index;
            last = i + 1;
            break;
          case '&':
            if (i > last) {
              parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
            }
            parts->Add(ReplacementPart::SubjectMatch(), zone);
            i = next_index;
            last = i + 1;
            break;
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9': {
            int capture_ref = c2 - '0';
            if (capture_ref > capture_count) {
              i = next_index;
              continue;
            }
            int second_digit_index = next_index + 1;
            if (second_digit_index < length) {
              // Peek ahead to see if we have two digits.
              Char c3 = characters[second_digit_index];
              if ('0' <= c3 && c3 <= '9') {  // Double digits.
                int double_digit_ref = capture_ref * 10 + c3 - '0';
                if (double_digit_ref <= capture_count) {
                  next_index = second_digit_index;
                  capture_ref = double_digit_ref;
                }
              }
            }
            if (capture_ref > 0) {
              if (i > last) {
                parts->Add(ReplacementPart::ReplacementSubString(last, i),
                           zone);
              }
              DCHECK(capture_ref <= capture_count);
              parts->Add(ReplacementPart::SubjectCapture(capture_ref), zone);
              last = next_index + 1;
            }
            i = next_index;
            break;
          }
          default:
            i = next_index;
            break;
        }
      }
    }
    if (length > last) {
      if (last == 0) {
        // Replacement is simple.  Do not use Apply to do the replacement.
        return true;
      } else {
        parts->Add(ReplacementPart::ReplacementSubString(last, length), zone);
      }
    }
    return false;
  }

  ZoneList<ReplacementPart> parts_;
  ZoneList<Handle<String> > replacement_substrings_;
  Zone* zone_;
};


bool CompiledReplacement::Compile(Handle<String> replacement, int capture_count,
                                  int subject_length) {
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent content = replacement->GetFlatContent();
    DCHECK(content.IsFlat());
    bool simple = false;
    if (content.IsOneByte()) {
      simple = ParseReplacementPattern(&parts_, content.ToOneByteVector(),
                                       capture_count, subject_length, zone());
    } else {
      DCHECK(content.IsTwoByte());
      simple = ParseReplacementPattern(&parts_, content.ToUC16Vector(),
                                       capture_count, subject_length, zone());
    }
    if (simple) return true;
  }

  Isolate* isolate = replacement->GetIsolate();
  // Find substrings of replacement string and create them as String objects.
  int substring_index = 0;
  for (int i = 0, n = parts_.length(); i < n; i++) {
    int tag = parts_[i].tag;
    if (tag <= 0) {  // A replacement string slice.
      int from = -tag;
      int to = parts_[i].data;
      replacement_substrings_.Add(
          isolate->factory()->NewSubString(replacement, from, to), zone());
      parts_[i].tag = REPLACEMENT_SUBSTRING;
      parts_[i].data = substring_index;
      substring_index++;
    } else if (tag == REPLACEMENT_STRING) {
      replacement_substrings_.Add(replacement, zone());
      parts_[i].data = substring_index;
      substring_index++;
    }
  }
  return false;
}


void CompiledReplacement::Apply(ReplacementStringBuilder* builder,
                                int match_from, int match_to, int32_t* match) {
  DCHECK_LT(0, parts_.length());
  for (int i = 0, n = parts_.length(); i < n; i++) {
    ReplacementPart part = parts_[i];
    switch (part.tag) {
      case SUBJECT_PREFIX:
        if (match_from > 0) builder->AddSubjectSlice(0, match_from);
        break;
      case SUBJECT_SUFFIX: {
        int subject_length = part.data;
        if (match_to < subject_length) {
          builder->AddSubjectSlice(match_to, subject_length);
        }
        break;
      }
      case SUBJECT_CAPTURE: {
        int capture = part.data;
        int from = match[capture * 2];
        int to = match[capture * 2 + 1];
        if (from >= 0 && to > from) {
          builder->AddSubjectSlice(from, to);
        }
        break;
      }
      case REPLACEMENT_SUBSTRING:
      case REPLACEMENT_STRING:
        builder->AddString(replacement_substrings_[part.data]);
        break;
      default:
        UNREACHABLE();
    }
  }
}

void FindOneByteStringIndices(Vector<const uint8_t> subject, uint8_t pattern,
                              List<int>* indices, unsigned int limit) {
  DCHECK(limit > 0);
  // Collect indices of pattern in subject using memchr.
  // Stop after finding at most limit values.
  const uint8_t* subject_start = subject.start();
  const uint8_t* subject_end = subject_start + subject.length();
  const uint8_t* pos = subject_start;
  while (limit > 0) {
    pos = reinterpret_cast<const uint8_t*>(
        memchr(pos, pattern, subject_end - pos));
    if (pos == NULL) return;
    indices->Add(static_cast<int>(pos - subject_start));
    pos++;
    limit--;
  }
}

void FindTwoByteStringIndices(const Vector<const uc16> subject, uc16 pattern,
                              List<int>* indices, unsigned int limit) {
  DCHECK(limit > 0);
  const uc16* subject_start = subject.start();
  const uc16* subject_end = subject_start + subject.length();
  for (const uc16* pos = subject_start; pos < subject_end && limit > 0; pos++) {
    if (*pos == pattern) {
      indices->Add(static_cast<int>(pos - subject_start));
      limit--;
    }
  }
}

template <typename SubjectChar, typename PatternChar>
void FindStringIndices(Isolate* isolate, Vector<const SubjectChar> subject,
                       Vector<const PatternChar> pattern, List<int>* indices,
                       unsigned int limit) {
  DCHECK(limit > 0);
  // Collect indices of pattern in subject.
  // Stop after finding at most limit values.
  int pattern_length = pattern.length();
  int index = 0;
  StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
  while (limit > 0) {
    index = search.Search(subject, index);
    if (index < 0) return;
    indices->Add(index);
    index += pattern_length;
    limit--;
  }
}

void FindStringIndicesDispatch(Isolate* isolate, String* subject,
                               String* pattern, List<int>* indices,
                               unsigned int limit) {
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent subject_content = subject->GetFlatContent();
    String::FlatContent pattern_content = pattern->GetFlatContent();
    DCHECK(subject_content.IsFlat());
    DCHECK(pattern_content.IsFlat());
    if (subject_content.IsOneByte()) {
      Vector<const uint8_t> subject_vector = subject_content.ToOneByteVector();
      if (pattern_content.IsOneByte()) {
        Vector<const uint8_t> pattern_vector =
            pattern_content.ToOneByteVector();
        if (pattern_vector.length() == 1) {
          FindOneByteStringIndices(subject_vector, pattern_vector[0], indices,
                                   limit);
        } else {
          FindStringIndices(isolate, subject_vector, pattern_vector, indices,
                            limit);
        }
      } else {
        FindStringIndices(isolate, subject_vector,
                          pattern_content.ToUC16Vector(), indices, limit);
      }
    } else {
      Vector<const uc16> subject_vector = subject_content.ToUC16Vector();
      if (pattern_content.IsOneByte()) {
        Vector<const uint8_t> pattern_vector =
            pattern_content.ToOneByteVector();
        if (pattern_vector.length() == 1) {
          FindTwoByteStringIndices(subject_vector, pattern_vector[0], indices,
                                   limit);
        } else {
          FindStringIndices(isolate, subject_vector, pattern_vector, indices,
                            limit);
        }
      } else {
        Vector<const uc16> pattern_vector = pattern_content.ToUC16Vector();
        if (pattern_vector.length() == 1) {
          FindTwoByteStringIndices(subject_vector, pattern_vector[0], indices,
                                   limit);
        } else {
          FindStringIndices(isolate, subject_vector, pattern_vector, indices,
                            limit);
        }
      }
    }
  }
}

namespace {
List<int>* GetRewoundRegexpIndicesList(Isolate* isolate) {
  List<int>* list = isolate->regexp_indices();
  list->Rewind(0);
  return list;
}

void TruncateRegexpIndicesList(Isolate* isolate) {
  // Same size as smallest zone segment, preserving behavior from the
  // runtime zone.
  static const int kMaxRegexpIndicesListCapacity = 8 * KB;
  if (isolate->regexp_indices()->capacity() > kMaxRegexpIndicesListCapacity) {
    isolate->regexp_indices()->Clear();  //  Throw away backing storage
  }
}
}  // namespace

template <typename ResultSeqString>
MUST_USE_RESULT static Object* StringReplaceGlobalAtomRegExpWithString(
    Isolate* isolate, Handle<String> subject, Handle<JSRegExp> pattern_regexp,
    Handle<String> replacement, Handle<RegExpMatchInfo> last_match_info) {
  DCHECK(subject->IsFlat());
  DCHECK(replacement->IsFlat());

  List<int>* indices = GetRewoundRegexpIndicesList(isolate);

  DCHECK_EQ(JSRegExp::ATOM, pattern_regexp->TypeTag());
  String* pattern =
      String::cast(pattern_regexp->DataAt(JSRegExp::kAtomPatternIndex));
  int subject_len = subject->length();
  int pattern_len = pattern->length();
  int replacement_len = replacement->length();

  FindStringIndicesDispatch(isolate, *subject, pattern, indices, 0xffffffff);

  int matches = indices->length();
  if (matches == 0) return *subject;

  // Detect integer overflow.
  int64_t result_len_64 = (static_cast<int64_t>(replacement_len) -
                           static_cast<int64_t>(pattern_len)) *
                              static_cast<int64_t>(matches) +
                          static_cast<int64_t>(subject_len);
  int result_len;
  if (result_len_64 > static_cast<int64_t>(String::kMaxLength)) {
    STATIC_ASSERT(String::kMaxLength < kMaxInt);
    result_len = kMaxInt;  // Provoke exception.
  } else {
    result_len = static_cast<int>(result_len_64);
  }

  int subject_pos = 0;
  int result_pos = 0;

  MaybeHandle<SeqString> maybe_res;
  if (ResultSeqString::kHasOneByteEncoding) {
    maybe_res = isolate->factory()->NewRawOneByteString(result_len);
  } else {
    maybe_res = isolate->factory()->NewRawTwoByteString(result_len);
  }
  Handle<SeqString> untyped_res;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, untyped_res, maybe_res);
  Handle<ResultSeqString> result = Handle<ResultSeqString>::cast(untyped_res);

  for (int i = 0; i < matches; i++) {
    // Copy non-matched subject content.
    if (subject_pos < indices->at(i)) {
      String::WriteToFlat(*subject, result->GetChars() + result_pos,
                          subject_pos, indices->at(i));
      result_pos += indices->at(i) - subject_pos;
    }

    // Replace match.
    if (replacement_len > 0) {
      String::WriteToFlat(*replacement, result->GetChars() + result_pos, 0,
                          replacement_len);
      result_pos += replacement_len;
    }

    subject_pos = indices->at(i) + pattern_len;
  }
  // Add remaining subject content at the end.
  if (subject_pos < subject_len) {
    String::WriteToFlat(*subject, result->GetChars() + result_pos, subject_pos,
                        subject_len);
  }

  int32_t match_indices[] = {indices->at(matches - 1),
                             indices->at(matches - 1) + pattern_len};
  RegExpImpl::SetLastMatchInfo(last_match_info, subject, 0, match_indices);

  TruncateRegexpIndicesList(isolate);

  return *result;
}

MUST_USE_RESULT static Object* StringReplaceGlobalRegExpWithString(
    Isolate* isolate, Handle<String> subject, Handle<JSRegExp> regexp,
    Handle<String> replacement, Handle<RegExpMatchInfo> last_match_info) {
  DCHECK(subject->IsFlat());
  DCHECK(replacement->IsFlat());

  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  // CompiledReplacement uses zone allocation.
  Zone zone(isolate->allocator(), ZONE_NAME);
  CompiledReplacement compiled_replacement(&zone);
  bool simple_replace =
      compiled_replacement.Compile(replacement, capture_count, subject_length);

  // Shortcut for simple non-regexp global replacements
  if (regexp->TypeTag() == JSRegExp::ATOM && simple_replace) {
    if (subject->HasOnlyOneByteChars() && replacement->HasOnlyOneByteChars()) {
      return StringReplaceGlobalAtomRegExpWithString<SeqOneByteString>(
          isolate, subject, regexp, replacement, last_match_info);
    } else {
      return StringReplaceGlobalAtomRegExpWithString<SeqTwoByteString>(
          isolate, subject, regexp, replacement, last_match_info);
    }
  }

  RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
  if (global_cache.HasException()) return isolate->heap()->exception();

  int32_t* current_match = global_cache.FetchNext();
  if (current_match == NULL) {
    if (global_cache.HasException()) return isolate->heap()->exception();
    return *subject;
  }

  // Guessing the number of parts that the final result string is built
  // from. Global regexps can match any number of times, so we guess
  // conservatively.
  int expected_parts = (compiled_replacement.parts() + 1) * 4 + 1;
  ReplacementStringBuilder builder(isolate->heap(), subject, expected_parts);

  // Number of parts added by compiled replacement plus preceeding
  // string and possibly suffix after last match.  It is possible for
  // all components to use two elements when encoded as two smis.
  const int parts_added_per_loop = 2 * (compiled_replacement.parts() + 2);

  int prev = 0;

  do {
    builder.EnsureCapacity(parts_added_per_loop);

    int start = current_match[0];
    int end = current_match[1];

    if (prev < start) {
      builder.AddSubjectSlice(prev, start);
    }

    if (simple_replace) {
      builder.AddString(replacement);
    } else {
      compiled_replacement.Apply(&builder, start, end, current_match);
    }
    prev = end;

    current_match = global_cache.FetchNext();
  } while (current_match != NULL);

  if (global_cache.HasException()) return isolate->heap()->exception();

  if (prev < subject_length) {
    builder.EnsureCapacity(2);
    builder.AddSubjectSlice(prev, subject_length);
  }

  RegExpImpl::SetLastMatchInfo(last_match_info, subject, capture_count,
                               global_cache.LastSuccessfulMatch());

  RETURN_RESULT_OR_FAILURE(isolate, builder.ToString());
}

template <typename ResultSeqString>
MUST_USE_RESULT static Object* StringReplaceGlobalRegExpWithEmptyString(
    Isolate* isolate, Handle<String> subject, Handle<JSRegExp> regexp,
    Handle<RegExpMatchInfo> last_match_info) {
  DCHECK(subject->IsFlat());

  // Shortcut for simple non-regexp global replacements
  if (regexp->TypeTag() == JSRegExp::ATOM) {
    Handle<String> empty_string = isolate->factory()->empty_string();
    if (subject->IsOneByteRepresentation()) {
      return StringReplaceGlobalAtomRegExpWithString<SeqOneByteString>(
          isolate, subject, regexp, empty_string, last_match_info);
    } else {
      return StringReplaceGlobalAtomRegExpWithString<SeqTwoByteString>(
          isolate, subject, regexp, empty_string, last_match_info);
    }
  }

  RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
  if (global_cache.HasException()) return isolate->heap()->exception();

  int32_t* current_match = global_cache.FetchNext();
  if (current_match == NULL) {
    if (global_cache.HasException()) return isolate->heap()->exception();
    return *subject;
  }

  int start = current_match[0];
  int end = current_match[1];
  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  int new_length = subject_length - (end - start);
  if (new_length == 0) return isolate->heap()->empty_string();

  Handle<ResultSeqString> answer;
  if (ResultSeqString::kHasOneByteEncoding) {
    answer = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawOneByteString(new_length).ToHandleChecked());
  } else {
    answer = Handle<ResultSeqString>::cast(
        isolate->factory()->NewRawTwoByteString(new_length).ToHandleChecked());
  }

  int prev = 0;
  int position = 0;

  do {
    start = current_match[0];
    end = current_match[1];
    if (prev < start) {
      // Add substring subject[prev;start] to answer string.
      String::WriteToFlat(*subject, answer->GetChars() + position, prev, start);
      position += start - prev;
    }
    prev = end;

    current_match = global_cache.FetchNext();
  } while (current_match != NULL);

  if (global_cache.HasException()) return isolate->heap()->exception();

  RegExpImpl::SetLastMatchInfo(last_match_info, subject, capture_count,
                               global_cache.LastSuccessfulMatch());

  if (prev < subject_length) {
    // Add substring subject[prev;length] to answer string.
    String::WriteToFlat(*subject, answer->GetChars() + position, prev,
                        subject_length);
    position += subject_length - prev;
  }

  if (position == 0) return isolate->heap()->empty_string();

  // Shorten string and fill
  int string_size = ResultSeqString::SizeFor(position);
  int allocated_string_size = ResultSeqString::SizeFor(new_length);
  int delta = allocated_string_size - string_size;

  answer->set_length(position);
  if (delta == 0) return *answer;

  Address end_of_string = answer->address() + string_size;
  Heap* heap = isolate->heap();

  // The trimming is performed on a newly allocated object, which is on a
  // fresly allocated page or on an already swept page. Hence, the sweeper
  // thread can not get confused with the filler creation. No synchronization
  // needed.
  // TODO(hpayer): We should shrink the large object page if the size
  // of the object changed significantly.
  if (!heap->lo_space()->Contains(*answer)) {
    heap->CreateFillerObjectAt(end_of_string, delta, ClearRecordedSlots::kNo);
  }
  heap->AdjustLiveBytes(*answer, -delta);
  return *answer;
}

namespace {

Object* StringReplaceGlobalRegExpWithStringHelper(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
    Handle<String> replacement, Handle<RegExpMatchInfo> last_match_info) {
  CHECK(regexp->GetFlags() & JSRegExp::kGlobal);

  subject = String::Flatten(subject);

  if (replacement->length() == 0) {
    if (subject->HasOnlyOneByteChars()) {
      return StringReplaceGlobalRegExpWithEmptyString<SeqOneByteString>(
          isolate, subject, regexp, last_match_info);
    } else {
      return StringReplaceGlobalRegExpWithEmptyString<SeqTwoByteString>(
          isolate, subject, regexp, last_match_info);
    }
  }

  replacement = String::Flatten(replacement);

  return StringReplaceGlobalRegExpWithString(isolate, subject, regexp,
                                             replacement, last_match_info);
}

}  // namespace

RUNTIME_FUNCTION(Runtime_StringReplaceGlobalRegExpWithString) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, replacement, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_HANDLE_CHECKED(RegExpMatchInfo, last_match_info, 3);

  return StringReplaceGlobalRegExpWithStringHelper(
      isolate, regexp, subject, replacement, last_match_info);
}

RUNTIME_FUNCTION(Runtime_StringSplit) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);
  CHECK(limit > 0);

  int subject_length = subject->length();
  int pattern_length = pattern->length();
  CHECK(pattern_length > 0);

  if (limit == 0xffffffffu) {
    FixedArray* last_match_cache_unused;
    Handle<Object> cached_answer(
        RegExpResultsCache::Lookup(isolate->heap(), *subject, *pattern,
                                   &last_match_cache_unused,
                                   RegExpResultsCache::STRING_SPLIT_SUBSTRINGS),
        isolate);
    if (*cached_answer != Smi::kZero) {
      // The cache FixedArray is a COW-array and can therefore be reused.
      Handle<JSArray> result = isolate->factory()->NewJSArrayWithElements(
          Handle<FixedArray>::cast(cached_answer));
      return *result;
    }
  }

  // The limit can be very large (0xffffffffu), but since the pattern
  // isn't empty, we can never create more parts than ~half the length
  // of the subject.

  subject = String::Flatten(subject);
  pattern = String::Flatten(pattern);

  List<int>* indices = GetRewoundRegexpIndicesList(isolate);

  FindStringIndicesDispatch(isolate, *subject, *pattern, indices, limit);

  if (static_cast<uint32_t>(indices->length()) < limit) {
    indices->Add(subject_length);
  }

  // The list indices now contains the end of each part to create.

  // Create JSArray of substrings separated by separator.
  int part_count = indices->length();

  Handle<JSArray> result =
      isolate->factory()->NewJSArray(FAST_ELEMENTS, part_count, part_count,
                                     INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);

  DCHECK(result->HasFastObjectElements());

  Handle<FixedArray> elements(FixedArray::cast(result->elements()));

  if (part_count == 1 && indices->at(0) == subject_length) {
    elements->set(0, *subject);
  } else {
    int part_start = 0;
    FOR_WITH_HANDLE_SCOPE(isolate, int, i = 0, i, i < part_count, i++, {
      int part_end = indices->at(i);
      Handle<String> substring =
          isolate->factory()->NewProperSubString(subject, part_start, part_end);
      elements->set(i, *substring);
      part_start = part_end + pattern_length;
    });
  }

  if (limit == 0xffffffffu) {
    if (result->HasFastObjectElements()) {
      RegExpResultsCache::Enter(isolate, subject, pattern, elements,
                                isolate->factory()->empty_fixed_array(),
                                RegExpResultsCache::STRING_SPLIT_SUBSTRINGS);
    }
  }

  TruncateRegexpIndicesList(isolate);

  return *result;
}

// ES##sec-regexpcreate
// RegExpCreate ( P, F )
RUNTIME_FUNCTION(Runtime_RegExpCreate) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, source_object, 0);

  Handle<String> source;
  if (source_object->IsUndefined(isolate)) {
    source = isolate->factory()->empty_string();
  } else {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, source, Object::ToString(isolate, source_object));
  }

  Handle<Map> map(isolate->regexp_function()->initial_map());
  Handle<JSRegExp> regexp =
      Handle<JSRegExp>::cast(isolate->factory()->NewJSObjectFromMap(map));

  JSRegExp::Flags flags = JSRegExp::kNone;

  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              JSRegExp::Initialize(regexp, source, flags));

  return *regexp;
}

RUNTIME_FUNCTION(Runtime_RegExpExec) {
  HandleScope scope(isolate);
  DCHECK_EQ(4, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  CONVERT_INT32_ARG_CHECKED(index, 2);
  CONVERT_ARG_HANDLE_CHECKED(RegExpMatchInfo, last_match_info, 3);
  // Due to the way the JS calls are constructed this must be less than the
  // length of a string, i.e. it is always a Smi.  We check anyway for security.
  CHECK(index >= 0);
  CHECK(index <= subject->length());
  isolate->counters()->regexp_entry_runtime()->Increment();
  RETURN_RESULT_OR_FAILURE(
      isolate, RegExpImpl::Exec(regexp, subject, index, last_match_info));
}

RUNTIME_FUNCTION(Runtime_RegExpInternalReplace) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, replacement, 2);

  Handle<RegExpMatchInfo> internal_match_info =
      isolate->regexp_internal_match_info();

  return StringReplaceGlobalRegExpWithStringHelper(
      isolate, regexp, subject, replacement, internal_match_info);
}

namespace {

class MatchInfoBackedMatch : public String::Match {
 public:
  MatchInfoBackedMatch(Isolate* isolate, Handle<String> subject,
                       Handle<RegExpMatchInfo> match_info)
      : isolate_(isolate), match_info_(match_info) {
    subject_ = String::Flatten(subject);
  }

  Handle<String> GetMatch() override {
    return RegExpUtils::GenericCaptureGetter(isolate_, match_info_, 0, nullptr);
  }

  MaybeHandle<String> GetCapture(int i, bool* capture_exists) override {
    Handle<Object> capture_obj = RegExpUtils::GenericCaptureGetter(
        isolate_, match_info_, i, capture_exists);
    return (*capture_exists) ? Object::ToString(isolate_, capture_obj)
                             : isolate_->factory()->empty_string();
  }

  Handle<String> GetPrefix() override {
    const int match_start = match_info_->Capture(0);
    return isolate_->factory()->NewSubString(subject_, 0, match_start);
  }

  Handle<String> GetSuffix() override {
    const int match_end = match_info_->Capture(1);
    return isolate_->factory()->NewSubString(subject_, match_end,
                                             subject_->length());
  }

  int CaptureCount() override {
    return match_info_->NumberOfCaptureRegisters() / 2;
  }

  virtual ~MatchInfoBackedMatch() {}

 private:
  Isolate* isolate_;
  Handle<String> subject_;
  Handle<RegExpMatchInfo> match_info_;
};

class VectorBackedMatch : public String::Match {
 public:
  VectorBackedMatch(Isolate* isolate, Handle<String> subject,
                    Handle<String> match, int match_position,
                    ZoneVector<Handle<Object>>* captures)
      : isolate_(isolate),
        match_(match),
        match_position_(match_position),
        captures_(captures) {
    subject_ = String::Flatten(subject);
  }

  Handle<String> GetMatch() override { return match_; }

  MaybeHandle<String> GetCapture(int i, bool* capture_exists) override {
    Handle<Object> capture_obj = captures_->at(i);
    if (capture_obj->IsUndefined(isolate_)) {
      *capture_exists = false;
      return isolate_->factory()->empty_string();
    }
    *capture_exists = true;
    return Object::ToString(isolate_, capture_obj);
  }

  Handle<String> GetPrefix() override {
    return isolate_->factory()->NewSubString(subject_, 0, match_position_);
  }

  Handle<String> GetSuffix() override {
    const int match_end_position = match_position_ + match_->length();
    return isolate_->factory()->NewSubString(subject_, match_end_position,
                                             subject_->length());
  }

  int CaptureCount() override { return static_cast<int>(captures_->size()); }

  virtual ~VectorBackedMatch() {}

 private:
  Isolate* isolate_;
  Handle<String> subject_;
  Handle<String> match_;
  const int match_position_;
  ZoneVector<Handle<Object>>* captures_;
};

// Only called from Runtime_RegExpExecMultiple so it doesn't need to maintain
// separate last match info.  See comment on that function.
template <bool has_capture>
static Object* SearchRegExpMultiple(Isolate* isolate, Handle<String> subject,
                                    Handle<JSRegExp> regexp,
                                    Handle<RegExpMatchInfo> last_match_array,
                                    Handle<JSArray> result_array) {
  DCHECK(subject->IsFlat());
  DCHECK_NE(has_capture, regexp->CaptureCount() == 0);

  int capture_count = regexp->CaptureCount();
  int subject_length = subject->length();

  static const int kMinLengthToCache = 0x1000;

  if (subject_length > kMinLengthToCache) {
    FixedArray* last_match_cache;
    Object* cached_answer = RegExpResultsCache::Lookup(
        isolate->heap(), *subject, regexp->data(), &last_match_cache,
        RegExpResultsCache::REGEXP_MULTIPLE_INDICES);
    if (cached_answer->IsFixedArray()) {
      int capture_registers = (capture_count + 1) * 2;
      int32_t* last_match = NewArray<int32_t>(capture_registers);
      for (int i = 0; i < capture_registers; i++) {
        last_match[i] = Smi::cast(last_match_cache->get(i))->value();
      }
      Handle<FixedArray> cached_fixed_array =
          Handle<FixedArray>(FixedArray::cast(cached_answer));
      // The cache FixedArray is a COW-array and we need to return a copy.
      Handle<FixedArray> copied_fixed_array =
          isolate->factory()->CopyFixedArrayWithMap(
              cached_fixed_array, isolate->factory()->fixed_array_map());
      JSArray::SetContent(result_array, copied_fixed_array);
      RegExpImpl::SetLastMatchInfo(last_match_array, subject, capture_count,
                                   last_match);
      DeleteArray(last_match);
      return *result_array;
    }
  }

  RegExpImpl::GlobalCache global_cache(regexp, subject, isolate);
  if (global_cache.HasException()) return isolate->heap()->exception();

  // Ensured in Runtime_RegExpExecMultiple.
  DCHECK(result_array->HasFastObjectElements());
  Handle<FixedArray> result_elements(
      FixedArray::cast(result_array->elements()));
  if (result_elements->length() < 16) {
    result_elements = isolate->factory()->NewFixedArrayWithHoles(16);
  }

  FixedArrayBuilder builder(result_elements);

  // Position to search from.
  int match_start = -1;
  int match_end = 0;
  bool first = true;

  // Two smis before and after the match, for very long strings.
  static const int kMaxBuilderEntriesPerRegExpMatch = 5;

  while (true) {
    int32_t* current_match = global_cache.FetchNext();
    if (current_match == NULL) break;
    match_start = current_match[0];
    builder.EnsureCapacity(kMaxBuilderEntriesPerRegExpMatch);
    if (match_end < match_start) {
      ReplacementStringBuilder::AddSubjectSlice(&builder, match_end,
                                                match_start);
    }
    match_end = current_match[1];
    {
      // Avoid accumulating new handles inside loop.
      HandleScope temp_scope(isolate);
      Handle<String> match;
      if (!first) {
        match = isolate->factory()->NewProperSubString(subject, match_start,
                                                       match_end);
      } else {
        match =
            isolate->factory()->NewSubString(subject, match_start, match_end);
        first = false;
      }

      if (has_capture) {
        // Arguments array to replace function is match, captures, index and
        // subject, i.e., 3 + capture count in total.
        Handle<FixedArray> elements =
            isolate->factory()->NewFixedArray(3 + capture_count);

        elements->set(0, *match);
        for (int i = 1; i <= capture_count; i++) {
          int start = current_match[i * 2];
          if (start >= 0) {
            int end = current_match[i * 2 + 1];
            DCHECK(start <= end);
            Handle<String> substring =
                isolate->factory()->NewSubString(subject, start, end);
            elements->set(i, *substring);
          } else {
            DCHECK(current_match[i * 2 + 1] < 0);
            elements->set(i, isolate->heap()->undefined_value());
          }
        }
        elements->set(capture_count + 1, Smi::FromInt(match_start));
        elements->set(capture_count + 2, *subject);
        builder.Add(*isolate->factory()->NewJSArrayWithElements(elements));
      } else {
        builder.Add(*match);
      }
    }
  }

  if (global_cache.HasException()) return isolate->heap()->exception();

  if (match_start >= 0) {
    // Finished matching, with at least one match.
    if (match_end < subject_length) {
      ReplacementStringBuilder::AddSubjectSlice(&builder, match_end,
                                                subject_length);
    }

    RegExpImpl::SetLastMatchInfo(last_match_array, subject, capture_count,
                                 global_cache.LastSuccessfulMatch());

    if (subject_length > kMinLengthToCache) {
      // Store the last successful match into the array for caching.
      // TODO(yangguo): do not expose last match to JS and simplify caching.
      int capture_registers = (capture_count + 1) * 2;
      Handle<FixedArray> last_match_cache =
          isolate->factory()->NewFixedArray(capture_registers);
      int32_t* last_match = global_cache.LastSuccessfulMatch();
      for (int i = 0; i < capture_registers; i++) {
        last_match_cache->set(i, Smi::FromInt(last_match[i]));
      }
      Handle<FixedArray> result_fixed_array = builder.array();
      result_fixed_array->Shrink(builder.length());
      // Cache the result and copy the FixedArray into a COW array.
      Handle<FixedArray> copied_fixed_array =
          isolate->factory()->CopyFixedArrayWithMap(
              result_fixed_array, isolate->factory()->fixed_array_map());
      RegExpResultsCache::Enter(
          isolate, subject, handle(regexp->data(), isolate), copied_fixed_array,
          last_match_cache, RegExpResultsCache::REGEXP_MULTIPLE_INDICES);
    }
    return *builder.ToJSArray(result_array);
  } else {
    return isolate->heap()->null_value();  // No matches at all.
  }
}

MUST_USE_RESULT MaybeHandle<String> StringReplaceNonGlobalRegExpWithFunction(
    Isolate* isolate, Handle<String> subject, Handle<JSRegExp> regexp,
    Handle<Object> replace_obj) {
  Factory* factory = isolate->factory();
  Handle<RegExpMatchInfo> last_match_info = isolate->regexp_last_match_info();

  // TODO(jgruber): This is a pattern we could refactor.
  Handle<Object> match_indices_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, match_indices_obj,
      RegExpImpl::Exec(regexp, subject, 0, last_match_info), String);

  if (match_indices_obj->IsNull(isolate)) {
    RETURN_ON_EXCEPTION(isolate, RegExpUtils::SetLastIndex(isolate, regexp, 0),
                        String);
    return subject;
  }

  Handle<RegExpMatchInfo> match_indices =
      Handle<RegExpMatchInfo>::cast(match_indices_obj);

  const int index = match_indices->Capture(0);
  const int end_of_match = match_indices->Capture(1);

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(factory->NewSubString(subject, 0, index));

  // Compute the parameter list consisting of the match, captures, index,
  // and subject for the replace function invocation.
  // The number of captures plus one for the match.
  const int m = match_indices->NumberOfCaptureRegisters() / 2;

  const int argc = m + 2;
  ScopedVector<Handle<Object>> argv(argc);

  for (int j = 0; j < m; j++) {
    bool ok;
    Handle<String> capture =
        RegExpUtils::GenericCaptureGetter(isolate, match_indices, j, &ok);
    if (ok) {
      argv[j] = capture;
    } else {
      argv[j] = factory->undefined_value();
    }
  }

  argv[argc - 2] = handle(Smi::FromInt(index), isolate);
  argv[argc - 1] = subject;

  Handle<Object> replacement_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, replacement_obj,
      Execution::Call(isolate, replace_obj, factory->undefined_value(), argc,
                      argv.start()),
      String);

  Handle<String> replacement;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, replacement, Object::ToString(isolate, replacement_obj), String);

  builder.AppendString(replacement);
  builder.AppendString(
      factory->NewSubString(subject, end_of_match, subject->length()));

  return builder.Finish();
}

// Legacy implementation of RegExp.prototype[Symbol.replace] which
// doesn't properly call the underlying exec method.
MUST_USE_RESULT MaybeHandle<String> RegExpReplace(Isolate* isolate,
                                                  Handle<JSRegExp> regexp,
                                                  Handle<String> string,
                                                  Handle<Object> replace_obj) {
  Factory* factory = isolate->factory();

  // TODO(jgruber): We need the even stricter guarantee of an unmodified
  // JSRegExp map here for access to GetFlags to be legal.
  const int flags = regexp->GetFlags();
  const bool global = (flags & JSRegExp::kGlobal) != 0;

  // Functional fast-paths are dispatched directly by replace builtin.
  DCHECK(!replace_obj->IsCallable());

  Handle<String> replace;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, replace,
                             Object::ToString(isolate, replace_obj), String);
  replace = String::Flatten(replace);

  Handle<RegExpMatchInfo> last_match_info = isolate->regexp_last_match_info();

  if (!global) {
    // Non-global regexp search, string replace.

    Handle<Object> match_indices_obj;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, match_indices_obj,
        RegExpImpl::Exec(regexp, string, 0, last_match_info), String);

    if (match_indices_obj->IsNull(isolate)) {
      RETURN_ON_EXCEPTION(
          isolate, RegExpUtils::SetLastIndex(isolate, regexp, 0), String);
      return string;
    }

    auto match_indices = Handle<RegExpMatchInfo>::cast(match_indices_obj);

    const int start_index = match_indices->Capture(0);
    const int end_index = match_indices->Capture(1);

    IncrementalStringBuilder builder(isolate);
    builder.AppendString(factory->NewSubString(string, 0, start_index));

    if (replace->length() > 0) {
      MatchInfoBackedMatch m(isolate, string, match_indices);
      Handle<String> replacement;
      ASSIGN_RETURN_ON_EXCEPTION(isolate, replacement,
                                 String::GetSubstitution(isolate, &m, replace),
                                 String);
      builder.AppendString(replacement);
    }

    builder.AppendString(
        factory->NewSubString(string, end_index, string->length()));
    return builder.Finish();
  } else {
    // Global regexp search, string replace.
    DCHECK(global);
    RETURN_ON_EXCEPTION(isolate, RegExpUtils::SetLastIndex(isolate, regexp, 0),
                        String);

    if (replace->length() == 0) {
      if (string->HasOnlyOneByteChars()) {
        Object* result =
            StringReplaceGlobalRegExpWithEmptyString<SeqOneByteString>(
                isolate, string, regexp, last_match_info);
        return handle(String::cast(result), isolate);
      } else {
        Object* result =
            StringReplaceGlobalRegExpWithEmptyString<SeqTwoByteString>(
                isolate, string, regexp, last_match_info);
        return handle(String::cast(result), isolate);
      }
    }

    Object* result = StringReplaceGlobalRegExpWithString(
        isolate, string, regexp, replace, last_match_info);
    if (result->IsString()) {
      return handle(String::cast(result), isolate);
    } else {
      return MaybeHandle<String>();
    }
  }

  UNREACHABLE();
  return MaybeHandle<String>();
}

}  // namespace

// This is only called for StringReplaceGlobalRegExpWithFunction.
RUNTIME_FUNCTION(Runtime_RegExpExecMultiple) {
  HandleScope handles(isolate);
  DCHECK_EQ(4, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 1);
  CONVERT_ARG_HANDLE_CHECKED(RegExpMatchInfo, last_match_info, 2);
  CONVERT_ARG_HANDLE_CHECKED(JSArray, result_array, 3);
  CHECK(result_array->HasFastObjectElements());

  subject = String::Flatten(subject);
  CHECK(regexp->GetFlags() & JSRegExp::kGlobal);

  if (regexp->CaptureCount() == 0) {
    return SearchRegExpMultiple<false>(isolate, subject, regexp,
                                       last_match_info, result_array);
  } else {
    return SearchRegExpMultiple<true>(isolate, subject, regexp, last_match_info,
                                      result_array);
  }
}

RUNTIME_FUNCTION(Runtime_StringReplaceNonGlobalRegExpWithFunction) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 1);
  CONVERT_ARG_HANDLE_CHECKED(JSObject, replace, 2);

  RETURN_RESULT_OR_FAILURE(isolate, StringReplaceNonGlobalRegExpWithFunction(
                                        isolate, subject, regexp, replace));
}

namespace {

// ES##sec-speciesconstructor
// SpeciesConstructor ( O, defaultConstructor )
MUST_USE_RESULT MaybeHandle<Object> SpeciesConstructor(
    Isolate* isolate, Handle<JSReceiver> recv,
    Handle<JSFunction> default_ctor) {
  Handle<Object> ctor_obj;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, ctor_obj,
      JSObject::GetProperty(recv, isolate->factory()->constructor_string()),
      Object);

  if (ctor_obj->IsUndefined(isolate)) return default_ctor;

  if (!ctor_obj->IsJSReceiver()) {
    THROW_NEW_ERROR(isolate,
                    NewTypeError(MessageTemplate::kConstructorNotReceiver),
                    Object);
  }

  Handle<JSReceiver> ctor = Handle<JSReceiver>::cast(ctor_obj);

  Handle<Object> species;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, species,
      JSObject::GetProperty(ctor, isolate->factory()->species_symbol()),
      Object);

  if (species->IsNullOrUndefined(isolate)) {
    return default_ctor;
  }

  if (species->IsConstructor()) return species;

  THROW_NEW_ERROR(
      isolate, NewTypeError(MessageTemplate::kSpeciesNotConstructor), Object);
}

MUST_USE_RESULT MaybeHandle<Object> ToUint32(Isolate* isolate,
                                             Handle<Object> object,
                                             uint32_t* out) {
  if (object->IsUndefined(isolate)) {
    *out = kMaxUInt32;
    return object;
  }

  Handle<Object> number;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, number, Object::ToNumber(object), Object);
  *out = NumberToUint32(*number);
  return object;
}

Handle<JSArray> NewJSArrayWithElements(Isolate* isolate,
                                       Handle<FixedArray> elems,
                                       int num_elems) {
  elems->Shrink(num_elems);
  return isolate->factory()->NewJSArrayWithElements(elems);
}

}  // namespace

// Slow path for:
// ES#sec-regexp.prototype-@@replace
// RegExp.prototype [ @@split ] ( string, limit )
RUNTIME_FUNCTION(Runtime_RegExpSplit) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  DCHECK(args[1]->IsString());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, recv, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, string, 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, limit_obj, 2);

  Factory* factory = isolate->factory();

  Handle<JSFunction> regexp_fun = isolate->regexp_function();
  Handle<Object> ctor;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, ctor, SpeciesConstructor(isolate, recv, regexp_fun));

  Handle<Object> flags_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, flags_obj, JSObject::GetProperty(recv, factory->flags_string()));

  Handle<String> flags;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, flags,
                                     Object::ToString(isolate, flags_obj));

  Handle<String> u_str = factory->LookupSingleCharacterStringFromCode('u');
  const bool unicode = (String::IndexOf(isolate, flags, u_str, 0) >= 0);

  Handle<String> y_str = factory->LookupSingleCharacterStringFromCode('y');
  const bool sticky = (String::IndexOf(isolate, flags, y_str, 0) >= 0);

  Handle<String> new_flags = flags;
  if (!sticky) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, new_flags,
                                       factory->NewConsString(flags, y_str));
  }

  Handle<JSReceiver> splitter;
  {
    const int argc = 2;

    ScopedVector<Handle<Object>> argv(argc);
    argv[0] = recv;
    argv[1] = new_flags;

    Handle<JSFunction> ctor_fun = Handle<JSFunction>::cast(ctor);
    Handle<Object> splitter_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, splitter_obj, Execution::New(ctor_fun, argc, argv.start()));

    splitter = Handle<JSReceiver>::cast(splitter_obj);
  }

  uint32_t limit;
  RETURN_FAILURE_ON_EXCEPTION(isolate, ToUint32(isolate, limit_obj, &limit));

  const uint32_t length = string->length();

  if (limit == 0) return *factory->NewJSArray(0);

  if (length == 0) {
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, RegExpUtils::RegExpExec(isolate, splitter, string,
                                                 factory->undefined_value()));

    if (!result->IsNull(isolate)) return *factory->NewJSArray(0);

    Handle<FixedArray> elems = factory->NewUninitializedFixedArray(1);
    elems->set(0, *string);
    return *factory->NewJSArrayWithElements(elems);
  }

  static const int kInitialArraySize = 8;
  Handle<FixedArray> elems = factory->NewFixedArrayWithHoles(kInitialArraySize);
  int num_elems = 0;

  uint32_t string_index = 0;
  uint32_t prev_string_index = 0;
  while (string_index < length) {
    RETURN_FAILURE_ON_EXCEPTION(
        isolate, RegExpUtils::SetLastIndex(isolate, splitter, string_index));

    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, RegExpUtils::RegExpExec(isolate, splitter, string,
                                                 factory->undefined_value()));

    if (result->IsNull(isolate)) {
      string_index = RegExpUtils::AdvanceStringIndex(isolate, string,
                                                     string_index, unicode);
      continue;
    }

    Handle<Object> last_index_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, last_index_obj, RegExpUtils::GetLastIndex(isolate, splitter));

    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, last_index_obj, Object::ToLength(isolate, last_index_obj));

    const uint32_t end =
        std::min(PositiveNumberToUint32(*last_index_obj), length);
    if (end == prev_string_index) {
      string_index = RegExpUtils::AdvanceStringIndex(isolate, string,
                                                     string_index, unicode);
      continue;
    }

    {
      Handle<String> substr =
          factory->NewSubString(string, prev_string_index, string_index);
      elems = FixedArray::SetAndGrow(elems, num_elems++, substr);
      if (static_cast<uint32_t>(num_elems) == limit) {
        return *NewJSArrayWithElements(isolate, elems, num_elems);
      }
    }

    prev_string_index = end;

    Handle<Object> num_captures_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num_captures_obj,
        Object::GetProperty(result, isolate->factory()->length_string()));

    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, num_captures_obj, Object::ToLength(isolate, num_captures_obj));
    const int num_captures = PositiveNumberToUint32(*num_captures_obj);

    for (int i = 1; i < num_captures; i++) {
      Handle<Object> capture;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, capture, Object::GetElement(isolate, result, i));
      elems = FixedArray::SetAndGrow(elems, num_elems++, capture);
      if (static_cast<uint32_t>(num_elems) == limit) {
        return *NewJSArrayWithElements(isolate, elems, num_elems);
      }
    }

    string_index = prev_string_index;
  }

  {
    Handle<String> substr =
        factory->NewSubString(string, prev_string_index, length);
    elems = FixedArray::SetAndGrow(elems, num_elems++, substr);
  }

  return *NewJSArrayWithElements(isolate, elems, num_elems);
}

// Slow path for:
// ES#sec-regexp.prototype-@@replace
// RegExp.prototype [ @@replace ] ( string, replaceValue )
RUNTIME_FUNCTION(Runtime_RegExpReplace) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());

  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, recv, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, string, 1);
  Handle<Object> replace_obj = args.at(2);

  Factory* factory = isolate->factory();

  string = String::Flatten(string);

  // Fast-path for unmodified JSRegExps.
  if (RegExpUtils::IsUnmodifiedRegExp(isolate, recv)) {
    RETURN_RESULT_OR_FAILURE(
        isolate, RegExpReplace(isolate, Handle<JSRegExp>::cast(recv), string,
                               replace_obj));
  }

  const uint32_t length = string->length();
  const bool functional_replace = replace_obj->IsCallable();

  Handle<String> replace;
  if (!functional_replace) {
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, replace,
                                       Object::ToString(isolate, replace_obj));
  }

  Handle<Object> global_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, global_obj,
      JSReceiver::GetProperty(recv, factory->global_string()));
  const bool global = global_obj->BooleanValue();

  bool unicode = false;
  if (global) {
    Handle<Object> unicode_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, unicode_obj,
        JSReceiver::GetProperty(recv, factory->unicode_string()));
    unicode = unicode_obj->BooleanValue();

    RETURN_FAILURE_ON_EXCEPTION(isolate,
                                RegExpUtils::SetLastIndex(isolate, recv, 0));
  }

  Zone zone(isolate->allocator(), ZONE_NAME);
  ZoneVector<Handle<Object>> results(&zone);

  while (true) {
    Handle<Object> result;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, result, RegExpUtils::RegExpExec(isolate, recv, string,
                                                 factory->undefined_value()));

    if (result->IsNull(isolate)) break;

    results.push_back(result);
    if (!global) break;

    Handle<Object> match_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, match_obj,
                                       Object::GetElement(isolate, result, 0));

    Handle<String> match;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, match,
                                       Object::ToString(isolate, match_obj));

    if (match->length() == 0) {
      RETURN_FAILURE_ON_EXCEPTION(isolate, RegExpUtils::SetAdvancedStringIndex(
                                               isolate, recv, string, unicode));
    }
  }

  // TODO(jgruber): Look into ReplacementStringBuilder instead.
  IncrementalStringBuilder builder(isolate);
  uint32_t next_source_position = 0;

  for (const auto& result : results) {
    Handle<Object> captures_length_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, captures_length_obj,
        Object::GetProperty(result, factory->length_string()));

    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, captures_length_obj,
        Object::ToLength(isolate, captures_length_obj));
    const int captures_length = PositiveNumberToUint32(*captures_length_obj);

    Handle<Object> match_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, match_obj,
                                       Object::GetElement(isolate, result, 0));

    Handle<String> match;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, match,
                                       Object::ToString(isolate, match_obj));

    const int match_length = match->length();

    Handle<Object> position_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, position_obj,
        Object::GetProperty(result, factory->index_string()));

    // TODO(jgruber): Extract and correct error handling. Since we can go up to
    // 2^53 - 1 (at least for ToLength), we might actually need uint64_t here?
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, position_obj, Object::ToInteger(isolate, position_obj));
    const uint32_t position =
        std::min(PositiveNumberToUint32(*position_obj), length);

    ZoneVector<Handle<Object>> captures(&zone);
    for (int n = 0; n < captures_length; n++) {
      Handle<Object> capture;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, capture, Object::GetElement(isolate, result, n));

      if (!capture->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, capture,
                                           Object::ToString(isolate, capture));
      }
      captures.push_back(capture);
    }

    Handle<String> replacement;
    if (functional_replace) {
      const int argc = captures_length + 2;
      ScopedVector<Handle<Object>> argv(argc);

      for (int j = 0; j < captures_length; j++) {
        argv[j] = captures[j];
      }

      argv[captures_length] = handle(Smi::FromInt(position), isolate);
      argv[captures_length + 1] = string;

      Handle<Object> replacement_obj;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, replacement_obj,
          Execution::Call(isolate, replace_obj, factory->undefined_value(),
                          argc, argv.start()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, replacement, Object::ToString(isolate, replacement_obj));
    } else {
      VectorBackedMatch m(isolate, string, match, position, &captures);
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, replacement, String::GetSubstitution(isolate, &m, replace));
    }

    if (position >= next_source_position) {
      builder.AppendString(
          factory->NewSubString(string, next_source_position, position));
      builder.AppendString(replacement);

      next_source_position = position + match_length;
    }
  }

  if (next_source_position < length) {
    builder.AppendString(
        factory->NewSubString(string, next_source_position, length));
  }

  RETURN_RESULT_OR_FAILURE(isolate, builder.Finish());
}

RUNTIME_FUNCTION(Runtime_RegExpExecReThrow) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(4, args.length());
  Object* exception = isolate->pending_exception();
  isolate->clear_pending_exception();
  return isolate->ReThrow(exception);
}

RUNTIME_FUNCTION(Runtime_RegExpInitializeAndCompile) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(JSRegExp, regexp, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, source, 1);
  CONVERT_ARG_HANDLE_CHECKED(String, flags, 2);

  RETURN_FAILURE_ON_EXCEPTION(isolate,
                              JSRegExp::Initialize(regexp, source, flags));

  return *regexp;
}

RUNTIME_FUNCTION(Runtime_IsRegExp) {
  SealHandleScope shs(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_CHECKED(Object, obj, 0);
  return isolate->heap()->ToBoolean(obj->IsJSRegExp());
}

}  // namespace internal
}  // namespace v8
