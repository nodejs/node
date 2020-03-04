// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>

#include "src/api/api-inl.h"
#include "src/codegen/assembler-inl.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/interface-types.h"
#include "src/execution/frames-inl.h"
#include "src/execution/simulator.h"
#include "src/init/v8.h"
#include "src/objects/js-array-inl.h"
#include "src/objects/objects.h"
#include "src/objects/property-descriptor.h"
#include "src/snapshot/snapshot.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-manager.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects-inl.h"
#include "src/wasm/wasm-result.h"
#include "src/wasm/wasm-text.h"

namespace v8 {
namespace internal {
namespace wasm {

// static
const uint32_t WasmElemSegment::kNullIndex;

WireBytesRef WasmModule::LookupFunctionName(const ModuleWireBytes& wire_bytes,
                                            uint32_t function_index) const {
  if (!function_names) {
    function_names.reset(new std::unordered_map<uint32_t, WireBytesRef>());
    DecodeFunctionNames(wire_bytes.start(), wire_bytes.end(),
                        function_names.get());
  }
  auto it = function_names->find(function_index);
  if (it == function_names->end()) return WireBytesRef();
  return it->second;
}

// static
int MaxNumExportWrappers(const WasmModule* module) {
  // For each signature there may exist a wrapper, both for imported and
  // internal functions.
  return static_cast<int>(module->signature_map.size()) * 2;
}

// static
int GetExportWrapperIndex(const WasmModule* module, const FunctionSig* sig,
                          bool is_import) {
  int result = module->signature_map.Find(*sig);
  CHECK_GE(result, 0);
  result += is_import ? module->signature_map.size() : 0;
  return result;
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

// static
v8::debug::WasmDisassembly DisassembleWasmFunction(
    const WasmModule* module, const ModuleWireBytes& wire_bytes,
    int func_index) {
  if (func_index < 0 ||
      static_cast<uint32_t>(func_index) >= module->functions.size())
    return {};

  std::ostringstream disassembly_os;
  v8::debug::WasmDisassembly::OffsetTable offset_table;

  PrintWasmText(module, wire_bytes, static_cast<uint32_t>(func_index),
                disassembly_os, &offset_table);

  return {disassembly_os.str(), std::move(offset_table)};
}

void WasmModule::AddFunctionNameForTesting(int function_index,
                                           WireBytesRef name) {
  if (!function_names) {
    function_names.reset(new std::unordered_map<uint32_t, WireBytesRef>());
  }
  function_names->insert(std::make_pair(function_index, name));
}

// Get a string stored in the module bytes representing a name.
WasmName ModuleWireBytes::GetNameOrNull(WireBytesRef ref) const {
  if (!ref.is_set()) return {nullptr, 0};  // no name.
  CHECK(BoundsCheck(ref.offset(), ref.length()));
  return WasmName::cast(
      module_bytes_.SubVector(ref.offset(), ref.end_offset()));
}

// Get a string stored in the module bytes representing a function name.
WasmName ModuleWireBytes::GetNameOrNull(const WasmFunction* function,
                                        const WasmModule* module) const {
  return GetNameOrNull(module->LookupFunctionName(*this, function->func_index));
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

namespace {

// Converts the given {type} into a string representation that can be used in
// reflective functions. Should be kept in sync with the {GetValueType} helper.
Handle<String> ToValueTypeString(Isolate* isolate, ValueType type) {
  Factory* factory = isolate->factory();
  Handle<String> string;
  switch (type) {
    case i::wasm::kWasmI32: {
      string = factory->InternalizeUtf8String("i32");
      break;
    }
    case i::wasm::kWasmI64: {
      string = factory->InternalizeUtf8String("i64");
      break;
    }
    case i::wasm::kWasmF32: {
      string = factory->InternalizeUtf8String("f32");
      break;
    }
    case i::wasm::kWasmF64: {
      string = factory->InternalizeUtf8String("f64");
      break;
    }
    case i::wasm::kWasmAnyRef: {
      string = factory->InternalizeUtf8String("anyref");
      break;
    }
    case i::wasm::kWasmFuncRef: {
      string = factory->InternalizeUtf8String("anyfunc");
      break;
    }
    case i::wasm::kWasmExnRef: {
      string = factory->InternalizeUtf8String("exnref");
      break;
    }
    default:
      UNREACHABLE();
  }
  return string;
}

}  // namespace

Handle<JSObject> GetTypeForFunction(Isolate* isolate, FunctionSig* sig) {
  Factory* factory = isolate->factory();

  // Extract values for the {ValueType[]} arrays.
  int param_index = 0;
  int param_count = static_cast<int>(sig->parameter_count());
  Handle<FixedArray> param_values = factory->NewFixedArray(param_count);
  for (ValueType type : sig->parameters()) {
    Handle<String> type_value = ToValueTypeString(isolate, type);
    param_values->set(param_index++, *type_value);
  }
  int result_index = 0;
  int result_count = static_cast<int>(sig->return_count());
  Handle<FixedArray> result_values = factory->NewFixedArray(result_count);
  for (ValueType type : sig->returns()) {
    Handle<String> type_value = ToValueTypeString(isolate, type);
    result_values->set(result_index++, *type_value);
  }

  // Create the resulting {FunctionType} object.
  Handle<JSFunction> object_function = isolate->object_function();
  Handle<JSObject> object = factory->NewJSObject(object_function);
  Handle<JSArray> params = factory->NewJSArrayWithElements(param_values);
  Handle<JSArray> results = factory->NewJSArrayWithElements(result_values);
  Handle<String> params_string = factory->InternalizeUtf8String("parameters");
  Handle<String> results_string = factory->InternalizeUtf8String("results");
  JSObject::AddProperty(isolate, object, params_string, params, NONE);
  JSObject::AddProperty(isolate, object, results_string, results, NONE);

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
                                  base::Optional<uint32_t> max_size) {
  Factory* factory = isolate->factory();

  Handle<JSFunction> object_function = isolate->object_function();
  Handle<JSObject> object = factory->NewJSObject(object_function);
  Handle<String> minimum_string = factory->InternalizeUtf8String("minimum");
  Handle<String> maximum_string = factory->InternalizeUtf8String("maximum");
  JSObject::AddProperty(isolate, object, minimum_string,
                        factory->NewNumberFromUint(min_size), NONE);
  if (max_size.has_value()) {
    JSObject::AddProperty(isolate, object, maximum_string,
                          factory->NewNumberFromUint(max_size.value()), NONE);
  }

  return object;
}

Handle<JSObject> GetTypeForTable(Isolate* isolate, ValueType type,
                                 uint32_t min_size,
                                 base::Optional<uint32_t> max_size) {
  Factory* factory = isolate->factory();

  Handle<String> element;
  if (type == ValueType::kWasmFuncRef) {
    // TODO(wasm): We should define the "anyfunc" string in one central place
    // and then use that constant everywhere.
    element = factory->InternalizeUtf8String("anyfunc");
  } else {
    DCHECK(WasmFeatures::FromFlags().has_anyref() &&
           type == ValueType::kWasmAnyRef);
    element = factory->InternalizeUtf8String("anyref");
  }

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
  Handle<String> exception_string = factory->InternalizeUtf8String("exception");

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
              GetTypeForMemory(isolate, module->initial_pages, maximum_size);
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
      case kExternalException:
        import_kind = exception_string;
        break;
      default:
        UNREACHABLE();
    }

    MaybeHandle<String> import_module =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, import.module_name);

    MaybeHandle<String> import_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, import.field_name);

    JSObject::AddProperty(isolate, entry, module_string,
                          import_module.ToHandleChecked(), NONE);
    JSObject::AddProperty(isolate, entry, name_string,
                          import_name.ToHandleChecked(), NONE);
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
  Handle<String> exception_string = factory->InternalizeUtf8String("exception");

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
              GetTypeForMemory(isolate, module->initial_pages, maximum_size);
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
      case kExternalException:
        export_kind = exception_string;
        break;
      default:
        UNREACHABLE();
    }

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    MaybeHandle<String> export_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, exp.name);

    JSObject::AddProperty(isolate, entry, name_string,
                          export_name.ToHandleChecked(), NONE);
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

  Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  std::vector<CustomSectionOffset> custom_sections =
      DecodeCustomSections(wire_bytes.begin(), wire_bytes.end());

  std::vector<Handle<Object>> matching_sections;

  // Gather matching sections.
  for (auto& section : custom_sections) {
    MaybeHandle<String> section_name =
        WasmModuleObject::ExtractUtf8StringFromModuleBytes(
            isolate, module_object, section.name);

    if (!name->Equals(*section_name.ToHandleChecked())) continue;

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

Handle<FixedArray> DecodeLocalNames(Isolate* isolate,
                                    Handle<WasmModuleObject> module_object) {
  Vector<const uint8_t> wire_bytes =
      module_object->native_module()->wire_bytes();
  LocalNames decoded_locals;
  DecodeLocalNames(wire_bytes.begin(), wire_bytes.end(), &decoded_locals);
  Handle<FixedArray> locals_names =
      isolate->factory()->NewFixedArray(decoded_locals.max_function_index + 1);
  for (LocalNamesPerFunction& func : decoded_locals.names) {
    Handle<FixedArray> func_locals_names =
        isolate->factory()->NewFixedArray(func.max_local_index + 1);
    locals_names->set(func.function_index, *func_locals_names);
    for (LocalName& name : func.names) {
      Handle<String> name_str =
          WasmModuleObject::ExtractUtf8StringFromModuleBytes(
              isolate, module_object, name.name)
              .ToHandleChecked();
      func_locals_names->set(name.local_index, *name_str);
    }
  }
  return locals_names;
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
         VectorSize(module->signatures) + VectorSize(module->signature_ids) +
         VectorSize(module->functions) + VectorSize(module->data_segments) +
         VectorSize(module->tables) + VectorSize(module->import_table) +
         VectorSize(module->export_table) + VectorSize(module->exceptions) +
         VectorSize(module->elem_segments);
}

size_t PrintSignature(Vector<char> buffer, wasm::FunctionSig* sig) {
  if (buffer.empty()) return 0;
  size_t old_size = buffer.size();
  auto append_char = [&buffer](char c) {
    if (buffer.size() == 1) return;  // Keep last character for '\0'.
    buffer[0] = c;
    buffer += 1;
  };
  for (wasm::ValueType t : sig->parameters()) {
    append_char(wasm::ValueTypes::ShortNameOf(t));
  }
  append_char(':');
  for (wasm::ValueType t : sig->returns()) {
    append_char(wasm::ValueTypes::ShortNameOf(t));
  }
  buffer[0] = '\0';
  return old_size - buffer.size();
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
