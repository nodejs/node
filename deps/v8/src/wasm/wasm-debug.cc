// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/assembler-inl.h"
#include "src/assert-scope.h"
#include "src/compiler/wasm-compiler.h"
#include "src/debug/debug.h"
#include "src/factory.h"
#include "src/frames-inl.h"
#include "src/isolate.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-interpreter.h"
#include "src/wasm/wasm-limits.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-objects.h"
#include "src/zone/accounting-allocator.h"

using namespace v8::internal;
using namespace v8::internal::wasm;

namespace {

class InterpreterHandle {
  AccountingAllocator allocator_;
  WasmInstance instance_;
  WasmInterpreter interpreter_;

 public:
  // Initialize in the right order, using helper methods to make this possible.
  // WasmInterpreter has to be allocated in place, since it is not movable.
  InterpreterHandle(Isolate* isolate, WasmDebugInfo* debug_info)
      : instance_(debug_info->wasm_instance()->compiled_module()->module()),
        interpreter_(GetBytesEnv(&instance_, debug_info), &allocator_) {
    Handle<JSArrayBuffer> mem_buffer =
        handle(debug_info->wasm_instance()->memory_buffer(), isolate);
    if (mem_buffer->IsUndefined(isolate)) {
      DCHECK_EQ(0, instance_.module->min_mem_pages);
      instance_.mem_start = nullptr;
      instance_.mem_size = 0;
    } else {
      instance_.mem_start =
          reinterpret_cast<byte*>(mem_buffer->backing_store());
      CHECK(mem_buffer->byte_length()->ToUint32(&instance_.mem_size));
    }
  }

  static ModuleBytesEnv GetBytesEnv(WasmInstance* instance,
                                    WasmDebugInfo* debug_info) {
    // Return raw pointer into heap. The WasmInterpreter will make its own copy
    // of this data anyway, and there is no heap allocation in-between.
    SeqOneByteString* bytes_str =
        debug_info->wasm_instance()->compiled_module()->module_bytes();
    Vector<const byte> bytes(bytes_str->GetChars(), bytes_str->length());
    return ModuleBytesEnv(instance->module, instance, bytes);
  }

  WasmInterpreter* interpreter() { return &interpreter_; }
  const WasmModule* module() { return instance_.module; }

  void Execute(uint32_t func_index, uint8_t* arg_buffer) {
    DCHECK_GE(module()->functions.size(), func_index);
    FunctionSig* sig = module()->functions[func_index].sig;
    DCHECK_GE(kMaxInt, sig->parameter_count());
    int num_params = static_cast<int>(sig->parameter_count());
    ScopedVector<WasmVal> wasm_args(num_params);
    uint8_t* arg_buf_ptr = arg_buffer;
    for (int i = 0; i < num_params; ++i) {
      uint32_t param_size = 1 << ElementSizeLog2Of(sig->GetParam(i));
#define CASE_ARG_TYPE(type, ctype)                                  \
  case type:                                                        \
    DCHECK_EQ(param_size, sizeof(ctype));                           \
    wasm_args[i] = WasmVal(*reinterpret_cast<ctype*>(arg_buf_ptr)); \
    break;
      switch (sig->GetParam(i)) {
        CASE_ARG_TYPE(kWasmI32, uint32_t)
        CASE_ARG_TYPE(kWasmI64, uint64_t)
        CASE_ARG_TYPE(kWasmF32, float)
        CASE_ARG_TYPE(kWasmF64, double)
#undef CASE_ARG_TYPE
        default:
          UNREACHABLE();
      }
      arg_buf_ptr += param_size;
    }

    WasmInterpreter::Thread* thread = interpreter_.GetThread(0);
    // We do not support reentering an already running interpreter at the moment
    // (like INTERPRETER -> JS -> WASM -> INTERPRETER).
    DCHECK(thread->state() == WasmInterpreter::STOPPED ||
           thread->state() == WasmInterpreter::FINISHED);
    thread->Reset();
    thread->PushFrame(&module()->functions[func_index], wasm_args.start());
    WasmInterpreter::State state;
    do {
      state = thread->Run();
      switch (state) {
        case WasmInterpreter::State::PAUSED: {
          // We hit a breakpoint.
          // TODO(clemensh): Handle this.
        } break;
        case WasmInterpreter::State::FINISHED:
          // Perfect, just break the switch and exit the loop.
          break;
        case WasmInterpreter::State::TRAPPED:
          // TODO(clemensh): Generate appropriate JS exception.
          UNIMPLEMENTED();
          break;
        // STOPPED and RUNNING should never occur here.
        case WasmInterpreter::State::STOPPED:
        case WasmInterpreter::State::RUNNING:
        default:
          UNREACHABLE();
      }
    } while (state != WasmInterpreter::State::FINISHED);

    // Copy back the return value
    DCHECK_GE(kV8MaxWasmFunctionReturns, sig->return_count());
    // TODO(wasm): Handle multi-value returns.
    DCHECK_EQ(1, kV8MaxWasmFunctionReturns);
    if (sig->return_count()) {
      WasmVal ret_val = thread->GetReturnValue(0);
#define CASE_RET_TYPE(type, ctype)                                       \
  case type:                                                             \
    DCHECK_EQ(1 << ElementSizeLog2Of(sig->GetReturn(0)), sizeof(ctype)); \
    *reinterpret_cast<ctype*>(arg_buffer) = ret_val.to<ctype>();         \
    break;
      switch (sig->GetReturn(0)) {
        CASE_RET_TYPE(kWasmI32, uint32_t)
        CASE_RET_TYPE(kWasmI64, uint64_t)
        CASE_RET_TYPE(kWasmF32, float)
        CASE_RET_TYPE(kWasmF64, double)
#undef CASE_RET_TYPE
        default:
          UNREACHABLE();
      }
    }
  }
};

InterpreterHandle* GetOrCreateInterpreterHandle(
    Isolate* isolate, Handle<WasmDebugInfo> debug_info) {
  Handle<Object> handle(debug_info->get(WasmDebugInfo::kInterpreterHandle),
                        isolate);
  if (handle->IsUndefined(isolate)) {
    InterpreterHandle* cpp_handle = new InterpreterHandle(isolate, *debug_info);
    handle = Managed<InterpreterHandle>::New(isolate, cpp_handle);
    debug_info->set(WasmDebugInfo::kInterpreterHandle, *handle);
  }

  return Handle<Managed<InterpreterHandle>>::cast(handle)->get();
}

int GetNumFunctions(WasmInstanceObject* instance) {
  size_t num_functions =
      instance->compiled_module()->module()->functions.size();
  DCHECK_GE(kMaxInt, num_functions);
  return static_cast<int>(num_functions);
}

Handle<FixedArray> GetOrCreateInterpretedFunctions(
    Isolate* isolate, Handle<WasmDebugInfo> debug_info) {
  Handle<Object> obj(debug_info->get(WasmDebugInfo::kInterpretedFunctions),
                     isolate);
  if (!obj->IsUndefined(isolate)) return Handle<FixedArray>::cast(obj);

  Handle<FixedArray> new_arr = isolate->factory()->NewFixedArray(
      GetNumFunctions(debug_info->wasm_instance()));
  debug_info->set(WasmDebugInfo::kInterpretedFunctions, *new_arr);
  return new_arr;
}

void RedirectCallsitesInCode(Code* code, Code* old_target, Code* new_target) {
  DisallowHeapAllocation no_gc;
  for (RelocIterator it(code, RelocInfo::kCodeTargetMask); !it.done();
       it.next()) {
    DCHECK(RelocInfo::IsCodeTarget(it.rinfo()->rmode()));
    Code* target = Code::GetCodeFromTargetAddress(it.rinfo()->target_address());
    if (target != old_target) continue;
    it.rinfo()->set_target_address(new_target->instruction_start());
  }
}

void RedirectCallsitesInInstance(Isolate* isolate, WasmInstanceObject* instance,
                                 Code* old_target, Code* new_target) {
  DisallowHeapAllocation no_gc;
  // Redirect all calls in wasm functions.
  FixedArray* code_table = instance->compiled_module()->ptr_to_code_table();
  for (int i = 0, e = GetNumFunctions(instance); i < e; ++i) {
    RedirectCallsitesInCode(Code::cast(code_table->get(i)), old_target,
                            new_target);
  }

  // Redirect all calls in exported functions.
  FixedArray* weak_exported_functions =
      instance->compiled_module()->ptr_to_weak_exported_functions();
  for (int i = 0, e = weak_exported_functions->length(); i != e; ++i) {
    WeakCell* weak_function = WeakCell::cast(weak_exported_functions->get(i));
    if (weak_function->cleared()) continue;
    Code* code = JSFunction::cast(weak_function->value())->code();
    RedirectCallsitesInCode(code, old_target, new_target);
  }
}

void EnsureRedirectToInterpreter(Isolate* isolate,
                                 Handle<WasmDebugInfo> debug_info,
                                 int func_index) {
  Handle<FixedArray> interpreted_functions =
      GetOrCreateInterpretedFunctions(isolate, debug_info);
  if (!interpreted_functions->get(func_index)->IsUndefined(isolate)) return;

  Handle<WasmInstanceObject> instance(debug_info->wasm_instance(), isolate);
  Handle<Code> new_code = compiler::CompileWasmInterpreterEntry(
      isolate, func_index,
      instance->compiled_module()->module()->functions[func_index].sig,
      instance);

  Handle<FixedArray> code_table = instance->compiled_module()->code_table();
  Handle<Code> old_code(Code::cast(code_table->get(func_index)), isolate);
  interpreted_functions->set(func_index, *new_code);

  RedirectCallsitesInInstance(isolate, *instance, *old_code, *new_code);
}

}  // namespace

Handle<WasmDebugInfo> WasmDebugInfo::New(Handle<WasmInstanceObject> instance) {
  Isolate* isolate = instance->GetIsolate();
  Factory* factory = isolate->factory();
  Handle<FixedArray> arr = factory->NewFixedArray(kFieldCount, TENURED);
  arr->set(kInstance, *instance);
  return Handle<WasmDebugInfo>::cast(arr);
}

bool WasmDebugInfo::IsDebugInfo(Object* object) {
  if (!object->IsFixedArray()) return false;
  FixedArray* arr = FixedArray::cast(object);
  if (arr->length() != kFieldCount) return false;
  if (!IsWasmInstance(arr->get(kInstance))) return false;
  Isolate* isolate = arr->GetIsolate();
  if (!arr->get(kInterpreterHandle)->IsUndefined(isolate) &&
      !arr->get(kInterpreterHandle)->IsForeign())
    return false;
  return true;
}

WasmDebugInfo* WasmDebugInfo::cast(Object* object) {
  DCHECK(IsDebugInfo(object));
  return reinterpret_cast<WasmDebugInfo*>(object);
}

WasmInstanceObject* WasmDebugInfo::wasm_instance() {
  return WasmInstanceObject::cast(get(kInstance));
}

void WasmDebugInfo::SetBreakpoint(Handle<WasmDebugInfo> debug_info,
                                  int func_index, int offset) {
  Isolate* isolate = debug_info->GetIsolate();
  InterpreterHandle* handle = GetOrCreateInterpreterHandle(isolate, debug_info);
  WasmInterpreter* interpreter = handle->interpreter();
  DCHECK_LE(0, func_index);
  DCHECK_GT(handle->module()->functions.size(), func_index);
  const WasmFunction* func = &handle->module()->functions[func_index];
  interpreter->SetBreakpoint(func, offset, true);
  EnsureRedirectToInterpreter(isolate, debug_info, func_index);
}

void WasmDebugInfo::RunInterpreter(Handle<WasmDebugInfo> debug_info,
                                   int func_index, uint8_t* arg_buffer) {
  DCHECK_LE(0, func_index);
  InterpreterHandle* interp_handle =
      GetOrCreateInterpreterHandle(debug_info->GetIsolate(), debug_info);
  interp_handle->Execute(static_cast<uint32_t>(func_index), arg_buffer);
}
