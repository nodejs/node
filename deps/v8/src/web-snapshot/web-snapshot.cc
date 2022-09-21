// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/web-snapshot/web-snapshot.h"

#include <limits>

#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-object.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "src/api/api-inl.h"
#include "src/handles/handles.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/bigint.h"
#include "src/objects/contexts-inl.h"
#include "src/objects/js-array-buffer-inl.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/script.h"

namespace v8 {
namespace internal {

constexpr uint8_t WebSnapshotSerializerDeserializer::kMagicNumber[4];
constexpr int WebSnapshotSerializerDeserializer::kBuiltinObjectCount;

// When encountering an error during deserializing, we note down the error but
// don't bail out from processing the snapshot further. This is to speed up
// deserialization; the error case is now slower since we don't bail out, but
// the non-error case is faster, since we don't repeatedly check for errors.
// (Invariant: we might fill our internal data structures with arbitrary data,
// but it shouldn't have an observable effect.)

// This doesn't increase the complexity of processing the data in a robust and
// secure way. We cannot trust the data anyway, so every upcoming byte can have
// an arbitrary value, not depending on whether or not we've encountered an
// error before.
void WebSnapshotSerializerDeserializer::Throw(const char* message) {
  if (error_message_ != nullptr) {
    return;
  }
  error_message_ = message;
  if (!isolate_->has_pending_exception()) {
    isolate_->Throw(*factory()->NewError(
        MessageTemplate::kWebSnapshotError,
        factory()->NewStringFromAsciiChecked(error_message_)));
  }
}

void WebSnapshotSerializerDeserializer::IterateBuiltinObjects(
    std::function<void(Handle<String>, Handle<HeapObject>)> func) {
  // TODO(v8:11525): Add more builtins.
  auto roots = ReadOnlyRoots(isolate_);

  func(handle(roots.Error_string(), isolate_),
       handle(isolate_->context().error_function(), isolate_));

  func(factory()->NewStringFromAsciiChecked("Error.prototype"),
       handle(isolate_->context().error_function().instance_prototype(),
              isolate_));

  func(handle(roots.Object_string(), isolate_),
       handle(isolate_->context().object_function(), isolate_));

  func(factory()->NewStringFromAsciiChecked("Object.prototype"),
       handle(isolate_->context().initial_object_prototype(), isolate_));

  func(handle(roots.Function_string(), isolate_),
       handle(isolate_->context().function_function(), isolate_));

  func(factory()->NewStringFromAsciiChecked("Function.prototype"),
       handle(isolate_->context().function_prototype(), isolate_));

  // TODO(v8:11525): There are no obvious names for these, since AsyncFunction
  // etc are not properties of the global object.

  func(factory()->NewStringFromAsciiChecked("AsyncFunction"),
       handle(isolate_->context().async_function_constructor(), isolate_));

  func(
      factory()->NewStringFromAsciiChecked("AsyncFunction"),
      handle(
          isolate_->context().async_function_constructor().instance_prototype(),
          isolate_));

  auto generator_function =
      handle(JSFunction::cast(isolate_->context()
                                  .generator_function_map()
                                  .constructor_or_back_pointer()),
             isolate_);
  func(factory()->NewStringFromAsciiChecked("GeneratorFunction"),
       generator_function);

  func(factory()->NewStringFromAsciiChecked("GeneratorFunction.prototype"),
       handle(generator_function->instance_prototype(), isolate_));

  auto async_generator_function =
      handle(JSFunction::cast(isolate_->context()
                                  .async_generator_function_map()
                                  .constructor_or_back_pointer()),
             isolate_);
  func(factory()->NewStringFromAsciiChecked("AsyncGeneratorFunction"),
       async_generator_function);

  func(factory()->NewStringFromAsciiChecked("AsyncGeneratorFunction.prototype"),
       handle(async_generator_function->instance_prototype(), isolate_));

  static_assert(kBuiltinObjectCount == 12);
}

uint8_t WebSnapshotSerializerDeserializer::FunctionKindToFunctionFlags(
    FunctionKind kind) {
  // TODO(v8:11525): Support more function kinds.
  switch (kind) {
    case FunctionKind::kNormalFunction:
    case FunctionKind::kArrowFunction:
    case FunctionKind::kGeneratorFunction:
    case FunctionKind::kAsyncFunction:
    case FunctionKind::kAsyncArrowFunction:
    case FunctionKind::kAsyncGeneratorFunction:
    case FunctionKind::kBaseConstructor:
    case FunctionKind::kDefaultBaseConstructor:
    case FunctionKind::kDerivedConstructor:
    case FunctionKind::kDefaultDerivedConstructor:
    case FunctionKind::kConciseMethod:
    case FunctionKind::kAsyncConciseMethod:
    case FunctionKind::kStaticConciseMethod:
    case FunctionKind::kStaticAsyncConciseMethod:
    case FunctionKind::kStaticConciseGeneratorMethod:
    case FunctionKind::kStaticAsyncConciseGeneratorMethod:
      break;
    default:
      Throw("Unsupported function kind");
  }
  auto flags = AsyncFunctionBitField::encode(IsAsyncFunction(kind)) |
               GeneratorFunctionBitField::encode(IsGeneratorFunction(kind)) |
               ArrowFunctionBitField::encode(IsArrowFunction(kind)) |
               MethodBitField::encode(IsConciseMethod(kind)) |
               StaticBitField::encode(IsStatic(kind)) |
               ClassConstructorBitField::encode(IsClassConstructor(kind)) |
               DefaultConstructorBitField::encode(IsDefaultConstructor(kind)) |
               DerivedConstructorBitField::encode(IsDerivedConstructor(kind));
  return flags;
}

// TODO(v8:11525): Optionally, use an enum instead.
FunctionKind WebSnapshotSerializerDeserializer::FunctionFlagsToFunctionKind(
    uint8_t flags) {
  FunctionKind kind;
  if (IsFunctionOrMethod(flags)) {
    if (ArrowFunctionBitField::decode(flags) && MethodBitField::decode(flags)) {
      kind = FunctionKind::kInvalid;
    } else {
      uint32_t index = AsyncFunctionBitField::decode(flags) << 0 |
                       GeneratorFunctionBitField::decode(flags) << 1 |
                       (ArrowFunctionBitField::decode(flags) ||
                        StaticBitField::decode(flags))
                           << 2 |
                       MethodBitField::decode(flags) << 3;
      static const FunctionKind kFunctionKinds[] = {
          // kNormalFunction
          // is_generator = false
          FunctionKind::kNormalFunction,  // is_async = false
          FunctionKind::kAsyncFunction,   // is_async = true
          // is_generator = true
          FunctionKind::kGeneratorFunction,       // is_async = false
          FunctionKind::kAsyncGeneratorFunction,  // is_async = true

          // kArrowFunction
          // is_generator = false
          FunctionKind::kArrowFunction,       // is_async = false
          FunctionKind::kAsyncArrowFunction,  // is_async = true
          // is_generator = true
          FunctionKind::kInvalid,  // is_async = false
          FunctionKind::kInvalid,  // is_async = true

          // kNonStaticMethod
          // is_generator = false
          FunctionKind::kConciseMethod,       // is_async = false
          FunctionKind::kAsyncConciseMethod,  // is_async = true
          // is_generator = true
          // TODO(v8::11525) Support FunctionKind::kConciseGeneratorMethod.
          FunctionKind::kInvalid,  // is_async = false
          // TODO(v8::11525) Support FunctionKind::kAsyncConciseGeneratorMethod.
          FunctionKind::kInvalid,  // is_async = true

          // kStaticMethod
          // is_generator = false
          FunctionKind::kStaticConciseMethod,       // is_async = false
          FunctionKind::kStaticAsyncConciseMethod,  // is_async = true
          // is_generator = true
          FunctionKind::kStaticConciseGeneratorMethod,      // is_async = false
          FunctionKind::kStaticAsyncConciseGeneratorMethod  // is_async = true
      };
      kind = kFunctionKinds[index];
    }
  } else if (IsConstructor(flags)) {
    static const FunctionKind kFunctionKinds[] = {
        // is_derived = false
        FunctionKind::kBaseConstructor,         // is_default = false
        FunctionKind::kDefaultBaseConstructor,  // is_default = true
        // is_derived = true
        FunctionKind::kDerivedConstructor,        // is_default = false
        FunctionKind::kDefaultDerivedConstructor  // is_default = true
    };
    kind = kFunctionKinds[flags >> DefaultConstructorBitField::kShift];
  } else {
    kind = FunctionKind::kInvalid;
  }
  if (kind == FunctionKind::kInvalid) {
    Throw("Invalid function flags\n");
  }
  return kind;
}

bool WebSnapshotSerializerDeserializer::IsFunctionOrMethod(uint8_t flags) {
  uint32_t mask = AsyncFunctionBitField::kMask |
                  GeneratorFunctionBitField::kMask |
                  ArrowFunctionBitField::kMask | MethodBitField::kMask |
                  StaticBitField::kMask;
  return (flags & mask) == flags;
}

bool WebSnapshotSerializerDeserializer::IsConstructor(uint8_t flags) {
  uint32_t mask = ClassConstructorBitField::kMask |
                  DefaultConstructorBitField::kMask |
                  DerivedConstructorBitField::kMask;
  return ClassConstructorBitField::decode(flags) && (flags & mask) == flags;
}

uint8_t WebSnapshotSerializerDeserializer::GetDefaultAttributeFlags() {
  auto flags = ReadOnlyBitField::encode(false) |
               ConfigurableBitField::encode(true) |
               EnumerableBitField::encode(true);
  return flags;
}

uint8_t WebSnapshotSerializerDeserializer::AttributesToFlags(
    PropertyDetails details) {
  auto flags = ReadOnlyBitField::encode(details.IsReadOnly()) |
               ConfigurableBitField::encode(details.IsConfigurable()) |
               EnumerableBitField::encode(details.IsEnumerable());
  return flags;
}

PropertyAttributes WebSnapshotSerializerDeserializer::FlagsToAttributes(
    uint8_t flags) {
  int attributes = ReadOnlyBitField::decode(flags) * READ_ONLY +
                   !ConfigurableBitField::decode(flags) * DONT_DELETE +
                   !EnumerableBitField::decode(flags) * DONT_ENUM;
  return PropertyAttributesFromInt(attributes);
}

WebSnapshotSerializer::WebSnapshotSerializer(v8::Isolate* isolate)
    : WebSnapshotSerializer(reinterpret_cast<v8::internal::Isolate*>(isolate)) {
}

WebSnapshotSerializer::WebSnapshotSerializer(Isolate* isolate)
    : WebSnapshotSerializerDeserializer(isolate),
      string_serializer_(isolate_, nullptr),
      symbol_serializer_(isolate_, nullptr),
      bigint_serializer_(isolate_, nullptr),
      map_serializer_(isolate_, nullptr),
      builtin_object_serializer_(isolate_, nullptr),
      context_serializer_(isolate_, nullptr),
      function_serializer_(isolate_, nullptr),
      class_serializer_(isolate_, nullptr),
      array_serializer_(isolate_, nullptr),
      typed_array_serializer_(isolate_, nullptr),
      array_buffer_serializer_(isolate_, nullptr),
      data_view_serializer_(isolate_, nullptr),
      object_serializer_(isolate_, nullptr),
      export_serializer_(isolate_, nullptr),
      external_object_ids_(isolate_->heap()),
      string_ids_(isolate_->heap()),
      symbol_ids_(isolate_->heap()),
      bigint_ids_(isolate_->heap()),
      map_ids_(isolate_->heap()),
      context_ids_(isolate_->heap()),
      function_ids_(isolate_->heap()),
      class_ids_(isolate_->heap()),
      array_ids_(isolate_->heap()),
      typed_array_ids_(isolate->heap()),
      array_buffer_ids_(isolate->heap()),
      data_view_ids_(isolate->heap()),
      object_ids_(isolate_->heap()),
      builtin_object_to_name_(isolate_->heap()),
      builtin_object_ids_(isolate_->heap()),
      all_strings_(isolate_->heap()) {
  auto empty_array_list = factory()->empty_array_list();
  strings_ = empty_array_list;
  symbols_ = empty_array_list;
  bigints_ = empty_array_list;
  maps_ = empty_array_list;
  contexts_ = empty_array_list;
  functions_ = empty_array_list;
  classes_ = empty_array_list;
  arrays_ = empty_array_list;
  array_buffers_ = empty_array_list;
  typed_arrays_ = empty_array_list;
  data_views_ = empty_array_list;
  objects_ = empty_array_list;
}

WebSnapshotSerializer::~WebSnapshotSerializer() {}

bool WebSnapshotSerializer::TakeSnapshot(
    Handle<Object> object, MaybeHandle<FixedArray> maybe_externals,
    WebSnapshotData& data_out) {
  if (string_ids_.size() > 0) {
    Throw("Can't reuse WebSnapshotSerializer");
    return false;
  }
  if (!maybe_externals.is_null()) {
    ShallowDiscoverExternals(*maybe_externals.ToHandleChecked());
  }

  v8::Local<v8::Context> context =
      reinterpret_cast<v8::Isolate*>(isolate_)->GetCurrentContext();
  ShallowDiscoverBuiltinObjects(context);

  if (object->IsHeapObject()) Discover(Handle<HeapObject>::cast(object));

  ConstructSource();
  // The export is serialized with the empty string as name; we need to
  // "discover" the name here.
  DiscoverString(factory()->empty_string());
  SerializeExport(object, factory()->empty_string());

  WriteSnapshot(data_out.buffer, data_out.buffer_size);

  if (has_error()) {
    isolate_->ReportPendingMessages();
    return false;
  }
  return true;
}

bool WebSnapshotSerializer::TakeSnapshot(v8::Local<v8::Context> context,
                                         v8::Local<v8::PrimitiveArray> exports,
                                         WebSnapshotData& data_out) {
  if (string_ids_.size() > 0) {
    Throw("Can't reuse WebSnapshotSerializer");
    return false;
  }
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);

  ShallowDiscoverBuiltinObjects(context);

  Handle<FixedArray> export_objects =
      isolate_->factory()->NewFixedArray(exports->Length());
  for (int i = 0, length = exports->Length(); i < length; ++i) {
    v8::Local<v8::String> str =
        exports->Get(v8_isolate, i)->ToString(context).ToLocalChecked();
    if (str->Length() == 0) {
      continue;
    }
    // Discover the export name.
    DiscoverString(Handle<String>::cast(Utils::OpenHandle(*str)));
    v8::ScriptCompiler::Source source(str);
    auto script = ScriptCompiler::Compile(context, &source).ToLocalChecked();
    v8::MaybeLocal<v8::Value> script_result = script->Run(context);
    v8::Local<v8::Object> v8_object;
    if (script_result.IsEmpty() ||
        !script_result.ToLocalChecked()->ToObject(context).ToLocal(
            &v8_object)) {
      Throw("Exported object not found");
      return false;
    }
    auto object = Handle<JSObject>::cast(Utils::OpenHandle(*v8_object));
    export_objects->set(i, *object);
    Discover(object);
    // The error messages will be confusing if we continue running code when
    // already in the error state.
    if (has_error()) {
      isolate_->ReportPendingMessages();
      return false;
    }
  }

  ConstructSource();

  for (int i = 0, length = exports->Length(); i < length; ++i) {
    v8::Local<v8::String> str =
        exports->Get(v8_isolate, i)->ToString(context).ToLocalChecked();
    if (str->Length() == 0) {
      continue;
    }
    SerializeExport(handle(export_objects->get(i), isolate_),
                    Handle<String>::cast(Utils::OpenHandle(*str)));
  }

  WriteSnapshot(data_out.buffer, data_out.buffer_size);

  if (has_error()) {
    isolate_->ReportPendingMessages();
    return false;
  }
  return true;
}

void WebSnapshotSerializer::SerializePendingItems() {
  // The information about string reference counts is now complete. The strings
  // in strings_ are not in place and can be serialized now. The in-place
  // strings will be serialized as part of their respective objects.
  for (int i = 0; i < strings_->Length(); ++i) {
    Handle<String> string = handle(String::cast(strings_->Get(i)), isolate_);
    SerializeString(string, string_serializer_);
  }

  for (int i = 0; i < symbols_->Length(); ++i) {
    Handle<Symbol> symbol = handle(Symbol::cast(symbols_->Get(i)), isolate_);
    SerializeSymbol(symbol);
  }

  for (int i = 0; i < bigints_->Length(); ++i) {
    Handle<BigInt> bigint = handle(BigInt::cast(bigints_->Get(i)), isolate_);
    SerializeBigInt(bigint);
  }

  for (int i = 0; i < maps_->Length(); ++i) {
    Handle<Map> map = handle(Map::cast(maps_->Get(i)), isolate_);
    SerializeMap(map);
  }
  for (auto name_id : builtin_objects_) {
    SerializeBuiltinObject(name_id);
  }

  for (int i = 0; i < contexts_->Length(); ++i) {
    Handle<Context> context =
        handle(Context::cast(contexts_->Get(i)), isolate_);
    SerializeContext(context, static_cast<uint32_t>(i));
  }

  // Serialize the items in the reverse order. The items at the end of the
  // functions_ etc get lower IDs and vice versa. IDs which items use for
  // referring to each other are reversed by Get<item>Id() functions.
  for (int i = functions_->Length() - 1; i >= 0; --i) {
    Handle<JSFunction> function =
        handle(JSFunction::cast(functions_->Get(i)), isolate_);
    SerializeFunction(function);
  }
  for (int i = classes_->Length() - 1; i >= 0; --i) {
    Handle<JSFunction> function =
        handle(JSFunction::cast(classes_->Get(i)), isolate_);
    SerializeClass(function);
  }
  for (int i = arrays_->Length() - 1; i >= 0; --i) {
    Handle<JSArray> array = handle(JSArray::cast(arrays_->Get(i)), isolate_);
    SerializeArray(array);
  }
  for (int i = array_buffers_->Length() - 1; i >= 0; --i) {
    Handle<JSArrayBuffer> array_buffer =
        handle(JSArrayBuffer::cast(array_buffers_->Get(i)), isolate_);
    SerializeArrayBuffer(array_buffer);
  }
  for (int i = typed_arrays_->Length() - 1; i >= 0; --i) {
    Handle<JSTypedArray> typed_array =
        handle(JSTypedArray::cast(typed_arrays_->Get(i)), isolate_);
    SerializeTypedArray(typed_array);
  }
  for (int i = data_views_->Length() - 1; i >= 0; --i) {
    Handle<JSDataView> data_view =
        handle(JSDataView::cast(data_views_->Get(i)), isolate_);
    SerializeDataView(data_view);
  }
  for (int i = objects_->Length() - 1; i >= 0; --i) {
    Handle<JSObject> object =
        handle(JSObject::cast(objects_->Get(i)), isolate_);
    SerializeObject(object);
  }
}

// Format (full snapshot):
// - Magic number (4 bytes)
// - String count
// - For each string:
//   - Serialized string
// - Symbol count
// - For each symbol:
//   - Serialized symbol
// - Builtin object count
// - For each builtin object:
//   - Id of the builtin object name string
// - Shape count
// - For each shape:
//   - Serialized shape
// - Context count
// - For each context:
//   - Serialized context
// - Function count
// - For each function:
//   - Serialized function
// - Object count
// - For each object:
//   - Serialized object
// - Export count
// - For each export:
//   - Serialized export
void WebSnapshotSerializer::WriteSnapshot(uint8_t*& buffer,
                                          size_t& buffer_size) {
  if (has_error()) {
    return;
  }
  SerializePendingItems();

  ValueSerializer total_serializer(isolate_, nullptr);
  size_t needed_size =
      sizeof(kMagicNumber) + string_serializer_.buffer_size_ +
      symbol_serializer_.buffer_size_ + bigint_serializer_.buffer_size_ +
      builtin_object_serializer_.buffer_size_ + map_serializer_.buffer_size_ +
      context_serializer_.buffer_size_ + function_serializer_.buffer_size_ +
      class_serializer_.buffer_size_ + array_serializer_.buffer_size_ +
      array_buffer_serializer_.buffer_size_ +
      typed_array_serializer_.buffer_size_ +
      data_view_serializer_.buffer_size_ + object_serializer_.buffer_size_ +
      export_serializer_.buffer_size_ + 14 * sizeof(uint32_t);
  if (total_serializer.ExpandBuffer(needed_size).IsNothing()) {
    Throw("Out of memory");
    return;
  }

  total_serializer.WriteRawBytes(kMagicNumber, 4);
  WriteObjects(total_serializer, string_count(), string_serializer_, "strings");
  WriteObjects(total_serializer, symbol_count(), symbol_serializer_, "symbols");
  WriteObjects(total_serializer, bigint_count(), bigint_serializer_, "bigints");
  WriteObjects(total_serializer, builtin_object_count(),
               builtin_object_serializer_, "builtin_objects");
  WriteObjects(total_serializer, map_count(), map_serializer_, "maps");
  WriteObjects(total_serializer, context_count(), context_serializer_,
               "contexts");
  WriteObjects(total_serializer, function_count(), function_serializer_,
               "functions");
  WriteObjects(total_serializer, array_count(), array_serializer_, "arrays");
  WriteObjects(total_serializer, array_buffer_count(), array_buffer_serializer_,
               "array buffers");
  WriteObjects(total_serializer, typed_array_count(), typed_array_serializer_,
               "typed arrays");
  WriteObjects(total_serializer, data_view_count(), data_view_serializer_,
               "data views");
  WriteObjects(total_serializer, object_count(), object_serializer_, "objects");
  WriteObjects(total_serializer, class_count(), class_serializer_, "classes");
  WriteObjects(total_serializer, export_count_, export_serializer_, "exports");

  if (has_error()) {
    return;
  }

  auto result = total_serializer.Release();
  buffer = result.first;
  buffer_size = result.second;
}
void WebSnapshotSerializer::WriteObjects(ValueSerializer& destination,
                                         size_t count, ValueSerializer& source,
                                         const char* name) {
  if (count > std::numeric_limits<uint32_t>::max()) {
    Throw("Too many objects");
    return;
  }
  destination.WriteUint32(static_cast<uint32_t>(count));
  destination.WriteRawBytes(source.buffer_, source.buffer_size_);
}

bool WebSnapshotSerializer::InsertIntoIndexMap(ObjectCacheIndexMap& map,
                                               HeapObject heap_object,
                                               uint32_t& id) {
  DisallowGarbageCollection no_gc;
  int index_out;
  bool found = map.LookupOrInsert(heap_object, &index_out);
  id = static_cast<uint32_t>(index_out);
  return found;
}

// Format:
// - Length
// - Raw bytes (data)
void WebSnapshotSerializer::SerializeString(Handle<String> string,
                                            ValueSerializer& serializer) {
  DisallowGarbageCollection no_gc;
  String::FlatContent flat = string->GetFlatContent(no_gc);
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    base::Vector<const uint8_t> chars = flat.ToOneByteVector();
    serializer.WriteUint32(chars.length());
    serializer.WriteRawBytes(chars.begin(), chars.length() * sizeof(uint8_t));
  } else if (flat.IsTwoByte()) {
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
    v8::Local<v8::String> api_string = Utils::ToLocal(string);
    int length = api_string->Utf8Length(v8_isolate);
    std::unique_ptr<char[]> buffer(new char[length]);
    api_string->WriteUtf8(v8_isolate, buffer.get(), length);
    serializer.WriteUint32(length);
    serializer.WriteRawBytes(buffer.get(), length * sizeof(uint8_t));
  } else {
    UNREACHABLE();
  }
}

// Format (serialized symbol):
// - 0 if the symbol is non-global and there's no description, 1 if the symbol
// is non-global and there is a description, 2 if the symbol is global (there
// must be a description).
void WebSnapshotSerializer::SerializeSymbol(Handle<Symbol> symbol) {
  if (symbol->description().IsUndefined()) {
    CHECK(!symbol->is_in_public_symbol_table());
    symbol_serializer_.WriteUint32(SymbolType::kNonGlobalNoDesription);
  } else {
    symbol_serializer_.WriteUint32(symbol->is_in_public_symbol_table()
                                       ? SymbolType::kGlobal
                                       : SymbolType::kNonGlobal);
    WriteStringId(handle(String::cast(symbol->description()), isolate_),
                  symbol_serializer_);
  }
}

// Format (serialized bigint)
// - BigIntFlags, including sign and byte length.
// - digit bytes.
void WebSnapshotSerializer::SerializeBigInt(Handle<BigInt> bigint) {
  uint32_t flags = BigIntSignAndLengthToFlags(bigint);
  bigint_serializer_.WriteUint32(flags);
  int byte_length = BigIntLengthBitField::decode(flags);
  uint8_t* dest;
  if (bigint_serializer_.ReserveRawBytes(byte_length).To(&dest)) {
    bigint->SerializeDigits(dest);
  } else {
    Throw("Serialize BigInt failed");
    return;
  }
}

bool WebSnapshotSerializer::ShouldBeSerialized(Handle<Name> key) {
  // Don't serialize class_positions_symbol property in Class.
  if (key->Equals(*factory()->class_positions_symbol())) {
    return false;
  }
  return true;
}

// Format (serialized shape):
// - PropertyAttributesType
// - Property count
// - For each property
//   - Name: STRING_ID + String id or SYMBOL_ID + Symbol id or in-place string
//   - If the PropertyAttributesType is CUSTOM: attributes
// - __proto__: Serialized value
void WebSnapshotSerializer::SerializeMap(Handle<Map> map) {
  DCHECK(!map->is_dictionary_map());
  int first_custom_index = -1;
  std::vector<Handle<Name>> keys;
  std::vector<uint8_t> attributes;
  keys.reserve(map->NumberOfOwnDescriptors());
  attributes.reserve(map->NumberOfOwnDescriptors());
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details =
        map->instance_descriptors(kRelaxedLoad).GetDetails(i);

    // If there are non-field properties in a map that doesn't allow them, i.e.,
    // a non-function map, DiscoverMap has already thrown.
    if (details.location() != PropertyLocation::kField) {
      continue;
    }

    Handle<Name> key(map->instance_descriptors(kRelaxedLoad).GetKey(i),
                     isolate_);
    if (!ShouldBeSerialized(key)) {
      continue;
    }
    keys.push_back(key);
    if (first_custom_index >= 0 || details.IsReadOnly() ||
        !details.IsConfigurable() || details.IsDontEnum()) {
      if (first_custom_index == -1) first_custom_index = i.as_int();
      attributes.push_back(AttributesToFlags(details));
    }
  }

  map_serializer_.WriteUint32(first_custom_index == -1
                                  ? PropertyAttributesType::DEFAULT
                                  : PropertyAttributesType::CUSTOM);

  map_serializer_.WriteUint32(static_cast<uint32_t>(keys.size()));

  uint8_t default_flags = GetDefaultAttributeFlags();
  for (size_t i = 0; i < keys.size(); ++i) {
    if (keys[i]->IsString()) {
      WriteStringMaybeInPlace(Handle<String>::cast(keys[i]), map_serializer_);
    } else if (keys[i]->IsSymbol()) {
      map_serializer_.WriteByte(ValueType::SYMBOL_ID);
      map_serializer_.WriteUint32(GetSymbolId(Symbol::cast(*keys[i])));
    } else {
      // This error should've been recognized in the discovery phase.
      CHECK(false);
    }
    if (first_custom_index >= 0) {
      if (static_cast<int>(i) < first_custom_index) {
        map_serializer_.WriteByte(default_flags);
      } else {
        map_serializer_.WriteByte(attributes[i - first_custom_index]);
      }
    }
  }

  WriteValue(handle(map->prototype(), isolate_), map_serializer_);
}

void WebSnapshotSerializer::SerializeBuiltinObject(uint32_t name_id) {
  builtin_object_serializer_.WriteUint32(name_id);
}

// Construct the minimal source string to be included in the snapshot. Maintain
// the "inner function is textually inside its outer function" relationship.
// Example:
// Input:
// Full source:       abcdefghijklmnopqrstuvwxyzåäö
// Functions:            11111111       22222222  3
// Inner functions:       44  55         666
// Output:
// Constructed source:   defghijkstuvwxyzö
// Functions:            11111111222222223
// Inner functions        44  55  666
void WebSnapshotSerializer::ConstructSource() {
  if (source_intervals_.empty()) {
    return;
  }

  Handle<String> source_string = factory()->empty_string();
  int current_interval_start = 0;
  int current_interval_end = 0;
  for (const auto& interval : source_intervals_) {
    DCHECK_LE(current_interval_start, interval.first);  // Iterated in order.
    DCHECK_LE(interval.first, interval.second);
    if (interval.second <= current_interval_end) {
      // This interval is fully within the current interval. We don't need to
      // include any new source code, just record the position conversion.
      auto offset_within_parent = interval.first - current_interval_start;
      source_offset_to_compacted_source_offset_[interval.first] =
          source_offset_to_compacted_source_offset_[current_interval_start] +
          offset_within_parent;
      continue;
    }
    // Start a new interval.
    current_interval_start = interval.first;
    current_interval_end = interval.second;
    source_offset_to_compacted_source_offset_[current_interval_start] =
        source_string->length();
    MaybeHandle<String> new_source_string = factory()->NewConsString(
        source_string,
        factory()->NewSubString(full_source_, current_interval_start,
                                current_interval_end));
    if (!new_source_string.ToHandle(&source_string)) {
      Throw("Cannot construct source string");
      return;
    }
  }
  DiscoverString(source_string);
  bool in_place = false;
  source_id_ = GetStringId(source_string, in_place);
  DCHECK(!in_place);
}

void WebSnapshotSerializer::SerializeFunctionProperties(
    Handle<JSFunction> function, ValueSerializer& serializer) {
  Handle<Map> map(function->map(), isolate_);
  if (function->map() ==
      isolate_->context().get(function->shared().function_map_index())) {
    serializer.WriteUint32(0);
    return;
  } else {
    serializer.WriteUint32(GetMapId(function->map()) + 1);
  }
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details =
        map->instance_descriptors(kRelaxedLoad).GetDetails(i);
    if (details.location() == PropertyLocation::kDescriptor) {
      continue;
    }
    if (!ShouldBeSerialized(
            handle(map->instance_descriptors().GetKey(i), isolate_))) {
      continue;
    }
    FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
    Handle<Object> value = JSObject::FastPropertyAt(
        isolate_, function, details.representation(), field_index);
    WriteValue(value, serializer);
  }
}

void WebSnapshotSerializer::SerializeFunctionInfo(Handle<JSFunction> function,
                                                  ValueSerializer& serializer) {
  if (!function->shared().HasSourceCode()) {
    Throw("Function without source code");
    return;
  }

  {
    DisallowGarbageCollection no_gc;
    Context context = function->context();
    if (context.IsNativeContext() || context.IsScriptContext()) {
      serializer.WriteUint32(0);
    } else {
      DCHECK(context.IsFunctionContext() || context.IsBlockContext());
      uint32_t context_id = GetContextId(context);
      serializer.WriteUint32(context_id + 1);
    }
  }

  serializer.WriteUint32(source_id_);
  Handle<Script> script =
      handle(Script::cast(function->shared().script()), isolate_);
  int start = function->shared().StartPosition();
  int end = function->shared().EndPosition();
  int final_offset =
      source_offset_to_compacted_source_offset_[script_offsets_[script->id()] +
                                                start];
  serializer.WriteUint32(final_offset);
  serializer.WriteUint32(end - start);

  serializer.WriteUint32(
      function->shared().internal_formal_parameter_count_without_receiver());
  serializer.WriteByte(FunctionKindToFunctionFlags(function->shared().kind()));

  if (function->has_prototype_slot() && function->has_instance_prototype()) {
    DisallowGarbageCollection no_gc;
    JSObject prototype = JSObject::cast(function->instance_prototype());
    uint32_t prototype_id = GetObjectId(prototype);
    serializer.WriteUint32(prototype_id + 1);
  } else {
    serializer.WriteUint32(0);
  }
}

void WebSnapshotSerializer::ShallowDiscoverExternals(FixedArray externals) {
  DisallowGarbageCollection no_gc;
  for (int i = 0; i < externals.length(); i++) {
    Object object = externals.get(i);
    if (!object.IsHeapObject()) continue;
    uint32_t unused_id = 0;
    InsertIntoIndexMap(external_object_ids_, HeapObject::cast(object),
                       unused_id);
  }
}

void WebSnapshotSerializer::ShallowDiscoverBuiltinObjects(
    v8::Local<v8::Context> context) {
  // Fill in builtin_object_to_name_. Don't discover them or their
  // names, so that they won't be included in the snapshot unless needed.

  builtin_object_name_strings_ =
      isolate_->factory()->NewFixedArray(kBuiltinObjectCount);

  int i = 0;
  IterateBuiltinObjects([&](Handle<String> name, Handle<HeapObject> object) {
    builtin_object_name_strings_->set(i, *name);
    uint32_t id;
    bool already_exists =
        InsertIntoIndexMap(builtin_object_to_name_, *object, id);
    CHECK(!already_exists);
    CHECK_EQ(static_cast<int>(id), i);
    ++i;
  });
  DCHECK_EQ(i, kBuiltinObjectCount);
}

void WebSnapshotSerializer::Discover(Handle<HeapObject> start_object) {
  // The object discovery phase assigns IDs for objects / functions / classes /
  // arrays and discovers outgoing references from them. This is needed so that
  // e.g., we know all functions upfront and can construct the source code that
  // covers them before serializing the functions.

  discovery_queue_.push(start_object);

  while (!discovery_queue_.empty()) {
    const Handle<HeapObject>& object = discovery_queue_.front();
    switch (object->map().instance_type()) {
      case JS_FUNCTION_TYPE:
        DiscoverFunction(Handle<JSFunction>::cast(object));
        break;
      case JS_CLASS_CONSTRUCTOR_TYPE:
        DiscoverClass(Handle<JSFunction>::cast(object));
        break;
      case JS_OBJECT_TYPE:
      case JS_OBJECT_PROTOTYPE_TYPE:
        DiscoverObject(Handle<JSObject>::cast(object));
        break;
      case JS_ARRAY_TYPE:
        DiscoverArray(Handle<JSArray>::cast(object));
        break;
      case SYMBOL_TYPE:
        DiscoverSymbol(Handle<Symbol>::cast(object));
        break;
      case BIGINT_TYPE:
        DiscoverBigInt(Handle<BigInt>::cast(object));
        break;
      case ODDBALL_TYPE:
      case HEAP_NUMBER_TYPE:
        // Can't contain references to other objects.
        break;
      case JS_PRIMITIVE_WRAPPER_TYPE: {
        Handle<JSPrimitiveWrapper> wrapper =
            Handle<JSPrimitiveWrapper>::cast(object);
        Handle<Object> value = handle(wrapper->value(), isolate_);
        if (value->IsHeapObject()) {
          discovery_queue_.push(Handle<HeapObject>::cast(value));
        }
        break;
      }
      case JS_REG_EXP_TYPE: {
        Handle<JSRegExp> regexp = Handle<JSRegExp>::cast(object);
        Handle<String> pattern = handle(regexp->source(), isolate_);
        DiscoverString(pattern);
        Handle<String> flags_string =
            JSRegExp::StringFromFlags(isolate_, regexp->flags());
        DiscoverString(flags_string);
        break;
      }
      case JS_ARRAY_BUFFER_TYPE: {
        Handle<JSArrayBuffer> array_buffer =
            Handle<JSArrayBuffer>::cast(object);
        DiscoverArrayBuffer(array_buffer);
        break;
      }
      case JS_TYPED_ARRAY_TYPE: {
        Handle<JSTypedArray> typed_array = Handle<JSTypedArray>::cast(object);
        DiscoverTypedArray(typed_array);
        break;
      }
      case JS_DATA_VIEW_TYPE: {
        Handle<JSDataView> data_view = Handle<JSDataView>::cast(object);
        DiscoverDataView(data_view);
        break;
      }
      default:
        if (object->IsString()) {
          // These are array elements / object properties -> allow in place
          // strings.
          DiscoverString(Handle<String>::cast(object), AllowInPlace::Yes);
          break;
        } else if (external_object_ids_.size() > 0) {
          int unused_id;
          external_object_ids_.LookupOrInsert(*object, &unused_id);
        } else {
          Throw("Unsupported object");
        }
    }
    discovery_queue_.pop();
  }
}

void WebSnapshotSerializer::DiscoverPropertyKey(Handle<Name> key) {
  if (key->IsString()) {
    DiscoverString(Handle<String>::cast(key), AllowInPlace::Yes);
  } else if (key->IsSymbol()) {
    DiscoverSymbol(Handle<Symbol>::cast(key));
  } else {
    Throw("Property key is not a String / Symbol");
    return;
  }
}

void WebSnapshotSerializer::DiscoverMap(Handle<Map> map,
                                        bool allow_property_in_descriptor) {
  // Dictionary map object names get discovered in DiscoverObject.
  if (map->is_dictionary_map()) {
    return;
  }

  uint32_t id;
  if (InsertIntoIndexMap(map_ids_, *map, id)) {
    return;
  }
  DCHECK_EQ(id, maps_->Length());
  maps_ = ArrayList::Add(isolate_, maps_, map);
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details =
        map->instance_descriptors(kRelaxedLoad).GetDetails(i);
    if (details.location() != PropertyLocation::kField) {
      if (!allow_property_in_descriptor) {
        Throw("Properties which are not fields not supported");
        return;
      } else {
        continue;
      }
    }
    Handle<Name> key(map->instance_descriptors(kRelaxedLoad).GetKey(i),
                     isolate_);
    if (ShouldBeSerialized(key)) {
      DiscoverPropertyKey(key);
    }
  }
}

void WebSnapshotSerializer::DiscoverString(Handle<String> string,
                                           AllowInPlace can_be_in_place) {
  // Can't contain references to other objects. We only log the existence of the
  // string itself. Internalize the strings so that we can properly track which
  // String objects are the same string.
  string = factory()->InternalizeString(string);
  auto result = all_strings_.FindOrInsert(string);
  if (can_be_in_place == AllowInPlace::Yes && !result.already_exists) {
    // This is the only reference to the string so far. Don't generate and
    // ID for it yet; only generate it when another reference to the string is
    // found.
    return;
  }
  // The string is referred to more than two places, or in-placing not allowed
  // -> not a candidate for writing it in-place. Generate an ID for it.

  // TODO(v8:11525): Allow in-place strings in more places. Heuristics for
  // when to make them in place?
  uint32_t id;
  if (InsertIntoIndexMap(string_ids_, *string, id)) {
    return;
  }
  DCHECK_EQ(id, strings_->Length());
  strings_ = ArrayList::Add(isolate_, strings_, string);
}

void WebSnapshotSerializer::DiscoverMapForFunction(
    Handle<JSFunction> function) {
  JSObject::MigrateSlowToFast(function, 0, "Web snapshot");
  // TODO(v8:11525): Support functions with so many properties that they can't
  // be in fast mode.
  if (!function->HasFastProperties()) {
    Throw("Unsupported function with dictionary map");
    return;
  }
  if (function->map() !=
      isolate_->context().get(function->shared().function_map_index())) {
    Handle<Map> map(function->map(), isolate_);
    // We only serialize properties which are fields in function. And properties
    // which are descriptors will be setup in CreateJSFunction.
    DiscoverMap(map, true);
    discovery_queue_.push(handle(map->prototype(), isolate_));
    // Discover property values.
    for (InternalIndex i : map->IterateOwnDescriptors()) {
      PropertyDetails details =
          map->instance_descriptors(kRelaxedLoad).GetDetails(i);
      if (details.location() == PropertyLocation::kDescriptor) {
        continue;
      }
      if (!ShouldBeSerialized(
              handle(map->instance_descriptors().GetKey(i), isolate_))) {
        continue;
      }
      FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
      Handle<Object> value = JSObject::FastPropertyAt(
          isolate_, function, details.representation(), field_index);
      if (!value->IsHeapObject()) continue;
      discovery_queue_.push(Handle<HeapObject>::cast(value));
    }
  }
}

void WebSnapshotSerializer::DiscoverFunction(Handle<JSFunction> function) {
  if (DiscoverIfBuiltinObject(function)) {
    return;
  }

  uint32_t id;
  if (InsertIntoIndexMap(function_ids_, *function, id)) {
    return;
  }

  DCHECK_EQ(id, functions_->Length());
  functions_ = ArrayList::Add(isolate_, functions_, function);

  DiscoverContextAndPrototype(function);

  DiscoverMapForFunction(function);
  DiscoverSource(function);
}

void WebSnapshotSerializer::DiscoverClass(Handle<JSFunction> function) {
  uint32_t id;
  if (InsertIntoIndexMap(class_ids_, *function, id)) {
    return;
  }

  DCHECK_EQ(id, classes_->Length());
  classes_ = ArrayList::Add(isolate_, classes_, function);

  DiscoverContextAndPrototype(function);

  DiscoverMapForFunction(function);
  // TODO(v8:11525): Support class members.
  DiscoverSource(function);
}

void WebSnapshotSerializer::DiscoverContextAndPrototype(
    Handle<JSFunction> function) {
  Handle<Context> context(function->context(), isolate_);
  if (context->IsFunctionContext() || context->IsBlockContext()) {
    DiscoverContext(context);
  }

  if (function->has_prototype_slot() &&
      function->map().has_non_instance_prototype()) {
    Throw("Functions with non-instance prototypes not supported");
    return;
  }

  if (function->has_prototype_slot() && function->has_instance_prototype()) {
    Handle<JSObject> prototype = Handle<JSObject>::cast(
        handle(function->instance_prototype(), isolate_));
    discovery_queue_.push(prototype);
  }

  discovery_queue_.push(handle(function->map().prototype(), isolate_));
}

void WebSnapshotSerializer::DiscoverContext(Handle<Context> context) {
  // Make sure the parent context (if any), gets a smaller ID. This ensures the
  // parent context references in the snapshot are not deferred.
  if (!context->previous().IsNativeContext() &&
      !context->previous().IsScriptContext()) {
    DiscoverContext(handle(context->previous(), isolate_));
  }

  uint32_t id;
  if (InsertIntoIndexMap(context_ids_, *context, id)) return;

  DCHECK_EQ(id, contexts_->Length());
  contexts_ = ArrayList::Add(isolate_, contexts_, context);

  Handle<ScopeInfo> scope_info = handle(context->scope_info(), isolate_);
  for (auto it : ScopeInfo::IterateLocalNames(scope_info)) {
    DiscoverString(handle(it->name(), isolate_));
    Object value =
        context->get(scope_info->ContextHeaderLength() + it->index());
    if (!value.IsHeapObject()) continue;
    discovery_queue_.push(handle(HeapObject::cast(value), isolate_));
  }

  if (!context->previous().IsNativeContext() &&
      !context->previous().IsScriptContext()) {
    DiscoverContext(handle(context->previous(), isolate_));
  }
}

void WebSnapshotSerializer::DiscoverSource(Handle<JSFunction> function) {
  // Function may not have source code, e.g. we discover source for a builtin
  // function. In SerializeFunctionInfo, we also check if the function has
  // source code, and we throw the same error here if the function doesn't
  // have source code to be consistent with SerializeFunctionInfo.
  if (!function->shared().HasSourceCode()) {
    Throw("Function without source code");
    return;
  }
  // There might be multiple scripts where functions are coming from. Construct
  // a combined source code for them by simply concatenating the sources (and
  // keep track which script source is where); the source code will be later
  // optimized by ConstructSource.
  Handle<Script> script =
      handle(Script::cast(function->shared().script()), isolate_);
  Handle<String> function_script_source =
      handle(String::cast(script->source()), isolate_);
  int script_offset_int;
  if (full_source_.is_null()) {
    // This is the first script.
    script_offset_int = 0;
    full_source_ = function_script_source;
    script_offsets_.insert({script->id(), script_offset_int});
  } else {
    auto it = script_offsets_.find(script->id());
    if (it == script_offsets_.end()) {
      // This script hasn't been encountered yet and its source code has to be
      // added to full_source_.
      DCHECK(!full_source_.is_null());
      script_offset_int = full_source_->length();
      script_offsets_.insert({script->id(), script_offset_int});
      if (!factory()
               ->NewConsString(full_source_, function_script_source)
               .ToHandle(&full_source_)) {
        Throw("Can't construct source");
        return;
      }
    } else {
      // The script source is already somewhere in full_source_.
      script_offset_int = it->second;
    }
  }
  source_intervals_.emplace(
      script_offset_int + function->shared().StartPosition(),
      script_offset_int + function->shared().EndPosition());
}

void WebSnapshotSerializer::DiscoverArray(Handle<JSArray> array) {
  uint32_t id;
  if (InsertIntoIndexMap(array_ids_, *array, id)) {
    return;
  }
  DCHECK_EQ(id, arrays_->Length());
  arrays_ = ArrayList::Add(isolate_, arrays_, array);

  DiscoverElements(array);
}

void WebSnapshotSerializer::DiscoverElements(Handle<JSObject> object) {
  auto elements_kind = object->GetElementsKind();

  DisallowGarbageCollection no_gc;

  // TODO(v8:11525): Handle sealed & frozen elements correctly. (Also: handle
  // sealed & frozen objects.)
  switch (elements_kind) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS: {
      FixedArray elements = FixedArray::cast(object->elements());
      for (int i = 0; i < elements.length(); ++i) {
        Object object = elements.get(i);
        if (!object.IsHeapObject()) continue;
        discovery_queue_.push(handle(HeapObject::cast(object), isolate_));
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      Handle<NumberDictionary> dict(object->element_dictionary(), isolate_);
      ReadOnlyRoots roots(isolate_);
      for (InternalIndex index : dict->IterateEntries()) {
        Handle<Object> key = handle(dict->KeyAt(index), isolate_);
        if (!dict->IsKey(roots, *key)) {
          continue;
        }
        DCHECK(key->IsNumber());
        if (key->Number() > std::numeric_limits<uint32_t>::max()) {
          // TODO(v8:11525): Support large element indices.
          Throw("Large element indices not supported");
          return;
        }
        Handle<Object> object = handle(dict->ValueAt(index), isolate_);
        if (!object->IsHeapObject()) continue;
        discovery_queue_.push(Handle<HeapObject>::cast(object));
      }
      break;
    }
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS: {
      break;
    }
    default: {
      Throw("Unsupported elements");
      return;
    }
  }
}

void WebSnapshotSerializer::DiscoverArrayBuffer(
    Handle<JSArrayBuffer> array_buffer) {
  if (array_buffer->was_detached()) {
    CHECK_EQ(array_buffer->GetByteLength(), 0);
  }
  uint32_t id;
  if (InsertIntoIndexMap(array_buffer_ids_, *array_buffer, id)) {
    return;
  }
  DCHECK_EQ(id, array_buffers_->Length());
  array_buffers_ = ArrayList::Add(isolate_, array_buffers_, array_buffer);
}

void WebSnapshotSerializer::DiscoverDataView(Handle<JSDataView> data_view) {
  uint32_t id;
  if (InsertIntoIndexMap(data_view_ids_, *data_view, id)) {
    return;
  }
  DCHECK_EQ(id, data_views_->Length());
  data_views_ = ArrayList::Add(isolate_, data_views_, data_view);
  discovery_queue_.push(handle(data_view->buffer(), isolate_));
}

void WebSnapshotSerializer::DiscoverTypedArray(
    Handle<JSTypedArray> typed_array) {
  uint32_t id;
  if (InsertIntoIndexMap(typed_array_ids_, *typed_array, id)) {
    return;
  }
  DCHECK_EQ(id, typed_arrays_->Length());
  typed_arrays_ = ArrayList::Add(isolate_, typed_arrays_, typed_array);
  discovery_queue_.push(typed_array->GetBuffer());
}

template <typename T>
void WebSnapshotSerializer::DiscoverObjectPropertiesWithDictionaryMap(T dict) {
  DisallowGarbageCollection no_gc;

  ReadOnlyRoots roots(isolate_);
  for (InternalIndex index : dict->IterateEntries()) {
    Handle<Object> key = handle(dict->KeyAt(index), isolate_);
    if (!dict->IsKey(roots, *key)) {
      // Ignore deleted entries.
      continue;
    }
    DiscoverPropertyKey(Handle<Name>::cast(key));
    Handle<Object> value = handle(dict->ValueAt(index), isolate_);
    if (!value->IsHeapObject()) {
      continue;
    } else {
      discovery_queue_.push(Handle<HeapObject>::cast(value));
    }
  }
}

void WebSnapshotSerializer::DiscoverObject(Handle<JSObject> object) {
  if (GetExternalId(*object)) {
    return;
  }
  if (DiscoverIfBuiltinObject(object)) {
    return;
  }

  uint32_t id;
  if (InsertIntoIndexMap(object_ids_, *object, id)) return;

  DCHECK_EQ(id, objects_->Length());
  objects_ = ArrayList::Add(isolate_, objects_, object);

  // TODO(v8:11525): After we allow "non-map" objects which are small
  // enough to have a fast map, we should remove this. Although we support
  // objects with dictionary map now, we still check the property count is
  // bigger than kMaxNumberOfDescriptors when deserializing dictionary map and
  // then removing this will break deserializing prototype objects having a
  // dictionary map with few properties.
  JSObject::MigrateSlowToFast(object, 0, "Web snapshot");

  Handle<Map> map(object->map(), isolate_);
  DiscoverMap(map);

  // Discover __proto__.
  discovery_queue_.push(handle(map->prototype(), isolate_));

  if (object->HasFastProperties()) {
    // Discover property values.
    for (InternalIndex i : map->IterateOwnDescriptors()) {
      PropertyDetails details =
          map->instance_descriptors(kRelaxedLoad).GetDetails(i);
      FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
      Handle<Object> value = JSObject::FastPropertyAt(
          isolate_, object, details.representation(), field_index);
      if (!value->IsHeapObject()) continue;
      discovery_queue_.push(Handle<HeapObject>::cast(value));
    }
  } else {
    ReadOnlyRoots roots(isolate_);
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      Handle<SwissNameDictionary> swiss_dictionary =
          handle(object->property_dictionary_swiss(), isolate_);
      DiscoverObjectPropertiesWithDictionaryMap(swiss_dictionary);
    } else {
      Handle<NameDictionary> dictionary =
          handle(object->property_dictionary(), isolate_);
      DiscoverObjectPropertiesWithDictionaryMap(dictionary);
    }
  }

  DiscoverElements(object);
}

bool WebSnapshotSerializer::DiscoverIfBuiltinObject(Handle<HeapObject> object) {
  // TODO(v8:11525): Consider speccing a set of fixed builtins (such as
  // Object.prototype) for objects which are almost always included in the
  // snapshot.
  uint32_t name_index;
  if (!GetBuiltinObjectNameIndex(*object, name_index)) {
    return false;
  }
  CHECK_LT(name_index, builtin_object_name_strings_->length());
  Handle<String> name_string = handle(
      String::cast(builtin_object_name_strings_->get(name_index)), isolate_);
  DiscoverString(name_string, AllowInPlace::No);

  // Ensure the builtin object reference gets included in the snapshot.
  uint32_t id;
  if (InsertIntoIndexMap(builtin_object_ids_, *object, id)) {
    // The builtin object is already referred to by something else.
    return true;
  }
  DCHECK_EQ(id, builtin_objects_.size());

  bool in_place = false;
  uint32_t name_id = GetStringId(name_string, in_place);
  DCHECK(!in_place);
  USE(in_place);
  builtin_objects_.push_back(name_id);
  return true;
}

void WebSnapshotSerializer::DiscoverSymbol(Handle<Symbol> symbol) {
  if (symbol->is_well_known_symbol()) {
    // TODO(v8:11525): Support well-known Symbols.
    Throw("Well known Symbols aren't supported");
    return;
  }
  uint32_t id;
  if (InsertIntoIndexMap(symbol_ids_, *symbol, id)) return;

  DCHECK_EQ(id, symbols_->Length());
  symbols_ = ArrayList::Add(isolate_, symbols_, symbol);

  if (!symbol->description().IsUndefined()) {
    DiscoverString(handle(String::cast(symbol->description()), isolate_));
  }
}

void WebSnapshotSerializer::DiscoverBigInt(Handle<BigInt> bigint) {
  uint32_t id;
  if (InsertIntoIndexMap(bigint_ids_, *bigint, id)) return;

  DCHECK_EQ(id, bigints_->Length());
  bigints_ = ArrayList::Add(isolate_, bigints_, bigint);
}

// Format (serialized function):
// - 0 if there's no context, 1 + context id otherwise
// - String id (source snippet)
// - Start position in the source snippet
// - Length in the source snippet
// - Formal parameter count
// - Flags (see FunctionFlags)
// - 0 if there's no map, 1 + map id otherwise
// - For each function property
//   - Serialized value
// - Function prototype
// TODO(v8:11525): Investigate whether the length is really needed.
void WebSnapshotSerializer::SerializeFunction(Handle<JSFunction> function) {
  SerializeFunctionInfo(function, function_serializer_);
  SerializeFunctionProperties(function, function_serializer_);
  WriteValue(handle(function->map().prototype(), isolate_),
             function_serializer_);
}

// Format (serialized class):
// - 1 + context id
// - String id (source snippet)
// - Start position in the source snippet
// - Length in the source snippet
// - Formal parameter count
// - Flags (see FunctionFlags)
// - 0 if there's no map, 1 + map id otherwise
// - For each function property
//   - Serialized value
// - Function prototype
void WebSnapshotSerializer::SerializeClass(Handle<JSFunction> function) {
  SerializeFunctionInfo(function, class_serializer_);
  SerializeFunctionProperties(function, class_serializer_);
  WriteValue(handle(function->map().prototype(), isolate_), class_serializer_);
  // TODO(v8:11525): Support class members.
}

// Format (serialized context):
// - 0 if there's no parent context, 1 + parent context id otherwise
// - Variable count
// - For each variable:
//   - String id (name)
// - For each variable:
//   - Serialized value
void WebSnapshotSerializer::SerializeContext(Handle<Context> context,
                                             uint32_t id) {
  uint32_t parent_context_id = 0;
  if (!context->previous().IsNativeContext() &&
      !context->previous().IsScriptContext()) {
    parent_context_id = GetContextId(context->previous());
    DCHECK_LT(parent_context_id, id);
    ++parent_context_id;  // 0 is reserved for "no parent context".
  }

  // TODO(v8:11525): Use less space for encoding the context type.
  if (context->IsFunctionContext()) {
    context_serializer_.WriteUint32(ContextType::FUNCTION);
  } else if (context->IsBlockContext()) {
    context_serializer_.WriteUint32(ContextType::BLOCK);
  } else {
    Throw("Unsupported context type");
    return;
  }

  context_serializer_.WriteUint32(parent_context_id);

  Handle<ScopeInfo> scope_info(context->scope_info(), isolate_);
  int count = scope_info->ContextLocalCount();
  context_serializer_.WriteUint32(count);

  for (auto it : ScopeInfo::IterateLocalNames(scope_info)) {
    // TODO(v8:11525): support parameters
    // TODO(v8:11525): distinguish variable modes
    WriteStringId(handle(it->name(), isolate_), context_serializer_);
  }
  for (auto it : ScopeInfo::IterateLocalNames(scope_info)) {
    Handle<Object> value(
        context->get(scope_info->ContextHeaderLength() + it->index()),
        isolate_);
    WriteValue(value, context_serializer_);
  }
}

template <typename T>
void WebSnapshotSerializer::SerializeObjectPropertiesWithDictionaryMap(T dict) {
  DisallowGarbageCollection no_gc;

  std::vector<uint8_t> attributes;
  attributes.reserve(dict->NumberOfElements());
  HandleScope scope(isolate_);
  int first_custom_index = -1;

  ReadOnlyRoots roots(isolate_);
  for (InternalIndex index : dict->IterateEntries()) {
    if (!dict->IsKey(roots, dict->KeyAt(index))) {
      continue;
    }
    PropertyDetails details = dict->DetailsAt(index);
    if (first_custom_index >= 0 || details.IsReadOnly() ||
        !details.IsConfigurable() || details.IsDontEnum()) {
      if (first_custom_index == -1) first_custom_index = index.as_int();
      attributes.push_back(AttributesToFlags(details));
    }
  }
  object_serializer_.WriteUint32(first_custom_index == -1
                                     ? PropertyAttributesType::DEFAULT
                                     : PropertyAttributesType::CUSTOM);
  object_serializer_.WriteUint32(dict->NumberOfElements());

  uint8_t default_flags = GetDefaultAttributeFlags();
  for (InternalIndex index : dict->IterateEntries()) {
    Object key = dict->KeyAt(index);
    if (!dict->IsKey(roots, key)) {
      continue;
    }
    WriteValue(handle(key, isolate_), object_serializer_);
    WriteValue(handle(dict->ValueAt(index), isolate_), object_serializer_);
    if (first_custom_index >= 0) {
      if (index.as_int() < first_custom_index) {
        object_serializer_.WriteByte(default_flags);
      } else {
        object_serializer_.WriteByte(
            attributes[index.as_int() - first_custom_index]);
      }
    }
  }
}

// Format (serialized object):
// - 0 if there's no shape (dictionary map), 1 + shape id otherwise
// If has shape
//   - For each property:
//     - Serialized value
// Else (dictionary map)
//   - PropertyAttributesType
//   - Property count
//   - For each property
//     - Name: STRING_ID + String id or SYMBOL_ID + Symbol id or in-place string
//     - Serialized value
//     - If the PropertyAttributesType is CUSTOM: attributes
//   - __proto__: serialized value
// - Elements (see serialized array)
// TODO(v8:11525): Support packed elements with a denser format.
void WebSnapshotSerializer::SerializeObject(Handle<JSObject> object) {
  Handle<Map> map(object->map(), isolate_);
  if (map->is_dictionary_map()) {
    object_serializer_.WriteUint32(0);
  } else {
    uint32_t map_id = GetMapId(*map);
    object_serializer_.WriteUint32(map_id + 1);
  }

  if (object->HasFastProperties()) {
    // Properties.
    for (InternalIndex i : map->IterateOwnDescriptors()) {
      PropertyDetails details =
          map->instance_descriptors(kRelaxedLoad).GetDetails(i);
      FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
      Handle<Object> value = JSObject::FastPropertyAt(
          isolate_, object, details.representation(), field_index);
      WriteValue(value, object_serializer_);
    }
  } else {
    if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
      Handle<SwissNameDictionary> swiss_dictionary =
          handle(object->property_dictionary_swiss(), isolate_);
      SerializeObjectPropertiesWithDictionaryMap(swiss_dictionary);
    } else {
      Handle<NameDictionary> dictionary =
          handle(object->property_dictionary(), isolate_);
      SerializeObjectPropertiesWithDictionaryMap(dictionary);
    }
    WriteValue(handle(map->prototype(), isolate_), object_serializer_);
  }

  // Elements.
  SerializeElements(object, object_serializer_);
}

// Format (serialized array):
// - Elements type (dense or sparse)
// - Length
// If dense array
//   - For each element:
//     - Serialized value
// If sparse array
//   - For each element:
//     - Element index
//     - Serialized value
void WebSnapshotSerializer::SerializeArray(Handle<JSArray> array) {
  SerializeElements(array, array_serializer_);
}

void WebSnapshotSerializer::SerializeElements(Handle<JSObject> object,
                                              ValueSerializer& serializer) {
  // TODO(v8:11525): Handle sealed & frozen elements correctly. (Also: handle
  // sealed & frozen objects.)

  // TODO(v8:11525): Sometimes it would make sense to serialize dictionary
  // mode elements as dense (the number of elements is large but the array is
  // densely filled).

  // TODO(v8:11525): Sometimes it would make sense to serialize packed mode
  // elements as sparse (if there are a considerable amount of holes in it).
  ReadOnlyRoots roots(isolate_);
  auto elements_kind = object->GetElementsKind();
  switch (elements_kind) {
    case PACKED_SMI_ELEMENTS:
    case PACKED_ELEMENTS:
    case HOLEY_SMI_ELEMENTS:
    case HOLEY_ELEMENTS:
    case PACKED_FROZEN_ELEMENTS:
    case PACKED_SEALED_ELEMENTS:
    case HOLEY_FROZEN_ELEMENTS:
    case HOLEY_SEALED_ELEMENTS: {
      serializer.WriteUint32(ElementsType::kDense);
      Handle<FixedArray> elements =
          handle(FixedArray::cast(object->elements()), isolate_);
      uint32_t length = static_cast<uint32_t>(elements->length());
      serializer.WriteUint32(length);
      for (uint32_t i = 0; i < length; ++i) {
        WriteValue(handle(elements->get(i), isolate_), serializer);
      }
      break;
    }
    case PACKED_DOUBLE_ELEMENTS:
    case HOLEY_DOUBLE_ELEMENTS: {
      serializer.WriteUint32(ElementsType::kDense);
      Handle<FixedDoubleArray> elements =
          handle(FixedDoubleArray::cast(object->elements()), isolate_);
      uint32_t length = static_cast<uint32_t>(elements->length());
      serializer.WriteUint32(length);
      for (uint32_t i = 0; i < length; ++i) {
        if (!elements->is_the_hole(i)) {
          double double_value = elements->get_scalar(i);
          Handle<Object> element_value =
              isolate_->factory()->NewNumber(double_value);
          WriteValue(element_value, serializer);
        } else {
          WriteValue(handle(roots.the_hole_value(), isolate_), serializer);
        }
      }
      break;
    }
    case DICTIONARY_ELEMENTS: {
      DisallowGarbageCollection no_gc;
      serializer.WriteUint32(ElementsType::kSparse);

      auto dict = object->element_dictionary();
      serializer.WriteUint32(dict.NumberOfElements());

      for (InternalIndex index : dict.IterateEntries()) {
        Object key = dict.KeyAt(index);
        if (!dict.IsKey(roots, key)) {
          continue;
        }
        CHECK(key.IsNumber());
        // This case is checked by DiscoverElements.
        // TODO(v8:11525): Support large element indices.
        CHECK_LE(key.Number(), std::numeric_limits<uint32_t>::max());
        uint32_t element_index = static_cast<uint32_t>(key.Number());
        serializer.WriteUint32(element_index);
        WriteValue(handle(dict.ValueAt(index), isolate_), serializer);
      }
      break;
    }
    default: {
      Throw("Unsupported elements");
      return;
    }
  }
}
uint8_t WebSnapshotSerializerDeserializer::ArrayBufferKindToFlags(
    Handle<JSArrayBuffer> array_buffer) {
  return DetachedBitField::encode(array_buffer->was_detached()) |
         SharedBitField::encode(array_buffer->is_shared()) |
         ResizableBitField::encode(array_buffer->is_resizable());
}

uint32_t WebSnapshotSerializerDeserializer::BigIntSignAndLengthToFlags(
    Handle<BigInt> bigint) {
  uint32_t bitfield = bigint->GetBitfieldForSerialization();
  int byte_length = BigInt::DigitsByteLengthForBitfield(bitfield);
  int sign = BigInt::SignBits::decode(bitfield);

  return BigIntSignBitField::encode(sign) |
         BigIntLengthBitField::encode(byte_length);
}

uint32_t WebSnapshotSerializerDeserializer::BigIntFlagsToBitField(
    uint32_t flags) {
  int byte_length = BigIntLengthBitField::decode(flags);
  int sign = BigIntSignBitField::decode(flags);
  return BigInt::SignBits::encode(sign) |
         BigInt::LengthBits::encode(byte_length);
}

// Format (serialized array buffer):
// - ArrayBufferFlags, including was_detached, is_shared and is_resizable.
// - Byte length
// - if is_resizable
//   - Max byte length
// - Raw bytes
void WebSnapshotSerializer::SerializeArrayBuffer(
    Handle<JSArrayBuffer> array_buffer) {
  size_t byte_length = array_buffer->GetByteLength();
  if (byte_length > std::numeric_limits<uint32_t>::max()) {
    Throw("Too large array buffer");
    return;
  }
  array_buffer_serializer_.WriteByte(ArrayBufferKindToFlags(array_buffer));

  array_buffer_serializer_.WriteUint32(static_cast<uint32_t>(byte_length));
  if (array_buffer->is_resizable()) {
    size_t max_byte_length = array_buffer->max_byte_length();
    if (max_byte_length > std::numeric_limits<uint32_t>::max()) {
      Throw("Too large resizable array buffer");
      return;
    }
    array_buffer_serializer_.WriteUint32(
        static_cast<uint32_t>(array_buffer->max_byte_length()));
  }
  array_buffer_serializer_.WriteRawBytes(array_buffer->backing_store(),
                                         byte_length);
}

uint8_t WebSnapshotSerializerDeserializer::ArrayBufferViewKindToFlags(
    Handle<JSArrayBufferView> array_buffer_view) {
  return LengthTrackingBitField::encode(
      array_buffer_view->is_length_tracking());
}

// static
ExternalArrayType
WebSnapshotSerializerDeserializer::TypedArrayTypeToExternalArrayType(
    TypedArrayType type) {
  switch (type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case k##Type##Array:                            \
    return kExternal##Type##Array;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
}

// static
WebSnapshotSerializerDeserializer::TypedArrayType
WebSnapshotSerializerDeserializer::ExternalArrayTypeToTypedArrayType(
    ExternalArrayType type) {
  switch (type) {
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype) \
  case kExternal##Type##Array:                    \
    return TypedArrayType::k##Type##Array;
    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
  }
  UNREACHABLE();
}

// Format (serialized array buffer view):
// - Serialized ArrayBuffer
// - ArrayBufferViewFlags, including is_length_tracking
// - Byte offset
// - If not is_length_tracking
//   - Byte length
void WebSnapshotSerializer::SerializeArrayBufferView(
    Handle<JSArrayBufferView> array_buffer_view, ValueSerializer& serializer) {
  WriteValue(handle(array_buffer_view->buffer(), isolate_), serializer);
  // TODO(v8:11525): Implement WriteByte.
  serializer.WriteByte(ArrayBufferViewKindToFlags(array_buffer_view));
  if (array_buffer_view->byte_offset() > std::numeric_limits<uint32_t>::max()) {
    Throw("Too large byte offset in TypedArray");
    return;
  }
  serializer.WriteUint32(
      static_cast<uint32_t>(array_buffer_view->byte_offset()));
  if (array_buffer_view->byte_length() > std::numeric_limits<uint32_t>::max()) {
    Throw("Too large byte length in TypedArray");
    return;
  }
  if (!array_buffer_view->is_length_tracking()) {
    serializer.WriteUint32(
        static_cast<uint32_t>(array_buffer_view->byte_length()));
  }
}

// Format (serialized typed array):
// - TypedArrayType
// - ArrayBufferView
void WebSnapshotSerializer::SerializeTypedArray(
    Handle<JSTypedArray> typed_array) {
  TypedArrayType typed_array_type =
      ExternalArrayTypeToTypedArrayType(typed_array->type());
  typed_array_serializer_.WriteUint32(typed_array_type);
  SerializeArrayBufferView(typed_array, typed_array_serializer_);
}

// Format (serialized data view):
// - ArrayBufferView
void WebSnapshotSerializer::SerializeDataView(Handle<JSDataView> data_view) {
  SerializeArrayBufferView(data_view, data_view_serializer_);
}

// Format (serialized export):
// - String id (export name)
// - Serialized value (export value)
void WebSnapshotSerializer::SerializeExport(Handle<Object> object,
                                            Handle<String> export_name) {
  ++export_count_;
  WriteStringId(export_name, export_serializer_);
  if (object->IsJSPrimitiveWrapper()) {
    Handle<JSPrimitiveWrapper> wrapper =
        Handle<JSPrimitiveWrapper>::cast(object);
    Handle<Object> export_value = handle(wrapper->value(), isolate_);
    WriteValue(export_value, export_serializer_);
  } else {
    WriteValue(object, export_serializer_);
  }
}

// Format (serialized value):
// - Type id (ValueType enum)
// - Value or id (interpretation depends on the type)
void WebSnapshotSerializer::WriteValue(Handle<Object> object,
                                       ValueSerializer& serializer) {
  if (object->IsSmi()) {
    serializer.WriteByte(ValueType::INTEGER);
    serializer.WriteZigZag<int32_t>(Smi::cast(*object).value());
    return;
  }

  uint32_t id;
  if (GetExternalId(HeapObject::cast(*object), &id)) {
    serializer.WriteByte(ValueType::EXTERNAL_ID);
    serializer.WriteUint32(id);
    return;
  }

  if (GetBuiltinObjectId(HeapObject::cast(*object), id)) {
    serializer.WriteByte(ValueType::BUILTIN_OBJECT_ID);
    serializer.WriteUint32(id);
    return;
  }

  DCHECK(object->IsHeapObject());
  Handle<HeapObject> heap_object = Handle<HeapObject>::cast(object);
  switch ((*heap_object).map().instance_type()) {
    case ODDBALL_TYPE:
      switch (Oddball::cast(*heap_object).kind()) {
        case Oddball::kFalse:
          serializer.WriteByte(ValueType::FALSE_CONSTANT);
          return;
        case Oddball::kTrue:
          serializer.WriteByte(ValueType::TRUE_CONSTANT);
          return;
        case Oddball::kNull:
          serializer.WriteByte(ValueType::NULL_CONSTANT);
          return;
        case Oddball::kUndefined:
          serializer.WriteByte(ValueType::UNDEFINED_CONSTANT);
          return;
        case Oddball::kTheHole:
          serializer.WriteByte(ValueType::NO_ELEMENT_CONSTANT);
          return;
        default:
          UNREACHABLE();
      }
    case HEAP_NUMBER_TYPE:
      // TODO(v8:11525): Handle possible endianness mismatch.
      serializer.WriteByte(ValueType::DOUBLE);
      serializer.WriteDouble(HeapNumber::cast(*heap_object).value());
      break;
    case JS_FUNCTION_TYPE:
      serializer.WriteByte(ValueType::FUNCTION_ID);
      serializer.WriteUint32(GetFunctionId(JSFunction::cast(*heap_object)));
      break;
    case JS_CLASS_CONSTRUCTOR_TYPE:
      serializer.WriteByte(ValueType::CLASS_ID);
      serializer.WriteUint32(GetClassId(JSFunction::cast(*heap_object)));
      break;
    case JS_OBJECT_TYPE:
      serializer.WriteByte(ValueType::OBJECT_ID);
      serializer.WriteUint32(GetObjectId(JSObject::cast(*heap_object)));
      break;
    case JS_ARRAY_TYPE:
      serializer.WriteByte(ValueType::ARRAY_ID);
      serializer.WriteUint32(GetArrayId(JSArray::cast(*heap_object)));
      break;
    case SYMBOL_TYPE:
      serializer.WriteByte(ValueType::SYMBOL_ID);
      serializer.WriteUint32(GetSymbolId(Symbol::cast(*heap_object)));
      break;
    case BIGINT_TYPE:
      serializer.WriteByte(ValueType::BIGINT_ID);
      serializer.WriteUint32(GetBigIntId(BigInt::cast(*heap_object)));
      break;
    case JS_REG_EXP_TYPE: {
      Handle<JSRegExp> regexp = Handle<JSRegExp>::cast(heap_object);
      if (regexp->map() != isolate_->regexp_function()->initial_map()) {
        Throw("Unsupported RegExp map");
        return;
      }
      serializer.WriteByte(ValueType::REGEXP);
      Handle<String> pattern = handle(regexp->source(), isolate_);
      WriteStringId(pattern, serializer);
      Handle<String> flags_string =
          JSRegExp::StringFromFlags(isolate_, regexp->flags());
      WriteStringId(flags_string, serializer);
      break;
    }
    case JS_ARRAY_BUFFER_TYPE: {
      Handle<JSArrayBuffer> array_buffer =
          Handle<JSArrayBuffer>::cast(heap_object);
      serializer.WriteByte(ValueType::ARRAY_BUFFER_ID);
      serializer.WriteUint32(GetArrayBufferId(*array_buffer));
      break;
    }
    case JS_TYPED_ARRAY_TYPE: {
      Handle<JSTypedArray> typed_array =
          Handle<JSTypedArray>::cast(heap_object);
      serializer.WriteByte(ValueType::TYPED_ARRAY_ID);
      serializer.WriteUint32(GetTypedArrayId(*typed_array));
      break;
    }
    case JS_DATA_VIEW_TYPE: {
      Handle<JSDataView> data_view = Handle<JSDataView>::cast(heap_object);
      serializer.WriteUint32(ValueType::DATA_VIEW_ID);
      serializer.WriteUint32(GetDataViewId(*data_view));
      break;
    }
    default:
      if (heap_object->IsString()) {
        // Write strings which are referred to only once as in-place strings.
        WriteStringMaybeInPlace(Handle<String>::cast(heap_object), serializer);
      } else {
        Throw("Unsupported object");
      }
  }
  // TODO(v8:11525): Support more types.
}

void WebSnapshotSerializer::WriteStringMaybeInPlace(
    Handle<String> string, ValueSerializer& serializer) {
  // If the string is only referred to by one location, write it in-place.
  bool in_place = false;
  uint32_t id = GetStringId(string, in_place);
  if (in_place) {
    serializer.WriteByte(ValueType::IN_PLACE_STRING_ID);
    SerializeString(string, serializer);
  } else {
    serializer.WriteByte(ValueType::STRING_ID);
    serializer.WriteUint32(id);
  }
}

void WebSnapshotSerializer::WriteStringId(Handle<String> string,
                                          ValueSerializer& serializer) {
  bool in_place = false;
  uint32_t id = GetStringId(string, in_place);
  CHECK(!in_place);  // The string must have an ID.
  serializer.WriteUint32(id);
}

uint32_t WebSnapshotSerializer::GetStringId(Handle<String> string,
                                            bool& in_place) {
  // Internalize strings so that they're unique.
  string = factory()->InternalizeString(string);

  // Strings referred to more than one places are inserted in string_ids_.
  // Strings referred to by only one place aren't.
#ifdef DEBUG
  auto result = all_strings_.FindOrInsert(string);
  DCHECK(result.already_exists);
#endif
  int id = 0;
  in_place = !string_ids_.Lookup(*string, &id);
  return static_cast<uint32_t>(id);
}

uint32_t WebSnapshotSerializer::GetSymbolId(Symbol symbol) {
  int id;
  bool return_value = symbol_ids_.Lookup(symbol, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(id);
}

uint32_t WebSnapshotSerializer::GetBigIntId(BigInt bigint) {
  int id;
  bool return_value = bigint_ids_.Lookup(bigint, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(id);
}

uint32_t WebSnapshotSerializer::GetMapId(Map map) {
  int id;
  bool return_value = map_ids_.Lookup(map, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(id);
}

uint32_t WebSnapshotSerializer::GetFunctionId(JSFunction function) {
  int id;
  bool return_value = function_ids_.Lookup(function, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(function_ids_.size() - 1 - id);
}

uint32_t WebSnapshotSerializer::GetClassId(JSFunction function) {
  int id;
  bool return_value = class_ids_.Lookup(function, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(class_ids_.size() - 1 - id);
}

uint32_t WebSnapshotSerializer::GetContextId(Context context) {
  int id;
  bool return_value = context_ids_.Lookup(context, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(id);
}

uint32_t WebSnapshotSerializer::GetArrayId(JSArray array) {
  int id;
  bool return_value = array_ids_.Lookup(array, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(array_ids_.size() - 1 - id);
}

uint32_t WebSnapshotSerializer::GetTypedArrayId(JSTypedArray typed_array) {
  int id;
  bool return_value = typed_array_ids_.Lookup(typed_array, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(typed_array_ids_.size() - 1 - id);
}

uint32_t WebSnapshotSerializer::GetDataViewId(JSDataView data_view) {
  int id;
  bool return_value = data_view_ids_.Lookup(data_view, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(data_view_ids_.size() - 1 - id);
}

uint32_t WebSnapshotSerializer::GetArrayBufferId(JSArrayBuffer array_buffer) {
  int id;
  bool return_value = array_buffer_ids_.Lookup(array_buffer, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(array_buffer_ids_.size() - 1 - id);
}

uint32_t WebSnapshotSerializer::GetObjectId(JSObject object) {
  int id;
  bool return_value = object_ids_.Lookup(object, &id);
  DCHECK(return_value);
  USE(return_value);
  return static_cast<uint32_t>(object_ids_.size() - 1 - id);
}

bool WebSnapshotSerializer::GetExternalId(HeapObject object, uint32_t* id) {
  int id_int;
  bool return_value = external_object_ids_.Lookup(object, &id_int);
  if (id != nullptr) {
    *id = static_cast<uint32_t>(id_int);
  }
  return return_value;
}

bool WebSnapshotSerializer::GetBuiltinObjectNameIndex(HeapObject object,
                                                      uint32_t& index) {
  int index_int = 0;
  bool return_value = builtin_object_to_name_.Lookup(object, &index_int);
  index = static_cast<uint32_t>(index_int);
  return return_value;
}

bool WebSnapshotSerializer::GetBuiltinObjectId(HeapObject object,
                                               uint32_t& id) {
  int id_int;
  bool return_value = builtin_object_ids_.Lookup(object, &id_int);
  id = static_cast<uint32_t>(id_int);
  return return_value;
}

Handle<FixedArray> WebSnapshotSerializer::GetExternals() {
  return external_object_ids_.Values(isolate_);
}

WebSnapshotDeserializer::WebSnapshotDeserializer(v8::Isolate* isolate,
                                                 const uint8_t* data,
                                                 size_t buffer_size)
    : WebSnapshotDeserializer(reinterpret_cast<i::Isolate*>(isolate),
                              Handle<Object>(), {data, buffer_size}) {}

WebSnapshotDeserializer::WebSnapshotDeserializer(
    Isolate* isolate, Handle<Script> snapshot_as_script)
    : WebSnapshotSerializerDeserializer(isolate),
      script_name_(handle(snapshot_as_script->name(), isolate_)),
      roots_(isolate) {
  auto [data, buffer_size, buffer_owned] =
      ExtractScriptBuffer(isolate, snapshot_as_script);
  deserializer_.reset(new ValueDeserializer(isolate_, data, buffer_size));
  if (buffer_owned) {
    owned_data_.reset(data);
  }
}

WebSnapshotDeserializer::WebSnapshotDeserializer(
    Isolate* isolate, Handle<Object> script_name,
    base::Vector<const uint8_t> buffer)
    : WebSnapshotSerializerDeserializer(isolate),
      script_name_(script_name),
      deserializer_(
          new ValueDeserializer(isolate_, buffer.data(), buffer.length())),
      roots_(isolate) {
  Handle<FixedArray> empty_array = factory()->empty_fixed_array();
  strings_handle_ = empty_array;
  symbols_handle_ = empty_array;
  bigints_handle_ = empty_array;
  builtin_objects_handle_ = empty_array;
  maps_handle_ = empty_array;
  contexts_handle_ = empty_array;
  functions_handle_ = empty_array;
  classes_handle_ = empty_array;
  arrays_handle_ = empty_array;
  array_buffers_handle_ = empty_array;
  typed_arrays_handle_ = empty_array;
  data_views_handle_ = empty_array;
  objects_handle_ = empty_array;
  external_references_handle_ = empty_array;
  isolate_->heap()->AddGCEpilogueCallback(UpdatePointersCallback,
                                          v8::kGCTypeAll, this);
}

WebSnapshotDeserializer::~WebSnapshotDeserializer() {
  isolate_->heap()->RemoveGCEpilogueCallback(UpdatePointersCallback, this);
}

void WebSnapshotDeserializer::UpdatePointers() {
  strings_ = *strings_handle_;
  symbols_ = *symbols_handle_;
  bigints_ = *bigints_handle_;
  builtin_objects_ = *builtin_objects_handle_;
  maps_ = *maps_handle_;
  contexts_ = *contexts_handle_;
  functions_ = *functions_handle_;
  classes_ = *classes_handle_;
  arrays_ = *arrays_handle_;
  array_buffers_ = *array_buffers_handle_;
  typed_arrays_ = *typed_arrays_handle_;
  data_views_ = *data_views_handle_;
  objects_ = *objects_handle_;
  external_references_ = *external_references_handle_;
}

// static
std::tuple<const uint8_t*, uint32_t, bool>
WebSnapshotDeserializer::ExtractScriptBuffer(
    Isolate* isolate, Handle<Script> snapshot_as_script) {
  Handle<String> source =
      handle(String::cast(snapshot_as_script->source()), isolate);
  if (source->IsExternalOneByteString()) {
    const v8::String::ExternalOneByteStringResource* resource =
        ExternalOneByteString::cast(*source).resource();
    return std::make_tuple(reinterpret_cast<const uint8_t*>(resource->data()),
                           resource->length(), false);
  } else if (source->IsSeqOneByteString()) {
    SeqOneByteString source_as_seq = SeqOneByteString::cast(*source);
    size_t length = source_as_seq.length();
    std::unique_ptr<uint8_t[]> data_copy(new uint8_t[length]);
    {
      DisallowGarbageCollection no_gc;
      uint8_t* data = source_as_seq.GetChars(no_gc);
      memcpy(data_copy.get(), data, length);
    }
    return std::make_tuple(data_copy.release(), length, true);
  } else if (source->IsExternalTwoByteString()) {
    // TODO(v8:11525): Implement end-to-end snapshot processing which gets rid
    // of the need to copy the data here.
    const v8::String::ExternalStringResource* resource =
        ExternalTwoByteString::cast(*source).resource();
    size_t length = resource->length();
    std::unique_ptr<uint8_t[]> data_copy(new uint8_t[length]);
    {
      DisallowGarbageCollection no_gc;
      const uint16_t* data = resource->data();
      uint8_t* data_copy_ptr = data_copy.get();
      for (size_t i = 0; i < length; ++i) {
        data_copy_ptr[i] = static_cast<uint8_t>(data[i]);
      }
    }
    return std::make_tuple(data_copy.release(), length, true);
  } else if (source->IsSeqTwoByteString()) {
    SeqTwoByteString source_as_seq = SeqTwoByteString::cast(*source);
    size_t length = source_as_seq.length();
    std::unique_ptr<uint8_t[]> data_copy(new uint8_t[length]);
    {
      DisallowGarbageCollection no_gc;
      uint16_t* data = source_as_seq.GetChars(no_gc);
      uint8_t* data_copy_ptr = data_copy.get();
      for (size_t i = 0; i < length; ++i) {
        data_copy_ptr[i] = static_cast<uint8_t>(data[i]);
      }
    }
    return std::make_tuple(data_copy.release(), length, true);
  }
  UNREACHABLE();
}

void WebSnapshotDeserializer::Throw(const char* message) {
  string_count_ = 0;
  symbol_count_ = 0;
  bigint_count_ = 0;
  map_count_ = 0;
  builtin_object_count_ = 0;
  context_count_ = 0;
  class_count_ = 0;
  function_count_ = 0;
  object_count_ = 0;
  deferred_references_->SetLength(0);

  // Make sure we don't read any more data
  deserializer_->position_ = deserializer_->end_;

  WebSnapshotSerializerDeserializer::Throw(message);
}

bool WebSnapshotDeserializer::Deserialize(
    MaybeHandle<FixedArray> external_references, bool skip_exports) {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize);
  if (external_references.ToHandle(&external_references_handle_)) {
    external_references_ = *external_references_handle_;
  } else {
    external_references_handle_ = roots_.empty_fixed_array_handle();
  }

  if (deserialized_) {
    Throw("Can't reuse WebSnapshotDeserializer");
    return false;
  }
  deserialized_ = true;
  auto buffer_size = deserializer_->end_ - deserializer_->position_;

  base::ElapsedTimer timer;
  if (FLAG_trace_web_snapshot) {
    timer.Start();
  }
  if (!DeserializeSnapshot(skip_exports)) {
    return false;
  }
  if (!DeserializeScript()) {
    return false;
  }

  if (FLAG_trace_web_snapshot) {
    double ms = timer.Elapsed().InMillisecondsF();
    PrintF("[Deserializing snapshot (%zu bytes) took %0.3f ms]\n", buffer_size,
           ms);
  }
  return true;
}

#ifdef VERIFY_HEAP
void WebSnapshotDeserializer::VerifyObjects() {
  for (int i = 0; i < strings_.length(); i++) {
    String::cast(strings_.get(i)).StringVerify(isolate_);
  }
  for (int i = 0; i < symbols_.length(); i++) {
    Symbol::cast(symbols_.get(i)).SymbolVerify(isolate_);
  }
  for (int i = 0; i < builtin_objects_.length(); i++) {
    builtin_objects_.get(i).ObjectVerify(isolate_);
  }
  for (int i = 0; i < maps_.length(); i++) {
    Map::cast(maps_.get(i)).MapVerify(isolate_);
  }
  for (int i = 0; i < contexts_.length(); i++) {
    Context::cast(contexts_.get(i)).ContextVerify(isolate_);
  }
  for (int i = 0; i < functions_.length(); i++) {
    JSFunction::cast(functions_.get(i)).JSFunctionVerify(isolate_);
  }
  for (int i = 0; i < arrays_.length(); i++) {
    JSArray::cast(arrays_.get(i)).JSArrayVerify(isolate_);
  }
  for (int i = 0; i < array_buffers_.length(); i++) {
    JSArrayBuffer::cast(array_buffers_.get(i)).JSArrayBufferVerify(isolate_);
  }
  for (int i = 0; i < typed_arrays_.length(); i++) {
    JSTypedArray::cast(typed_arrays_.get(i)).JSTypedArrayVerify(isolate_);
  }
  for (int i = 0; i < data_views_.length(); i++) {
    JSDataView::cast(data_views_.get(i)).JSDataViewVerify(isolate_);
  }
  for (int i = 0; i < objects_.length(); i++) {
    JSObject::cast(objects_.get(i)).JSObjectVerify(isolate_);
  }
  for (int i = 0; i < classes_.length(); i++) {
    JSFunction::cast(classes_.get(i)).JSFunctionVerify(isolate_);
  }
}
#endif

bool WebSnapshotDeserializer::DeserializeSnapshot(bool skip_exports) {
  CollectBuiltinObjects();

  deferred_references_ = ArrayList::New(isolate_, 30);

  const void* magic_bytes;
  if (!deserializer_->ReadRawBytes(sizeof(kMagicNumber), &magic_bytes) ||
      memcmp(magic_bytes, kMagicNumber, sizeof(kMagicNumber)) != 0) {
    Throw("Invalid magic number");
    return false;
  }

  DeserializeStrings();
  DeserializeSymbols();
  DeserializeBigInts();
  DeserializeBuiltinObjects();
  DeserializeMaps();
  DeserializeContexts();
  DeserializeFunctions();
  DeserializeArrays();
  DeserializeArrayBuffers();
  DeserializeTypedArrays();
  DeserializeDataViews();
  DeserializeObjects();
  DeserializeClasses();
  ProcessDeferredReferences();
  DeserializeExports(skip_exports);
  DCHECK_EQ(0, deferred_references_->Length());

#ifdef VERIFY_HEAP
  // Verify the objects we produced during deserializing snapshot.
  if (FLAG_verify_heap && !has_error()) {
    VerifyObjects();
  }
#endif

  return !has_error();
}

void WebSnapshotDeserializer::CollectBuiltinObjects() {
  // TODO(v8:11525): Look up the builtin objects from the global object.
  builtin_object_name_to_object_ =
      ObjectHashTable::New(isolate_, kBuiltinObjectCount);
#if DEBUG
  int i = 0;
#endif
  IterateBuiltinObjects([&](Handle<String> name, Handle<HeapObject> object) {
    auto new_builtin_object_name_to_object =
        ObjectHashTable::Put(builtin_object_name_to_object_, name, object);
    USE(new_builtin_object_name_to_object);
    // We preallocated the correct size, so the hash table doesn't grow.
    DCHECK_EQ(*new_builtin_object_name_to_object,
              *builtin_object_name_to_object_);
#if DEBUG
    ++i;
#endif
  });
  DCHECK_EQ(i, kBuiltinObjectCount);
}

bool WebSnapshotDeserializer::DeserializeScript() {
  // If there is more data, treat it as normal JavaScript.
  DCHECK_LE(deserializer_->position_, deserializer_->end_);
  auto remaining_bytes = deserializer_->end_ - deserializer_->position_;
  if (remaining_bytes > 0 && remaining_bytes < v8::String::kMaxLength) {
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
    v8::Local<v8::String> source =
        v8::String::NewFromUtf8(
            v8_isolate, reinterpret_cast<const char*>(deserializer_->position_),
            NewStringType::kNormal, static_cast<int>(remaining_bytes))
            .ToLocalChecked();

    ScriptOrigin origin(v8_isolate, Utils::ToLocal(script_name_));

    ScriptCompiler::Source script_source(source, origin);
    Local<UnboundScript> script;
    if (!ScriptCompiler::CompileUnboundScript(v8_isolate, &script_source)
             .ToLocal(&script)) {
      // The exception has already been reported.
      DCHECK(!isolate_->has_pending_exception());
      return false;
    }
    Local<Value> result;
    if (!script->BindToCurrentContext()
             ->Run(v8_isolate->GetCurrentContext())
             .ToLocal(&result)) {
      // The exception has already been reported.
      DCHECK(!isolate_->has_pending_exception());
      return false;
    }
  }

  return !has_error();
}

void WebSnapshotDeserializer::DeserializeStrings() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Strings);
  if (!ReadCount(string_count_)) {
    Throw("Malformed string table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  strings_handle_ = factory()->NewFixedArray(string_count_);
  strings_ = *strings_handle_;
  for (uint32_t i = 0; i < string_count_; ++i) {
    MaybeHandle<String> maybe_string =
        deserializer_->ReadUtf8String(AllocationType::kOld);
    Handle<String> string;
    if (!maybe_string.ToHandle(&string)) {
      Throw("Malformed string");
      return;
    }
    strings_.set(i, *string);
  }
}

String WebSnapshotDeserializer::ReadString(
    InternalizeStrings internalize_strings) {
  DCHECK(!strings_handle_->is_null());
  uint32_t string_id;
  if (!deserializer_->ReadUint32(&string_id) || string_id >= string_count_) {
    Throw("malformed string id\n");
    return roots_.empty_string();
  }
  String string = String::cast(strings_.get(string_id));
  if (internalize_strings == InternalizeStrings::kYes &&
      !string.IsInternalizedString(isolate_)) {
    string = *factory()->InternalizeString(handle(string, isolate_));
    strings_.set(string_id, string);
  }
  return string;
}

String WebSnapshotDeserializer::ReadInPlaceString(
    InternalizeStrings internalize_strings) {
  MaybeHandle<String> maybe_string =
      deserializer_->ReadUtf8String(AllocationType::kOld);
  Handle<String> string;
  if (!maybe_string.ToHandle(&string)) {
    Throw("Malformed string");
    return roots_.empty_string();
  }
  if (internalize_strings == InternalizeStrings::kYes) {
    string = factory()->InternalizeString(string);
  }
  return *string;
}

Object WebSnapshotDeserializer::ReadSymbol() {
  DCHECK(!symbols_handle_->is_null());
  uint32_t symbol_id;
  if (!deserializer_->ReadUint32(&symbol_id) || symbol_id >= symbol_count_) {
    Throw("malformed symbol id\n");
    return roots_.undefined_value();
  }
  return symbols_.get(symbol_id);
}

Object WebSnapshotDeserializer::ReadBigInt() {
  DCHECK(!bigints_handle_->is_null());
  uint32_t bigint_id;
  if (!deserializer_->ReadUint32(&bigint_id) || bigint_id >= bigint_count_) {
    Throw("malformed bigint id\n");
    return roots_.undefined_value();
  }
  return bigints_.get(bigint_id);
}

void WebSnapshotDeserializer::DeserializeBigInts() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_BigInts);
  if (!ReadCount(bigint_count_)) {
    Throw("Malformed bigint table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  bigints_handle_ = factory()->NewFixedArray(bigint_count_);
  bigints_ = *bigints_handle_;
  for (uint32_t i = 0; i < bigint_count_; ++i) {
    uint32_t flags;
    if (!deserializer_->ReadUint32(&flags)) {
      Throw("malformed bigint flag");
      return;
    }
    int byte_length = BigIntLengthBitField::decode(flags);
    base::Vector<const uint8_t> digits_storage;
    if (!deserializer_->ReadRawBytes(byte_length).To(&digits_storage)) {
      Throw("malformed bigint");
      return;
    }
    Handle<BigInt> bigint;
    // BigIntFlags are browser independent, so we explicity convert BigIntFlags
    // to BigInt bitfield here though they are same now.
    if (!BigInt::FromSerializedDigits(isolate_, BigIntFlagsToBitField(flags),
                                      digits_storage)
             .ToHandle(&bigint)) {
      Throw("malformed bigint");
      return;
    }
    bigints_.set(i, *bigint);
  }
}

void WebSnapshotDeserializer::DeserializeSymbols() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Symbols);
  if (!ReadCount(symbol_count_)) {
    Throw("Malformed symbol table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  symbols_handle_ = factory()->NewFixedArray(symbol_count_);
  symbols_ = *symbols_handle_;
  for (uint32_t i = 0; i < symbol_count_; ++i) {
    uint32_t symbol_type;
    if (!deserializer_->ReadUint32(&symbol_type) || symbol_type > 2) {
      Throw("malformed symbol\n");
    }

    Handle<Symbol> symbol;
    if (symbol_type == SymbolType::kNonGlobalNoDesription) {
      symbol = factory()->NewSymbol();
    } else {  // Symbol with description
      uint32_t string_id;
      if (!deserializer_->ReadUint32(&string_id) ||
          string_id >= string_count_) {
        Throw("malformed string id\n");
      }
      if (symbol_type == SymbolType::kNonGlobal) {
        symbol = factory()->NewSymbol();
        symbol->set_description(String::cast(strings_.get(string_id)));
      } else {
        DCHECK_EQ(SymbolType::kGlobal, symbol_type);
        symbol = isolate_->SymbolFor(
            RootIndex::kPublicSymbolTable,
            handle(String::cast(strings_.get(string_id)), isolate_), false);
      }
    }
    symbols_.set(i, *symbol);
  }
}

void WebSnapshotDeserializer::DeserializeMaps() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Maps);
  if (!ReadCount(map_count_)) {
    Throw("Malformed shape table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  maps_handle_ = factory()->NewFixedArray(map_count_);
  maps_ = *maps_handle_;
  for (uint32_t i = 0; i < map_count_; ++i) {
    bool has_custom_property_attributes = ReadMapType();

    uint32_t property_count;
    if (!deserializer_->ReadUint32(&property_count)) {
      Throw("Malformed shape");
      return;
    }
    // TODO(v8:11525): Consider passing the upper bound as a param and
    // systematically enforcing it on the ValueSerializer side.
    // TODO(v8:11525): Allow "objects with map" which need to be turned to
    // dictionary mode objects.
    // TODO(v8:11525): Create map trees.
    if (property_count > kMaxNumberOfDescriptors) {
      Throw("Malformed shape: too many properties");
      return;
    }

    if (property_count == 0) {
      Handle<Map> map = DeserializeObjectPrototypeAndCreateEmptyMap();
      maps_.set(i, *map);
      continue;
    }

    Handle<DescriptorArray> descriptors =
        factory()->NewDescriptorArray(property_count, 0);
    for (InternalIndex i : InternalIndex::Range(property_count)) {
      // No deferred references here, since strings and symbols have already
      // been deserialized.
      Object key = std::get<0>(
          ReadValue(Handle<HeapObject>(), 0, InternalizeStrings::kYes));
      DisallowGarbageCollection no_gc;
      if (!key.IsName()) {
        Throw("Invalid map key");
        return;
      }
      PropertyAttributes attributes = PropertyAttributes::NONE;
      if (has_custom_property_attributes) {
        uint8_t flags;
        if (!deserializer_->ReadByte(&flags)) {
          Throw("Malformed property attributes");
          return;
        }
        attributes = FlagsToAttributes(flags);
      }
      // Use the "none" representation until we see the first object having this
      // map. At that point, modify the representation.
      Descriptor desc =
          Descriptor::DataField(isolate_, handle(Name::cast(key), isolate_),
                                i.as_int(), attributes, Representation::None());
      descriptors->Set(i, &desc);
    }
    DCHECK_EQ(descriptors->number_of_descriptors(), property_count);
    descriptors->Sort();

    Handle<Map> map = factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize,
                                        HOLEY_ELEMENTS, 0);
    map->InitializeDescriptors(isolate_, *descriptors);
    // TODO(v8:11525): Set 'constructor'.
    DeserializeObjectPrototype(map);
    maps_.set(i, *map);
  }
}

void WebSnapshotDeserializer::DeserializeBuiltinObjects() {
  RCS_SCOPE(isolate_,
            RuntimeCallCounterId::kWebSnapshotDeserialize_BuiltinObjects);
  if (!ReadCount(builtin_object_count_)) {
    Throw("Malformed builtin object table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  builtin_objects_handle_ = factory()->NewFixedArray(builtin_object_count_);
  builtin_objects_ = *builtin_objects_handle_;
  for (uint32_t i = 0; i < builtin_object_count_; ++i) {
    Handle<String> name = handle(ReadString(), isolate_);
    builtin_objects_.set(static_cast<int>(i),
                         builtin_object_name_to_object_->Lookup(name));
  }
}

void WebSnapshotDeserializer::DeserializeContexts() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Contexts);
  if (!ReadCount(context_count_)) {
    Throw("Malformed context table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  contexts_handle_ = factory()->NewFixedArray(context_count_);
  contexts_ = *contexts_handle_;
  for (uint32_t i = 0; i < context_count_; ++i) {
    uint32_t context_type;
    if (!deserializer_->ReadUint32(&context_type)) {
      Throw("Malformed context type");
      return;
    }

    uint32_t parent_context_id;
    // Parent context is serialized before child context. Note: not >= on
    // purpose, we're going to subtract 1 later.
    if (!deserializer_->ReadUint32(&parent_context_id) ||
        parent_context_id > i) {
      Throw("Malformed context");
      return;
    }

    uint32_t variable_count;
    if (!deserializer_->ReadUint32(&variable_count)) {
      Throw("Malformed context");
      return;
    }
    const bool has_inlined_local_names =
        variable_count < kScopeInfoMaxInlinedLocalNamesSize;
    // TODO(v8:11525): Enforce upper limit for variable count.
    Handle<ScopeInfo> scope_info = CreateScopeInfo(
        variable_count, parent_context_id > 0,
        static_cast<ContextType>(context_type), has_inlined_local_names);

    Handle<Context> parent_context;
    if (parent_context_id > 0) {
      parent_context =
          handle(Context::cast(contexts_.get(parent_context_id - 1)), isolate_);
      scope_info->set_outer_scope_info(parent_context->scope_info());
    } else {
      parent_context = handle(isolate_->context(), isolate_);
    }

    const int local_names_container_size =
        has_inlined_local_names ? variable_count : 1;
    const int context_local_base = ScopeInfo::kVariablePartIndex;
    const int context_local_info_base =
        context_local_base + local_names_container_size;

    for (int variable_index = 0;
         variable_index < static_cast<int>(variable_count); ++variable_index) {
      {
        String name = ReadString(InternalizeStrings::kYes);
        if (has_inlined_local_names) {
          scope_info->set(context_local_base + variable_index, name);
        } else {
          Handle<NameToIndexHashTable> local_names_hashtable(
              scope_info->context_local_names_hashtable(), isolate_);

          Handle<NameToIndexHashTable> new_table =
              NameToIndexHashTable::Add(isolate_, local_names_hashtable,
                                        handle(name, isolate_), variable_index);
          // The hash table didn't grow, since it was preallocated to
          // be large enough in CreateScopeInfo.
          DCHECK_EQ(*new_table, *local_names_hashtable);
          USE(new_table);
        }
      }
      // TODO(v8:11525): Support variable modes etc.
      uint32_t info =
          ScopeInfo::VariableModeBits::encode(VariableMode::kLet) |
          ScopeInfo::InitFlagBit::encode(
              InitializationFlag::kNeedsInitialization) |
          ScopeInfo::MaybeAssignedFlagBit::encode(
              MaybeAssignedFlag::kMaybeAssigned) |
          ScopeInfo::ParameterNumberBits::encode(
              ScopeInfo::ParameterNumberBits::kMax) |
          ScopeInfo::IsStaticFlagBit::encode(IsStaticFlag::kNotStatic);
      scope_info->set(context_local_info_base + variable_index,
                      Smi::FromInt(info));
    }

    // Allocate the FunctionContext after setting up the ScopeInfo to avoid
    // pointing to a ScopeInfo which is not set up yet.
    Handle<Context> context;
    switch (context_type) {
      case ContextType::FUNCTION:
        context = factory()->NewFunctionContext(parent_context, scope_info);
        break;
      case ContextType::BLOCK:
        context = factory()->NewBlockContext(parent_context, scope_info);
        break;
      default:
        Throw("Unsupported context type");
        return;
    }
    for (int variable_index = 0;
         variable_index < static_cast<int>(variable_count); ++variable_index) {
      int context_index = scope_info->ContextHeaderLength() + variable_index;
      Object value = std::get<0>(ReadValue(context, context_index));
      context->set(context_index, value);
    }
    contexts_.set(i, *context);
  }
}

Handle<ScopeInfo> WebSnapshotDeserializer::CreateScopeInfo(
    uint32_t variable_count, bool has_parent, ContextType context_type,
    bool has_inlined_local_names) {
  // TODO(v8:11525): Decide how to handle language modes. (The code below sets
  // the language mode as strict.)
  // TODO(v8:11525): Support (context-allocating) receiver.
  // TODO(v8:11525): Support function variable & function name.
  // TODO(v8:11525): Support classes.

  ScopeType scope_type;
  int flags =
      ScopeInfo::SloppyEvalCanExtendVarsBit::encode(false) |
      ScopeInfo::LanguageModeBit::encode(LanguageMode::kStrict) |
      ScopeInfo::DeclarationScopeBit::encode(false) |
      ScopeInfo::ReceiverVariableBits::encode(VariableAllocationInfo::NONE) |
      ScopeInfo::ClassScopeHasPrivateBrandBit::encode(false) |
      ScopeInfo::HasSavedClassVariableBit::encode(false) |
      ScopeInfo::HasNewTargetBit::encode(false) |
      ScopeInfo::FunctionVariableBits::encode(VariableAllocationInfo::NONE) |
      ScopeInfo::HasInferredFunctionNameBit::encode(false) |
      ScopeInfo::IsAsmModuleBit::encode(false) |
      ScopeInfo::HasSimpleParametersBit::encode(false) |
      ScopeInfo::FunctionKindBits::encode(FunctionKind::kNormalFunction) |
      ScopeInfo::HasOuterScopeInfoBit::encode(has_parent) |
      ScopeInfo::IsDebugEvaluateScopeBit::encode(false) |
      ScopeInfo::ForceContextAllocationBit::encode(false) |
      ScopeInfo::PrivateNameLookupSkipsOuterClassBit::encode(false) |
      ScopeInfo::HasContextExtensionSlotBit::encode(false) |
      ScopeInfo::IsReplModeScopeBit::encode(false) |
      ScopeInfo::HasLocalsBlockListBit::encode(false);
  switch (context_type) {
    case ContextType::FUNCTION:
      scope_type = ScopeType::FUNCTION_SCOPE;
      flags |= ScopeInfo::DeclarationScopeBit::encode(true) |
               ScopeInfo::HasSimpleParametersBit::encode(true);
      break;
    case ContextType::BLOCK:
      scope_type = ScopeType::CLASS_SCOPE;
      flags |= ScopeInfo::ForceContextAllocationBit::encode(true);
      break;
    default:
      // Default to a CLASS_SCOPE, so that the rest of the code can be executed
      // without failures.
      scope_type = ScopeType::CLASS_SCOPE;
      Throw("Unsupported context type");
  }
  flags |= ScopeInfo::ScopeTypeBits::encode(scope_type);
  const int local_names_container_size =
      has_inlined_local_names ? variable_count : 1;
  const int length = ScopeInfo::kVariablePartIndex +
                     (ScopeInfo::NeedsPositionInfo(scope_type)
                          ? ScopeInfo::kPositionInfoEntries
                          : 0) +
                     (has_parent ? 1 : 0) + local_names_container_size +
                     variable_count;
  Handle<NameToIndexHashTable> local_names_hashtable;
  if (!has_inlined_local_names) {
    local_names_hashtable = NameToIndexHashTable::New(isolate_, variable_count,
                                                      AllocationType::kOld);
  }
  Handle<ScopeInfo> scope_info = factory()->NewScopeInfo(length);
  {
    DisallowGarbageCollection no_gc;
    ScopeInfo raw = *scope_info;

    raw.set_flags(flags);
    DCHECK(!raw.IsEmpty());

    raw.set_context_local_count(variable_count);
    // TODO(v8:11525): Support parameters.
    raw.set_parameter_count(0);
    if (raw.HasPositionInfo()) {
      raw.SetPositionInfo(0, 0);
    }
    if (!has_inlined_local_names) {
      raw.set_context_local_names_hashtable(*local_names_hashtable);
    }
  }
  return scope_info;
}

Handle<JSFunction> WebSnapshotDeserializer::CreateJSFunction(
    int shared_function_info_index, uint32_t start_position, uint32_t length,
    uint32_t parameter_count, uint8_t flags, uint32_t context_id) {
  // TODO(v8:11525): Deduplicate the SFIs for class methods.
  FunctionKind kind = FunctionFlagsToFunctionKind(flags);
  Handle<SharedFunctionInfo> shared = factory()->NewSharedFunctionInfo(
      factory()->empty_string(), MaybeHandle<Code>(), Builtin::kCompileLazy,
      kind);
  Handle<UncompiledData> uncompiled_data =
      factory()->NewUncompiledDataWithoutPreparseData(
          roots_.empty_string_handle(), start_position,
          start_position + length);
  {
    DisallowGarbageCollection no_gc;
    SharedFunctionInfo raw = *shared;
    if (IsConciseMethod(kind)) {
      raw.set_syntax_kind(FunctionSyntaxKind::kAccessorOrMethod);
    }
    raw.set_script(*script_);
    raw.set_function_literal_id(shared_function_info_index);
    raw.set_internal_formal_parameter_count(JSParameterCount(parameter_count));
    // TODO(v8:11525): Decide how to handle language modes.
    raw.set_language_mode(LanguageMode::kStrict);
    raw.set_uncompiled_data(*uncompiled_data);
    raw.set_allows_lazy_compilation(true);
    shared_function_infos_.Set(shared_function_info_index,
                               HeapObjectReference::Weak(raw));
  }
  shared_function_info_table_ = ObjectHashTable::Put(
      shared_function_info_table_,
      handle(Smi::FromInt(start_position), isolate_),
      handle(Smi::FromInt(shared_function_info_index), isolate_));

  Handle<JSFunction> function =
      Factory::JSFunctionBuilder(isolate_, shared, isolate_->native_context())
          .Build();
  if (context_id > 0) {
    DCHECK_LT(context_id - 1, context_count_);
    // Guards raw pointer "context" below.
    DisallowHeapAllocation no_heap_access;
    Context context = Context::cast(contexts_.get(context_id - 1));
    function->set_context(context);
    shared->set_outer_scope_info(context.scope_info());
  }
  return function;
}

void WebSnapshotDeserializer::DeserializeFunctionProperties(
    Handle<JSFunction> function) {
  uint32_t map_id;
  if (!deserializer_->ReadUint32(&map_id) || map_id >= map_count_ + 1) {
    Throw("Malformed function");
    return;
  }

  if (map_id > 0) {
    map_id--;  // Subtract 1 to get the real map_id.
    Handle<Map> map(Map::cast(maps_.get(map_id)), isolate_);
    int no_properties = map->NumberOfOwnDescriptors();
    Handle<DescriptorArray> descriptors =
        handle(map->instance_descriptors(kRelaxedLoad), isolate_);
    Handle<PropertyArray> property_array =
        DeserializePropertyArray(descriptors, no_properties);
    // This function map was already deserialized completely and can be
    // directly used.
    auto iter = deserialized_function_maps_.find(map_id);
    if (iter != deserialized_function_maps_.end()) {
      function->set_map(*iter->second, kReleaseStore);
      function->set_raw_properties_or_hash(*property_array);
    } else {
      // TODO(v8:11525): In-object properties.
      Handle<Map> function_map = Map::Copy(
          isolate_, handle(function->map(), isolate_), "Web Snapshot");
      Map::EnsureDescriptorSlack(isolate_, function_map,
                                 descriptors->number_of_descriptors());
      {
        for (InternalIndex i : map->IterateOwnDescriptors()) {
          Descriptor d = Descriptor::DataField(
              isolate_, handle(descriptors->GetKey(i), isolate_),
              descriptors->GetDetails(i).field_index(),
              descriptors->GetDetails(i).attributes(),
              descriptors->GetDetails(i).representation());
          function_map->instance_descriptors().Append(&d);
          if (d.GetKey()->IsInterestingSymbol()) {
            function_map->set_may_have_interesting_symbols(true);
          }
        }
        function_map->SetNumberOfOwnDescriptors(
            function_map->NumberOfOwnDescriptors() +
            descriptors->number_of_descriptors());
        function->set_map(*function_map, kReleaseStore);
        function->set_raw_properties_or_hash(*property_array);
      }
      deserialized_function_maps_.insert(std::make_pair(map_id, function_map));
    }
  }
}

void WebSnapshotDeserializer::DeserializeFunctions() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Functions);
  if (!ReadCount(function_count_)) {
    Throw("Malformed function table");
    return;
  }
  static_assert(kMaxItemCount + 1 <= FixedArray::kMaxLength);
  functions_handle_ = factory()->NewFixedArray(function_count_);
  functions_ = *functions_handle_;

  // Overallocate the array for SharedFunctionInfos; functions which we
  // deserialize soon will create more SharedFunctionInfos when called.
  shared_function_infos_handle_ = factory()->NewWeakFixedArray(
      WeakArrayList::CapacityForLength(function_count_ + 1),
      AllocationType::kOld);
  shared_function_infos_ = *shared_function_infos_handle_;
  shared_function_info_table_ = ObjectHashTable::New(isolate_, function_count_);
  script_ = factory()->NewScript(factory()->empty_string());
  {
    DisallowGarbageCollection no_gc;
    Script raw = *script_;
    raw.set_type(Script::TYPE_WEB_SNAPSHOT);
    raw.set_shared_function_infos(shared_function_infos_);
    raw.set_shared_function_info_table(*shared_function_info_table_);
  }

  for (; current_function_count_ < function_count_; ++current_function_count_) {
    uint32_t context_id;
    // Note: > (not >= on purpose, we will subtract 1).
    if (!deserializer_->ReadUint32(&context_id) ||
        context_id > context_count_) {
      Throw("Malformed function");
      return;
    }
    {
      String source = ReadString();
      DisallowGarbageCollection no_gc;
      if (current_function_count_ == 0) {
        script_->set_source(source);
      } else {
        // TODO(v8:11525): Support multiple source snippets.
        DCHECK_EQ(script_->source(), source);
      }
    }

    uint32_t start_position;
    uint32_t length;
    uint32_t parameter_count;
    uint8_t flags;
    if (!deserializer_->ReadUint32(&start_position) ||
        !deserializer_->ReadUint32(&length) ||
        !deserializer_->ReadUint32(&parameter_count) ||
        !deserializer_->ReadByte(&flags)) {
      Throw("Malformed function");
      return;
    }

    // Index 0 is reserved for top-level shared function info (which web
    // snapshot scripts don't have).
    Handle<JSFunction> function =
        CreateJSFunction(current_function_count_ + 1, start_position, length,
                         parameter_count, flags, context_id);
    functions_.set(current_function_count_, *function);

    ReadFunctionPrototype(function);
    DeserializeFunctionProperties(function);
    DeserializeObjectPrototypeForFunction(function);
  }
}

void WebSnapshotDeserializer::DeserializeClasses() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Classes);
  if (!ReadCount(class_count_)) {
    Throw("Malformed class table");
    return;
  }
  static_assert(kMaxItemCount + 1 <= FixedArray::kMaxLength);
  classes_handle_ = factory()->NewFixedArray(class_count_);
  classes_ = *classes_handle_;

  // Grow the array for SharedFunctionInfos.
  shared_function_infos_handle_ = WeakFixedArray::EnsureSpace(
      isolate_, shared_function_infos_handle_,
      WeakArrayList::CapacityForLength(function_count_ + 1 + class_count_));
  shared_function_infos_ = *shared_function_infos_handle_;
  script_->set_shared_function_infos(shared_function_infos_);

  for (; current_class_count_ < class_count_; ++current_class_count_) {
    uint32_t context_id;
    // Note: > (not >= on purpose, we will subtract 1).
    if (!deserializer_->ReadUint32(&context_id) ||
        context_id > context_count_) {
      Throw("Malformed class");
      return;
    }

    {
      String source = ReadString();
      if (current_function_count_ + current_class_count_ == 0) {
        script_->set_source(source);
      } else {
        // TODO(v8:11525): Support multiple source snippets.
        DCHECK_EQ(script_->source(), source);
      }
    }

    uint32_t start_position;
    uint32_t length;
    uint32_t parameter_count;
    uint8_t flags;
    if (!deserializer_->ReadUint32(&start_position) ||
        !deserializer_->ReadUint32(&length) ||
        !deserializer_->ReadUint32(&parameter_count) ||
        !deserializer_->ReadByte(&flags)) {
      Throw("Malformed class");
      return;
    }

    // Index 0 is reserved for top-level shared function info (which web
    // snapshot scripts don't have).
    Handle<JSFunction> function = CreateJSFunction(
        function_count_ + current_class_count_ + 1, start_position, length,
        parameter_count, flags, context_id);

    ReadFunctionPrototype(function);
    // TODO(v8:11525): Use serialized start_position and length to add
    // ClassPositions property to class.
    DeserializeFunctionProperties(function);
    DeserializeObjectPrototypeForFunction(function);
    classes_.set(current_class_count_, *function);
  }
}

void WebSnapshotDeserializer::DeserializeObjectPrototype(Handle<Map> map) {
  auto result = ReadValue(map, 0, InternalizeStrings::kNo);
  Object prototype = std::get<0>(result);
  bool was_deferred = std::get<1>(result);
  if (!was_deferred) {
    SetPrototype(map, handle(prototype, isolate_));
  }
}

bool WebSnapshotDeserializer::IsInitialFunctionPrototype(Object prototype) {
  return prototype == isolate_->context().function_prototype() ||
         // Asyncfunction prototype.
         prototype == isolate_->context()
                          .async_function_constructor()
                          .instance_prototype() ||
         // GeneratorFunction prototype.
         prototype == JSFunction::cast(isolate_->context()
                                           .generator_function_map()
                                           .constructor_or_back_pointer())
                          .instance_prototype() ||
         // AsyncGeneratorFunction prototype
         prototype == JSFunction::cast(isolate_->context()
                                           .async_generator_function_map()
                                           .constructor_or_back_pointer())
                          .instance_prototype();
}

void WebSnapshotDeserializer::DeserializeObjectPrototypeForFunction(
    Handle<JSFunction> function) {
  Handle<Map> map(function->map(), isolate_);
  // If the function prototype is not the initial function prototype, then the
  // map must not be the canonical maps because we already copy the map when
  // deserializaing the map for the function. And so we don't need to copy the
  // map.
  // TODO(v8:11525): Ensure we create the same map tree as for non-websnapshot
  // functions + add a test.
  auto result = ReadValue(map, 0, InternalizeStrings::kNo);
  Object prototype = std::get<0>(result);
  bool was_deferred = std::get<1>(result);
  // If we got a deferred reference, the prototype cannot be a builtin; those
  // references aren't deferred.
  // TODO(v8:11525): if the object order is relaxed, it's possible to have a
  // deferred reference to Function.prototype, and we'll need to recognize and
  // handle that case.
  if (IsInitialFunctionPrototype(prototype)) {
    DCHECK(IsInitialFunctionPrototype(function->map().prototype()));
    return;
  }
  if (!was_deferred) {
    SetPrototype(map, handle(prototype, isolate_));
  }
}

Handle<Map>
WebSnapshotDeserializer::DeserializeObjectPrototypeAndCreateEmptyMap() {
  Handle<Map> map = factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize,
                                      HOLEY_ELEMENTS, 0);
  auto result = ReadValue(map, 0, InternalizeStrings::kNo);
  Object prototype = std::get<0>(result);
  bool was_deferred = std::get<1>(result);
  // If we got a deferred reference, the prototype cannot be a builtin; those
  // references aren't deferred.
  // TODO(v8:11525): if the object order is relaxed, it's possible to have a
  // deferred reference to Object.prototype, and we'll need to recognize and
  // handle that case.
  if (prototype == isolate_->context().initial_object_prototype()) {
    // TODO(v8:11525): Avoid map creation (above) in this case.
    // TODO(v8:11525): Should the __proto__ be a property of the map or a
    // property of the object? Investigate which solution is better for the
    // other JS engines.
    return handle(isolate_->native_context()->object_function().initial_map(),
                  isolate_);
  }
  if (!was_deferred) {
    SetPrototype(map, handle(prototype, isolate_));
  }
  return map;
}

void WebSnapshotDeserializer::SetPrototype(Handle<Map> map,
                                           Handle<Object> prototype) {
  if (prototype->IsJSObject()) {
    HeapObject::cast(*prototype).map().set_is_prototype_map(true);
    Map::SetPrototype(isolate_, map, Handle<JSObject>::cast(prototype));
  } else if (prototype->IsNull(isolate_)) {
    map->set_prototype(HeapObject::cast(*prototype));
  } else {
    Throw("Invalid prototype");
  }
}

template <typename T>
void WebSnapshotDeserializer::DeserializeObjectPropertiesWithDictionaryMap(
    T dict, uint32_t property_count, bool has_custom_property_attributes) {
  for (uint32_t i = 0; i < property_count; i++) {
    Handle<Object> key(std::get<0>(ReadValue(Handle<HeapObject>(), 0,
                                             InternalizeStrings::kYes)),
                       isolate_);
    if (!key->IsName()) {
      Throw("Invalid map key");
      return;
    }
    Handle<Object> value(std::get<0>(ReadValue()), isolate_);
    PropertyAttributes attributes = PropertyAttributes::NONE;
    if (has_custom_property_attributes) {
      uint8_t flags;
      if (!deserializer_->ReadByte(&flags)) {
        Throw("Malformed property attributes");
        return;
      }
      attributes = FlagsToAttributes(flags);
    }

    PropertyDetails details(PropertyKind::kData, attributes,
                            PropertyDetails::kConstIfDictConstnessTracking);
    auto new_dict =
        dict->Add(isolate_, dict, Handle<Name>::cast(key), value, details);
    // The dictionary didn't grow, since it was preallocated to be large enough
    // in DeserializeObjects.
    DCHECK_EQ(*new_dict, *dict);
    USE(new_dict);
  }
}

bool WebSnapshotDeserializer::ReadMapType() {
  uint32_t map_type;
  if (!deserializer_->ReadUint32(&map_type)) {
    Throw("Malformed shape");
    return false;
  }

  switch (map_type) {
    case PropertyAttributesType::DEFAULT:
      return false;
    case PropertyAttributesType::CUSTOM:
      return true;
    default:
      Throw("Unsupported map type");
      return false;
  }
}

Handle<PropertyArray> WebSnapshotDeserializer::DeserializePropertyArray(
    Handle<DescriptorArray> descriptors, int no_properties) {
  Handle<PropertyArray> property_array =
      factory()->NewPropertyArray(no_properties);
  for (int i = 0; i < no_properties; ++i) {
    Object value = std::get<0>(ReadValue(property_array, i));
    DisallowGarbageCollection no_gc;
    // Read the representation from the map.
    DescriptorArray raw_descriptors = *descriptors;
    PropertyDetails details = raw_descriptors.GetDetails(InternalIndex(i));
    CHECK_EQ(details.location(), PropertyLocation::kField);
    CHECK_EQ(PropertyKind::kData, details.kind());
    Representation r = details.representation();
    if (r.IsNone()) {
      // Switch over to wanted_representation.
      details = details.CopyWithRepresentation(Representation::Tagged());
      raw_descriptors.SetDetails(InternalIndex(i), details);
    } else if (!r.Equals(Representation::Tagged())) {
      // TODO(v8:11525): Support this case too.
      UNREACHABLE();
    }
    property_array->set(i, value);
  }
  return property_array;
}

void WebSnapshotDeserializer::DeserializeObjects() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Objects);
  if (!ReadCount(object_count_)) {
    Throw("Malformed objects table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  objects_handle_ = factory()->NewFixedArray(object_count_);
  objects_ = *objects_handle_;
  bool map_from_snapshot = false;
  for (; current_object_count_ < object_count_; ++current_object_count_) {
    uint32_t map_id;
    if (!deserializer_->ReadUint32(&map_id) || map_id >= map_count_ + 1) {
      Throw("Malformed object");
      return;
    }
    Handle<JSObject> object;
    if (map_id > 0) {
      map_id--;  // Subtract 1 to get the real map_id.
      Map raw_map = Map::cast(maps_.get(map_id));
      map_from_snapshot = true;
      Handle<DescriptorArray> descriptors =
          handle(raw_map.instance_descriptors(kRelaxedLoad), isolate_);
      int no_properties = raw_map.NumberOfOwnDescriptors();
      // TODO(v8:11525): In-object properties.
      Handle<Map> map(raw_map, isolate_);
      Handle<PropertyArray> property_array =
          DeserializePropertyArray(descriptors, no_properties);
      object = factory()->NewJSObjectFromMap(map);
      object->set_raw_properties_or_hash(*property_array, kRelaxedStore);
    } else {
      Handle<Map> map = factory()->NewMap(JS_OBJECT_TYPE, JSObject::kHeaderSize,
                                          HOLEY_ELEMENTS, 0);
      map->set_may_have_interesting_symbols(true);
      map->set_is_dictionary_map(true);

      bool has_custom_property_attributes = ReadMapType();

      uint32_t property_count;
      if (!deserializer_->ReadUint32(&property_count)) {
        Throw("Malformed object");
        return;
      }
      // TODO(v8:11525): Allow "non-map" objects which are small enough to have
      // a fast map.
      if (property_count <= kMaxNumberOfDescriptors) {
        Throw("Malformed object: too few properties for 'no map' object");
        return;
      }

      if (V8_ENABLE_SWISS_NAME_DICTIONARY_BOOL) {
        Handle<SwissNameDictionary> swiss_dictionary =
            isolate_->factory()->NewSwissNameDictionary(property_count);
        DeserializeObjectPropertiesWithDictionaryMap(
            swiss_dictionary, property_count, has_custom_property_attributes);
        object = factory()->NewJSObjectFromMap(map);
        object->SetProperties(*swiss_dictionary);
      } else {
        Handle<NameDictionary> dictionary =
            isolate_->factory()->NewNameDictionary(property_count);
        DeserializeObjectPropertiesWithDictionaryMap(
            dictionary, property_count, has_custom_property_attributes);
        object = factory()->NewJSObjectFromMap(map);
        object->SetProperties(*dictionary);
      }
      DeserializeObjectPrototype(map);
    }
    DCHECK(!object->is_null());

    DeserializeObjectElements(object, map_from_snapshot);
    objects_.set(static_cast<int>(current_object_count_), *object);
  }
}

void WebSnapshotDeserializer::DeserializeObjectElements(
    Handle<JSObject> object, bool map_from_snapshot) {
  auto [elements, elements_kind, length] = DeserializeElements();
  USE(length);
  // Ensure objects always get HOLEY_ELEMENTS or DICTIONARY_ELEMENTS: don't
  // change the elements kind if it's holey.
  DCHECK(object->HasHoleyElements());
  if (IsDictionaryElementsKind(elements_kind)) {
    DCHECK_GT(length, 0);
    Handle<Map> map(object->map(), isolate_);
    if (map_from_snapshot) {
      // Copy the map so that we don't end up modifying the maps coming from
      // the web snapshot, since they might get reused.
      // TODO(v8:11525): Is it reasonable to encode the elements kind to the
      // map? Investigate what other JS engines do.
      // TODO(v8:11525): Add a test where two objects share the map but have
      // different elements kinds.
      map = Map::Copy(isolate_, map, "Web Snapshot");
      object->set_map(*map, kReleaseStore);
    }
    map->set_elements_kind(elements_kind);
  }
  object->set_elements(*elements);
  DCHECK(object->HasHoleyElements() || object->HasDictionaryElements());
}

WebSnapshotDeserializer::ElementsType
WebSnapshotDeserializer::ReadElementsType() {
  uint32_t elements_type;
  if (!deserializer_->ReadUint32(&elements_type)) {
    Throw("Malformed elements type");
    return ElementsType::kDense;
  }
  if (elements_type != ElementsType::kDense &&
      elements_type != ElementsType::kSparse) {
    Throw("Unknown elements type");
    return ElementsType::kDense;
  }
  return static_cast<ElementsType>(elements_type);
}

std::tuple<Handle<FixedArrayBase>, ElementsKind, uint32_t>
WebSnapshotDeserializer::DeserializeElements() {
  uint32_t length;
  ElementsType elements_type = ReadElementsType();
  if (!deserializer_->ReadUint32(&length) || length > kMaxItemCount) {
    Throw("Malformed elements");
    return std::make_tuple(factory()->NewFixedArray(0), PACKED_SMI_ELEMENTS, 0);
  }
  if (elements_type == ElementsType::kDense) {
    // TODO(v8:11525): we need to convert the elements to dictionary mode if
    // there are too many elements for packed elements.
    return ReadDenseElements(length);
  } else {
    // TODO(v8:11525): we need to convert sparse elements to packed elements
    // (including double elements) if the elements fit into packed elements
    // kind.
    return ReadSparseElements(length);
  }
}

std::tuple<Handle<FixedArrayBase>, ElementsKind, uint32_t>
WebSnapshotDeserializer::ReadDenseElements(uint32_t length) {
  Handle<FixedArray> elements = factory()->NewFixedArray(length);
  ElementsKind elements_kind = PACKED_SMI_ELEMENTS;
  bool has_hole = false;
  bool has_non_number = false;
  for (uint32_t i = 0; i < length; ++i) {
    Object value = std::get<0>(ReadValue(elements, i));
    DisallowGarbageCollection no_gc;
    if (!value.IsSmi()) {
      elements_kind = PACKED_ELEMENTS;
    }
    if (!value.IsNumber()) {
      has_non_number = true;
    }
    if (value.IsTheHole()) {
      has_hole = true;
    }
    elements->set(static_cast<int>(i), value);
  }
  if (has_hole) {
    elements_kind =
        elements_kind == PACKED_ELEMENTS ? HOLEY_ELEMENTS : HOLEY_SMI_ELEMENTS;
  }
  // If all elements are number and not all elements are smi, we could convert
  // the array to double array.
  if (!has_non_number && !IsSmiElementsKind(elements_kind)) {
    DCHECK(IsObjectElementsKind(elements_kind));
    ElementsKind new_elements_kind =
        has_hole ? HOLEY_DOUBLE_ELEMENTS : PACKED_DOUBLE_ELEMENTS;
    Handle<FixedArrayBase> new_elements =
        isolate_->factory()->NewFixedDoubleArray(length);
    ElementsAccessor* element_accessor =
        ElementsAccessor::ForKind(new_elements_kind);
    element_accessor->CopyElements(isolate_, elements, elements_kind,
                                   new_elements, length);
    return std::make_tuple(new_elements, new_elements_kind, length);
  }
  return std::make_tuple(elements, elements_kind, length);
}

std::tuple<Handle<FixedArrayBase>, ElementsKind, uint32_t>
WebSnapshotDeserializer::ReadSparseElements(uint32_t length) {
  Handle<NumberDictionary> dict = NumberDictionary::New(isolate_, length);
  uint32_t max_element_index = 0;
  for (uint32_t i = 0; i < length; ++i) {
    uint32_t element_index;
    if (!deserializer_->ReadUint32(&element_index)) {
      Throw("Malformed element index in sparse elements");
      return std::make_tuple(dict, DICTIONARY_ELEMENTS, 0);
    }
    Object value = std::get<0>(ReadValue(dict, element_index));
    Handle<NumberDictionary> new_dict =
        dict->Set(isolate_, dict, element_index, handle(value, isolate_));
    // The number dictionary didn't grow, since it was preallocated to be
    // large enough before.
    DCHECK_EQ(*new_dict, *dict);
    USE(new_dict);
    if (element_index > max_element_index) {
      max_element_index = element_index;
    }
  }
  // Bypasses JSObject::RequireSlowElements which is fine when we're setting up
  // objects from the web snapshot.
  dict->UpdateMaxNumberKey(max_element_index, Handle<JSObject>());
  return std::make_tuple(dict, DICTIONARY_ELEMENTS, max_element_index + 1);
}

void WebSnapshotDeserializer::DeserializeArrays() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Arrays);
  if (!ReadCount(array_count_)) {
    Throw("Malformed array table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  arrays_handle_ = factory()->NewFixedArray(array_count_);
  arrays_ = *arrays_handle_;
  for (; current_array_count_ < array_count_; ++current_array_count_) {
    auto [elements, elements_kind, length] = DeserializeElements();
    Handle<JSArray> array;

    if (IsDictionaryElementsKind(elements_kind)) {
      array = isolate_->factory()->NewJSArray(0);

      Handle<Object> array_length =
          isolate_->factory()->NewNumberFromUint(length);
      Handle<Map> map =
          JSObject::GetElementsTransitionMap(array, DICTIONARY_ELEMENTS);
      array->set_length(*array_length);
      array->set_elements(*elements);
      array->set_map(*map, kReleaseStore);
    } else {
      array =
          factory()->NewJSArrayWithElements(elements, elements_kind, length);
    }
    DCHECK(!array->is_null());
    arrays_.set(static_cast<int>(current_array_count_), *array);
  }
}

void WebSnapshotDeserializer::DeserializeArrayBuffers() {
  RCS_SCOPE(isolate_,
            RuntimeCallCounterId::kWebSnapshotDeserialize_ArrayBuffers);
  if (!ReadCount(array_buffer_count_)) {
    Throw("Malformed array buffer table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  array_buffers_handle_ = factory()->NewFixedArray(array_buffer_count_);
  array_buffers_ = *array_buffers_handle_;
  for (; current_array_buffer_count_ < array_buffer_count_;
       ++current_array_buffer_count_) {
    uint8_t flags;
    uint32_t byte_length;
    if (!deserializer_->ReadByte(&flags) ||
        !deserializer_->ReadUint32(&byte_length) ||
        byte_length > static_cast<size_t>(deserializer_->end_ -
                                          deserializer_->position_)) {
      Throw("Malformed array buffer");
      return;
    }

    uint32_t mask = DetachedBitField::kMask | SharedBitField::kMask |
                    ResizableBitField::kMask;
    if ((flags | mask) != mask) {
      Throw("Malformed array buffer");
      return;
    }
    bool was_detached = DetachedBitField::decode(flags);
    CHECK_IMPLIES(was_detached, (byte_length == 0));
    SharedFlag shared = SharedBitField::decode(flags) ? SharedFlag::kShared
                                                      : SharedFlag::kNotShared;
    CHECK_IMPLIES(was_detached, (shared == SharedFlag::kNotShared));
    ResizableFlag resizable = ResizableBitField::decode(flags)
                                  ? ResizableFlag::kResizable
                                  : ResizableFlag::kNotResizable;
    uint32_t max_byte_length = byte_length;
    if (resizable == ResizableFlag::kResizable) {
      if (!deserializer_->ReadUint32(&max_byte_length)) {
        Throw("Malformed array buffer");
        return;
      }
      CHECK_GE(max_byte_length, byte_length);
    }

    Handle<Map> map;
    if (shared == SharedFlag::kNotShared) {
      map = handle(
          isolate_->raw_native_context().array_buffer_fun().initial_map(),
          isolate_);
    } else {
      map = handle(isolate_->raw_native_context()
                       .shared_array_buffer_fun()
                       .initial_map(),
                   isolate_);
    }
    Handle<JSArrayBuffer> array_buffer = Handle<JSArrayBuffer>::cast(
        isolate_->factory()->NewJSObjectFromMap(map, AllocationType::kYoung));
    array_buffer->Setup(shared, resizable, nullptr);

    std::unique_ptr<BackingStore> backing_store;
    if (was_detached) {
      array_buffer->set_was_detached(true);
    } else {
      if (resizable == ResizableFlag::kNotResizable) {
        backing_store = BackingStore::Allocate(isolate_, byte_length, shared,
                                               InitializedFlag::kUninitialized);
      } else {
        size_t page_size, initial_pages, max_pages;
        if (JSArrayBuffer::GetResizableBackingStorePageConfiguration(
                isolate_, byte_length, max_byte_length, kThrowOnError,
                &page_size, &initial_pages, &max_pages)
                .IsNothing()) {
          Throw("Create array buffer failed");
          return;
        }
        backing_store = BackingStore::TryAllocateAndPartiallyCommitMemory(
            isolate_, byte_length, max_byte_length, page_size, initial_pages,
            max_pages, WasmMemoryFlag::kNotWasm, shared);
      }
      if (!backing_store) {
        Throw("Create array buffer failed");
        return;
      }
      array_buffer->Attach(std::move(backing_store));
    }

    array_buffer->set_max_byte_length(max_byte_length);

    if (byte_length > 0) {
      memcpy(array_buffer->backing_store(), deserializer_->position_,
             byte_length);
    }
    deserializer_->position_ += byte_length;
    array_buffers_.set(static_cast<int>(current_array_buffer_count_),
                       *array_buffer);
  }
}

void WebSnapshotDeserializer::DeserializeDataViews() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_DataViews);
  if (!ReadCount(data_view_count_)) {
    Throw("Malformed data view table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  data_views_handle_ = factory()->NewFixedArray(data_view_count_);
  data_views_ = *data_views_handle_;
  for (; current_data_view_count_ < data_view_count_;
       ++current_data_view_count_) {
    Handle<JSArrayBuffer> array_buffer(
        JSArrayBuffer::cast(std::get<0>(ReadValue())), isolate_);
    uint32_t byte_offset = 0;
    uint8_t flags = 0;
    if (!deserializer_->ReadByte(&flags) ||
        !deserializer_->ReadUint32(&byte_offset)) {
      Throw("Malformed data view");
      return;
    }

    Handle<Map> map(
        isolate_->raw_native_context().data_view_fun().initial_map(), isolate_);
    uint32_t mask = LengthTrackingBitField::kMask;
    if ((flags | mask) != mask) {
      Throw("Malformed data view");
      return;
    }

    uint32_t byte_length = 0;
    bool is_length_tracking = LengthTrackingBitField::decode(flags);

    if (is_length_tracking) {
      CHECK(array_buffer->is_resizable());
    } else {
      if (!deserializer_->ReadUint32(&byte_length)) {
        Throw("Malformed data view");
        return;
      }
    }

    Handle<JSDataView> data_view =
        Handle<JSDataView>::cast(factory()->NewJSArrayBufferView(
            map, factory()->empty_fixed_array(), array_buffer, byte_offset,
            byte_length));

    {
      DisallowGarbageCollection no_gc;
      JSDataView raw_data_view = *data_view;
      JSArrayBuffer raw_array_buffer = *array_buffer;
      raw_data_view.set_data_pointer(
          isolate_, static_cast<uint8_t*>(raw_array_buffer.backing_store()) +
                        byte_offset);
      raw_data_view.set_is_length_tracking(is_length_tracking);
      raw_data_view.set_is_backed_by_rab(!raw_array_buffer.is_shared() &&
                                         raw_array_buffer.is_resizable());
    }

    data_views_.set(static_cast<int>(current_data_view_count_), *data_view);
  }
}

bool WebSnapshotDeserializer::ReadCount(uint32_t& count) {
  return deserializer_->ReadUint32(&count) && count <= kMaxItemCount;
}

void WebSnapshotDeserializer::DeserializeTypedArrays() {
  RCS_SCOPE(isolate_,
            RuntimeCallCounterId::kWebSnapshotDeserialize_TypedArrays);
  if (!ReadCount(typed_array_count_)) {
    Throw("Malformed typed array table");
    return;
  }
  static_assert(kMaxItemCount <= FixedArray::kMaxLength);
  typed_arrays_handle_ = factory()->NewFixedArray(typed_array_count_);
  typed_arrays_ = *typed_arrays_handle_;
  for (; current_typed_array_count_ < typed_array_count_;
       ++current_typed_array_count_) {
    uint32_t typed_array_type;
    if (!deserializer_->ReadUint32(&typed_array_type)) {
      Throw("Malformed array buffer");
      return;
    }
    Handle<JSArrayBuffer> array_buffer(
        JSArrayBuffer::cast(std::get<0>(ReadValue())), isolate_);
    uint32_t byte_offset = 0;
    uint8_t flags = 0;
    if (!deserializer_->ReadByte(&flags) ||
        !deserializer_->ReadUint32(&byte_offset)) {
      Throw("Malformed typed array");
      return;
    }
    size_t element_size = 0;
    ElementsKind element_kind = UINT8_ELEMENTS;
    JSTypedArray::ForFixedTypedArray(
        TypedArrayTypeToExternalArrayType(
            static_cast<TypedArrayType>(typed_array_type)),
        &element_size, &element_kind);

    Handle<Map> map(
        isolate_->raw_native_context().TypedArrayElementsKindToCtorMap(
            element_kind),
        isolate_);
    uint32_t mask = LengthTrackingBitField::kMask;
    if ((flags | mask) != mask) {
      Throw("Malformed typed array");
      return;
    }

    if (byte_offset % element_size != 0) {
      Throw("Malformed typed array");
      return;
    }

    uint32_t byte_length = 0;
    size_t length = 0;
    bool is_length_tracking = LengthTrackingBitField::decode(flags);

    if (is_length_tracking) {
      CHECK(array_buffer->is_resizable());
    } else {
      if (!deserializer_->ReadUint32(&byte_length)) {
        Throw("Malformed typed array");
        return;
      }
      if (byte_length % element_size != 0) {
        Throw("Malformed typed array");
        return;
      }
      length = byte_length / element_size;
      if (length > JSTypedArray::kMaxLength) {
        Throw("Too large typed array");
        return;
      }
    }

    bool rabGsab = array_buffer->is_resizable() &&
                   (!array_buffer->is_shared() || is_length_tracking);
    if (rabGsab) {
      map = handle(
          isolate_->raw_native_context().TypedArrayElementsKindToRabGsabCtorMap(
              element_kind),
          isolate_);
    }

    Handle<JSTypedArray> typed_array =
        Handle<JSTypedArray>::cast(factory()->NewJSArrayBufferView(
            map, factory()->empty_byte_array(), array_buffer, byte_offset,
            byte_length));

    {
      DisallowGarbageCollection no_gc;
      JSTypedArray raw = *typed_array;
      raw.set_length(length);
      raw.SetOffHeapDataPtr(isolate_, array_buffer->backing_store(),
                            byte_offset);
      raw.set_is_length_tracking(is_length_tracking);
      raw.set_is_backed_by_rab(array_buffer->is_resizable() &&
                               !array_buffer->is_shared());
    }

    typed_arrays_.set(static_cast<int>(current_typed_array_count_),
                      *typed_array);
  }
}

void WebSnapshotDeserializer::DeserializeExports(bool skip_exports) {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Exports);
  uint32_t count;
  if (!ReadCount(count)) {
    Throw("Malformed export table");
    return;
  }

  if (skip_exports) {
    // In the skip_exports mode, we read the exports but don't do anything about
    // them. This is useful for stress testing; otherwise the GlobalDictionary
    // handling below dominates.
    for (uint32_t i = 0; i < count; ++i) {
      Handle<String> export_name(ReadString(InternalizeStrings::kYes),
                                 isolate_);
      // No deferred references should occur at this point, since all objects
      // have been deserialized.
      Object export_value = std::get<0>(ReadValue());
#ifdef VERIFY_HEAP
      if (FLAG_verify_heap) {
        export_value.ObjectVerify(isolate_);
      }
#endif
      USE(export_name);
      USE(export_value);
    }
    return;
  }

  // Pre-reserve the space for the properties we're going to add to the global
  // object.
  Handle<JSGlobalObject> global = isolate_->global_object();
  Handle<GlobalDictionary> dictionary(
      global->global_dictionary(isolate_, kAcquireLoad), isolate_);

  dictionary = GlobalDictionary::EnsureCapacity(
      isolate_, dictionary, dictionary->NumberOfElements() + count,
      AllocationType::kYoung);
  bool has_exported_values = false;

  // TODO(v8:11525): The code below skips checks, in particular
  // LookupIterator::UpdateProtectors and
  // LookupIterator::ExtendingNonExtensible.
  InternalIndex entry = InternalIndex::NotFound();
  for (uint32_t i = 0; i < count; ++i) {
    Handle<String> export_name(ReadString(InternalizeStrings::kYes), isolate_);
    // No deferred references should occur at this point, since all objects have
    // been deserialized.
    Object export_value = std::get<0>(ReadValue());
#ifdef VERIFY_HEAP
    if (FLAG_verify_heap) {
      export_value.ObjectVerify(isolate_);
    }
#endif

    if (export_name->length() == 0 && i == 0) {
      // Hack: treat the first empty-string-named export value as a return value
      // from the deserializer.
      CHECK_EQ(i, 0);
      return_value_ = handle(export_value, isolate_);
      continue;
    }

    DisallowGarbageCollection no_gc;
    // Check for the correctness of the snapshot (thus far) before producing
    // something observable. TODO(v8:11525): Strictly speaking, we should
    // produce observable effects only when we know that the whole snapshot is
    // correct.
    if (has_error()) return;

    PropertyDetails property_details =
        PropertyDetails(PropertyKind::kData, NONE,
                        PropertyCell::InitialType(isolate_, export_value));
    Handle<Object> export_value_handle(export_value, isolate_);
    AllowGarbageCollection allow_gc;
    Handle<PropertyCell> transition_cell = factory()->NewPropertyCell(
        export_name, property_details, export_value_handle);
    dictionary =
        GlobalDictionary::Add(isolate_, dictionary, export_name,
                              transition_cell, property_details, &entry);
    has_exported_values = true;
  }

  if (!has_exported_values) return;

  global->set_global_dictionary(*dictionary, kReleaseStore);
  JSObject::InvalidatePrototypeChains(global->map(isolate_));
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadValue(
    Handle<HeapObject> container, uint32_t container_index,
    InternalizeStrings internalize_strings) {
  uint8_t value_type;
  if (!deserializer_->ReadByte(&value_type)) {
    Throw("Malformed variable");
    // Return a placeholder "value" so that the "keep on trucking" error
    // handling won't fail.
    return std::make_tuple(Smi::zero(), false);
  }
  switch (value_type) {
    case ValueType::FALSE_CONSTANT:
      return std::make_tuple(roots_.false_value(), false);
    case ValueType::TRUE_CONSTANT:
      return std::make_tuple(roots_.true_value(), false);
    case ValueType::NULL_CONSTANT:
      return std::make_tuple(roots_.null_value(), false);
    case ValueType::UNDEFINED_CONSTANT:
      return std::make_tuple(roots_.undefined_value(), false);
    case ValueType::NO_ELEMENT_CONSTANT:
      return std::make_tuple(roots_.the_hole_value(), false);
    case ValueType::INTEGER:
      return std::make_tuple(ReadInteger(), false);
    case ValueType::DOUBLE:
      return std::make_tuple(ReadNumber(), false);
    case ValueType::STRING_ID:
      return std::make_tuple(ReadString(internalize_strings), false);
    case ValueType::ARRAY_ID:
      return ReadArray(container, container_index);
    case ValueType::OBJECT_ID:
      return ReadObject(container, container_index);
    case ValueType::FUNCTION_ID:
      return ReadFunction(container, container_index);
    case ValueType::CLASS_ID:
      return ReadClass(container, container_index);
    case ValueType::REGEXP:
      return std::make_tuple(ReadRegexp(), false);
    case ValueType::SYMBOL_ID:
      return std::make_tuple(ReadSymbol(), false);
    case ValueType::BIGINT_ID:
      return std::make_tuple(ReadBigInt(), false);
    case ValueType::EXTERNAL_ID:
      return std::make_tuple(ReadExternalReference(), false);
    case ValueType::BUILTIN_OBJECT_ID:
      return std::make_tuple(ReadBuiltinObjectReference(), false);
    case ValueType::IN_PLACE_STRING_ID:
      return std::make_tuple(ReadInPlaceString(internalize_strings), false);
    case ValueType::ARRAY_BUFFER_ID:
      return ReadArrayBuffer(container, container_index);
    case ValueType::TYPED_ARRAY_ID:
      return ReadTypedArray(container, container_index);
    case ValueType::DATA_VIEW_ID:
      return ReadDataView(container, container_index);
    default:
      // TODO(v8:11525): Handle other value types.
      Throw("Unsupported value type");
      return std::make_tuple(Smi::zero(), false);
  }
}

Object WebSnapshotDeserializer::ReadInteger() {
  Maybe<int32_t> number = deserializer_->ReadZigZag<int32_t>();
  if (number.IsNothing()) {
    Throw("Malformed integer");
    return Smi::zero();
  }
  return *factory()->NewNumberFromInt(number.FromJust());
}

Object WebSnapshotDeserializer::ReadNumber() {
  double number;
  if (!deserializer_->ReadDouble(&number)) {
    Throw("Malformed double");
    return Smi::zero();
  }
  return *factory()->NewNumber(number);
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadArray(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t array_id;
  if (!deserializer_->ReadUint32(&array_id) || array_id >= kMaxItemCount) {
    Throw("Malformed variable");
    return std::make_tuple(Smi::zero(), false);
  }
  if (array_id < current_array_count_) {
    return std::make_tuple(arrays_.get(array_id), false);
  }
  // The array hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, ARRAY_ID, array_id), true);
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadArrayBuffer(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t array_buffer_id;
  if (!deserializer_->ReadUint32(&array_buffer_id) ||
      array_buffer_id >= kMaxItemCount) {
    Throw("Malformed variable");
    return std::make_tuple(Smi::zero(), false);
  }
  if (array_buffer_id < current_array_buffer_count_) {
    return std::make_tuple(array_buffers_.get(array_buffer_id), false);
  }
  // The array buffer hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, ARRAY_BUFFER_ID, array_buffer_id),
      true);
}
std::tuple<Object, bool> WebSnapshotDeserializer::ReadTypedArray(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t typed_array_id;
  if (!deserializer_->ReadUint32(&typed_array_id) ||
      typed_array_id >= kMaxItemCount) {
    Throw("Malformed variable");
    return std::make_tuple(Smi::zero(), false);
  }
  if (typed_array_id < current_typed_array_count_) {
    return std::make_tuple(typed_arrays_.get(typed_array_id), false);
  }
  // The typed array hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, TYPED_ARRAY_ID, typed_array_id),
      true);
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadDataView(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t data_view_id;
  if (!deserializer_->ReadUint32(&data_view_id) ||
      data_view_id >= kMaxItemCount) {
    Throw("Malformed variable");
    return std::make_tuple(Smi::zero(), false);
  }
  if (data_view_id < current_data_view_count_) {
    return std::make_tuple(data_views_.get(data_view_id), false);
  }
  // The data view hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, DATA_VIEW_ID, data_view_id), true);
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadObject(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t object_id;
  if (!deserializer_->ReadUint32(&object_id) || object_id > kMaxItemCount) {
    Throw("Malformed variable");
    return std::make_tuple(Smi::zero(), false);
  }
  if (object_id < current_object_count_) {
    return std::make_tuple(objects_.get(object_id), false);
  }
  // The object hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, OBJECT_ID, object_id), true);
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadFunction(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t function_id;
  if (!deserializer_->ReadUint32(&function_id)) {
    Throw("Malformed object property");
    return std::make_tuple(Smi::zero(), false);
  }
  if (function_id < current_function_count_) {
    return std::make_tuple(functions_.get(function_id), false);
  }
  // The function hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, FUNCTION_ID, function_id), true);
}

std::tuple<Object, bool> WebSnapshotDeserializer::ReadClass(
    Handle<HeapObject> container, uint32_t index) {
  uint32_t class_id;
  if (!deserializer_->ReadUint32(&class_id) || class_id >= kMaxItemCount) {
    Throw("Malformed object property");
    return std::make_tuple(Smi::zero(), false);
  }
  if (class_id < current_class_count_) {
    return std::make_tuple(classes_.get(class_id), false);
  }
  // The class hasn't been deserialized yet.
  return std::make_tuple(
      AddDeferredReference(container, index, CLASS_ID, class_id), true);
}

Object WebSnapshotDeserializer::ReadRegexp() {
  Handle<String> pattern(ReadString(), isolate_);
  Handle<String> flags_string(ReadString(), isolate_);
  base::Optional<JSRegExp::Flags> flags =
      JSRegExp::FlagsFromString(isolate_, flags_string);
  if (!flags.has_value()) {
    Throw("Malformed flags in regular expression");
    return Smi::zero();
  }
  MaybeHandle<JSRegExp> maybe_regexp =
      JSRegExp::New(isolate_, pattern, flags.value());
  Handle<JSRegExp> regexp;
  if (!maybe_regexp.ToHandle(&regexp)) {
    Throw("Malformed RegExp");
    return Smi::zero();
  }
  return *regexp;
}

Object WebSnapshotDeserializer::ReadExternalReference() {
  uint32_t ref_id;
  if (!deserializer_->ReadUint32(&ref_id) ||
      ref_id >= static_cast<uint32_t>(external_references_.length())) {
    Throw("Invalid external reference");
    return Smi::zero();
  }
  return external_references_.get(ref_id);
}

Object WebSnapshotDeserializer::ReadBuiltinObjectReference() {
  uint32_t ref_id;
  if (!deserializer_->ReadUint32(&ref_id) ||
      ref_id >= static_cast<uint32_t>(builtin_objects_.length())) {
    Throw("Invalid builtin object reference");
    return Smi::zero();
  }
  return builtin_objects_.get(ref_id);
}

void WebSnapshotDeserializer::ReadFunctionPrototype(
    Handle<JSFunction> function) {
  uint32_t object_id;

  if (!deserializer_->ReadUint32(&object_id) || object_id > kMaxItemCount + 1) {
    Throw("Malformed class / function");
    return;
  }
  if (object_id == 0) {
    // No prototype.
    return;
  }
  --object_id;
  if (object_id < current_object_count_) {
    if (!SetFunctionPrototype(*function,
                              JSReceiver::cast(objects_.get(object_id)))) {
      Throw("Can't reuse function prototype");
      return;
    }
  } else {
    // The object hasn't been deserialized yet.
    AddDeferredReference(function, 0, OBJECT_ID, object_id);
  }
}

bool WebSnapshotDeserializer::SetFunctionPrototype(JSFunction function,
                                                   JSReceiver prototype) {
  DisallowGarbageCollection no_gc;
  // TODO(v8:11525): Enforce the invariant that no two prototypes share a map.
  Map map = prototype.map();
  map.set_is_prototype_map(true);
  if (!map.constructor_or_back_pointer().IsNullOrUndefined(isolate_)) {
    return false;
  }
  map.set_constructor_or_back_pointer(function);
  function.set_prototype_or_initial_map(prototype, kReleaseStore);
  return true;
}

HeapObject WebSnapshotDeserializer::AddDeferredReference(
    Handle<HeapObject> container, uint32_t index, ValueType target_type,
    uint32_t target_index) {
  if (container.is_null()) {
    const char* message = "Invalid reference";
    switch (target_type) {
      case ARRAY_ID:
        message = "Invalid array reference";
        break;
      case OBJECT_ID:
        message = "Invalid object reference";
        break;
      case CLASS_ID:
        message = "Invalid class reference";
        break;
      case FUNCTION_ID:
        message = "Invalid function reference";
        break;
      default:
        break;
    }
    Throw(message);
    return roots_.undefined_value();
  }
  DCHECK(container->IsPropertyArray() || container->IsContext() ||
         container->IsFixedArray() || container->IsJSFunction() ||
         container->IsMap());
  deferred_references_ = ArrayList::Add(
      isolate_, deferred_references_, container, Smi::FromInt(index),
      Smi::FromInt(target_type), Smi::FromInt(target_index));
  // Use HeapObject as placeholder since this might break elements kinds.
  return roots_.undefined_value();
}

void WebSnapshotDeserializer::ProcessDeferredReferences() {
  // Check for error now, since the FixedArrays below might not have been
  // created if there was an error.
  if (has_error()) return;

  DisallowGarbageCollection no_gc;
  ArrayList raw_deferred_references = *deferred_references_;

  // Deferred references is a list of (object, index, target type, target index)
  // tuples.
  for (int i = 0; i < raw_deferred_references.Length() - 3; i += 4) {
    HeapObject container = HeapObject::cast(raw_deferred_references.Get(i));
    int index = raw_deferred_references.Get(i + 1).ToSmi().value();
    ValueType target_type = static_cast<ValueType>(
        raw_deferred_references.Get(i + 2).ToSmi().value());
    int target_index = raw_deferred_references.Get(i + 3).ToSmi().value();
    Object target;
    switch (target_type) {
      case FUNCTION_ID:
        if (static_cast<uint32_t>(target_index) >= function_count_) {
          // Throw can allocate, but it's ok, since we're not using the raw
          // pointers after that.
          AllowGarbageCollection allow_gc;
          Throw("Invalid function reference");
          return;
        }
        target = functions_.get(target_index);
        break;
      case CLASS_ID:
        if (static_cast<uint32_t>(target_index) >= class_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Invalid class reference");
          return;
        }
        target = classes_.get(target_index);
        break;
      case ARRAY_ID:
        if (static_cast<uint32_t>(target_index) >= array_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Invalid array reference");
          return;
        }
        target = arrays_.get(target_index);
        break;
      case ARRAY_BUFFER_ID:
        if (static_cast<uint32_t>(target_index) >= array_buffer_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Invalid array buffer reference");
          return;
        }
        target = array_buffers_.get(target_index);
        break;
      case TYPED_ARRAY_ID:
        if (static_cast<uint32_t>(target_index) >= typed_array_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Invalid typed array reference");
          return;
        }
        target = typed_arrays_.get(target_index);
        break;
      case DATA_VIEW_ID:
        if (static_cast<uint32_t>(target_index) >= data_view_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Invalid data view reference");
          return;
        }
        target = data_views_.get(target_index);
        break;
      case OBJECT_ID:
        if (static_cast<uint32_t>(target_index) >= object_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Invalid object reference");
          return;
        }
        target = objects_.get(target_index);
        break;
      default:
        UNREACHABLE();
    }
    InstanceType instance_type = container.map().instance_type();
    if (InstanceTypeChecker::IsPropertyArray(instance_type)) {
      PropertyArray::cast(container).set(index, target);
    } else if (InstanceTypeChecker::IsContext(instance_type)) {
      Context::cast(container).set(index, target);
    } else if (InstanceTypeChecker::IsNumberDictionary(instance_type)) {
      // NumberDictionary::Set may need create HeapNumber for index.
      AllowGarbageCollection allow_gc;
      Handle<NumberDictionary> new_container = NumberDictionary::Set(
          isolate_, handle(NumberDictionary::cast(container), isolate_), index,
          handle(target, isolate_));
      // The number dictionary didn't grow, since it was preallocated to be
      // large enough in DeserializeArrays.
      DCHECK_EQ(*new_container, container);
      USE(new_container);
      // We also need to reload raw_deferred_references because
      // NumberDictionary::Set may allocate.
      raw_deferred_references = *deferred_references_;
    } else if (InstanceTypeChecker::IsFixedArray(instance_type)) {
      FixedArray::cast(container).set(index, target);
    } else if (InstanceTypeChecker::IsJSFunction(instance_type)) {
      // The only deferred reference allowed for a JSFunction is the function
      // prototype.
      DCHECK_EQ(index, 0);
      DCHECK(target.IsJSReceiver());
      if (!SetFunctionPrototype(JSFunction::cast(container),
                                JSReceiver::cast(target))) {
        AllowGarbageCollection allow_gc;
        Throw("Can't reuse function prototype");
        return;
      }
    } else if (InstanceTypeChecker::IsMap(instance_type)) {
      // The only deferred reference allowed for a Map is the __proto__.
      DCHECK_EQ(index, 0);
      DCHECK(target.IsJSReceiver());
      AllowGarbageCollection allow_gc;
      SetPrototype(handle(Map::cast(container), isolate_),
                   handle(target, isolate_));
      raw_deferred_references = *deferred_references_;
    } else {
      UNREACHABLE();
    }
  }
  deferred_references_->SetLength(0);
}

}  // namespace internal
}  // namespace v8
