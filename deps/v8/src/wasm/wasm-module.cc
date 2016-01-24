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
  os << "WASM function with signature ";

  // TODO(titzer): factor out rendering of signatures.
  if (function.sig->return_count() == 0) os << "v";
  for (size_t i = 0; i < function.sig->return_count(); i++) {
    os << WasmOpcodes::ShortNameOf(function.sig->GetReturn(i));
  }
  os << "_";
  if (function.sig->parameter_count() == 0) os << "v";
  for (size_t i = 0; i < function.sig->parameter_count(); i++) {
    os << WasmOpcodes::ShortNameOf(function.sig->GetParam(i));
  }
  os << " locals: ";
  if (function.local_int32_count)
    os << function.local_int32_count << " int32s ";
  if (function.local_int64_count)
    os << function.local_int64_count << " int64s ";
  if (function.local_float32_count)
    os << function.local_float32_count << " float32s ";
  if (function.local_float64_count)
    os << function.local_float64_count << " float64s ";

  os << " code bytes: "
     << (function.code_end_offset - function.code_start_offset);
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


Handle<JSArrayBuffer> NewArrayBuffer(Isolate* isolate, int size,
                                     byte** backing_store) {
  void* memory = isolate->array_buffer_allocator()->Allocate(size);
  if (!memory) return Handle<JSArrayBuffer>::null();
  *backing_store = reinterpret_cast<byte*>(memory);

#if DEBUG
  // Double check the API allocator actually zero-initialized the memory.
  for (int i = 0; i < size; i++) {
    DCHECK_EQ(0, (*backing_store)[i]);
  }
#endif

  Handle<JSArrayBuffer> buffer = isolate->factory()->NewJSArrayBuffer();
  JSArrayBuffer::Setup(buffer, isolate, false, memory, size);
  buffer->set_is_neuterable(false);
  return buffer;
}
}  // namespace


WasmModule::WasmModule()
    : globals(nullptr),
      signatures(nullptr),
      functions(nullptr),
      data_segments(nullptr),
      function_table(nullptr) {}


WasmModule::~WasmModule() {
  if (globals) delete globals;
  if (signatures) delete signatures;
  if (functions) delete functions;
  if (data_segments) delete data_segments;
  if (function_table) delete function_table;
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
  // Memory is bigger than maximum supported size.
  if (memory.is_null() && min_mem_size_log2 > kMaxMemSize) {
    thrower.Error("Out of memory: wasm memory too large");
    return MaybeHandle<JSObject>();
  }

  Handle<Map> map = factory->NewMap(
      JS_OBJECT_TYPE,
      JSObject::kHeaderSize + kWasmModuleInternalFieldCount * kPointerSize);

  //-------------------------------------------------------------------------
  // Allocate the module object.
  //-------------------------------------------------------------------------
  Handle<JSObject> module = factory->NewJSObjectFromMap(map, TENURED);
  Handle<FixedArray> code_table =
      factory->NewFixedArray(static_cast<int>(functions->size()), TENURED);

  //-------------------------------------------------------------------------
  // Allocate the linear memory.
  //-------------------------------------------------------------------------
  uint32_t mem_size = 1 << min_mem_size_log2;
  byte* mem_addr = nullptr;
  Handle<JSArrayBuffer> mem_buffer;
  if (!memory.is_null()) {
    memory->set_is_neuterable(false);
    mem_addr = reinterpret_cast<byte*>(memory->backing_store());
    mem_size = memory->byte_length()->Number();
    mem_buffer = memory;
  } else {
    mem_buffer = NewArrayBuffer(isolate, mem_size, &mem_addr);
    if (!mem_addr) {
      // Not enough space for backing store of memory
      thrower.Error("Out of memory: wasm memory");
      return MaybeHandle<JSObject>();
    }
  }

  // Load initialized data segments.
  LoadDataSegments(this, mem_addr, mem_size);

  module->SetInternalField(kWasmMemArrayBuffer, *mem_buffer);

  if (mem_export) {
    // Export the memory as a named property.
    Handle<String> name = factory->InternalizeUtf8String("memory");
    JSObject::AddProperty(module, name, mem_buffer, READ_ONLY);
  }

  //-------------------------------------------------------------------------
  // Allocate the globals area if necessary.
  //-------------------------------------------------------------------------
  size_t globals_size = AllocateGlobalsOffsets(globals);
  byte* globals_addr = nullptr;
  if (globals_size > 0) {
    Handle<JSArrayBuffer> globals_buffer =
        NewArrayBuffer(isolate, mem_size, &globals_addr);
    if (!globals_addr) {
      // Not enough space for backing store of globals.
      thrower.Error("Out of memory: wasm globals");
      return MaybeHandle<JSObject>();
    }

    module->SetInternalField(kWasmGlobalsArrayBuffer, *globals_buffer);
  } else {
    module->SetInternalField(kWasmGlobalsArrayBuffer, Smi::FromInt(0));
  }

  //-------------------------------------------------------------------------
  // Compile all functions in the module.
  //-------------------------------------------------------------------------
  int index = 0;
  WasmLinker linker(isolate, functions->size());
  ModuleEnv module_env;
  module_env.module = this;
  module_env.mem_start = reinterpret_cast<uintptr_t>(mem_addr);
  module_env.mem_end = reinterpret_cast<uintptr_t>(mem_addr) + mem_size;
  module_env.globals_area = reinterpret_cast<uintptr_t>(globals_addr);
  module_env.linker = &linker;
  module_env.function_code = nullptr;
  module_env.function_table = BuildFunctionTable(isolate, this);
  module_env.memory = memory;
  module_env.context = isolate->native_context();
  module_env.asm_js = false;

  // First pass: compile each function and initialize the code table.
  for (const WasmFunction& func : *functions) {
    if (thrower.error()) break;

    const char* cstr = GetName(func.name_offset);
    Handle<String> name = factory->InternalizeUtf8String(cstr);
    Handle<Code> code = Handle<Code>::null();
    Handle<JSFunction> function = Handle<JSFunction>::null();
    if (func.external) {
      // Lookup external function in FFI object.
      if (!ffi.is_null()) {
        MaybeHandle<Object> result = Object::GetProperty(ffi, name);
        if (!result.is_null()) {
          Handle<Object> obj = result.ToHandleChecked();
          if (obj->IsJSFunction()) {
            function = Handle<JSFunction>::cast(obj);
            code = compiler::CompileWasmToJSWrapper(isolate, &module_env,
                                                    function, index);
          } else {
            thrower.Error("FFI function #%d:%s is not a JSFunction.", index,
                          cstr);
            return MaybeHandle<JSObject>();
          }
        } else {
          thrower.Error("FFI function #%d:%s not found.", index, cstr);
          return MaybeHandle<JSObject>();
        }
      } else {
        thrower.Error("FFI table is not an object.");
        return MaybeHandle<JSObject>();
      }
    } else {
      // Compile the function.
      code = compiler::CompileWasmFunction(thrower, isolate, &module_env, func,
                                           index);
      if (code.is_null()) {
        thrower.Error("Compilation of #%d:%s failed.", index, cstr);
        return MaybeHandle<JSObject>();
      }
      if (func.exported) {
        function = compiler::CompileJSToWasmWrapper(isolate, &module_env, name,
                                                    code, module, index);
      }
    }
    if (!code.is_null()) {
      // Install the code into the linker table.
      linker.Finish(index, code);
      code_table->set(index, *code);
    }
    if (func.exported) {
      // Exported functions are installed as read-only properties on the module.
      JSObject::AddProperty(module, name, function, READ_ONLY);
    }
    index++;
  }

  // Second pass: patch all direct call sites.
  linker.Link(module_env.function_table, this->function_table);

  module->SetInternalField(kWasmModuleFunctionTable, Smi::FromInt(0));
  module->SetInternalField(kWasmModuleCodeTable, *code_table);
  return module;
}


Handle<Code> ModuleEnv::GetFunctionCode(uint32_t index) {
  DCHECK(IsValidFunction(index));
  if (linker) return linker->GetFunctionCode(index);
  if (function_code) return function_code->at(index);
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

  // Allocate temporary linear memory and globals.
  size_t mem_size = 1 << module->min_mem_size_log2;
  size_t globals_size = AllocateGlobalsOffsets(module->globals);

  base::SmartArrayPointer<byte> mem_addr(new byte[mem_size]);
  base::SmartArrayPointer<byte> globals_addr(new byte[globals_size]);

  memset(mem_addr.get(), 0, mem_size);
  memset(globals_addr.get(), 0, globals_size);

  // Create module environment.
  WasmLinker linker(isolate, module->functions->size());
  ModuleEnv module_env;
  module_env.module = module;
  module_env.mem_start = reinterpret_cast<uintptr_t>(mem_addr.get());
  module_env.mem_end = reinterpret_cast<uintptr_t>(mem_addr.get()) + mem_size;
  module_env.globals_area = reinterpret_cast<uintptr_t>(globals_addr.get());
  module_env.linker = &linker;
  module_env.function_code = nullptr;
  module_env.function_table = BuildFunctionTable(isolate, module);
  module_env.asm_js = false;

  // Load data segments.
  // TODO(titzer): throw instead of crashing if segments don't fit in memory?
  LoadDataSegments(module, mem_addr.get(), mem_size);

  // Compile all functions.
  Handle<Code> main_code = Handle<Code>::null();  // record last code.
  int index = 0;
  for (const WasmFunction& func : *module->functions) {
    if (!func.external) {
      // Compile the function and install it in the code table.
      Handle<Code> code = compiler::CompileWasmFunction(
          thrower, isolate, &module_env, func, index);
      if (!code.is_null()) {
        if (func.exported) main_code = code;
        linker.Finish(index, code);
      }
      if (thrower.error()) return -1;
    }
    index++;
  }

  if (!main_code.is_null()) {
    linker.Link(module_env.function_table, module->function_table);
#if USE_SIMULATOR && V8_TARGET_ARCH_ARM64
    // Run the main code on arm64 simulator.
    Simulator* simulator = Simulator::current(isolate);
    Simulator::CallArgument args[] = {Simulator::CallArgument(0),
                                      Simulator::CallArgument::End()};
    return static_cast<int32_t>(simulator->CallInt64(main_code->entry(), args));
#elif USE_SIMULATOR
    // Run the main code on simulator.
    Simulator* simulator = Simulator::current(isolate);
    return static_cast<int32_t>(
        simulator->Call(main_code->entry(), 4, 0, 0, 0, 0));
#else
    // Run the main code as raw machine code.
    int32_t (*raw_func)() = reinterpret_cast<int32_t (*)()>(
        reinterpret_cast<uintptr_t>(main_code->entry()));
    return raw_func();
#endif
  } else {
    // No main code was found.
    isolate->Throw(*isolate->factory()->NewStringFromStaticChars(
        "WASM.compileRun() failed: no valid main code produced."));
  }
  return -1;
}
}  // namespace wasm
}  // namespace internal
}  // namespace v8
