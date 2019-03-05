#include "node_dir.h"
#include "node_file-inl.h"
#include "node_process.h"
#include "memory_tracker-inl.h"
#include "util.h"

#include "tracing/trace_event.h"

#include "string_bytes.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>
#include <cerrno>
#include <climits>

#include <memory>

namespace node {

namespace fs_dir {

using fs::FSReqAfterScope;
using fs::FSReqBase;
using fs::FSReqWrapSync;
using fs::GetReqWrap;

using v8::Array;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Value;

#define TRACE_NAME(name) "fs_dir.sync." #name
#define GET_TRACE_ENABLED                                                      \
  (*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED                                 \
  (TRACING_CATEGORY_NODE2(fs_dir, sync)) != 0)
#define FS_DIR_SYNC_TRACE_BEGIN(syscall, ...)                                  \
  if (GET_TRACE_ENABLED)                                                       \
  TRACE_EVENT_BEGIN(TRACING_CATEGORY_NODE2(fs_dir, sync), TRACE_NAME(syscall), \
  ##__VA_ARGS__);
#define FS_DIR_SYNC_TRACE_END(syscall, ...)                                    \
  if (GET_TRACE_ENABLED)                                                       \
  TRACE_EVENT_END(TRACING_CATEGORY_NODE2(fs_dir, sync), TRACE_NAME(syscall),   \
  ##__VA_ARGS__);

DirHandle::DirHandle(Environment* env, Local<Object> obj, uv_dir_t* dir)
    : AsyncWrap(env, obj, AsyncWrap::PROVIDER_DIRHANDLE),
      dir_(dir) {
  MakeWeak();

  dir_->nentries = 0;
  dir_->dirents = nullptr;
}

DirHandle* DirHandle::New(Environment* env, uv_dir_t* dir) {
  Local<Object> obj;
  if (!env->dir_instance_template()
          ->NewInstance(env->context())
          .ToLocal(&obj)) {
    return nullptr;
  }

  return new DirHandle(env, obj, dir);
}

void DirHandle::New(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}

DirHandle::~DirHandle() {
  CHECK(!closing_);  // We should not be deleting while explicitly closing!
  GCClose();         // Close synchronously and emit warning
  CHECK(closed_);    // We have to be closed at the point
}

void DirHandle::MemoryInfo(MemoryTracker* tracker) const {
  tracker->TrackFieldWithSize("dir", sizeof(*dir_));
}

// Close the directory handle if it hasn't already been closed. A process
// warning will be emitted using a SetImmediate to avoid calling back to
// JS during GC. If closing the fd fails at this point, a fatal exception
// will crash the process immediately.
inline void DirHandle::GCClose() {
  if (closed_) return;
  uv_fs_t req;
  int ret = uv_fs_closedir(nullptr, &req, dir_, nullptr);
  uv_fs_req_cleanup(&req);
  closing_ = false;
  closed_ = true;

  struct err_detail { int ret; };

  err_detail detail { ret };

  if (ret < 0) {
    // Do not unref this
    env()->SetImmediate([detail](Environment* env) {
      char msg[70];
      snprintf(msg, arraysize(msg),
              "Closing directory handle on garbage collection failed");
      // This exception will end up being fatal for the process because
      // it is being thrown from within the SetImmediate handler and
      // there is no JS stack to bubble it to. In other words, tearing
      // down the process is the only reasonable thing we can do here.
      HandleScope handle_scope(env->isolate());
      env->ThrowUVException(detail.ret, "close", msg);
    });
    return;
  }

  // If the close was successful, we still want to emit a process warning
  // to notify that the file descriptor was gc'd. We want to be noisy about
  // this because not explicitly closing the DirHandle is a bug.

  env()->SetUnrefImmediate([](Environment* env) {
    ProcessEmitWarning(env,
                       "Closing directory handle on garbage collection");
  });
}

void AfterClose(uv_fs_t* req) {
  FSReqBase* req_wrap = FSReqBase::from_req(req);
  FSReqAfterScope after(req_wrap, req);

  if (after.Proceed())
    req_wrap->Resolve(Undefined(req_wrap->env()->isolate()));
}

void DirHandle::Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();
  CHECK_GE(argc, 1);

  DirHandle* dir;
  ASSIGN_OR_RETURN_UNWRAP(&dir, args.Holder());

  dir->closing_ = false;
  dir->closed_ = true;

  FSReqBase* req_wrap_async = GetReqWrap(args, 0);
  if (req_wrap_async != nullptr) {  // close(req)
    AsyncCall(env, req_wrap_async, args, "closedir", UTF8, AfterClose,
              uv_fs_closedir, dir->dir());
  } else {  // close(undefined, ctx)
    CHECK_EQ(argc, 2);
    FSReqWrapSync req_wrap_sync;
    FS_DIR_SYNC_TRACE_BEGIN(closedir);
    SyncCall(env, args[1], &req_wrap_sync, "closedir", uv_fs_closedir,
             dir->dir());
    FS_DIR_SYNC_TRACE_END(closedir);
  }
}

static MaybeLocal<Array> DirentListToArray(
    Environment* env,
    uv_dirent_t* ents,
    int num,
    enum encoding encoding,
    Local<Value>* err_out) {
  MaybeStackBuffer<Local<Value>, 64> entries(num * 2);

  // Return an array of all read filenames.
  int j = 0;
  for (int i = 0; i < num; i++) {
    Local<Value> filename;
    Local<Value> error;
    const size_t namelen = strlen(ents[i].name);
    if (!StringBytes::Encode(env->isolate(),
                             ents[i].name,
                             namelen,
                             encoding,
                             &error).ToLocal(&filename)) {
      *err_out = error;
      return MaybeLocal<Array>();
    }

    entries[j++] = filename;
    entries[j++] = Integer::New(env->isolate(), ents[i].type);
  }

  return Array::New(env->isolate(), entries.out(), j);
}

static void AfterDirRead(uv_fs_t* req) {
  FSReqBase* req_wrap = FSReqBase::from_req(req);
  FSReqAfterScope after(req_wrap, req);

  if (!after.Proceed()) {
    return;
  }

  Environment* env = req_wrap->env();
  Isolate* isolate = env->isolate();

  if (req->result == 0) {
    // Done
    Local<Value> done = Null(isolate);
    req_wrap->Resolve(done);
    return;
  }

  uv_dir_t* dir = static_cast<uv_dir_t*>(req->ptr);
  req->ptr = nullptr;

  Local<Value> error;
  Local<Array> js_array;
  if (!DirentListToArray(env,
                         dir->dirents,
                         req->result,
                         req_wrap->encoding(),
                         &error).ToLocal(&js_array)) {
    return req_wrap->Reject(error);
  }

  req_wrap->Resolve(js_array);
}


void DirHandle::Read(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  const int argc = args.Length();
  CHECK_GE(argc, 3);

  const enum encoding encoding = ParseEncoding(isolate, args[0], UTF8);

  DirHandle* dir;
  ASSIGN_OR_RETURN_UNWRAP(&dir, args.Holder());

  CHECK(args[1]->IsNumber());
  uint64_t buffer_size = args[1].As<Number>()->Value();

  if (buffer_size != dir->dirents_.size()) {
    dir->dirents_.resize(buffer_size);
    dir->dir_->nentries = buffer_size;
    dir->dir_->dirents = dir->dirents_.data();
  }

  FSReqBase* req_wrap_async = GetReqWrap(args, 2);
  if (req_wrap_async != nullptr) {  // dir.read(encoding, bufferSize, req)
    AsyncCall(env, req_wrap_async, args, "readdir", encoding,
              AfterDirRead, uv_fs_readdir, dir->dir());
  } else {  // dir.read(encoding, bufferSize, undefined, ctx)
    CHECK_EQ(argc, 4);
    FSReqWrapSync req_wrap_sync;
    FS_DIR_SYNC_TRACE_BEGIN(readdir);
    int err = SyncCall(env, args[3], &req_wrap_sync, "readdir", uv_fs_readdir,
                       dir->dir());
    FS_DIR_SYNC_TRACE_END(readdir);
    if (err < 0) {
      return;  // syscall failed, no need to continue, error info is in ctx
    }

    if (req_wrap_sync.req.result == 0) {
      // Done
      Local<Value> done = Null(isolate);
      args.GetReturnValue().Set(done);
      return;
    }

    CHECK_GE(req_wrap_sync.req.result, 0);

    Local<Value> error;
    Local<Array> js_array;
    if (!DirentListToArray(env,
                           dir->dir()->dirents,
                           req_wrap_sync.req.result,
                           encoding,
                           &error).ToLocal(&js_array)) {
      Local<Object> ctx = args[2].As<Object>();
      USE(ctx->Set(env->context(), env->error_string(), error));
      return;
    }

    args.GetReturnValue().Set(js_array);
  }
}

void AfterOpenDir(uv_fs_t* req) {
  FSReqBase* req_wrap = FSReqBase::from_req(req);
  FSReqAfterScope after(req_wrap, req);

  if (!after.Proceed()) {
    return;
  }

  Environment* env = req_wrap->env();

  uv_dir_t* dir = static_cast<uv_dir_t*>(req->ptr);
  DirHandle* handle = DirHandle::New(env, dir);

  req_wrap->Resolve(handle->object().As<Value>());
}

static void OpenDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  const int argc = args.Length();
  CHECK_GE(argc, 3);

  BufferValue path(isolate, args[0]);
  CHECK_NOT_NULL(*path);

  const enum encoding encoding = ParseEncoding(isolate, args[1], UTF8);

  FSReqBase* req_wrap_async = GetReqWrap(args, 2);
  if (req_wrap_async != nullptr) {  // openDir(path, encoding, req)
    AsyncCall(env, req_wrap_async, args, "opendir", encoding, AfterOpenDir,
              uv_fs_opendir, *path);
  } else {  // openDir(path, encoding, undefined, ctx)
    CHECK_EQ(argc, 4);
    FSReqWrapSync req_wrap_sync;
    FS_DIR_SYNC_TRACE_BEGIN(opendir);
    int result = SyncCall(env, args[3], &req_wrap_sync, "opendir",
                          uv_fs_opendir, *path);
    FS_DIR_SYNC_TRACE_END(opendir);
    if (result < 0) {
      return;  // syscall failed, no need to continue, error info is in ctx
    }

    uv_fs_t* req = &req_wrap_sync.req;
    uv_dir_t* dir = static_cast<uv_dir_t*>(req->ptr);
    DirHandle* handle = DirHandle::New(env, dir);

    args.GetReturnValue().Set(handle->object().As<Value>());
  }
}

void Initialize(Local<Object> target,
                Local<Value> unused,
                Local<Context> context,
                void* priv) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  env->SetMethod(target, "opendir", OpenDir);

  // Create FunctionTemplate for DirHandle
  Local<FunctionTemplate> dir = env->NewFunctionTemplate(DirHandle::New);
  dir->Inherit(AsyncWrap::GetConstructorTemplate(env));
  env->SetProtoMethod(dir, "read", DirHandle::Read);
  env->SetProtoMethod(dir, "close", DirHandle::Close);
  Local<ObjectTemplate> dirt = dir->InstanceTemplate();
  dirt->SetInternalFieldCount(DirHandle::kInternalFieldCount);
  Local<String> handleString =
       FIXED_ONE_BYTE_STRING(isolate, "DirHandle");
  dir->SetClassName(handleString);
  target
      ->Set(context, handleString,
            dir->GetFunction(env->context()).ToLocalChecked())
      .FromJust();
  env->set_dir_instance_template(dirt);
}

}  // namespace fs_dir

}  // end namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(fs_dir, node::fs_dir::Initialize)
