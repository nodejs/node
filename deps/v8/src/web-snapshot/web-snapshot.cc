// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/web-snapshot/web-snapshot.h"

#include <limits>

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/base/platform/wrappers.h"
#include "src/handles/handles.h"
#include "src/objects/contexts.h"
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
    v8_isolate->ThrowException(v8::Exception::Error(
        v8::String::NewFromUtf8(v8_isolate, message).ToLocalChecked()));
  }
}

WebSnapshotSerializer::WebSnapshotSerializer(v8::Isolate* isolate)
    : WebSnapshotSerializerDeserializer(
          reinterpret_cast<v8::internal::Isolate*>(isolate)),
      string_serializer_(isolate_, nullptr),
      map_serializer_(isolate_, nullptr),
      context_serializer_(isolate_, nullptr),
      function_serializer_(isolate_, nullptr),
      object_serializer_(isolate_, nullptr),
      export_serializer_(isolate_, nullptr),
      string_ids_(isolate_->heap()),
      map_ids_(isolate_->heap()),
      context_ids_(isolate_->heap()),
      function_ids_(isolate_->heap()),
      object_ids_(isolate_->heap()) {}

WebSnapshotSerializer::~WebSnapshotSerializer() {}

bool WebSnapshotSerializer::TakeSnapshot(
    v8::Local<v8::Context> context, const std::vector<std::string>& exports,
    WebSnapshotData& data_out) {
  if (string_ids_.size() > 0) {
    Throw("Web snapshot: Can't reuse WebSnapshotSerializer");
    return false;
  }
  v8::Isolate* v8_isolate = reinterpret_cast<v8::Isolate*>(isolate_);
  for (const std::string& export_name : exports) {
    v8::ScriptCompiler::Source source(
        v8::String::NewFromUtf8(v8_isolate, export_name.c_str(),
                                NewStringType::kNormal,
                                static_cast<int>(export_name.length()))
            .ToLocalChecked());
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
    SerializeExport(object, export_name);
  }
  WriteSnapshot(data_out.buffer, data_out.buffer_size);
  return !has_error();
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
  while (!pending_objects_.empty()) {
    const Handle<JSObject>& object = pending_objects_.front();
    SerializePendingObject(object);
    pending_objects_.pop();
  }

  ValueSerializer total_serializer(isolate_, nullptr);
  size_t needed_size =
      string_serializer_.buffer_size_ + map_serializer_.buffer_size_ +
      context_serializer_.buffer_size_ + function_serializer_.buffer_size_ +
      object_serializer_.buffer_size_ + export_serializer_.buffer_size_ +
      6 * sizeof(uint32_t);
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
  total_serializer.WriteUint32(static_cast<uint32_t>(object_count()));
  total_serializer.WriteRawBytes(object_serializer_.buffer_,
                                 object_serializer_.buffer_size_);
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
    Vector<const uint8_t> chars = flat.ToOneByteVector();
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

  std::vector<uint32_t> string_ids;
  for (InternalIndex i : map->IterateOwnDescriptors()) {
    Handle<Name> key(map->instance_descriptors(kRelaxedLoad).GetKey(i),
                     isolate_);
    if (!key->IsString()) {
      Throw("Web snapshot: Key is not a string");
      return;
    }

    PropertyDetails details =
        map->instance_descriptors(kRelaxedLoad).GetDetails(i);
    if (details.IsDontEnum()) {
      Throw("Web snapshot: Non-enumerable properties not supported");
      return;
    }

    if (details.location() != kField) {
      Throw("Web snapshot: Properties which are not fields not supported");
      return;
    }

    uint32_t string_id = 0;
    SerializeString(Handle<String>::cast(key), string_id);
    string_ids.push_back(string_id);

    // TODO(v8:11525): Support property attributes.
  }
  map_serializer_.WriteUint32(static_cast<uint32_t>(string_ids.size()));
  for (auto i : string_ids) {
    map_serializer_.WriteUint32(i);
  }
}

// Format (serialized function):
// - 0 if there's no context, 1 + context id otherwise
// - String id (source string)
void WebSnapshotSerializer::SerializeFunction(Handle<JSFunction> function,
                                              uint32_t& id) {
  if (InsertIntoIndexMap(function_ids_, function, id)) {
    return;
  }

  if (!function->shared().HasSourceCode()) {
    Throw("Web snapshot: Function without source code");
    return;
  }

  Handle<Context> context(function->context(), isolate_);
  if (context->IsNativeContext()) {
    function_serializer_.WriteUint32(0);
  } else {
    DCHECK(context->IsFunctionContext());
    uint32_t context_id = 0;
    SerializeContext(context, context_id);
    function_serializer_.WriteUint32(context_id + 1);
  }

  // TODO(v8:11525): For inner functions which occur inside a serialized
  // function, create a "substring" type, so that we don't need to serialize the
  // same content twice.
  Handle<String> full_source(
      String::cast(Script::cast(function->shared().script()).source()),
      isolate_);
  int start = function->shared().StartPosition();
  int end = function->shared().EndPosition();
  Handle<String> source =
      isolate_->factory()->NewSubString(full_source, start, end);
  uint32_t source_id = 0;
  SerializeString(source, source_id);
  function_serializer_.WriteUint32(source_id);

  // TODO(v8:11525): Serialize .prototype.
  // TODO(v8:11525): Support properties in functions.
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
  if (!context->previous().IsNativeContext()) {
    SerializeContext(handle(context->previous(), isolate_), parent_context_id);
    ++parent_context_id;
  }

  InsertIntoIndexMap(context_ids_, context, id);

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
}

void WebSnapshotSerializer::SerializeObject(Handle<JSObject> object,
                                            uint32_t& id) {
  DCHECK(!object->IsJSFunction());
  if (InsertIntoIndexMap(object_ids_, object, id)) {
    return;
  }
  pending_objects_.push(object);
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

// Format (serialized export):
// - String id (export name)
// - Object id (exported object)
void WebSnapshotSerializer::SerializeExport(Handle<JSObject> object,
                                            const std::string& export_name) {
  // TODO(v8:11525): Support exporting functions.
  ++export_count_;
  // TODO(v8:11525): How to avoid creating the String but still de-dupe?
  Handle<String> export_name_string =
      isolate_->factory()
          ->NewStringFromOneByte(Vector<const uint8_t>(
              reinterpret_cast<const uint8_t*>(export_name.c_str()),
              static_cast<int>(export_name.length())))
          .ToHandleChecked();
  uint32_t string_id = 0;
  SerializeString(export_name_string, string_id);
  uint32_t object_id = 0;
  SerializeObject(object, object_id);
  export_serializer_.WriteUint32(string_id);
  export_serializer_.WriteUint32(object_id);
}

// Format (serialized value):
// - Type id (ValueType enum)
// - Value or id (interpretation depends on the type)
void WebSnapshotSerializer::WriteValue(Handle<Object> object,
                                       ValueSerializer& serializer) {
  uint32_t id = 0;
  if (object->IsSmi()) {
    // TODO(v8:11525): Implement.
    UNREACHABLE();
  }

  DCHECK(object->IsHeapObject());
  switch (HeapObject::cast(*object).map().instance_type()) {
    case ODDBALL_TYPE:
      // TODO(v8:11525): Implement.
      UNREACHABLE();
    case HEAP_NUMBER_TYPE:
      // TODO(v8:11525): Implement.
      UNREACHABLE();
    case JS_FUNCTION_TYPE:
      SerializeFunction(Handle<JSFunction>::cast(object), id);
      serializer.WriteUint32(ValueType::FUNCTION_ID);
      serializer.WriteUint32(id);
      break;
    case JS_OBJECT_TYPE:
      SerializeObject(Handle<JSObject>::cast(object), id);
      serializer.WriteUint32(ValueType::OBJECT_ID);
      serializer.WriteUint32(id);
      break;
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
  function_count_ = 0;
  object_count_ = 0;
  // Make sure we don't read any more data
  deserializer_->position_ = deserializer_->end_;

  WebSnapshotSerializerDeserializer::Throw(message);
}

bool WebSnapshotDeserializer::UseWebSnapshot(const uint8_t* data,
                                             size_t buffer_size) {
  if (deserialized_) {
    Throw("Web snapshot: Can't reuse WebSnapshotDeserializer");
    return false;
  }
  deserialized_ = true;

  // TODO(v8:11525): Add RuntimeCallStats.
  base::ElapsedTimer timer;
  if (FLAG_trace_web_snapshot) {
    timer.Start();
  }

  deserializer_.reset(new ValueDeserializer(isolate_, data, buffer_size));
  DeserializeStrings();
  DeserializeMaps();
  DeserializeContexts();
  DeserializeFunctions();
  DeserializeObjects();
  DeserializeExports();
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
  if (!deserializer_->ReadUint32(&map_count_) || map_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed shape table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  maps_ = isolate_->factory()->NewFixedArray(map_count_);
  for (uint32_t i = 0; i < map_count_; ++i) {
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

    Handle<DescriptorArray> descriptors =
        isolate_->factory()->NewDescriptorArray(0, property_count);
    for (uint32_t p = 0; p < property_count; ++p) {
      Handle<String> key = ReadString(true);

      // Use the "none" representation until we see the first object having this
      // map. At that point, modify the representation.
      Descriptor desc = Descriptor::DataField(
          isolate_, key, static_cast<int>(p), PropertyAttributes::NONE,
          Representation::None());
      descriptors->Append(&desc);
    }

    Handle<Map> map = isolate_->factory()->NewMap(
        JS_OBJECT_TYPE, JSObject::kHeaderSize * kTaggedSize, HOLEY_ELEMENTS, 0);
    map->InitializeDescriptors(isolate_, *descriptors);

    maps_->set(i, *map);
  }
}

void WebSnapshotDeserializer::DeserializeContexts() {
  if (!deserializer_->ReadUint32(&context_count_) ||
      context_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed context table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  contexts_ = isolate_->factory()->NewFixedArray(context_count_);
  for (uint32_t i = 0; i < context_count_; ++i) {
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
        CreateScopeInfo(variable_count, parent_context_id > 0);

    Handle<Context> parent_context;
    if (parent_context_id > 0) {
      parent_context = handle(
          Context::cast(contexts_->get(parent_context_id - 1)), isolate_);
      scope_info->set_outer_scope_info(parent_context->scope_info());
    } else {
      parent_context = handle(isolate_->context(), isolate_);
    }

    Handle<Context> context =
        isolate_->factory()->NewFunctionContext(parent_context, scope_info);
    contexts_->set(i, *context);

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

      Handle<Object> value;
      Representation representation;
      ReadValue(value, representation);
      context->set(scope_info->ContextHeaderLength() + variable_index, *value);
    }
  }
}

Handle<ScopeInfo> WebSnapshotDeserializer::CreateScopeInfo(
    uint32_t variable_count, bool has_parent) {
  // TODO(v8:11525): Decide how to handle language modes. (The code below sets
  // the language mode as strict.)
  // TODO(v8:11525): Support (context-allocating) receiver.
  // TODO(v8:11525): Support function variable & function name.
  // TODO(v8:11525): Support classes.
  const int length = ScopeInfo::kVariablePartIndex +
                     ScopeInfo::kPositionInfoEntries + (has_parent ? 1 : 0) +
                     2 * variable_count;

  Handle<ScopeInfo> scope_info = isolate_->factory()->NewScopeInfo(length);
  int flags =
      ScopeInfo::ScopeTypeBits::encode(ScopeType::FUNCTION_SCOPE) |
      ScopeInfo::SloppyEvalCanExtendVarsBit::encode(false) |
      ScopeInfo::LanguageModeBit::encode(LanguageMode::kStrict) |
      ScopeInfo::DeclarationScopeBit::encode(true) |
      ScopeInfo::ReceiverVariableBits::encode(VariableAllocationInfo::NONE) |
      ScopeInfo::HasClassBrandBit::encode(false) |
      ScopeInfo::HasSavedClassVariableIndexBit::encode(false) |
      ScopeInfo::HasNewTargetBit::encode(false) |
      ScopeInfo::FunctionVariableBits::encode(VariableAllocationInfo::NONE) |
      ScopeInfo::HasInferredFunctionNameBit::encode(false) |
      ScopeInfo::IsAsmModuleBit::encode(false) |
      ScopeInfo::HasSimpleParametersBit::encode(true) |
      ScopeInfo::FunctionKindBits::encode(FunctionKind::kNormalFunction) |
      ScopeInfo::HasOuterScopeInfoBit::encode(has_parent) |
      ScopeInfo::IsDebugEvaluateScopeBit::encode(false) |
      ScopeInfo::ForceContextAllocationBit::encode(false) |
      ScopeInfo::PrivateNameLookupSkipsOuterClassBit::encode(false) |
      ScopeInfo::HasContextExtensionSlotBit::encode(false) |
      ScopeInfo::IsReplModeScopeBit::encode(false) |
      ScopeInfo::HasLocalsBlockListBit::encode(false);
  scope_info->set_flags(flags);
  DCHECK(!scope_info->IsEmpty());

  scope_info->set_context_local_count(variable_count);
  // TODO(v8:11525): Support parameters.
  scope_info->set_parameter_count(0);
  scope_info->SetPositionInfo(0, 0);
  return scope_info;
}

void WebSnapshotDeserializer::DeserializeFunctions() {
  if (!deserializer_->ReadUint32(&function_count_) ||
      function_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed function table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  functions_ = isolate_->factory()->NewFixedArray(function_count_);
  for (uint32_t i = 0; i < function_count_; ++i) {
    uint32_t context_id;
    // Note: > (not >= on purpose, we will subtract 1).
    if (!deserializer_->ReadUint32(&context_id) ||
        context_id > context_count_) {
      Throw("Web snapshot: Malformed function");
      return;
    }

    Handle<String> source = ReadString(false);

    // TODO(v8:11525): Support other function kinds.
    // TODO(v8:11525): Support (exported) top level functions.
    Handle<Script> script = isolate_->factory()->NewScript(source);
    // TODO(v8:11525): Deduplicate the SFIs for inner functions the user creates
    // post-deserialization (by calling the outer function, if it's also in the
    // snapshot) against the ones we create here.
    Handle<SharedFunctionInfo> shared =
        isolate_->factory()->NewSharedFunctionInfo(
            isolate_->factory()->empty_string(), MaybeHandle<Code>(),
            Builtins::kCompileLazy, FunctionKind::kNormalFunction);
    shared->set_function_literal_id(1);
    // TODO(v8:11525): Decide how to handle language modes.
    shared->set_language_mode(LanguageMode::kStrict);
    shared->set_uncompiled_data(
        *isolate_->factory()->NewUncompiledDataWithoutPreparseData(
            ReadOnlyRoots(isolate_).empty_string_handle(), 0,
            source->length()));
    shared->set_script(*script);
    Handle<WeakFixedArray> infos(
        isolate_->factory()->NewWeakFixedArray(3, AllocationType::kOld));
    infos->Set(1, HeapObjectReference::Weak(*shared));
    script->set_shared_function_infos(*infos);

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
    functions_->set(i, *function);
  }
}

void WebSnapshotDeserializer::DeserializeObjects() {
  if (!deserializer_->ReadUint32(&object_count_) ||
      object_count_ > kMaxItemCount) {
    Throw("Web snapshot: Malformed objects table");
    return;
  }
  STATIC_ASSERT(kMaxItemCount <= FixedArray::kMaxLength);
  objects_ = isolate_->factory()->NewFixedArray(object_count_);
  for (size_t object_ix = 0; object_ix < object_count_; ++object_ix) {
    uint32_t map_id;
    if (!deserializer_->ReadUint32(&map_id) || map_id >= map_count_) {
      Throw("Web snapshot: Malformed object");
      return;
    }
    Handle<Map> map = handle(Map::cast(maps_->get(map_id)), isolate_);
    Handle<DescriptorArray> descriptors =
        handle(map->instance_descriptors(kRelaxedLoad), isolate_);
    int no_properties = map->NumberOfOwnDescriptors();
    Handle<PropertyArray> property_array =
        isolate_->factory()->NewPropertyArray(no_properties);
    for (int i = 0; i < no_properties; ++i) {
      Handle<Object> value;
      Representation wanted_representation = Representation::None();
      ReadValue(value, wanted_representation);
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
    object->set_raw_properties_or_hash(*property_array);
    objects_->set(static_cast<int>(object_ix), *object);
  }
}

void WebSnapshotDeserializer::DeserializeExports() {
  uint32_t count;
  if (!deserializer_->ReadUint32(&count) || count > kMaxItemCount) {
    Throw("Web snapshot: Malformed export table");
    return;
  }
  for (uint32_t i = 0; i < count; ++i) {
    Handle<String> export_name = ReadString(true);
    uint32_t object_id = 0;
    if (!deserializer_->ReadUint32(&object_id) || object_id >= object_count_) {
      Throw("Web snapshot: Malformed export");
      return;
    }
    Handle<Object> exported_object = handle(objects_->get(object_id), isolate_);

    // Check for the correctness of the snapshot (thus far) before producing
    // something observable. TODO(v8:11525): Strictly speaking, we should
    // produce observable effects only when we know that the whole snapshot is
    // correct.
    if (has_error()) {
      return;
    }

    auto result = Object::SetProperty(isolate_, isolate_->global_object(),
                                      export_name, exported_object);
    if (result.is_null()) {
      Throw("Web snapshot: Setting global property failed");
      return;
    }
  }
}

void WebSnapshotDeserializer::ReadValue(Handle<Object>& value,
                                        Representation& representation) {
  uint32_t value_type;
  // TODO(v8:11525): Consider adding a ReadByte.
  if (!deserializer_->ReadUint32(&value_type)) {
    Throw("Web snapshot: Malformed variable");
    return;
  }
  switch (value_type) {
    case ValueType::STRING_ID: {
      value = ReadString(false);
      representation = Representation::Tagged();
      break;
    }
    case ValueType::OBJECT_ID:
      uint32_t object_id;
      if (!deserializer_->ReadUint32(&object_id) ||
          object_id >= object_count_) {
        // TODO(v8:11525): Handle circular references + contexts referencing
        // objects.
        Throw("Web snapshot: Malformed variable");
        return;
      }
      value = handle(objects_->get(object_id), isolate_);
      representation = Representation::Tagged();
      break;
    case ValueType::FUNCTION_ID:
      // TODO(v8:11525): Handle contexts referencing functions.
      uint32_t function_id;
      if (!deserializer_->ReadUint32(&function_id) ||
          function_id >= function_count_) {
        Throw("Web snapshot: Malformed object property");
        return;
      }
      value = handle(functions_->get(function_id), isolate_);
      representation = Representation::Tagged();
      break;
    default:
      // TODO(v8:11525): Handle other value types.
      Throw("Web snapshot: Unsupported value type");
      return;
  }
}

}  // namespace internal
}  // namespace v8
