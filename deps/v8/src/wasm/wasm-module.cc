// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <functional>
#include <memory>

#include "src/code-stubs.h"
#include "src/debug/interface-types.h"
#include "src/frames-inl.h"
#include "src/objects.h"
#include "src/property-descriptor.h"
#include "src/simulator.h"
#include "src/snapshot/snapshot.h"
#include "src/v8.h"

#include "src/wasm/compilation-manager.h"
#include "src/wasm/module-compiler.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-code-specialization.h"
#include "src/wasm/wasm-js.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/wasm/wasm-result.h"

#if __clang__
// TODO(mostynb@opera.com): remove the using statements and these pragmas.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wheader-hygiene"
#endif

using namespace v8::internal;
using namespace v8::internal::wasm;
namespace base = v8::base;

#if __clang__
// TODO(mostynb@opera.com): remove the using statements and these pragmas.
#pragma clang diagnostic pop
#endif

#define TRACE(...)                                      \
  do {                                                  \
    if (FLAG_trace_wasm_instances) PrintF(__VA_ARGS__); \
  } while (false)

#define TRACE_CHAIN(instance)        \
  do {                               \
    instance->PrintInstancesChain(); \
  } while (false)

#define TRACE_COMPILE(...)                             \
  do {                                                 \
    if (FLAG_trace_wasm_compiler) PrintF(__VA_ARGS__); \
  } while (false)

namespace {


void* TryAllocateBackingStore(Isolate* isolate, size_t size,
                              bool enable_guard_regions, void*& allocation_base,
                              size_t& allocation_length) {
  // TODO(eholk): Right now enable_guard_regions has no effect on 32-bit
  // systems. It may be safer to fail instead, given that other code might do
  // things that would be unsafe if they expected guard pages where there
  // weren't any.
  if (enable_guard_regions && kGuardRegionsSupported) {
    // TODO(eholk): On Windows we want to make sure we don't commit the guard
    // pages yet.

    // We always allocate the largest possible offset into the heap, so the
    // addressable memory after the guard page can be made inaccessible.
    allocation_length = RoundUp(kWasmMaxHeapOffset, base::OS::CommitPageSize());
    DCHECK_EQ(0, size % base::OS::CommitPageSize());

    // AllocateGuarded makes the whole region inaccessible by default.
    allocation_base =
        isolate->array_buffer_allocator()->Reserve(allocation_length);
    if (allocation_base == nullptr) {
      return nullptr;
    }

    void* memory = allocation_base;

    // Make the part we care about accessible.
    isolate->array_buffer_allocator()->SetProtection(
        memory, size, v8::ArrayBuffer::Allocator::Protection::kReadWrite);

    reinterpret_cast<v8::Isolate*>(isolate)
        ->AdjustAmountOfExternalAllocatedMemory(size);

    return memory;
  } else {
    void* memory =
        size == 0 ? nullptr : isolate->array_buffer_allocator()->Allocate(size);
    allocation_base = memory;
    allocation_length = size;
    return memory;
  }
}

static void InstanceFinalizer(const v8::WeakCallbackInfo<void>& data) {
  DisallowHeapAllocation no_gc;
  JSObject** p = reinterpret_cast<JSObject**>(data.GetParameter());
  WasmInstanceObject* owner = reinterpret_cast<WasmInstanceObject*>(*p);
  Isolate* isolate = reinterpret_cast<Isolate*>(data.GetIsolate());
  // If a link to shared memory instances exists, update the list of memory
  // instances before the instance is destroyed.
  WasmCompiledModule* compiled_module = owner->compiled_module();
  TRACE("Finalizing %d {\n", compiled_module->instance_id());
  DCHECK(compiled_module->has_weak_wasm_module());
  WeakCell* weak_wasm_module = compiled_module->ptr_to_weak_wasm_module();

  if (trap_handler::UseTrapHandler()) {
    Handle<FixedArray> code_table = compiled_module->code_table();
    for (int i = 0; i < code_table->length(); ++i) {
      Handle<Code> code = code_table->GetValueChecked<Code>(isolate, i);
      int index = code->trap_handler_index()->value();
      if (index >= 0) {
        trap_handler::ReleaseHandlerData(index);
        code->set_trap_handler_index(Smi::FromInt(-1));
      }
    }
  }

  // Since the order of finalizers is not guaranteed, it can be the case
  // that {instance->compiled_module()->module()}, which is a
  // {Managed<WasmModule>} has been collected earlier in this GC cycle.
  // Weak references to this instance won't be cleared until
  // the next GC cycle, so we need to manually break some links (such as
  // the weak references from {WasmMemoryObject::instances}.
  if (owner->has_memory_object()) {
    Handle<WasmMemoryObject> memory(owner->memory_object(), isolate);
    Handle<WasmInstanceObject> instance(owner, isolate);
    WasmMemoryObject::RemoveInstance(isolate, memory, instance);
  }

  // weak_wasm_module may have been cleared, meaning the module object
  // was GC-ed. In that case, there won't be any new instances created,
  // and we don't need to maintain the links between instances.
  if (!weak_wasm_module->cleared()) {
    WasmModuleObject* wasm_module =
        WasmModuleObject::cast(weak_wasm_module->value());
    WasmCompiledModule* current_template = wasm_module->compiled_module();

    TRACE("chain before {\n");
    TRACE_CHAIN(current_template);
    TRACE("}\n");

    DCHECK(!current_template->has_weak_prev_instance());
    WeakCell* next = compiled_module->maybe_ptr_to_weak_next_instance();
    WeakCell* prev = compiled_module->maybe_ptr_to_weak_prev_instance();

    if (current_template == compiled_module) {
      if (next == nullptr) {
        WasmCompiledModule::Reset(isolate, compiled_module);
      } else {
        WasmCompiledModule* next_compiled_module =
            WasmCompiledModule::cast(next->value());
        WasmModuleObject::cast(wasm_module)
            ->set_compiled_module(next_compiled_module);
        DCHECK_NULL(prev);
        next_compiled_module->reset_weak_prev_instance();
      }
    } else {
      DCHECK(!(prev == nullptr && next == nullptr));
      // the only reason prev or next would be cleared is if the
      // respective objects got collected, but if that happened,
      // we would have relinked the list.
      if (prev != nullptr) {
        DCHECK(!prev->cleared());
        if (next == nullptr) {
          WasmCompiledModule::cast(prev->value())->reset_weak_next_instance();
        } else {
          WasmCompiledModule::cast(prev->value())
              ->set_ptr_to_weak_next_instance(next);
        }
      }
      if (next != nullptr) {
        DCHECK(!next->cleared());
        if (prev == nullptr) {
          WasmCompiledModule::cast(next->value())->reset_weak_prev_instance();
        } else {
          WasmCompiledModule::cast(next->value())
              ->set_ptr_to_weak_prev_instance(prev);
        }
      }
    }
    TRACE("chain after {\n");
    TRACE_CHAIN(wasm_module->compiled_module());
    TRACE("}\n");
  }
  compiled_module->reset_weak_owning_instance();
  GlobalHandles::Destroy(reinterpret_cast<Object**>(p));
  TRACE("}\n");
}

int AdvanceSourcePositionTableIterator(SourcePositionTableIterator& iterator,
                                       int offset) {
  DCHECK(!iterator.done());
  int byte_pos;
  do {
    byte_pos = iterator.source_position().ScriptOffset();
    iterator.Advance();
  } while (!iterator.done() && iterator.code_offset() <= offset);
  return byte_pos;
}

void RecordLazyCodeStats(Code* code, Counters* counters) {
  counters->wasm_lazily_compiled_functions()->Increment();
  counters->wasm_generated_code_size()->Increment(code->body_size());
  counters->wasm_reloc_size()->Increment(code->relocation_info()->length());
}

compiler::ModuleEnv CreateModuleEnvFromCompiledModule(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  DisallowHeapAllocation no_gc;
  WasmModule* module = compiled_module->module();

  std::vector<GlobalHandleAddress> function_tables;
  std::vector<GlobalHandleAddress> signature_tables;
  std::vector<SignatureMap*> signature_maps;

  int num_function_tables = static_cast<int>(module->function_tables.size());
  for (int i = 0; i < num_function_tables; ++i) {
    FixedArray* ft = compiled_module->ptr_to_function_tables();
    FixedArray* st = compiled_module->ptr_to_signature_tables();

    // TODO(clemensh): defer these handles for concurrent compilation.
    function_tables.push_back(WasmCompiledModule::GetTableValue(ft, i));
    signature_tables.push_back(WasmCompiledModule::GetTableValue(st, i));
    signature_maps.push_back(&module->function_tables[i].map);
  }

  std::vector<Handle<Code>> empty_code;

  compiler::ModuleEnv result = {
      module,                                             // --
      function_tables,                                    // --
      signature_tables,                                   // --
      signature_maps,                                     // --
      empty_code,                                         // --
      BUILTIN_CODE(isolate, WasmCompileLazy),             // --
      reinterpret_cast<uintptr_t>(                        // --
          compiled_module->GetEmbeddedMemStartOrNull()),  // --
      compiled_module->GetEmbeddedMemSizeOrZero(),        // --
      reinterpret_cast<uintptr_t>(                        // --
          compiled_module->GetGlobalsStartOrNull())       // --
  };
  return result;
}

}  // namespace

// static
const WasmExceptionSig wasm::WasmException::empty_sig_(0, 0, nullptr);

Handle<JSArrayBuffer> wasm::SetupArrayBuffer(
    Isolate* isolate, void* allocation_base, size_t allocation_length,
    void* backing_store, size_t size, bool is_external,
    bool enable_guard_regions, SharedFlag shared) {
  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer(shared);
  DCHECK_GE(kMaxInt, size);
  if (shared == SharedFlag::kShared) DCHECK(FLAG_experimental_wasm_threads);
  JSArrayBuffer::Setup(buffer, isolate, is_external, allocation_base,
                       allocation_length, backing_store, static_cast<int>(size),
                       shared);
  buffer->set_is_neuterable(false);
  buffer->set_is_growable(true);
  buffer->set_has_guard_region(enable_guard_regions);
  return buffer;
}

Handle<JSArrayBuffer> wasm::NewArrayBuffer(Isolate* isolate, size_t size,
                                           bool enable_guard_regions,
                                           SharedFlag shared) {
  // Check against kMaxInt, since the byte length is stored as int in the
  // JSArrayBuffer. Note that wasm_max_mem_pages can be raised from the command
  // line, and we don't want to fail a CHECK then.
  if (size > FLAG_wasm_max_mem_pages * WasmModule::kPageSize ||
      size > kMaxInt) {
    // TODO(titzer): lift restriction on maximum memory allocated here.
    return Handle<JSArrayBuffer>::null();
  }

  enable_guard_regions = enable_guard_regions && kGuardRegionsSupported;

  void* allocation_base = nullptr;  // Set by TryAllocateBackingStore
  size_t allocation_length = 0;     // Set by TryAllocateBackingStore
  // Do not reserve memory till non zero memory is encountered.
  void* memory =
      (size == 0) ? nullptr
                  : TryAllocateBackingStore(isolate, size, enable_guard_regions,
                                            allocation_base, allocation_length);

  if (size > 0 && memory == nullptr) {
    return Handle<JSArrayBuffer>::null();
  }

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  const byte* bytes = reinterpret_cast<const byte*>(memory);
  for (size_t i = 0; i < size; ++i) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif

  constexpr bool is_external = false;
  return SetupArrayBuffer(isolate, allocation_base, allocation_length, memory,
                          size, is_external, enable_guard_regions, shared);
}

void wasm::UnpackAndRegisterProtectedInstructions(
    Isolate* isolate, Handle<FixedArray> code_table) {
  for (int i = 0; i < code_table->length(); ++i) {
    Handle<Code> code;
    // This is sometimes undefined when we're called from cctests.
    if (!code_table->GetValue<Code>(isolate, i).ToHandle(&code)) {
      continue;
    }

    if (code->kind() != Code::WASM_FUNCTION) {
      continue;
    }

    const intptr_t base = reinterpret_cast<intptr_t>(code->entry());

    Zone zone(isolate->allocator(), "Wasm Module");
    ZoneVector<trap_handler::ProtectedInstructionData> unpacked(&zone);
    const int mode_mask =
        RelocInfo::ModeMask(RelocInfo::WASM_PROTECTED_INSTRUCTION_LANDING);
    for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
      trap_handler::ProtectedInstructionData data;
      data.instr_offset = it.rinfo()->data();
      data.landing_offset = reinterpret_cast<intptr_t>(it.rinfo()->pc()) - base;
      unpacked.emplace_back(data);
    }
    if (unpacked.size() > 0) {
      int size = code->CodeSize();
      const int index = RegisterHandlerData(reinterpret_cast<void*>(base), size,
                                            unpacked.size(), &unpacked[0]);
      // TODO(eholk): if index is negative, fail.
      DCHECK(index >= 0);
      code->set_trap_handler_index(Smi::FromInt(index));
    }
  }
}

std::ostream& wasm::operator<<(std::ostream& os, const WasmFunctionName& name) {
  os << "#" << name.function_->func_index;
  if (name.function_->name.is_set()) {
    if (name.name_.start()) {
      os << ":";
      os.write(name.name_.start(), name.name_.length());
    }
  } else {
    os << "?";
  }
  return os;
}

WasmInstanceObject* wasm::GetOwningWasmInstance(Code* code) {
  DisallowHeapAllocation no_gc;
  DCHECK(code->kind() == Code::WASM_FUNCTION ||
         code->kind() == Code::WASM_INTERPRETER_ENTRY);
  FixedArray* deopt_data = code->deoptimization_data();
  DCHECK_EQ(code->kind() == Code::WASM_INTERPRETER_ENTRY ? 1 : 2,
            deopt_data->length());
  Object* weak_link = deopt_data->get(0);
  DCHECK(weak_link->IsWeakCell());
  WeakCell* cell = WeakCell::cast(weak_link);
  if (cell->cleared()) return nullptr;
  return WasmInstanceObject::cast(cell->value());
}

WasmModule::WasmModule(std::unique_ptr<Zone> owned)
    : signature_zone(std::move(owned)) {}

WasmFunction* wasm::GetWasmFunctionForImportWrapper(Isolate* isolate,
                                                    Handle<Object> target) {
  if (target->IsJSFunction()) {
    Handle<JSFunction> func = Handle<JSFunction>::cast(target);
    if (func->code()->kind() == Code::JS_TO_WASM_FUNCTION) {
      auto exported = Handle<WasmExportedFunction>::cast(func);
      Handle<WasmInstanceObject> other_instance(exported->instance(), isolate);
      int func_index = exported->function_index();
      return &other_instance->module()->functions[func_index];
    }
  }
  return nullptr;
}

Handle<Code> wasm::UnwrapImportWrapper(Handle<Object> import_wrapper) {
  Handle<JSFunction> func = Handle<JSFunction>::cast(import_wrapper);
  Handle<Code> export_wrapper_code = handle(func->code());
  int mask = RelocInfo::ModeMask(RelocInfo::CODE_TARGET);
  for (RelocIterator it(*export_wrapper_code, mask);; it.next()) {
    DCHECK(!it.done());
    Code* target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
    if (target->kind() != Code::WASM_FUNCTION &&
        target->kind() != Code::WASM_TO_JS_FUNCTION &&
        target->kind() != Code::WASM_INTERPRETER_ENTRY)
      continue;
// There should only be this one call to wasm code.
#ifdef DEBUG
    for (it.next(); !it.done(); it.next()) {
      Code* code = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      DCHECK(code->kind() != Code::WASM_FUNCTION &&
             code->kind() != Code::WASM_TO_JS_FUNCTION &&
             code->kind() != Code::WASM_INTERPRETER_ENTRY);
    }
#endif
    return handle(target);
  }
  UNREACHABLE();
}

void wasm::UpdateDispatchTables(Isolate* isolate,
                                Handle<FixedArray> dispatch_tables, int index,
                                WasmFunction* function, Handle<Code> code) {
  DCHECK_EQ(0, dispatch_tables->length() % 4);
  for (int i = 0; i < dispatch_tables->length(); i += 4) {
    int table_index = Smi::ToInt(dispatch_tables->get(i + 1));
    Handle<FixedArray> function_table(
        FixedArray::cast(dispatch_tables->get(i + 2)), isolate);
    Handle<FixedArray> signature_table(
        FixedArray::cast(dispatch_tables->get(i + 3)), isolate);
    if (function) {
      // TODO(titzer): the signature might need to be copied to avoid
      // a dangling pointer in the signature map.
      Handle<WasmInstanceObject> instance(
          WasmInstanceObject::cast(dispatch_tables->get(i)), isolate);
      auto& func_table = instance->module()->function_tables[table_index];
      uint32_t sig_index = func_table.map.FindOrInsert(function->sig);
      signature_table->set(index, Smi::FromInt(static_cast<int>(sig_index)));
      function_table->set(index, *code);
    } else {
      signature_table->set(index, Smi::FromInt(-1));
      function_table->set(index, Smi::kZero);
    }
  }
}


void wasm::TableSet(ErrorThrower* thrower, Isolate* isolate,
                    Handle<WasmTableObject> table, int64_t index,
                    Handle<JSFunction> function) {
  Handle<FixedArray> array(table->functions(), isolate);

  if (index < 0 || index >= array->length()) {
    thrower->RangeError("index out of bounds");
    return;
  }
  int index32 = static_cast<int>(index);

  Handle<FixedArray> dispatch_tables(table->dispatch_tables(), isolate);

  WasmFunction* wasm_function = nullptr;
  Handle<Code> code = Handle<Code>::null();
  Handle<Object> value = handle(isolate->heap()->null_value());

  if (!function.is_null()) {
    wasm_function = GetWasmFunctionForImportWrapper(isolate, function);
    code = UnwrapImportWrapper(function);
    value = Handle<Object>::cast(function);
  }

  UpdateDispatchTables(isolate, dispatch_tables, index32, wasm_function, code);
  array->set(index32, *value);
}

Handle<Script> wasm::GetScript(Handle<JSObject> instance) {
  WasmCompiledModule* compiled_module =
      WasmInstanceObject::cast(*instance)->compiled_module();
  return handle(compiled_module->script());
}

bool wasm::IsWasmCodegenAllowed(Isolate* isolate, Handle<Context> context) {
  // TODO(wasm): Once wasm has its own CSP policy, we should introduce a
  // separate callback that includes information about the module about to be
  // compiled. For the time being, pass an empty string as placeholder for the
  // sources.
  return isolate->allow_code_gen_callback() == nullptr ||
         isolate->allow_code_gen_callback()(
             v8::Utils::ToLocal(context),
             v8::Utils::ToLocal(isolate->factory()->empty_string()));
}

void wasm::DetachWebAssemblyMemoryBuffer(Isolate* isolate,
                                         Handle<JSArrayBuffer> buffer,
                                         bool free_memory) {
  const bool is_external = buffer->is_external();
  DCHECK(!buffer->is_neuterable());
  if (!is_external) {
    buffer->set_is_external(true);
    isolate->heap()->UnregisterArrayBuffer(*buffer);
    if (free_memory) {
      // We need to free the memory before neutering the buffer because
      // FreeBackingStore reads buffer->allocation_base(), which is nulled out
      // by Neuter. This means there is a dangling pointer until we neuter the
      // buffer. Since there is no way for the user to directly call
      // FreeBackingStore, we can ensure this is safe.
      buffer->FreeBackingStore();
    }
  }
  buffer->set_is_neuterable(true);
  buffer->Neuter();
}

void testing::ValidateInstancesChain(Isolate* isolate,
                                     Handle<WasmModuleObject> module_obj,
                                     int instance_count) {
  CHECK_GE(instance_count, 0);
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK_EQ(JSObject::cast(compiled_module->ptr_to_weak_wasm_module()->value()),
           *module_obj);
  Object* prev = nullptr;
  int found_instances = compiled_module->has_weak_owning_instance() ? 1 : 0;
  WasmCompiledModule* current_instance = compiled_module;
  while (current_instance->has_weak_next_instance()) {
    CHECK((prev == nullptr && !current_instance->has_weak_prev_instance()) ||
          current_instance->ptr_to_weak_prev_instance()->value() == prev);
    CHECK_EQ(current_instance->ptr_to_weak_wasm_module()->value(), *module_obj);
    CHECK(current_instance->ptr_to_weak_owning_instance()
              ->value()
              ->IsWasmInstanceObject());
    prev = current_instance;
    current_instance = WasmCompiledModule::cast(
        current_instance->ptr_to_weak_next_instance()->value());
    ++found_instances;
    CHECK_LE(found_instances, instance_count);
  }
  CHECK_EQ(found_instances, instance_count);
}

void testing::ValidateModuleState(Isolate* isolate,
                                  Handle<WasmModuleObject> module_obj) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = module_obj->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK_EQ(compiled_module->ptr_to_weak_wasm_module()->value(), *module_obj);
  CHECK(!compiled_module->has_weak_prev_instance());
  CHECK(!compiled_module->has_weak_next_instance());
  CHECK(!compiled_module->has_weak_owning_instance());
}

void testing::ValidateOrphanedInstance(Isolate* isolate,
                                       Handle<WasmInstanceObject> instance) {
  DisallowHeapAllocation no_gc;
  WasmCompiledModule* compiled_module = instance->compiled_module();
  CHECK(compiled_module->has_weak_wasm_module());
  CHECK(compiled_module->ptr_to_weak_wasm_module()->cleared());
}

Handle<JSArray> wasm::GetImports(Isolate* isolate,
                                 Handle<WasmModuleObject> module_object) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  Handle<String> module_string = factory->InternalizeUtf8String("module");
  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");

  // Create the result array.
  WasmModule* module = compiled_module->module();
  int num_imports = static_cast<int>(module->import_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_imports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_imports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_imports; ++index) {
    WasmImport& import = module->import_table[index];

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    Handle<String> import_kind;
    switch (import.kind) {
      case kExternalFunction:
        import_kind = function_string;
        break;
      case kExternalTable:
        import_kind = table_string;
        break;
      case kExternalMemory:
        import_kind = memory_string;
        break;
      case kExternalGlobal:
        import_kind = global_string;
        break;
      default:
        UNREACHABLE();
    }

    MaybeHandle<String> import_module =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.module_name);

    MaybeHandle<String> import_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, import.field_name);

    JSObject::AddProperty(entry, module_string, import_module.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, name_string, import_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, import_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> wasm::GetExports(Isolate* isolate,
                                 Handle<WasmModuleObject> module_object) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  Handle<String> name_string = factory->InternalizeUtf8String("name");
  Handle<String> kind_string = factory->InternalizeUtf8String("kind");

  Handle<String> function_string = factory->InternalizeUtf8String("function");
  Handle<String> table_string = factory->InternalizeUtf8String("table");
  Handle<String> memory_string = factory->InternalizeUtf8String("memory");
  Handle<String> global_string = factory->InternalizeUtf8String("global");

  // Create the result array.
  WasmModule* module = compiled_module->module();
  int num_exports = static_cast<int>(module->export_table.size());
  Handle<JSArray> array_object = factory->NewJSArray(PACKED_ELEMENTS, 0, 0);
  Handle<FixedArray> storage = factory->NewFixedArray(num_exports);
  JSArray::SetContent(array_object, storage);
  array_object->set_length(Smi::FromInt(num_exports));

  Handle<JSFunction> object_function =
      Handle<JSFunction>(isolate->native_context()->object_function(), isolate);

  // Populate the result array.
  for (int index = 0; index < num_exports; ++index) {
    WasmExport& exp = module->export_table[index];

    Handle<String> export_kind;
    switch (exp.kind) {
      case kExternalFunction:
        export_kind = function_string;
        break;
      case kExternalTable:
        export_kind = table_string;
        break;
      case kExternalMemory:
        export_kind = memory_string;
        break;
      case kExternalGlobal:
        export_kind = global_string;
        break;
      default:
        UNREACHABLE();
    }

    Handle<JSObject> entry = factory->NewJSObject(object_function);

    MaybeHandle<String> export_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, exp.name);

    JSObject::AddProperty(entry, name_string, export_name.ToHandleChecked(),
                          NONE);
    JSObject::AddProperty(entry, kind_string, export_kind, NONE);

    storage->set(index, *entry);
  }

  return array_object;
}

Handle<JSArray> wasm::GetCustomSections(Isolate* isolate,
                                        Handle<WasmModuleObject> module_object,
                                        Handle<String> name,
                                        ErrorThrower* thrower) {
  Handle<WasmCompiledModule> compiled_module(module_object->compiled_module(),
                                             isolate);
  Factory* factory = isolate->factory();

  std::vector<CustomSectionOffset> custom_sections;
  {
    DisallowHeapAllocation no_gc;  // for raw access to string bytes.
    Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                          isolate);
    const byte* start =
        reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
    const byte* end = start + module_bytes->length();
    custom_sections = DecodeCustomSections(start, end);
  }

  std::vector<Handle<Object>> matching_sections;

  // Gather matching sections.
  for (auto& section : custom_sections) {
    MaybeHandle<String> section_name =
        WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
            isolate, compiled_module, section.name);

    if (!name->Equals(*section_name.ToHandleChecked())) continue;

    // Make a copy of the payload data in the section.
    size_t size = section.payload.length();
    void* memory =
        size == 0 ? nullptr : isolate->array_buffer_allocator()->Allocate(size);

    if (size && !memory) {
      thrower->RangeError("out of memory allocating custom section data");
      return Handle<JSArray>();
    }
    Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
    constexpr bool is_external = false;
    JSArrayBuffer::Setup(buffer, isolate, is_external, memory, size, memory,
                         size);
    DisallowHeapAllocation no_gc;  // for raw access to string bytes.
    Handle<SeqOneByteString> module_bytes(compiled_module->module_bytes(),
                                          isolate);
    const byte* start =
        reinterpret_cast<const byte*>(module_bytes->GetCharsAddress());
    memcpy(memory, start + section.payload.offset(), section.payload.length());

    matching_sections.push_back(buffer);
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

Handle<FixedArray> wasm::DecodeLocalNames(
    Isolate* isolate, Handle<WasmCompiledModule> compiled_module) {
  Handle<SeqOneByteString> wire_bytes(compiled_module->module_bytes(), isolate);
  LocalNames decoded_locals;
  {
    DisallowHeapAllocation no_gc;
    wasm::DecodeLocalNames(wire_bytes->GetChars(),
                           wire_bytes->GetChars() + wire_bytes->length(),
                           &decoded_locals);
  }
  Handle<FixedArray> locals_names =
      isolate->factory()->NewFixedArray(decoded_locals.max_function_index + 1);
  for (LocalNamesPerFunction& func : decoded_locals.names) {
    Handle<FixedArray> func_locals_names =
        isolate->factory()->NewFixedArray(func.max_local_index + 1);
    locals_names->set(func.function_index, *func_locals_names);
    for (LocalName& name : func.names) {
      Handle<String> name_str =
          WasmCompiledModule::ExtractUtf8StringFromModuleBytes(
              isolate, compiled_module, name.name)
              .ToHandleChecked();
      func_locals_names->set(name.local_index, *name_str);
    }
  }
  return locals_names;
}

bool wasm::SyncValidate(Isolate* isolate, const ModuleWireBytes& bytes) {
  if (bytes.start() == nullptr || bytes.length() == 0) return false;
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), true, kWasmOrigin);
  return result.ok();
}

MaybeHandle<WasmModuleObject> wasm::SyncCompileTranslatedAsmJs(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    Handle<Script> asm_js_script,
    Vector<const byte> asm_js_offset_table_bytes) {
  ModuleResult result = SyncDecodeWasmModule(isolate, bytes.start(),
                                             bytes.end(), false, kAsmJsOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership to the {WasmModuleWrapper} generated in
  // {CompileToModuleObject}.
  Handle<Code> centry_stub = CEntryStub(isolate, 1).GetCode();
  ModuleCompiler compiler(isolate, std::move(result.val), centry_stub);
  return compiler.CompileToModuleObject(thrower, bytes, asm_js_script,
                                        asm_js_offset_table_bytes);
}

MaybeHandle<WasmModuleObject> wasm::SyncCompile(Isolate* isolate,
                                                ErrorThrower* thrower,
                                                const ModuleWireBytes& bytes) {
  if (!IsWasmCodegenAllowed(isolate, isolate->native_context())) {
    thrower->CompileError("Wasm code generation disallowed in this context");
    return {};
  }

  // TODO(titzer): only make a copy of the bytes if SharedArrayBuffer
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());
  ModuleWireBytes bytes_copy(copy.get(), copy.get() + bytes.length());

  ModuleResult result = SyncDecodeWasmModule(
      isolate, bytes_copy.start(), bytes_copy.end(), false, kWasmOrigin);
  if (result.failed()) {
    thrower->CompileFailed("Wasm decoding failed", result);
    return {};
  }

  // Transfer ownership to the {WasmModuleWrapper} generated in
  // {CompileToModuleObject}.
  Handle<Code> centry_stub = CEntryStub(isolate, 1).GetCode();
  ModuleCompiler compiler(isolate, std::move(result.val), centry_stub);
  return compiler.CompileToModuleObject(thrower, bytes_copy, Handle<Script>(),
                                        Vector<const byte>());
}

MaybeHandle<WasmInstanceObject> wasm::SyncInstantiate(
    Isolate* isolate, ErrorThrower* thrower,
    Handle<WasmModuleObject> module_object, MaybeHandle<JSReceiver> imports,
    MaybeHandle<JSArrayBuffer> memory) {
  InstanceBuilder builder(isolate, thrower, module_object, imports, memory,
                          &InstanceFinalizer);
  return builder.Build();
}

MaybeHandle<WasmInstanceObject> wasm::SyncCompileAndInstantiate(
    Isolate* isolate, ErrorThrower* thrower, const ModuleWireBytes& bytes,
    MaybeHandle<JSReceiver> imports, MaybeHandle<JSArrayBuffer> memory) {
  MaybeHandle<WasmModuleObject> module =
      wasm::SyncCompile(isolate, thrower, bytes);
  DCHECK_EQ(thrower->error(), module.is_null());
  if (module.is_null()) return {};

  return wasm::SyncInstantiate(isolate, thrower, module.ToHandleChecked(),
                               Handle<JSReceiver>::null(),
                               Handle<JSArrayBuffer>::null());
}

namespace v8 {
namespace internal {
namespace wasm {

void RejectPromise(Isolate* isolate, Handle<Context> context,
                   ErrorThrower& thrower, Handle<JSPromise> promise) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Utils::PromiseToLocal(promise).As<v8::Promise::Resolver>();
  auto maybe = resolver->Reject(v8::Utils::ToLocal(context),
                                v8::Utils::ToLocal(thrower.Reify()));
  CHECK_IMPLIES(!maybe.FromMaybe(false), isolate->has_scheduled_exception());
}

void ResolvePromise(Isolate* isolate, Handle<Context> context,
                    Handle<JSPromise> promise, Handle<Object> result) {
  v8::Local<v8::Promise::Resolver> resolver =
      v8::Utils::PromiseToLocal(promise).As<v8::Promise::Resolver>();
  auto maybe = resolver->Resolve(v8::Utils::ToLocal(context),
                                 v8::Utils::ToLocal(result));
  CHECK_IMPLIES(!maybe.FromMaybe(false), isolate->has_scheduled_exception());
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8

void wasm::AsyncInstantiate(Isolate* isolate, Handle<JSPromise> promise,
                            Handle<WasmModuleObject> module_object,
                            MaybeHandle<JSReceiver> imports) {
  ErrorThrower thrower(isolate, nullptr);
  MaybeHandle<WasmInstanceObject> instance_object = SyncInstantiate(
      isolate, &thrower, module_object, imports, Handle<JSArrayBuffer>::null());
  if (thrower.error()) {
    RejectPromise(isolate, handle(isolate->context()), thrower, promise);
    return;
  }
  ResolvePromise(isolate, handle(isolate->context()), promise,
                 instance_object.ToHandleChecked());
}

void wasm::AsyncCompile(Isolate* isolate, Handle<JSPromise> promise,
                        const ModuleWireBytes& bytes) {
  if (!FLAG_wasm_async_compilation) {
    ErrorThrower thrower(isolate, "WasmCompile");
    // Compile the module.
    MaybeHandle<WasmModuleObject> module_object =
        SyncCompile(isolate, &thrower, bytes);
    if (thrower.error()) {
      RejectPromise(isolate, handle(isolate->context()), thrower, promise);
      return;
    }
    Handle<WasmModuleObject> module = module_object.ToHandleChecked();
    ResolvePromise(isolate, handle(isolate->context()), promise, module);
    return;
  }

  // Make a copy of the wire bytes in case the user program changes them
  // during asynchronous compilation.
  std::unique_ptr<byte[]> copy(new byte[bytes.length()]);
  memcpy(copy.get(), bytes.start(), bytes.length());
  isolate->wasm_compilation_manager()->StartAsyncCompileJob(
      isolate, std::move(copy), bytes.length(), handle(isolate->context()),
      promise);
}

Handle<Code> wasm::CompileLazy(Isolate* isolate) {
  HistogramTimerScope lazy_time_scope(
      isolate->counters()->wasm_lazy_compilation_time());

  // Find the wasm frame which triggered the lazy compile, to get the wasm
  // instance.
  StackFrameIterator it(isolate);
  // First frame: C entry stub.
  DCHECK(!it.done());
  DCHECK_EQ(StackFrame::EXIT, it.frame()->type());
  it.Advance();
  // Second frame: WasmCompileLazy builtin.
  DCHECK(!it.done());
  Handle<Code> lazy_compile_code(it.frame()->LookupCode(), isolate);
  DCHECK_EQ(Builtins::kWasmCompileLazy, lazy_compile_code->builtin_index());
  Handle<WasmInstanceObject> instance;
  Handle<FixedArray> exp_deopt_data;
  int func_index = -1;
  if (lazy_compile_code->deoptimization_data()->length() > 0) {
    // Then it's an indirect call or via JS->wasm wrapper.
    DCHECK_LE(2, lazy_compile_code->deoptimization_data()->length());
    exp_deopt_data = handle(lazy_compile_code->deoptimization_data(), isolate);
    auto* weak_cell = WeakCell::cast(exp_deopt_data->get(0));
    instance = handle(WasmInstanceObject::cast(weak_cell->value()), isolate);
    func_index = Smi::ToInt(exp_deopt_data->get(1));
  }
  it.Advance();
  // Third frame: The calling wasm code or js-to-wasm wrapper.
  DCHECK(!it.done());
  DCHECK(it.frame()->is_js_to_wasm() || it.frame()->is_wasm_compiled());
  Handle<Code> caller_code = handle(it.frame()->LookupCode(), isolate);
  if (it.frame()->is_js_to_wasm()) {
    DCHECK(!instance.is_null());
  } else if (instance.is_null()) {
    // Then this is a direct call (otherwise we would have attached the instance
    // via deopt data to the lazy compile stub). Just use the instance of the
    // caller.
    instance = handle(wasm::GetOwningWasmInstance(*caller_code), isolate);
  }
  int offset =
      static_cast<int>(it.frame()->pc() - caller_code->instruction_start());
  // Only patch the caller code if this is *no* indirect call.
  // exp_deopt_data will be null if the called function is not exported at all,
  // and its length will be <= 2 if all entries in tables were already patched.
  // Note that this check is conservative: If the first call to an exported
  // function is direct, we will just patch the export tables, and only on the
  // second call we will patch the caller.
  bool patch_caller = caller_code->kind() == Code::JS_TO_WASM_FUNCTION ||
                      exp_deopt_data.is_null() || exp_deopt_data->length() <= 2;

  Handle<Code> compiled_code = WasmCompiledModule::CompileLazy(
      isolate, instance, caller_code, offset, func_index, patch_caller);
  if (!exp_deopt_data.is_null() && exp_deopt_data->length() > 2) {
    // See EnsureExportedLazyDeoptData: exp_deopt_data[2...(len-1)] are pairs of
    // <export_table, index> followed by undefined values.
    // Use this information here to patch all export tables.
    DCHECK_EQ(0, exp_deopt_data->length() % 2);
    for (int idx = 2, end = exp_deopt_data->length(); idx < end; idx += 2) {
      if (exp_deopt_data->get(idx)->IsUndefined(isolate)) break;
      FixedArray* exp_table = FixedArray::cast(exp_deopt_data->get(idx));
      int exp_index = Smi::ToInt(exp_deopt_data->get(idx + 1));
      DCHECK(exp_table->get(exp_index) == *lazy_compile_code);
      exp_table->set(exp_index, *compiled_code);
    }
    // After processing, remove the list of exported entries, such that we don't
    // do the patching redundantly.
    Handle<FixedArray> new_deopt_data =
        isolate->factory()->CopyFixedArrayUpTo(exp_deopt_data, 2, TENURED);
    lazy_compile_code->set_deoptimization_data(*new_deopt_data);
  }

  return compiled_code;
}

void LazyCompilationOrchestrator::CompileFunction(
    Isolate* isolate, Handle<WasmInstanceObject> instance, int func_index) {
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);
  if (Code::cast(compiled_module->code_table()->get(func_index))->kind() ==
      Code::WASM_FUNCTION) {
    return;
  }

  compiler::ModuleEnv module_env =
      CreateModuleEnvFromCompiledModule(isolate, compiled_module);

  const uint8_t* module_start = compiled_module->module_bytes()->GetChars();

  const WasmFunction* func = &module_env.module->functions[func_index];
  wasm::FunctionBody body{func->sig, func->code.offset(),
                          module_start + func->code.offset(),
                          module_start + func->code.end_offset()};
  // TODO(wasm): Refactor this to only get the name if it is really needed for
  // tracing / debugging.
  std::string func_name;
  {
    wasm::WasmName name = Vector<const char>::cast(
        compiled_module->GetRawFunctionName(func_index));
    // Copy to std::string, because the underlying string object might move on
    // the heap.
    func_name.assign(name.start(), static_cast<size_t>(name.length()));
  }
  ErrorThrower thrower(isolate, "WasmLazyCompile");
  compiler::WasmCompilationUnit unit(isolate, &module_env, body,
                                     CStrVector(func_name.c_str()), func_index,
                                     CEntryStub(isolate, 1).GetCode());
  unit.ExecuteCompilation();
  MaybeHandle<Code> maybe_code = unit.FinishCompilation(&thrower);

  // If there is a pending error, something really went wrong. The module was
  // verified before starting execution with lazy compilation.
  // This might be OOM, but then we cannot continue execution anyway.
  // TODO(clemensh): According to the spec, we can actually skip validation at
  // module creation time, and return a function that always traps here.
  CHECK(!thrower.error());
  Handle<Code> code = maybe_code.ToHandleChecked();

  Handle<FixedArray> deopt_data = isolate->factory()->NewFixedArray(2, TENURED);
  Handle<WeakCell> weak_instance = isolate->factory()->NewWeakCell(instance);
  // TODO(wasm): Introduce constants for the indexes in wasm deopt data.
  deopt_data->set(0, *weak_instance);
  deopt_data->set(1, Smi::FromInt(func_index));
  code->set_deoptimization_data(*deopt_data);

  DCHECK_EQ(Builtins::kWasmCompileLazy,
            Code::cast(compiled_module->code_table()->get(func_index))
                ->builtin_index());
  compiled_module->code_table()->set(func_index, *code);

  // Now specialize the generated code for this instance.
  Zone specialization_zone(isolate->allocator(), ZONE_NAME);
  CodeSpecialization code_specialization(isolate, &specialization_zone);
  code_specialization.RelocateDirectCalls(instance);
  code_specialization.ApplyToWasmCode(*code, SKIP_ICACHE_FLUSH);
  Assembler::FlushICache(isolate, code->instruction_start(),
                         code->instruction_size());
  RecordLazyCodeStats(*code, isolate->counters());
}

Handle<Code> LazyCompilationOrchestrator::CompileLazy(
    Isolate* isolate, Handle<WasmInstanceObject> instance, Handle<Code> caller,
    int call_offset, int exported_func_index, bool patch_caller) {
  struct NonCompiledFunction {
    int offset;
    int func_index;
  };
  std::vector<NonCompiledFunction> non_compiled_functions;
  int func_to_return_idx = exported_func_index;
  wasm::Decoder decoder(nullptr, nullptr);
  bool is_js_to_wasm = caller->kind() == Code::JS_TO_WASM_FUNCTION;
  Handle<WasmCompiledModule> compiled_module(instance->compiled_module(),
                                             isolate);

  if (is_js_to_wasm) {
    non_compiled_functions.push_back({0, exported_func_index});
  } else if (patch_caller) {
    DisallowHeapAllocation no_gc;
    SeqOneByteString* module_bytes = compiled_module->module_bytes();
    SourcePositionTableIterator source_pos_iterator(
        caller->SourcePositionTable());
    DCHECK_EQ(2, caller->deoptimization_data()->length());
    int caller_func_index = Smi::ToInt(caller->deoptimization_data()->get(1));
    const byte* func_bytes =
        module_bytes->GetChars() +
        compiled_module->module()->functions[caller_func_index].code.offset();
    for (RelocIterator it(*caller, RelocInfo::kCodeTargetMask); !it.done();
         it.next()) {
      Code* callee =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (callee->builtin_index() != Builtins::kWasmCompileLazy) continue;
      // TODO(clemensh): Introduce safe_cast<T, bool> which (D)CHECKS
      // (depending on the bool) against limits of T and then static_casts.
      size_t offset_l = it.rinfo()->pc() - caller->instruction_start();
      DCHECK_GE(kMaxInt, offset_l);
      int offset = static_cast<int>(offset_l);
      int byte_pos =
          AdvanceSourcePositionTableIterator(source_pos_iterator, offset);
      int called_func_index =
          ExtractDirectCallIndex(decoder, func_bytes + byte_pos);
      non_compiled_functions.push_back({offset, called_func_index});
      // Call offset one instruction after the call. Remember the last called
      // function before that offset.
      if (offset < call_offset) func_to_return_idx = called_func_index;
    }
  }

  // TODO(clemensh): compile all functions in non_compiled_functions in
  // background, wait for func_to_return_idx.
  CompileFunction(isolate, instance, func_to_return_idx);

  if (is_js_to_wasm || patch_caller) {
    DisallowHeapAllocation no_gc;
    // Now patch the code object with all functions which are now compiled.
    int idx = 0;
    for (RelocIterator it(*caller, RelocInfo::kCodeTargetMask); !it.done();
         it.next()) {
      Code* callee =
          Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
      if (callee->builtin_index() != Builtins::kWasmCompileLazy) continue;
      DCHECK_GT(non_compiled_functions.size(), idx);
      int called_func_index = non_compiled_functions[idx].func_index;
      // Check that the callee agrees with our assumed called_func_index.
      DCHECK_IMPLIES(callee->deoptimization_data()->length() > 0,
                     Smi::ToInt(callee->deoptimization_data()->get(1)) ==
                         called_func_index);
      if (is_js_to_wasm) {
        DCHECK_EQ(func_to_return_idx, called_func_index);
      } else {
        DCHECK_EQ(non_compiled_functions[idx].offset,
                  it.rinfo()->pc() - caller->instruction_start());
      }
      ++idx;
      Handle<Code> callee_compiled(
          Code::cast(compiled_module->code_table()->get(called_func_index)));
      if (callee_compiled->builtin_index() == Builtins::kWasmCompileLazy) {
        DCHECK_NE(func_to_return_idx, called_func_index);
        continue;
      }
      DCHECK_EQ(Code::WASM_FUNCTION, callee_compiled->kind());
      it.rinfo()->set_target_address(isolate,
                                     callee_compiled->instruction_start());
    }
    DCHECK_EQ(non_compiled_functions.size(), idx);
  }

  Code* ret =
      Code::cast(compiled_module->code_table()->get(func_to_return_idx));
  DCHECK_EQ(Code::WASM_FUNCTION, ret->kind());
  return handle(ret, isolate);
}

const char* wasm::ExternalKindName(WasmExternalKind kind) {
  switch (kind) {
    case kExternalFunction:
      return "function";
    case kExternalTable:
      return "table";
    case kExternalMemory:
      return "memory";
    case kExternalGlobal:
      return "global";
  }
  return "unknown";
}
