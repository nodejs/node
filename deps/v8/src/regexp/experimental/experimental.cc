// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/regexp/experimental/experimental.h"

#include <vector>

#include "src/objects/js-regexp-inl.h"

namespace v8 {
namespace internal {

void ExperimentalRegExp::Initialize(Isolate* isolate, Handle<JSRegExp> re,
                                    Handle<String> source,
                                    JSRegExp::Flags flags, int capture_count) {
  if (FLAG_trace_experimental_regexp_engine) {
    std::cout << "Using experimental regexp engine for: " << *source
              << std::endl;
  }

  isolate->factory()->SetRegExpExperimentalData(re, source, flags,
                                                capture_count);
}

bool ExperimentalRegExp::IsCompiled(Handle<JSRegExp> re) {
  return re->DataAt(JSRegExp::kExperimentalPatternIndex).IsString();
}

void ExperimentalRegExp::Compile(Isolate* isolate, Handle<JSRegExp> re) {
  DCHECK_EQ(re->TypeTag(), JSRegExp::EXPERIMENTAL);
  // TODO(mbid,v8:10765): Actually compile here.
  Handle<FixedArray> data =
      Handle<FixedArray>(FixedArray::cast(re->data()), isolate);
  Handle<Code> trampoline = BUILTIN_CODE(isolate, RegExpExperimentalTrampoline);

  data->set(JSRegExp::kIrregexpLatin1CodeIndex, *trampoline);
  data->set(JSRegExp::kIrregexpUC16CodeIndex, *trampoline);

  data->set(JSRegExp::kExperimentalPatternIndex,
            data->get(JSRegExp::kSourceIndex));
}

struct match_range {
  int32_t begin;
  int32_t end;
};

// Returns the number of matches.
int32_t ExperimentalRegExp::ExecRaw(JSRegExp regexp, String subject,
                                    int32_t* output_registers,
                                    int32_t output_register_count,
                                    int32_t subject_index) {
  String needle =
      String::cast(regexp.DataAt(JSRegExp::kExperimentalPatternIndex));

  if (FLAG_trace_experimental_regexp_engine) {
    std::cout << "Searching for " << output_register_count / 2
              << " occurences of " << needle << " in " << subject << std::endl;
  }

  DCHECK(needle.IsFlat());
  DCHECK(subject.IsFlat());

  const int needle_len = needle.length();
  const int subject_len = subject.length();

  DCHECK_GT(needle_len, 0);

  DCHECK_EQ(output_register_count % 2, 0);

  if (subject_index + needle_len > subject_len) {
    return 0;
  }

  match_range* matches = reinterpret_cast<match_range*>(output_registers);
  const int32_t max_match_num = output_register_count / 2;

  // `state_num` does not overflow because the max length of strings is
  // strictly less than INT_MAX.
  const int state_num = needle_len + 1;
  const int start_state = 0;
  const int accepting_state = needle_len;
  // TODO(mbid,v8:10765): We probably don't want to allocate a new vector here
  // in every execution.
  std::vector<int8_t> in_state(state_num, false);
  in_state[start_state] = true;

  DisallowHeapAllocation no_gc;
  String::FlatContent needle_content = needle.GetFlatContent(no_gc);
  String::FlatContent subject_content = subject.GetFlatContent(no_gc);

  DCHECK(needle_content.IsFlat());
  DCHECK(subject_content.IsFlat());

  int32_t match_num = 0;
  while (subject_index != subject_len && match_num != max_match_num) {
    uc16 subject_char = subject_content.Get(subject_index);

    for (int needle_index = needle_len - 1; needle_index >= 0; --needle_index) {
      uc16 needle_char = needle_content.Get(needle_index);
      if (in_state[needle_index] && needle_char == subject_char) {
        in_state[needle_index + 1] = true;
      } else {
        in_state[needle_index + 1] = false;
      }
    }
    if (in_state[accepting_state]) {
      match_range& match = matches[match_num];
      match.end = subject_index + 1;
      match.begin = match.end - needle_len;
      if (FLAG_trace_experimental_regexp_engine) {
        std::cout << "Found match at [" << match.begin << ", " << match.end
                  << ")" << std::endl;
      }
      ++match_num;
      in_state.assign(state_num, false);
      in_state[start_state] = true;
    }
    ++subject_index;
  }

  return match_num;
}

int32_t ExperimentalRegExp::MatchForCallFromJs(
    Address subject, int32_t start_position, Address input_start,
    Address input_end, int* output_registers, int32_t output_register_count,
    Address backtrack_stack, RegExp::CallOrigin call_origin, Isolate* isolate,
    Address regexp) {
  DCHECK_NOT_NULL(isolate);
  DCHECK_NOT_NULL(output_registers);
  DCHECK(call_origin == RegExp::CallOrigin::kFromJs);

  DisallowHeapAllocation no_gc;
  DisallowJavascriptExecution no_js(isolate);
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  String subject_string = String::cast(Object(subject));

  JSRegExp regexp_obj = JSRegExp::cast(Object(regexp));

  return ExecRaw(regexp_obj, subject_string, output_registers,
                 output_register_count, start_position);
}

MaybeHandle<Object> ExperimentalRegExp::Exec(
    Isolate* isolate, Handle<JSRegExp> regexp, Handle<String> subject,
    int subject_index, Handle<RegExpMatchInfo> last_match_info) {
  regexp->DataAt(JSRegExp::kExperimentalPatternIndex);
  if (!IsCompiled(regexp)) {
    Compile(isolate, regexp);
  }

  subject = String::Flatten(isolate, subject);

  match_range match;

  int32_t* output_registers = &match.begin;
  int32_t output_register_count = sizeof(match_range) / sizeof(int32_t);

  int capture_count = regexp->CaptureCount();

  int num_matches = ExecRaw(*regexp, *subject, output_registers,
                            output_register_count, subject_index);

  if (num_matches == 0) {
    return isolate->factory()->null_value();
  } else {
    DCHECK_EQ(num_matches, 1);
    return RegExp::SetLastMatchInfo(isolate, last_match_info, subject,
                                    capture_count, output_registers);
    return last_match_info;
  }
}

}  // namespace internal
}  // namespace v8
