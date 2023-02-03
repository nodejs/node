// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/testing.h"

#include "src/api/api-inl.h"
#include "src/api/api-natives.h"
#include "src/common/globals.h"
#include "src/execution/isolate-inl.h"
#include "src/heap/factory.h"
#include "src/objects/backing-store.h"
#include "src/objects/js-objects.h"
#include "src/objects/templates.h"
#include "src/sandbox/sandbox.h"

#ifdef V8_OS_LINUX
#include <signal.h>
#include <unistd.h>
#endif  // V8_OS_LINUX

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX

#ifdef V8_EXPOSE_MEMORY_CORRUPTION_API

namespace {

// Sandbox.byteLength
void SandboxGetByteLength(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  double sandbox_size = GetProcessWideSandbox()->size();
  args.GetReturnValue().Set(v8::Number::New(isolate, sandbox_size));
}

// new Sandbox.MemoryView(args) -> Sandbox.MemoryView
void SandboxMemoryView(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();
  Local<v8::Context> context = isolate->GetCurrentContext();

  if (!args.IsConstructCall()) {
    isolate->ThrowError("Sandbox.MemoryView must be invoked with 'new'");
    return;
  }

  Local<v8::Integer> arg1, arg2;
  if (!args[0]->ToInteger(context).ToLocal(&arg1) ||
      !args[1]->ToInteger(context).ToLocal(&arg2)) {
    isolate->ThrowError("Expects two number arguments (start offset and size)");
    return;
  }

  Sandbox* sandbox = GetProcessWideSandbox();
  CHECK_LE(sandbox->size(), kMaxSafeIntegerUint64);

  uint64_t offset = arg1->Value();
  uint64_t size = arg2->Value();
  if (offset > sandbox->size() || size > sandbox->size() ||
      (offset + size) > sandbox->size()) {
    isolate->ThrowError(
        "The MemoryView must be entirely contained within the sandbox");
    return;
  }

  Factory* factory = reinterpret_cast<Isolate*>(isolate)->factory();
  std::unique_ptr<BackingStore> memory = BackingStore::WrapAllocation(
      reinterpret_cast<void*>(sandbox->base() + offset), size,
      v8::BackingStore::EmptyDeleter, nullptr, SharedFlag::kNotShared);
  if (!memory) {
    isolate->ThrowError("Out of memory: MemoryView backing store");
    return;
  }
  Handle<JSArrayBuffer> buffer = factory->NewJSArrayBuffer(std::move(memory));
  args.GetReturnValue().Set(Utils::ToLocal(buffer));
}

// Sandbox.getAddressOf(object) -> Number
void SandboxGetAddressOf(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  if (args.Length() == 0) {
    isolate->ThrowError("First argument must be provided");
    return;
  }

  Handle<Object> arg = Utils::OpenHandle(*args[0]);
  if (!arg->IsHeapObject()) {
    isolate->ThrowError("First argument must be a HeapObject");
    return;
  }

  // HeapObjects must be allocated inside the pointer compression cage so their
  // address relative to the start of the sandbox can be obtained simply by
  // taking the lowest 32 bits of the absolute address.
  uint32_t address = static_cast<uint32_t>(HeapObject::cast(*arg).address());
  args.GetReturnValue().Set(v8::Integer::NewFromUnsigned(isolate, address));
}

// Sandbox.getSizeOf(object) -> Number
void SandboxGetSizeOf(const v8::FunctionCallbackInfo<v8::Value>& args) {
  v8::Isolate* isolate = args.GetIsolate();

  if (args.Length() == 0) {
    isolate->ThrowError("First argument must be provided");
    return;
  }

  Handle<Object> arg = Utils::OpenHandle(*args[0]);
  if (!arg->IsHeapObject()) {
    isolate->ThrowError("First argument must be a HeapObject");
    return;
  }

  int size = HeapObject::cast(*arg).Size();
  args.GetReturnValue().Set(v8::Integer::New(isolate, size));
}

Handle<FunctionTemplateInfo> NewFunctionTemplate(
    Isolate* isolate, FunctionCallback func,
    ConstructorBehavior constructor_behavior) {
  // Use the API functions here as they are more convenient to use.
  v8::Isolate* api_isolate = reinterpret_cast<v8::Isolate*>(isolate);
  Local<FunctionTemplate> function_template =
      FunctionTemplate::New(api_isolate, func, {}, {}, 0, constructor_behavior,
                            SideEffectType::kHasSideEffect);
  return v8::Utils::OpenHandle(*function_template);
}

Handle<JSFunction> CreateFunc(Isolate* isolate, FunctionCallback func,
                              Handle<String> name, bool is_constructor) {
  ConstructorBehavior constructor_behavior = is_constructor
                                                 ? ConstructorBehavior::kAllow
                                                 : ConstructorBehavior::kThrow;
  Handle<FunctionTemplateInfo> function_template =
      NewFunctionTemplate(isolate, func, constructor_behavior);
  return ApiNatives::InstantiateFunction(function_template, name)
      .ToHandleChecked();
}

void InstallFunc(Isolate* isolate, Handle<JSObject> holder,
                 FunctionCallback func, const char* name, int num_parameters,
                 bool is_constructor) {
  Factory* factory = isolate->factory();
  Handle<String> function_name = factory->NewStringFromAsciiChecked(name);
  Handle<JSFunction> function =
      CreateFunc(isolate, func, function_name, is_constructor);
  function->shared().set_length(num_parameters);
  JSObject::AddProperty(isolate, holder, function_name, function, NONE);
}

void InstallGetter(Isolate* isolate, Handle<JSObject> object,
                   FunctionCallback func, const char* name) {
  Factory* factory = isolate->factory();
  Handle<String> property_name = factory->NewStringFromAsciiChecked(name);
  Handle<JSFunction> getter = CreateFunc(isolate, func, property_name, false);
  Handle<Object> setter = factory->null_value();
  JSObject::DefineAccessor(object, property_name, getter, setter, FROZEN);
}

void InstallFunction(Isolate* isolate, Handle<JSObject> holder,
                     FunctionCallback func, const char* name,
                     int num_parameters) {
  InstallFunc(isolate, holder, func, name, num_parameters, false);
}

void InstallConstructor(Isolate* isolate, Handle<JSObject> holder,
                        FunctionCallback func, const char* name,
                        int num_parameters) {
  InstallFunc(isolate, holder, func, name, num_parameters, true);
}

}  // namespace

void SandboxTesting::InstallMemoryCorruptionApi(Isolate* isolate) {
  CHECK(GetProcessWideSandbox()->is_initialized());

#ifndef V8_EXPOSE_MEMORY_CORRUPTION_API
#error "This function should not be available in any shipping build "          \
       "where it could potentially be abused to facilitate exploitation."
#endif

  Factory* factory = isolate->factory();

  // Create the special Sandbox object that provides read/write access to the
  // sandbox address space alongside other miscellaneous functionality.
  Handle<JSObject> sandbox =
      factory->NewJSObject(isolate->object_function(), AllocationType::kOld);

  InstallGetter(isolate, sandbox, SandboxGetByteLength, "byteLength");
  InstallConstructor(isolate, sandbox, SandboxMemoryView, "MemoryView", 2);
  InstallFunction(isolate, sandbox, SandboxGetAddressOf, "getAddressOf", 1);
  InstallFunction(isolate, sandbox, SandboxGetSizeOf, "getSizeOf", 1);

  // Install the Sandbox object as property on the global object.
  Handle<JSGlobalObject> global = isolate->global_object();
  Handle<String> name = factory->NewStringFromAsciiChecked("Sandbox");
  JSObject::AddProperty(isolate, global, name, sandbox, DONT_ENUM);
}

#endif  // V8_EXPOSE_MEMORY_CORRUPTION_API

namespace {

#ifdef V8_OS_LINUX
void PrintToStderr(const char* output) {
  // NOTE: This code MUST be async-signal safe.
  // NO malloc or stdio is allowed here.
  ssize_t return_val = write(STDERR_FILENO, output, strlen(output));
  USE(return_val);
}

// Signal handler checking whether a memory access violation happened inside or
// outside of the sandbox address space. If inside, the signal is ignored and
// the process terminated normally, in the latter case the original signal
// handler is restored and the signal delivered again.
struct sigaction g_old_sigbus_handler, g_old_sigsegv_handler;
void SandboxSignalHandler(int signal, siginfo_t* info, void* void_context) {
  // NOTE: This code MUST be async-signal safe.
  // NO malloc or stdio is allowed here.

  if (signal == SIGABRT) {
    // SIGABRT typically indicates a failed CHECK which is harmless.
    PrintToStderr("Caught harmless signal (SIGABRT). Exiting process...\n");
    _exit(0);
  }

  Address faultaddr = reinterpret_cast<Address>(info->si_addr);

  if (GetProcessWideSandbox()->Contains(faultaddr)) {
    // Access violation happened inside the sandbox.
    PrintToStderr(
        "Caught harmless memory access violaton (inside sandbox address "
        "space). Exiting process...\n");
    _exit(0);
  }

  if (info->si_code == SI_KERNEL && faultaddr == 0) {
    // This combination appears to indicate a crash at a non-canonical address
    // on Linux. Crashes at non-canonical addresses are for example caused by
    // failed external pointer type checks. Memory accesses that _always_ land
    // at a non-canonical address are not exploitable and so these are filtered
    // out here. However, testcases need to be written with this in mind and
    // must cause crashes at valid addresses.
    PrintToStderr(
        "Caught harmless memory access violaton (non-canonical address). "
        "Exiting process...\n");
    _exit(0);
  }

  if (faultaddr < 0x1000) {
    printf("Faultaddr: 0x%lx\n", faultaddr);
    // Nullptr dereferences are harmless as nothing can be mapped there. We use
    // the typical page size (which is also the default value of mmap_min_addr
    // on Linux) to determine what counts as a nullptr dereference here.
    PrintToStderr(
        "Caught harmless memory access violaton (nullptr dereference). Exiting "
        "process...\n");
    _exit(0);
  }

  if (info->si_code == SEGV_ACCERR) {
    // This indicates an access to a (valid) mapping but with insufficient
    // permissions (e.g. accessing a region mapped with PROT_NONE). Some
    // mechanisms (e.g. the lookup of external pointers in an
    // ExternalPointerTable) omit bounds checks and instead guarantee that any
    // out-of-bounds access will land in a PROT_NONE mapping. Memory accesses
    // that _always_ cause such a permission violation are not exploitable and
    // so these crashes are filtered out here. However, testcases need to be
    // written with this in mind and must access other memory ranges.
    PrintToStderr(
        "Caught harmless memory access violaton (memory permission violation). "
        "Exiting process...\n");
    _exit(0);
  }

  // Otherwise it's a sandbox violation, so restore the original signal
  // handler, then return from this handler. The faulting instruction will be
  // re-executed and will again trigger the access violation, but now the
  // signal will be handled by the original signal handler.
  //
  // Should any of the sigaction calls below ever fail, the default signal
  // handler will be invoked (due to SA_RESETHAND) and will terminate the
  // process, so there's no need to attempt to handle that condition.
  sigaction(SIGBUS, &g_old_sigbus_handler, nullptr);
  sigaction(SIGSEGV, &g_old_sigsegv_handler, nullptr);
}
#endif  // V8_OS_LINUX

}  // namespace

void SandboxTesting::InstallSandboxCrashFilter() {
  CHECK(GetProcessWideSandbox()->is_initialized());
#ifdef V8_OS_LINUX
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_flags = SA_RESETHAND | SA_SIGINFO;
  action.sa_sigaction = &SandboxSignalHandler;
  sigemptyset(&action.sa_mask);

  bool success = true;
  success &= (sigaction(SIGABRT, &action, nullptr) == 0);
  success &= (sigaction(SIGBUS, &action, &g_old_sigbus_handler) == 0);
  success &= (sigaction(SIGSEGV, &action, &g_old_sigsegv_handler) == 0);
  CHECK(success);
#else
  FATAL("The sandbox crash filter is currently only available on Linux");
#endif  // V8_OS_LINUX
}

#endif  // V8_ENABLE_SANDBOX

}  // namespace internal
}  // namespace v8
