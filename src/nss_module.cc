#include "nss_module.h"

namespace node {
namespace nss_module {

using nss_wrap::NSSReqWrap;
using v8::Boolean;
using v8::Context;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::Local;
using v8::Object;
using v8::String;
using v8::Value;

NSSModule::NSSModule(Environment* env,
                     Local<Object> req_wrap_obj,
                     uv_lib_t* lib,
                     nss_gethostbyname3_r ghbn_3,
                     nss_gethostbyname4_r ghbn_4,
                     nss_gethostbyaddr2_r ghba_2)
    : BaseObject(env, req_wrap_obj),
      lib_(lib),
      ghbn3(ghbn_3),
      ghbn4(ghbn_4),
      ghba2(ghba_2) {
  Wrap(req_wrap_obj, this);
}


NSSModule::~NSSModule() {
  if (lib_ != nullptr) {
    uv_dlclose(lib_);
    delete lib_;
  }
}


// args: moduleName
void NSSModule::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

#ifdef _WIN32
  return env->ThrowError("NSSModule is not available on Windows.");
#else
  CHECK(args.IsConstructCall());
  CHECK(args[0]->IsString());

  node::Utf8Value js_module_name(env->isolate(), args[0]);
  const char* module_string = *js_module_name;
  int module_len = strlen(module_string);

  // Use allocated memory for both the library filename and the actual
  // function names. The latter is always longer, so we allocate that much
  // here.
  int name_len = 5 + module_len + 17 + 1;
  char* name = static_cast<char*>(malloc(name_len));

  sprintf(name, "libnss_%s.so.2", module_string);

  uv_lib_t* lib = static_cast<uv_lib_t*>(malloc(sizeof(uv_lib_t)));
  if (lib == nullptr)
    return env->ThrowError("malloc failed");

  int status = uv_dlopen(name, lib);
  if (status == -1) {
    free(name);
    const char* err_msg = uv_dlerror(lib);
    return env->ThrowError(env->isolate(), err_msg);
  }

  sprintf(name, "_nss_%s_gethostbyname3_r", module_string);

  nss_gethostbyname3_r ghbn3 = nullptr;
  nss_gethostbyname4_r ghbn4 = nullptr;
  nss_gethostbyaddr2_r ghba2 = nullptr;

  status = uv_dlsym(lib,
                    static_cast<const char*>(name),
                    reinterpret_cast<void**>(&ghbn3));
  if (ghbn3 == nullptr) {
    name[name_len - 4] = '2';
    status = uv_dlsym(lib,
                      static_cast<const char*>(name),
                      reinterpret_cast<void**>(&ghbn3));
    // TODO: resort to gethostbyname_r which does AF_UNSPEC?
  }

  // No need to obtain the parallel gethostbyname if we do not at least have the
  // non-parallel version
  if (ghbn3 != nullptr) {
    // gethostbyname4_r sends out parallel A and AAAA queries and
    // is thus only suitable for AF_UNSPEC.
    name[name_len - 4] = '4';
    status = uv_dlsym(lib,
                      static_cast<const char*>(name),
                      reinterpret_cast<void**>(&ghbn4));
  }

  memcpy(name + name_len - 8, "addr2", 5);
  status = uv_dlsym(lib,
                    static_cast<const char*>(name),
                    reinterpret_cast<void**>(&ghba2));

  if (ghba2 == nullptr) {
    // Try to get the non-ttl version
    memcpy(name + name_len - 8, "addr_r\0", 7);
    status = uv_dlsym(lib,
                      static_cast<const char*>(name),
                      reinterpret_cast<void**>(&ghba2));
  }

  free(name);

  if (ghbn3 == nullptr && ghba2 == nullptr) {
    uv_dlclose(lib);
    delete lib;
    return env->ThrowError("nss module missing needed gethostby*_r functions");
  }

  Local<Boolean> has_ghbn_v = Boolean::New(env->isolate(), ghbn3 != nullptr);
  Local<Boolean> has_ghba_v = Boolean::New(env->isolate(), ghba2 != nullptr);
  args.This()->ForceSet(env->hasbyname(), has_ghbn_v, v8::ReadOnly);
  args.This()->ForceSet(env->hasbyaddr(), has_ghba_v, v8::ReadOnly);
  args.This()->ForceSet(env->name_string(), args[0], v8::ReadOnly);

  new NSSModule(env, args.This(), lib, ghbn3, ghbn4, ghba2);
#endif
}


// args: req, [family]
void NSSModule::QueryName(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  NSSModule* nss = Unwrap<NSSModule>(args.Holder());

  if (nss->ghbn3 == nullptr && nss->ghbn4 == nullptr)
    return args.GetReturnValue().Set(false);

  CHECK(args[0]->IsObject());

  Local<Object> req_wrap_obj = args[0].As<Object>();

  int family = (args[1]->IsInt32()) ? args[1]->Int32Value() : 0;

  if (family != 0 && family != 4 && family != 6)
    return env->ThrowError("bad address family");

  NSSReqWrap* req_wrap = Unwrap<NSSReqWrap>(req_wrap_obj);

  if (req_wrap->host_type != -1)
    return env->ThrowError("wrong nss request type");

  req_wrap->module = nss;
  req_wrap->family = family;

  req_wrap->Ref();
  uv_queue_work(env->event_loop(),
                &req_wrap->req_,
                NSSReqWrap::NameWork,
                NSSReqWrap::NameAfter);
  req_wrap->Dispatched();

  args.GetReturnValue().Set(true);
}


// args: req
void NSSModule::QueryAddr(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  NSSModule* nss = Unwrap<NSSModule>(args.Holder());

  if (nss->ghba2 == nullptr)
    return args.GetReturnValue().Set(false);

  CHECK(args[0]->IsObject());

  Local<Object> req_wrap_obj = args[0].As<Object>();

  NSSReqWrap* req_wrap = Unwrap<NSSReqWrap>(req_wrap_obj);

  if (req_wrap->host_type != 4 && req_wrap->host_type != 6)
    return env->ThrowError("wrong nss request type");

  req_wrap->module = nss;

  req_wrap->Ref();
  uv_queue_work(env->event_loop(),
                &req_wrap->req_,
                NSSReqWrap::AddrWork,
                NSSReqWrap::AddrAfter);
  req_wrap->Dispatched();

  args.GetReturnValue().Set(true);
}


void NSSModule::Initialize(Handle<Object> target,
                           Handle<Value> unused,
                           Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  Local<String> class_name = FIXED_ONE_BYTE_STRING(env->isolate(), "NSSModule");
  Local<FunctionTemplate> nssmodule_tmpl =
      env->NewFunctionTemplate(NSSModule::New);
  nssmodule_tmpl->InstanceTemplate()->SetInternalFieldCount(1);
  nssmodule_tmpl->SetClassName(class_name);
  env->SetProtoMethod(nssmodule_tmpl, "queryName", NSSModule::QueryName);
  env->SetProtoMethod(nssmodule_tmpl, "queryAddr", NSSModule::QueryAddr);
  target->Set(class_name, nssmodule_tmpl->GetFunction());
}

}
}

NODE_MODULE_CONTEXT_AWARE_BUILTIN(nss_module,
                                  node::nss_module::NSSModule::Initialize)
