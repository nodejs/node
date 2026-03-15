#if HAVE_FFI

#include "node_ffi.h"
#include "../base_object-inl.h"
#include "../env-inl.h"
#include "../node_errors.h"

namespace node {

using v8::Context;
using v8::Function;
using v8::FunctionTemplate;
using v8::Local;
using v8::Object;
using v8::Value;

namespace ffi {

// Module initialization.
static void Initialize(Local<Object> target,
                       Local<Value> unused,
                       Local<Context> context,
                       void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // Create the DynamicLibrary template, but don't export it.
  DynamicLibrary::GetConstructorTemplate(env);

  // Create UnsafeFnPointer template
  Local<FunctionTemplate> fnptr_tmpl =
      UnsafeFnPointer::GetConstructorTemplate(env);
  SetConstructorFunction(context, target, "UnsafeFnPointer", fnptr_tmpl);

  // Create UnsafeCallback template
  Local<FunctionTemplate> cb_tmpl = UnsafeCallback::GetConstructorTemplate(env);
  SetConstructorFunction(context, target, "UnsafeCallback", cb_tmpl);

  Local<Function> unsafe_callback_ctor =
      cb_tmpl->GetFunction(context).ToLocalChecked();
  Local<Object> unsafe_callback_obj = unsafe_callback_ctor.As<Object>();
  SetMethod(
      context, unsafe_callback_obj, "threadSafe", UnsafeCallback::ThreadSafe);

  // Create UnsafePointer template
  Local<FunctionTemplate> ptr_tmpl = UnsafePointer::GetConstructorTemplate(env);

  // Export UnsafePointer as a constructor with static methods
  SetConstructorFunction(context, target, "UnsafePointer", ptr_tmpl);

  // Add static methods to the UnsafePointer constructor function
  Local<Function> unsafe_pointer_ctor =
      ptr_tmpl->GetFunction(context).ToLocalChecked();
  Local<Object> unsafe_pointer_obj = unsafe_pointer_ctor.As<Object>();

  SetMethod(context, unsafe_pointer_obj, "create", UnsafePointer::Create);
  SetMethod(context, unsafe_pointer_obj, "equals", UnsafePointer::Equals);
  SetMethod(context, unsafe_pointer_obj, "offset", UnsafePointer::Offset);
  SetMethod(context, unsafe_pointer_obj, "value", UnsafePointer::GetValue);

  // Create UnsafePointerView template
  Local<FunctionTemplate> ptrview_tmpl =
      UnsafePointerView::GetConstructorTemplate(env);
  SetConstructorFunction(context, target, "UnsafePointerView", ptrview_tmpl);

  // Add static methods to the UnsafePointerView constructor function
  Local<Function> unsafe_pointer_view_ctor =
      ptrview_tmpl->GetFunction(context).ToLocalChecked();
  Local<Object> unsafe_pointer_view_obj = unsafe_pointer_view_ctor.As<Object>();

  SetMethod(context,
            unsafe_pointer_view_obj,
            "getCString",
            UnsafePointerView::StaticGetCString);
  SetMethod(context,
            unsafe_pointer_view_obj,
            "copyInto",
            UnsafePointerView::StaticCopyInto);
  SetMethod(context,
            unsafe_pointer_view_obj,
            "getArrayBuffer",
            UnsafePointerView::StaticGetArrayBuffer);

  SetMethod(context, target, "dlopen", Dlopen);
}

}  // namespace ffi
}  // namespace node

NODE_BINDING_CONTEXT_AWARE_INTERNAL(ffi, node::ffi::Initialize)

#endif  // HAVE_FFI
