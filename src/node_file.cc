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
#include "node_stat_watcher.h"
#include "req_wrap.h"

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
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Handle;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::Persistent;
using v8::String;
using v8::Value;

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define TYPE_ERROR(msg) ThrowTypeError(msg)

#define THROW_BAD_ARGS TYPE_ERROR("Bad argument")

class FSReqWrap: public ReqWrap<uv_fs_t> {
 public:
  FSReqWrap(const char* syscall)
    : syscall_(syscall) {
  }

  const char* syscall() { return syscall_; }

 private:
  const char* syscall_;
};


static Cached<String> oncomplete_sym;


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

static inline int IsInt64(double x) {
  return x == static_cast<double>(static_cast<int64_t>(x));
}


static void After(uv_fs_t *req) {
  HandleScope scope(node_isolate);

  FSReqWrap* req_wrap = (FSReqWrap*) req->data;
  assert(&req_wrap->req_ == req);

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
        argv[1] = BuildStatsObject(static_cast<const uv_stat_t*>(req->ptr));
        break;

      case UV_FS_READLINK:
        argv[1] = String::New(static_cast<char*>(req->ptr));
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
            Local<String> name = String::New(namebuf);
            names->Set(Integer::New(i, node_isolate), name);
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

  if (oncomplete_sym.IsEmpty()) {
    oncomplete_sym = String::New("oncomplete");
  }
  MakeCallback(req_wrap->object(), oncomplete_sym, argc, argv);

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


#define ASYNC_CALL(func, callback, ...)                           \
  FSReqWrap* req_wrap = new FSReqWrap(#func);                     \
  int err = uv_fs_ ## func(uv_default_loop(), &req_wrap->req_,    \
      __VA_ARGS__, After);                                        \
  req_wrap->object()->Set(oncomplete_sym, callback);              \
  req_wrap->Dispatched();                                         \
  if (err < 0) {                                                  \
    uv_fs_t* req = &req_wrap->req_;                               \
    req->result = err;                                            \
    req->path = NULL;                                             \
    After(req);                                                   \
  }                                                               \
  args.GetReturnValue().Set(req_wrap->persistent());

#define SYNC_CALL(func, path, ...)                                \
  fs_req_wrap req_wrap;                                           \
  int err = uv_fs_ ## func(uv_default_loop(),                     \
                           &req_wrap.req,                         \
                           __VA_ARGS__,                           \
                           NULL);                                 \
  if (err < 0) return ThrowUVException(err, #func, NULL, path);   \

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


static Persistent<Function> stats_constructor;

static Cached<String> dev_symbol;
static Cached<String> ino_symbol;
static Cached<String> mode_symbol;
static Cached<String> nlink_symbol;
static Cached<String> uid_symbol;
static Cached<String> gid_symbol;
static Cached<String> rdev_symbol;
static Cached<String> size_symbol;
static Cached<String> blksize_symbol;
static Cached<String> blocks_symbol;
static Cached<String> atime_symbol;
static Cached<String> mtime_symbol;
static Cached<String> ctime_symbol;

Local<Object> BuildStatsObject(const uv_stat_t* s) {
  HandleScope scope(node_isolate);

  if (dev_symbol.IsEmpty()) {
    dev_symbol = String::New("dev");
    ino_symbol = String::New("ino");
    mode_symbol = String::New("mode");
    nlink_symbol = String::New("nlink");
    uid_symbol = String::New("uid");
    gid_symbol = String::New("gid");
    rdev_symbol = String::New("rdev");
    size_symbol = String::New("size");
    blksize_symbol = String::New("blksize");
    blocks_symbol = String::New("blocks");
    atime_symbol = String::New("atime");
    mtime_symbol = String::New("mtime");
    ctime_symbol = String::New("ctime");
  }

  Local<Function> constructor = PersistentToLocal(stats_constructor);
  Local<Object> stats = constructor->NewInstance();
  if (stats.IsEmpty()) return Local<Object>();

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
    if (val.IsEmpty()) return Local<Object>();                                \
    stats->Set(name##_symbol, val);                                           \
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
    if (val.IsEmpty()) return Local<Object>();                                \
    stats->Set(name##_symbol, val);                                           \
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
    if (val.IsEmpty()) return Local<Object>();                                \
    stats->Set(name##_symbol, val);                                           \
  }
  X(atime, atim)
  X(mtime, mtim)
  X(ctime, ctim)
#undef X

  return scope.Close(stats);
}

static void Stat(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1) return TYPE_ERROR("path required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(stat, args[1], *path)
  } else {
    SYNC_CALL(stat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void LStat(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1) return TYPE_ERROR("path required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(lstat, args[1], *path)
  } else {
    SYNC_CALL(lstat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
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
        BuildStatsObject(static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void Symlink(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  int len = args.Length();
  if (len < 1) return TYPE_ERROR("dest path required");
  if (len < 2) return TYPE_ERROR("src path required");
  if (!args[0]->IsString()) return TYPE_ERROR("dest path must be a string");
  if (!args[1]->IsString()) return TYPE_ERROR("src path must be a string");

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
  if (len < 1) return TYPE_ERROR("dest path required");
  if (len < 2) return TYPE_ERROR("src path required");
  if (!args[0]->IsString()) return TYPE_ERROR("dest path must be a string");
  if (!args[1]->IsString()) return TYPE_ERROR("src path must be a string");

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

  if (args.Length() < 1) return TYPE_ERROR("path required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readlink, args[1], *path)
  } else {
    SYNC_CALL(readlink, *path, *path)
    args.GetReturnValue().Set(
        String::New(static_cast<const char*>(SYNC_REQ.ptr)));
  }
}

static void Rename(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  int len = args.Length();
  if (len < 1) return TYPE_ERROR("old path required");
  if (len < 2) return TYPE_ERROR("new path required");
  if (!args[0]->IsString()) return TYPE_ERROR("old path must be a string");
  if (!args[1]->IsString()) return TYPE_ERROR("new path must be a string");
  
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

  if (args.Length() < 1) return TYPE_ERROR("path required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(unlink, args[1], *path)
  } else {
    SYNC_CALL(unlink, *path, *path)
  }
}

static void RMDir(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (args.Length() < 1) return TYPE_ERROR("path required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");

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

  if (args.Length() < 1) return TYPE_ERROR("path required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");

  String::Utf8Value path(args[0]);

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readdir, args[1], *path, 0 /*flags*/)
  } else {
    SYNC_CALL(readdir, *path, *path, 0 /*flags*/)

    char *namebuf = static_cast<char*>(SYNC_REQ.ptr);
    int nnames = req_wrap.req.result;
    Local<Array> names = Array::New(nnames);

    for (int i = 0; i < nnames; i++) {
      Local<String> name = String::New(namebuf);
      names->Set(Integer::New(i, node_isolate), name);
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
  if (len < 1) return TYPE_ERROR("path required");
  if (len < 2) return TYPE_ERROR("flags required");
  if (len < 3) return TYPE_ERROR("mode required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");
  if (!args[1]->IsInt32()) return TYPE_ERROR("flags must be an int");
  if (!args[2]->IsInt32()) return TYPE_ERROR("mode must be an int");

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

// bytesWritten = write(fd, data, position, enc, callback)
// Wrapper for write(2).
//
// 0 fd        integer. file descriptor
// 1 buffer    the data to write
// 2 offset    where in the buffer to start from
// 3 length    how much to write
// 4 position  if integer, position to write at in the file.
//             if null, write from the current position
static void Write(const FunctionCallbackInfo<Value>& args) {
  HandleScope scope(node_isolate);

  if (!args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

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

  ssize_t len = args[3]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowError("off + len > buffer.length");
  }

  ASSERT_OFFSET(args[4]);
  int64_t pos = GET_OFFSET(args[4]);

  char * buf = (char*)buffer_data + off;
  Local<Value> cb = args[5];

  if (cb->IsFunction()) {
    ASYNC_CALL(write, cb, fd, buf, len, pos)
  } else {
    SYNC_CALL(write, 0, fd, buf, len, pos)
    args.GetReturnValue().Set(SYNC_RESULT);
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

  if(args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  String::Utf8Value path(args[0]);
  int mode = static_cast<int>(args[1]->Int32Value());

  if(args[2]->IsFunction()) {
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

  if(args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  int fd = args[0]->Int32Value();
  int mode = static_cast<int>(args[1]->Int32Value());

  if(args[2]->IsFunction()) {
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
  if (len < 1) return TYPE_ERROR("path required");
  if (len < 2) return TYPE_ERROR("uid required");
  if (len < 3) return TYPE_ERROR("gid required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");
  if (!args[1]->IsUint32()) return TYPE_ERROR("uid must be an unsigned int");
  if (!args[2]->IsUint32()) return TYPE_ERROR("gid must be an unsigned int");

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
  if (len < 1) return TYPE_ERROR("fd required");
  if (len < 2) return TYPE_ERROR("uid required");
  if (len < 3) return TYPE_ERROR("gid required");
  if (!args[0]->IsInt32()) return TYPE_ERROR("fd must be an int");
  if (!args[1]->IsUint32()) return TYPE_ERROR("uid must be an unsigned int");
  if (!args[2]->IsUint32()) return TYPE_ERROR("gid must be an unsigned int");

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
  if (len < 1) return TYPE_ERROR("path required");
  if (len < 2) return TYPE_ERROR("atime required");
  if (len < 3) return TYPE_ERROR("mtime required");
  if (!args[0]->IsString()) return TYPE_ERROR("path must be a string");
  if (!args[1]->IsNumber()) return TYPE_ERROR("atime must be a number");
  if (!args[2]->IsNumber()) return TYPE_ERROR("mtime must be a number");

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
  if (len < 1) return TYPE_ERROR("fd required");
  if (len < 2) return TYPE_ERROR("atime required");
  if (len < 3) return TYPE_ERROR("mtime required");
  if (!args[0]->IsInt32()) return TYPE_ERROR("fd must be an int");
  if (!args[1]->IsNumber()) return TYPE_ERROR("atime must be a number");
  if (!args[2]->IsNumber()) return TYPE_ERROR("mtime must be a number");

  const int fd = args[0]->Int32Value();
  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(futime, args[3], fd, atime, mtime);
  } else {
    SYNC_CALL(futime, 0, fd, atime, mtime);
  }
}


void File::Initialize(Handle<Object> target) {
  HandleScope scope(node_isolate);

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
  NODE_SET_METHOD(target, "write", Write);

  NODE_SET_METHOD(target, "chmod", Chmod);
  NODE_SET_METHOD(target, "fchmod", FChmod);
  //NODE_SET_METHOD(target, "lchmod", LChmod);

  NODE_SET_METHOD(target, "chown", Chown);
  NODE_SET_METHOD(target, "fchown", FChown);
  //NODE_SET_METHOD(target, "lchown", LChown);

  NODE_SET_METHOD(target, "utimes", UTimes);
  NODE_SET_METHOD(target, "futimes", FUTimes);
}

void InitFs(Handle<Object> target) {
  HandleScope scope(node_isolate);

  // Initialize the stats object
  Local<Function> constructor = FunctionTemplate::New()->GetFunction();
  target->Set(String::NewSymbol("Stats"), constructor);
  stats_constructor.Reset(node_isolate, constructor);

  File::Initialize(target);

  oncomplete_sym = String::New("oncomplete");

  StatWatcher::Initialize(target);
}

}  // end namespace node

NODE_MODULE(node_fs, node::InitFs)
