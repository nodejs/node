#include <node.h>
#include <v8.h>
#include <stdlib.h>

#ifdef _AIX
// AIX allows over-allocation, and will SIGKILL when the allocated pages are
// used if there is not enough VM. Check for available space until
// out-of-memory.  Don't allow more then some (large) proportion of it to be
// used for the test strings, so Node & V8 have some space for allocations.
#include <signal.h>
#include <sys/vminfo.h>

static void* Allowed(size_t size) {
  blkcnt_t allowedBlocks = psdanger(SIGKILL);

  if (allowedBlocks < 1) {
    // Already OOM
    return nullptr;
  }
  const size_t BYTES_PER_BLOCK = 512;
  size_t allowed = (allowedBlocks * BYTES_PER_BLOCK * 4) / 5;
  if (size < allowed) {
    return malloc(size);
  }
  return nullptr;
}
#else
// Other systems also allow over-allocation, but this malloc-and-free approach
// seems to be working for them.
static void* Allowed(size_t size) {
  return malloc(size);
}
#endif  // _AIX

void EnsureAllocation(const v8::FunctionCallbackInfo<v8::Value> &args) {
  v8::Isolate* isolate = args.GetIsolate();
  uintptr_t size = args[0].As<v8::Integer>()->Value();
  v8::Local<v8::Boolean> success;

  void* buffer = Allowed(size);
  if (buffer) {
    success = v8::Boolean::New(isolate, true);
    free(buffer);
  } else {
    success = v8::Boolean::New(isolate, false);
  }
  args.GetReturnValue().Set(success);
}

void init(v8::Local<v8::Object> exports) {
  NODE_SET_METHOD(exports, "ensureAllocation", EnsureAllocation);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
