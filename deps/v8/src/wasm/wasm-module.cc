// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/macro-assembler.h"
#include "src/objects.h"
#include "src/v8.h"

#include "src/simulator.h"

#include "src/wasm/ast-decoder.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"

#include "src/compiler/wasm-compiler.h"

namespace v8 {
namespace internal {
namespace wasm {

std::ostream& operator<<(std::ostream& os, const WasmModule& module) {
  os << "WASM module with ";
  os << (1 << module.min_mem_size_log2) << " min mem";
  os << (1 << module.max_mem_size_log2) << " max mem";
  if (module.functions) os << module.functions->size() << " functions";
  if (module.globals) os << module.functions->size() << " globals";
  if (module.data_segments) os << module.functions->size() << " data segments";
  return os;
}


std::ostream& operator<<(std::ostream& os, const WasmFunction& function) {
  os << "WASM function with signature " << *function.sig;

  os << " locals: ";
  if (function.local_i32_count) os << function.local_i32_count << " i32s ";
  if (function.local_i64_count) os << function.local_i64_count << " i64s ";
  if (function.local_f32_count) os << function.local_f32_count << " f32s ";
  if (function.local_f64_count) os << function.local_f64_count << " f64s ";

  os << " code bytes: "
     << (function.code_end_offset - function.code_start_offset);
  return os;
}

std::ostream& operator<<(std::ostream& os, const WasmFunctionName& pair) {
  os << "#" << pair.function_->func_index << ":";
  if (pair.function_->name_offset > 0) {
    if (pair.module_) {
      os << pair.module_->GetName(pair.function_->name_offset);
    } else {
      os << "+" << pair.function_->func_index;
    }
  } else {
    os << "?";
  }
  return os;
}

// A helper class for compiling multiple wasm functions that offers
// placeholder code objects for calling functions that are not yet compiled.
class WasmLinker {
 public:
  WasmLinker(Isolate* isolate, size_t size)
      : isolate_(isolate), placeholder_code_(size), function_code_(size) {}

  // Get the code object for a function, allocating a placeholder if it has
  // not yet been compiled.
  Handle<Code> GetFunctionCode(uint32_t index) {
    DCHECK(index < function_code_.size());
    if (function_code_[index].is_null()) {
      // Create a placeholder code object and encode the corresponding index in
      // the {constant_pool_offset} field of the code object.
      // TODO(titzer): placeholder code objects are somewhat dangerous.
      Handle<Code> self(nullptr, isolate_);
      byte buffer[] = {0, 0, 0, 0, 0, 0, 0, 0};  // fake instructions.
      CodeDesc desc = {buffer, 8, 8, 0, 0, nullptr};
      Handle<Code> code = isolate_->factory()->NewCode(
          desc, Code::KindField::encode(Code::WASM_FUNCTION), self);
      code->set_constant_pool_offset(index + kPlaceholderMarker);
      placeholder_code_[index] = code;
      function_code_[index] = code;
    }
    return function_code_[index];
  }

  void Finish(uint32_t index, Handle<Code> code) {
    DCHECK(index < function_code_.size());
    function_code_[index] = code;
  }

  void Link(Handle<FixedArray> function_table,
            std::vector<uint16_t>* functions) {
    for (size_t i = 0; i < function_code_.size(); i++) {
      LinkFunction(function_code_[i]);
    }
    if (functions && !function_table.is_null()) {
      int table_size = static_cast<int>(functions->size());
      DCHECK_EQ(function_table->length(), table_size * 2);
      for (int i = 0; i < table_size; i++) {
        function_table->set(i + table_size, *function_code_[functions->at(i)]);
      }
    }
  }

 private:
  static const int kPlaceholderMarker = 1000000000;

  Isolate* isolate_;
  std::vector<Handle<Code>> placeholder_code_;
  std::vector<Handle<Code>> function_code_;

  void LinkFunction(Handle<Code> code) {
    bool modified = false;
    int mode_mask = RelocInfo::kCodeTargetMask;
    AllowDeferredHandleDereference embedding_raw_address;
    for (RelocIterator it(*code, mode_mask); !it.done(); it.next()) {
      RelocInfo::Mode mode = it.rinfo()->rmode();
      if (RelocInfo::IsCodeTarget(mode)) {
        Code* target =
            Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
        if (target->kind() == Code::WASM_FUNCTION &&
            target->constant_pool_offset() >= kPlaceholderMarker) {
          // Patch direct calls to placeholder code objects.
          uint32_t index = target->constant_pool_offset() - kPlaceholderMarker;
          CHECK(index < function_code_.size());
          Handle<Code> new_target = function_code_[index];
          if (target != *new_target) {
            CHECK_EQ(*placeholder_code_[index], target);
            it.rinfo()->set_target_address(new_target->instruction_start(),
                                           SKIP_WRITE_BARRIER,
                                           SKIP_ICACHE_FLUSH);
            modified = true;
          }
        }
      }
    }
    if (modified) {
      Assembler::FlushICache(isolate_, code->instruction_start(),
                             code->instruction_size());
    }
  }
};

namespace {
// Internal constants for the layout of the module object.
const int kWasmModuleInternalFieldCount = 4;
const int kWasmModuleFunctionTable = 0;
const int kWasmModuleCodeTable = 1;
const int kWasmMemArrayBuffer = 2;
const int kWasmGlobalsArrayBuffer = 3;


size_t AllocateGlobalsOffsets(std::vector<WasmGlobal>* globals) {
  uint32_t offset = 0;
  if (!globals) return 0;
  for (WasmGlobal& global : *globals) {
    byte size = WasmOpcodes::MemSize(global.type);
    offset = (offset + size - 1) & ~(size - 1);  // align
    global.offset = offset;
    offset += size;
  }
  return offset;
}


void LoadDataSegments(WasmModule* module, byte* mem_addr, size_t mem_size) {
  for (const WasmDataSegment& segment : *module->data_segments) {
    if (!segment.init) continue;
    CHECK_LT(segment.dest_addr, mem_size);
    CHECK_LE(segment.source_size, mem_size);
    CHECK_LE(segment.dest_addr + segment.source_size, mem_size);
    byte* addr = mem_addr + segment.dest_addr;
    memcpy(addr, module->module_start + segment.source_offset,
           segment.source_size);
  }
}


Handle<FixedArray> BuildFunctionTable(Isolate* isolate, WasmModule* module) {
  if (!module->function_table || module->function_table->size() == 0) {
    return Handle<FixedArray>::null();
  }
  int table_size = static_cast<int>(module->function_table->size());
  Handle<FixedArray> fixed = isolate->factory()->NewFixedArray(2 * table_size);
  for (int i = 0; i < table_size; i++) {
    WasmFunction* function =
        &module->functions->at(module->function_table->at(i));
    fixed->set(i, Smi::FromInt(function->sig_index));
  }
  return fixed;
}

Handle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, size_t size,
                                     byte** backing_store) {
  if (size > (1 << WasmModule::kMaxMemSize)) {
    // TODO(titzer): lift restriction on maximum memory allocated here.
    *backing_store = nullptr;
    return Handle<JSArrayBuffer>::null();
  }
  void* memory =
      isolate->array_buffer_allocator()->Allocate(static_cast<int>(size));
  if (!memory) {
    *backing_store = nullptr;
    return Handle<JSArrayBuffer>::null();
  }

  *backing_store = reinterpret_cast<byte*>(memory);

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  byte* bytes = reinterpret_cast<byte*>(*backing_store);
  for (size_t i = 0; i < size; i++) {
    DCHECK_EQ(0, bytes[i]);
  }
#endif

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  JSArrayBuffer::Setup(buffer, isolate, false, memory, static_cast<int>(size));
  buffer->set_is_neuterable(false);
  return buffer;
}

// Set the memory for a module instance to be the {memory} array buffer.
void SetMemory(WasmModuleInstance* instance, Handle<JSArrayBuffer> memory) {
  memory->set_is_neuterable(false);
  instance->mem_start = reinterpret_cast<byte*>(memory->backing_store());
  instance->mem_size = memory->byte_length()->Number();
  instance->mem_buffer = memory;
}

// Allocate memory for a module instance as a new JSArrayBuffer.
bool AllocateMemory(ErrorThrower* thrower, Isolate* isolate,
                    WasmModuleInstance* instance) {
  DCHECK(instance->module);
  DCHECK(instance->mem_buffer.is_null());

  if (instance->module->min_mem_size_log2 > WasmModule::kMaxMemSize) {
    thrower->Error("Out of memory: wasm memory too large");
    return false;
  }
  instance->mem_size = static_cast<size_t>(1)
                       << instance->module->min_mem_size_log2;
  instance->mem_buffer =
      NewArrayBuffer(isolate, instance->mem_size, &instance->mem_start);
  if (!instance->mem_start) {
    thrower->Error("Out of memory: wasm memory");
    instance->mem_size = 0;
    return false;
  }
  return true;
}

bool AllocateGlobals(ErrorThrower* thrower, Isolate* isolate,
                     WasmModuleInstance* instance) {
  instance->globals_size = AllocateGlobalsOffsets(instance->module->globals);

  if (instance->globals_size > 0) {
    instance->globals_buffer = NewArrayBuffer(isolate, instance->globals_size,
                                              &instance->globals_start);
    if (!instance->globals_start) {
      // Not enough space for backing store of globals.
      thrower->Error("Out of memory: wasm globals");
      return false;
    }
  }
  return true;
}
}  // namespace

WasmModule::WasmModule()
    : shared_isolate(nullptr),
      module_start(nullptr),
      module_end(nullptr),
      min_mem_size_log2(0),
      max_mem_size_log2(0),
      mem_export(false),
      mem_external(false),
      start_function_index(-1),
      globals(nullptr),
      signatures(nullptr),
      functions(nullptr),
      data_segments(nullptr),
      function_table(nullptr),
      import_table(nullptr) {}

WasmModule::~WasmModule() {
  if (globals) delete globals;
  if (signatures) delete signatures;
  if (functions) delete functions;
  if (data_segments) delete data_segments;
  if (function_table) delete function_table;
  if (import_table) delete import_table;
}

static MaybeHandle<JSFunction> LookupFunction(ErrorThrower& thrower,
                                              Handle<JSObject> ffi,
                                              uint32_t index,
                                              Handle<String> name,
                                              const char* cstr) {
  if (!ffi.is_null()) {
    MaybeHandle<Object> result = Object::GetProperty(ffi, name);
    if (!result.is_null()) {
      Handle<Object> obj = result.ToHandleChecked();
      if (obj->IsJSFunction()) {
        return Handle<JSFunction>::cast(obj);
      } else {
        thrower.Error("FFI function #%d:%s is not a JSFunction.", index, cstr);
        return MaybeHandle<JSFunction>();
      }
    } else {
      thrower.Error("FFI function #%d:%s not found.", index, cstr);
      return MaybeHandle<JSFunction>();
    }
  } else {
    thrower.Error("FFI table is not an object.");
    return MaybeHandle<JSFunction>();
  }
}

// Instantiates a wasm module as a JSObject.
//  * allocates a backing store of {mem_size} bytes.
//  * installs a named property "memory" for that buffer if exported
//  * installs named properties on the object for exported functions
//  * compiles wasm code to machine code
MaybeHandle<JSObject> WasmModule::Instantiate(Isolate* isolate,
                                              Handle<JSObject> ffi,
                                              Handle<JSArrayBuffer> memory) {
  this->shared_isolate = isolate;  // TODO(titzer): have a real shared isolate.
  ErrorThrower thrower(isolate, "WasmModule::Instantiate()");
  Factory* factory = isolate->factory();

  //-------------------------------------------------------------------------
  // Allocate the instance and its JS counterpart.
  //-------------------------------------------------------------------------
  Handle<Map> map = factory->NewMap(
      JS_OBJECT_TYPE,
      JSObject::kHeaderSize + kWasmModuleInternalFieldCount * kPointerSize);
  WasmModuleInstance instance(this);
  std::vector<Handle<Code>> import_code;
  instance.context = isolate->native_context();
  instance.js_object = factory->NewJSObjectFromMap(map, TENURED);
  Handle<FixedArray> code_table =
      factory->NewFixedArray(static_cast<int>(functions->size()), TENURED);
  instance.js_object->SetInternalField(kWasmModuleCodeTable, *code_table);

  //-------------------------------------------------------------------------
  // Allocate and initialize the linear memory.
  //-------------------------------------------------------------------------
  if (memory.is_null()) {
    if (!AllocateMemory(&thrower, isolate, &instance)) {
      return MaybeHandle<JSObject>();
    }
  } else {
    SetMemory(&instance, memory);
  }
  instance.js_object->SetInternalField(kWasmMemArrayBuffer,
                                       *instance.mem_buffer);
  LoadDataSegments(this, instance.mem_start, instance.mem_size);

  if (mem_export) {
    // Export the memory as a named property.
    Handle<String> name = factory->InternalizeUtf8String("memory");
    JSObject::AddProperty(instance.js_object, name, instance.mem_buffer,
                          READ_ONLY);
  }

  //-------------------------------------------------------------------------
  // Allocate the globals area if necessary.
  //-------------------------------------------------------------------------
  if (!AllocateGlobals(&thrower, isolate, &instance)) {
    return MaybeHandle<JSObject>();
  }
  if (!instance.globals_buffer.is_null()) {
    instance.js_object->SetInternalField(kWasmGlobalsArrayBuffer,
                                         *instance.globals_buffer);
  }

  //-------------------------------------------------------------------------
  // Compile wrappers to imported functions.
  //-------------------------------------------------------------------------
  uint32_t index = 0;
  instance.function_table = BuildFunctionTable(isolate, this);
  WasmLinker linker(isolate, functions->size());
  ModuleEnv module_env;
  module_env.module = this;
  module_env.instance = &instance;
  module_env.linker = &linker;
  module_env.asm_js = false;

  if (import_table->size() > 0) {
    instance.import_code = &import_code;
    instance.import_code->reserve(import_table->size());
    for (const WasmImport& import : *import_table) {
      const char* cstr = GetName(import.function_name_offset);
      Handle<String> name = factory->InternalizeUtf8String(cstr);
      MaybeHandle<JSFunction> function =
          LookupFunction(thrower, ffi, index, name, cstr);
      if (function.is_null()) return MaybeHandle<JSObject>();
      Handle<Code> code = compiler::CompileWasmToJSWrapper(
          isolate, &module_env, function.ToHandleChecked(), import.sig, cstr);
      instance.import_code->push_back(code);
      index++;
    }
  }

  //-------------------------------------------------------------------------
  // Compile all functions in the module.
  //-------------------------------------------------------------------------

  // First pass: compile each function and initialize the code table.
  index = 0;
  for (const WasmFunction& func : *functions) {
    if (thrower.error()) break;
    DCHECK_EQ(index, func.func_index);

    const char* cstr = GetName(func.name_offset);
    Handle<String> name = factory->InternalizeUtf8String(cstr);
    Handle<Code> code = Handle<Code>::null();
    Handle<JSFunction> function = Handle<JSFunction>::null();
    if (func.external) {
      // Lookup external function in FFI object.
      MaybeHandle<JSFunction> function =
          LookupFunction(thrower, ffi, index, name, cstr);
      if (function.is_null()) return MaybeHandle<JSObject>();
      code = compiler::CompileWasmToJSWrapper(
          isolate, &module_env, function.ToHandleChecked(), func.sig, cstr);
    } else {
      // Compile the function.
      code = compiler::CompileWasmFunction(thrower, isolate, &module_env, func);
      if (code.is_null()) {
        thrower.Error("Compilation of #%d:%s failed.", index, cstr);
        return MaybeHandle<JSObject>();
      }
      if (func.exported) {
        function = compiler::CompileJSToWasmWrapper(
            isolate, &module_env, name, code, instance.js_object, index);
      }
    }
    if (!code.is_null()) {
      // Install the code into the linker table.
      linker.Finish(index, code);
      code_table->set(index, *code);
    }
    if (func.exported) {
      // Exported functions are installed as read-only properties on the module.
      JSObject::AddProperty(instance.js_object, name, function, READ_ONLY);
    }
    index++;
  }

  // Second pass: patch all direct call sites.
  linker.Link(instance.function_table, this->function_table);
  instance.js_object->SetInternalField(kWasmModuleFunctionTable,
                                       Smi::FromInt(0));

  // Run the start function if one was specified.
  if (this->start_function_index >= 0) {
    HandleScope scope(isolate);
    uint32_t index = static_cast<uint32_t>(this->start_function_index);
    Handle<String> name = isolate->factory()->NewStringFromStaticChars("start");
    Handle<Code> code = linker.GetFunctionCode(index);
    Handle<JSFunction> jsfunc = compiler::CompileJSToWasmWrapper(
        isolate, &module_env, name, code, instance.js_object, index);

    // Call the JS function.
    Handle<Object> undefined(isolate->heap()->undefined_value(), isolate);
    MaybeHandle<Object> retval =
        Execution::Call(isolate, jsfunc, undefined, 0, nullptr);

    if (retval.is_null()) {
      thrower.Error("WASM.instantiateModule(): start function failed");
    }
  }
  return instance.js_object;
}


Handle<Code> ModuleEnv::GetFunctionCode(uint32_t index) {
  DCHECK(IsValidFunction(index));
  if (linker) return linker->GetFunctionCode(index);
  if (instance && instance->function_code) {
    return instance->function_code->at(index);
  }
  return Handle<Code>::null();
}

Handle<Code> ModuleEnv::GetImportCode(uint32_t index) {
  DCHECK(IsValidImport(index));
  if (instance && instance->import_code) {
    return instance->import_code->at(index);
  }
  return Handle<Code>::null();
}

compiler::CallDescriptor* ModuleEnv::GetCallDescriptor(Zone* zone,
                                                       uint32_t index) {
  DCHECK(IsValidFunction(index));
  // Always make a direct call to whatever is in the table at that location.
  // A wrapper will be generated for FFI calls.
  WasmFunction* function = &module->functions->at(index);
  return GetWasmCallDescriptor(zone, function->sig);
}


int32_t CompileAndRunWasmModule(Isolate* isolate, const byte* module_start,
                                const byte* module_end, bool asm_js) {
  HandleScope scope(isolate);
  Zone zone;
  // Decode the module, but don't verify function bodies, since we'll
  // be compiling them anyway.
  ModuleResult result =
      DecodeWasmModule(isolate, &zone, module_start, module_end, false, false);
  if (result.failed()) {
    // Module verification failed. throw.
    std::ostringstream str;
    str << "WASM.compileRun() failed: " << result;
    isolate->Throw(
        *isolate->factory()->NewStringFromAsciiChecked(str.str().c_str()));
    return -1;
  }

  int32_t retval = CompileAndRunWasmModule(isolate, result.val);
  delete result.val;
  return retval;
}


int32_t CompileAndRunWasmModule(Isolate* isolate, WasmModule* module) {
  ErrorThrower thrower(isolate, "CompileAndRunWasmModule");
  WasmModuleInstance instance(module);

  // Allocate and initialize the linear memory.
  if (!AllocateMemory(&thrower, isolate, &instance)) {
    return -1;
  }
  LoadDataSegments(module, instance.mem_start, instance.mem_size);

  // Allocate the globals area if necessary.
  if (!AllocateGlobals(&thrower, isolate, &instance)) {
    return -1;
  }

  // Build the function table.
  instance.function_table = BuildFunctionTable(isolate, module);

  // Create module environment.
  WasmLinker linker(isolate, module->functions->size());
  ModuleEnv module_env;
  module_env.module = module;
  module_env.instance = &instance;
  module_env.linker = &linker;
  module_env.asm_js = false;

  // Compile all functions.
  Handle<Code> main_code = Handle<Code>::null();  // record last code.
  uint32_t index = 0;
  int main_index = 0;
  for (const WasmFunction& func : *module->functions) {
    DCHECK_EQ(index, func.func_index);
    if (!func.external) {
      // Compile the function and install it in the code table.
      Handle<Code> code =
          compiler::CompileWasmFunction(thrower, isolate, &module_env, func);
      if (!code.is_null()) {
        if (func.exported) {
          main_code = code;
          main_index = index;
        }
        linker.Finish(index, code);
      }
      if (thrower.error()) return -1;
    }
    index++;
  }

  if (main_code.is_null()) {
    thrower.Error("WASM.compileRun() failed: no main code found");
    return -1;
  }

  linker.Link(instance.function_table, instance.module->function_table);

  // Wrap the main code so it can be called as a JS function.
  Handle<String> name = isolate->factory()->NewStringFromStaticChars("main");
  Handle<JSObject> module_object = Handle<JSObject>(0, isolate);
  Handle<JSFunction> jsfunc = compiler::CompileJSToWasmWrapper(
      isolate, &module_env, name, main_code, module_object, main_index);

  // Call the JS function.
  Handle<Object> undefined(isolate->heap()->undefined_value(), isolate);
  MaybeHandle<Object> retval =
      Execution::Call(isolate, jsfunc, undefined, 0, nullptr);

  // The result should be a number.
  if (retval.is_null()) {
    thrower.Error("WASM.compileRun() failed: Invocation was null");
    return -1;
  }
  Handle<Object> result = retval.ToHandleChecked();
  if (result->IsSmi()) {
    return Smi::cast(*result)->value();
  }
  if (result->IsHeapNumber()) {
    return static_cast<int32_t>(HeapNumber::cast(*result)->value());
  }
  thrower.Error("WASM.compileRun() failed: Return value should be number");
  return -1;
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
