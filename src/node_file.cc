#include "node.h"
#include "node_file.h"
#include "node_buffer.h"
#include "node_internals.h"
#include "node_stat_watcher.h"

#include "env.h"
#include "env-inl.h"
#include "req-wrap.h"
#include "req-wrap-inl.h"
#include "string_bytes.h"
#include "util.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#if defined(__MINGW32__) || defined(_MSC_VER)
# include <io.h>
#endif

#include <vector>

namespace node {

using v8::Array;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::Local;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Value;

#ifndef MIN
# define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#define TYPE_ERROR(msg) env->ThrowTypeError(msg)

#define GET_OFFSET(a) ((a)->IsNumber() ? (a)->IntegerValue() : -1)

class FSReqWrap: public ReqWrap<uv_fs_t> {
 public:
  enum Ownership { COPY, MOVE };

  inline static FSReqWrap* New(Environment* env,
                               Local<Object> req,
                               const char* syscall,
                               const char* data = nullptr,
                               enum encoding encoding = UTF8,
                               Ownership ownership = COPY);

  inline void Dispose();

  void ReleaseEarly() {
    if (data_ != inline_data()) {
      delete[] data_;
      data_ = nullptr;
    }
  }

  const char* syscall() const { return syscall_; }
  const char* data() const { return data_; }
  const enum encoding encoding_;

  size_t self_size() const override { return sizeof(*this); }

 private:
  FSReqWrap(Environment* env,
            Local<Object> req,
            const char* syscall,
            const char* data,
            enum encoding encoding)
      : ReqWrap(env, req, AsyncWrap::PROVIDER_FSREQWRAP),
        encoding_(encoding),
        syscall_(syscall),
        data_(data) {
    Wrap(object(), this);
  }

  ~FSReqWrap() { ReleaseEarly(); }

  void* operator new(size_t size) = delete;
  void* operator new(size_t size, char* storage) { return storage; }
  char* inline_data() { return reinterpret_cast<char*>(this + 1); }

  const char* syscall_;
  const char* data_;

  DISALLOW_COPY_AND_ASSIGN(FSReqWrap);
};

#define ASSERT_PATH(path)                                                   \
  if (*path == nullptr)                                                     \
    return TYPE_ERROR( #path " must be a string or Buffer");

FSReqWrap* FSReqWrap::New(Environment* env,
                          Local<Object> req,
                          const char* syscall,
                          const char* data,
                          enum encoding encoding,
                          Ownership ownership) {
  const bool copy = (data != nullptr && ownership == COPY);
  const size_t size = copy ? 1 + strlen(data) : 0;
  FSReqWrap* that;
  char* const storage = new char[sizeof(*that) + size];
  that = new(storage) FSReqWrap(env, req, syscall, data, encoding);
  if (copy)
    that->data_ = static_cast<char*>(memcpy(that->inline_data(), data, size));
  return that;
}


void FSReqWrap::Dispose() {
  this->~FSReqWrap();
  delete[] reinterpret_cast<char*>(this);
}


static void NewFSReqWrap(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.IsConstructCall());
}


static inline bool IsInt64(double x) {
  return x == static_cast<double>(static_cast<int64_t>(x));
}

static void After(uv_fs_t *req) {
  FSReqWrap* req_wrap = static_cast<FSReqWrap*>(req->data);
  CHECK_EQ(&req_wrap->req_, req);
  req_wrap->ReleaseEarly();  // Free memory that's no longer used now.

  Environment* env = req_wrap->env();
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  // there is always at least one argument. "error"
  int argc = 1;

  // Allocate space for two args. We may only use one depending on the case.
  // (Feel free to increase this if you need more)
  Local<Value> argv[2];
  Local<Value> link;

  if (req->result < 0) {
    // An error happened.
    argv[0] = UVException(env->isolate(),
                          req->result,
                          req_wrap->syscall(),
                          nullptr,
                          req->path,
                          req_wrap->data());
  } else {
    // error value is empty or null for non-error.
    argv[0] = Null(env->isolate());

    // All have at least two args now.
    argc = 2;

    switch (req->fs_type) {
      // These all have no data to pass.
      case UV_FS_ACCESS:
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

      case UV_FS_MKDTEMP:
        link = StringBytes::Encode(env->isolate(),
                                   static_cast<const char*>(req->path),
                                   req_wrap->encoding_);
        if (link.IsEmpty()) {
          argv[0] = UVException(env->isolate(),
                                UV_EINVAL,
                                req_wrap->syscall(),
                                "Invalid character encoding for filename",
                                req->path,
                                req_wrap->data());
        } else {
          argv[1] = link;
        }
        break;

      case UV_FS_READLINK:
        link = StringBytes::Encode(env->isolate(),
                                   static_cast<const char*>(req->ptr),
                                   req_wrap->encoding_);
        if (link.IsEmpty()) {
          argv[0] = UVException(env->isolate(),
                                UV_EINVAL,
                                req_wrap->syscall(),
                                "Invalid character encoding for link",
                                req->path,
                                req_wrap->data());
        } else {
          argv[1] = link;
        }
        break;

      case UV_FS_REALPATH:
        link = StringBytes::Encode(env->isolate(),
                                   static_cast<const char*>(req->ptr),
                                   req_wrap->encoding_);
        if (link.IsEmpty()) {
          argv[0] = UVException(env->isolate(),
                                UV_EINVAL,
                                req_wrap->syscall(),
                                "Invalid character encoding for link",
                                req->path,
                                req_wrap->data());
        } else {
          argv[1] = link;
        }
        break;

      case UV_FS_READ:
        // Buffer interface
        argv[1] = Integer::New(env->isolate(), req->result);
        break;

      case UV_FS_SCANDIR:
        {
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
              argv[0] = UVException(r,
                                    nullptr,
                                    req_wrap->syscall(),
                                    static_cast<const char*>(req->path));
              break;
            }

            Local<Value> filename = StringBytes::Encode(env->isolate(),
                                                        ent.name,
                                                        req_wrap->encoding_);
            if (filename.IsEmpty()) {
              argv[0] = UVException(env->isolate(),
                                    UV_EINVAL,
                                    req_wrap->syscall(),
                                    "Invalid character encoding for filename",
                                    req->path,
                                    req_wrap->data());
              break;
            }
            name_argv[name_idx++] = filename;

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

          argv[1] = names;
        }
        break;

      default:
        CHECK(0 && "Unhandled eio response");
    }
  }

  req_wrap->MakeCallback(env->oncomplete_string(), argc, argv);

  uv_fs_req_cleanup(&req_wrap->req_);
  req_wrap->Dispose();
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


#define ASYNC_DEST_CALL(func, req, dest, encoding, ...)                       \
  Environment* env = Environment::GetCurrent(args);                           \
  CHECK(req->IsObject());                                                     \
  FSReqWrap* req_wrap = FSReqWrap::New(env, req.As<Object>(),                 \
                                       #func, dest, encoding);                \
  int err = uv_fs_ ## func(env->event_loop(),                                 \
                           &req_wrap->req_,                                   \
                           __VA_ARGS__,                                       \
                           After);                                            \
  req_wrap->Dispatched();                                                     \
  if (err < 0) {                                                              \
    uv_fs_t* uv_req = &req_wrap->req_;                                        \
    uv_req->result = err;                                                     \
    uv_req->path = nullptr;                                                   \
    After(uv_req);                                                            \
    req_wrap = nullptr;                                                       \
  } else {                                                                    \
    args.GetReturnValue().Set(req_wrap->persistent());                        \
  }

#define ASYNC_CALL(func, req, encoding, ...)                                  \
  ASYNC_DEST_CALL(func, req, nullptr, encoding, __VA_ARGS__)                  \

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

static void Access(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());
  HandleScope scope(env->isolate());

  if (args.Length() < 2)
    return TYPE_ERROR("path and mode are required");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("mode must be an integer");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(access, args[2], UTF8, *path, mode);
  } else {
    SYNC_CALL(access, *path, *path, mode);
  }
}


static void Close(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("fd is required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(close, args[1], UTF8, fd)
  } else {
    SYNC_CALL(close, 0, fd)
  }
}


Local<Value> BuildStatsObject(Environment* env, const uv_stat_t* s) {
  EscapableHandleScope handle_scope(env->isolate());

  // If you hit this assertion, you forgot to enter the v8::Context first.
  CHECK_EQ(env->context(), env->isolate()->GetCurrentContext());

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
      env->fs_stats_constructor_function()->NewInstance(
          env->context(),
          arraysize(argv),
          argv).FromMaybe(Local<Value>());

  if (stats.IsEmpty())
    return handle_scope.Escape(Local<Object>());

  return handle_scope.Escape(stats);
}

// Used to speed up module loading.  Returns the contents of the file as
// a string or undefined when the file cannot be opened.  The speedup
// comes from not creating Error objects on failure.
static void InternalModuleReadFile(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_loop_t* loop = env->event_loop();

  CHECK(args[0]->IsString());
  node::Utf8Value path(env->isolate(), args[0]);

  uv_fs_t open_req;
  const int fd = uv_fs_open(loop, &open_req, *path, O_RDONLY, 0, nullptr);
  uv_fs_req_cleanup(&open_req);

  if (fd < 0) {
    return;
  }

  std::vector<char> chars;
  int64_t offset = 0;
  for (;;) {
    const size_t kBlockSize = 32 << 10;
    const size_t start = chars.size();
    chars.resize(start + kBlockSize);

    uv_buf_t buf;
    buf.base = &chars[start];
    buf.len = kBlockSize;

    uv_fs_t read_req;
    const ssize_t numchars =
        uv_fs_read(loop, &read_req, fd, &buf, 1, offset, nullptr);
    uv_fs_req_cleanup(&read_req);

    CHECK_GE(numchars, 0);
    if (static_cast<size_t>(numchars) < kBlockSize) {
      chars.resize(start + numchars);
    }
    if (numchars == 0) {
      break;
    }
    offset += numchars;
  }

  uv_fs_t close_req;
  CHECK_EQ(0, uv_fs_close(loop, &close_req, fd, nullptr));
  uv_fs_req_cleanup(&close_req);

  size_t start = 0;
  if (chars.size() >= 3 && 0 == memcmp(&chars[0], "\xEF\xBB\xBF", 3)) {
    start = 3;  // Skip UTF-8 BOM.
  }

  Local<String> chars_string =
      String::NewFromUtf8(env->isolate(),
                          &chars[start],
                          String::kNormalString,
                          chars.size() - start);
  args.GetReturnValue().Set(chars_string);
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

  if (args.Length() < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  if (args[1]->IsObject()) {
    ASYNC_CALL(stat, args[1], UTF8, *path)
  } else {
    SYNC_CALL(stat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void LStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  if (args[1]->IsObject()) {
    ASYNC_CALL(lstat, args[1], UTF8, *path)
  } else {
    SYNC_CALL(lstat, *path, *path)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void FStat(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("fd is required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(fstat, args[1], UTF8, fd)
  } else {
    SYNC_CALL(fstat, 0, fd)
    args.GetReturnValue().Set(
        BuildStatsObject(env, static_cast<const uv_stat_t*>(SYNC_REQ.ptr)));
  }
}

static void Symlink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("target path required");
  if (len < 2)
    return TYPE_ERROR("src path required");

  BufferValue target(env->isolate(), args[0]);
  ASSERT_PATH(target)
  BufferValue path(env->isolate(), args[1]);
  ASSERT_PATH(path)

  int flags = 0;

  if (args[2]->IsString()) {
    node::Utf8Value mode(env->isolate(), args[2]);
    if (strcmp(*mode, "dir") == 0) {
      flags |= UV_FS_SYMLINK_DIR;
    } else if (strcmp(*mode, "junction") == 0) {
      flags |= UV_FS_SYMLINK_JUNCTION;
    } else if (strcmp(*mode, "file") != 0) {
      return env->ThrowError("Unknown symlink type");
    }
  }

  if (args[3]->IsObject()) {
    ASYNC_DEST_CALL(symlink, args[3], *path, UTF8, *target, *path, flags)
  } else {
    SYNC_DEST_CALL(symlink, *target, *path, *target, *path, flags)
  }
}

static void Link(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("src path required");
  if (len < 2)
    return TYPE_ERROR("dest path required");

  BufferValue src(env->isolate(), args[0]);
  ASSERT_PATH(src)

  BufferValue dest(env->isolate(), args[1]);
  ASSERT_PATH(dest)

  if (args[2]->IsObject()) {
    ASYNC_DEST_CALL(link, args[2], *dest, UTF8, *src, *dest)
  } else {
    SYNC_DEST_CALL(link, *src, *dest, *src, *dest)
  }
}

static void ReadLink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();

  if (argc < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  Local<Value> callback = Null(env->isolate());
  if (argc == 3)
    callback = args[2];

  if (callback->IsObject()) {
    ASYNC_CALL(readlink, callback, encoding, *path)
  } else {
    SYNC_CALL(readlink, *path, *path)
    const char* link_path = static_cast<const char*>(SYNC_REQ.ptr);
    Local<Value> rc = StringBytes::Encode(env->isolate(),
                                          link_path,
                                          encoding);
    if (rc.IsEmpty()) {
      return env->ThrowUVException(UV_EINVAL,
                                   "readlink",
                                   "Invalid character encoding for link",
                                   *path);
    }
    args.GetReturnValue().Set(rc);
  }
}

static void Rename(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("old path required");
  if (len < 2)
    return TYPE_ERROR("new path required");

  BufferValue old_path(env->isolate(), args[0]);
  ASSERT_PATH(old_path)
  BufferValue new_path(env->isolate(), args[1]);
  ASSERT_PATH(new_path)

  if (args[2]->IsObject()) {
    ASYNC_DEST_CALL(rename, args[2], *new_path, UTF8, *old_path, *new_path)
  } else {
    SYNC_DEST_CALL(rename, *old_path, *new_path, *old_path, *new_path)
  }
}

static void FTruncate(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 2)
    return TYPE_ERROR("fd and length are required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");

  int fd = args[0]->Int32Value();

  // FIXME(bnoordhuis) It's questionable to reject non-ints here but still
  // allow implicit coercion from null or undefined to zero.  Probably best
  // handled in lib/fs.js.
  Local<Value> len_v(args[1]);
  if (!len_v->IsUndefined() &&
      !len_v->IsNull() &&
      !IsInt64(len_v->NumberValue())) {
    return env->ThrowTypeError("Not an integer");
  }

  const int64_t len = len_v->IntegerValue();

  if (args[2]->IsObject()) {
    ASYNC_CALL(ftruncate, args[2], UTF8, fd, len)
  } else {
    SYNC_CALL(ftruncate, 0, fd, len)
  }
}

static void Fdatasync(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("fd is required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(fdatasync, args[1], UTF8, fd)
  } else {
    SYNC_CALL(fdatasync, 0, fd)
  }
}

static void Fsync(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("fd is required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");

  int fd = args[0]->Int32Value();

  if (args[1]->IsObject()) {
    ASYNC_CALL(fsync, args[1], UTF8, fd)
  } else {
    SYNC_CALL(fsync, 0, fd)
  }
}

static void Unlink(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  if (args[1]->IsObject()) {
    ASYNC_CALL(unlink, args[1], UTF8, *path)
  } else {
    SYNC_CALL(unlink, *path, *path)
  }
}

static void RMDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  if (args[1]->IsObject()) {
    ASYNC_CALL(rmdir, args[1], UTF8, *path)
  } else {
    SYNC_CALL(rmdir, *path, *path)
  }
}

static void MKDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 2)
    return TYPE_ERROR("path and mode are required");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("mode must be an integer");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(mkdir, args[2], UTF8, *path, mode)
  } else {
    SYNC_CALL(mkdir, *path, *path, mode)
  }
}

static void RealPath(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();

  if (argc < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  Local<Value> callback = Null(env->isolate());
  if (argc == 3)
    callback = args[2];

  if (callback->IsObject()) {
    ASYNC_CALL(realpath, callback, encoding, *path);
  } else {
    SYNC_CALL(realpath, *path, *path);
    const char* link_path = static_cast<const char*>(SYNC_REQ.ptr);
    Local<Value> rc = StringBytes::Encode(env->isolate(),
                                          link_path,
                                          encoding);
    if (rc.IsEmpty()) {
      return env->ThrowUVException(UV_EINVAL,
                                   "realpath",
                                   "Invalid character encoding for path",
                                   *path);
    }
    args.GetReturnValue().Set(rc);
  }
}

static void ReadDir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  const int argc = args.Length();

  if (argc < 1)
    return TYPE_ERROR("path required");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  Local<Value> callback = Null(env->isolate());
  if (argc == 3)
    callback = args[2];

  if (callback->IsObject()) {
    ASYNC_CALL(scandir, callback, encoding, *path, 0 /*flags*/)
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

      Local<Value> filename = StringBytes::Encode(env->isolate(),
                                                  ent.name,
                                                  encoding);
      if (filename.IsEmpty()) {
        return env->ThrowUVException(UV_EINVAL,
                                     "readdir",
                                     "Invalid character encoding for filename",
                                     *path);
      }

      name_v[name_idx++] = filename;

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

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("path required");
  if (len < 2)
    return TYPE_ERROR("flags required");
  if (len < 3)
    return TYPE_ERROR("mode required");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("flags must be an int");
  if (!args[2]->IsInt32())
    return TYPE_ERROR("mode must be an int");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  int flags = args[1]->Int32Value();
  int mode = static_cast<int>(args[2]->Int32Value());

  if (args[3]->IsObject()) {
    ASYNC_CALL(open, args[3], UTF8, *path, flags, mode)
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
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsInt32())
    return env->ThrowTypeError("First argument must be file descriptor");

  CHECK(Buffer::HasInstance(args[1]));

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
    ASYNC_CALL(write, req, UTF8, fd, &uvbuf, 1, pos)
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
  Local<Value> req = args[3];

  MaybeStackBuffer<uv_buf_t> iovs(chunks->Length());

  for (uint32_t i = 0; i < iovs.length(); i++) {
    Local<Value> chunk = chunks->Get(i);

    if (!Buffer::HasInstance(chunk))
      return env->ThrowTypeError("Array elements all need to be buffers");

    iovs[i] = uv_buf_init(Buffer::Data(chunk), Buffer::Length(chunk));
  }

  if (req->IsObject()) {
    ASYNC_CALL(write, req, UTF8, fd, *iovs, iovs.length(), pos)
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

  if (!args[0]->IsInt32())
    return env->ThrowTypeError("First argument must be file descriptor");

  Local<Value> req;
  Local<Value> string = args[1];
  int fd = args[0]->Int32Value();
  char* buf = nullptr;
  int64_t pos;
  size_t len;
  FSReqWrap::Ownership ownership = FSReqWrap::COPY;

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
    ownership = FSReqWrap::MOVE;
  }
  pos = GET_OFFSET(args[2]);
  req = args[4];

  uv_buf_t uvbuf = uv_buf_init(const_cast<char*>(buf), len);

  if (!req->IsObject()) {
    // SYNC_CALL returns on error.  Make sure to always free the memory.
    struct Delete {
      inline explicit Delete(char* pointer) : pointer_(pointer) {}
      inline ~Delete() { delete[] pointer_; }
      char* const pointer_;
    };
    Delete delete_on_return(ownership == FSReqWrap::MOVE ? buf : nullptr);
    SYNC_CALL(write, nullptr, fd, &uvbuf, 1, pos)
    return args.GetReturnValue().Set(SYNC_RESULT);
  }

  FSReqWrap* req_wrap =
      FSReqWrap::New(env, req.As<Object>(), "write", buf, UTF8, ownership);
  int err = uv_fs_write(env->event_loop(),
                        &req_wrap->req_,
                        fd,
                        &uvbuf,
                        1,
                        pos,
                        After);
  req_wrap->Dispatched();
  if (err < 0) {
    uv_fs_t* uv_req = &req_wrap->req_;
    uv_req->result = err;
    uv_req->path = nullptr;
    After(uv_req);
    return;
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
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 2)
    return TYPE_ERROR("fd and buffer are required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");
  if (!Buffer::HasInstance(args[1]))
    return TYPE_ERROR("Second argument needs to be a buffer");

  int fd = args[0]->Int32Value();

  Local<Value> req;

  size_t len;
  int64_t pos;

  char * buf = nullptr;

  Local<Object> buffer_obj = args[1]->ToObject(env->isolate());
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
    ASYNC_CALL(read, req, UTF8, fd, &uvbuf, 1, pos);
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

  if (args.Length() < 2)
    return TYPE_ERROR("path and mode are required");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("mode must be an integer");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(chmod, args[2], UTF8, *path, mode);
  } else {
    SYNC_CALL(chmod, *path, *path, mode);
  }
}


/* fs.fchmod(fd, mode);
 * Wrapper for fchmod(1) / EIO_FCHMOD
 */
static void FChmod(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() < 2)
    return TYPE_ERROR("fd and mode are required");
  if (!args[0]->IsInt32())
    return TYPE_ERROR("fd must be a file descriptor");
  if (!args[1]->IsInt32())
    return TYPE_ERROR("mode must be an integer");

  int fd = args[0]->Int32Value();
  int mode = static_cast<int>(args[1]->Int32Value());

  if (args[2]->IsObject()) {
    ASYNC_CALL(fchmod, args[2], UTF8, fd, mode);
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
  if (len < 1)
    return TYPE_ERROR("path required");
  if (len < 2)
    return TYPE_ERROR("uid required");
  if (len < 3)
    return TYPE_ERROR("gid required");
  if (!args[1]->IsUint32())
    return TYPE_ERROR("uid must be an unsigned int");
  if (!args[2]->IsUint32())
    return TYPE_ERROR("gid must be an unsigned int");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  uv_uid_t uid = static_cast<uv_uid_t>(args[1]->Uint32Value());
  uv_gid_t gid = static_cast<uv_gid_t>(args[2]->Uint32Value());

  if (args[3]->IsObject()) {
    ASYNC_CALL(chown, args[3], UTF8, *path, uid, gid);
  } else {
    SYNC_CALL(chown, *path, *path, uid, gid);
  }
}


/* fs.fchown(fd, uid, gid);
 * Wrapper for fchown(1) / EIO_FCHOWN
 */
static void FChown(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

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
    ASYNC_CALL(fchown, args[3], UTF8, fd, uid, gid);
  } else {
    SYNC_CALL(fchown, 0, fd, uid, gid);
  }
}


static void UTimes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int len = args.Length();
  if (len < 1)
    return TYPE_ERROR("path required");
  if (len < 2)
    return TYPE_ERROR("atime required");
  if (len < 3)
    return TYPE_ERROR("mtime required");
  if (!args[1]->IsNumber())
    return TYPE_ERROR("atime must be a number");
  if (!args[2]->IsNumber())
    return TYPE_ERROR("mtime must be a number");

  BufferValue path(env->isolate(), args[0]);
  ASSERT_PATH(path)

  const double atime = static_cast<double>(args[1]->NumberValue());
  const double mtime = static_cast<double>(args[2]->NumberValue());

  if (args[3]->IsObject()) {
    ASYNC_CALL(utime, args[3], UTF8, *path, atime, mtime);
  } else {
    SYNC_CALL(utime, *path, *path, atime, mtime);
  }
}

static void FUTimes(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

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
    ASYNC_CALL(futime, args[3], UTF8, fd, atime, mtime);
  } else {
    SYNC_CALL(futime, 0, fd, atime, mtime);
  }
}

static void Mkdtemp(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK_GE(args.Length(), 2);

  BufferValue tmpl(env->isolate(), args[0]);
  if (*tmpl == nullptr)
    return TYPE_ERROR("template must be a string or Buffer");

  const enum encoding encoding = ParseEncoding(env->isolate(), args[1], UTF8);

  if (args[2]->IsObject()) {
    ASYNC_CALL(mkdtemp, args[2], encoding, *tmpl);
  } else {
    SYNC_CALL(mkdtemp, *tmpl, *tmpl);
    const char* path = static_cast<const char*>(SYNC_REQ.path);
    Local<Value> rc = StringBytes::Encode(env->isolate(), path, encoding);
    if (rc.IsEmpty()) {
      return env->ThrowUVException(UV_EINVAL,
                                   "mkdtemp",
                                   "Invalid character encoding for filename",
                                   *tmpl);
    }
    args.GetReturnValue().Set(rc);
  }
}

void FSInitialize(const FunctionCallbackInfo<Value>& args) {
  Local<Function> stats_constructor = args[0].As<Function>();
  CHECK(stats_constructor->IsFunction());

  Environment* env = Environment::GetCurrent(args);
  env->set_fs_stats_constructor_function(stats_constructor);
}

void InitFs(Local<Object> target,
            Local<Value> unused,
            Local<Context> context,
            void* priv) {
  Environment* env = Environment::GetCurrent(context);

  // Function which creates a new Stats object.
  target->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "FSInitialize"),
              env->NewFunctionTemplate(FSInitialize)->GetFunction());

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
  env->SetMethod(target, "internalModuleReadFile", InternalModuleReadFile);
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

  env->SetMethod(target, "chmod", Chmod);
  env->SetMethod(target, "fchmod", FChmod);
  // env->SetMethod(target, "lchmod", LChmod);

  env->SetMethod(target, "chown", Chown);
  env->SetMethod(target, "fchown", FChown);
  // env->SetMethod(target, "lchown", LChown);

  env->SetMethod(target, "utimes", UTimes);
  env->SetMethod(target, "futimes", FUTimes);

  env->SetMethod(target, "mkdtemp", Mkdtemp);

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
