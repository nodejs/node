// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-module.h"

#include <functional>
#include <memory>

#include "src/api/api-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/objects.h"
#include "src/wasm/jump-table-assembler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-init-expr.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module-builder.h"  // For {ZoneBuffer}.
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-subtyping.h"

namespace v8::internal::wasm {

template <class Value>
void AdaptiveMap<Value>::FinishInitialization() {
  uint32_t count = 0;
  uint32_t max = 0;
  DCHECK_EQ(mode_, kInitializing);
  for (const auto& entry : *map_) {
    count++;
    max = std::max(max, entry.first);
  }
  if (count >= (max + 1) / kLoadFactor) {
    mode_ = kDense;
    vector_.resize(max + 1);
    for (auto& entry : *map_) {
      vector_[entry.first] = std::move(entry.second);
    }
    map_.reset();
  } else {
    mode_ = kSparse;
  }
}
template void NameMap::FinishInitialization();
template void IndirectNameMap::FinishInitialization();

WireBytesRef LazilyGeneratedNames::LookupFunctionName(
    const ModuleWireBytes& wire_bytes, uint32_t function_index) {
  base::MutexGuard lock(&mutex_);
  if (!has_functions_) {
    has_functions_ = true;
    DecodeFunctionNames(wire_bytes.start(), wire_bytes.end(), function_names_);
  }
  const WireBytesRef* result = function_names_.Get(function_index);
  if (!result) return WireBytesRef();
  return *result;
}

bool LazilyGeneratedNames::Has(uint32_t function_index) {
  DCHECK(has_functions_);
  base::MutexGuard lock(&mutex_);
  return function_names_.Get(function_index) != nullptr;
}

// static
int MaxNumExportWrappers(const WasmModule* module) {
  // For each signature there may exist a wrapper, both for imported and
  // internal functions.
  return static_cast<int>(module->signature_map.size()) * 2;
}

int GetExportWrapperIndexInternal(const WasmModule* module,
                                  int canonical_sig_index, bool is_import) {
  if (is_import) canonical_sig_index += module->signature_map.size();
  return canonical_sig_index;
}

int GetExportWrapperIndex(const WasmModule* module, const FunctionSig* sig,
                          bool is_import) {
  int canonical_sig_index = module->signature_map.Find(*sig);
  CHECK_GE(canonical_sig_index, 0);
  return GetExportWrapperIndexInternal(module, canonical_sig_index, is_import);
}

int GetExportWrapperIndex(const WasmModule* module, uint32_t sig_index,
                          bool is_import) {
  uint32_t canonical_sig_index =
      module->per_module_canonical_type_ids[sig_index];
  return GetExportWrapperIndexInternal(module, canonical_sig_index, is_import);
}

// static
int GetWasmFunctionOffset(const WasmModule* module, uint32_t func_index) {
  const std::vector<WasmFunction>& functions = module->functions;
  if (static_cast<uint32_t>(func_index) >= functions.size()) return -1;
  DCHECK_GE(kMaxInt, functions[func_index].code.offset());
  return static_cast<int>(functions[func_index].code.offset());
}

// static
int GetNearestWasmFunction(const WasmModule* module, uint32_t byte_offset) {
  const std::vector<WasmFunction>& functions = module->functions;

  // Binary search for a function containing the given position.
  int left = 0;                                    // inclusive
  int right = static_cast<int>(functions.size());  // exclusive
  if (right == 0) return -1;
  while (right - left > 1) {
    int mid = left + (right - left) / 2;
    if (functions[mid].code.offset() <= byte_offset) {
      left = mid;
    } else {
      right = mid;
    }
  }

  return left;
}

// static
int GetContainingWasmFunction(const WasmModule* module, uint32_t byte_offset) {
  int func_index = GetNearestWasmFunction(module, byte_offset);

  if (func_index >= 0) {
    // If the found function does not contain the given position, return -1.
    const WasmFunction& func = module->functions[func_index];
    if (byte_offset < func.code.offset() ||
        byte_offset >= func.code.end_offset()) {
      return -1;
    }
  }
  return func_index;
}

// TODO(7748): Measure whether this iterative implementation is fast enough.
// We could cache the result on the module, in yet another vector indexed by
// type index.
int GetSubtypingDepth(const WasmModule* module, uint32_t type_index) {
  uint32_t starting_point = type_index;
  int depth = 0;
  while ((type_index = module->supertype(type_index)) != kNoSuperType) {
    if (type_index == starting_point) return -1;  // Cycle detected.
    depth++;
    if (depth > static_cast<int>(kV8MaxRttSubtypingDepth)) break;
  }
  return depth;
}

void LazilyGeneratedNames::AddForTesting(int function_index,
                                         WireBytesRef name) {
  base::MutexGuard lock(&mutex_);
  function_names_.Put(function_index, name);
}

AsmJsOffsetInformation::AsmJsOffsetInformation(
    base::Vector<const byte> encoded_offsets)
    : encoded_offsets_(base::OwnedVector<const uint8_t>::Of(encoded_offsets)) {}

AsmJsOffsetInformation::~AsmJsOffsetInformation() = default;

int AsmJsOffsetInformation::GetSourcePosition(int declared_func_index,
                                              int byte_offset,
                                              bool is_at_number_conversion) {
  EnsureDecodedOffsets();

  DCHECK_LE(0, declared_func_index);
  DCHECK_GT(decoded_offsets_->functions.size(), declared_func_index);
  std::vector<AsmJsOffsetEntry>& function_offsets =
      decoded_offsets_->functions[declared_func_index].entries;

  auto byte_offset_less = [](const AsmJsOffsetEntry& a,
                             const AsmJsOffsetEntry& b) {
    return a.byte_offset < b.byte_offset;
  };
  SLOW_DCHECK(std::is_sorted(function_offsets.begin(), function_offsets.end(),
                             byte_offset_less));
  auto it =
      std::lower_bound(function_offsets.begin(), function_offsets.end(),
                       AsmJsOffsetEntry{byte_offset, 0, 0}, byte_offset_less);
  DCHECK_NE(function_offsets.end(), it);
  DCHECK_EQ(byte_offset, it->byte_offset);
  return is_at_number_conversion ? it->source_position_number_conversion
                                 : it->source_position_call;
}

std::pair<int, int> AsmJsOffsetInformation::GetFunctionOffsets(
    int declared_func_index) {
  EnsureDecodedOffsets();

  DCHECK_LE(0, declared_func_index);
  DCHECK_GT(decoded_offsets_->functions.size(), declared_func_index);
  AsmJsOffsetFunctionEntries& function_info =
      decoded_offsets_->functions[declared_func_index];

  return {function_info.start_offset, function_info.end_offset};
}

void AsmJsOffsetInformation::EnsureDecodedOffsets() {
  base::MutexGuard mutex_guard(&mutex_);
  DCHECK_EQ(encoded_offsets_ == nullptr, decoded_offsets_ != nullptr);

  if (decoded_offsets_) return;
  AsmJsOffsetsResult result =
      wasm::DecodeAsmJsOffsets(encoded_offsets_.as_vector());
  decoded_offsets_ = std::make_unique<AsmJsOffsets>(std::move(result).value());
  encoded_offsets_.ReleaseData();
}

// Get a string stored in the module bytes representing a name.
WasmName ModuleWireBytes::GetNameOrNull(WireBytesRef ref) const {
  if (!ref.is_set()) return {nullptr, 0};  // no name.
  DCHECK(BoundsCheck(ref));
  return WasmName::cast(
      module_bytes_.SubVector(ref.offset(), ref.end_offset()));
}

// Get a string stored in the module bytes representing a function name.
WasmName ModuleWireBytes::GetNameOrNull(const WasmFunction* function,
                                        const WasmModule* module) const {
  return GetNameOrNull(module->lazily_generated_names.LookupFunctionName(
      *this, function->func_index));
}

std::ostream& operator<<(std::ostream& os, const WasmFunctionName& name) {
  os << "#" << name.function_->func_index;
  if (!name.name_.empty()) {
    if (name.name_.begin()) {
      os << ":";
      os.write(name.name_.begin(), name.name_.length());
    }
  } else {
    os << "?";
  }
  return os;
}

WasmModule::WasmModule(std::unique_ptr<Zone> signature_zone)
    : signature_zone(std::move(signature_zone)) {}

bool IsWasmCodegenAllowed(Isolate* isolate, Handle<Context> context) {
  // TODO(wasm): Once wasm has its own CSP policy, we should introduce a
  // separate callback that includes information about the module about to be
  // compiled. For the time being, pass an empty string as placeholder for the
  // sources.
  if (auto wasm_codegen_callback = isolate->allow_wasm_code_gen_callback()) {
    return wasm_codegen_callback(
        v8::Utils::ToLocal(context),
        v8::Utils::ToLocal(isolate->factory()->empty_string()));
  }
  auto codegen_callback = isolate->allow_code_gen_callback();
  return codegen_callback == nullptr ||
         codegen_callback(
             v8::Utils::ToLocal(context),
             v8::Utils::ToLocal(isolate->factory()->empty_string()));
}

Handle<String> ErrorStringForCodegen(Isolate* isolate,
                                     Handle<Context> context) {
  Handle<Object> error = context->ErrorMessageForWasmCodeGeneration();
  DCHECK(!error.is_null());
  return Object::NoSideEffectsToString(isolate, error);
}

namespace {

// Converts the given {type} into a string representation that can be used in
// reflective functions. Should be kept in sync with the {GetValueType} helper.
Handle<String> ToValueTypeString(Isolate* isolate, ValueType type) {
  return isolate->factory()->InternalizeUtf8String(base::VectorOf(type.name()));
}
}  // namespace

Handle<JSObject> GetTypeForFunction(Isolate* isolate, const FunctionSig* sig,
                                    bool for_exception) {
  Factory* factory = isolate->factory();

  // Extract values for the {ValueType[]} arrays.
  int param_index = 0;
  int param_count = static_cast<int>(sig->parameter_count());
  Handle<FixedArray> param_values = factory->NewFixedArray(param_count);
  for (ValueType type : sig->parameters()) {
    Handle<String> type_value = ToValueTypeString(isolate, type);
    param_values->set(param_index++, *type_value);
  }

  // Create the resulting {FunctionType} object.
  Handle<JSFunction> object_function = isolate->object_function();
  Handle<JSObject> object = factory->NewJSObject(object_function);
  Handle<JSArray> params = factory->NewJSArrayWithElements(param_values);
  Handle<String> params_string = factory->InternalizeUtf8String("parameters");
  Handle<String> results_string = factory->InternalizeUtf8String("results");
  JSObject::AddProperty(isolate, object, params_string, params, NONE);

  // Now add the result types if needed.
  if (for_exception) {
    DCHECK_EQ(sig->returns().size(), 0);
  } else {
    int result_index = 0;
    int result_count = static_cast<int>(sig->return_count());
    Handle<FixedArray> result_values = factory->NewFixedArray(result_count);
    for (ValueType type : sig->returns()) {
      Handle<String> type_value = ToValueTypeString(isolate, type);
      result_values->set(result_index++, *type_value);
    }
    Handle<JSArray> results = factory->NewJSArrayWithElements(result_values);
    JSObject::AddProperty(isolate, object, results_string, results, NONE);
  }

  return object;
}

Handle<JSObject> GetTypeForGlobal(Isolate* isolate, bool is_mutable,
                                  ValueType type) {
  Factory* factory = isolate->factory();

  Handle<JSFunction> object_function = isolate->object_function();
  Handle<JSObject> object = factory->NewJSObject(object_function);
  Handle<String> mutable_string = factory->InternalizeUtf8String("mutable");
  Handle<String> value_string = factory->InternalizeUtf8String("value");
  JSObject::AddProperty(isolate, object, mutable_string,
                        factory->ToBoolean(is_mutable), NONE);
  JSObject::AddProperty(isolate, object, value_string,
                        ToValueTypeString(isolate, type), NONE);

  return object;
}

Handle<JSObject> GetTypeForMemory(Isolate* isolate, uint32_t min_size,
                                  base::Optional<uint32_t> max_size,
                                  bool shared) {
  Factory* factory = isolate->factory();

  Handle<JSFunction> object_function = isolate->object_function();
  Handle<JSObject> object = factory->NewJSObject(object_function);
  Handle<String> minimum_string = factory->InternalizeUtf8String("minimum");
  Handle<String> maximum_string = factory->InternalizeUtf8String("maximum");
  Handle<String> shared_string = factory->InternalizeUtf8String("shared");
  JSObject::AddProperty(isolate, object, minimum_string,
                        factory->NewNumberFromUint(min_size), NONE);
  if (max_size.has_value()) {
    JSObject::AddProperty(isolate, object, maximum_string,
                          factory->NewNumberFromUint(max_size.value()), NONE);
  }
  JSObject::AddProperty(isolate, object, shared_string,
                        factory->ToBoolean(shared), NONE);

  return object;
}

Handle<JSObject> GetTypeForTable(Isolate* isolate, ValueType type,
                                 uint32_t min_size,
                                 base::Optional<uint32_t> max_size) {
  Factory* factory = isolate->factory();

  Handle<String> element =
      factory->InternalizeUtf8String(base::VectorOf(type.name()));

  Handle<JSFunction> object_function = isolate->object_function();
  Handle<JSObject> object = factory->NewJSObject(object_function);
  Handle<String> element_string = factory->InternalizeUtf8String("element");
  Handle<String> minimum_string = factory->InternalizeUtf8String("minimum");
  Handle<String> maximum_string = factory->InternalizeUtf8String("maximum");
  JSObject::AddProperty(isolate, object, element_string, element, NONE);
  JSObject::AddProperty(isolate, object, minimum_string,
                        factory->NewNumberFromUint(min_size), NONE);
  if (max_size.has_value()) {
    JSObject::AddProperty(isolate, object, maximum_string,
                          factory->NewNumberFromUint(max_size.value()), NONE);
  }

  return object;
}

Handle<JSArray> GetImports(Isolate* isolate,
                           Handle<WasmModuleObject> module_object) {
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(isolate);
  Factory* factory = isolate->factory();

  Handle<String> module_string = factory->InternalizeUtf8String("module");
  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");
  Handle<String> type_string = factory->InternalizeUtf8String("type");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");
  Handle<String> tag_string = factory->InternalizeUtf8String("tag");

  // Create the result array.
  const WasmModule* module = module_object->module();
  int num_imports = static_cast<int>(module->import_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_imports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_imports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_imports; ++index) {
    const WasmImport& import = module->import_table[index];

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    Handle<String> import_kind;
    Handle<JSObject> type_value;
    switch (import.kind) {
      case kExternalFunction:
        if (enabled_features.has_type_reflection()) {
          auto& func = module->functions[import.index];
          type_value = GetTypeForFunction(isolate, func.sig);
        }
        import_kind = function_string;
        break;
      case kExternalTable:
        if (enabled_features.has_type_reflection()) {
          auto& table = module->tables[import.index];
          base::Optional<uint32_t> maximum_size;
          if (table.has_maximum_size) maximum_size.emplace(table.maximum_size);
          type_value = GetTypeForTable(isolate, table.type, table.initial_size,
                                       maximum_size);
        }
        import_kind = table_string;
        break;
      case kExternalMemory:
        if (enabled_features.has_type_reflection()) {
          DCHECK_EQ(0, import.index);  // Only one memory supported.
          base::Optional<uint32_t> maximum_size;
          if (module->has_maximum_pages) {
            maximum_size.emplace(module->maximum_pages);
          }
          type_value =
              GetTypeForMemory(isolate, module->initial_pages, maximum_size,
                               module->has_shared_memory);
        }
        import_kind = memory_string;
        break;
      case kExternalGlobal:
        if (enabled_features.has_type_reflection()) {
          auto& global = module->globals[import.index];
          type_value =
              GetTypeForGlobal(isolate, global.mutability, global.type);
        }
        import_kind = global_string;
        break;
      case kExternalTag:
        import_kind = tag_string;
        break;
    }
    DCHECK(!import_kind->is_null());

    Handle<String> import_module =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, import.module_name, kInternalize);

    Handle<String> import_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, import.field_name, kInternalize);

    JSObject::AddProperty(isolate, entry, module_string, import_module, NONE);
    JSObject::AddProperty(isolate, entry, name_string, import_name, NONE);
    JSObject::AddProperty(isolate, entry, kind_string, import_kind, NONE);
    if (!type_value.is_null()) {
      JSObject::AddProperty(isolate, entry, type_string, type_value, NONE);
    }

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> GetExports(Isolate* isolate,
                           Handle<WasmModuleObject> module_object) {
  auto enabled_features = i::wasm::WasmFeatures::FromIsolate(isolate);
  Factory* factory = isolate->factory();

  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");
  Handle<String> type_string = factory->InternalizeUtf8String("type");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");
  Handle<String> tag_string = factory->InternalizeUtf8String("tag");

  // Create the result array.
  const WasmModule* module = module_object->module();
  int num_exports = static_cast<int>(module->export_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_exports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_exports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_exports; ++index) {
    const WasmExport& exp = module->export_table[index];

    Handle<String> export_kind;
    Handle<JSObject> type_value;
    switch (exp.kind) {
      case kExternalFunction:
        if (enabled_features.has_type_reflection()) {
          auto& func = module->functions[exp.index];
          type_value = GetTypeForFunction(isolate, func.sig);
        }
        export_kind = function_string;
        break;
      case kExternalTable:
        if (enabled_features.has_type_reflection()) {
          auto& table = module->tables[exp.index];
          base::Optional<uint32_t> maximum_size;
          if (table.has_maximum_size) maximum_size.emplace(table.maximum_size);
          type_value = GetTypeForTable(isolate, table.type, table.initial_size,
                                       maximum_size);
        }
        export_kind = table_string;
        break;
      case kExternalMemory:
        if (enabled_features.has_type_reflection()) {
          DCHECK_EQ(0, exp.index);  // Only one memory supported.
          base::Optional<uint32_t> maximum_size;
          if (module->has_maximum_pages) {
            maximum_size.emplace(module->maximum_pages);
          }
          type_value =
              GetTypeForMemory(isolate, module->initial_pages, maximum_size,
                               module->has_shared_memory);
        }
        export_kind = memory_string;
        break;
      case kExternalGlobal:
        if (enabled_features.has_type_reflection()) {
          auto& global = module->globals[exp.index];
          type_value =
              GetTypeForGlobal(isolate, global.mutability, global.type);
        }
        export_kind = global_string;
        break;
      case kExternalTag:
        export_kind = tag_string;
        break;
      default:
        UNREACHABLE();
    }

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    Handle<String> export_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, exp.name, kNoInternalize);

    JSObject::AddProperty(isolate, entry, name_string, export_name, NONE);
    JSObject::AddProperty(isolate, entry, kind_string, export_kind, NONE);
    if (!type_value.is_null()) {
      JSObject::AddProperty(isolate, entry, type_string, type_value, NONE);
    }

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> GetCustomSections(Isolate* isolate,
                                  Handle<WasmModuleObject> module_object,
                                  Handle<String> name, ErrorThrower* thrower) {
  Factory* factory = isolate->factory();

  base::Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  std::vector<CustomSectionOffset> custom_sections =
      DecodeCustomSections(wire_bytes.begin(), wire_bytes.end());

  std::vector<Handle<Object>> matching_sections;

  // Gather matching sections.
  for (auto& section : custom_sections) {
    Handle<String> section_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, section.name, kNoInternalize);

    if (!name->Equals(*section_name)) continue;

    // Make a copy of the payload data in the section.
    size_t size = section.payload.length();
    MaybeHandle<JSArrayBuffer> result =
        isolate->factory()->NewJSArrayBufferAndBackingStore(
            size, InitializedFlag::kUninitialized);
    Handle<JSArrayBuffer> array_buffer;
    if (!result.ToHandle(&array_buffer)) {
      thrower->RangeError("out of memory allocating custom section data");
      return Handle<JSArray>();
    }
    memcpy(array_buffer->backing_store(),
           wire_bytes.begin() + section.payload.offset(),
           section.payload.length());

    matching_sections.push_back(array_buffer);
  }

  int num_custom_sections = static_cast<int>(matching_sections.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_custom_sections);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_custom_sections));

  for (int i = 0; i < num_custom_sections; i++) {
    storage->set(i, *matching_sections[i]);
  }

  return array_object;
}

// Get the source position from a given function index and byte offset,
// for either asm.js or pure Wasm modules.
int GetSourcePosition(const WasmModule* module, uint32_t func_index,
                      uint32_t byte_offset, bool is_at_number_conversion) {
  DCHECK_EQ(is_asmjs_module(module),
            module->asm_js_offset_information != nullptr);
  if (!is_asmjs_module(module)) {
    // For non-asm.js modules, we just add the function's start offset
    // to make a module-relative position.
    return byte_offset + GetWasmFunctionOffset(module, func_index);
  }

  // asm.js modules have an additional offset table that must be searched.
  return module->asm_js_offset_information->GetSourcePosition(
      declared_function_index(module, func_index), byte_offset,
      is_at_number_conversion);
}

namespace {
template <typename T>
inline size_t VectorSize(const std::vector<T>& vector) {
  return sizeof(T) * vector.size();
}
}  // namespace

size_t EstimateStoredSize(const WasmModule* module) {
  return sizeof(WasmModule) + VectorSize(module->globals) +
         (module->signature_zone ? module->signature_zone->allocation_size()
                                 : 0) +
         VectorSize(module->types) +
         VectorSize(module->per_module_canonical_type_ids) +
         VectorSize(module->isorecursive_canonical_type_ids) +
         VectorSize(module->functions) + VectorSize(module->data_segments) +
         VectorSize(module->tables) + VectorSize(module->import_table) +
         VectorSize(module->export_table) + VectorSize(module->tags) +
         VectorSize(module->elem_segments);
}

size_t PrintSignature(base::Vector<char> buffer, const wasm::FunctionSig* sig,
                      char delimiter) {
  if (buffer.empty()) return 0;
  size_t old_size = buffer.size();
  auto append_char = [&buffer](char c) {
    if (buffer.size() == 1) return;  // Keep last character for '\0'.
    buffer[0] = c;
    buffer += 1;
  };
  for (wasm::ValueType t : sig->parameters()) {
    append_char(t.short_name());
  }
  append_char(delimiter);
  for (wasm::ValueType t : sig->returns()) {
    append_char(t.short_name());
  }
  buffer[0] = '\0';
  return old_size - buffer.size();
}

int JumpTableOffset(const WasmModule* module, int func_index) {
  return JumpTableAssembler::JumpSlotIndexToOffset(
      declared_function_index(module, func_index));
}

size_t GetWireBytesHash(base::Vector<const uint8_t> wire_bytes) {
  return StringHasher::HashSequentialString(
      reinterpret_cast<const char*>(wire_bytes.begin()), wire_bytes.length(),
      kZeroHashSeed);
}

base::OwnedVector<uint8_t> GetProfileData(const WasmModule* module) {
  const TypeFeedbackStorage& type_feedback = module->type_feedback;
  AccountingAllocator allocator;
  Zone zone{&allocator, "wasm::GetProfileData"};
  ZoneBuffer buffer{&zone};
  base::MutexGuard mutex_guard{&type_feedback.mutex};

  // Get an ordered list of function indexes, so we generate deterministic data.
  std::vector<uint32_t> ordered_func_indexes;
  ordered_func_indexes.reserve(type_feedback.feedback_for_function.size());
  for (const auto& entry : type_feedback.feedback_for_function) {
    ordered_func_indexes.push_back(entry.first);
  }
  std::sort(ordered_func_indexes.begin(), ordered_func_indexes.end());

  buffer.write_u32v(static_cast<uint32_t>(ordered_func_indexes.size()));
  for (const uint32_t func_index : ordered_func_indexes) {
    buffer.write_u32v(func_index);
    // Serialize {feedback_vector}.
    const FunctionTypeFeedback& feedback =
        type_feedback.feedback_for_function.at(func_index);
    buffer.write_u32v(static_cast<uint32_t>(feedback.feedback_vector.size()));
    for (const CallSiteFeedback& call_site_feedback :
         feedback.feedback_vector) {
      int cases = call_site_feedback.num_cases();
      buffer.write_i32v(cases);
      for (int i = 0; i < cases; ++i) {
        buffer.write_i32v(call_site_feedback.function_index(i));
        buffer.write_i32v(call_site_feedback.call_count(i));
      }
    }
    // Serialize {call_targets}.
    buffer.write_u32v(static_cast<uint32_t>(feedback.call_targets.size()));
    for (uint32_t call_target : feedback.call_targets) {
      buffer.write_u32v(call_target);
    }
  }
  return base::OwnedVector<uint8_t>::Of(buffer);
}

void RestoreProfileData(WasmModule* module,
                        base::Vector<uint8_t> profile_data) {
  TypeFeedbackStorage& type_feedback = module->type_feedback;
  Decoder decoder{profile_data.begin(), profile_data.end()};
  uint32_t num_entries = decoder.consume_u32v("num function entries");
  CHECK_LE(num_entries, module->num_declared_functions);
  for (uint32_t missing_entries = num_entries; missing_entries > 0;
       --missing_entries) {
    uint32_t function_index = decoder.consume_u32v("function index");
    CHECK(!type_feedback.feedback_for_function.count(function_index));
    FunctionTypeFeedback& feedback =
        type_feedback.feedback_for_function[function_index];
    // Deserialize {feedback_vector}.
    uint32_t feedback_vector_size =
        decoder.consume_u32v("feedback vector size");
    feedback.feedback_vector.resize(feedback_vector_size);
    for (CallSiteFeedback& feedback : feedback.feedback_vector) {
      int num_cases = decoder.consume_i32v("num cases");
      if (num_cases == 0) continue;  // no feedback
      if (num_cases == 1) {          // monomorphic
        int called_function_index = decoder.consume_i32v("function index");
        int call_count = decoder.consume_i32v("call count");
        feedback = CallSiteFeedback{called_function_index, call_count};
      } else {  // polymorphic
        auto* polymorphic = new CallSiteFeedback::PolymorphicCase[num_cases];
        for (int i = 0; i < num_cases; ++i) {
          polymorphic[i].function_index =
              decoder.consume_i32v("function index");
          polymorphic[i].absolute_call_frequency =
              decoder.consume_i32v("call count");
        }
        feedback = CallSiteFeedback{polymorphic, num_cases};
      }
    }
    // Deserialize {call_targets}.
    uint32_t num_call_targets = decoder.consume_u32v("num call targets");
    feedback.call_targets =
        base::OwnedVector<uint32_t>::NewForOverwrite(num_call_targets);
    for (uint32_t& call_target : feedback.call_targets) {
      call_target = decoder.consume_u32v("call target");
    }
  }
  CHECK(decoder.ok());
  CHECK_EQ(decoder.pc(), decoder.end());
}

void DumpProfileToFile(const WasmModule* module,
                       base::Vector<const uint8_t> wire_bytes) {
  CHECK(!wire_bytes.empty());
  // File are named `profile-wasm-<hash>`.
  // We use the same hash as for reported scripts, to make it easier to
  // correlate files to wasm modules (see {CreateWasmScript}).
  uint32_t hash = static_cast<uint32_t>(GetWireBytesHash(wire_bytes));
  base::EmbeddedVector<char, 32> filename;
  SNPrintF(filename, "profile-wasm-%08x", hash);
  base::OwnedVector<uint8_t> profile_data = GetProfileData(module);
  PrintF("Dumping Wasm PGO data to file '%s' (%zu bytes)\n", filename.begin(),
         profile_data.size());
  if (FILE* file = base::OS::FOpen(filename.begin(), "wb")) {
    CHECK_EQ(profile_data.size(),
             fwrite(profile_data.begin(), 1, profile_data.size(), file));
    base::Fclose(file);
  }
}

void LoadProfileFromFile(WasmModule* module,
                         base::Vector<const uint8_t> wire_bytes) {
  CHECK(!wire_bytes.empty());
  // File are named `profile-wasm-<hash>`.
  // We use the same hash as for reported scripts, to make it easier to
  // correlate files to wasm modules (see {CreateWasmScript}).
  uint32_t hash = static_cast<uint32_t>(GetWireBytesHash(wire_bytes));
  base::EmbeddedVector<char, 32> filename;
  SNPrintF(filename, "profile-wasm-%08x", hash);

  FILE* file = base::OS::FOpen(filename.begin(), "rb");
  if (!file) {
    PrintF("No Wasm PGO data found: Cannot open file '%s'\n", filename.begin());
    return;
  }

  fseek(file, 0, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  PrintF("Loading Wasm PGO data from file '%s' (%zu bytes)\n", filename.begin(),
         size);
  base::OwnedVector<uint8_t> profile_data =
      base::OwnedVector<uint8_t>::NewForOverwrite(size);
  for (size_t read = 0; read < size;) {
    read += fread(profile_data.begin() + read, 1, size - read, file);
    CHECK(!ferror(file));
  }

  base::Fclose(file);

  RestoreProfileData(module, profile_data.as_vector());

  // Check that the generated profile is deterministic.
  DCHECK_EQ(profile_data.as_vector(), GetProfileData(module).as_vector());
}

}  // namespace v8::internal::wasm
