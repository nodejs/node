// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/json/json-parser.h"

#include "src/common/message-template.h"
#include "src/debug/debug.h"
#include "src/numbers/conversions.h"
#include "src/numbers/hash-seed-inl.h"
#include "src/objects/field-type.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/strings/char-predicates-inl.h"
#include "src/strings/string-hasher.h"

namespace v8 {
namespace internal {

namespace {

constexpr JsonToken GetOneCharJsonToken(uint8_t c) {
  // clang-format off
  return
     c == '"' ? JsonToken::STRING :
     IsDecimalDigit(c) ?  JsonToken::NUMBER :
     c == '-' ? JsonToken::NUMBER :
     c == '[' ? JsonToken::LBRACK :
     c == '{' ? JsonToken::LBRACE :
     c == ']' ? JsonToken::RBRACK :
     c == '}' ? JsonToken::RBRACE :
     c == 't' ? JsonToken::TRUE_LITERAL :
     c == 'f' ? JsonToken::FALSE_LITERAL :
     c == 'n' ? JsonToken::NULL_LITERAL :
     c == ' ' ? JsonToken::WHITESPACE :
     c == '\t' ? JsonToken::WHITESPACE :
     c == '\r' ? JsonToken::WHITESPACE :
     c == '\n' ? JsonToken::WHITESPACE :
     c == ':' ? JsonToken::COLON :
     c == ',' ? JsonToken::COMMA :
     JsonToken::ILLEGAL;
  // clang-format on
}

// Table of one-character tokens, by character (0x00..0xFF only).
static const constexpr JsonToken one_char_json_tokens[256] = {
#define CALL_GET_SCAN_FLAGS(N) GetOneCharJsonToken(N),
    INT_0_TO_127_LIST(CALL_GET_SCAN_FLAGS)
#undef CALL_GET_SCAN_FLAGS
#define CALL_GET_SCAN_FLAGS(N) GetOneCharJsonToken(128 + N),
        INT_0_TO_127_LIST(CALL_GET_SCAN_FLAGS)
#undef CALL_GET_SCAN_FLAGS
};

enum class EscapeKind : uint8_t {
  kIllegal,
  kSelf,
  kBackspace,
  kTab,
  kNewLine,
  kFormFeed,
  kCarriageReturn,
  kUnicode
};

using EscapeKindField = base::BitField8<EscapeKind, 0, 3>;
using MayTerminateStringField = EscapeKindField::Next<bool, 1>;
using NumberPartField = MayTerminateStringField::Next<bool, 1>;

constexpr bool MayTerminateJsonString(uint8_t flags) {
  return MayTerminateStringField::decode(flags);
}

constexpr EscapeKind GetEscapeKind(uint8_t flags) {
  return EscapeKindField::decode(flags);
}

constexpr bool IsNumberPart(uint8_t flags) {
  return NumberPartField::decode(flags);
}

constexpr uint8_t GetJsonScanFlags(uint8_t c) {
  // clang-format off
  return (c == 'b' ? EscapeKindField::encode(EscapeKind::kBackspace)
          : c == 't' ? EscapeKindField::encode(EscapeKind::kTab)
          : c == 'n' ? EscapeKindField::encode(EscapeKind::kNewLine)
          : c == 'f' ? EscapeKindField::encode(EscapeKind::kFormFeed)
          : c == 'r' ? EscapeKindField::encode(EscapeKind::kCarriageReturn)
          : c == 'u' ? EscapeKindField::encode(EscapeKind::kUnicode)
          : c == '"' ? EscapeKindField::encode(EscapeKind::kSelf)
          : c == '\\' ? EscapeKindField::encode(EscapeKind::kSelf)
          : c == '/' ? EscapeKindField::encode(EscapeKind::kSelf)
          : EscapeKindField::encode(EscapeKind::kIllegal)) |
         (c < 0x20 ? MayTerminateStringField::encode(true)
          : c == '"' ? MayTerminateStringField::encode(true)
          : c == '\\' ? MayTerminateStringField::encode(true)
          : MayTerminateStringField::encode(false)) |
         NumberPartField::encode(c == '.' ||
                                 c == 'e' ||
                                 c == 'E' ||
                                 IsDecimalDigit(c) ||
                                 c == '-' ||
                                 c == '+');
  // clang-format on
}

// Table of one-character scan flags, by character (0x00..0xFF only).
static const constexpr uint8_t character_json_scan_flags[256] = {
#define CALL_GET_SCAN_FLAGS(N) GetJsonScanFlags(N),
    INT_0_TO_127_LIST(CALL_GET_SCAN_FLAGS)
#undef CALL_GET_SCAN_FLAGS
#define CALL_GET_SCAN_FLAGS(N) GetJsonScanFlags(128 + N),
        INT_0_TO_127_LIST(CALL_GET_SCAN_FLAGS)
#undef CALL_GET_SCAN_FLAGS
};

}  // namespace

MaybeHandle<Object> JsonParseInternalizer::Internalize(Isolate* isolate,
                                                       Handle<Object> object,
                                                       Handle<Object> reviver) {
  DCHECK(reviver->IsCallable());
  JsonParseInternalizer internalizer(isolate,
                                     Handle<JSReceiver>::cast(reviver));
  Handle<JSObject> holder =
      isolate->factory()->NewJSObject(isolate->object_function());
  Handle<String> name = isolate->factory()->empty_string();
  JSObject::AddProperty(isolate, holder, name, object, NONE);
  return internalizer.InternalizeJsonProperty(holder, name);
}

MaybeHandle<Object> JsonParseInternalizer::InternalizeJsonProperty(
    Handle<JSReceiver> holder, Handle<String> name) {
  HandleScope outer_scope(isolate_);
  Handle<Object> value;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, value, Object::GetPropertyOrElement(isolate_, holder, name),
      Object);
  if (value->IsJSReceiver()) {
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(value);
    Maybe<bool> is_array = Object::IsArray(object);
    if (is_array.IsNothing()) return MaybeHandle<Object>();
    if (is_array.FromJust()) {
      Handle<Object> length_object;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate_, length_object,
          Object::GetLengthFromArrayLike(isolate_, object), Object);
      double length = length_object->Number();
      for (double i = 0; i < length; i++) {
        HandleScope inner_scope(isolate_);
        Handle<Object> index = isolate_->factory()->NewNumber(i);
        Handle<String> name = isolate_->factory()->NumberToString(index);
        if (!RecurseAndApply(object, name)) return MaybeHandle<Object>();
      }
    } else {
      Handle<FixedArray> contents;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate_, contents,
          KeyAccumulator::GetKeys(object, KeyCollectionMode::kOwnOnly,
                                  ENUMERABLE_STRINGS,
                                  GetKeysConversion::kConvertToString),
          Object);
      for (int i = 0; i < contents->length(); i++) {
        HandleScope inner_scope(isolate_);
        Handle<String> name(String::cast(contents->get(i)), isolate_);
        if (!RecurseAndApply(object, name)) return MaybeHandle<Object>();
      }
    }
  }
  Handle<Object> argv[] = {name, value};
  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate_, result, Execution::Call(isolate_, reviver_, holder, 2, argv),
      Object);
  return outer_scope.CloseAndEscape(result);
}

bool JsonParseInternalizer::RecurseAndApply(Handle<JSReceiver> holder,
                                            Handle<String> name) {
  STACK_CHECK(isolate_, false);

  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION_VALUE(
      isolate_, result, InternalizeJsonProperty(holder, name), false);
  Maybe<bool> change_result = Nothing<bool>();
  if (result->IsUndefined(isolate_)) {
    change_result = JSReceiver::DeletePropertyOrElement(holder, name,
                                                        LanguageMode::kSloppy);
  } else {
    PropertyDescriptor desc;
    desc.set_value(result);
    desc.set_configurable(true);
    desc.set_enumerable(true);
    desc.set_writable(true);
    change_result = JSReceiver::DefineOwnProperty(isolate_, holder, name, &desc,
                                                  Just(kDontThrow));
  }
  MAYBE_RETURN(change_result, false);
  return true;
}

template <typename Char>
JsonParser<Char>::JsonParser(Isolate* isolate, Handle<String> source)
    : isolate_(isolate),
      hash_seed_(HashSeed(isolate)),
      object_constructor_(isolate_->object_function()),
      original_source_(source) {
  size_t start = 0;
  size_t length = source->length();
  if (source->IsSlicedString()) {
    SlicedString string = SlicedString::cast(*source);
    start = string.offset();
    String parent = string.parent();
    if (parent.IsThinString()) parent = ThinString::cast(parent).actual();
    source_ = handle(parent, isolate);
  } else {
    source_ = String::Flatten(isolate, source);
  }

  if (StringShape(*source_).IsExternal()) {
    chars_ =
        static_cast<const Char*>(SeqExternalString::cast(*source_).GetChars());
    chars_may_relocate_ = false;
  } else {
    DisallowHeapAllocation no_gc;
    isolate->heap()->AddGCEpilogueCallback(UpdatePointersCallback,
                                           v8::kGCTypeAll, this);
    chars_ = SeqString::cast(*source_).GetChars(no_gc);
    chars_may_relocate_ = true;
  }
  cursor_ = chars_ + start;
  end_ = cursor_ + length;
}

template <typename Char>
void JsonParser<Char>::ReportUnexpectedToken(JsonToken token) {
  // Some exception (for example stack overflow) is already pending.
  if (isolate_->has_pending_exception()) return;

  // Parse failed. Current character is the unexpected token.
  Factory* factory = this->factory();
  MessageTemplate message;
  int offset = original_source_->IsSlicedString()
                   ? SlicedString::cast(*original_source_).offset()
                   : 0;
  int pos = position() - offset;
  Handle<Object> arg1 = Handle<Smi>(Smi::FromInt(pos), isolate());
  Handle<Object> arg2;

  switch (token) {
    case JsonToken::EOS:
      message = MessageTemplate::kJsonParseUnexpectedEOS;
      break;
    case JsonToken::NUMBER:
      message = MessageTemplate::kJsonParseUnexpectedTokenNumber;
      break;
    case JsonToken::STRING:
      message = MessageTemplate::kJsonParseUnexpectedTokenString;
      break;
    default:
      message = MessageTemplate::kJsonParseUnexpectedToken;
      arg2 = arg1;
      arg1 = factory->LookupSingleCharacterStringFromCode(*cursor_);
      break;
  }

  Handle<Script> script(factory->NewScript(original_source_));
  if (isolate()->NeedsSourcePositionsForProfiling()) {
    Script::InitLineEnds(script);
  }
  // We should sent compile error event because we compile JSON object in
  // separated source file.
  isolate()->debug()->OnCompileError(script);
  MessageLocation location(script, pos, pos + 1);
  Handle<Object> error = factory->NewSyntaxError(message, arg1, arg2);
  isolate()->Throw(*error, &location);

  // Move the cursor to the end so we won't be able to proceed parsing.
  cursor_ = end_;
}

template <typename Char>
void JsonParser<Char>::ReportUnexpectedCharacter(uc32 c) {
  JsonToken token = JsonToken::ILLEGAL;
  if (c == kEndOfString) {
    token = JsonToken::EOS;
  } else if (c <= unibrow::Latin1::kMaxChar) {
    token = one_char_json_tokens[c];
  }
  return ReportUnexpectedToken(token);
}

template <typename Char>
JsonParser<Char>::~JsonParser() {
  if (StringShape(*source_).IsExternal()) {
    // Check that the string shape hasn't changed. Otherwise our GC hooks are
    // broken.
    SeqExternalString::cast(*source_);
  } else {
    // Check that the string shape hasn't changed. Otherwise our GC hooks are
    // broken.
    SeqString::cast(*source_);
    isolate()->heap()->RemoveGCEpilogueCallback(UpdatePointersCallback, this);
  }
}

template <typename Char>
MaybeHandle<Object> JsonParser<Char>::ParseJson() {
  MaybeHandle<Object> result = ParseJsonValue();
  if (!Check(JsonToken::EOS)) ReportUnexpectedToken(peek());
  if (isolate_->has_pending_exception()) return MaybeHandle<Object>();
  return result;
}

MaybeHandle<Object> InternalizeJsonProperty(Handle<JSObject> holder,
                                            Handle<String> key);

template <typename Char>
void JsonParser<Char>::SkipWhitespace() {
  next_ = JsonToken::EOS;

  cursor_ = std::find_if(cursor_, end_, [this](Char c) {
    JsonToken current = V8_LIKELY(c <= unibrow::Latin1::kMaxChar)
                            ? one_char_json_tokens[c]
                            : JsonToken::ILLEGAL;
    bool result = current != JsonToken::WHITESPACE;
    if (result) next_ = current;
    return result;
  });
}

template <typename Char>
uc32 JsonParser<Char>::ScanUnicodeCharacter() {
  uc32 value = 0;
  for (int i = 0; i < 4; i++) {
    int digit = HexValue(NextCharacter());
    if (V8_UNLIKELY(digit < 0)) return -1;
    value = value * 16 + digit;
  }
  return value;
}

// Parse any JSON value.
template <typename Char>
JsonString JsonParser<Char>::ScanJsonPropertyKey(JsonContinuation* cont) {
  {
    DisallowHeapAllocation no_gc;
    const Char* start = cursor_;
    uc32 first = CurrentCharacter();
    if (first == '\\' && NextCharacter() == 'u') first = ScanUnicodeCharacter();
    if (IsDecimalDigit(first)) {
      if (first == '0') {
        if (NextCharacter() == '"') {
          advance();
          // Record element information.
          cont->elements++;
          DCHECK_LE(0, cont->max_index);
          return JsonString(0);
        }
      } else {
        uint32_t index = first - '0';
        while (true) {
          cursor_ = std::find_if(cursor_ + 1, end_, [&index](Char c) {
            return !TryAddArrayIndexChar(&index, c);
          });

          if (CurrentCharacter() == '"') {
            advance();
            // Record element information.
            cont->elements++;
            cont->max_index = Max(cont->max_index, index);
            return JsonString(index);
          }

          if (CurrentCharacter() == '\\' && NextCharacter() == 'u') {
            if (TryAddArrayIndexChar(&index, ScanUnicodeCharacter())) continue;
          }

          break;
        }
      }
    }
    // Reset cursor_ to start if the key is not an index.
    cursor_ = start;
  }
  return ScanJsonString(true);
}

namespace {
Handle<Map> ParentOfDescriptorOwner(Isolate* isolate, Handle<Map> maybe_root,
                                    Handle<Map> source, int descriptor) {
  if (descriptor == 0) {
    DCHECK_EQ(0, maybe_root->NumberOfOwnDescriptors());
    return maybe_root;
  }
  return handle(source->FindFieldOwner(isolate, InternalIndex(descriptor - 1)),
                isolate);
}
}  // namespace

template <typename Char>
Handle<Object> JsonParser<Char>::BuildJsonObject(
    const JsonContinuation& cont,
    const std::vector<JsonProperty>& property_stack, Handle<Map> feedback) {
  size_t start = cont.index;
  int length = static_cast<int>(property_stack.size() - start);
  int named_length = length - cont.elements;

  Handle<Map> initial_map = factory()->ObjectLiteralMapFromCache(
      isolate_->native_context(), named_length);

  Handle<Map> map = initial_map;

  Handle<FixedArrayBase> elements = factory()->empty_fixed_array();

  // First store the elements.
  if (cont.elements > 0) {
    // Store as dictionary elements if that would use less memory.
    if (ShouldConvertToSlowElements(cont.elements, cont.max_index + 1)) {
      Handle<NumberDictionary> elms =
          NumberDictionary::New(isolate_, cont.elements);
      for (int i = 0; i < length; i++) {
        const JsonProperty& property = property_stack[start + i];
        if (!property.string.is_index()) continue;
        uint32_t index = property.string.index();
        Handle<Object> value = property.value;
        elms = NumberDictionary::Set(isolate_, elms, index, value);
      }
      map = Map::AsElementsKind(isolate_, map, DICTIONARY_ELEMENTS);
      elements = elms;
    } else {
      Handle<FixedArray> elms =
          factory()->NewFixedArrayWithHoles(cont.max_index + 1);
      DisallowHeapAllocation no_gc;
      WriteBarrierMode mode = elms->GetWriteBarrierMode(no_gc);
      DCHECK_EQ(HOLEY_ELEMENTS, map->elements_kind());

      for (int i = 0; i < length; i++) {
        const JsonProperty& property = property_stack[start + i];
        if (!property.string.is_index()) continue;
        uint32_t index = property.string.index();
        Handle<Object> value = property.value;
        elms->set(static_cast<int>(index), *value, mode);
      }
      elements = elms;
    }
  }

  int feedback_descriptors =
      (feedback.is_null() ||
       feedback->elements_kind() != map->elements_kind() ||
       feedback->instance_size() != map->instance_size())
          ? 0
          : feedback->NumberOfOwnDescriptors();

  int i;
  int descriptor = 0;
  int new_mutable_double = 0;
  for (i = 0; i < length; i++) {
    const JsonProperty& property = property_stack[start + i];
    if (property.string.is_index()) continue;
    Handle<String> expected;
    Handle<Map> target;
    InternalIndex descriptor_index(descriptor);
    if (descriptor < feedback_descriptors) {
      expected = handle(String::cast(feedback->instance_descriptors().GetKey(
                            descriptor_index)),
                        isolate_);
    } else {
      DisallowHeapAllocation no_gc;
      TransitionsAccessor transitions(isolate(), *map, &no_gc);
      expected = transitions.ExpectedTransitionKey();
      if (!expected.is_null()) {
        // Directly read out the target while reading out the key, otherwise it
        // might die while building the string below.
        target = TransitionsAccessor(isolate(), *map, &no_gc)
                     .ExpectedTransitionTarget();
      }
    }

    Handle<String> key = MakeString(property.string, expected);
    if (key.is_identical_to(expected)) {
      if (descriptor < feedback_descriptors) target = feedback;
    } else {
      if (descriptor < feedback_descriptors) {
        map = ParentOfDescriptorOwner(isolate_, map, feedback, descriptor);
        feedback_descriptors = 0;
      }
      if (!TransitionsAccessor(isolate(), map)
               .FindTransitionToField(key)
               .ToHandle(&target)) {
        break;
      }
    }

    Handle<Object> value = property.value;

    PropertyDetails details =
        target->instance_descriptors().GetDetails(descriptor_index);
    Representation expected_representation = details.representation();

    if (!value->FitsRepresentation(expected_representation)) {
      Representation representation = value->OptimalRepresentation(isolate());
      representation = representation.generalize(expected_representation);
      if (!expected_representation.CanBeInPlaceChangedTo(representation)) {
        map = ParentOfDescriptorOwner(isolate_, map, target, descriptor);
        break;
      }
      Handle<FieldType> value_type =
          value->OptimalType(isolate(), representation);
      Map::GeneralizeField(isolate(), target, descriptor_index,
                           details.constness(), representation, value_type);
    } else if (expected_representation.IsHeapObject() &&
               !target->instance_descriptors()
                    .GetFieldType(descriptor_index)
                    .NowContains(value)) {
      Handle<FieldType> value_type =
          value->OptimalType(isolate(), expected_representation);
      Map::GeneralizeField(isolate(), target, descriptor_index,
                           details.constness(), expected_representation,
                           value_type);
    } else if (!FLAG_unbox_double_fields &&
               expected_representation.IsDouble() && value->IsSmi()) {
      new_mutable_double++;
    }

    DCHECK(target->instance_descriptors()
               .GetFieldType(descriptor_index)
               .NowContains(value));
    map = target;
    descriptor++;
  }

  // Fast path: Write all transitioned named properties.
  if (i == length && descriptor < feedback_descriptors) {
    map = ParentOfDescriptorOwner(isolate_, map, map, descriptor);
  }

  // Preallocate all mutable heap numbers so we don't need to allocate while
  // setting up the object. Otherwise verification of that object may fail.
  Handle<ByteArray> mutable_double_buffer;
  // Allocate enough space so we can double-align the payload.
  const int kMutableDoubleSize = sizeof(double) * 2;
  STATIC_ASSERT(HeapNumber::kSize <= kMutableDoubleSize);
  if (new_mutable_double > 0) {
    mutable_double_buffer =
        factory()->NewByteArray(kMutableDoubleSize * new_mutable_double);
  }

  Handle<JSObject> object = initial_map->is_dictionary_map()
                                ? factory()->NewSlowJSObjectFromMap(map)
                                : factory()->NewJSObjectFromMap(map);
  object->set_elements(*elements);

  {
    descriptor = 0;
    DisallowHeapAllocation no_gc;
    WriteBarrierMode mode = object->GetWriteBarrierMode(no_gc);
    Address mutable_double_address =
        mutable_double_buffer.is_null()
            ? 0
            : reinterpret_cast<Address>(
                  mutable_double_buffer->GetDataStartAddress());
    Address filler_address = mutable_double_address;
    if (kTaggedSize != kDoubleSize) {
      if (IsAligned(mutable_double_address, kDoubleAlignment)) {
        mutable_double_address += kTaggedSize;
      } else {
        filler_address += HeapNumber::kSize;
      }
    }
    for (int j = 0; j < i; j++) {
      const JsonProperty& property = property_stack[start + j];
      if (property.string.is_index()) continue;
      InternalIndex descriptor_index(descriptor);
      PropertyDetails details =
          map->instance_descriptors().GetDetails(descriptor_index);
      Object value = *property.value;
      FieldIndex index = FieldIndex::ForDescriptor(*map, descriptor_index);
      descriptor++;

      if (details.representation().IsDouble()) {
        if (object->IsUnboxedDoubleField(index)) {
          uint64_t bits;
          if (value.IsSmi()) {
            bits = bit_cast<uint64_t>(static_cast<double>(Smi::ToInt(value)));
          } else {
            DCHECK(value.IsHeapNumber());
            bits = HeapNumber::cast(value).value_as_bits();
          }
          object->RawFastDoublePropertyAsBitsAtPut(index, bits);
          continue;
        }

        if (value.IsSmi()) {
          if (kTaggedSize != kDoubleSize) {
            // Write alignment filler.
            HeapObject filler = HeapObject::FromAddress(filler_address);
            filler.set_map_after_allocation(
                *factory()->one_pointer_filler_map());
            filler_address += kMutableDoubleSize;
          }

          uint64_t bits =
              bit_cast<uint64_t>(static_cast<double>(Smi::ToInt(value)));
          // Allocate simple heapnumber with immortal map, with non-pointer
          // payload, so we can skip notifying object layout change.

          HeapObject hn = HeapObject::FromAddress(mutable_double_address);
          hn.set_map_after_allocation(*factory()->heap_number_map());
          HeapNumber::cast(hn).set_value_as_bits(bits);
          value = hn;
          mutable_double_address += kMutableDoubleSize;
        } else {
          DCHECK(value.IsHeapNumber());
          HeapObject::cast(value).synchronized_set_map(
              *factory()->heap_number_map());
        }
      }
      object->RawFastInobjectPropertyAtPut(index, value, mode);
    }
    // Make all mutable HeapNumbers alive.
    if (!mutable_double_buffer.is_null()) {
#ifdef DEBUG
      Address end =
          reinterpret_cast<Address>(mutable_double_buffer->GetDataEndAddress());
      if (kTaggedSize != kDoubleSize) {
        DCHECK_EQ(Min(filler_address, mutable_double_address), end);
        DCHECK_GE(filler_address, end);
        DCHECK_GE(mutable_double_address, end);
      } else {
        DCHECK_EQ(mutable_double_address, end);
      }
#endif
      mutable_double_buffer->set_length(0);
    }
  }

  // Slow path: define remaining named properties.
  for (; i < length; i++) {
    HandleScope scope(isolate_);
    const JsonProperty& property = property_stack[start + i];
    if (property.string.is_index()) continue;
    Handle<String> key = MakeString(property.string);
#ifdef DEBUG
    uint32_t index;
    DCHECK(!key->AsArrayIndex(&index));
#endif
    Handle<Object> value = property.value;
    LookupIterator it(isolate_, object, key, object, LookupIterator::OWN);
    JSObject::DefineOwnPropertyIgnoreAttributes(&it, value, NONE).Check();
  }

  return object;
}

template <typename Char>
Handle<Object> JsonParser<Char>::BuildJsonArray(
    const JsonContinuation& cont,
    const std::vector<Handle<Object>>& element_stack) {
  size_t start = cont.index;
  int length = static_cast<int>(element_stack.size() - start);

  ElementsKind kind = PACKED_SMI_ELEMENTS;
  for (size_t i = start; i < element_stack.size(); i++) {
    Object value = *element_stack[i];
    if (value.IsHeapObject()) {
      if (HeapObject::cast(value).IsHeapNumber()) {
        kind = PACKED_DOUBLE_ELEMENTS;
      } else {
        kind = PACKED_ELEMENTS;
        break;
      }
    }
  }

  Handle<JSArray> array = factory()->NewJSArray(kind, length, length);
  if (kind == PACKED_DOUBLE_ELEMENTS) {
    DisallowHeapAllocation no_gc;
    FixedDoubleArray elements = FixedDoubleArray::cast(array->elements());
    for (int i = 0; i < length; i++) {
      elements.set(i, element_stack[start + i]->Number());
    }
  } else {
    DisallowHeapAllocation no_gc;
    FixedArray elements = FixedArray::cast(array->elements());
    WriteBarrierMode mode = kind == PACKED_SMI_ELEMENTS
                                ? SKIP_WRITE_BARRIER
                                : elements.GetWriteBarrierMode(no_gc);
    for (int i = 0; i < length; i++) {
      elements.set(i, *element_stack[start + i], mode);
    }
  }
  return array;
}

// Parse any JSON value.
template <typename Char>
MaybeHandle<Object> JsonParser<Char>::ParseJsonValue() {
  std::vector<JsonContinuation> cont_stack;
  std::vector<JsonProperty> property_stack;
  std::vector<Handle<Object>> element_stack;

  cont_stack.reserve(16);
  property_stack.reserve(16);
  element_stack.reserve(16);

  JsonContinuation cont(isolate_, JsonContinuation::kReturn, 0);

  Handle<Object> value;
  while (true) {
    // Produce a json value.
    //
    // Iterate until a value is produced. Starting but not immediately finishing
    // objects and arrays will cause the loop to continue until a first member
    // is completed.
    while (true) {
      SkipWhitespace();
      // The switch is immediately followed by 'break' so we can use 'break' to
      // break out of the loop, and 'continue' to continue the loop.
      switch (peek()) {
        case JsonToken::STRING:
          Consume(JsonToken::STRING);
          value = MakeString(ScanJsonString(false));
          break;

        case JsonToken::NUMBER:
          value = ParseJsonNumber();
          break;

        case JsonToken::LBRACE: {
          Consume(JsonToken::LBRACE);
          if (Check(JsonToken::RBRACE)) {
            // TODO(verwaest): Directly use the map instead.
            value = factory()->NewJSObject(object_constructor_);
            break;
          }

          // Start parsing an object with properties.
          cont_stack.emplace_back(std::move(cont));
          cont = JsonContinuation(isolate_, JsonContinuation::kObjectProperty,
                                  property_stack.size());

          // Parse the property key.
          ExpectNext(JsonToken::STRING);
          property_stack.emplace_back(ScanJsonPropertyKey(&cont));

          ExpectNext(JsonToken::COLON);

          // Continue to start producing the first property value.
          continue;
        }

        case JsonToken::LBRACK:
          Consume(JsonToken::LBRACK);
          if (Check(JsonToken::RBRACK)) {
            value = factory()->NewJSArray(0, PACKED_SMI_ELEMENTS);
            break;
          }

          // Start parsing an array with elements.
          cont_stack.emplace_back(std::move(cont));
          cont = JsonContinuation(isolate_, JsonContinuation::kArrayElement,
                                  element_stack.size());

          // Continue to start producing the first array element.
          continue;

        case JsonToken::TRUE_LITERAL:
          ScanLiteral("true");
          value = factory()->true_value();
          break;

        case JsonToken::FALSE_LITERAL:
          ScanLiteral("false");
          value = factory()->false_value();
          break;

        case JsonToken::NULL_LITERAL:
          ScanLiteral("null");
          value = factory()->null_value();
          break;

        case JsonToken::COLON:
        case JsonToken::COMMA:
        case JsonToken::ILLEGAL:
        case JsonToken::RBRACE:
        case JsonToken::RBRACK:
        case JsonToken::EOS:
          ReportUnexpectedCharacter(CurrentCharacter());
          // Pop the continuation stack to correctly tear down handle scopes.
          while (!cont_stack.empty()) {
            cont = std::move(cont_stack.back());
            cont_stack.pop_back();
          }
          return MaybeHandle<Object>();

        case JsonToken::WHITESPACE:
          UNREACHABLE();
      }

      // Done producing a value, consume it.
      break;
    }

    // Consume a produced json value.
    //
    // Iterate as long as values are produced (arrays or object literals are
    // finished).
    while (true) {
      // The switch is immediately followed by 'break' so we can use 'break' to
      // break out of the loop, and 'continue' to continue the loop.
      switch (cont.type()) {
        case JsonContinuation::kReturn:
          return cont.scope.CloseAndEscape(value);

        case JsonContinuation::kObjectProperty: {
          // Store the previous property value into its property info.
          property_stack.back().value = value;

          if (V8_LIKELY(Check(JsonToken::COMMA))) {
            // Parse the property key.
            ExpectNext(JsonToken::STRING);

            property_stack.emplace_back(ScanJsonPropertyKey(&cont));
            ExpectNext(JsonToken::COLON);

            // Break to start producing the subsequent property value.
            break;
          }

          Handle<Map> feedback;
          if (cont_stack.size() > 0 &&
              cont_stack.back().type() == JsonContinuation::kArrayElement &&
              cont_stack.back().index < element_stack.size() &&
              element_stack.back()->IsJSObject()) {
            Map maybe_feedback = JSObject::cast(*element_stack.back()).map();
            // Don't consume feedback from objects with a map that's detached
            // from the transition tree.
            if (!maybe_feedback.GetBackPointer().IsUndefined(isolate_)) {
              feedback = handle(maybe_feedback, isolate_);
            }
          }
          value = BuildJsonObject(cont, property_stack, feedback);
          property_stack.resize(cont.index);
          Expect(JsonToken::RBRACE);

          // Return the object.
          value = cont.scope.CloseAndEscape(value);
          // Pop the continuation.
          cont = std::move(cont_stack.back());
          cont_stack.pop_back();
          // Consume to produced object.
          continue;
        }

        case JsonContinuation::kArrayElement: {
          // Store the previous element on the stack.
          element_stack.emplace_back(value);
          // Break to start producing the subsequent element value.
          if (V8_LIKELY(Check(JsonToken::COMMA))) break;

          value = BuildJsonArray(cont, element_stack);
          element_stack.resize(cont.index);
          Expect(JsonToken::RBRACK);

          // Return the array.
          value = cont.scope.CloseAndEscape(value);
          // Pop the continuation.
          cont = std::move(cont_stack.back());
          cont_stack.pop_back();
          // Consume the produced array.
          continue;
        }
      }

      // Done consuming a value. Produce next value.
      break;
    }
  }
}

template <typename Char>
void JsonParser<Char>::AdvanceToNonDecimal() {
  cursor_ =
      std::find_if(cursor_, end_, [](Char c) { return !IsDecimalDigit(c); });
}

template <typename Char>
Handle<Object> JsonParser<Char>::ParseJsonNumber() {
  double number;
  int sign = 1;

  {
    const Char* start = cursor_;
    DisallowHeapAllocation no_gc;

    uc32 c = *cursor_;
    if (c == '-') {
      sign = -1;
      c = NextCharacter();
    }

    if (c == '0') {
      // Prefix zero is only allowed if it's the only digit before
      // a decimal point or exponent.
      c = NextCharacter();
      if (base::IsInRange(c, 0,
                          static_cast<int32_t>(unibrow::Latin1::kMaxChar)) &&
          IsNumberPart(character_json_scan_flags[c])) {
        if (V8_UNLIKELY(IsDecimalDigit(c))) {
          AllowHeapAllocation allow_before_exception;
          ReportUnexpectedToken(JsonToken::NUMBER);
          return handle(Smi::FromInt(0), isolate_);
        }
      } else if (sign > 0) {
        return handle(Smi::FromInt(0), isolate_);
      }
    } else {
      const Char* smi_start = cursor_;
      AdvanceToNonDecimal();
      if (V8_UNLIKELY(smi_start == cursor_)) {
        AllowHeapAllocation allow_before_exception;
        ReportUnexpectedCharacter(CurrentCharacter());
        return handle(Smi::FromInt(0), isolate_);
      }
      uc32 c = CurrentCharacter();
      STATIC_ASSERT(Smi::IsValid(-999999999));
      STATIC_ASSERT(Smi::IsValid(999999999));
      const int kMaxSmiLength = 9;
      if ((cursor_ - smi_start) <= kMaxSmiLength &&
          (!base::IsInRange(c, 0,
                            static_cast<int32_t>(unibrow::Latin1::kMaxChar)) ||
           !IsNumberPart(character_json_scan_flags[c]))) {
        // Smi.
        int32_t i = 0;
        for (; smi_start != cursor_; smi_start++) {
          DCHECK(IsDecimalDigit(*smi_start));
          i = (i * 10) + ((*smi_start) - '0');
        }
        // TODO(verwaest): Cache?
        return handle(Smi::FromInt(i * sign), isolate_);
      }
    }

    if (CurrentCharacter() == '.') {
      uc32 c = NextCharacter();
      if (!IsDecimalDigit(c)) {
        AllowHeapAllocation allow_before_exception;
        ReportUnexpectedCharacter(c);
        return handle(Smi::FromInt(0), isolate_);
      }
      AdvanceToNonDecimal();
    }

    if (AsciiAlphaToLower(CurrentCharacter()) == 'e') {
      uc32 c = NextCharacter();
      if (c == '-' || c == '+') c = NextCharacter();
      if (!IsDecimalDigit(c)) {
        AllowHeapAllocation allow_before_exception;
        ReportUnexpectedCharacter(c);
        return handle(Smi::FromInt(0), isolate_);
      }
      AdvanceToNonDecimal();
    }

    Vector<const Char> chars(start, cursor_ - start);
    number = StringToDouble(chars,
                            NO_FLAGS,  // Hex, octal or trailing junk.
                            std::numeric_limits<double>::quiet_NaN());

    DCHECK(!std::isnan(number));
  }

  return factory()->NewNumber(number);
}

namespace {

template <typename Char>
bool Matches(const Vector<const Char>& chars, Handle<String> string) {
  DCHECK(!string.is_null());

  if (chars.length() != string->length()) return false;

  DisallowHeapAllocation no_gc;
  if (string->IsOneByteRepresentation()) {
    const uint8_t* string_data = string->GetChars<uint8_t>(no_gc);
    return CompareChars(chars.begin(), string_data, chars.length()) == 0;
  }
  const uint16_t* string_data = string->GetChars<uint16_t>(no_gc);
  return CompareChars(chars.begin(), string_data, chars.length()) == 0;
}

}  // namespace

template <typename Char>
template <typename SinkSeqString>
Handle<String> JsonParser<Char>::DecodeString(
    const JsonString& string, Handle<SinkSeqString> intermediate,
    Handle<String> hint) {
  using SinkChar = typename SinkSeqString::Char;
  {
    DisallowHeapAllocation no_gc;
    SinkChar* dest = intermediate->GetChars(no_gc);
    if (!string.has_escape()) {
      DCHECK(!string.internalize());
      CopyChars(dest, chars_ + string.start(), string.length());
      return intermediate;
    }
    DecodeString(dest, string.start(), string.length());

    if (!string.internalize()) return intermediate;

    Vector<const SinkChar> data(dest, string.length());
    if (!hint.is_null() && Matches(data, hint)) return hint;
  }

  return factory()->InternalizeString(intermediate, 0, string.length());
}

template <typename Char>
Handle<String> JsonParser<Char>::MakeString(const JsonString& string,
                                            Handle<String> hint) {
  if (string.length() == 0) return factory()->empty_string();

  if (string.internalize() && !string.has_escape()) {
    if (!hint.is_null()) {
      Vector<const Char> data(chars_ + string.start(), string.length());
      if (Matches(data, hint)) return hint;
    }
    if (chars_may_relocate_) {
      return factory()->InternalizeString(Handle<SeqString>::cast(source_),
                                          string.start(), string.length(),
                                          string.needs_conversion());
    }
    Vector<const Char> chars(chars_ + string.start(), string.length());
    return factory()->InternalizeString(chars, string.needs_conversion());
  }

  if (sizeof(Char) == 1 ? V8_LIKELY(!string.needs_conversion())
                        : string.needs_conversion()) {
    Handle<SeqOneByteString> intermediate =
        factory()->NewRawOneByteString(string.length()).ToHandleChecked();
    return DecodeString(string, intermediate, hint);
  }

  Handle<SeqTwoByteString> intermediate =
      factory()->NewRawTwoByteString(string.length()).ToHandleChecked();
  return DecodeString(string, intermediate, hint);
}

template <typename Char>
template <typename SinkChar>
void JsonParser<Char>::DecodeString(SinkChar* sink, int start, int length) {
  SinkChar* sink_start = sink;
  const Char* cursor = chars_ + start;
  while (true) {
    const Char* end = cursor + length - (sink - sink_start);
    cursor = std::find_if(cursor, end, [&sink](Char c) {
      if (c == '\\') return true;
      *sink++ = c;
      return false;
    });

    if (cursor == end) return;

    cursor++;

    switch (GetEscapeKind(character_json_scan_flags[*cursor])) {
      case EscapeKind::kSelf:
        *sink++ = *cursor;
        break;

      case EscapeKind::kBackspace:
        *sink++ = '\x08';
        break;

      case EscapeKind::kTab:
        *sink++ = '\x09';
        break;

      case EscapeKind::kNewLine:
        *sink++ = '\x0A';
        break;

      case EscapeKind::kFormFeed:
        *sink++ = '\x0C';
        break;

      case EscapeKind::kCarriageReturn:
        *sink++ = '\x0D';
        break;

      case EscapeKind::kUnicode: {
        uc32 value = 0;
        for (int i = 0; i < 4; i++) {
          value = value * 16 + HexValue(*++cursor);
        }
        if (value <=
            static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
          *sink++ = value;
        } else {
          *sink++ = unibrow::Utf16::LeadSurrogate(value);
          *sink++ = unibrow::Utf16::TrailSurrogate(value);
        }
        break;
      }

      case EscapeKind::kIllegal:
        UNREACHABLE();
    }
    cursor++;
  }
}

template <typename Char>
JsonString JsonParser<Char>::ScanJsonString(bool needs_internalization) {
  DisallowHeapAllocation no_gc;
  int start = position();
  int offset = start;
  bool has_escape = false;
  uc32 bits = 0;

  while (true) {
    cursor_ = std::find_if(cursor_, end_, [&bits](Char c) {
      if (sizeof(Char) == 2 && V8_UNLIKELY(c > unibrow::Latin1::kMaxChar)) {
        bits |= c;
        return false;
      }
      return MayTerminateJsonString(character_json_scan_flags[c]);
    });

    if (V8_UNLIKELY(is_at_end())) {
      AllowHeapAllocation allow_before_exception;
      ReportUnexpectedCharacter(kEndOfString);
      break;
    }

    if (*cursor_ == '"') {
      int end = position();
      advance();
      int length = end - offset;
      bool convert = sizeof(Char) == 1 ? bits > unibrow::Latin1::kMaxChar
                                       : bits <= unibrow::Latin1::kMaxChar;
      return JsonString(start, length, convert, needs_internalization,
                        has_escape);
    }

    if (*cursor_ == '\\') {
      has_escape = true;
      uc32 c = NextCharacter();
      if (V8_UNLIKELY(!base::IsInRange(
              c, 0, static_cast<int32_t>(unibrow::Latin1::kMaxChar)))) {
        AllowHeapAllocation allow_before_exception;
        ReportUnexpectedCharacter(c);
        break;
      }

      switch (GetEscapeKind(character_json_scan_flags[c])) {
        case EscapeKind::kSelf:
        case EscapeKind::kBackspace:
        case EscapeKind::kTab:
        case EscapeKind::kNewLine:
        case EscapeKind::kFormFeed:
        case EscapeKind::kCarriageReturn:
          offset += 1;
          break;

        case EscapeKind::kUnicode: {
          uc32 value = ScanUnicodeCharacter();
          if (value == -1) {
            AllowHeapAllocation allow_before_exception;
            ReportUnexpectedCharacter(CurrentCharacter());
            return JsonString();
          }
          bits |= value;
          // \uXXXX results in either 1 or 2 Utf16 characters, depending on
          // whether the decoded value requires a surrogate pair.
          offset += 5 - (value > static_cast<uc32>(
                                     unibrow::Utf16::kMaxNonSurrogateCharCode));
          break;
        }

        case EscapeKind::kIllegal:
          AllowHeapAllocation allow_before_exception;
          ReportUnexpectedCharacter(c);
          return JsonString();
      }

      advance();
      continue;
    }

    DCHECK_LT(*cursor_, 0x20);
    AllowHeapAllocation allow_before_exception;
    ReportUnexpectedCharacter(*cursor_);
    break;
  }

  return JsonString();
}

// Explicit instantiation.
template class JsonParser<uint8_t>;
template class JsonParser<uint16_t>;

}  // namespace internal
}  // namespace v8
