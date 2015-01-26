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

#include "node.h"
#include "node_file.h"
#include "node_buffer.h"
#include "node_internals.h"
#include "node_stat_watcher.h"

#include "env.h"
#include "env-inl.h"
#include "req_wrap.h"
#include "string_bytes.h"
#include "util.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#if defined(__MINGW32__) || defined(_MSC_VER)
# include <io.h>
#endif

namespace node {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

#define MIN(a, b) ((a) < (b) ? (a) : (b))

#define TYPE_ERROR(msg) env->ThrowTypeError(msg)

#define THROW_BAD_ARGS TYPE_ERROR("Bad argument")

class FSReqWrap: public ReqWrap<uv_fs_t> {
 public:
  void* operator new(size_t size) { return new char[size]; }
  void* operator new(size_t size, char* storage) { return storage; }

  FSReqWrap(Environment* env,
            Local<Object> req,
            const char* syscall,
            char* data = NULL)
    : ReqWrap<uv_fs_t>(env, req, AsyncWrap::PROVIDER_FSREQWRAP),
      syscall_(syscall),
      data_(data),
      dest_len_(0) {
    Wrap(object(), this);
  }

  void ReleaseEarly() {
    if (data_ == NULL)
      return;
    delete[] data_;
    data_ = NULL;
  }

  inline const char* syscall() const { return syscall_; }
  inline const char* dest() const { return dest_; }
  inline unsigned int dest_len() const { return dest_len_; }
  inline void dest_len(unsigned int dest_len) { dest_len_ = dest_len; }

 private:
  const char* syscall_;
  char* data_;
  unsigned int dest_len_;
  char dest_[1];
};


static void NewFSReqWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


#define ASSERT_OFFSET(a) \
  if (!(a)->IsUndefined() && !(a)->IsNull() && !IsInt64((a)->NumberValue())) { \
    return env->ThrowTypeError("Not an integer"); \
  }
#define ASSERT_TRUNCATE_LENGTH(a) \
  if (!(a)->IsUndefined() && !(a)->IsNull() && !IsInt64((a)->NumberValue())) { \
    return env->ThrowTypeError("Not an integer"); \
  }
#define GET_OFFSET(a) ((a)->IsNumber() ? (a)->IntegerValue() : -1)
#define GET_TRUNCATE_LENGTH(a) ((a)->IntegerValue())

static inline bool IsInt64(double x) {
  return x == static_cast<double>(static_cast<int64_t>(x));
}


static void After(uv_fs_t *req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  assert(&req_wrap->req_ == req);
  req_wrap->ReleaseEarly();  // Free memory that's no longer used now.

  Environment* env = req_wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // there is always at least one argument. "error"
  int argc = 1;

  // Allocate space for two args. We may only use one depending on the case.
  // (Feel free to increase this if you need more)
  Local<Value> argv[2];

  if (req->result < 0) {
    // If the request doesn't have a path parameter set.
    if (req->path == NULL) {
      argv[0] = UVException(req->result, NULL, req_wrap->syscall());
    } else if ((req->result == UV_EEXIST ||
                req->result == UV_ENOTEMPTY ||
                req->result == UV_EPERM) &&
               req_wrap->dest_len() > 0) {
      argv[0] = UVException(req->result,
                            NULL,
                            req_wrap->syscall(),
                            req_wrap->dest());
    } else {
      argv[0] = UVException(req->result,
                            NULL,
                            req_wrap->syscall(),
                            static_cast<const char*>(req->path));
    }
  } else {
    // error value is empty or null for non-error.
    argv[0] = Null(env->isolate());

    // All have at least two args now.
    argc = 2;

    switch (req->fs_type) {
      // These all have no data to pass.
      case UV_FS_CLOSE:
      case UV_FS_RENAME:
      case UV_FS_UNLINK:
      case UV_FS_RMDIR:
      case UV_FS_MKDIR:
      case UV_FS_FTRUNCATE:
      case UV_FS_FSYNC:
      case UV_FS_FDATASYNC:
      case UV_FS_LINK:
      case UV_FS_SYMLINK:
      case UV_FS_CHMOD:
      case UV_FS_FCHMOD:
      case UV_FS_CHOWN:
      case UV_FS_FCHOWN:
        // These, however, don't.
        argc = 1;
        break;

      case UV_FS_ACCESS:
        argv[1] = Integer::New(env->isolate(), req->result);
        break;

      case UV_FS_UTIME:
      case UV_FS_FUTIME:
        argc = 0;
        break;

      case UV_FS_OPEN:
        argv[1] = Integer::New(env->isolate(), req->result);
        break;

      case UV_FS_WRITE:
        argv[1] = Integer::New(env->isolate(), req->result);
        break;

      case UV_FS_STAT:
      case UV_FS_LSTAT:
      case UV_FS_FSTAT:
        argv[1] = BuildStatsObject(env,
                                   static_cast<const uv_stat_t*>(req->ptr));
        break;

      case UV_FS_READLINK:
        argv[1] = String::NewFromUtf8(env->isolate(),
                                      static_cast<const char*>(req->ptr));
        break;

      case UV_FS_READ:
        // Buffer interface
        argv[1] = Integer::New(env->isolate(), req->result);
        break;

      case UV_FS_SCANDIR:
        {
          int r;
          Local<Array> names = Array::New(env->isolate(), 0);

          for (int i = 0; ; i++) {
            uv_dirent_t ent;

            r = uv_fs_scandir_next(req, &ent);
            if (r == UV_EOF)
              break;
            if (r != 0) {
              argv[0] = UVException(r,
                                    NULL,
                                    req_wrap->syscall(),
                                    static_cast<const char*>(req->path));
              break;
            }

            Local<String> name = String::NewFromUtf8(env->isolate(),
                                                     ent.name);
            names->Set(i, name);
          }

          argv[1] = names;
        }
        break;

      default:
        assert(0 && "Unhandled eio response");
    }
  }

  req_wrap->MakeCallback(env->oncomplete_string(), argc, argv);

  uv_fs_req_cleanup(&req_wrap->req_);
  delete req_wrap;
}

// This struct is only used on sync fs calls.
// For async calls FSReqWrap is used.
struct fs_req_wrap {
  fs_req_wrap() {}
  ~fs_req_wrap() { uv_fs_req_cleanup(&req); }
  // Ensure that copy ctor and assignment operator are not used.
  fs_req_wrap(const fs_req_wrap& req);
  fs_req_wrap& operator=(const fs_req_wrap& req);
  uv_fs_t req;
};


#define ASYNC_DEST_CALL(func, req, dest_path, ...)                            \
  Environment* env = Environment::GetCurrent(args.GetIsolate());              \
  FSReqWrap* req_wrap;                                                        \
  char* dest_str = (dest_path);                                               \
  int dest_len = dest_str == NULL ? 0 : strlen(dest_str);                     \
  char* storage = new char[sizeof(*req_wrap) + dest_len];                     \
  CHECK(req->IsObject());                                                     \
  req_wrap = new(storage) FSReqWrap(env, req.As<Object>(), #func);            \
  req_wrap->dest_len(dest_len);                                               \
  if (dest_str != NULL) {                                                     \
    memcpy(const_cast<char*>(req_wrap->dest()),                               \
           dest_str,                                                          \
           dest_len + 1);                                                     \
  }                                                                           \
  int err = uv_fs_ ## func(env->event_loop(),                                 \
                           &req_wrap->req_,                                   \
                           __VA_ARGS__,                                       \
                           After);                                            \
  req_wrap->Dispatched();                                                     \
  if (err < 0) {                                                              \
    uv_fs_t* uv_req = &req_wrap->req_;                                        \
    uv_req->result = err;                                                     \
    uv_req->path = NULL;                                                      \
    After(uv_req);                                                            \
  }                                                                           \
  args.GetReturnValue().Set(req_wrap->persistent());

#define ASYNC_CALL(func, req, ...)                                            \
  ASYNC_DEST_CALL(func, req, NULL, __VA_ARGS__)                               \

#define SYNC_DEST_CALL(func, path, dest, ...)                                 \
  fs_req_wrap req_wrap;                                                       \
  int err = uv_fs_ ## func(env->event_loop(),                                 \
                         &req_wrap.req,                                       \
                         __VA_ARGS__,                                         \
                         NULL);                                               \
  if (err < 0) {                                                              \
    if (dest != NULL &&                                                       \
        (err == UV_EEXIST ||                                                  \
         err == UV_ENOTEMPTY ||                                               \
         err == UV_EPERM)) {                                                  \
      return env->ThrowUVException(err, #func, "", dest);                     \
    } else {                                                                  \
      return env->ThrowUVException(err, #func, "", path);                     \
    }                                                                         \
  }                                                                           \

#define SYNC_CALL(func, path, ...)                                            \
  SYNC_DEST_CALL(func, path, NULL, __VA_ARGS__)                               \

#define SYNC_REQ req_wrap.req

#define SYNC_RESULT err


static void Access(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2)
    return THROW_BAD_ARGS;
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("mode must be an integer");

  node::Utf8Value path(args[0]);
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(access, args[2], *path, mode);
  } else {
    SYNC_CALL(access, *path, *path, mode);
    args.GetReturnValue().Set(SYNC_RESULT);
  }
}


static void Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(close, args[1], fd)
  } else {
    SYNC_CALL(close, 0, fd)
  }
}


Local<Value> BuildStatsObject(Environment* env, const uv_stat_t* s) {
  // If you hit this assertion, you forgot to enter the v8::Context first.
  assert(env->context() == env->isolate()->GetCurrentContext());

  EscapableHandleScope handle_scope(env->isolate());

  // The code below is very nasty-looking but it prevents a segmentation fault
  // when people run JS code like the snippet below. It's apparently more
  // common than you would expect, several people have reported this crash...
  //
  //   function crash() {
  //     fs.statSync('.');
  //     crash();
  //   }
  //
  // We need to check the return value of Integer::New() and Date::New()
  // and make sure that we bail out when V8 returns an empty handle.

  // Integers.
#define X(name)                                                               \
  Local<Value> name = Integer::New(env->isolate(), s->st_##name);             \
  if (name.IsEmpty())                                                         \
    return handle_scope.Escape(Local<Object>());                              \

  X(dev)
  X(mode)
  X(nlink)
  X(uid)
  X(gid)
  X(rdev)
# if defined(__POSIX__)
  X(blksize)
# else
  Local<Value> blksize = Undefined(env->isolate());
# endif
#undef X

  // Numbers.
#define X(name)                                                               \
  Local<Value> name = Number::New(env->isolate(),                             \
                                  static_cast<double>(s->st_##name));         \
  if (name.IsEmpty())                                                         \
    return handle_scope.Escape(Local<Object>());                              \

  X(ino)
  X(size)
# if defined(__POSIX__)
  X(blocks)
# else
  Local<Value> blocks = Undefined(env->isolate());
# endif
#undef X

  // Dates.
#define X(name)                                                               \
  Local<Value> name##_msec =                                                  \
    Number::New(env->isolate(),                                               \
        (static_cast<double>(s->st_##name.tv_sec) * 1000) +                   \
        (static_cast<double>(s->st_##name.tv_nsec / 1000000)));               \
                                                                              \
  if (name##_msec.IsEmpty())                                                  \
    return handle_scope.Escape(Local<Object>());                              \

  X(atim)
  X(mtim)
  X(ctim)
  X(birthtim)
#undef X

  // Pass stats as the first argument, this is the object we are modifying.
  Local<Value> argv[] = {
    dev,
    mode,
    nlink,
    uid,
    gid,
    rdev,
    blksize,
    ino,
    size,
    blocks,
    atim_msec,
    mtim_msec,
    ctim_msec,
    birthtim_msec
  };

  // Call out to JavaScript to create the stats object.
  Local<Value> stats =
    env->fs_stats_constructor_function()->NewInstance(ARRAY_SIZE(argv), argv);

  if (stats.IsEmpty())
    return handle_scope.Escape(Local<Object>());

  return handle_scope.Escape(stats);
}

static void Stat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  node::Utf8Value path(args[0]);

  if (args[1]->IsObject()) {
    ASYNC_CALL(stat, args[1], *path)
  } else {
    SYNC_CALL(stat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void LStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  node::Utf8Value path(args[0]);

  if (args[1]->IsObject()) {
    ASYNC_CALL(lstat, args[1], *path)
  } else {
    SYNC_CALL(lstat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void FStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(fstat, args[1], fd)
  } else {
    SYNC_CALL(fstat, 0, fd)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void Symlink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("dest path required");
  if (len < 2)
    return TYPE_ERROR("src path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("dest path must be a string");
  if (!args[1]->IsString())
    return TYPE_ERROR("src path must be a string");

  node::Utf8Value dest(args[0]);
  node::Utf8Value path(args[1]);
  int flags = 0;

  if (args[2]->IsString()) {
    node::Utf8Value mode(args[2]);
    if (strcmp(*mode, "dir") == 0) {
      flags |= UV_FS_SYMLINK_DIR;
    } else if (strcmp(*mode, "junction") == 0) {
      flags |= UV_FS_SYMLINK_JUNCTION;
    } else if (strcmp(*mode, "file") != 0) {
      return env->ThrowError("Unknown symlink type");
    }
  }

  if (args[3]->IsObject()) {
    ASYNC_DEST_CALL(symlink, args[3], *path, *dest, *path, flags)
  } else {
    SYNC_DEST_CALL(symlink, *dest, *path, *dest, *path, flags)
  }
}

static void Link(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("dest path required");
  if (len < 2)
    return TYPE_ERROR("src path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("dest path must be a string");
  if (!args[1]->IsString())
    return TYPE_ERROR("src path must be a string");

  node::Utf8Value orig_path(args[0]);
  node::Utf8Value new_path(args[1]);

  if (args[2]->IsObject()) {
    ASYNC_DEST_CALL(link, args[2], *new_path, *orig_path, *new_path)
  } else {
    SYNC_DEST_CALL(link, *orig_path, *new_path, *orig_path, *new_path)
  }
}

static void ReadLink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  node::Utf8Value path(args[0]);

  if (args[1]->IsObject()) {
    ASYNC_CALL(readlink, args[1], *path)
  } else {
    SYNC_CALL(readlink, *path, *path)
    const char* link_path = static_cast<const char*>(SYNC_REQ.ptr);
    Local<String> rc = String::NewFromUtf8(env->isolate(), link_path);
    args.GetReturnValue().Set(rc);
  }
}

static void Rename(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("old path required");
  if (len < 2)
    return TYPE_ERROR("new path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("old path must be a string");
  if (!args[1]->IsString())
    return TYPE_ERROR("new path must be a string");

  node::Utf8Value old_path(args[0]);
  node::Utf8Value new_path(args[1]);

  if (args[2]->IsObject()) {
    ASYNC_DEST_CALL(rename, args[2], *new_path, *old_path, *new_path)
  } else {
    SYNC_DEST_CALL(rename, *old_path, *new_path, *old_path, *new_path)
  }
}

static void FTruncate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  ASSERT_TRUNCATE_LENGTH(args[1]);
  int64_t len = GET_TRUNCATE_LENGTH(args[1]);

  if (args[2]->IsObject()) {
    ASYNC_CALL(ftruncate, args[2], fd, len)
  } else {
    SYNC_CALL(ftruncate, 0, fd, len)
  }
}

static void Fdatasync(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(fdatasync, args[1], fd)
  } else {
    SYNC_CALL(fdatasync, 0, fd)
  }
}

static void Fsync(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(fsync, args[1], fd)
  } else {
    SYNC_CALL(fsync, 0, fd)
  }
}

static void Unlink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  node::Utf8Value path(args[0]);

  if (args[1]->IsObject()) {
    ASYNC_CALL(unlink, args[1], *path)
  } else {
    SYNC_CALL(unlink, *path, *path)
  }
}

static void RMDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  node::Utf8Value path(args[0]);

  if (args[1]->IsObject()) {
    ASYNC_CALL(rmdir, args[1], *path)
  } else {
    SYNC_CALL(rmdir, *path, *path)
  }
}

static void MKDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  node::Utf8Value path(args[0]);
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(mkdir, args[2], *path, mode)
  } else {
    SYNC_CALL(mkdir, *path, *path, mode)
  }
}

static void ReadDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  node::Utf8Value path(args[0]);

  if (args[1]->IsObject()) {
    ASYNC_CALL(scandir, args[1], *path, 0 /*flags*/)
  } else {
    SYNC_CALL(scandir, *path, *path, 0 /*flags*/)

    assert(SYNC_REQ.result >= 0);
    int r;
    Local<Array> names = Array::New(env->isolate(), 0);

    for (int i = 0; ; i++) {
      uv_dirent_t ent;

      r = uv_fs_scandir_next(&SYNC_REQ, &ent);
      if (r == UV_EOF)
        break;
      if (r != 0)
        return env->ThrowUVException(r, "readdir", "", *path);

      Local<String> name = String::NewFromUtf8(env->isolate(),
                                               ent.name);
      names->Set(i, name);
    }

    args.GetReturnValue().Set(names);
  }
}

static void Open(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("path required");
  if (len < 2)
    return TYPE_ERROR("flags required");
  if (len < 3)
    return TYPE_ERROR("mode required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("flags must be an int");
  if (!args[2]->IsInt32())
    return TYPE_ERROR("mode must be an int");

  node::Utf8Value path(args[0]);
  int flags = args[1]->Int32Value();
  int mode = static_cast<int>(args[2]->Int32Value());

  if (args[3]->IsObject()) {
    ASYNC_CALL(open, args[3], *path, flags, mode)
  } else {
    SYNC_CALL(open, *path, *path, flags, mode)
    args.GetReturnValue().Set(SYNC_RESULT);
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  assert(args[0]->IsInt32());
  assert(Buffer::HasInstance(args[1]));

  int fd = args[0]->Int32Value();
  Local<Object> obj = args[1].As<Object>();
  const char* buf = Buffer::Data(obj);
  size_t buffer_length = Buffer::Length(obj);
  size_t off = args[2]->Uint32Value();
  size_t len = args[3]->Uint32Value();
  int64_t pos = GET_OFFSET(args[4]);
  Local<Value> req = args[5];

  if (off > buffer_length)
    return env->ThrowRangeError("offset out of bounds");
  if (len > buffer_length)
    return env->ThrowRangeError("length out of bounds");
  if (off + len < off)
    return env->ThrowRangeError("off + len overflow");
  if (!Buffer::IsWithinBounds(off, len, buffer_length))
    return env->ThrowRangeError("off + len > buffer.length");

  buf += off;

  uv_buf_t uvbuf = uv_buf_init(const_cast<char*>(buf), len);

  if (req->IsObject()) {
    ASYNC_CALL(write, req, fd, &uvbuf, 1, pos)
    return;
  }

  SYNC_CALL(write, NULL, fd, &uvbuf, 1, pos)
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
  HandleScope handle_scope(args.GetIsolate());
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  if (!args[0]->IsInt32())
    return env->ThrowTypeError("First argument must be file descriptor");

  Local<Value> req;
  Local<Value> string = args[1];
  int fd = args[0]->Int32Value();
  char* buf = NULL;
  int64_t pos;
  size_t len;
  bool must_free = false;

  // will assign buf and len if string was external
  if (!StringBytes::GetExternalParts(env->isolate(),
                                     string,
                                     const_cast<const char**>(&buf),
                                     &len)) {
    enum encoding enc = ParseEncoding(env->isolate(), args[3], UTF8);
    len = StringBytes::StorageSize(env->isolate(), string, enc);
    buf = new char[len];
    // StorageSize may return too large a char, so correct the actual length
    // by the write size
    len = StringBytes::Write(env->isolate(), buf, len, args[1], enc);
    must_free = true;
  }
  pos = GET_OFFSET(args[2]);
  req = args[4];

  uv_buf_t uvbuf = uv_buf_init(const_cast<char*>(buf), len);

  if (!req->IsObject()) {
    SYNC_CALL(write, NULL, fd, &uvbuf, 1, pos)
    if (must_free)
      delete[] buf;
    return args.GetReturnValue().Set(SYNC_RESULT);
  }

  FSReqWrap* req_wrap =
      new FSReqWrap(env, req.As<Object>(), "write", must_free ? buf : NULL);
  int err = uv_fs_write(env->event_loop(),
                        &req_wrap->req_,
                        fd,
                        &uvbuf,
                        1,
                        pos,
                        After);
  req_wrap->Dispatched();
  if (err < 0) {
    uv_fs_t* req = &req_wrap->req_;
    req->result = err;
    req->path = NULL;
    After(req);
  }

  return args.GetReturnValue().Set(req_wrap->persistent());
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  Local<Value> req;

  size_t len;
  int64_t pos;

  char * buf = NULL;

  if (!Buffer::HasInstance(args[1])) {
    return env->ThrowError("Second argument needs to be a buffer");
  }

  Local<Object> buffer_obj = args[1]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[2]->Int32Value();
  if (off >= buffer_length) {
    return env->ThrowError("Offset is out of bounds");
  }

  len = args[3]->Int32Value();
  if (!Buffer::IsWithinBounds(off, len, buffer_length))
    return env->ThrowRangeError("Length extends beyond buffer");

  pos = GET_OFFSET(args[4]);

  buf = buffer_data + off;

  uv_buf_t uvbuf = uv_buf_init(const_cast<char*>(buf), len);

  req = args[5];

  if (req->IsObject()) {
    ASYNC_CALL(read, req, fd, &uvbuf, 1, pos);
  } else {
    SYNC_CALL(read, 0, fd, &uvbuf, 1, pos)
    args.GetReturnValue().Set(SYNC_RESULT);
  }
}


/* fs.chmod(path, mode);
 * Wrapper for chmod(1) / EIO_CHMOD
 */
static void Chmod(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  node::Utf8Value path(args[0]);
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(chmod, args[2], *path, mode);
  } else {
    SYNC_CALL(chmod, *path, *path, mode);
  }
}


/* fs.fchmod(fd, mode);
 * Wrapper for fchmod(1) / EIO_FCHMOD
 */
static void FChmod(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  int fd = args[0]->Int32Value();
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(fchmod, args[2], fd, mode);
  } else {
    SYNC_CALL(fchmod, 0, fd, mode);
  }
}


/* fs.chown(path, uid, gid);
 * Wrapper for chown(1) / EIO_CHOWN
 */
static void Chown(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("path required");
  if (len < 2)
    return TYPE_ERROR("uid required");
  if (len < 3)
    return TYPE_ERROR("gid required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");
  if (!args[1]->IsUint32())
    return TYPE_ERROR("uid must be an unsigned int");
  if (!args[2]->IsUint32())
    return TYPE_ERROR("gid must be an unsigned int");

  node::Utf8Value path(args[0]);
  uv_uid_t uid = static_cast<uv_uid_t>(args[1]->Uint32Value());
  uv_gid_t gid = static_cast<uv_gid_t>(args[2]->Uint32Value());

  if (args[3]->IsObject()) {
    ASYNC_CALL(chown, args[3], *path, uid, gid);
  } else {
    SYNC_CALL(chown, *path, *path, uid, gid);
  }
}


/* fs.fchown(fd, uid, gid);
 * Wrapper for fchown(1) / EIO_FCHOWN
 */
static void FChown(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("fd required");
  if (len < 2)
    return TYPE_ERROR("uid required");
  if (len < 3)
    return TYPE_ERROR("gid required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be an int");
  if (!args[1]->IsUint32())
    return TYPE_ERROR("uid must be an unsigned int");
  if (!args[2]->IsUint32())
    return TYPE_ERROR("gid must be an unsigned int");

  int fd = args[0]->Int32Value();
  uv_uid_t uid = static_cast<uv_uid_t>(args[1]->Uint32Value());
  uv_gid_t gid = static_cast<uv_gid_t>(args[2]->Uint32Value());

  if (args[3]->IsObject()) {
    ASYNC_CALL(fchown, args[3], fd, uid, gid);
  } else {
    SYNC_CALL(fchown, 0, fd, uid, gid);
  }
}


static void UTimes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("path required");
  if (len < 2)
    return TYPE_ERROR("atime required");
  if (len < 3)
    return TYPE_ERROR("mtime required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");
  if (!args[1]->IsNumber())
    return TYPE_ERROR("atime must be a number");
  if (!args[2]->IsNumber())
    return TYPE_ERROR("mtime must be a number");

  const node::Utf8Value path(args[0]);
  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsObject()) {
    ASYNC_CALL(utime, args[3], *path, atime, mtime);
  } else {
    SYNC_CALL(utime, *path, *path, atime, mtime);
  }
}

static void FUTimes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("fd required");
  if (len < 2)
    return TYPE_ERROR("atime required");
  if (len < 3)
    return TYPE_ERROR("mtime required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be an int");
  if (!args[1]->IsNumber())
    return TYPE_ERROR("atime must be a number");
  if (!args[2]->IsNumber())
    return TYPE_ERROR("mtime must be a number");

  const int fd = args[0]->Int32Value();
  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsObject()) {
    ASYNC_CALL(futime, args[3], fd, atime, mtime);
  } else {
    SYNC_CALL(futime, 0, fd, atime, mtime);
  }
}

void FSInitialize(const FunctionCallbackInfo<Value>& args) {
  Local<Function> stats_constructor = args[0].As<Function>();
  assert(stats_constructor->IsFunction());

  Environment* env = Environment::GetCurrent(args.GetIsolate());
  env->set_fs_stats_constructor_function(stats_constructor);
}

void InitFs(Handle<Object> target,
            Handle<Value> unused,
            Handle<Context> context,
            void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // Function which creates a new Stats object.
  target->Set(
      FIXED_ONE_BYTE_STRING(env->isolate(), "FSInitialize"),
      FunctionTemplate::New(env->isolate(), FSInitialize)->GetFunction());

  NODE_SET_METHOD(target, "access", Access);
  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "open", Open);
  NODE_SET_METHOD(target, "read", Read);
  NODE_SET_METHOD(target, "fdatasync", Fdatasync);
  NODE_SET_METHOD(target, "fsync", Fsync);
  NODE_SET_METHOD(target, "rename", Rename);
  NODE_SET_METHOD(target, "ftruncate", FTruncate);
  NODE_SET_METHOD(target, "rmdir", RMDir);
  NODE_SET_METHOD(target, "mkdir", MKDir);
  NODE_SET_METHOD(target, "readdir", ReadDir);
  NODE_SET_METHOD(target, "stat", Stat);
  NODE_SET_METHOD(target, "lstat", LStat);
  NODE_SET_METHOD(target, "fstat", FStat);
  NODE_SET_METHOD(target, "link", Link);
  NODE_SET_METHOD(target, "symlink", Symlink);
  NODE_SET_METHOD(target, "readlink", ReadLink);
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "writeBuffer", WriteBuffer);
  NODE_SET_METHOD(target, "writeString", WriteString);

  NODE_SET_METHOD(target, "chmod", Chmod);
  NODE_SET_METHOD(target, "fchmod", FChmod);
  // NODE_SET_METHOD(target, "lchmod", LChmod);

  NODE_SET_METHOD(target, "chown", Chown);
  NODE_SET_METHOD(target, "fchown", FChown);
  // NODE_SET_METHOD(target, "lchown", LChown);

  NODE_SET_METHOD(target, "utimes", UTimes);
  NODE_SET_METHOD(target, "futimes", FUTimes);

  StatWatcher::Initialize(env, target);

  // Create FunctionTemplate for FSReqWrap
  Local<FunctionTemplate> fst =
      FunctionTemplate::New(env->isolate(), NewFSReqWrap);
  fst->InstanceTemplate()->SetInternalFieldCount(1);
  fst->SetClassName(FIXED_ONE_BYTE_STRING(env->isolate(), "FSReqWrap"));
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "FSReqWrap"),
              fst->GetFunction());
}

}  // end namespace node

NODE_MODULE_CONTEXT_AWARE_BUILTIN(fs, node::InitFs)
