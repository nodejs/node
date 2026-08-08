// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-regexp.h"

#include <optional>

#include "src/base/strings.h"
#include "src/common/globals.h"
#include "src/objects/code.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/regexp/regexp.h"

namespace v8::internal {

DirectHandle<JSRegExpResultIndices> JSRegExpResultIndices::BuildIndices(
    Isolate* isolate, DirectHandle<RegExpMatchInfo> match_info,
    DirectHandle<RegExpData> re_data) {
  DirectHandle<JSRegExpResultIndices> indices(
      Cast<JSRegExpResultIndices>(isolate->factory()->NewJSObjectFromMap(
          isolate->regexp_result_indices_map())));

  // Initialize indices length to avoid having a partially initialized object
  // should GC be triggered by creating a NewFixedArray.
  indices->set_length(Smi::zero());

  // Build indices array from RegExpMatchInfo.
  int num_indices = match_info->number_of_capture_registers();
  int num_results = num_indices >> 1;
  DirectHandle<FixedArray> indices_array =
      isolate->factory()->NewFixedArray(num_results);
  JSArray::SetContent(isolate, indices, indices_array);

  for (int i = 0; i < num_results; i++) {
    const int start_offset =
        match_info->capture(RegExpMatchInfo::capture_start_index(i));
    const int end_offset =
        match_info->capture(RegExpMatchInfo::capture_end_index(i));

    // Any unmatched captures are set to undefined, otherwise we set them to a
    // subarray of the indices.
    if (start_offset == -1) {
      indices_array->set(i, ReadOnlyRoots(isolate).undefined_value());
    } else {
      DirectHandle<FixedArray> indices_sub_array(
          isolate->factory()->NewFixedArray(2));
      indices_sub_array->set(0, Smi::FromInt(start_offset));
      indices_sub_array->set(1, Smi::FromInt(end_offset));
      DirectHandle<JSArray> indices_sub_jsarray =
          isolate->factory()->NewJSArrayWithElements(indices_sub_array,
                                                     PACKED_SMI_ELEMENTS, 2);
      indices_array->set(i, *indices_sub_jsarray);
    }
  }

  // If there are no capture groups, set the groups property to undefined.
  FieldIndex groups_index = FieldIndex::ForDescriptor(
      indices->map(), InternalIndex(kGroupsDescriptorIndex));
  if (re_data->type_tag() != RegExpData::Type::IRREGEXP ||
      !TrustedCast<IrRegExpData>(re_data)->has_capture_name_map()) {
    indices->FastPropertyAtPut(groups_index,
                               ReadOnlyRoots(isolate).undefined_value());
    return indices;
  }

  // Create a groups property which returns a dictionary of named captures to
  // their corresponding capture indices.
  auto names = direct_handle(
      TrustedCast<IrRegExpData>(re_data)->capture_name_map(), isolate);
  const int num_names = static_cast<int>(names->ulength().value() >> 1);
  DirectHandle<HeapObject> group_names;
  if constexpr (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
    group_names = isolate->factory()->NewSwissNameDictionary(num_names);
  } else {
    group_names = isolate->factory()->NewNameDictionary(num_names);
  }
  DirectHandle<PropertyDictionary> group_names_dict =
      Cast<PropertyDictionary>(group_names);
  for (int i = 0; i < num_names; i++) {
    int base_offset = i * 2;
    int name_offset = base_offset;
    int index_offset = base_offset + 1;
    DirectHandle<String> name(Cast<String>(names->get(name_offset)), isolate);
    Tagged<Smi> smi_index = Cast<Smi>(names->get(index_offset));
    DirectHandle<Object> capture_indices(indices_array->get(smi_index.value()),
                                         isolate);
    if (!IsUndefined(*capture_indices, isolate)) {
      capture_indices = Cast<JSArray>(capture_indices);
    }
    InternalIndex group_entry = group_names_dict->FindEntry(isolate, name);
    // Duplicate group entries are possible if the capture groups are in
    // different alternatives, i.e. only one of them can actually match.
    // Therefore when we find a duplicate entry, either the current entry is
    // undefined (didn't match anything) or the indices for the current capture
    // are undefined. In the latter case we don't do anything, in the former
    // case we update the entry.
    if (group_entry.is_found()) {
      DCHECK(v8_flags.js_regexp_duplicate_named_groups);
      if (!IsUndefined(*capture_indices, isolate)) {
        DCHECK(IsUndefined(group_names_dict->ValueAt(group_entry), isolate));
        group_names_dict->ValueAtPut(group_entry, *capture_indices);
      }
    } else {
      group_names_dict =
          PropertyDictionary::Add(isolate, group_names_dict, name,
                                  capture_indices, PropertyDetails::Empty())
              .ToHandleChecked();
    }
  }

  // Convert group_names to a JSObject and store at the groups property of the
  // result indices.
  DirectHandle<FixedArrayBase> elements =
      isolate->factory()->empty_fixed_array();
  DirectHandle<Null> null = isolate->factory()->null_value();
  DirectHandle<JSObject> js_group_names =
      isolate->factory()->NewSlowJSObjectWithPropertiesAndElements(
          null, group_names, elements);
  indices->FastPropertyAtPut(groups_index, *js_group_names);
  return indices;
}

// static
std::optional<JSRegExp::Flags> JSRegExp::FlagsFromString(
    Isolate* isolate, DirectHandle<String> flags) {
  const int length = flags->length();

  // A longer flags string cannot be valid.
  if (length > JSRegExp::kFlagCount) return {};

  regexp::Flags value;
  FlatStringReader reader(isolate, String::Flatten(isolate, flags));

  for (int i = 0; i < length; i++) {
    std::optional<regexp::Flag> flag = JSRegExp::FlagFromChar(reader.Get(i));
    if (!flag.has_value()) return {};
    if (value & flag.value()) return {};  // Duplicate.
    value |= flag.value();
  }

  return JSRegExp::AsJSRegExpFlags(value);
}

// static
DirectHandle<String> JSRegExp::StringFromFlags(Isolate* isolate,
                                               JSRegExp::Flags flags) {
  FlagsBuffer buffer;
  return isolate->factory()->NewStringFromAsciiChecked(
      FlagsToString(flags, &buffer));
}

// static
MaybeDirectHandle<JSRegExp> JSRegExp::New(Isolate* isolate,
                                          DirectHandle<String> original_source,
                                          Flags flags,
                                          uint32_t backtrack_limit) {
  DirectHandle<JSFunction> constructor = isolate->regexp_function();
  DirectHandle<JSRegExp> regexp =
      Cast<JSRegExp>(isolate->factory()->NewJSObject(constructor));

  // Clear the data field, as a GC can be triggered before the field is set
  // during compilation.
  regexp->clear_data();

  return JSRegExp::Initialize(isolate, regexp, original_source, flags,
                              backtrack_limit);
}

// static
MaybeDirectHandle<JSRegExp> JSRegExp::Initialize(
    Isolate* isolate, DirectHandle<JSRegExp> regexp,
    DirectHandle<String> original_source, DirectHandle<String> flags_string) {
  std::optional<Flags> flags = JSRegExp::FlagsFromString(isolate, flags_string);
  if (!flags.has_value() ||
      !RegExp::VerifyFlags(JSRegExp::AsRegExpFlags(flags.value()))) {
    THROW_NEW_ERROR(
        isolate,
        NewSyntaxError(MessageTemplate::kInvalidRegExpFlags, flags_string));
  }
  return Initialize(isolate, regexp, original_source, flags.value());
}

// static
MaybeDirectHandle<JSRegExp> JSRegExp::Initialize(
    Isolate* isolate, DirectHandle<JSRegExp> regexp,
    DirectHandle<String> original_source, Flags flags,
    uint32_t backtrack_limit) {
  Factory* factory = isolate->factory();
  // If source is the empty string we set it to "(?:)" instead as
  // suggested by ECMA-262, 5th, section 15.10.4.1.
  if (original_source->length() == 0) {
    original_source = factory->query_colon_string();
  }

  original_source = String::Flatten(isolate, original_source);

  RETURN_ON_EXCEPTION(isolate, RegExp::Compile(isolate, regexp, original_source,
                                               JSRegExp::AsRegExpFlags(flags),
                                               backtrack_limit));

  regexp->set_flags(Smi::FromInt(flags));

  Tagged<Map> map = regexp->map();
  Tagged<Object> constructor = map->GetConstructor();
  if (IsJSFunction(constructor) &&
      Cast<JSFunction>(constructor)->initial_map() == map) {
    // If we still have the original map, set in-object properties directly.
    regexp->InObjectPropertyPutAtOffset(JSRegExp::kLastIndexOffset,
                                        Smi::FromInt(kInitialLastIndexValue),
                                        SKIP_WRITE_BARRIER);
  } else {
    // Map has changed, so use generic, but slower, method.
    RETURN_ON_EXCEPTION(
        isolate,
        Object::SetProperty(
            isolate, regexp, factory->lastIndex_string(),
            DirectHandle<Smi>(Smi::FromInt(kInitialLastIndexValue), isolate)));
  }

  return regexp;
}

bool RegExpData::HasCompiledCode() const {
  if (type_tag() != Type::IRREGEXP) return false;
  Tagged<IrRegExpData> re_data = TrustedCast<IrRegExpData>(this);
  return re_data->has_latin1_code() || re_data->has_uc16_code();
}

// Only irregexps are subject to tier-up.
bool IrRegExpData::CanTierUp() {
  return v8_flags.regexp_tier_up && type_tag() == Type::IRREGEXP;
}

// An irregexp is considered to be marked for tier up if the tier-up ticks
// value reaches zero.
bool IrRegExpData::MarkedForTierUp() {
  if (!CanTierUp()) {
    return false;
  }

  return ticks_until_tier_up() == 0;
}

void IrRegExpData::ResetLastTierUpTick() {
  DCHECK(v8_flags.regexp_tier_up);
  int tier_up_ticks = ticks_until_tier_up();
  set_ticks_until_tier_up(tier_up_ticks + 1);
}

void IrRegExpData::TierUpTick() {
  int tier_up_ticks = ticks_until_tier_up();
  if (tier_up_ticks == 0) {
    return;
  }

  set_ticks_until_tier_up(tier_up_ticks - 1);
}

void IrRegExpData::MarkTierUpForNextExec() {
  DCHECK(v8_flags.regexp_tier_up);
  set_ticks_until_tier_up(0);
}

bool IrRegExpData::ShouldProduceBytecode() {
  return v8_flags.regexp_interpret_all ||
         (v8_flags.regexp_tier_up && !MarkedForTierUp());
}

void IrRegExpData::DiscardCompiledCodeForSerialization() {
  DCHECK(HasCompiledCode());
  clear_latin1_code();
  clear_uc16_code();
  clear_latin1_bytecode();
  clear_uc16_bytecode();
}

void IrRegExpData::SetBytecodeForExperimental(
    Isolate* isolate, Tagged<TrustedByteArray> bytecode) {
  set_latin1_bytecode(bytecode);
  set_uc16_bytecode(bytecode);

  Tagged<Code> trampoline =
      *BUILTIN_CODE(isolate, RegExpExperimentalTrampoline);
  set_latin1_code(trampoline);
  set_uc16_code(trampoline);
}

}  // namespace v8::internal
