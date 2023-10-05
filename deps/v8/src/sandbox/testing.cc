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
#include <sys/mman.h>
#include <unistd.h>
#endif  // V8_OS_LINUX

namespace v8 {
namespace internal {

#ifdef V8_ENABLE_SANDBOX

#ifdef V8_EXPOSE_MEMORY_CORRUPTION_API

namespace {

// Sandbox.byteLength
void SandboxGetByteLength(const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();
  double sandbox_size = GetProcessWideSandbox()->size();
  info.GetReturnValue().Set(v8::Number::New(isolate, sandbox_size));
}

// new Sandbox.MemoryView(info) -> Sandbox.MemoryView
void SandboxMemoryView(const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();
  Local<v8::Context> context = isolate->GetCurrentContext();

  if (!info.IsConstructCall()) {
    isolate->ThrowError("Sandbox.MemoryView must be invoked with 'new'");
    return;
  }

  Local<v8::Integer> arg1, arg2;
  if (!info[0]->ToInteger(context).ToLocal(&arg1) ||
      !info[1]->ToInteger(context).ToLocal(&arg2)) {
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
  info.GetReturnValue().Set(Utils::ToLocal(buffer));
}

// The methods below either take a HeapObject or the address of a HeapObject as
// argument. These helper functions can be used to extract the argument object
// in both cases.
using ArgumentObjectExtractorFunction = std::function<bool(
    const v8::FunctionCallbackInfo<v8::Value>&, Tagged<HeapObject>* out)>;

static bool GetArgumentObjectPassedAsReference(
    const v8::FunctionCallbackInfo<v8::Value>& info, Tagged<HeapObject>* out) {
  v8::Isolate* isolate = info.GetIsolate();

  if (info.Length() == 0) {
    isolate->ThrowError("First argument must be provided");
    return false;
  }

  Handle<Object> arg = Utils::OpenHandle(*info[0]);
  if (!IsHeapObject(*arg)) {
    isolate->ThrowError("First argument must be a HeapObject");
    return false;
  }

  *out = HeapObject::cast(*arg);
  return true;
}

static bool GetArgumentObjectPassedAsAddress(
    const v8::FunctionCallbackInfo<v8::Value>& info, Tagged<HeapObject>* out) {
  Sandbox* sandbox = GetProcessWideSandbox();
  v8::Isolate* isolate = info.GetIsolate();
  Local<v8::Context> context = isolate->GetCurrentContext();

  if (info.Length() == 0) {
    isolate->ThrowError("First argument must be provided");
    return false;
  }

  Local<v8::Uint32> arg1;
  if (!info[0]->ToUint32(context).ToLocal(&arg1)) {
    isolate->ThrowError("First argument must be the address of a HeapObject");
    return false;
  }

  uint32_t address = arg1->Value();
  // Allow tagged addresses by removing the kHeapObjectTag and
  // kWeakHeapObjectTag. This allows clients to just read tagged pointers from
  // the heap and use them for these APIs.
  address &= ~kHeapObjectTagMask;
  *out = HeapObject::FromAddress(sandbox->base() + address);
  return true;
}

// Sandbox.getAddressOf(Object) -> Number
void SandboxGetAddressOf(const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();

  Tagged<HeapObject> obj;
  if (!GetArgumentObjectPassedAsReference(info, &obj)) {
    return;
  }

  // HeapObjects must be allocated inside the pointer compression cage so their
  // address relative to the start of the sandbox can be obtained simply by
  // taking the lowest 32 bits of the absolute address.
  uint32_t address = static_cast<uint32_t>(obj->address());
  info.GetReturnValue().Set(v8::Integer::NewFromUnsigned(isolate, address));
}

// Sandbox.getObjectAt(Number) -> Object
void SandboxGetObjectAt(const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();

  Tagged<HeapObject> obj;
  if (!GetArgumentObjectPassedAsAddress(info, &obj)) {
    return;
  }

  Isolate* i_isolate = reinterpret_cast<Isolate*>(isolate);
  Handle<Object> handle(obj, i_isolate);
  info.GetReturnValue().Set(ToApiHandle<v8::Value>(handle));
}

// Sandbox.isValidObjectAt(Address) -> Bool
void SandboxIsValidObjectAt(const v8::FunctionCallbackInfo<v8::Value>& info) {
  DCHECK(ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();

  Tagged<HeapObject> obj;
  if (!GetArgumentObjectPassedAsAddress(info, &obj)) {
    return;
  }

  Heap* heap = reinterpret_cast<Isolate*>(isolate)->heap();
  auto IsLocatedInMappedMemory = [&](Tagged<HeapObject> obj) {
    // Note that IsOutsideAllocatedSpace is imprecise and may return false for
    // some addresses outside the allocated space. However, it's probably good
    // enough for our purposes.
    return !heap->memory_allocator()->IsOutsideAllocatedSpace(obj.address());
  };

  bool is_valid = false;
  if (IsLocatedInMappedMemory(obj)) {
    Tagged<Map> map = obj->map();
    if (IsLocatedInMappedMemory(map)) {
      is_valid = IsMap(map);
    }
  }

  info.GetReturnValue().Set(is_valid);
}

static void SandboxIsWritableImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    ArgumentObjectExtractorFunction getArgumentObject) {
  DCHECK(ValidateCallbackInfo(info));

  Tagged<HeapObject> obj;
  if (!getArgumentObject(info, &obj)) {
    return;
  }

  BasicMemoryChunk* chunk = BasicMemoryChunk::FromHeapObject(obj);
  bool is_writable = chunk->IsWritable();
  info.GetReturnValue().Set(is_writable);
}

// Sandbox.isWritable(Object) -> Bool
void SandboxIsWritable(const v8::FunctionCallbackInfo<v8::Value>& info) {
  SandboxIsWritableImpl(info, &GetArgumentObjectPassedAsReference);
}

// Sandbox.isWritableObjectAt(Number) -> Bool
void SandboxIsWritableObjectAt(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SandboxIsWritableImpl(info, &GetArgumentObjectPassedAsAddress);
}

static void SandboxGetSizeOfImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    ArgumentObjectExtractorFunction getArgumentObject) {
  DCHECK(ValidateCallbackInfo(info));

  Tagged<HeapObject> obj;
  if (!getArgumentObject(info, &obj)) {
    return;
  }

  int size = obj->Size();
  info.GetReturnValue().Set(size);
}

// Sandbox.getSizeOf(Object) -> Number
void SandboxGetSizeOf(const v8::FunctionCallbackInfo<v8::Value>& info) {
  SandboxGetSizeOfImpl(info, &GetArgumentObjectPassedAsReference);
}

// Sandbox.getSizeOfObjectAt(Number) -> Number
void SandboxGetSizeOfObjectAt(const v8::FunctionCallbackInfo<v8::Value>& info) {
  SandboxGetSizeOfImpl(info, &GetArgumentObjectPassedAsAddress);
}

static void SandboxGetInstanceTypeOfImpl(
    const v8::FunctionCallbackInfo<v8::Value>& info,
    ArgumentObjectExtractorFunction getArgumentObject) {
  DCHECK(ValidateCallbackInfo(info));
  v8::Isolate* isolate = info.GetIsolate();

  Tagged<HeapObject> obj;
  if (!getArgumentObject(info, &obj)) {
    return;
  }

  InstanceType type = obj->map()->instance_type();
  std::stringstream out;
  out << type;
  MaybeLocal<v8::String> result =
      v8::String::NewFromUtf8(isolate, out.str().c_str());
  info.GetReturnValue().Set(result.ToLocalChecked());
}

// Sandbox.getInstanceTypeOf(Object) -> String
void SandboxGetInstanceTypeOf(const v8::FunctionCallbackInfo<v8::Value>& info) {
  SandboxGetInstanceTypeOfImpl(info, &GetArgumentObjectPassedAsReference);
}

// Sandbox.getInstanceTypeOfObjectAt(Number) -> String
void SandboxGetInstanceTypeOfObjectAt(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  SandboxGetInstanceTypeOfImpl(info, &GetArgumentObjectPassedAsAddress);
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
  return ApiNatives::InstantiateFunction(isolate, function_template, name)
      .ToHandleChecked();
}

void InstallFunc(Isolate* isolate, Handle<JSObject> holder,
                 FunctionCallback func, const char* name, int num_parameters,
                 bool is_constructor) {
  Factory* factory = isolate->factory();
  Handle<String> function_name = factory->NewStringFromAsciiChecked(name);
  Handle<JSFunction> function =
      CreateFunc(isolate, func, function_name, is_constructor);
  function->shared()->set_length(num_parameters);
  JSObject::AddProperty(isolate, holder, function_name, function, NONE);
}

void InstallGetter(Isolate* isolate, Handle<JSObject> object,
                   FunctionCallback func, const char* name) {
  Factory* factory = isolate->factory();
  Handle<String> property_name = factory->NewStringFromAsciiChecked(name);
  Handle<JSFunction> getter = CreateFunc(isolate, func, property_name, false);
  Handle<Object> setter = factory->null_value();
  JSObject::DefineOwnAccessorIgnoreAttributes(object, property_name, getter,
                                              setter, FROZEN);
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
  InstallFunction(isolate, sandbox, SandboxGetObjectAt, "getObjectAt", 1);
  InstallFunction(isolate, sandbox, SandboxIsValidObjectAt, "isValidObjectAt",
                  1);
  InstallFunction(isolate, sandbox, SandboxIsWritable, "isWritable", 1);
  InstallFunction(isolate, sandbox, SandboxIsWritableObjectAt,
                  "isWritableObjectAt", 1);
  InstallFunction(isolate, sandbox, SandboxGetSizeOf, "getSizeOf", 1);
  InstallFunction(isolate, sandbox, SandboxGetSizeOfObjectAt,
                  "getSizeOfObjectAt", 1);
  InstallFunction(isolate, sandbox, SandboxGetInstanceTypeOf,
                  "getInstanceTypeOf", 1);
  InstallFunction(isolate, sandbox, SandboxGetInstanceTypeOfObjectAt,
                  "getInstanceTypeOfObjectAt", 1);

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
struct sigaction g_old_sigabrt_handler, g_old_sigtrap_handler,
    g_old_sigbus_handler, g_old_sigsegv_handler;
void SandboxSignalHandler(int signal, siginfo_t* info, void* void_context) {
  // NOTE: This code MUST be async-signal safe.
  // NO malloc or stdio is allowed here.

  if (signal == SIGABRT) {
    // SIGABRT typically indicates a failed CHECK which is harmless.
    PrintToStderr("Caught harmless signal (SIGABRT). Exiting process...\n");
    _exit(0);
  }

  if (signal == SIGTRAP) {
    // Similarly, SIGTRAP probably indicates UNREACHABLE code.
    PrintToStderr("Caught harmless signal (SIGTRAP). Exiting process...\n");
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

  if (faultaddr >= 0x8000'0000'0000'0000ULL) {
    // On Linux, it appears that the kernel will still report valid (i.e.
    // canonical) kernel space addresses via the si_addr field, so we need to
    // handle these separately. We've already filtered out non-canonical
    // addresses above, so here we can just test if the most-significant bit of
    // the address is set, and if so assume that it's a kernel address.
    PrintToStderr(
        "Caught harmless memory access violatation (kernel space address). "
        "Exiting process...\n");
    _exit(0);
  }

  if (faultaddr < 0x1000) {
    // Nullptr dereferences are harmless as nothing can be mapped there. We use
    // the typical page size (which is also the default value of mmap_min_addr
    // on Linux) to determine what counts as a nullptr dereference here.
    PrintToStderr(
        "Caught harmless memory access violaton (nullptr dereference). Exiting "
        "process...\n");
    _exit(0);
  }

  if (faultaddr < 4ULL * GB) {
    // Currently we also ignore access violations in the first 4GB of the
    // virtual address space. See crbug.com/1470641 for more details.
    PrintToStderr(
        "Caught harmless memory access violaton (first 4GB of virtual address "
        "space). Exiting process...\n");
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

  // TODO(saelo) also try to detect harmless stack overflows here if possible.

  // Otherwise it's a sandbox violation, so restore the original signal
  // handlers, then return from this handler. The faulting instruction will be
  // re-executed and will again trigger the access violation, but now the
  // signal will be handled by the original signal handler.
  //
  // It's important that we restore all signal handlers here. For example, if
  // we forward a SIGSEGV to Asan's signal handler, that signal handler may
  // terminate the process with SIGABRT, which we must then *not* ignore.
  //
  // Should any of the sigaction calls below ever fail, the default signal
  // handler will be invoked (due to SA_RESETHAND) and will terminate the
  // process, so there's no need to attempt to handle that condition.
  sigaction(SIGABRT, &g_old_sigabrt_handler, nullptr);
  sigaction(SIGTRAP, &g_old_sigtrap_handler, nullptr);
  sigaction(SIGBUS, &g_old_sigbus_handler, nullptr);
  sigaction(SIGSEGV, &g_old_sigsegv_handler, nullptr);
}
#endif  // V8_OS_LINUX

}  // namespace

void SandboxTesting::InstallSandboxCrashFilter() {
  CHECK(GetProcessWideSandbox()->is_initialized());
#ifdef V8_OS_LINUX
  // Register an alternate stack for signal delivery so that signal handlers
  // can run properly even if for example the stack pointer has been corrupted
  // or the stack has overflowed.
  // Note that the alternate stack is currently only registered for the main
  // thread. Stack pointer corruption or stack overflows on background threads
  // may therefore still cause the signal handler to crash.
  void* alternate_stack = mmap(nullptr, SIGSTKSZ, PROT_READ | PROT_WRITE,
                               MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  CHECK_NE(alternate_stack, MAP_FAILED);
  stack_t signalstack = {
      .ss_sp = alternate_stack,
      .ss_flags = 0,
      .ss_size = static_cast<size_t>(SIGSTKSZ),
  };
  CHECK_EQ(sigaltstack(&signalstack, nullptr), 0);

  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_flags = SA_RESETHAND | SA_SIGINFO | SA_ONSTACK;
  action.sa_sigaction = &SandboxSignalHandler;
  sigemptyset(&action.sa_mask);

  bool success = true;
  success &= (sigaction(SIGABRT, &action, &g_old_sigabrt_handler) == 0);
  success &= (sigaction(SIGTRAP, &action, &g_old_sigtrap_handler) == 0);
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
