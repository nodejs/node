#ifndef SRC_NODE_FFI_H_
#define SRC_NODE_FFI_H_

#include "node.h"
#include "v8.h"
#include "ffi.h"

namespace node {
namespace ffi {

using v8::Isolate;
using v8::Object;
using v8::Persistent;

/**
 * Created for every ffi.Closure JS instance that gets instantiated.
 */

typedef struct _closure_info {
  ffi_closure closure;
  Persistent<Object> callback;
  ffi_cif* cif;
  Isolate* isolate;
} closure_info;


/**
 * Created for every ffi.callAsync() invokation.
 */

typedef struct _async_call {
  Persistent<Object> callback;
  ffi_cif* cif;
  char* fn;
  void* res;
  void** argv;
  Isolate* isolate;
} async_call;


/**
 * Synchronization object to ensure following order of execution:
 *   -> WaitForExecution()     invoked
 *   -> SignalDoneExecuting()  returned
 *   -> WaitForExecution()     returned
 *
 * ^WaitForExecution() must always be called from the thread which
 * owns the object.
 */

class ThreadedCallbackInvokation {
  public:
    ThreadedCallbackInvokation(closure_info* info, void* ret, void** args);
    ~ThreadedCallbackInvokation();

    void SignalDoneExecuting();
    void WaitForExecution();

    closure_info* info_;
    void* ret_;
    void** args_;
    ThreadedCallbackInvokation* next_;

  private:
    uv_cond_t cond_;
    uv_mutex_t mutex_;
};


// populated in ::Initialize()
#ifdef WIN32
static DWORD js_thread_id;
#else
static uv_thread_t js_thread;
#endif  // WIN32
static uv_mutex_t queue_mutex;
static ThreadedCallbackInvokation* callback_queue;
static uv_async_t async;

}  // namespace ffi
}  // namespace node

#endif  // SRC_NODE_FFI_H_
