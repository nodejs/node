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

#define TYPE_ERROR(msg) ThrowTypeError(msg)

#define THROW_BAD_ARGS TYPE_ERROR("Bad argument")

class FSReqWrap: public ReqWrap<uv_fs_t> {
 public:
  FSReqWrap(Environment* env, const char* syscall, char* data = NULL)
    : ReqWrap<uv_fs_t>(env, Object::New()),
      syscall_(syscall),
      data_(data) {
  }

  void ReleaseEarly() {
    if (data_ == NULL)
      return;
    delete[] data_;
    data_ = NULL;
  }

  const char* syscall() { return syscall_; }

 private:
  const char* syscall_;
  char* data_;
};


#define ASSERT_OFFSET(a) \
  if (!(a)->IsUndefined() && !(a)->IsNull() && !IsInt64((a)->NumberValue())) { \
    return ThrowTypeError("Not an integer"); \
  }
#define ASSERT_TRUNCATE_LENGTH(a) \
  if (!(a)->IsUndefined() && !(a)->IsNull() && !IsInt64((a)->NumberValue())) { \
    return ThrowTypeError("Not an integer"); \
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
  Context::Scope context_scope(env->context());
  HandleScope handle_scope(env->isolate());

  // there is always at least one argument. "error"
  int argc = 1;

  // Allocate space for two args. We may only use one depending on the case.
  // (Feel free to increase this if you need more)
  Local<Value> argv[2];

  if (req->result < 0) {
    // If the request doesn't have a path parameter set.
    if (req->path == NULL) {
      argv[0] = UVException(req->result, NULL, req_wrap->syscall());
    } else {
      argv[0] = UVException(req->result,
                            NULL,
                            req_wrap->syscall(),
                            static_cast<const char*>(req->path));
    }
  } else {
    // error value is empty or null for non-error.
    argv[0] = Null(node_isolate);

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

      case UV_FS_UTIME:
      case UV_FS_FUTIME:
        argc = 0;
        break;

      case UV_FS_OPEN:
        argv[1] = Integer::New(req->result, node_isolate);
        break;

      case UV_FS_WRITE:
        argv[1] = Integer::New(req->result, node_isolate);
        break;

      case UV_FS_STAT:
      case UV_FS_LSTAT:
      case UV_FS_FSTAT:
        argv[1] = BuildStatsObject(env,
                                   static_cast<const uv_stat_t*>(req->ptr));
        break;

      case UV_FS_READLINK:
        argv[1] = String::NewFromUtf8(node_isolate,
                                      static_cast<const char*>(req->ptr));
        break;

      case UV_FS_READ:
        // Buffer interface
        argv[1] = Integer::New(req->result, node_isolate);
        break;

      case UV_FS_READDIR:
        {
          char *namebuf = static_cast<char*>(req->ptr);
          int nnames = req->result;

          Local<Array> names = Array::New(nnames);

          for (int i = 0; i < nnames; i++) {
            Local<String> name = String::NewFromUtf8(node_isolate, namebuf);
            names->Set(i, name);
#ifndef NDEBUG
            namebuf += strlen(namebuf);
            assert(*namebuf == '\0');
            namebuf += 1;
#else
            namebuf += strlen(namebuf) + 1;
#endif
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


#define ASYNC_CALL(func, callback, ...)                                       \
  Environment* env = Environment::GetCurrent(args.GetIsolate());              \
  FSReqWrap* req_wrap = new FSReqWrap(env, #func);                            \
  int err = uv_fs_ ## func(env->event_loop(),                                 \
                           &req_wrap->req_,                                   \
                           __VA_ARGS__,                                       \
                           After);                                            \
  req_wrap->object()->Set(env->oncomplete_string(), callback);                \
  req_wrap->Dispatched();                                                     \
  if (err < 0) {                                                              \
    uv_fs_t* req = &req_wrap->req_;                                           \
    req->result = err;                                                        \
    req->path = NULL;                                                         \
    After(req);                                                               \
  }                                                                           \
  args.GetReturnValue().Set(req_wrap->persistent());

#define SYNC_CALL(func, path, ...)                                            \
  fs_req_wrap req_wrap;                                                       \
  Environment* env = Environment::GetCurrent(args.GetIsolate());              \
  int err = uv_fs_ ## func(env->event_loop(),                                 \
                           &req_wrap.req,                                     \
                           __VA_ARGS__,                                       \
                           NULL);                                             \
  if (err < 0)                                                                \
    return ThrowUVException(err, #func, NULL, path);                          \

#define SYNC_REQ req_wrap.req

#define SYNC_RESULT err


static void Close(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(close, args[1], fd)
  } else {
    SYNC_CALL(close, 0, fd)
  }
}


Local<Object> BuildStatsObject(Environment* env, const uv_stat_t* s) {
  // If you hit this assertion, you forgot to enter the v8::Context first.
  assert(env->context() == env->isolate()->GetCurrentContext());

  HandleScope handle_scope(env->isolate());

  Local<Object> stats = env->stats_constructor_function()->NewInstance();
  if (stats.IsEmpty()) {
    return Local<Object>();
  }

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
#define X(name)                                                               \
  {                                                                           \
    Local<Value> val = Integer::New(s->st_##name, node_isolate);              \
    if (val.IsEmpty())                                                        \
      return Local<Object>();                                                 \
    stats->Set(env->name ## _string(), val);                                  \
  }
  X(dev)
  X(mode)
  X(nlink)
  X(uid)
  X(gid)
  X(rdev)
# if defined(__POSIX__)
  X(blksize)
# endif
#undef X

#define X(name)                                                               \
  {                                                                           \
    Local<Value> val = Number::New(static_cast<double>(s->st_##name));        \
    if (val.IsEmpty())                                                        \
      return Local<Object>();                                                 \
    stats->Set(env->name ## _string(), val);                                  \
  }
  X(ino)
  X(size)
# if defined(__POSIX__)
  X(blocks)
# endif
#undef X

#define X(name, rec)                                                          \
  {                                                                           \
    double msecs = static_cast<double>(s->st_##rec.tv_sec) * 1000;            \
    msecs += static_cast<double>(s->st_##rec.tv_nsec / 1000000);              \
    Local<Value> val = v8::Date::New(msecs);                                  \
    if (val.IsEmpty())                                                        \
      return Local<Object>();                                                 \
    stats->Set(env->name ## _string(), val);                                  \
  }
  X(atime, atim)
  X(mtime, mtim)
  X(ctime, ctim)
  X(birthtime, birthtim)
#undef X

  return handle_scope.Close(stats);
}

static void Stat(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(stat, args[1], *path)
  } else {
    SYNC_CALL(stat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void LStat(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(lstat, args[1], *path)
  } else {
    SYNC_CALL(lstat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void FStat(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fstat, args[1], fd)
  } else {
    SYNC_CALL(fstat, 0, fd)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void Symlink(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("dest path required");
  if (len < 2)
    return TYPE_ERROR("src path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("dest path must be a string");
  if (!args[1]->IsString())
    return TYPE_ERROR("src path must be a string");

  String::Utf8Value dest(args[0]);
  String::Utf8Value path(args[1]);
  int flags = 0;

  if (args[2]->IsString()) {
    String::Utf8Value mode(args[2]);
    if (strcmp(*mode, "dir") == 0) {
      flags |= UV_FS_SYMLINK_DIR;
    } else if (strcmp(*mode, "junction") == 0) {
      flags |= UV_FS_SYMLINK_JUNCTION;
    } else if (strcmp(*mode, "file") != 0) {
      return ThrowError("Unknown symlink type");
    }
  }

  if (args[3]->IsFunction()) {
    ASYNC_CALL(symlink, args[3], *dest, *path, flags)
  } else {
    SYNC_CALL(symlink, *path, *dest, *path, flags)
  }
}

static void Link(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("dest path required");
  if (len < 2)
    return TYPE_ERROR("src path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("dest path must be a string");
  if (!args[1]->IsString())
    return TYPE_ERROR("src path must be a string");

  String::Utf8Value orig_path(args[0]);
  String::Utf8Value new_path(args[1]);

  if (args[2]->IsFunction()) {
    ASYNC_CALL(link, args[2], *orig_path, *new_path)
  } else {
    SYNC_CALL(link, *orig_path, *orig_path, *new_path)
  }
}

static void ReadLink(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readlink, args[1], *path)
  } else {
    SYNC_CALL(readlink, *path, *path)
    const char* link_path = static_cast<const char*>(SYNC_REQ.ptr);
    Local<String> rc = String::NewFromUtf8(node_isolate, link_path);
    args.GetReturnValue().Set(rc);
  }
}

static void Rename(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("old path required");
  if (len < 2)
    return TYPE_ERROR("new path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("old path must be a string");
  if (!args[1]->IsString())
    return TYPE_ERROR("new path must be a string");

  String::Utf8Value old_path(args[0]);
  String::Utf8Value new_path(args[1]);

  if (args[2]->IsFunction()) {
    ASYNC_CALL(rename, args[2], *old_path, *new_path)
  } else {
    SYNC_CALL(rename, *old_path, *old_path, *new_path)
  }
}

static void FTruncate(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  ASSERT_TRUNCATE_LENGTH(args[1]);
  int64_t len = GET_TRUNCATE_LENGTH(args[1]);

  if (args[2]->IsFunction()) {
    ASYNC_CALL(ftruncate, args[2], fd, len)
  } else {
    SYNC_CALL(ftruncate, 0, fd, len)
  }
}

static void Fdatasync(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fdatasync, args[1], fd)
  } else {
    SYNC_CALL(fdatasync, 0, fd)
  }
}

static void Fsync(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fsync, args[1], fd)
  } else {
    SYNC_CALL(fsync, 0, fd)
  }
}

static void Unlink(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(unlink, args[1], *path)
  } else {
    SYNC_CALL(unlink, *path, *path)
  }
}

static void RMDir(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(rmdir, args[1], *path)
  } else {
    SYNC_CALL(rmdir, *path, *path)
  }
}

static void MKDir(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]);
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(mkdir, args[2], *path, mode)
  } else {
    SYNC_CALL(mkdir, *path, *path, mode)
  }
}

static void ReadDir(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");
  if (!args[0]->IsString())
    return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readdir, args[1], *path, 0 /*flags*/)
  } else {
    SYNC_CALL(readdir, *path, *path, 0 /*flags*/)

    assert(SYNC_REQ.result >= 0);
    char* namebuf = static_cast<char*>(SYNC_REQ.ptr);
    uint32_t nnames = SYNC_REQ.result;
    Local<Array> names = Array::New(nnames);

    for (uint32_t i = 0; i < nnames; ++i) {
      names->Set(i, String::NewFromUtf8(node_isolate, namebuf));
#ifndef NDEBUG
      namebuf += strlen(namebuf);
      assert(*namebuf == '\0');
      namebuf += 1;
#else
      namebuf += strlen(namebuf) + 1;
#endif
    }

    args.GetReturnValue().Set(names);
  }
}

static void Open(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

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

  String::Utf8Value path(args[0]);
  int flags = args[1]->Int32Value();
  int mode = static_cast<int>(args[2]->Int32Value());

  if (args[3]->IsFunction()) {
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
  HandleScope scope(node_isolate);

  assert(args[0]->IsInt32());
  assert(Buffer::HasInstance(args[1]));

  int fd = args[0]->Int32Value();
  Local<Object> obj = args[1].As<Object>();
  const char* buf = Buffer::Data(obj);
  size_t buffer_length = Buffer::Length(obj);
  size_t off = args[2]->Uint32Value();
  size_t len = args[3]->Uint32Value();
  int64_t pos = GET_OFFSET(args[4]);
  Local<Value> cb = args[5];

  if (off > buffer_length)
    return ThrowRangeError("offset out of bounds");
  if (len > buffer_length)
    return ThrowRangeError("length out of bounds");
  if (off + len < off)
    return ThrowRangeError("off + len overflow");
  if (off + len > buffer_length)
    return ThrowRangeError("off + len > buffer.length");

  buf += off;

  if (cb->IsFunction()) {
    ASYNC_CALL(write, cb, fd, buf, len, pos)
    return;
  }

  SYNC_CALL(write, NULL, fd, buf, len, pos)
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
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope handle_scope(args.GetIsolate());

  if (!args[0]->IsInt32())
    return ThrowTypeError("First argument must be file descriptor");

  Local<Value> cb;
  Local<Value> string = args[1];
  int fd = args[0]->Int32Value();
  char* buf = NULL;
  int64_t pos;
  size_t len;
  bool must_free = false;

  // will assign buf and len if string was external
  if (!StringBytes::GetExternalParts(string,
                                     const_cast<const char**>(&buf),
                                     &len)) {
    enum encoding enc = ParseEncoding(args[3], UTF8);
    len = StringBytes::StorageSize(string, enc);
    buf = new char[len];
    // StorageSize may return too large a char, so correct the actual length
    // by the write size
    len = StringBytes::Write(buf, len, args[1], enc);
    must_free = true;
  }
  pos = GET_OFFSET(args[2]);
  cb = args[4];

  if (!cb->IsFunction()) {
    SYNC_CALL(write, NULL, fd, buf, len, pos)
    if (must_free)
      delete[] buf;
    return args.GetReturnValue().Set(SYNC_RESULT);
  }

  FSReqWrap* req_wrap = new FSReqWrap(env, "write", must_free ? buf : NULL);
  int err = uv_fs_write(env->event_loop(),
                        &req_wrap->req_,
                        fd,
                        buf,
                        len,
                        pos,
                        After);
  req_wrap->object()->Set(env->oncomplete_string(), cb);
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
  HandleScope scope(node_isolate);

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  Local<Value> cb;

  size_t len;
  int64_t pos;

  char * buf = NULL;

  if (!Buffer::HasInstance(args[1])) {
    return ThrowError("Second argument needs to be a buffer");
  }

  Local<Object> buffer_obj = args[1]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[2]->Int32Value();
  if (off >= buffer_length) {
    return ThrowError("Offset is out of bounds");
  }

  len = args[3]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowError("Length extends beyond buffer");
  }

  pos = GET_OFFSET(args[4]);

  buf = buffer_data + off;

  cb = args[5];

  if (cb->IsFunction()) {
    ASYNC_CALL(read, cb, fd, buf, len, pos);
  } else {
    SYNC_CALL(read, 0, fd, buf, len, pos)
    args.GetReturnValue().Set(SYNC_RESULT);
  }
}


/* fs.chmod(path, mode);
 * Wrapper for chmod(1) / EIO_CHMOD
 */
static void Chmod(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  String::Utf8Value path(args[0]);
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(chmod, args[2], *path, mode);
  } else {
    SYNC_CALL(chmod, *path, *path, mode);
  }
}


/* fs.fchmod(fd, mode);
 * Wrapper for fchmod(1) / EIO_FCHMOD
 */
static void FChmod(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  int fd = args[0]->Int32Value();
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(fchmod, args[2], fd, mode);
  } else {
    SYNC_CALL(fchmod, 0, fd, mode);
  }
}


/* fs.chown(path, uid, gid);
 * Wrapper for chown(1) / EIO_CHOWN
 */
static void Chown(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

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

  String::Utf8Value path(args[0]);
  uv_uid_t uid = static_cast<uv_uid_t>(args[1]->Uint32Value());
  uv_gid_t gid = static_cast<uv_gid_t>(args[2]->Uint32Value());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(chown, args[3], *path, uid, gid);
  } else {
    SYNC_CALL(chown, *path, *path, uid, gid);
  }
}


/* fs.fchown(fd, uid, gid);
 * Wrapper for fchown(1) / EIO_FCHOWN
 */
static void FChown(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

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

  if (args[3]->IsFunction()) {
    ASYNC_CALL(fchown, args[3], fd, uid, gid);
  } else {
    SYNC_CALL(fchown, 0, fd, uid, gid);
  }
}


static void UTimes(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

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

  const String::Utf8Value path(args[0]);
  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(utime, args[3], *path, atime, mtime);
  } else {
    SYNC_CALL(utime, *path, *path, atime, mtime);
  }
}

static void FUTimes(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

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

  if (args[3]->IsFunction()) {
    ASYNC_CALL(futime, args[3], fd, atime, mtime);
  } else {
    SYNC_CALL(futime, 0, fd, atime, mtime);
  }
}


void InitFs(Handle<Object> target,
            Handle<Value> unused,
            Handle<Context> context) {
  Environment* env = Environment::GetCurrent(context);

  // Initialize the stats object
  Local<Function> constructor = FunctionTemplate::New()->GetFunction();
  target->Set(FIXED_ONE_BYTE_STRING(node_isolate, "Stats"), constructor);
  env->set_stats_constructor_function(constructor);

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

  StatWatcher::Initialize(target);
}

}  // end namespace node

NODE_MODULE_CONTEXT_AWARE(node_fs, node::InitFs)
