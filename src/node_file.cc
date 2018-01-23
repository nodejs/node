// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "aliased_buffer.h"
#include "node_buffer.h"
#include "node_internals.h"
#include "node_stat_watcher.h"
#include "node_file.h"

#include "req_wrap-inl.h"
#include "string_bytes.h"
#include "string_search.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#if defined(__MINGW32__) || defined(_MSC_VER)
# include <io.h>
#endif

#include <memory>
#include <vector>

namespace node {

void FillStatsArray(AliasedBuffer<double, v8::Float64Array>* fields_ptr,
                    const uv_stat_t* s, int offset) {
  AliasedBuffer<double, v8::Float64Array>& fields = *fields_ptr;
  fields[offset + 0] = s->st_dev;
  fields[offset + 1] = s->st_mode;
  fields[offset + 2] = s->st_nlink;
  fields[offset + 3] = s->st_uid;
  fields[offset + 4] = s->st_gid;
  fields[offset + 5] = s->st_rdev;
#if defined(__POSIX__)
  fields[offset + 6] = s->st_blksize;
#else
  fields[offset + 6] = -1;
#endif
  fields[offset + 7] = s->st_ino;
  fields[offset + 8] = s->st_size;
#if defined(__POSIX__)
  fields[offset + 9] = s->st_blocks;
#else
  fields[offset + 9] = -1;
#endif
// Dates.
// NO-LINT because the fields are 'long' and we just want to cast to `unsigned`
#define X(idx, name)                                                    \
  /* NOLINTNEXTLINE(runtime/int) */                                     \
  fields[offset + idx] = ((unsigned long)(s->st_##name.tv_sec) * 1e3) + \
  /* NOLINTNEXTLINE(runtime/int) */                                     \
                ((unsigned long)(s->st_##name.tv_nsec) / 1e6);          \

  X(10, atim)
  X(11, mtim)
  X(12, ctim)
  X(13, birthtim)
#undef X
}

namespace fs {

using v8::Array;
using v8::Context;
using v8::Float64Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Int32;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::MaybeLocal;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Undefined;
using v8::Value;

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define GET_OFFSET(a) ((a)->IsNumber() ? (a)->IntegerValue() : -1)

void FSReqWrap::Reject(Local<Value> reject) {
  MakeCallback(env()->oncomplete_string(), 1, &reject);
}

void FSReqWrap::FillStatsArray(const uv_stat_t* stat) {
  node::FillStatsArray(env()->fs_stats_field_array(), stat);
}

void FSReqWrap::ResolveStat() {
  Resolve(Undefined(env()->isolate()));
}

void FSReqWrap::Resolve(Local<Value> value) {
  Local<Value> argv[2] {
    Null(env()->isolate()),
    value
  };
  MakeCallback(env()->oncomplete_string(), arraysize(argv), argv);
}

void FSReqWrap::Init(const char* syscall,
                     const char* data,
                     size_t len,
                     enum encoding encoding) {
  syscall_ = syscall;
  encoding_ = encoding;

  if (data != nullptr) {
    CHECK_EQ(data_, nullptr);
    buffer_.AllocateSufficientStorage(len + 1);
    buffer_.SetLengthAndZeroTerminate(len);
    memcpy(*buffer_, data, len);
    data_ = *buffer_;
  }
}

void NewFSReqWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  new FSReqWrap(env, args.This());
}


FSReqAfterScope::FSReqAfterScope(FSReqWrap* wrap, uv_fs_t* req)
    : wrap_(wrap),
      req_(req),
      handle_scope_(wrap->env()->isolate()),
      context_scope_(wrap->env()->context()) {
  CHECK_EQ(wrap_->req(), req);
}

FSReqAfterScope::~FSReqAfterScope() {
  uv_fs_req_cleanup(wrap_->req());
  delete wrap_;
}

// TODO(joyeecheung): create a normal context object, and
// construct the actual errors in the JS land using the context.
// The context should include fds for some fs APIs, currently they are
// missing in the error messages. The path, dest, syscall, fd, .etc
// can be put into the context before the binding is even invoked,
// the only information that has to come from the C++ layer is the
// error number (and possibly the syscall for abstraction),
// which is also why the errors should have been constructed
// in JS for more flexibility.
void FSReqAfterScope::Reject(uv_fs_t* req) {
  wrap_->Reject(UVException(wrap_->env()->isolate(),
                            req->result,
                            wrap_->syscall(),
                            nullptr,
                            req->path,
                            wrap_->data()));
}

bool FSReqAfterScope::Proceed() {
  if (req_->result < 0) {
    Reject(req_);
    return false;
  }
  return true;
}

void AfterNoArgs(uv_fs_t* req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  FSReqAfterScope after(req_wrap, req);

  if (after.Proceed())
    req_wrap->Resolve(Undefined(req_wrap->env()->isolate()));
}

void AfterStat(uv_fs_t* req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  FSReqAfterScope after(req_wrap, req);

  if (after.Proceed()) {
    req_wrap->FillStatsArray(&req->statbuf);
    req_wrap->ResolveStat();
  }
}

void AfterInteger(uv_fs_t* req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  FSReqAfterScope after(req_wrap, req);

  if (after.Proceed())
    req_wrap->Resolve(Integer::New(req_wrap->env()->isolate(), req->result));
}

void AfterStringPath(uv_fs_t* req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  FSReqAfterScope after(req_wrap, req);

  MaybeLocal<Value> link;
  Local<Value> error;

  if (after.Proceed()) {
    link = StringBytes::Encode(req_wrap->env()->isolate(),
                               static_cast<const char*>(req->path),
                               req_wrap->encoding(),
                               &error);
    if (link.IsEmpty())
      req_wrap->Reject(error);
    else
      req_wrap->Resolve(link.ToLocalChecked());
  }
}

void AfterStringPtr(uv_fs_t* req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  FSReqAfterScope after(req_wrap, req);

  MaybeLocal<Value> link;
  Local<Value> error;

  if (after.Proceed()) {
    link = StringBytes::Encode(req_wrap->env()->isolate(),
                               static_cast<const char*>(req->ptr),
                               req_wrap->encoding(),
                               &error);
    if (link.IsEmpty())
      req_wrap->Reject(error);
    else
      req_wrap->Resolve(link.ToLocalChecked());
  }
}

void AfterScanDir(uv_fs_t* req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  FSReqAfterScope after(req_wrap, req);

  if (after.Proceed()) {
    Environment* env = req_wrap->env();
    Local<Value> error;
    int r;
    Local<Array> names = Array::New(env->isolate(), 0);
    Local<Function> fn = env->push_values_to_array_function();
    Local<Value> name_argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
    size_t name_idx = 0;

    for (int i = 0; ; i++) {
      uv_dirent_t ent;

      r = uv_fs_scandir_next(req, &ent);
      if (r == UV_EOF)
        break;
      if (r != 0) {
        return req_wrap->Reject(
            UVException(r, nullptr, req_wrap->syscall(),
                        static_cast<const char*>(req->path)));
      }

      MaybeLocal<Value> filename =
          StringBytes::Encode(env->isolate(),
                              ent.name,
                              req_wrap->encoding(),
                              &error);
      if (filename.IsEmpty())
        return req_wrap->Reject(error);

      name_argv[name_idx++] = filename.ToLocalChecked();

      if (name_idx >= arraysize(name_argv)) {
        fn->Call(env->context(), names, name_idx, name_argv)
            .ToLocalChecked();
        name_idx = 0;
      }
    }

    if (name_idx > 0) {
      fn->Call(env->context(), names, name_idx, name_argv)
          .ToLocalChecked();
    }

    req_wrap->Resolve(names);
  }
}


// This struct is only used on sync fs calls.
// For async calls FSReqWrap is used.
class fs_req_wrap {
 public:
  fs_req_wrap() {}
  ~fs_req_wrap() { uv_fs_req_cleanup(&req); }
  uv_fs_t req;

 private:
  DISALLOW_COPY_AND_ASSIGN(fs_req_wrap);
};

template <typename Func, typename... Args>
inline FSReqWrap* AsyncDestCall(Environment* env,
    const FunctionCallbackInfo<Value>& args,
    const char* syscall, const char* dest, size_t len,
    enum encoding enc, uv_fs_cb after, Func fn, Args... fn_args) {
  Local<Object> req = args[args.Length() - 1].As<Object>();
  FSReqWrap* req_wrap = Unwrap<FSReqWrap>(req);
  CHECK_NE(req_wrap, nullptr);
  req_wrap->Init(syscall, dest, len, enc);
  int err = fn(env->event_loop(), req_wrap->req(), fn_args..., after);
  req_wrap->Dispatched();
  if (err < 0) {
    uv_fs_t* uv_req = req_wrap->req();
    uv_req->result = err;
    uv_req->path = nullptr;
    after(uv_req);
    req_wrap = nullptr;
  }

  if (req_wrap != nullptr) {
    args.GetReturnValue().Set(req_wrap->persistent());
  }
  return req_wrap;
}

template <typename Func, typename... Args>
inline FSReqWrap* AsyncCall(Environment* env,
    const FunctionCallbackInfo<Value>& args,
    const char* syscall, enum encoding enc,
    uv_fs_cb after, Func fn, Args... fn_args) {
  return AsyncDestCall(env, args,
                       syscall, nullptr, 0, enc,
                       after, fn, fn_args...);
}

// Template counterpart of SYNC_CALL, except that it only puts
// the error number and the syscall in the context instead of
// creating an error in the C++ land.
// ctx must be checked using value->IsObject() before being passed.
template <typename Func, typename... Args>
inline int SyncCall(Environment* env, Local<Value> ctx, fs_req_wrap* req_wrap,
    const char* syscall, Func fn, Args... args) {
  env->PrintSyncTrace();
  int err = fn(env->event_loop(), &(req_wrap->req), args..., nullptr);
  if (err < 0) {
    Local<Context> context = env->context();
    Local<Object> ctx_obj = ctx.As<Object>();
    Isolate *isolate = env->isolate();
    ctx_obj->Set(context,
             env->errno_string(),
             Integer::New(isolate, err)).FromJust();
    ctx_obj->Set(context,
             env->syscall_string(),
             OneByteString(isolate, syscall)).FromJust();
  }
  return err;
}

#define SYNC_DEST_CALL(func, path, dest, ...)                                 \
  fs_req_wrap req_wrap;                                                       \
  env->PrintSyncTrace();                                                      \
  int err = uv_fs_ ## func(env->event_loop(),                                 \
                         &req_wrap.req,                                       \
                         __VA_ARGS__,                                         \
                         nullptr);                                            \
  if (err < 0) {                                                              \
    return env->ThrowUVException(err, #func, nullptr, path, dest);            \
  }                                                                           \

#define SYNC_CALL(func, path, ...)                                            \
  SYNC_DEST_CALL(func, path, nullptr, __VA_ARGS__)                            \

#define SYNC_REQ req_wrap.req

#define SYNC_RESULT err

void Access(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  const int argc = args.Length();
  CHECK_GE(argc, 2);

  CHECK(args[1]->IsInt32());
  int mode = args[1].As<Int32>()->Value();

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  if (args[2]->IsObject()) {  // access(path, mode, req)
    CHECK_EQ(argc, 3);
    AsyncCall(env, args, "access", UTF8, AfterNoArgs,
              uv_fs_access, *path, mode);
  } else {  // access(path, mode, undefined, ctx)
    CHECK_EQ(argc, 4);
    fs_req_wrap req_wrap;
    SyncCall(env, args[3], &req_wrap, "access", uv_fs_access, *path, mode);
  }
}


void Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();
  CHECK_GE(argc, 2);

  CHECK(args[0]->IsInt32());
  int fd = args[0].As<Int32>()->Value();

  if (args[1]->IsObject()) {  // close(fd, req)
    CHECK_EQ(argc, 2);
    AsyncCall(env, args, "close", UTF8, AfterNoArgs,
              uv_fs_close, fd);
  } else {  // close(fd, undefined, ctx)
    CHECK_EQ(argc, 3);
    fs_req_wrap req_wrap;
    SyncCall(env, args[2], &req_wrap, "close", uv_fs_close, fd);
  }
}


// Used to speed up module loading.  Returns the contents of the file as
// a string or undefined when the file cannot be opened.  Returns an empty
// string when the file does not contain the substring '"main"' because that
// is the property we care about.
static void InternalModuleReadJSON(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_loop_t* loop = env->event_loop();

  CHECK(args[0]->IsString());
  node::Utf8Value path(env->isolate(), args[0]);

  if (strlen(*path) != path.length())
    return;  // Contains a nul byte.

  uv_fs_t open_req;
  const int fd = uv_fs_open(loop, &open_req, *path, O_RDONLY, 0, nullptr);
  uv_fs_req_cleanup(&open_req);

  if (fd < 0) {
    return;
  }

  const size_t kBlockSize = 32 << 10;
  std::vector<char> chars;
  int64_t offset = 0;
  ssize_t numchars;
  do {
    const size_t start = chars.size();
    chars.resize(start + kBlockSize);

    uv_buf_t buf;
    buf.base = &chars[start];
    buf.len = kBlockSize;

    uv_fs_t read_req;
    numchars = uv_fs_read(loop, &read_req, fd, &buf, 1, offset, nullptr);
    uv_fs_req_cleanup(&read_req);

    CHECK_GE(numchars, 0);
    offset += numchars;
  } while (static_cast<size_t>(numchars) == kBlockSize);

  uv_fs_t close_req;
  CHECK_EQ(0, uv_fs_close(loop, &close_req, fd, nullptr));
  uv_fs_req_cleanup(&close_req);

  size_t start = 0;
  if (offset >= 3 && 0 == memcmp(&chars[0], "\xEF\xBB\xBF", 3)) {
    start = 3;  // Skip UTF-8 BOM.
  }

  const size_t size = offset - start;
  if (size == 0 || size == SearchString(&chars[start], size, "\"main\"")) {
    args.GetReturnValue().SetEmptyString();
  } else {
    Local<String> chars_string =
        String::NewFromUtf8(env->isolate(),
                            &chars[start],
                            String::kNormalString,
                            size);
    args.GetReturnValue().Set(chars_string);
  }
}

// Used to speed up module loading.  Returns 0 if the path refers to
// a file, 1 when it's a directory or < 0 on error (usually -ENOENT.)
// The speedup comes from not creating thousands of Stat and Error objects.
static void InternalModuleStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsString());
  node::Utf8Value path(env->isolate(), args[0]);

  uv_fs_t req;
  int rc = uv_fs_stat(env->event_loop(), &req, *path, nullptr);
  if (rc == 0) {
    const uv_stat_t* const s = static_cast<const uv_stat_t*>(req.ptr);
    rc = !!(s->st_mode & S_IFDIR);
  }
  uv_fs_req_cleanup(&req);

  args.GetReturnValue().Set(rc);
}

static void Stat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();
  CHECK_GE(argc, 1);

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  if (args[1]->IsObject()) {  // stat(path, req)
    CHECK_EQ(argc, 2);
    AsyncCall(env, args, "stat", UTF8, AfterStat,
              uv_fs_stat, *path);
  } else {  // stat(path, undefined, ctx)
    CHECK_EQ(argc, 3);
    fs_req_wrap req_wrap;
    int err = SyncCall(env, args[2], &req_wrap, "stat", uv_fs_stat, *path);
    if (err == 0) {
      FillStatsArray(env->fs_stats_field_array(),
                     static_cast<const uv_stat_t*>(req_wrap.req.ptr));
    }
  }
}

static void LStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();
  CHECK_GE(argc, 1);

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  if (args[1]->IsObject()) {  // lstat(path, req)
    CHECK_EQ(argc, 2);
    AsyncCall(env, args, "lstat", UTF8, AfterStat,
              uv_fs_lstat, *path);
  } else {  // lstat(path, undefined, ctx)
    CHECK_EQ(argc, 3);
    fs_req_wrap req_wrap;
    int err = SyncCall(env, args[2], &req_wrap, "lstat", uv_fs_lstat, *path);
    if (err == 0) {
      FillStatsArray(env->fs_stats_field_array(),
                     static_cast<const uv_stat_t*>(req_wrap.req.ptr));
    }
  }
}

static void FStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();
  CHECK_GE(argc, 1);

  CHECK(args[0]->IsInt32());
  int fd = args[0].As<Int32>()->Value();

  if (args[1]->IsObject()) {  // fstat(fd, req)
    CHECK_EQ(argc, 2);
    AsyncCall(env, args, "fstat", UTF8, AfterStat,
              uv_fs_fstat, fd);
  } else {  // fstat(fd, undefined, ctx)
    CHECK_EQ(argc, 3);
    fs_req_wrap req_wrap;
    int err = SyncCall(env, args[2], &req_wrap, "fstat", uv_fs_fstat, fd);
    if (err == 0) {
      FillStatsArray(env->fs_stats_field_array(),
                     static_cast<const uv_stat_t*>(req_wrap.req.ptr));
    }
  }
}

static void Symlink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 3);

  BufferValue target(env->isolate(), args[0]);
  CHECK_NE(*target, nullptr);
  BufferValue path(env->isolate(), args[1]);
  CHECK_NE(*path, nullptr);

  CHECK(args[2]->IsUint32());
  int flags = args[2]->Uint32Value(env->context()).ToChecked();

  if (args[3]->IsObject()) {  // symlink(target, path, flags, req)
    CHECK_EQ(args.Length(), 4);
    AsyncDestCall(env, args, "symlink", *path, path.length(), UTF8,
                  AfterNoArgs, uv_fs_symlink, *target, *path, flags);
  } else {  // symlink(target, path, flags)
    SYNC_DEST_CALL(symlink, *target, *path, *target, *path, flags)
  }
}

static void Link(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);

  BufferValue src(env->isolate(), args[0]);
  CHECK_NE(*src, nullptr);

  BufferValue dest(env->isolate(), args[1]);
  CHECK_NE(*dest, nullptr);

  if (args[2]->IsObject()) {  // link(src, dest, req)
    CHECK_EQ(args.Length(), 3);
    AsyncDestCall(env, args, "link", *dest, dest.length(), UTF8,
                  AfterNoArgs, uv_fs_link, *src, *dest);
  } else {  // link(src, dest)
    SYNC_DEST_CALL(link, *src, *dest, *src, *dest)
  }
}

static void ReadLink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();

  CHECK_GE(argc, 1);

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  if (args[2]->IsObject()) {  // readlink(path, encoding, req)
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "readlink", encoding, AfterStringPtr,
              uv_fs_readlink, *path);
  } else {
    SYNC_CALL(readlink, *path, *path)
    const char* link_path = static_cast<const char*>(SYNC_REQ.ptr);

    Local<Value> error;
    MaybeLocal<Value> rc = StringBytes::Encode(env->isolate(),
                                               link_path,
                                               encoding,
                                               &error);
    if (rc.IsEmpty()) {
      env->isolate()->ThrowException(error);
      return;
    }
    args.GetReturnValue().Set(rc.ToLocalChecked());
  }
}

static void Rename(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);

  BufferValue old_path(env->isolate(), args[0]);
  CHECK_NE(*old_path, nullptr);
  BufferValue new_path(env->isolate(), args[1]);
  CHECK_NE(*new_path, nullptr);

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncDestCall(env, args, "rename", *new_path, new_path.length(),
                  UTF8, AfterNoArgs, uv_fs_rename, *old_path, *new_path);
  } else {
    SYNC_DEST_CALL(rename, *old_path, *new_path, *old_path, *new_path)
  }
}

static void FTruncate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsNumber());

  int fd = args[0]->Int32Value();
  const int64_t len = args[1]->IntegerValue();

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "ftruncate", UTF8, AfterNoArgs,
              uv_fs_ftruncate, fd, len);
  } else {
    SYNC_CALL(ftruncate, 0, fd, len)
  }
}

static void Fdatasync(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    CHECK_EQ(args.Length(), 2);
    AsyncCall(env, args, "fdatasync", UTF8, AfterNoArgs,
              uv_fs_fdatasync, fd);
  } else {
    SYNC_CALL(fdatasync, 0, fd)
  }
}

static void Fsync(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    CHECK_EQ(args.Length(), 2);
    AsyncCall(env, args, "fsync", UTF8, AfterNoArgs,
              uv_fs_fsync, fd);
  } else {
    SYNC_CALL(fsync, 0, fd)
  }
}

static void Unlink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 1);

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  if (args[1]->IsObject()) {
    CHECK_EQ(args.Length(), 2);
    AsyncCall(env, args, "unlink", UTF8, AfterNoArgs,
              uv_fs_unlink, *path);
  } else {
    SYNC_CALL(unlink, *path, *path)
  }
}

static void RMDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 1);

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  if (args[1]->IsObject()) {
    CHECK_EQ(args.Length(), 2);
    AsyncCall(env, args, "rmdir", UTF8, AfterNoArgs,
              uv_fs_rmdir, *path);
  } else {
    SYNC_CALL(rmdir, *path, *path)
  }
}

static void MKDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);
  CHECK(args[1]->IsInt32());

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "mkdir", UTF8, AfterNoArgs,
              uv_fs_mkdir, *path, mode);
  } else {
    SYNC_CALL(mkdir, *path, *path, mode)
  }
}

static void RealPath(const FunctionCallbackInfo<Value>& args) {
  CHECK_GE(args.Length(), 2);
  Environment* env = Environment::GetCurrent(args);
  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "realpath", encoding, AfterStringPtr,
              uv_fs_realpath, *path);
  } else {
    SYNC_CALL(realpath, *path, *path);
    const char* link_path = static_cast<const char*>(SYNC_REQ.ptr);

    Local<Value> error;
    MaybeLocal<Value> rc = StringBytes::Encode(env->isolate(),
                                               link_path,
                                               encoding,
                                               &error);
    if (rc.IsEmpty()) {
      env->isolate()->ThrowException(error);
      return;
    }
    args.GetReturnValue().Set(rc.ToLocalChecked());
  }
}

static void ReadDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 1);

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "scandir", encoding, AfterScanDir,
              uv_fs_scandir, *path, 0 /*flags*/);
  } else {
    SYNC_CALL(scandir, *path, *path, 0 /*flags*/)

    CHECK_GE(SYNC_REQ.result, 0);
    int r;
    Local<Array> names = Array::New(env->isolate(), 0);
    Local<Function> fn = env->push_values_to_array_function();
    Local<Value> name_v[NODE_PUSH_VAL_TO_ARRAY_MAX];
    size_t name_idx = 0;

    for (int i = 0; ; i++) {
      uv_dirent_t ent;

      r = uv_fs_scandir_next(&SYNC_REQ, &ent);
      if (r == UV_EOF)
        break;
      if (r != 0)
        return env->ThrowUVException(r, "readdir", "", *path);

      Local<Value> error;
      MaybeLocal<Value> filename = StringBytes::Encode(env->isolate(),
                                                       ent.name,
                                                       encoding,
                                                       &error);
      if (filename.IsEmpty()) {
        env->isolate()->ThrowException(error);
        return;
      }

      name_v[name_idx++] = filename.ToLocalChecked();

      if (name_idx >= arraysize(name_v)) {
        fn->Call(env->context(), names, name_idx, name_v)
            .ToLocalChecked();
        name_idx = 0;
      }
    }

    if (name_idx > 0) {
      fn->Call(env->context(), names, name_idx, name_v).ToLocalChecked();
    }

    args.GetReturnValue().Set(names);
  }
}

static void Open(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 3);
  CHECK(args[1]->IsInt32());
  CHECK(args[2]->IsInt32());

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  int flags = args[1]->Int32Value();
  int mode = static_cast<int>(args[2]->Int32Value());

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "open", UTF8, AfterInteger,
              uv_fs_open, *path, flags, mode);
  } else {
    SYNC_CALL(open, *path, *path, flags, mode)
    args.GetReturnValue().Set(SYNC_RESULT);
  }
}


static void CopyFile(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 3);
  CHECK(args[2]->IsInt32());

  BufferValue src(env->isolate(), args[0]);
  CHECK_NE(*src, nullptr);
  BufferValue dest(env->isolate(), args[1]);
  CHECK_NE(*dest, nullptr);
  int flags = args[2]->Int32Value();

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "copyfile", UTF8, AfterNoArgs,
              uv_fs_copyfile, *src, *dest, flags);
  } else {
    SYNC_DEST_CALL(copyfile, *src, *dest, *src, *dest, flags)
  }
}


// Wrapper for write(2).
//
// bytesWritten = write(fd, buffer, offset, length, position, callback)
// 0 fd        integer. file descriptor
// 1 buffer    the data to write
// 2 offset    where in the buffer to start from
// 3 length    how much to write
// 4 position  if integer, position to write at in the file.
//             if null, write from the current position
static void WriteBuffer(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(Buffer::HasInstance(args[1]));

  int fd = args[0]->Int32Value();
  Local<Object> obj = args[1].As<Object>();
  const char* buf = Buffer::Data(obj);
  size_t buffer_length = Buffer::Length(obj);
  size_t off = args[2]->Uint32Value();
  size_t len = args[3]->Uint32Value();
  int64_t pos = GET_OFFSET(args[4]);

  CHECK_LE(off, buffer_length);
  CHECK_LE(len, buffer_length);
  CHECK_GE(off + len, off);
  CHECK(Buffer::IsWithinBounds(off, len, buffer_length));

  buf += off;

  uv_buf_t uvbuf = uv_buf_init(const_cast<char*>(buf), len);

  if (args[5]->IsObject()) {
    CHECK_EQ(args.Length(), 6);
    AsyncCall(env, args, "write", UTF8, AfterInteger,
              uv_fs_write, fd, &uvbuf, 1, pos);
    return;
  }

  SYNC_CALL(write, nullptr, fd, &uvbuf, 1, pos)
  args.GetReturnValue().Set(SYNC_RESULT);
}


// Wrapper for writev(2).
//
// bytesWritten = writev(fd, chunks, position, callback)
// 0 fd        integer. file descriptor
// 1 chunks    array of buffers to write
// 2 position  if integer, position to write at in the file.
//             if null, write from the current position
static void WriteBuffers(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsArray());

  int fd = args[0]->Int32Value();
  Local<Array> chunks = args[1].As<Array>();
  int64_t pos = GET_OFFSET(args[2]);

  MaybeStackBuffer<uv_buf_t> iovs(chunks->Length());

  for (uint32_t i = 0; i < iovs.length(); i++) {
    Local<Value> chunk = chunks->Get(i);
    CHECK(Buffer::HasInstance(chunk));
    iovs[i] = uv_buf_init(Buffer::Data(chunk), Buffer::Length(chunk));
  }

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "write", UTF8, AfterInteger,
              uv_fs_write, fd, *iovs, iovs.length(), pos);
    return;
  }

  SYNC_CALL(write, nullptr, fd, *iovs, iovs.length(), pos)
  args.GetReturnValue().Set(SYNC_RESULT);
}


// Wrapper for write(2).
//
// bytesWritten = write(fd, string, position, enc, callback)
// 0 fd        integer. file descriptor
// 1 string    non-buffer values are converted to strings
// 2 position  if integer, position to write at in the file.
//             if null, write from the current position
// 3 enc       encoding of string
static void WriteString(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());

  std::unique_ptr<char[]> delete_on_return;
  Local<Value> req;
  Local<Value> value = args[1];
  int fd = args[0]->Int32Value();
  char* buf = nullptr;
  size_t len;
  const int64_t pos = GET_OFFSET(args[2]);
  const auto enc = ParseEncoding(env->isolate(), args[3], UTF8);
  const auto is_async = args[4]->IsObject();

  // Avoid copying the string when it is externalized but only when:
  // 1. The target encoding is compatible with the string's encoding, and
  // 2. The write is synchronous, otherwise the string might get neutered
  //    while the request is in flight, and
  // 3. For UCS2, when the host system is little-endian.  Big-endian systems
  //    need to call StringBytes::Write() to ensure proper byte swapping.
  // The const_casts are conceptually sound: memory is read but not written.
  if (!is_async && value->IsString()) {
    auto string = value.As<String>();
    if ((enc == ASCII || enc == LATIN1) && string->IsExternalOneByte()) {
      auto ext = string->GetExternalOneByteStringResource();
      buf = const_cast<char*>(ext->data());
      len = ext->length();
    } else if (enc == UCS2 && IsLittleEndian() && string->IsExternal()) {
      auto ext = string->GetExternalStringResource();
      buf = reinterpret_cast<char*>(const_cast<uint16_t*>(ext->data()));
      len = ext->length() * sizeof(*ext->data());
    }
  }

  if (buf == nullptr) {
    len = StringBytes::StorageSize(env->isolate(), value, enc);
    buf = new char[len];
    // SYNC_CALL returns on error.  Make sure to always free the memory.
    if (!is_async) delete_on_return.reset(buf);
    // StorageSize may return too large a char, so correct the actual length
    // by the write size
    len = StringBytes::Write(env->isolate(), buf, len, args[1], enc);
  }

  uv_buf_t uvbuf = uv_buf_init(buf, len);

  if (is_async) {
    CHECK_EQ(args.Length(), 5);
    AsyncCall(env, args, "write", UTF8, AfterInteger,
              uv_fs_write, fd, &uvbuf, 1, pos);
  } else {
    SYNC_CALL(write, nullptr, fd, &uvbuf, 1, pos)
    return args.GetReturnValue().Set(SYNC_RESULT);
  }
}


/*
 * Wrapper for read(2).
 *
 * bytesRead = fs.read(fd, buffer, offset, length, position)
 *
 * 0 fd        integer. file descriptor
 * 1 buffer    instance of Buffer
 * 2 offset    integer. offset to start reading into inside buffer
 * 3 length    integer. length to read
 * 4 position  file position - null for current position
 *
 */
static void Read(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(Buffer::HasInstance(args[1]));

  int fd = args[0]->Int32Value();

  Local<Value> req;

  size_t len;
  int64_t pos;

  char * buf = nullptr;

  Local<Object> buffer_obj = args[1].As<Object>();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[2]->Int32Value();
  CHECK_LT(off, buffer_length);

  len = args[3]->Int32Value();
  CHECK(Buffer::IsWithinBounds(off, len, buffer_length));

  pos = GET_OFFSET(args[4]);

  buf = buffer_data + off;

  uv_buf_t uvbuf = uv_buf_init(const_cast<char*>(buf), len);

  if (args[5]->IsObject()) {
    CHECK_EQ(args.Length(), 6);
    AsyncCall(env, args, "read", UTF8, AfterInteger,
              uv_fs_read, fd, &uvbuf, 1, pos);
  } else {
    SYNC_CALL(read, 0, fd, &uvbuf, 1, pos)
    args.GetReturnValue().Set(SYNC_RESULT);
  }
}


/* fs.chmod(path, mode);
 * Wrapper for chmod(1) / EIO_CHMOD
 */
static void Chmod(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);
  CHECK(args[1]->IsInt32());

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "chmod", UTF8, AfterNoArgs,
              uv_fs_chmod, *path, mode);
  } else {
    SYNC_CALL(chmod, *path, *path, mode);
  }
}


/* fs.fchmod(fd, mode);
 * Wrapper for fchmod(1) / EIO_FCHMOD
 */
static void FChmod(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsInt32());

  int fd = args[0]->Int32Value();
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "fchmod", UTF8, AfterNoArgs,
              uv_fs_fchmod, fd, mode);
  } else {
    SYNC_CALL(fchmod, 0, fd, mode);
  }
}


/* fs.chown(path, uid, gid);
 * Wrapper for chown(1) / EIO_CHOWN
 */
static void Chown(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int len = args.Length();
  CHECK_GE(len, 3);
  CHECK(args[1]->IsUint32());
  CHECK(args[2]->IsUint32());

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  uv_uid_t uid = static_cast<uv_uid_t>(args[1]->Uint32Value());
  uv_gid_t gid = static_cast<uv_gid_t>(args[2]->Uint32Value());

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "chown", UTF8, AfterNoArgs,
              uv_fs_chown, *path, uid, gid);
  } else {
    SYNC_CALL(chown, *path, *path, uid, gid);
  }
}


/* fs.fchown(fd, uid, gid);
 * Wrapper for fchown(1) / EIO_FCHOWN
 */
static void FChown(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsUint32());
  CHECK(args[2]->IsUint32());

  int fd = args[0]->Int32Value();
  uv_uid_t uid = static_cast<uv_uid_t>(args[1]->Uint32Value());
  uv_gid_t gid = static_cast<uv_gid_t>(args[2]->Uint32Value());

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "fchown", UTF8, AfterNoArgs,
              uv_fs_fchown, fd, uid, gid);
  } else {
    SYNC_CALL(fchown, 0, fd, uid, gid);
  }
}


static void UTimes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 3);
  CHECK(args[1]->IsNumber());
  CHECK(args[2]->IsNumber());

  BufferValue path(env->isolate(), args[0]);
  CHECK_NE(*path, nullptr);

  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "utime", UTF8, AfterNoArgs,
              uv_fs_utime, *path, atime, mtime);
  } else {
    SYNC_CALL(utime, *path, *path, atime, mtime);
  }
}

static void FUTimes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsInt32());
  CHECK(args[1]->IsNumber());
  CHECK(args[2]->IsNumber());

  const int fd = args[0]->Int32Value();
  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsObject()) {
    CHECK_EQ(args.Length(), 4);
    AsyncCall(env, args, "futime", UTF8, AfterNoArgs,
              uv_fs_futime, fd, atime, mtime);
  } else {
    SYNC_CALL(futime, 0, fd, atime, mtime);
  }
}

static void Mkdtemp(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);

  BufferValue tmpl(env->isolate(), args[0]);
  CHECK_NE(*tmpl, nullptr);

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  if (args[2]->IsObject()) {
    CHECK_EQ(args.Length(), 3);
    AsyncCall(env, args, "mkdtemp", encoding, AfterStringPath,
              uv_fs_mkdtemp, *tmpl);
  } else {
    SYNC_CALL(mkdtemp, *tmpl, *tmpl);
    const char* path = static_cast<const char*>(SYNC_REQ.path);

    Local<Value> error;
    MaybeLocal<Value> rc =
        StringBytes::Encode(env->isolate(), path, encoding, &error);
    if (rc.IsEmpty()) {
      env->isolate()->ThrowException(error);
      return;
    }
    args.GetReturnValue().Set(rc.ToLocalChecked());
  }
}

void InitFs(Local<Object> target,
            Local<Value> unused,
            Local<Context> context,
            void* priv) {
  Environment* env = Environment::GetCurrent(context);

  env->SetMethod(target, "access", Access);
  env->SetMethod(target, "close", Close);
  env->SetMethod(target, "open", Open);
  env->SetMethod(target, "read", Read);
  env->SetMethod(target, "fdatasync", Fdatasync);
  env->SetMethod(target, "fsync", Fsync);
  env->SetMethod(target, "rename", Rename);
  env->SetMethod(target, "ftruncate", FTruncate);
  env->SetMethod(target, "rmdir", RMDir);
  env->SetMethod(target, "mkdir", MKDir);
  env->SetMethod(target, "readdir", ReadDir);
  env->SetMethod(target, "internalModuleReadJSON", InternalModuleReadJSON);
  env->SetMethod(target, "internalModuleStat", InternalModuleStat);
  env->SetMethod(target, "stat", Stat);
  env->SetMethod(target, "lstat", LStat);
  env->SetMethod(target, "fstat", FStat);
  env->SetMethod(target, "link", Link);
  env->SetMethod(target, "symlink", Symlink);
  env->SetMethod(target, "readlink", ReadLink);
  env->SetMethod(target, "unlink", Unlink);
  env->SetMethod(target, "writeBuffer", WriteBuffer);
  env->SetMethod(target, "writeBuffers", WriteBuffers);
  env->SetMethod(target, "writeString", WriteString);
  env->SetMethod(target, "realpath", RealPath);
  env->SetMethod(target, "copyFile", CopyFile);

  env->SetMethod(target, "chmod", Chmod);
  env->SetMethod(target, "fchmod", FChmod);
  // env->SetMethod(target, "lchmod", LChmod);

  env->SetMethod(target, "chown", Chown);
  env->SetMethod(target, "fchown", FChown);
  // env->SetMethod(target, "lchown", LChown);

  env->SetMethod(target, "utimes", UTimes);
  env->SetMethod(target, "futimes", FUTimes);

  env->SetMethod(target, "mkdtemp", Mkdtemp);

  target->Set(context,
              FIXED_ONE_BYTE_STRING(env->isolate(), "statValues"),
              env->fs_stats_field_array()->GetJSArray()).FromJust();

  StatWatcher::Initialize(env, target);

  // Create FunctionTemplate for FSReqWrap
  Local<FunctionTemplate> fst =
      FunctionTemplate::New(env->isolate(), NewFSReqWrap);
  fst->InstanceTemplate()->SetInternalFieldCount(1);
  AsyncWrap::AddWrapMethods(env, fst);
  Local<String> wrapString =
      FIXED_ONE_BYTE_STRING(env->isolate(), "FSReqWrap");
  fst->SetClassName(wrapString);
  target->Set(context, wrapString, fst->GetFunction()).FromJust();
}

}  // namespace fs

}  // end namespace node

NODE_BUILTIN_MODULE_CONTEXT_AWARE(fs, node::fs::InitFs)
