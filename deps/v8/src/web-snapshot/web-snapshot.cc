// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/web-snapshot/web-snapshot.h"

#include <limits>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/base/platform/wrappers.h"
#include "src/handles/handles.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/contexts.h"
#include "src/objects/js-regexp-inl.h"
#include "src/objects/script.h"

namespace v8 {
namespace internal {

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
    v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
    v8_isolate->ThrowError(
        v8::String::NewFromUtf8(v8_isolate, message).ToLocalChecked());
  }
}

uint32_t WebSnapshotSerializerDeserializer::FunctionKindToFunctionFlags(
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
    case FunctionKind::kConciseMethod:
    case FunctionKind::kAsyncConciseMethod:
      break;
    default:
      Throw("Web Snapshot: Unsupported function kind");
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
    uint32_t flags) {
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
          // TODO(v8::11525) Support FunctionKind::kStaticConciseMethod.
          FunctionKind::kInvalid,  // is_async = false
          // TODO(v8::11525) Support FunctionKind::kStaticAsyncConciseMethod.
          FunctionKind::kInvalid,  // is_async = true
          // is_generator = true
          // TODO(v8::11525) Support
          // FunctionKind::kStaticConciseGeneratorMethod.
          FunctionKind::kInvalid,  // is_async = false
          // TODO(v8::11525) Support
          // FunctionKind::kStaticAsyncConciseGeneratorMethod.
          FunctionKind::kInvalid  // is_async = true
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
    Throw("Web Snapshots: Invalid function flags\n");
  }
  return kind;
}

bool WebSnapshotSerializerDeserializer::IsFunctionOrMethod(uint32_t flags) {
  uint32_t mask = AsyncFunctionBitField::kMask |
                  GeneratorFunctionBitField::kMask |
                  ArrowFunctionBitField::kMask | MethodBitField::kMask |
                  StaticBitField::kMask;
  return (flags & mask) == flags;
}

bool WebSnapshotSerializerDeserializer::IsConstructor(uint32_t flags) {
  uint32_t mask = ClassConstructorBitField::kMask |
                  DefaultConstructorBitField::kMask |
                  DerivedConstructorBitField::kMask;
  return ClassConstructorBitField::decode(flags) && (flags & mask) == flags;
}

uint32_t WebSnapshotSerializerDeserializer::GetDefaultAttributeFlags() {
  auto flags = ReadOnlyBitField::encode(false) |
               ConfigurableBitField::encode(true) |
               EnumerableBitField::encode(true);
  return flags;
}

uint32_t WebSnapshotSerializerDeserializer::AttributesToFlags(
    PropertyDetails details) {
  auto flags = ReadOnlyBitField::encode(details.IsReadOnly()) |
               ConfigurableBitField::encode(details.IsConfigurable()) |
               EnumerableBitField::encode(details.IsEnumerable());
  return flags;
}

PropertyAttributes WebSnapshotSerializerDeserializer::FlagsToAttributes(
    uint32_t flags) {
  uint32_t attributes = ReadOnlyBitField::decode(flags) * READ_ONLY +
                        !ConfigurableBitField::decode(flags) * DONT_DELETE +
                        !EnumerableBitField::decode(flags) * DONT_ENUM;
  return static_cast<PropertyAttributes>(attributes);
}

WebSnapshotSerializer::WebSnapshotSerializer(v8::Isolate* isolate)
    : WebSnapshotSerializerDeserializer(
          reinterpret_cast<v8::internal::Isolate*>(isolate)),
      string_serializer_(isolate_, nullptr),
      map_serializer_(isolate_, nullptr),
      context_serializer_(isolate_, nullptr),
      function_serializer_(isolate_, nullptr),
      class_serializer_(isolate_, nullptr),
      array_serializer_(isolate_, nullptr),
      object_serializer_(isolate_, nullptr),
      export_serializer_(isolate_, nullptr),
      string_ids_(isolate_->heap()),
      map_ids_(isolate_->heap()),
      context_ids_(isolate_->heap()),
      function_ids_(isolate_->heap()),
      class_ids_(isolate_->heap()),
      array_ids_(isolate_->heap()),
      object_ids_(isolate_->heap()) {}

WebSnapshotSerializer::~WebSnapshotSerializer() {}

bool WebSnapshotSerializer::TakeSnapshot(v8::Local<v8::Context> context,
                                         v8::Local<v8::PrimitiveArray> exports,
                                         WebSnapshotData& data_out) {
  if (string_ids_.size() > 0) {
    Throw("Web snapshot: Can't reuse WebSnapshotSerializer");
    return false;
  }
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  for (int i = 0, length = exports->Length(); i < length; ++i) {
    v8::Local<v8::String> str =
        exports->Get(v8_isolate, i)->ToString(context).ToLocalChecked();
    if (str.IsEmpty()) {
      continue;
    }
    v8::ScriptCompiler::Source source(str);
    auto script = ScriptCompiler::Compile(context, &source).ToLocalChecked();
    v8::MaybeLocal<v8::Value> script_result = script->Run(context);
    v8::Local<v8::Object> v8_object;
    if (script_result.IsEmpty() ||
        !script_result.ToLocalChecked()->ToObject(context).ToLocal(
            &v8_object)) {
      Throw("Web snapshot: Exported object not found");
      return false;
    }

    auto object = Handle<JSObject>::cast(Utils::OpenHandle(*v8_object));
    SerializeExport(object, Handle<String>::cast(Utils::OpenHandle(*str)));
  }
  WriteSnapshot(data_out.buffer, data_out.buffer_size);
  return !has_error();
}

void WebSnapshotSerializer::SerializePendingItems() {
  while (!pending_objects_.empty() || !pending_arrays_.empty()) {
    while (!pending_objects_.empty()) {
      const Handle<JSObject>& object = pending_objects_.front();
      SerializePendingObject(object);
      pending_objects_.pop();
    }

    while (!pending_arrays_.empty()) {
      const Handle<JSArray>& array = pending_arrays_.front();
      SerializePendingArray(array);
      pending_arrays_.pop();
    }
  }
}

// Format (full snapshot):
// - String count
// - For each string:
//   - Serialized string
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
  SerializePendingItems();

  ValueSerializer total_serializer(isolate_, nullptr);
  size_t needed_size =
      string_serializer_.buffer_size_ + map_serializer_.buffer_size_ +
      context_serializer_.buffer_size_ + function_serializer_.buffer_size_ +
      class_serializer_.buffer_size_ + array_serializer_.buffer_size_ +
      object_serializer_.buffer_size_ + export_serializer_.buffer_size_ +
      8 * sizeof(uint32_t);
  if (total_serializer.ExpandBuffer(needed_size).IsNothing()) {
    Throw("Web snapshot: Out of memory");
    return;
  }

  total_serializer.WriteUint32(static_cast<uint32_t>(string_count()));
  total_serializer.WriteRawBytes(string_serializer_.buffer_,
                                 string_serializer_.buffer_size_);
  total_serializer.WriteUint32(static_cast<uint32_t>(map_count()));
  total_serializer.WriteRawBytes(map_serializer_.buffer_,
                                 map_serializer_.buffer_size_);
  total_serializer.WriteUint32(static_cast<uint32_t>(context_count()));
  total_serializer.WriteRawBytes(context_serializer_.buffer_,
                                 context_serializer_.buffer_size_);
  total_serializer.WriteUint32(static_cast<uint32_t>(function_count()));
  total_serializer.WriteRawBytes(function_serializer_.buffer_,
                                 function_serializer_.buffer_size_);
  total_serializer.WriteUint32(static_cast<uint32_t>(array_count()));
  total_serializer.WriteRawBytes(array_serializer_.buffer_,
                                 array_serializer_.buffer_size_);
  total_serializer.WriteUint32(static_cast<uint32_t>(object_count()));
  total_serializer.WriteRawBytes(object_serializer_.buffer_,
                                 object_serializer_.buffer_size_);
  total_serializer.WriteUint32(static_cast<uint32_t>(class_count()));
  total_serializer.WriteRawBytes(class_serializer_.buffer_,
                                 class_serializer_.buffer_size_);
  total_serializer.WriteUint32(export_count_);
  total_serializer.WriteRawBytes(export_serializer_.buffer_,
                                 export_serializer_.buffer_size_);

  if (has_error()) {
    return;
  }

  auto result = total_serializer.Release();
  buffer = result.first;
  buffer_size = result.second;
}

bool WebSnapshotSerializer::InsertIntoIndexMap(ObjectCacheIndexMap& map,
                                               Handle<HeapObject> object,
                                               uint32_t& id) {
  if (static_cast<uint32_t>(map.size()) >=
      std::numeric_limits<uint32_t>::max()) {
    Throw("Web snapshot: Too many objects");
    return true;
  }
  int index_out;
  bool found = map.LookupOrInsert(object, &index_out);
  id = static_cast<uint32_t>(index_out);
  return found;
}

// Format:
// - Length
// - Raw bytes (data)
void WebSnapshotSerializer::SerializeString(Handle<String> string,
                                            uint32_t& id) {
  if (InsertIntoIndexMap(string_ids_, string, id)) {
    return;
  }

  // TODO(v8:11525): Always write strings as UTF-8.
  string = String::Flatten(isolate_, string);
  DisallowGarbageCollection no_gc;
  String::FlatContent flat = string->GetFlatContent(no_gc);
  DCHECK(flat.IsFlat());
  if (flat.IsOneByte()) {
    base::Vector<const uint8_t> chars = flat.ToOneByteVector();
    string_serializer_.WriteUint32(chars.length());
    string_serializer_.WriteRawBytes(chars.begin(),
                                     chars.length() * sizeof(uint8_t));
  } else if (flat.IsTwoByte()) {
    // TODO(v8:11525): Support two-byte strings.
    UNREACHABLE();
  } else {
    UNREACHABLE();
  }
}

// Format (serialized shape):
// - Property count
// - For each property
//   - String id (name)
void WebSnapshotSerializer::SerializeMap(Handle<Map> map, uint32_t& id) {
  if (InsertIntoIndexMap(map_ids_, map, id)) {
    return;
  }

  int first_custom_index = -1;
  std::vector<uint32_t> string_ids;
  std::vector<uint32_t> attributes;
  string_ids.reserve(map->NumberOfOwnDescriptors());
  attributes.reserve(map->NumberOfOwnDescriptors());
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    Handle<Name> key(map->instance_descriptors(kRelaxedLoad).GetKey(i),
                     isolate_);
    if (!key->IsString()) {
      Throw("Web snapshot: Key is not a string");
      return;
    }

    PropertyDetails details =
        map->instance_descriptors(kRelaxedLoad).GetDetails(i);

    if (details.location() != kField) {
      Throw("Web snapshot: Properties which are not fields not supported");
      return;
    }
    if (first_custom_index >= 0 || details.IsReadOnly() ||
        !details.IsConfigurable() || details.IsDontEnum()) {
      if (first_custom_index == -1) first_custom_index = i.as_int();
      attributes.push_back(AttributesToFlags(details));
    }

    uint32_t string_id = 0;
    SerializeString(Handle<String>::cast(key), string_id);
    string_ids.push_back(string_id);
  }

  map_serializer_.WriteUint32(first_custom_index == -1
                                  ? PropertyAttributesType::DEFAULT
                                  : PropertyAttributesType::CUSTOM);
  map_serializer_.WriteUint32(static_cast<uint32_t>(string_ids.size()));

  uint32_t default_flags = GetDefaultAttributeFlags();
  for (size_t i = 0; i < string_ids.size(); ++i) {
    if (first_custom_index >= 0) {
      if (static_cast<int>(i) < first_custom_index) {
        map_serializer_.WriteUint32(default_flags);
      } else {
        map_serializer_.WriteUint32(attributes[i - first_custom_index]);
      }
    }
    map_serializer_.WriteUint32(string_ids[i]);
  }
}

void WebSnapshotSerializer::SerializeSource(ValueSerializer* serializer,
                                            Handle<JSFunction> function) {
  // TODO(v8:11525): Don't write the full source but instead, a set of minimal
  // snippets which cover the serialized functions.
  Handle<String> full_source(
      String::cast(Script::cast(function->shared().script()).source()),
      isolate_);
  uint32_t source_id = 0;
  SerializeString(full_source, source_id);
  serializer->WriteUint32(source_id);

  int start = function->shared().StartPosition();
  serializer->WriteUint32(start);
  int end = function->shared().EndPosition();
  serializer->WriteUint32(end - start);
}

void WebSnapshotSerializer::SerializeFunctionInfo(ValueSerializer* serializer,
                                                  Handle<JSFunction> function) {
  if (!function->shared().HasSourceCode()) {
    Throw("Web snapshot: Function without source code");
    return;
  }

  Handle<Context> context(function->context(), isolate_);
  if (context->IsNativeContext() || context->IsScriptContext()) {
    serializer->WriteUint32(0);
  } else {
    DCHECK(context->IsFunctionContext() || context->IsBlockContext());
    uint32_t context_id = 0;
    SerializeContext(context, context_id);
    serializer->WriteUint32(context_id + 1);
  }

  SerializeSource(serializer, function);

  serializer->WriteUint32(
      FunctionKindToFunctionFlags(function->shared().kind()));
}

// Format (serialized function):
// - 0 if there's no context, 1 + context id otherwise
// - String id (source snippet)
// - Start position in the source snippet
// - Length in the source snippet
// - Flags (see FunctionFlags)
// TODO(v8:11525): Investigate whether the length is really needed.
// TODO(v8:11525): Serialize the formal parameter count.
void WebSnapshotSerializer::SerializeFunction(Handle<JSFunction> function,
                                              uint32_t& id) {
  if (InsertIntoIndexMap(function_ids_, function, id)) {
    return;
  }

  SerializeFunctionInfo(&function_serializer_, function);

  // TODO(v8:11525): Serialize .prototype.
  // TODO(v8:11525): Support properties in functions.
  // TODO(v8:11525): Support function referencing a function indirectly (e.g.,
  // function -> context -> array -> function).
}

// Format (serialized class):
// - 1 + context id
// - String id (source snippet)
// - Start position in the source snippet
// - Length in the source snippet
// - Flags (see FunctionFlags)
// - Object id (function prototype)
void WebSnapshotSerializer::SerializeClass(Handle<JSFunction> function,
                                           uint32_t& id) {
  if (InsertIntoIndexMap(class_ids_, function, id)) {
    return;
  }

  SerializeFunctionInfo(&class_serializer_, function);

  Handle<JSObject> prototype =
      Handle<JSObject>::cast(handle(function->prototype(), isolate_));
  uint32_t prototype_id;
  SerializeObject(prototype, prototype_id);
  class_serializer_.WriteUint32(prototype_id);

  // TODO(v8:11525): Support properties in classes.
  // TODO(v8:11525): Support class referencing a class indirectly (e.g.,
  // class -> context -> array -> class).
  // TODO(v8:11525): Support class members.
}

// Format (serialized context):
// - 0 if there's no parent context, 1 + parent context id otherwise
// - Variable count
// - For each variable:
//   - String id (name)
//   - Serialized value
void WebSnapshotSerializer::SerializeContext(Handle<Context> context,
                                             uint32_t& id) {
  // Invariant: parent context is serialized first.

  // Can't use InsertIntoIndexMap here, because it might reserve a lower id
  // for the context than its parent.
  int index_out = 0;
  if (context_ids_.Lookup(context, &index_out)) {
    id = static_cast<uint32_t>(index_out);
    return;
  }

  uint32_t parent_context_id = 0;
  if (!context->previous().IsNativeContext() &&
      !context->previous().IsScriptContext()) {
    SerializeContext(handle(context->previous(), isolate_), parent_context_id);
    ++parent_context_id;
  }

  InsertIntoIndexMap(context_ids_, context, id);

  // TODO(v8:11525): Use less space for encoding the context type.
  if (context->IsFunctionContext()) {
    context_serializer_.WriteUint32(ContextType::FUNCTION);
  } else if (context->IsBlockContext()) {
    context_serializer_.WriteUint32(ContextType::BLOCK);
  } else {
    Throw("Web snapshot: Unsupported context type");
    return;
  }

  context_serializer_.WriteUint32(parent_context_id);

  Handle<ScopeInfo> scope_info(context->scope_info(), isolate_);
  int count = scope_info->ContextLocalCount();
  context_serializer_.WriteUint32(count);

  for (int i = 0; i < count; ++i) {
    // TODO(v8:11525): support parameters
    // TODO(v8:11525): distinguish variable modes
    Handle<String> name(scope_info->context_local_names(i), isolate_);
    uint32_t string_id = 0;
    SerializeString(name, string_id);
    context_serializer_.WriteUint32(string_id);
    Handle<Object> value(context->get(scope_info->ContextHeaderLength() + i),
                         isolate_);
    WriteValue(value, context_serializer_);
  }
  // TODO(v8:11525): Support context referencing a context indirectly (e.g.,
  // context -> array -> function -> context).
}

void WebSnapshotSerializer::SerializeObject(Handle<JSObject> object,
                                            uint32_t& id) {
  // TODO(v8:11525): Serialize the leaf objects first.
  DCHECK(!object->IsJSFunction());
  if (InsertIntoIndexMap(object_ids_, object, id)) {
    return;
  }
  pending_objects_.push(object);
}

void WebSnapshotSerializer::SerializeArray(Handle<JSArray> array,
                                           uint32_t& id) {
  // TODO(v8:11525): Serialize the leaf objects first.
  if (InsertIntoIndexMap(array_ids_, array, id)) {
    return;
  }
  pending_arrays_.push(array);
}

// Format (serialized object):
// - Shape id
// - For each property:
//   - Serialized value
void WebSnapshotSerializer::SerializePendingObject(Handle<JSObject> object) {
  Handle<Map> map(object->map(), isolate_);
  uint32_t map_id = 0;
  SerializeMap(map, map_id);

  if (*map != object->map()) {
    Throw("Web snapshot: Map changed");
    return;
  }

  object_serializer_.WriteUint32(map_id);

  for (InternalIndex i : map->IterateOwnDescriptors()) {
    PropertyDetails details =
        map->instance_descriptors(kRelaxedLoad).GetDetails(i);
    FieldIndex field_index = FieldIndex::ForDescriptor(*map, i);
    Handle<Object> value =
        JSObject::FastPropertyAt(object, details.representation(), field_index);
    WriteValue(value, object_serializer_);
  }
}

// Format (serialized array):
// - Length
// - For each element:
//   - Serialized value
void WebSnapshotSerializer::SerializePendingArray(Handle<JSArray> array) {
  auto elements_kind = array->GetElementsKind();
  if (elements_kind != PACKED_SMI_ELEMENTS &&
      elements_kind != PACKED_ELEMENTS) {
    Throw("Web Snapshot: Unsupported array");
    return;
  }
  // TODO(v8:11525): Support sparse arrays & arrays with holes.
  uint32_t length = static_cast<uint32_t>(array->length().ToSmi().value());
  array_serializer_.WriteUint32(length);
  Handle<FixedArray> elements =
      handle(FixedArray::cast(array->elements()), isolate_);
  for (uint32_t i = 0; i < length; ++i) {
    WriteValue(handle(elements->get(i), isolate_), array_serializer_);
  }
}

// Format (serialized export):
// - String id (export name)
// - Serialized value (export value)
void WebSnapshotSerializer::SerializeExport(Handle<JSObject> object,
                                            Handle<String> export_name) {
  ++export_count_;
  uint32_t string_id = 0;
  SerializeString(export_name, string_id);
  export_serializer_.WriteUint32(string_id);
  if (object->IsJSPrimitiveWrapper()) {
    Handle<JSPrimitiveWrapper> wrapper =
        Handle<JSPrimitiveWrapper>::cast(object);
    Handle<Object> export_value =
        handle(JSPrimitiveWrapper::cast(*wrapper).value(), isolate_);
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
  uint32_t id = 0;
  if (object->IsSmi()) {
    serializer.WriteUint32(ValueType::INTEGER);
    serializer.WriteZigZag<int32_t>(Smi::cast(*object).value());
    return;
  }

  DCHECK(object->IsHeapObject());
  switch (HeapObject::cast(*object).map().instance_type()) {
    case ODDBALL_TYPE:
      switch (Oddball::cast(*object).kind()) {
        case Oddball::kFalse:
          serializer.WriteUint32(ValueType::FALSE_CONSTANT);
          return;
        case Oddball::kTrue:
          serializer.WriteUint32(ValueType::TRUE_CONSTANT);
          return;
        case Oddball::kNull:
          serializer.WriteUint32(ValueType::NULL_CONSTANT);
          return;
        case Oddball::kUndefined:
          serializer.WriteUint32(ValueType::UNDEFINED_CONSTANT);
          return;
        default:
          UNREACHABLE();
      }
    case HEAP_NUMBER_TYPE:
      // TODO(v8:11525): Handle possible endianness mismatch.
      serializer.WriteUint32(ValueType::DOUBLE);
      serializer.WriteDouble(HeapNumber::cast(*object).value());
      break;
    case JS_FUNCTION_TYPE: {
      Handle<JSFunction> function = Handle<JSFunction>::cast(object);
      FunctionKind kind = function->shared().kind();
      if (IsClassConstructor(kind)) {
        SerializeClass(function, id);
        serializer.WriteUint32(ValueType::CLASS_ID);
      } else {
        SerializeFunction(function, id);
        serializer.WriteUint32(ValueType::FUNCTION_ID);
      }
      serializer.WriteUint32(id);
      break;
    }
    case JS_OBJECT_TYPE:
      SerializeObject(Handle<JSObject>::cast(object), id);
      serializer.WriteUint32(ValueType::OBJECT_ID);
      serializer.WriteUint32(id);
      break;
    case JS_ARRAY_TYPE:
      SerializeArray(Handle<JSArray>::cast(object), id);
      serializer.WriteUint32(ValueType::ARRAY_ID);
      serializer.WriteUint32(id);
      break;
    case JS_REG_EXP_TYPE: {
      Handle<JSRegExp> regexp = Handle<JSRegExp>::cast(object);
      if (regexp->map() != isolate_->regexp_function()->initial_map()) {
        Throw("Web snapshot: Unsupported RegExp map");
        return;
      }
      uint32_t pattern_id, flags_id;
      Handle<String> pattern = handle(regexp->Pattern(), isolate_);
      Handle<String> flags_string =
          JSRegExp::StringFromFlags(isolate_, regexp->GetFlags());
      SerializeString(pattern, pattern_id);
      SerializeString(flags_string, flags_id);
      serializer.WriteUint32(ValueType::REGEXP);
      serializer.WriteUint32(pattern_id);
      serializer.WriteUint32(flags_id);
      break;
    }
    default:
      if (object->IsString()) {
        SerializeString(Handle<String>::cast(object), id);
        serializer.WriteUint32(ValueType::STRING_ID);
        serializer.WriteUint32(id);
      } else {
        Throw("Web snapshot: Unsupported object");
      }
  }
  // TODO(v8:11525): Support more types.
}

WebSnapshotDeserializer::WebSnapshotDeserializer(v8::Isolate* isolate)
    : WebSnapshotSerializerDeserializer(
          reinterpret_cast<v8::internal::Isolate*>(isolate)) {}

WebSnapshotDeserializer::~WebSnapshotDeserializer() {}

void WebSnapshotDeserializer::Throw(const char* message) {
  string_count_ = 0;
  map_count_ = 0;
  context_count_ = 0;
  class_count_ = 0;
  function_count_ = 0;
  object_count_ = 0;
  // Make sure we don't read any more data
  deserializer_->position_ = deserializer_->end_;

  WebSnapshotSerializerDeserializer::Throw(message);
}

bool WebSnapshotDeserializer::UseWebSnapshot(const uint8_t* data,
                                             size_t buffer_size) {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize);
  if (deserialized_) {
    Throw("Web snapshot: Can't reuse WebSnapshotDeserializer");
    return false;
  }
  deserialized_ = true;

  base::ElapsedTimer timer;
  if (FLAG_trace_web_snapshot) {
    timer.Start();
  }

  deserializer_.reset(new ValueDeserializer(isolate_, data, buffer_size));
  deferred_references_ = ArrayList::New(isolate_, 30);
  DeserializeStrings();
  DeserializeMaps();
  DeserializeContexts();
  DeserializeFunctions();
  DeserializeArrays();
  DeserializeObjects();
  // It comes in handy to deserialize objects before classes. This
  // way, we already have the function prototype for a class deserialized when
  // processing the class and it's easier to adjust it as needed.
  DeserializeClasses();
  ProcessDeferredReferences();
  DeserializeExports();
  DCHECK_EQ(deferred_references_->Length(), 0);
  if (deserializer_->position_ != deserializer_->end_) {
    Throw("Web snapshot: Snapshot length mismatch");
    return false;
  }

  if (FLAG_trace_web_snapshot) {
    double ms = timer.Elapsed().InMillisecondsF();
    PrintF("[Deserializing snapshot (%zu bytes) took %0.3f ms]\n", buffer_size,
           ms);
  }

  // TODO(v8:11525): Add verification mode; verify the objects we just produced.
  return !has_error();
}

void WebSnapshotDeserializer::DeserializeStrings() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Strings);
  if (!deserializer_->ReadUint32(&string_count_) ||
      string_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed string table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  strings_ = isolate_->factory()->NewFixedArray(string_count_);
  for (uint32_t i = 0; i < string_count_; ++i) {
    // TODO(v8:11525): Read strings as UTF-8.
    MaybeHandle<String> maybe_string = deserializer_->ReadOneByteString();
    Handle<String> string;
    if (!maybe_string.ToHandle(&string)) {
      Throw("Web snapshot: Malformed string");
      return;
    }
    strings_->set(i, *string);
  }
}

Handle<String> WebSnapshotDeserializer::ReadString(bool internalize) {
  DCHECK(!strings_->is_null());
  uint32_t string_id;
  if (!deserializer_->ReadUint32(&string_id) || string_id >= string_count_) {
    Throw("Web snapshot: malformed string id\n");
    return isolate_->factory()->empty_string();
  }
  Handle<String> string =
      handle(String::cast(strings_->get(string_id)), isolate_);
  if (internalize && !string->IsInternalizedString()) {
    string = isolate_->factory()->InternalizeString(string);
    strings_->set(string_id, *string);
  }
  return string;
}

void WebSnapshotDeserializer::DeserializeMaps() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Maps);
  if (!deserializer_->ReadUint32(&map_count_) || map_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed shape table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  maps_ = isolate_->factory()->NewFixedArray(map_count_);
  for (uint32_t i = 0; i < map_count_; ++i) {
    uint32_t map_type;
    if (!deserializer_->ReadUint32(&map_type)) {
      Throw("Web snapshot: Malformed shape");
      return;
    }
    bool has_custom_property_attributes;
    switch (map_type) {
      case PropertyAttributesType::DEFAULT:
        has_custom_property_attributes = false;
        break;
      case PropertyAttributesType::CUSTOM:
        has_custom_property_attributes = true;
        break;
      default:
        Throw("Web snapshot: Unsupported map type");
        return;
    }

    uint32_t property_count;
    if (!deserializer_->ReadUint32(&property_count)) {
      Throw("Web snapshot: Malformed shape");
      return;
    }
    // TODO(v8:11525): Consider passing the upper bound as a param and
    // systematically enforcing it on the ValueSerializer side.
    if (property_count > kMaxNumberOfDescriptors) {
      Throw("Web snapshot: Malformed shape: too many properties");
      return;
    }

    if (property_count == 0) {
      DisallowGarbageCollection no_gc;
      Map empty_map =
          isolate_->native_context()->object_function().initial_map();
      maps_->set(i, empty_map);
      return;
    }

    Handle<DescriptorArray> descriptors =
        isolate_->factory()->NewDescriptorArray(0, property_count);
    for (uint32_t p = 0; p < property_count; ++p) {
      PropertyAttributes attributes = PropertyAttributes::NONE;
      if (has_custom_property_attributes) {
        uint32_t flags;
        if (!deserializer_->ReadUint32(&flags)) {
          Throw("Web snapshot: Malformed shape");
          return;
        }
        attributes = FlagsToAttributes(flags);
      }

      Handle<String> key = ReadString(true);

      // Use the "none" representation until we see the first object having this
      // map. At that point, modify the representation.
      Descriptor desc =
          Descriptor::DataField(isolate_, key, static_cast<int>(p), attributes,
                                Representation::None());
      descriptors->Append(&desc);
    }

    Handle<Map> map = isolate_->factory()->NewMap(
        JS_OBJECT_TYPE, JSObject::kHeaderSize * kTaggedSize, HOLEY_ELEMENTS, 0);
    map->InitializeDescriptors(isolate_, *descriptors);
    // TODO(v8:11525): Set 'constructor'.
    // TODO(v8:11525): Set the correct prototype.

    maps_->set(i, *map);
  }
}

void WebSnapshotDeserializer::DeserializeContexts() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Contexts);
  if (!deserializer_->ReadUint32(&context_count_) ||
      context_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed context table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  contexts_ = isolate_->factory()->NewFixedArray(context_count_);
  for (uint32_t i = 0; i < context_count_; ++i) {
    uint32_t context_type;
    if (!deserializer_->ReadUint32(&context_type)) {
      Throw("Web snapshot: Malformed context type");
      return;
    }

    uint32_t parent_context_id;
    // Parent context is serialized before child context. Note: not >= on
    // purpose, we're going to subtract 1 later.
    if (!deserializer_->ReadUint32(&parent_context_id) ||
        parent_context_id > i) {
      Throw("Web snapshot: Malformed context");
      return;
    }

    uint32_t variable_count;
    if (!deserializer_->ReadUint32(&variable_count)) {
      Throw("Web snapshot: Malformed context");
      return;
    }
    // TODO(v8:11525): Enforce upper limit for variable count.
    Handle<ScopeInfo> scope_info =
        CreateScopeInfo(variable_count, parent_context_id > 0,
                        static_cast<ContextType>(context_type));

    Handle<Context> parent_context;
    if (parent_context_id > 0) {
      parent_context = handle(
          Context::cast(contexts_->get(parent_context_id - 1)), isolate_);
      scope_info->set_outer_scope_info(parent_context->scope_info());
    } else {
      parent_context = handle(isolate_->context(), isolate_);
    }

    const int context_local_base = ScopeInfo::kVariablePartIndex;
    const int context_local_info_base = context_local_base + variable_count;
    for (int variable_index = 0;
         variable_index < static_cast<int>(variable_count); ++variable_index) {
      Handle<String> name = ReadString(true);
      scope_info->set(context_local_base + variable_index, *name);

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
        context =
            isolate_->factory()->NewFunctionContext(parent_context, scope_info);
        break;
      case ContextType::BLOCK:
        context =
            isolate_->factory()->NewBlockContext(parent_context, scope_info);
        break;
      default:
        Throw("Web snapshot: Unsupported context type");
        return;
    }
    for (int variable_index = 0;
         variable_index < static_cast<int>(variable_count); ++variable_index) {
      Handle<Object> value;
      Representation representation;
      ReadValue(value, representation, context,
                scope_info->ContextHeaderLength() + variable_index);
      context->set(scope_info->ContextHeaderLength() + variable_index, *value);
    }
    contexts_->set(i, *context);
  }
}

Handle<ScopeInfo> WebSnapshotDeserializer::CreateScopeInfo(
    uint32_t variable_count, bool has_parent, ContextType context_type) {
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
      ScopeInfo::HasClassBrandBit::encode(false) |
      ScopeInfo::HasSavedClassVariableIndexBit::encode(false) |
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
      Throw("Web snapshot: Unsupported context type");
  }
  flags |= ScopeInfo::ScopeTypeBits::encode(scope_type);
  const int length = ScopeInfo::kVariablePartIndex +
                     (ScopeInfo::NeedsPositionInfo(scope_type)
                          ? ScopeInfo::kPositionInfoEntries
                          : 0) +
                     (has_parent ? 1 : 0) + 2 * variable_count;
  Handle<ScopeInfo> scope_info = isolate_->factory()->NewScopeInfo(length);

  scope_info->set_flags(flags);
  DCHECK(!scope_info->IsEmpty());

  scope_info->set_context_local_count(variable_count);
  // TODO(v8:11525): Support parameters.
  scope_info->set_parameter_count(0);
  if (scope_info->HasPositionInfo()) {
    scope_info->SetPositionInfo(0, 0);
  }
  return scope_info;
}

Handle<JSFunction> WebSnapshotDeserializer::CreateJSFunction(
    int shared_function_info_index, uint32_t start_position, uint32_t length,
    uint32_t flags, uint32_t context_id) {
  // TODO(v8:11525): Deduplicate the SFIs for class methods.
  FunctionKind kind = FunctionFlagsToFunctionKind(flags);
  Handle<SharedFunctionInfo> shared =
      isolate_->factory()->NewSharedFunctionInfo(
          isolate_->factory()->empty_string(), MaybeHandle<Code>(),
          Builtin::kCompileLazy, kind);
  if (IsConciseMethod(kind)) {
    shared->set_syntax_kind(FunctionSyntaxKind::kAccessorOrMethod);
  }
  shared->set_script(*script_);
  shared->set_function_literal_id(shared_function_info_index);
  // TODO(v8:11525): Decide how to handle language modes.
  shared->set_language_mode(LanguageMode::kStrict);
  shared->set_uncompiled_data(
      *isolate_->factory()->NewUncompiledDataWithoutPreparseData(
          ReadOnlyRoots(isolate_).empty_string_handle(), start_position,
          start_position + length));
  shared->set_allows_lazy_compilation(true);
  shared_function_infos_->Set(shared_function_info_index,
                              HeapObjectReference::Weak(*shared));
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
    Context context = Context::cast(contexts_->get(context_id - 1));
    function->set_context(context);
    shared->set_outer_scope_info(context.scope_info());
  }
  return function;
}

void WebSnapshotDeserializer::DeserializeFunctions() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Functions);
  if (!deserializer_->ReadUint32(&function_count_) ||
      function_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed function table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount + 1 <= FixedArray::kMaxLength);
  functions_ = isolate_->factory()->NewFixedArray(function_count_);

  // Overallocate the array for SharedFunctionInfos; functions which we
  // deserialize soon will create more SharedFunctionInfos when called.
  shared_function_infos_ = isolate_->factory()->NewWeakFixedArray(
      WeakArrayList::CapacityForLength(function_count_ + 1),
      AllocationType::kOld);
  shared_function_info_table_ = ObjectHashTable::New(isolate_, function_count_);
  script_ = isolate_->factory()->NewScript(isolate_->factory()->empty_string());
  script_->set_type(Script::TYPE_WEB_SNAPSHOT);
  script_->set_shared_function_infos(*shared_function_infos_);
  script_->set_shared_function_info_table(*shared_function_info_table_);

  for (; current_function_count_ < function_count_; ++current_function_count_) {
    uint32_t context_id;
    // Note: > (not >= on purpose, we will subtract 1).
    if (!deserializer_->ReadUint32(&context_id) ||
        context_id > context_count_) {
      Throw("Web snapshot: Malformed function");
      return;
    }

    Handle<String> source = ReadString(false);
    if (current_function_count_ == 0) {
      script_->set_source(*source);
    } else {
      // TODO(v8:11525): Support multiple source snippets.
      DCHECK_EQ(script_->source(), *source);
    }

    uint32_t start_position;
    uint32_t length;
    uint32_t flags;
    if (!deserializer_->ReadUint32(&start_position) ||
        !deserializer_->ReadUint32(&length) ||
        !deserializer_->ReadUint32(&flags)) {
      Throw("Web snapshot: Malformed function");
      return;
    }

    // Index 0 is reserved for top-level shared function info (which web
    // snapshot scripts don't have).
    Handle<JSFunction> function = CreateJSFunction(
        current_function_count_ + 1, start_position, length, flags, context_id);
    functions_->set(current_function_count_, *function);
  }
}

void WebSnapshotDeserializer::DeserializeClasses() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Classes);
  if (!deserializer_->ReadUint32(&class_count_) ||
      class_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed class table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount + 1 <= FixedArray::kMaxLength);
  classes_ = isolate_->factory()->NewFixedArray(class_count_);

  // Grow the array for SharedFunctionInfos.
  shared_function_infos_ = WeakFixedArray::EnsureSpace(
      isolate_, shared_function_infos_,
      WeakArrayList::CapacityForLength(function_count_ + 1 + class_count_));
  script_->set_shared_function_infos(*shared_function_infos_);

  for (; current_class_count_ < class_count_; ++current_class_count_) {
    uint32_t context_id;
    // Note: > (not >= on purpose, we will subtract 1).
    if (!deserializer_->ReadUint32(&context_id) ||
        context_id > context_count_) {
      Throw("Web snapshot: Malformed class");
      return;
    }

    Handle<String> source = ReadString(false);
    if (current_function_count_ + current_class_count_ == 0) {
      script_->set_source(*source);
    } else {
      // TODO(v8:11525): Support multiple source snippets.
      DCHECK_EQ(script_->source(), *source);
    }

    uint32_t start_position;
    uint32_t length;
    uint32_t flags;
    if (!deserializer_->ReadUint32(&start_position) ||
        !deserializer_->ReadUint32(&length) ||
        !deserializer_->ReadUint32(&flags)) {
      Throw("Web snapshot: Malformed class");
      return;
    }

    // Index 0 is reserved for top-level shared function info (which web
    // snapshot scripts don't have).
    Handle<JSFunction> function =
        CreateJSFunction(function_count_ + current_class_count_ + 1,
                         start_position, length, flags, context_id);
    classes_->set(current_class_count_, *function);

    uint32_t function_prototype;
    if (!deserializer_->ReadUint32(&function_prototype) ||
        function_prototype >= object_count_) {
      Throw("Web snapshot: Malformed class");
      return;
    }

    Handle<JSObject> prototype = Handle<JSObject>::cast(
        handle(Object::cast(objects_->get(function_prototype)), isolate_));

    // TODO(v8:11525): Enforce the invariant that no two prototypes share a map.
    Map map = prototype->map();
    map.set_is_prototype_map(true);
    if (!map.constructor_or_back_pointer().IsNullOrUndefined()) {
      Throw("Web snapshot: Map already has a constructor or back pointer set");
      return;
    }
    map.set_constructor_or_back_pointer(*function);

    function->set_prototype_or_initial_map(*prototype, kReleaseStore);

    classes_->set(current_class_count_, *function);
  }
}

void WebSnapshotDeserializer::DeserializeObjects() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Objects);
  if (!deserializer_->ReadUint32(&object_count_) ||
      object_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed objects table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  objects_ = isolate_->factory()->NewFixedArray(object_count_);
  for (; current_object_count_ < object_count_; ++current_object_count_) {
    uint32_t map_id;
    if (!deserializer_->ReadUint32(&map_id) || map_id >= map_count_) {
      Throw("Web snapshot: Malformed object");
      return;
    }
    Handle<Map> map = handle(Map::cast(maps_->get(map_id)), isolate_);
    Handle<DescriptorArray> descriptors =
        handle(map->instance_descriptors(kRelaxedLoad), isolate_);
    int no_properties = map->NumberOfOwnDescriptors();
    // TODO(v8:11525): In-object properties.
    Handle<PropertyArray> property_array =
        isolate_->factory()->NewPropertyArray(no_properties);
    for (int i = 0; i < no_properties; ++i) {
      Handle<Object> value;
      Representation wanted_representation = Representation::None();
      ReadValue(value, wanted_representation, property_array, i);
      // Read the representation from the map.
      PropertyDetails details = descriptors->GetDetails(InternalIndex(i));
      CHECK_EQ(details.location(), kField);
      CHECK_EQ(kData, details.kind());
      Representation r = details.representation();
      if (r.IsNone()) {
        // Switch over to wanted_representation.
        details = details.CopyWithRepresentation(wanted_representation);
        descriptors->SetDetails(InternalIndex(i), details);
      } else if (!r.Equals(wanted_representation)) {
        // TODO(v8:11525): Support this case too.
        UNREACHABLE();
      }
      property_array->set(i, *value);
    }
    Handle<JSObject> object = isolate_->factory()->NewJSObjectFromMap(map);
    object->set_raw_properties_or_hash(*property_array, kRelaxedStore);
    objects_->set(static_cast<int>(current_object_count_), *object);
  }
}

void WebSnapshotDeserializer::DeserializeArrays() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Arrays);
  if (!deserializer_->ReadUint32(&array_count_) ||
      object_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed array table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  arrays_ = isolate_->factory()->NewFixedArray(array_count_);
  for (; current_array_count_ < array_count_; ++current_array_count_) {
    uint32_t length;
    if (!deserializer_->ReadUint32(&length) || length > kMaxItemCount) {
      Throw("Web snapshot: Malformed array");
      return;
    }
    Handle<FixedArray> elements = isolate_->factory()->NewFixedArray(length);
    ElementsKind elements_kind = PACKED_SMI_ELEMENTS;
    for (uint32_t i = 0; i < length; ++i) {
      Handle<Object> value;
      Representation wanted_representation = Representation::None();
      ReadValue(value, wanted_representation, elements, i);
      if (!wanted_representation.IsSmi()) {
        elements_kind = PACKED_ELEMENTS;
      }
      DCHECK(!value.is_null());
      elements->set(static_cast<int>(i), *value);
    }
    Handle<JSArray> array = isolate_->factory()->NewJSArrayWithElements(
        elements, elements_kind, length);
    arrays_->set(static_cast<int>(current_array_count_), *array);
  }
}

void WebSnapshotDeserializer::DeserializeExports() {
  RCS_SCOPE(isolate_, RuntimeCallCounterId::kWebSnapshotDeserialize_Exports);
  uint32_t count;
  if (!deserializer_->ReadUint32(&count) || count > kMaxItemCount) {
    Throw("Web snapshot: Malformed export table");
    return;
  }
  for (uint32_t i = 0; i < count; ++i) {
    Handle<String> export_name = ReadString(true);
    Handle<Object> export_value;
    Representation representation;
    // No deferred references should occur at this point, since all objects have
    // been deserialized.
    ReadValue(export_value, representation);

    // Check for the correctness of the snapshot (thus far) before producing
    // something observable. TODO(v8:11525): Strictly speaking, we should
    // produce observable effects only when we know that the whole snapshot is
    // correct.
    if (has_error()) {
      return;
    }

    auto result = Object::SetProperty(isolate_, isolate_->global_object(),
                                      export_name, export_value);
    if (result.is_null()) {
      Throw("Web snapshot: Setting global property failed");
      return;
    }
  }
}

void WebSnapshotDeserializer::ReadValue(
    Handle<Object>& value, Representation& representation,
    Handle<Object> object_for_deferred_reference,
    uint32_t index_for_deferred_reference) {
  uint32_t value_type;
  // TODO(v8:11525): Consider adding a ReadByte.
  if (!deserializer_->ReadUint32(&value_type)) {
    Throw("Web snapshot: Malformed variable");
    // Set "value" here so that the "keep on trucking" error handling won't fail
    // when dereferencing the handle.
    value = isolate_->factory()->undefined_value();
    representation = Representation::None();
    return;
  }
  switch (value_type) {
    case ValueType::FALSE_CONSTANT: {
      value = handle(ReadOnlyRoots(isolate_).false_value(), isolate_);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::TRUE_CONSTANT: {
      value = handle(ReadOnlyRoots(isolate_).true_value(), isolate_);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::NULL_CONSTANT: {
      value = handle(ReadOnlyRoots(isolate_).null_value(), isolate_);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::UNDEFINED_CONSTANT: {
      value = handle(ReadOnlyRoots(isolate_).undefined_value(), isolate_);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::INTEGER: {
      Maybe<int32_t> number = deserializer_->ReadZigZag<int32_t>();
      if (number.IsNothing()) {
        Throw("Web snapshot: Malformed integer");
        return;
      }
      value = isolate_->factory()->NewNumberFromInt(number.FromJust());
      representation = Representation::Tagged();
      break;
    }
    case ValueType::DOUBLE: {
      double number;
      if (!deserializer_->ReadDouble(&number)) {
        Throw("Web snapshot: Malformed double");
        return;
      }
      value = isolate_->factory()->NewNumber(number);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::STRING_ID: {
      value = ReadString(false);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::ARRAY_ID:
      uint32_t array_id;
      if (!deserializer_->ReadUint32(&array_id) || array_id >= kMaxItemCount) {
        Throw("Web snapshot: Malformed variable");
        return;
      }
      if (array_id < current_array_count_) {
        value = handle(arrays_->get(array_id), isolate_);
      } else {
        // The array hasn't been deserialized yet.
        value = isolate_->factory()->undefined_value();
        if (object_for_deferred_reference.is_null()) {
          Throw("Web snapshot: Invalid array reference");
          return;
        }
        AddDeferredReference(object_for_deferred_reference,
                             index_for_deferred_reference, ARRAY_ID, array_id);
      }
      representation = Representation::Tagged();
      break;
    case ValueType::OBJECT_ID:
      uint32_t object_id;
      if (!deserializer_->ReadUint32(&object_id) || object_id > kMaxItemCount) {
        Throw("Web snapshot: Malformed variable");
        return;
      }
      if (object_id < current_object_count_) {
        value = handle(objects_->get(object_id), isolate_);
      } else {
        // The object hasn't been deserialized yet.
        value = isolate_->factory()->undefined_value();
        if (object_for_deferred_reference.is_null()) {
          Throw("Web snapshot: Invalid object reference");
          return;
        }
        AddDeferredReference(object_for_deferred_reference,
                             index_for_deferred_reference, OBJECT_ID,
                             object_id);
      }
      representation = Representation::Tagged();
      break;
    case ValueType::FUNCTION_ID: {
      uint32_t function_id;
      if (!deserializer_->ReadUint32(&function_id) ||
          function_id >= function_count_) {
        Throw("Web snapshot: Malformed object property");
        return;
      }
      if (function_id < current_function_count_) {
        value = handle(functions_->get(function_id), isolate_);
      } else {
        // The function hasn't been deserialized yet.
        value = isolate_->factory()->undefined_value();
        if (object_for_deferred_reference.is_null()) {
          Throw("Web snapshot: Invalid object reference");
          return;
        }
        AddDeferredReference(object_for_deferred_reference,
                             index_for_deferred_reference, FUNCTION_ID,
                             function_id);
      }
      representation = Representation::Tagged();
      break;
    }
    case ValueType::CLASS_ID: {
      uint32_t class_id;
      if (!deserializer_->ReadUint32(&class_id) || class_id >= kMaxItemCount) {
        Throw("Web snapshot: Malformed object property");
        return;
      }
      if (class_id < current_class_count_) {
        value = handle(classes_->get(class_id), isolate_);
      } else {
        // The class hasn't been deserialized yet.
        value = isolate_->factory()->undefined_value();
        if (object_for_deferred_reference.is_null()) {
          Throw("Web snapshot: Invalid object reference");
          return;
        }
        AddDeferredReference(object_for_deferred_reference,
                             index_for_deferred_reference, CLASS_ID, class_id);
      }
      representation = Representation::Tagged();
      break;
    }
    case ValueType::REGEXP: {
      Handle<String> pattern = ReadString(false);
      Handle<String> flags_string = ReadString(false);
      bool success = false;
      JSRegExp::Flags flags =
          JSRegExp::FlagsFromString(isolate_, flags_string, &success);
      if (!success) {
        Throw("Web snapshot: Malformed flags in regular expression");
        return;
      }
      MaybeHandle<JSRegExp> maybe_regexp =
          JSRegExp::New(isolate_, pattern, flags);
      if (!maybe_regexp.ToHandle(&value)) {
        Throw("Web snapshot: Malformed RegExp");
        return;
      }
      representation = Representation::Tagged();
      break;
    }
    default:
      // TODO(v8:11525): Handle other value types.
      Throw("Web snapshot: Unsupported value type");
      return;
  }
}

void WebSnapshotDeserializer::AddDeferredReference(Handle<Object> container,
                                                   uint32_t index,
                                                   ValueType target_type,
                                                   uint32_t target_index) {
  DCHECK(container->IsPropertyArray() || container->IsContext() ||
         container->IsFixedArray());
  deferred_references_ = ArrayList::Add(
      isolate_, deferred_references_, container, Smi::FromInt(index),
      Smi::FromInt(target_type), Smi::FromInt(target_index));
}

void WebSnapshotDeserializer::ProcessDeferredReferences() {
  DisallowGarbageCollection no_gc;
  ArrayList raw_deferred_references = *deferred_references_;
  FixedArray raw_functions = *functions_;
  FixedArray raw_classes = *classes_;
  FixedArray raw_arrays = *arrays_;
  FixedArray raw_objects = *objects_;

  // Deferred references is a list of (object, index, target type, target index)
  // tuples.
  for (int i = 0; i < raw_deferred_references.Length() - 3; i += 4) {
    Object container = raw_deferred_references.Get(i);
    int index = raw_deferred_references.Get(i + 1).ToSmi().value();
    ValueType target_type =
        ValueType(raw_deferred_references.Get(i + 2).ToSmi().value());
    int target_index = raw_deferred_references.Get(i + 3).ToSmi().value();
    Object target;
    switch (target_type) {
      case FUNCTION_ID:
        if (static_cast<uint32_t>(target_index) >= function_count_) {
          // Throw can allocate, but it's ok, since we're not using the raw
          // pointers after that.
          AllowGarbageCollection allow_gc;
          Throw("Web Snapshots: Invalid function reference");
          return;
        }
        target = raw_functions.get(target_index);
        break;
      case CLASS_ID:
        if (static_cast<uint32_t>(target_index) >= class_count_) {
          // Throw can allocate, but it's ok, since we're not using the raw
          // pointers after that.
          AllowGarbageCollection allow_gc;
          Throw("Web Snapshots: Invalid class reference");
          return;
        }
        target = raw_classes.get(target_index);
        break;
      case ARRAY_ID:
        if (static_cast<uint32_t>(target_index) >= array_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Web Snapshots: Invalid array reference");
          return;
        }
        target = raw_arrays.get(target_index);
        break;
      case OBJECT_ID:
        if (static_cast<uint32_t>(target_index) >= object_count_) {
          AllowGarbageCollection allow_gc;
          Throw("Web Snapshots: Invalid object reference");
          return;
        }
        target = raw_objects.get(target_index);
        break;
      default:
        UNREACHABLE();
    }
    if (container.IsPropertyArray()) {
      PropertyArray::cast(container).set(index, target);
    } else if (container.IsContext()) {
      Context::cast(container).set(index, target);
    } else if (container.IsFixedArray()) {
      FixedArray::cast(container).set(index, target);
    } else {
      UNREACHABLE();
    }
  }
  deferred_references_->SetLength(0);
}

}  // namespace internal
}  // namespace v8
