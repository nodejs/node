// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include <functional>

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

namespace {

// Returns -1 for failure.
uint32_t GetArgcForReplaceCallable(uint32_t num_captures,
                                   bool has_named_captures) {
  const uint32_t kAdditionalArgsWithoutNamedCaptures = 2;
  const uint32_t kAdditionalArgsWithNamedCaptures = 3;
  if (num_captures > Code::kMaxArguments) return -1;
  uint32_t argc = has_named_captures
                      ? num_captures + kAdditionalArgsWithNamedCaptures
                      : num_captures + kAdditionalArgsWithoutNamedCaptures;
  STATIC_ASSERT(Code::kMaxArguments < std::numeric_limits<uint32_t>::max() -
                                          kAdditionalArgsWithNamedCaptures);
  return (argc > Code::kMaxArguments) ? -1 : argc;
}

// Looks up the capture of the given name. Returns the (1-based) numbered
// capture index or -1 on failure.
int LookupNamedCapture(std::function<bool(String*)> name_matches,
                       FixedArray* capture_name_map) {
  // TODO(jgruber): Sort capture_name_map and do binary search via
  // internalized strings.

  int maybe_capture_index = -1;
  const int named_capture_count = capture_name_map->length() >> 1;
  for (int j = 0; j < named_capture_count; j++) {
    // The format of {capture_name_map} is documented at
    // JSRegExp::kIrregexpCaptureNameMapIndex.
    const int name_ix = j * 2;
    const int index_ix = j * 2 + 1;

    String* capture_name = String::cast(capture_name_map->get(name_ix));
    if (!name_matches(capture_name)) continue;

    maybe_capture_index = Smi::ToInt(capture_name_map->get(index_ix));
    break;
  }

  return maybe_capture_index;
}

}  // namespace

class CompiledReplacement {
 public:
  explicit CompiledReplacement(Zone* zone)
      : parts_(1, zone), replacement_substrings_(0, zone), zone_(zone) {}

  // Return whether the replacement is simple.
  bool Compile(Handle<JSRegExp> regexp, Handle<String> replacement,
               int capture_count, int subject_length);

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
    EMPTY_REPLACEMENT,
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
    static inline ReplacementPart EmptyReplacement() {
      return ReplacementPart(EMPTY_REPLACEMENT, 0);
    }
    static inline ReplacementPart ReplacementSubString(int from, int to) {
      DCHECK_LE(0, from);
      DCHECK_GT(to, from);
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
    // tag == EMPTY_REPLACEMENT: data is unused.
    // tag <= 0: Temporary representation of the substring of the replacement
    //           string ranging over -tag .. data.
    //           Is replaced by REPLACEMENT_{SUB,}STRING when we create the
    //           substring objects.
    int data;
  };

  template <typename Char>
  bool ParseReplacementPattern(ZoneList<ReplacementPart>* parts,
                               Vector<Char> characters,
                               FixedArray* capture_name_map, int capture_count,
                               int subject_length, Zone* zone) {
    // Equivalent to String::GetSubstitution, except that this method converts
    // the replacement string into an internal representation that avoids
    // repeated parsing when used repeatedly.
    DCHECK_IMPLIES(capture_name_map != nullptr,
                   FLAG_harmony_regexp_named_captures);

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
          case '<': {
            if (capture_name_map == nullptr) {
              i = next_index;
              break;
            }

            // Scan until the next '>', and let the enclosed substring be the
            // groupName.

            const int name_start_index = next_index + 1;
            int closing_bracket_index = -1;
            for (int j = name_start_index; j < length; j++) {
              if (characters[j] == '>') {
                closing_bracket_index = j;
                break;
              }
            }

            // If no closing bracket is found, '$<' is treated as a string
            // literal.
            if (closing_bracket_index == -1) {
              i = next_index;
              break;
            }

            Vector<Char> requested_name =
                characters.SubVector(name_start_index, closing_bracket_index);

            // Let capture be ? Get(namedCaptures, groupName).

            const int capture_index = LookupNamedCapture(
                [=](String* capture_name) {
                  return capture_name->IsEqualTo(requested_name);
                },
                capture_name_map);

            // If capture is undefined or does not exist, replace the text
            // through the following '>' with the empty string.
            // Otherwise, replace the text through the following '>' with
            // ? ToString(capture).

            DCHECK(capture_index == -1 ||
                   (1 <= capture_index && capture_index <= capture_count));

            if (i > last) {
              parts->Add(ReplacementPart::ReplacementSubString(last, i), zone);
            }
            parts->Add((capture_index == -1)
                           ? ReplacementPart::EmptyReplacement()
                           : ReplacementPart::SubjectCapture(capture_index),
                       zone);
            last = closing_bracket_index + 1;
            i = closing_bracket_index;
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

bool CompiledReplacement::Compile(Handle<JSRegExp> regexp,
                                  Handle<String> replacement, int capture_count,
                                  int subject_length) {
  {
    DisallowHeapAllocation no_gc;
    String::FlatContent content = replacement->GetFlatContent();
    DCHECK(content.IsFlat());

    FixedArray* capture_name_map = nullptr;
    if (capture_count > 0) {
      DCHECK_EQ(regexp->TypeTag(), JSRegExp::IRREGEXP);
      Object* maybe_capture_name_map = regexp->CaptureNameMap();
      if (maybe_capture_name_map->IsFixedArray()) {
        DCHECK(FLAG_harmony_regexp_named_captures);
        capture_name_map = FixedArray::cast(maybe_capture_name_map);
      }
    }

    bool simple;
    if (content.IsOneByte()) {
      simple = ParseReplacementPattern(&parts_, content.ToOneByteVector(),
                                       capture_name_map, capture_count,
                                       subject_length, zone());
    } else {
      DCHECK(content.IsTwoByte());
      simple = ParseReplacementPattern(&parts_, content.ToUC16Vector(),
                                       capture_name_map, capture_count,
                                       subject_length, zone());
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
      case EMPTY_REPLACEMENT:
        break;
      default:
        UNREACHABLE();
    }
  }
}

void FindOneByteStringIndices(Vector<const uint8_t> subject, uint8_t pattern,
                              std::vector<int>* indices, unsigned int limit) {
  DCHECK_LT(0, limit);
  // Collect indices of pattern in subject using memchr.
  // Stop after finding at most limit values.
  const uint8_t* subject_start = subject.start();
  const uint8_t* subject_end = subject_start + subject.length();
  const uint8_t* pos = subject_start;
  while (limit > 0) {
    pos = reinterpret_cast<const uint8_t*>(
        memchr(pos, pattern, subject_end - pos));
    if (pos == nullptr) return;
    indices->push_back(static_cast<int>(pos - subject_start));
    pos++;
    limit--;
  }
}

void FindTwoByteStringIndices(const Vector<const uc16> subject, uc16 pattern,
                              std::vector<int>* indices, unsigned int limit) {
  DCHECK_LT(0, limit);
  const uc16* subject_start = subject.start();
  const uc16* subject_end = subject_start + subject.length();
  for (const uc16* pos = subject_start; pos < subject_end && limit > 0; pos++) {
    if (*pos == pattern) {
      indices->push_back(static_cast<int>(pos - subject_start));
      limit--;
    }
  }
}

template <typename SubjectChar, typename PatternChar>
void FindStringIndices(Isolate* isolate, Vector<const SubjectChar> subject,
                       Vector<const PatternChar> pattern,
                       std::vector<int>* indices, unsigned int limit) {
  DCHECK_LT(0, limit);
  // Collect indices of pattern in subject.
  // Stop after finding at most limit values.
  int pattern_length = pattern.length();
  int index = 0;
  StringSearch<PatternChar, SubjectChar> search(isolate, pattern);
  while (limit > 0) {
    index = search.Search(subject, index);
    if (index < 0) return;
    indices->push_back(index);
    index += pattern_length;
    limit--;
  }
}

void FindStringIndicesDispatch(Isolate* isolate, String* subject,
                               String* pattern, std::vector<int>* indices,
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
std::vector<int>* GetRewoundRegexpIndicesList(Isolate* isolate) {
  std::vector<int>* list = isolate->regexp_indices();
  list->clear();
  return list;
}

void TruncateRegexpIndicesList(Isolate* isolate) {
  // Same size as smallest zone segment, preserving behavior from the
  // runtime zone.
  static const int kMaxRegexpIndicesListCapacity = 8 * KB;
  std::vector<int>* indicies = isolate->regexp_indices();
  if (indicies->capacity() > kMaxRegexpIndicesListCapacity) {
    // Throw away backing storage.
    indicies->clear();
    indicies->shrink_to_fit();
  }
}
}  // namespace

template <typename ResultSeqString>
MUST_USE_RESULT static Object* StringReplaceGlobalAtomRegExpWithString(
    Isolate* isolate, Handle<String> subject, Handle<JSRegExp> pattern_regexp,
    Handle<String> replacement, Handle<RegExpMatchInfo> last_match_info) {
  DCHECK(subject->IsFlat());
  DCHECK(replacement->IsFlat());

  std::vector<int>* indices = GetRewoundRegexpIndicesList(isolate);

  DCHECK_EQ(JSRegExp::ATOM, pattern_regexp->TypeTag());
  String* pattern =
      String::cast(pattern_regexp->DataAt(JSRegExp::kAtomPatternIndex));
  int subject_len = subject->length();
  int pattern_len = pattern->length();
  int replacement_len = replacement->length();

  FindStringIndicesDispatch(isolate, *subject, pattern, indices, 0xffffffff);

  if (indices->empty()) return *subject;

  // Detect integer overflow.
  int64_t result_len_64 = (static_cast<int64_t>(replacement_len) -
                           static_cast<int64_t>(pattern_len)) *
                              static_cast<int64_t>(indices->size()) +
                          static_cast<int64_t>(subject_len);
  int result_len;
  if (result_len_64 > static_cast<int64_t>(String::kMaxLength)) {
    STATIC_ASSERT(String::kMaxLength < kMaxInt);
    result_len = kMaxInt;  // Provoke exception.
  } else {
    result_len = static_cast<int>(result_len_64);
  }
  if (result_len == 0) {
    return isolate->heap()->empty_string();
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

  for (int index : *indices) {
    // Copy non-matched subject content.
    if (subject_pos < index) {
      String::WriteToFlat(*subject, result->GetChars() + result_pos,
                          subject_pos, index);
      result_pos += index - subject_pos;
    }

    // Replace match.
    if (replacement_len > 0) {
      String::WriteToFlat(*replacement, result->GetChars() + result_pos, 0,
                          replacement_len);
      result_pos += replacement_len;
    }

    subject_pos = index + pattern_len;
  }
  // Add remaining subject content at the end.
  if (subject_pos < subject_len) {
    String::WriteToFlat(*subject, result->GetChars() + result_pos, subject_pos,
                        subject_len);
  }

  int32_t match_indices[] = {indices->back(), indices->back() + pattern_len};
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

  JSRegExp::Type typeTag = regexp->TypeTag();
  if (typeTag == JSRegExp::IRREGEXP) {
    // Ensure the RegExp is compiled so we can access the capture-name map.
    if (RegExpImpl::IrregexpPrepare(regexp, subject) == -1) {
      DCHECK(isolate->has_pending_exception());
      return isolate->heap()->exception();
    }
  }

  // CompiledReplacement uses zone allocation.
  Zone zone(isolate->allocator(), ZONE_NAME);
  CompiledReplacement compiled_replacement(&zone);
  const bool simple_replace = compiled_replacement.Compile(
      regexp, replacement, capture_count, subject_length);

  // Shortcut for simple non-regexp global replacements
  if (typeTag == JSRegExp::ATOM && simple_replace) {
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
  if (current_match == nullptr) {
    if (global_cache.HasException()) return isolate->heap()->exception();
    return *subject;
  }

  // Guessing the number of parts that the final result string is built
  // from. Global regexps can match any number of times, so we guess
  // conservatively.
  int expected_parts = (compiled_replacement.parts() + 1) * 4 + 1;
  ReplacementStringBuilder builder(isolate->heap(), subject, expected_parts);

  // Number of parts added by compiled replacement plus preceding
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
  } while (current_match != nullptr);

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
  if (current_match == nullptr) {
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
  } while (current_match != nullptr);

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
  // freshly allocated page or on an already swept page. Hence, the sweeper
  // thread can not get confused with the filler creation. No synchronization
  // needed.
  // TODO(hpayer): We should shrink the large object page if the size
  // of the object changed significantly.
  if (!heap->lo_space()->Contains(*answer)) {
    heap->CreateFillerObjectAt(end_of_string, delta, ClearRecordedSlots::kNo);
  }
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

RUNTIME_FUNCTION(Runtime_StringSplit) {
  HandleScope handle_scope(isolate);
  DCHECK_EQ(3, args.length());
  CONVERT_ARG_HANDLE_CHECKED(String, subject, 0);
  CONVERT_ARG_HANDLE_CHECKED(String, pattern, 1);
  CONVERT_NUMBER_CHECKED(uint32_t, limit, Uint32, args[2]);
  CHECK_LT(0, limit);

  int subject_length = subject->length();
  int pattern_length = pattern->length();
  CHECK_LT(0, pattern_length);

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

  std::vector<int>* indices = GetRewoundRegexpIndicesList(isolate);

  FindStringIndicesDispatch(isolate, *subject, *pattern, indices, limit);

  if (static_cast<uint32_t>(indices->size()) < limit) {
    indices->push_back(subject_length);
  }

  // The list indices now contains the end of each part to create.

  // Create JSArray of substrings separated by separator.
  int part_count = static_cast<int>(indices->size());

  Handle<JSArray> result =
      isolate->factory()->NewJSArray(PACKED_ELEMENTS, part_count, part_count,
                                     INITIALIZE_ARRAY_ELEMENTS_WITH_HOLE);

  DCHECK(result->HasObjectElements());

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
    if (result->HasObjectElements()) {
      RegExpResultsCache::Enter(isolate, subject, pattern, elements,
                                isolate->factory()->empty_fixed_array(),
                                RegExpResultsCache::STRING_SPLIT_SUBSTRINGS);
    }
  }

  TruncateRegexpIndicesList(isolate);

  return *result;
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
  CHECK_LE(0, index);
  CHECK_GE(subject->length(), index);
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
  MatchInfoBackedMatch(Isolate* isolate, Handle<JSRegExp> regexp,
                       Handle<String> subject,
                       Handle<RegExpMatchInfo> match_info)
      : isolate_(isolate), match_info_(match_info) {
    subject_ = String::Flatten(subject);

    if (regexp->TypeTag() == JSRegExp::IRREGEXP) {
      Object* o = regexp->CaptureNameMap();
      has_named_captures_ = o->IsFixedArray();
      if (has_named_captures_) {
        DCHECK(FLAG_harmony_regexp_named_captures);
        capture_name_map_ = handle(FixedArray::cast(o));
      }
    } else {
      has_named_captures_ = false;
    }
  }

  Handle<String> GetMatch() override {
    return RegExpUtils::GenericCaptureGetter(isolate_, match_info_, 0, nullptr);
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

  bool HasNamedCaptures() override { return has_named_captures_; }

  int CaptureCount() override {
    return match_info_->NumberOfCaptureRegisters() / 2;
  }

  MaybeHandle<String> GetCapture(int i, bool* capture_exists) override {
    Handle<Object> capture_obj = RegExpUtils::GenericCaptureGetter(
        isolate_, match_info_, i, capture_exists);
    return (*capture_exists) ? Object::ToString(isolate_, capture_obj)
                             : isolate_->factory()->empty_string();
  }

  MaybeHandle<String> GetNamedCapture(Handle<String> name,
                                      CaptureState* state) override {
    DCHECK(has_named_captures_);
    const int capture_index = LookupNamedCapture(
        [=](String* capture_name) { return capture_name->Equals(*name); },
        *capture_name_map_);

    if (capture_index == -1) {
      *state = INVALID;
      return name;  // Arbitrary string handle.
    }

    DCHECK(1 <= capture_index && capture_index <= CaptureCount());

    bool capture_exists;
    Handle<String> capture_value;
    ASSIGN_RETURN_ON_EXCEPTION(isolate_, capture_value,
                               GetCapture(capture_index, &capture_exists),
                               String);

    if (!capture_exists) {
      *state = UNMATCHED;
      return isolate_->factory()->empty_string();
    } else {
      *state = MATCHED;
      return capture_value;
    }
  }

 private:
  Isolate* isolate_;
  Handle<String> subject_;
  Handle<RegExpMatchInfo> match_info_;

  bool has_named_captures_;
  Handle<FixedArray> capture_name_map_;
};

class VectorBackedMatch : public String::Match {
 public:
  VectorBackedMatch(Isolate* isolate, Handle<String> subject,
                    Handle<String> match, int match_position,
                    ZoneVector<Handle<Object>>* captures,
                    Handle<Object> groups_obj)
      : isolate_(isolate),
        match_(match),
        match_position_(match_position),
        captures_(captures) {
    subject_ = String::Flatten(subject);

    DCHECK(groups_obj->IsUndefined(isolate) || groups_obj->IsJSReceiver());
    has_named_captures_ = !groups_obj->IsUndefined(isolate);
    if (has_named_captures_) groups_obj_ = Handle<JSReceiver>::cast(groups_obj);
  }

  Handle<String> GetMatch() override { return match_; }

  Handle<String> GetPrefix() override {
    return isolate_->factory()->NewSubString(subject_, 0, match_position_);
  }

  Handle<String> GetSuffix() override {
    const int match_end_position = match_position_ + match_->length();
    return isolate_->factory()->NewSubString(subject_, match_end_position,
                                             subject_->length());
  }

  bool HasNamedCaptures() override { return has_named_captures_; }

  int CaptureCount() override { return static_cast<int>(captures_->size()); }

  MaybeHandle<String> GetCapture(int i, bool* capture_exists) override {
    Handle<Object> capture_obj = captures_->at(i);
    if (capture_obj->IsUndefined(isolate_)) {
      *capture_exists = false;
      return isolate_->factory()->empty_string();
    }
    *capture_exists = true;
    return Object::ToString(isolate_, capture_obj);
  }

  MaybeHandle<String> GetNamedCapture(Handle<String> name,
                                      CaptureState* state) override {
    DCHECK(has_named_captures_);

    Maybe<bool> maybe_capture_exists =
        JSReceiver::HasProperty(groups_obj_, name);
    if (maybe_capture_exists.IsNothing()) return MaybeHandle<String>();

    if (!maybe_capture_exists.FromJust()) {
      *state = INVALID;
      return name;  // Arbitrary string handle.
    }

    Handle<Object> capture_obj;
    ASSIGN_RETURN_ON_EXCEPTION(isolate_, capture_obj,
                               Object::GetProperty(groups_obj_, name), String);
    if (capture_obj->IsUndefined(isolate_)) {
      *state = UNMATCHED;
      return isolate_->factory()->empty_string();
    } else {
      *state = MATCHED;
      return Object::ToString(isolate_, capture_obj);
    }
  }

 private:
  Isolate* isolate_;
  Handle<String> subject_;
  Handle<String> match_;
  const int match_position_;
  ZoneVector<Handle<Object>>* captures_;

  bool has_named_captures_;
  Handle<JSReceiver> groups_obj_;
};

// Create the groups object (see also the RegExp result creation in
// RegExpBuiltinsAssembler::ConstructNewResultFromMatchInfo).
Handle<JSObject> ConstructNamedCaptureGroupsObject(
    Isolate* isolate, Handle<FixedArray> capture_map,
    std::function<Object*(int)> f_get_capture) {
  DCHECK(FLAG_harmony_regexp_named_captures);
  Handle<JSObject> groups = isolate->factory()->NewJSObjectWithNullProto();

  const int capture_count = capture_map->length() >> 1;
  for (int i = 0; i < capture_count; i++) {
    const int name_ix = i * 2;
    const int index_ix = i * 2 + 1;

    Handle<String> capture_name(String::cast(capture_map->get(name_ix)));
    const int capture_ix = Smi::ToInt(capture_map->get(index_ix));
    DCHECK(1 <= capture_ix && capture_ix <= capture_count);

    Handle<Object> capture_value(f_get_capture(capture_ix), isolate);
    DCHECK(capture_value->IsUndefined(isolate) || capture_value->IsString());

    JSObject::AddProperty(groups, capture_name, capture_value, NONE);
  }

  return groups;
}

// Only called from Runtime_RegExpExecMultiple so it doesn't need to maintain
// separate last match info.  See comment on that function.
template <bool has_capture>
static Object* SearchRegExpMultiple(Isolate* isolate, Handle<String> subject,
                                    Handle<JSRegExp> regexp,
                                    Handle<RegExpMatchInfo> last_match_array,
                                    Handle<JSArray> result_array) {
  DCHECK(RegExpUtils::IsUnmodifiedRegExp(isolate, regexp));
  DCHECK_NE(has_capture, regexp->CaptureCount() == 0);
  DCHECK(subject->IsFlat());

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
        last_match[i] = Smi::ToInt(last_match_cache->get(i));
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
  DCHECK(result_array->HasObjectElements());
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
    if (current_match == nullptr) break;
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
        // subject, i.e., 3 + capture count in total. If the RegExp contains
        // named captures, they are also passed as the last argument.

        Handle<Object> maybe_capture_map(regexp->CaptureNameMap(), isolate);
        const bool has_named_captures = maybe_capture_map->IsFixedArray();
        DCHECK_IMPLIES(has_named_captures, FLAG_harmony_regexp_named_captures);

        const int argc =
            has_named_captures ? 4 + capture_count : 3 + capture_count;

        Handle<FixedArray> elements = isolate->factory()->NewFixedArray(argc);
        int cursor = 0;

        elements->set(cursor++, *match);
        for (int i = 1; i <= capture_count; i++) {
          int start = current_match[i * 2];
          if (start >= 0) {
            int end = current_match[i * 2 + 1];
            DCHECK(start <= end);
            Handle<String> substring =
                isolate->factory()->NewSubString(subject, start, end);
            elements->set(cursor++, *substring);
          } else {
            DCHECK_GT(0, current_match[i * 2 + 1]);
            elements->set(cursor++, isolate->heap()->undefined_value());
          }
        }

        elements->set(cursor++, Smi::FromInt(match_start));
        elements->set(cursor++, *subject);

        if (has_named_captures) {
          Handle<FixedArray> capture_map =
              Handle<FixedArray>::cast(maybe_capture_map);
          Handle<JSObject> groups = ConstructNamedCaptureGroupsObject(
              isolate, capture_map, [=](int ix) { return elements->get(ix); });
          elements->set(cursor++, *groups);
        }

        DCHECK_EQ(cursor, argc);
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

// Legacy implementation of RegExp.prototype[Symbol.replace] which
// doesn't properly call the underlying exec method.
MUST_USE_RESULT MaybeHandle<String> RegExpReplace(Isolate* isolate,
                                                  Handle<JSRegExp> regexp,
                                                  Handle<String> string,
                                                  Handle<Object> replace_obj) {
  // Functional fast-paths are dispatched directly by replace builtin.
  DCHECK(RegExpUtils::IsUnmodifiedRegExp(isolate, regexp));
  DCHECK(!replace_obj->IsCallable());

  Factory* factory = isolate->factory();

  const int flags = regexp->GetFlags();
  const bool global = (flags & JSRegExp::kGlobal) != 0;
  const bool sticky = (flags & JSRegExp::kSticky) != 0;

  Handle<String> replace;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, replace,
                             Object::ToString(isolate, replace_obj), String);
  replace = String::Flatten(replace);

  Handle<RegExpMatchInfo> last_match_info = isolate->regexp_last_match_info();

  if (!global) {
    // Non-global regexp search, string replace.

    uint32_t last_index = 0;
    if (sticky) {
      Handle<Object> last_index_obj(regexp->last_index(), isolate);
      ASSIGN_RETURN_ON_EXCEPTION(isolate, last_index_obj,
                                 Object::ToLength(isolate, last_index_obj),
                                 String);
      last_index = PositiveNumberToUint32(*last_index_obj);

      if (last_index > static_cast<uint32_t>(string->length())) last_index = 0;
    }

    Handle<Object> match_indices_obj;
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, match_indices_obj,
        RegExpImpl::Exec(regexp, string, last_index, last_match_info), String);

    if (match_indices_obj->IsNull(isolate)) {
      if (sticky) regexp->set_last_index(Smi::kZero, SKIP_WRITE_BARRIER);
      return string;
    }

    auto match_indices = Handle<RegExpMatchInfo>::cast(match_indices_obj);

    const int start_index = match_indices->Capture(0);
    const int end_index = match_indices->Capture(1);

    if (sticky)
      regexp->set_last_index(Smi::FromInt(end_index), SKIP_WRITE_BARRIER);

    IncrementalStringBuilder builder(isolate);
    builder.AppendString(factory->NewSubString(string, 0, start_index));

    if (replace->length() > 0) {
      MatchInfoBackedMatch m(isolate, regexp, string, match_indices);
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
  CHECK(result_array->HasObjectElements());

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
  CONVERT_ARG_HANDLE_CHECKED(JSReceiver, replace_obj, 2);

  DCHECK(RegExpUtils::IsUnmodifiedRegExp(isolate, regexp));
  DCHECK(replace_obj->map()->is_callable());

  Factory* factory = isolate->factory();
  Handle<RegExpMatchInfo> last_match_info = isolate->regexp_last_match_info();

  const int flags = regexp->GetFlags();
  DCHECK_EQ(flags & JSRegExp::kGlobal, 0);

  // TODO(jgruber): This should be an easy port to CSA with massive payback.

  const bool sticky = (flags & JSRegExp::kSticky) != 0;
  uint32_t last_index = 0;
  if (sticky) {
    Handle<Object> last_index_obj(regexp->last_index(), isolate);
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, last_index_obj, Object::ToLength(isolate, last_index_obj));
    last_index = PositiveNumberToUint32(*last_index_obj);

    if (last_index > static_cast<uint32_t>(subject->length())) last_index = 0;
  }

  Handle<Object> match_indices_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, match_indices_obj,
      RegExpImpl::Exec(regexp, subject, last_index, last_match_info));

  if (match_indices_obj->IsNull(isolate)) {
    if (sticky) regexp->set_last_index(Smi::kZero, SKIP_WRITE_BARRIER);
    return *subject;
  }

  Handle<RegExpMatchInfo> match_indices =
      Handle<RegExpMatchInfo>::cast(match_indices_obj);

  const int index = match_indices->Capture(0);
  const int end_of_match = match_indices->Capture(1);

  if (sticky)
    regexp->set_last_index(Smi::FromInt(end_of_match), SKIP_WRITE_BARRIER);

  IncrementalStringBuilder builder(isolate);
  builder.AppendString(factory->NewSubString(subject, 0, index));

  // Compute the parameter list consisting of the match, captures, index,
  // and subject for the replace function invocation. If the RegExp contains
  // named captures, they are also passed as the last argument.

  // The number of captures plus one for the match.
  const int m = match_indices->NumberOfCaptureRegisters() / 2;

  bool has_named_captures = false;
  Handle<FixedArray> capture_map;
  if (m > 1) {
    // The existence of capture groups implies IRREGEXP kind.
    DCHECK_EQ(regexp->TypeTag(), JSRegExp::IRREGEXP);

    Object* maybe_capture_map = regexp->CaptureNameMap();
    if (maybe_capture_map->IsFixedArray()) {
      has_named_captures = true;
      capture_map = handle(FixedArray::cast(maybe_capture_map));
    }
  }

  DCHECK_IMPLIES(has_named_captures, FLAG_harmony_regexp_named_captures);
  const uint32_t argc = GetArgcForReplaceCallable(m, has_named_captures);
  if (argc == static_cast<uint32_t>(-1)) {
    THROW_NEW_ERROR_RETURN_FAILURE(
        isolate, NewRangeError(MessageTemplate::kTooManyArguments));
  }
  ScopedVector<Handle<Object>> argv(argc);

  int cursor = 0;
  for (int j = 0; j < m; j++) {
    bool ok;
    Handle<String> capture =
        RegExpUtils::GenericCaptureGetter(isolate, match_indices, j, &ok);
    if (ok) {
      argv[cursor++] = capture;
    } else {
      argv[cursor++] = factory->undefined_value();
    }
  }

  argv[cursor++] = handle(Smi::FromInt(index), isolate);
  argv[cursor++] = subject;

  if (has_named_captures) {
    argv[cursor++] = ConstructNamedCaptureGroupsObject(
        isolate, capture_map, [&argv](int ix) { return *argv[ix]; });
  }

  DCHECK_EQ(cursor, argc);

  Handle<Object> replacement_obj;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, replacement_obj,
      Execution::Call(isolate, replace_obj, factory->undefined_value(), argc,
                      argv.start()));

  Handle<String> replacement;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, replacement, Object::ToString(isolate, replacement_obj));

  builder.AppendString(replacement);
  builder.AppendString(
      factory->NewSubString(subject, end_of_match, subject->length()));

  RETURN_RESULT_OR_FAILURE(isolate, builder.Finish());
}

namespace {

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
      isolate, ctor, Object::SpeciesConstructor(isolate, recv, regexp_fun));

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

    Handle<Object> splitter_obj;
    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, splitter_obj,
        Execution::New(isolate, ctor, argc, argv.start()));

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
  uint32_t num_elems = 0;

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
      string_index = static_cast<uint32_t>(RegExpUtils::AdvanceStringIndex(
          isolate, string, string_index, unicode));
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
      string_index = static_cast<uint32_t>(RegExpUtils::AdvanceStringIndex(
          isolate, string, string_index, unicode));
      continue;
    }

    {
      Handle<String> substr =
          factory->NewSubString(string, prev_string_index, string_index);
      elems = FixedArray::SetAndGrow(elems, num_elems++, substr);
      if (num_elems == limit) {
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
    const uint32_t num_captures = PositiveNumberToUint32(*num_captures_obj);

    for (uint32_t i = 1; i < num_captures; i++) {
      Handle<Object> capture;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, capture, Object::GetElement(isolate, result, i));
      elems = FixedArray::SetAndGrow(elems, num_elems++, capture);
      if (num_elems == limit) {
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
    const uint32_t captures_length =
        PositiveNumberToUint32(*captures_length_obj);

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

    ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
        isolate, position_obj, Object::ToInteger(isolate, position_obj));
    const uint32_t position =
        std::min(PositiveNumberToUint32(*position_obj), length);

    // Do not reserve capacity since captures_length is user-controlled.
    ZoneVector<Handle<Object>> captures(&zone);

    for (uint32_t n = 0; n < captures_length; n++) {
      Handle<Object> capture;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, capture, Object::GetElement(isolate, result, n));

      if (!capture->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, capture,
                                           Object::ToString(isolate, capture));
      }
      captures.push_back(capture);
    }

    Handle<Object> groups_obj = isolate->factory()->undefined_value();
    if (FLAG_harmony_regexp_named_captures) {
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, groups_obj,
          Object::GetProperty(result, factory->groups_string()));
    }

    const bool has_named_captures = !groups_obj->IsUndefined(isolate);
    DCHECK_IMPLIES(has_named_captures, FLAG_harmony_regexp_named_captures);

    Handle<String> replacement;
    if (functional_replace) {
      const uint32_t argc =
          GetArgcForReplaceCallable(captures_length, has_named_captures);
      if (argc == static_cast<uint32_t>(-1)) {
        THROW_NEW_ERROR_RETURN_FAILURE(
            isolate, NewRangeError(MessageTemplate::kTooManyArguments));
      }

      ScopedVector<Handle<Object>> argv(argc);

      int cursor = 0;
      for (uint32_t j = 0; j < captures_length; j++) {
        argv[cursor++] = captures[j];
      }

      argv[cursor++] = handle(Smi::FromInt(position), isolate);
      argv[cursor++] = string;
      if (has_named_captures) argv[cursor++] = groups_obj;

      DCHECK_EQ(cursor, argc);

      Handle<Object> replacement_obj;
      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, replacement_obj,
          Execution::Call(isolate, replace_obj, factory->undefined_value(),
                          argc, argv.start()));

      ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
          isolate, replacement, Object::ToString(isolate, replacement_obj));
    } else {
      DCHECK(!functional_replace);
      if (!groups_obj->IsUndefined(isolate)) {
        ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
            isolate, groups_obj, JSReceiver::ToObject(isolate, groups_obj));
      }
      VectorBackedMatch m(isolate, string, match, position, &captures,
                          groups_obj);
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
  DCHECK_EQ(0, args.length());
  Object* exception = isolate->pending_exception();
  isolate->clear_pending_exception();
  return isolate->ReThrow(exception);
}

RUNTIME_FUNCTION(Runtime_RegExpInitializeAndCompile) {
  HandleScope scope(isolate);
  DCHECK_EQ(3, args.length());
  // TODO(pwong): To follow the spec more closely and simplify calling code,
  // this could handle the canonicalization of pattern and flags. See
  // https://tc39.github.io/ecma262/#sec-regexpinitialize
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
