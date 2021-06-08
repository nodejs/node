// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-regexp.h"

#include "src/common/globals.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/regexp/regexp.h"

namespace v8 {
namespace internal {

Handle<JSRegExpResultIndices> JSRegExpResultIndices::BuildIndices(
    Isolate* isolate, Handle<RegExpMatchInfo> match_info,
    Handle<Object> maybe_names) {
  Handle<JSRegExpResultIndices> indices(Handle<JSRegExpResultIndices>::cast(
      isolate->factory()->NewJSObjectFromMap(
          isolate->regexp_result_indices_map())));

  // Initialize indices length to avoid having a partially initialized object
  // should GC be triggered by creating a NewFixedArray.
  indices->set_length(Smi::zero());

  // Build indices array from RegExpMatchInfo.
  int num_indices = match_info->NumberOfCaptureRegisters();
  int num_results = num_indices >> 1;
  Handle<FixedArray> indices_array =
      isolate->factory()->NewFixedArray(num_results);
  JSArray::SetContent(indices, indices_array);

  for (int i = 0; i < num_results; i++) {
    int base_offset = i * 2;
    int start_offset = match_info->Capture(base_offset);
    int end_offset = match_info->Capture(base_offset + 1);

    // Any unmatched captures are set to undefined, otherwise we set them to a
    // subarray of the indices.
    if (start_offset == -1) {
      indices_array->set(i, ReadOnlyRoots(isolate).undefined_value());
    } else {
      Handle<FixedArray> indices_sub_array(
          isolate->factory()->NewFixedArray(2));
      indices_sub_array->set(0, Smi::FromInt(start_offset));
      indices_sub_array->set(1, Smi::FromInt(end_offset));
      Handle<JSArray> indices_sub_jsarray =
          isolate->factory()->NewJSArrayWithElements(indices_sub_array,
                                                     PACKED_SMI_ELEMENTS, 2);
      indices_array->set(i, *indices_sub_jsarray);
    }
  }

  // If there are no capture groups, set the groups property to undefined.
  FieldIndex groups_index = FieldIndex::ForDescriptor(
      indices->map(), InternalIndex(kGroupsDescriptorIndex));
  if (maybe_names->IsUndefined(isolate)) {
    indices->FastPropertyAtPut(groups_index,
                               ReadOnlyRoots(isolate).undefined_value());
    return indices;
  }

  // Create a groups property which returns a dictionary of named captures to
  // their corresponding capture indices.
  Handle<FixedArray> names(Handle<FixedArray>::cast(maybe_names));
  int num_names = names->length() >> 1;
  Handle<HeapObject> group_names;
  if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    group_names = isolate->factory()->NewSwissNameDictionary(num_names);
  } else {
    group_names = isolate->factory()->NewNameDictionary(num_names);
  }
  for (int i = 0; i < num_names; i++) {
    int base_offset = i * 2;
    int name_offset = base_offset;
    int index_offset = base_offset + 1;
    Handle<String> name(String::cast(names->get(name_offset)), isolate);
    Handle<Smi> smi_index(Smi::cast(names->get(index_offset)), isolate);
    Handle<Object> capture_indices(indices_array->get(smi_index->value()),
                                   isolate);
    if (!capture_indices->IsUndefined(isolate)) {
      capture_indices = Handle<JSArray>::cast(capture_indices);
    }
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      group_names = SwissNameDictionary::Add(
          isolate, Handle<SwissNameDictionary>::cast(group_names), name,
          capture_indices, PropertyDetails::Empty());
    } else {
      group_names = NameDictionary::Add(
          isolate, Handle<NameDictionary>::cast(group_names), name,
          capture_indices, PropertyDetails::Empty());
    }
  }

  // Convert group_names to a JSObject and store at the groups property of the
  // result indices.
  Handle<FixedArrayBase> elements = isolate->factory()->empty_fixed_array();
  Handle<HeapObject> null =
      Handle<HeapObject>::cast(isolate->factory()->null_value());
  Handle<JSObject> js_group_names =
      isolate->factory()->NewSlowJSObjectWithPropertiesAndElements(
          null, group_names, elements);
  indices->FastPropertyAtPut(groups_index, *js_group_names);
  return indices;
}

uint32_t JSRegExp::BacktrackLimit() const {
  CHECK_EQ(TypeTag(), IRREGEXP);
  return static_cast<uint32_t>(Smi::ToInt(DataAt(kIrregexpBacktrackLimit)));
}

// static
JSRegExp::Flags JSRegExp::FlagsFromString(Isolate* isolate,
                                          Handle<String> flags, bool* success) {
  int length = flags->length();
  if (length == 0) {
    *success = true;
    return JSRegExp::kNone;
  }
  // A longer flags string cannot be valid.
  if (length > JSRegExp::kFlagCount) return JSRegExp::Flags(0);
  JSRegExp::Flags value(0);
  if (flags->IsSeqOneByteString()) {
    DisallowGarbageCollection no_gc;
    SeqOneByteString seq_flags = SeqOneByteString::cast(*flags);
    for (int i = 0; i < length; i++) {
      base::Optional<JSRegExp::Flag> maybe_flag =
          JSRegExp::FlagFromChar(seq_flags.Get(i));
      if (!maybe_flag.has_value()) return JSRegExp::Flags(0);
      JSRegExp::Flag flag = *maybe_flag;
      // Duplicate flag.
      if (value & flag) return JSRegExp::Flags(0);
      value |= flag;
    }
  } else {
    flags = String::Flatten(isolate, flags);
    DisallowGarbageCollection no_gc;
    String::FlatContent flags_content = flags->GetFlatContent(no_gc);
    for (int i = 0; i < length; i++) {
      base::Optional<JSRegExp::Flag> maybe_flag =
          JSRegExp::FlagFromChar(flags_content.Get(i));
      if (!maybe_flag.has_value()) return JSRegExp::Flags(0);
      JSRegExp::Flag flag = *maybe_flag;
      // Duplicate flag.
      if (value & flag) return JSRegExp::Flags(0);
      value |= flag;
    }
  }
  *success = true;
  return value;
}

// static
MaybeHandle<JSRegExp> JSRegExp::New(Isolate* isolate, Handle<String> pattern,
                                    Flags flags, uint32_t backtrack_limit) {
  Handle<JSFunction> constructor = isolate->regexp_function();
  Handle<JSRegExp> regexp =
      Handle<JSRegExp>::cast(isolate->factory()->NewJSObject(constructor));

  return JSRegExp::Initialize(regexp, pattern, flags, backtrack_limit);
}

Object JSRegExp::Code(bool is_latin1) const {
  DCHECK_EQ(TypeTag(), JSRegExp::IRREGEXP);
  return DataAt(code_index(is_latin1));
}

Object JSRegExp::Bytecode(bool is_latin1) const {
  DCHECK_EQ(TypeTag(), JSRegExp::IRREGEXP);
  return DataAt(bytecode_index(is_latin1));
}

bool JSRegExp::ShouldProduceBytecode() {
  return FLAG_regexp_interpret_all ||
         (FLAG_regexp_tier_up && !MarkedForTierUp());
}

// Only irregexps are subject to tier-up.
bool JSRegExp::CanTierUp() {
  return FLAG_regexp_tier_up && TypeTag() == JSRegExp::IRREGEXP;
}

// An irregexp is considered to be marked for tier up if the tier-up ticks
// value reaches zero.
bool JSRegExp::MarkedForTierUp() {
  DCHECK(data().IsFixedArray());

  if (!CanTierUp()) {
    return false;
  }

  return Smi::ToInt(DataAt(kIrregexpTicksUntilTierUpIndex)) == 0;
}

void JSRegExp::ResetLastTierUpTick() {
  DCHECK(FLAG_regexp_tier_up);
  DCHECK_EQ(TypeTag(), JSRegExp::IRREGEXP);
  int tier_up_ticks = Smi::ToInt(DataAt(kIrregexpTicksUntilTierUpIndex)) + 1;
  FixedArray::cast(data()).set(JSRegExp::kIrregexpTicksUntilTierUpIndex,
                               Smi::FromInt(tier_up_ticks));
}

void JSRegExp::TierUpTick() {
  DCHECK(FLAG_regexp_tier_up);
  DCHECK_EQ(TypeTag(), JSRegExp::IRREGEXP);
  int tier_up_ticks = Smi::ToInt(DataAt(kIrregexpTicksUntilTierUpIndex));
  if (tier_up_ticks == 0) {
    return;
  }
  FixedArray::cast(data()).set(JSRegExp::kIrregexpTicksUntilTierUpIndex,
                               Smi::FromInt(tier_up_ticks - 1));
}

void JSRegExp::MarkTierUpForNextExec() {
  DCHECK(FLAG_regexp_tier_up);
  DCHECK_EQ(TypeTag(), JSRegExp::IRREGEXP);
  FixedArray::cast(data()).set(JSRegExp::kIrregexpTicksUntilTierUpIndex,
                               Smi::zero());
}

// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source,
                                           Handle<String> flags_string) {
  Isolate* isolate = regexp->GetIsolate();
  bool success = false;
  Flags flags = JSRegExp::FlagsFromString(isolate, flags_string, &success);
  if (!success) {
    THROW_NEW_ERROR(
        isolate,
        NewSyntaxError(MessageTemplate::kInvalidRegExpFlags, flags_string),
        JSRegExp);
  }
  return Initialize(regexp, source, flags);
}

namespace {

bool IsLineTerminator(int c) {
  // Expected to return true for '\n', '\r', 0x2028, and 0x2029.
  return unibrow::IsLineTerminator(static_cast<unibrow::uchar>(c));
}

// TODO(jgruber): Consider merging CountAdditionalEscapeChars and
// WriteEscapedRegExpSource into a single function to deduplicate dispatch logic
// and move related code closer to each other.
template <typename Char>
int CountAdditionalEscapeChars(Handle<String> source, bool* needs_escapes_out) {
  DisallowGarbageCollection no_gc;
  int escapes = 0;
  bool needs_escapes = false;
  bool in_char_class = false;
  Vector<const Char> src = source->GetCharVector<Char>(no_gc);
  for (int i = 0; i < src.length(); i++) {
    const Char c = src[i];
    if (c == '\\') {
      if (i + 1 < src.length() && IsLineTerminator(src[i + 1])) {
        // This '\' is ignored since the next character itself will be escaped.
        escapes--;
      } else {
        // Escape. Skip next character, which will be copied verbatim;
        i++;
      }
    } else if (c == '/' && !in_char_class) {
      // Not escaped forward-slash needs escape.
      needs_escapes = true;
      escapes++;
    } else if (c == '[') {
      in_char_class = true;
    } else if (c == ']') {
      in_char_class = false;
    } else if (c == '\n') {
      needs_escapes = true;
      escapes++;
    } else if (c == '\r') {
      needs_escapes = true;
      escapes++;
    } else if (static_cast<int>(c) == 0x2028) {
      needs_escapes = true;
      escapes += std::strlen("\\u2028") - 1;
    } else if (static_cast<int>(c) == 0x2029) {
      needs_escapes = true;
      escapes += std::strlen("\\u2029") - 1;
    } else {
      DCHECK(!IsLineTerminator(c));
    }
  }
  DCHECK(!in_char_class);
  DCHECK_GE(escapes, 0);
  DCHECK_IMPLIES(escapes != 0, needs_escapes);
  *needs_escapes_out = needs_escapes;
  return escapes;
}

template <typename Char>
void WriteStringToCharVector(Vector<Char> v, int* d, const char* string) {
  int s = 0;
  while (string[s] != '\0') v[(*d)++] = string[s++];
}

template <typename Char, typename StringType>
Handle<StringType> WriteEscapedRegExpSource(Handle<String> source,
                                            Handle<StringType> result) {
  DisallowGarbageCollection no_gc;
  Vector<const Char> src = source->GetCharVector<Char>(no_gc);
  Vector<Char> dst(result->GetChars(no_gc), result->length());
  int s = 0;
  int d = 0;
  bool in_char_class = false;
  while (s < src.length()) {
    const Char c = src[s];
    if (c == '\\') {
      if (s + 1 < src.length() && IsLineTerminator(src[s + 1])) {
        // This '\' is ignored since the next character itself will be escaped.
        s++;
        continue;
      } else {
        // Escape. Copy this and next character.
        dst[d++] = src[s++];
      }
      if (s == src.length()) break;
    } else if (c == '/' && !in_char_class) {
      // Not escaped forward-slash needs escape.
      dst[d++] = '\\';
    } else if (c == '[') {
      in_char_class = true;
    } else if (c == ']') {
      in_char_class = false;
    } else if (c == '\n') {
      WriteStringToCharVector(dst, &d, "\\n");
      s++;
      continue;
    } else if (c == '\r') {
      WriteStringToCharVector(dst, &d, "\\r");
      s++;
      continue;
    } else if (static_cast<int>(c) == 0x2028) {
      WriteStringToCharVector(dst, &d, "\\u2028");
      s++;
      continue;
    } else if (static_cast<int>(c) == 0x2029) {
      WriteStringToCharVector(dst, &d, "\\u2029");
      s++;
      continue;
    } else {
      DCHECK(!IsLineTerminator(c));
    }
    dst[d++] = src[s++];
  }
  DCHECK_EQ(result->length(), d);
  DCHECK(!in_char_class);
  return result;
}

MaybeHandle<String> EscapeRegExpSource(Isolate* isolate,
                                       Handle<String> source) {
  DCHECK(source->IsFlat());
  if (source->length() == 0) return isolate->factory()->query_colon_string();
  bool one_byte = String::IsOneByteRepresentationUnderneath(*source);
  bool needs_escapes = false;
  int additional_escape_chars =
      one_byte ? CountAdditionalEscapeChars<uint8_t>(source, &needs_escapes)
               : CountAdditionalEscapeChars<uc16>(source, &needs_escapes);
  if (!needs_escapes) return source;
  int length = source->length() + additional_escape_chars;
  if (one_byte) {
    Handle<SeqOneByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawOneByteString(length),
                               String);
    return WriteEscapedRegExpSource<uint8_t>(source, result);
  } else {
    Handle<SeqTwoByteString> result;
    ASSIGN_RETURN_ON_EXCEPTION(isolate, result,
                               isolate->factory()->NewRawTwoByteString(length),
                               String);
    return WriteEscapedRegExpSource<uc16>(source, result);
  }
}

}  // namespace

// static
MaybeHandle<JSRegExp> JSRegExp::Initialize(Handle<JSRegExp> regexp,
                                           Handle<String> source, Flags flags,
                                           uint32_t backtrack_limit) {
  Isolate* isolate = regexp->GetIsolate();
  Factory* factory = isolate->factory();
  // If source is the empty string we set it to "(?:)" instead as
  // suggested by ECMA-262, 5th, section 15.10.4.1.
  if (source->length() == 0) source = factory->query_colon_string();

  source = String::Flatten(isolate, source);

  RETURN_ON_EXCEPTION(
      isolate, RegExp::Compile(isolate, regexp, source, flags, backtrack_limit),
      JSRegExp);

  Handle<String> escaped_source;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, escaped_source,
                             EscapeRegExpSource(isolate, source), JSRegExp);

  regexp->set_source(*escaped_source);
  regexp->set_flags(Smi::FromInt(flags));

  Map map = regexp->map();
  Object constructor = map.GetConstructor();
  if (constructor.IsJSFunction() &&
      JSFunction::cast(constructor).initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyAtPut(JSRegExp::kLastIndexFieldIndex,
                                  Smi::FromInt(kInitialLastIndexValue),
                                  SKIP_WRITE_BARRIER);
  } else {
    // Map has changed, so use generic, but slower, method.
    RETURN_ON_EXCEPTION(
        isolate,
        Object::SetProperty(
            isolate, regexp, factory->lastIndex_string(),
            Handle<Smi>(Smi::FromInt(kInitialLastIndexValue), isolate)),
        JSRegExp);
  }

  return regexp;
}

}  // namespace internal
}  // namespace v8
