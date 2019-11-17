// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>

#include "debug-helper-internal.h"
#include "heap-constants.h"
#include "include/v8-internal.h"
#include "src/common/ptr-compr-inl.h"
#include "src/objects/string-inl.h"
#include "src/strings/unicode-inl.h"
#include "torque-generated/class-debug-readers-tq.h"

namespace i = v8::internal;

namespace v8_debug_helper_internal {

// INSTANCE_TYPE_CHECKERS_SINGLE_BASE, trimmed down to only classes that have
// layouts defined in .tq files (this subset relationship is asserted below).
// For now, this is a hand-maintained list.
// TODO(v8:7793): Torque should know enough about instance types to generate
// this list.
#define TQ_INSTANCE_TYPES_SINGLE_BASE(V)                       \
  V(ByteArray, BYTE_ARRAY_TYPE)                                \
  V(BytecodeArray, BYTECODE_ARRAY_TYPE)                        \
  V(CallHandlerInfo, CALL_HANDLER_INFO_TYPE)                   \
  V(Cell, CELL_TYPE)                                           \
  V(DescriptorArray, DESCRIPTOR_ARRAY_TYPE)                    \
  V(EmbedderDataArray, EMBEDDER_DATA_ARRAY_TYPE)               \
  V(FeedbackCell, FEEDBACK_CELL_TYPE)                          \
  V(FeedbackVector, FEEDBACK_VECTOR_TYPE)                      \
  V(FixedDoubleArray, FIXED_DOUBLE_ARRAY_TYPE)                 \
  V(Foreign, FOREIGN_TYPE)                                     \
  V(FreeSpace, FREE_SPACE_TYPE)                                \
  V(HeapNumber, HEAP_NUMBER_TYPE)                              \
  V(JSArgumentsObject, JS_ARGUMENTS_OBJECT_TYPE)               \
  V(JSArray, JS_ARRAY_TYPE)                                    \
  V(JSArrayBuffer, JS_ARRAY_BUFFER_TYPE)                       \
  V(JSArrayIterator, JS_ARRAY_ITERATOR_TYPE)                   \
  V(JSAsyncFromSyncIterator, JS_ASYNC_FROM_SYNC_ITERATOR_TYPE) \
  V(JSAsyncFunctionObject, JS_ASYNC_FUNCTION_OBJECT_TYPE)      \
  V(JSAsyncGeneratorObject, JS_ASYNC_GENERATOR_OBJECT_TYPE)    \
  V(JSBoundFunction, JS_BOUND_FUNCTION_TYPE)                   \
  V(JSDataView, JS_DATA_VIEW_TYPE)                             \
  V(JSDate, JS_DATE_TYPE)                                      \
  V(JSFunction, JS_FUNCTION_TYPE)                              \
  V(JSGlobalObject, JS_GLOBAL_OBJECT_TYPE)                     \
  V(JSGlobalProxy, JS_GLOBAL_PROXY_TYPE)                       \
  V(JSMap, JS_MAP_TYPE)                                        \
  V(JSMessageObject, JS_MESSAGE_OBJECT_TYPE)                   \
  V(JSModuleNamespace, JS_MODULE_NAMESPACE_TYPE)               \
  V(JSPromise, JS_PROMISE_TYPE)                                \
  V(JSProxy, JS_PROXY_TYPE)                                    \
  V(JSRegExp, JS_REG_EXP_TYPE)                                 \
  V(JSRegExpStringIterator, JS_REG_EXP_STRING_ITERATOR_TYPE)   \
  V(JSSet, JS_SET_TYPE)                                        \
  V(JSStringIterator, JS_STRING_ITERATOR_TYPE)                 \
  V(JSTypedArray, JS_TYPED_ARRAY_TYPE)                         \
  V(JSPrimitiveWrapper, JS_PRIMITIVE_WRAPPER_TYPE)             \
  V(JSFinalizationGroup, JS_FINALIZATION_GROUP_TYPE)           \
  V(JSFinalizationGroupCleanupIterator,                        \
    JS_FINALIZATION_GROUP_CLEANUP_ITERATOR_TYPE)               \
  V(JSWeakMap, JS_WEAK_MAP_TYPE)                               \
  V(JSWeakRef, JS_WEAK_REF_TYPE)                               \
  V(JSWeakSet, JS_WEAK_SET_TYPE)                               \
  V(Map, MAP_TYPE)                                             \
  V(Oddball, ODDBALL_TYPE)                                     \
  V(PreparseData, PREPARSE_DATA_TYPE)                          \
  V(PropertyArray, PROPERTY_ARRAY_TYPE)                        \
  V(PropertyCell, PROPERTY_CELL_TYPE)                          \
  V(SharedFunctionInfo, SHARED_FUNCTION_INFO_TYPE)             \
  V(Symbol, SYMBOL_TYPE)                                       \
  V(WasmExceptionObject, WASM_EXCEPTION_OBJECT_TYPE)           \
  V(WasmGlobalObject, WASM_GLOBAL_OBJECT_TYPE)                 \
  V(WasmMemoryObject, WASM_MEMORY_OBJECT_TYPE)                 \
  V(WasmModuleObject, WASM_MODULE_OBJECT_TYPE)                 \
  V(WasmTableObject, WASM_TABLE_OBJECT_TYPE)                   \
  V(WeakArrayList, WEAK_ARRAY_LIST_TYPE)                       \
  V(WeakCell, WEAK_CELL_TYPE)
#ifdef V8_INTL_SUPPORT

#define TQ_INSTANCE_TYPES_SINGLE_NOSTRUCTS(V)           \
  TQ_INSTANCE_TYPES_SINGLE_BASE(V)                      \
  V(JSV8BreakIterator, JS_V8_BREAK_ITERATOR_TYPE)       \
  V(JSCollator, JS_COLLATOR_TYPE)                       \
  V(JSDateTimeFormat, JS_DATE_TIME_FORMAT_TYPE)         \
  V(JSListFormat, JS_LIST_FORMAT_TYPE)                  \
  V(JSLocale, JS_LOCALE_TYPE)                           \
  V(JSNumberFormat, JS_NUMBER_FORMAT_TYPE)              \
  V(JSPluralRules, JS_PLURAL_RULES_TYPE)                \
  V(JSRelativeTimeFormat, JS_RELATIVE_TIME_FORMAT_TYPE) \
  V(JSSegmentIterator, JS_SEGMENT_ITERATOR_TYPE)        \
  V(JSSegmenter, JS_SEGMENTER_TYPE)

#else

#define TQ_INSTANCE_TYPES_SINGLE_NOSTRUCTS(V) TQ_INSTANCE_TYPES_SINGLE_BASE(V)

#endif  // V8_INTL_SUPPORT

// Used in the static assertion below.
enum class InstanceTypeCheckersSingle {
#define ENUM_VALUE(ClassName, INSTANCE_TYPE) k##ClassName = i::INSTANCE_TYPE,
  INSTANCE_TYPE_CHECKERS_SINGLE(ENUM_VALUE)
#undef ENUM_VALUE
};

// Verify that the instance type list above stays in sync with the truth.
#define CHECK_VALUE(ClassName, INSTANCE_TYPE)                            \
  static_assert(                                                         \
      static_cast<i::InstanceType>(                                      \
          InstanceTypeCheckersSingle::k##ClassName) == i::INSTANCE_TYPE, \
      "TQ_INSTANCE_TYPES_SINGLE_NOSTRUCTS must be subset of "            \
      "INSTANCE_TYPE_CHECKERS_SINGLE. Invalid class: " #ClassName);
TQ_INSTANCE_TYPES_SINGLE_NOSTRUCTS(CHECK_VALUE)
#undef CHECK_VALUE

// Adapts one STRUCT_LIST_GENERATOR entry to (Name, NAME) format.
#define STRUCT_INSTANCE_TYPE_ADAPTER(V, NAME, Name, name) V(Name, NAME)

// Pairs of (ClassName, CLASS_NAME_TYPE) for every instance type that
// corresponds to a single Torque-defined class. Note that all Struct-derived
// classes are defined in Torque.
#define TQ_INSTANCE_TYPES_SINGLE(V)     \
  TQ_INSTANCE_TYPES_SINGLE_NOSTRUCTS(V) \
  STRUCT_LIST_GENERATOR(STRUCT_INSTANCE_TYPE_ADAPTER, V)

// Likewise, these are the subset of INSTANCE_TYPE_CHECKERS_RANGE that have
// definitions in .tq files, rearranged with more specific things first. Also
// includes JSObject and JSReceiver, which in the runtime are optimized to use
// a one-sided check.
#define TQ_INSTANCE_TYPES_RANGE(V)                                           \
  V(Context, FIRST_CONTEXT_TYPE, LAST_CONTEXT_TYPE)                          \
  V(FixedArray, FIRST_FIXED_ARRAY_TYPE, LAST_FIXED_ARRAY_TYPE)               \
  V(Microtask, FIRST_MICROTASK_TYPE, LAST_MICROTASK_TYPE)                    \
  V(String, FIRST_STRING_TYPE, LAST_STRING_TYPE)                             \
  V(Name, FIRST_NAME_TYPE, LAST_NAME_TYPE)                                   \
  V(WeakFixedArray, FIRST_WEAK_FIXED_ARRAY_TYPE, LAST_WEAK_FIXED_ARRAY_TYPE) \
  V(JSObject, FIRST_JS_OBJECT_TYPE, LAST_JS_OBJECT_TYPE)                     \
  V(JSReceiver, FIRST_JS_RECEIVER_TYPE, LAST_JS_RECEIVER_TYPE)

std::string AppendAddressAndType(const std::string& brief, uintptr_t address,
                                 const char* type) {
  std::stringstream brief_stream;
  brief_stream << "0x" << std::hex << address << " <" << type << ">";
  return brief.empty() ? brief_stream.str()
                       : brief + " (" + brief_stream.str() + ")";
}

struct TypedObject {
  TypedObject(d::TypeCheckResult type_check_result,
              std::unique_ptr<TqObject> object)
      : type_check_result(type_check_result), object(std::move(object)) {}

  // How we discovered the object's type, or why we failed to do so.
  d::TypeCheckResult type_check_result;

  // Pointer to some TqObject subclass, representing the most specific known
  // type for the object.
  std::unique_ptr<TqObject> object;

  // Collection of other guesses at more specific types than the one represented
  // by |object|.
  std::vector<TypedObject> possible_types;
};

TypedObject GetTypedObjectByHint(uintptr_t address,
                                 std::string type_hint_string) {
#define TYPE_NAME_CASE(ClassName, ...)                   \
  if (type_hint_string == "v8::internal::" #ClassName) { \
    return {d::TypeCheckResult::kUsedTypeHint,           \
            std::make_unique<Tq##ClassName>(address)};   \
  }

  TQ_INSTANCE_TYPES_SINGLE(TYPE_NAME_CASE)
  TQ_INSTANCE_TYPES_RANGE(TYPE_NAME_CASE)
  STRING_CLASS_TYPES(TYPE_NAME_CASE)

#undef TYPE_NAME_CASE

  return {d::TypeCheckResult::kUnknownTypeHint,
          std::make_unique<TqHeapObject>(address)};
}

TypedObject GetTypedObjectForString(uintptr_t address, i::InstanceType type,
                                    d::TypeCheckResult type_source) {
  class StringGetDispatcher : public i::AllStatic {
   public:
#define DEFINE_METHOD(ClassName)                                    \
  static inline TypedObject Handle##ClassName(                      \
      uintptr_t address, d::TypeCheckResult type_source) {          \
    return {type_source, std::make_unique<Tq##ClassName>(address)}; \
  }
    STRING_CLASS_TYPES(DEFINE_METHOD)
#undef DEFINE_METHOD
    static inline TypedObject HandleInvalidString(
        uintptr_t address, d::TypeCheckResult type_source) {
      return {d::TypeCheckResult::kUnknownInstanceType,
              std::make_unique<TqString>(address)};
    }
  };

  return i::StringShape(type)
      .DispatchToSpecificTypeWithoutCast<StringGetDispatcher, TypedObject>(
          address, type_source);
}

TypedObject GetTypedObjectByInstanceType(uintptr_t address,
                                         i::InstanceType type,
                                         d::TypeCheckResult type_source) {
  switch (type) {
#define INSTANCE_TYPE_CASE(ClassName, INSTANCE_TYPE) \
  case i::INSTANCE_TYPE:                             \
    return {type_source, std::make_unique<Tq##ClassName>(address)};
    TQ_INSTANCE_TYPES_SINGLE(INSTANCE_TYPE_CASE)
#undef INSTANCE_TYPE_CASE

    default:

      // Special case: concrete subtypes of String are not included in the
      // main instance type list because they use the low bits of the instance
      // type enum as flags.
      if (type <= i::LAST_STRING_TYPE) {
        return GetTypedObjectForString(address, type, type_source);
      }

#define INSTANCE_RANGE_CASE(ClassName, FIRST_TYPE, LAST_TYPE)       \
  if (type >= i::FIRST_TYPE && type <= i::LAST_TYPE) {              \
    return {type_source, std::make_unique<Tq##ClassName>(address)}; \
  }
      TQ_INSTANCE_TYPES_RANGE(INSTANCE_RANGE_CASE)
#undef INSTANCE_RANGE_CASE

      return {d::TypeCheckResult::kUnknownInstanceType,
              std::make_unique<TqHeapObject>(address)};
  }
}

TypedObject GetTypedHeapObject(uintptr_t address, d::MemoryAccessor accessor,
                               const char* type_hint,
                               const d::HeapAddresses& heap_addresses) {
  auto heap_object = std::make_unique<TqHeapObject>(address);
  Value<uintptr_t> map_ptr = heap_object->GetMapValue(accessor);

  if (map_ptr.validity != d::MemoryAccessResult::kOk) {
    // If we can't read the Map pointer from the object, then we likely can't
    // read anything else, so there's not any point in attempting to use the
    // type hint. Just return a failure.
    return {map_ptr.validity == d::MemoryAccessResult::kAddressNotValid
                ? d::TypeCheckResult::kObjectPointerInvalid
                : d::TypeCheckResult::kObjectPointerValidButInaccessible,
            std::move(heap_object)};
  }

  Value<i::InstanceType> type =
      TqMap(map_ptr.value).GetInstanceTypeValue(accessor);
  if (type.validity == d::MemoryAccessResult::kOk) {
    return GetTypedObjectByInstanceType(address, type.value,
                                        d::TypeCheckResult::kUsedMap);
  }

  // We can't read the Map, so check whether it is in the list of known Maps,
  // as another way to get its instance type.
  KnownInstanceType known_map_type =
      FindKnownMapInstanceType(map_ptr.value, heap_addresses);
  if (known_map_type.confidence == KnownInstanceType::Confidence::kHigh) {
    DCHECK_EQ(known_map_type.types.size(), 1);
    return GetTypedObjectByInstanceType(address, known_map_type.types[0],
                                        d::TypeCheckResult::kKnownMapPointer);
  }

  // Create a basic result that says that the object is a HeapObject and we
  // couldn't read its Map.
  TypedObject result = {
      type.validity == d::MemoryAccessResult::kAddressNotValid
          ? d::TypeCheckResult::kMapPointerInvalid
          : d::TypeCheckResult::kMapPointerValidButInaccessible,
      std::move(heap_object)};

  // If a type hint is available, it may give us something more specific than
  // HeapObject. However, a type hint of Object would be even less specific, so
  // we'll only use the type hint if it's a subclass of HeapObject.
  if (type_hint != nullptr) {
    TypedObject hint_result = GetTypedObjectByHint(address, type_hint);
    if (result.object->IsSuperclassOf(hint_result.object.get())) {
      result = std::move(hint_result);
    }
  }

  // If low-confidence results are available from known Maps, include them only
  // if they don't contradict the primary type and would provide some additional
  // specificity.
  for (const i::InstanceType type_guess : known_map_type.types) {
    TypedObject guess_result = GetTypedObjectByInstanceType(
        address, type_guess, d::TypeCheckResult::kKnownMapPointer);
    if (result.object->IsSuperclassOf(guess_result.object.get())) {
      result.possible_types.push_back(std::move(guess_result));
    }
  }

  return result;
}

#undef STRUCT_INSTANCE_TYPE_ADAPTER
#undef TQ_INSTANCE_TYPES_SINGLE_BASE
#undef TQ_INSTANCE_TYPES_SINGLE
#undef TQ_INSTANCE_TYPES_SINGLE_NOSTRUCTS
#undef TQ_INSTANCE_TYPES_RANGE

// An object visitor that accumulates the first few characters of a string.
class ReadStringVisitor : public TqObjectVisitor {
 public:
  ReadStringVisitor(d::MemoryAccessor accessor,
                    const d::HeapAddresses& heap_addresses)
      : accessor_(accessor),
        heap_addresses_(heap_addresses),
        index_(0),
        limit_(INT32_MAX),
        done_(false) {}

  // Returns the result as UTF-8 once visiting is complete.
  std::string GetString() {
    std::vector<char> result(
        string_.size() * unibrow::Utf16::kMaxExtraUtf8BytesForOneUtf16CodeUnit);
    unsigned write_index = 0;
    int prev_char = unibrow::Utf16::kNoPreviousCharacter;
    for (size_t read_index = 0; read_index < string_.size(); ++read_index) {
      uint16_t character = string_[read_index];
      write_index +=
          unibrow::Utf8::Encode(result.data() + write_index, character,
                                prev_char, /*replace_invalid=*/true);
      prev_char = character;
    }
    return {result.data(), write_index};
  }

  template <typename T>
  void ReadSeqString(const T* object) {
    int32_t length = GetOrFinish(object->GetLengthValue(accessor_));
    for (; index_ < length && index_ < limit_ && !done_; ++index_) {
      char16_t c = static_cast<char16_t>(
          GetOrFinish(object->GetCharsValue(accessor_, index_)));
      if (!done_) AddCharacter(c);
    }
  }

  void VisitSeqOneByteString(const TqSeqOneByteString* object) override {
    ReadSeqString(object);
  }

  void VisitSeqTwoByteString(const TqSeqTwoByteString* object) override {
    ReadSeqString(object);
  }

  void VisitConsString(const TqConsString* object) override {
    uintptr_t first_address = GetOrFinish(object->GetFirstValue(accessor_));
    if (done_) return;
    auto first =
        GetTypedHeapObject(first_address, accessor_, nullptr, heap_addresses_)
            .object;
    first->Visit(this);
    if (done_) return;
    int32_t first_length = GetOrFinish(
        static_cast<TqString*>(first.get())->GetLengthValue(accessor_));
    uintptr_t second = GetOrFinish(object->GetSecondValue(accessor_));
    if (done_) return;
    IndexModifier modifier(this, -first_length, -first_length);
    GetTypedHeapObject(second, accessor_, nullptr, heap_addresses_)
        .object->Visit(this);
  }

  void VisitSlicedString(const TqSlicedString* object) override {
    uintptr_t parent = GetOrFinish(object->GetParentValue(accessor_));
    int32_t length = GetOrFinish(object->GetLengthValue(accessor_));
    int32_t offset = i::PlatformSmiTagging::SmiToInt(
        GetOrFinish(object->GetOffsetValue(accessor_)));
    if (done_) return;
    int32_t limit_adjust = offset + length - limit_;
    IndexModifier modifier(this, offset, limit_adjust < 0 ? limit_adjust : 0);
    GetTypedHeapObject(parent, accessor_, nullptr, heap_addresses_)
        .object->Visit(this);
  }

  void VisitThinString(const TqThinString* object) override {
    uintptr_t actual = GetOrFinish(object->GetActualValue(accessor_));
    if (done_) return;
    GetTypedHeapObject(actual, accessor_, nullptr, heap_addresses_)
        .object->Visit(this);
  }

  void VisitExternalString(const TqExternalString* object) override {
    // TODO(v8:9376): External strings are very common and important when
    // attempting to print the source of a function in the browser. For now
    // we're just ignoring them, but eventually we'll want some kind of
    // mechanism where the user of this library can provide a callback function
    // that fetches data from external strings.
    AddEllipsisAndFinish();
  }

  void VisitObject(const TqObject* object) override {
    // If we fail to find a specific type for a sub-object within a cons string,
    // sliced string, or thin string, we will end up here.
    AddEllipsisAndFinish();
  }

 private:
  // Unpacks a value that was fetched from the debuggee. If the value indicates
  // that it couldn't successfully fetch memory, then prevents further work.
  template <typename T>
  T GetOrFinish(Value<T> value) {
    if (value.validity != d::MemoryAccessResult::kOk) {
      AddEllipsisAndFinish();
    }
    return value.value;
  }

  void AddEllipsisAndFinish() {
    if (!done_) {
      string_ += u"...";
      done_ = true;
    }
  }

  void AddCharacter(char16_t c) {
    if (string_.size() >= kMaxCharacters) {
      AddEllipsisAndFinish();
    } else {
      string_.push_back(c);
    }
  }

  // Temporarily adds offsets to both index_ and limit_, to handle ConsString
  // and SlicedString.
  class IndexModifier {
   public:
    IndexModifier(ReadStringVisitor* that, int32_t index_adjust,
                  int32_t limit_adjust)
        : that_(that),
          index_adjust_(index_adjust),
          limit_adjust_(limit_adjust) {
      that_->index_ += index_adjust_;
      that_->limit_ += limit_adjust_;
    }
    ~IndexModifier() {
      that_->index_ -= index_adjust_;
      that_->limit_ -= limit_adjust_;
    }

   private:
    ReadStringVisitor* that_;
    int32_t index_adjust_;
    int32_t limit_adjust_;
    DISALLOW_COPY_AND_ASSIGN(IndexModifier);
  };

  static constexpr int kMaxCharacters = 80;  // How many characters to print.

  std::u16string string_;  // Result string.
  d::MemoryAccessor accessor_;
  const d::HeapAddresses& heap_addresses_;
  int32_t index_;  // Index of next char to read.
  int32_t limit_;  // Don't read past this index (set by SlicedString).
  bool done_;      // Whether to stop further work.
};

// An object visitor that adds extra debugging information for some types.
class AddInfoVisitor : public TqObjectVisitor {
 public:
  AddInfoVisitor(const std::string& brief, d::MemoryAccessor accessor,
                 const d::HeapAddresses& heap_addresses)
      : accessor_(accessor), brief_(brief), heap_addresses_(heap_addresses) {}

  // Returns the brief object description, once visiting is complete.
  const std::string& GetBrief() { return brief_; }

  void VisitString(const TqString* object) override {
    ReadStringVisitor visitor(accessor_, heap_addresses_);
    object->Visit(&visitor);
    if (!brief_.empty()) brief_ += " ";
    brief_ += "\"" + visitor.GetString() + "\"";
  }

 private:
  d::MemoryAccessor accessor_;
  std::string brief_;
  const d::HeapAddresses& heap_addresses_;
};

std::unique_ptr<ObjectPropertiesResult> GetHeapObjectPropertiesNotCompressed(
    uintptr_t address, d::MemoryAccessor accessor, const char* type_hint,
    const d::HeapAddresses& heap_addresses) {
  // Regardless of whether we can read the object itself, maybe we can find its
  // pointer in the list of known objects.
  std::string brief = FindKnownObject(address, heap_addresses);

  TypedObject typed =
      GetTypedHeapObject(address, accessor, type_hint, heap_addresses);

  // TODO(v8:9376): Many object types need additional data that is not included
  // in their Torque layout definitions. For example, JSObject has an array of
  // in-object properties after its Torque-defined fields, which at a minimum
  // should be represented as an array in this response. If the relevant memory
  // is available, we should instead represent those properties (and any out-of-
  // object properties) using their JavaScript property names.
  AddInfoVisitor visitor(brief, accessor, heap_addresses);
  typed.object->Visit(&visitor);
  brief = visitor.GetBrief();

  brief = AppendAddressAndType(brief, address, typed.object->GetName());

  // Convert the low-confidence guessed types to a list of strings as expected
  // for the response.
  std::vector<std::string> guessed_types;
  for (const auto& guess : typed.possible_types) {
    guessed_types.push_back(guess.object->GetName());
  }

  return std::make_unique<ObjectPropertiesResult>(
      typed.type_check_result, brief, typed.object->GetName(),
      typed.object->GetProperties(accessor), std::move(guessed_types));
}

std::unique_ptr<ObjectPropertiesResult> GetHeapObjectPropertiesMaybeCompressed(
    uintptr_t address, d::MemoryAccessor memory_accessor,
    d::HeapAddresses heap_addresses, const char* type_hint) {
  // Try to figure out the heap range, for pointer compression (this is unused
  // if pointer compression is disabled).
  uintptr_t any_uncompressed_ptr = 0;
  if (!IsPointerCompressed(address)) any_uncompressed_ptr = address;
  if (any_uncompressed_ptr == 0)
    any_uncompressed_ptr = heap_addresses.any_heap_pointer;
  if (any_uncompressed_ptr == 0)
    any_uncompressed_ptr = heap_addresses.map_space_first_page;
  if (any_uncompressed_ptr == 0)
    any_uncompressed_ptr = heap_addresses.old_space_first_page;
  if (any_uncompressed_ptr == 0)
    any_uncompressed_ptr = heap_addresses.read_only_space_first_page;
  FillInUnknownHeapAddresses(&heap_addresses, any_uncompressed_ptr);
  if (any_uncompressed_ptr == 0) {
    // We can't figure out the heap range. Just check for known objects.
    std::string brief = FindKnownObject(address, heap_addresses);
    brief = AppendAddressAndType(brief, address, "v8::internal::TaggedValue");
    return std::make_unique<ObjectPropertiesResult>(
        d::TypeCheckResult::kUnableToDecompress, brief,
        "v8::internal::TaggedValue");
  }

  address = EnsureDecompressed(address, any_uncompressed_ptr);

  return GetHeapObjectPropertiesNotCompressed(address, memory_accessor,
                                              type_hint, heap_addresses);
}

std::unique_ptr<ObjectPropertiesResult> GetObjectProperties(
    uintptr_t address, d::MemoryAccessor memory_accessor,
    const d::HeapAddresses& heap_addresses, const char* type_hint) {
  if (static_cast<uint32_t>(address) == i::kClearedWeakHeapObjectLower32) {
    return std::make_unique<ObjectPropertiesResult>(
        d::TypeCheckResult::kWeakRef, "cleared weak ref",
        "v8::internal::HeapObject");
  }
  bool is_weak = (address & i::kHeapObjectTagMask) == i::kWeakHeapObjectTag;
  if (is_weak) {
    address &= ~i::kWeakHeapObjectMask;
  }
  if (i::Internals::HasHeapObjectTag(address)) {
    std::unique_ptr<ObjectPropertiesResult> result =
        GetHeapObjectPropertiesMaybeCompressed(address, memory_accessor,
                                               heap_addresses, type_hint);
    if (is_weak) {
      result->Prepend("weak ref to ");
    }
    return result;
  }

  // For smi values, construct a response with a description representing the
  // untagged value.
  int32_t value = i::PlatformSmiTagging::SmiToInt(address);
  std::stringstream stream;
  stream << value << " (0x" << std::hex << value << ")";
  return std::make_unique<ObjectPropertiesResult>(
      d::TypeCheckResult::kSmi, stream.str(), "v8::internal::Smi");
}

}  // namespace v8_debug_helper_internal

namespace di = v8_debug_helper_internal;

extern "C" {
V8_DEBUG_HELPER_EXPORT d::ObjectPropertiesResult*
_v8_debug_helper_GetObjectProperties(uintptr_t object,
                                     d::MemoryAccessor memory_accessor,
                                     const d::HeapAddresses& heap_addresses,
                                     const char* type_hint) {
  return di::GetObjectProperties(object, memory_accessor, heap_addresses,
                                 type_hint)
      .release()
      ->GetPublicView();
}
V8_DEBUG_HELPER_EXPORT void _v8_debug_helper_Free_ObjectPropertiesResult(
    d::ObjectPropertiesResult* result) {
  std::unique_ptr<di::ObjectPropertiesResult> ptr(
      static_cast<di::ObjectPropertiesResultExtended*>(result)->base);
}
}
