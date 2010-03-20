// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node_file.h>
#include <node_buffer.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

/* used for readlink, AIX doesn't provide it */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

namespace node {

using namespace v8;

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define THROW_BAD_ARGS \
  ThrowException(Exception::TypeError(String::New("Bad argument")))
static Persistent<String> encoding_symbol;
static Persistent<String> errno_symbol;

static inline Local<Value> errno_exception(int errorno) {
  Local<Value> e = Exception::Error(String::NewSymbol(strerror(errorno)));
  Local<Object> obj = e->ToObject();
  obj->Set(errno_symbol, Integer::New(errorno));
  return e;
}

static int After(eio_req *req) {
  HandleScope scope;

  Persistent<Function> *callback = cb_unwrap(req->data);

  ev_unref(EV_DEFAULT_UC);

  int argc = 0;
  Local<Value> argv[6];  // 6 is the maximum number of args

  if (req->errorno != 0) {
    argc = 1;
    argv[0] = errno_exception(req->errorno);
  } else {
    // Note: the error is always given the first argument of the callback.
    // If there is no error then then the first argument is null.
    argv[0] = Local<Value>::New(Null());

    switch (req->type) {
      case EIO_CLOSE:
      case EIO_RENAME:
      case EIO_UNLINK:
      case EIO_RMDIR:
      case EIO_MKDIR:
      case EIO_FTRUNCATE:
      case EIO_LINK:
      case EIO_SYMLINK:
      case EIO_CHMOD:
        argc = 0;
        break;

      case EIO_OPEN:
      case EIO_SENDFILE:
        argc = 2;
        argv[1] = Integer::New(req->result);
        break;

      case EIO_WRITE:
        argc = 2;
        argv[1] = Integer::New(req->result);
        break;

      case EIO_STAT:
      case EIO_LSTAT:
      {
        struct stat *s = reinterpret_cast<struct stat*>(req->ptr2);
        argc = 2;
        argv[1] = BuildStatsObject(s);
        break;
      }

      case EIO_READLINK:
      {
        argc = 2;
        argv[1] = String::New(static_cast<char*>(req->ptr2), req->result);
        break;
      }

      case EIO_READ:
      {
        argc = 3;
        Local<Object> obj = Local<Object>::New(*callback);
        Local<Value> enc_val = obj->GetHiddenValue(encoding_symbol);
        argv[1] = Encode(req->ptr2, req->result, ParseEncoding(enc_val));
        argv[2] = Integer::New(req->result);
        break;
      }

      case EIO_READDIR:
      {
        char *namebuf = static_cast<char*>(req->ptr2);
        int nnames = req->result;

        Local<Array> names = Array::New(nnames);

        for (int i = 0; i < nnames; i++) {
          Local<String> name = String::New(namebuf);
          names->Set(Integer::New(i), name);
#ifndef NDEBUG
          namebuf += strlen(namebuf);
          assert(*namebuf == '\0');
          namebuf += 1;
#else
          namebuf += strlen(namebuf) + 1;
#endif
        }

        argc = 2;
        argv[1] = names;
        break;
      }

      default:
        assert(0 && "Unhandled eio response");
    }
  }

  if (req->type == EIO_WRITE && req->int3 == 1) {
    assert(req->ptr2);
    delete [] reinterpret_cast<char*>(req->ptr2);
  }

  TryCatch try_catch;

  (*callback)->Call(Context::GetCurrent()->Global(), argc, argv);

  if (try_catch.HasCaught()) {
    FatalException(try_catch);
  }

  // Dispose of the persistent handle
  cb_destroy(callback);

  return 0;
}

#define ASYNC_CALL(func, callback, ...)                           \
  eio_req *req = eio_##func(__VA_ARGS__, EIO_PRI_DEFAULT, After,  \
    cb_persist(callback));                                        \
  assert(req);                                                    \
  ev_ref(EV_DEFAULT_UC);                                          \
  return Undefined();

static Handle<Value> Close(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(close, args[1], fd)
  } else {
    int ret = close(fd);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> Stat(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(stat, args[1], *path)
  } else {
    struct stat s;
    int ret = stat(*path, &s);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return scope.Close(BuildStatsObject(&s));
  }
}

static Handle<Value> LStat(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(lstat, args[1], *path)
  } else {
    struct stat s;
    int ret = lstat(*path, &s);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return scope.Close(BuildStatsObject(&s));
  }
}

static Handle<Value> Symlink(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value dest(args[0]->ToString());
  String::Utf8Value path(args[1]->ToString());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(symlink, args[2], *dest, *path)
  } else {
    int ret = symlink(*dest, *path);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> Link(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value orig_path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(link, args[2], *orig_path, *new_path)
  } else {
    int ret = link(*orig_path, *new_path);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> ReadLink(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readlink, args[1], *path)
  } else {
    char buf[PATH_MAX];
    ssize_t bz = readlink(*path, buf, PATH_MAX);
    if (bz == -1) return ThrowException(errno_exception(errno));
    return scope.Close(String::New(buf, bz));
  }
}

static Handle<Value> Rename(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value old_path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(rename, args[2], *old_path, *new_path)
  } else {
    int ret = rename(*old_path, *new_path);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> Truncate(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();
  off_t len = args[1]->Uint32Value();

  if (args[2]->IsFunction()) {
    ASYNC_CALL(ftruncate, args[2], fd, len)
  } else {
    int ret = ftruncate(fd, len);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> Unlink(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(unlink, args[1], *path)
  } else {
    int ret = unlink(*path);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> RMDir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(rmdir, args[1], *path)
  } else {
    int ret = rmdir(*path);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> MKDir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());
  mode_t mode = static_cast<mode_t>(args[1]->Int32Value());

  if (args[2]->IsFunction()) {
    ASYNC_CALL(mkdir, args[2], *path, mode)
  } else {
    int ret = mkdir(*path, mode);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}

static Handle<Value> SendFile(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4 || 
      !args[0]->IsInt32() ||
      !args[1]->IsInt32() ||
      !args[3]->IsNumber()) {
    return THROW_BAD_ARGS;
  }

  int out_fd = args[0]->Int32Value();
  int in_fd = args[1]->Int32Value();
  off_t in_offset = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;
  size_t length = args[3]->IntegerValue();

  if (args[4]->IsFunction()) {
    ASYNC_CALL(sendfile, args[4], out_fd, in_fd, in_offset, length)
  } else {
    ssize_t sent = eio_sendfile_sync (out_fd, in_fd, in_offset, length);
    // XXX is this the right errno to use?
    if (sent < 0) return ThrowException(errno_exception(errno));
    return Integer::New(sent);
  }
}

static Handle<Value> ReadDir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readdir, args[1], *path, 0 /*flags*/)
  } else {
    DIR *dir = opendir(*path);
    if (!dir) return ThrowException(errno_exception(errno));

    struct dirent *ent;

    Local<Array> files = Array::New();
    char *name;
    int i = 0;

    while (ent = readdir(dir)) {
      name = ent->d_name;

      if (name[0] != '.' || (name[1] && (name[1] != '.' || name[2]))) {
        files->Set(Integer::New(i), String::New(name));
        i++;
      }
    }

    closedir(dir);

    return scope.Close(files);
  }
}

static Handle<Value> Open(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3 ||
      !args[0]->IsString() ||
      !args[1]->IsInt32() ||
      !args[2]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());
  int flags = args[1]->Int32Value();
  mode_t mode = static_cast<mode_t>(args[2]->Int32Value());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(open, args[3], *path, flags, mode)
  } else {
    int fd = open(*path, flags, mode);
    if (fd < 0) return ThrowException(errno_exception(errno));
    return scope.Close(Integer::New(fd));
  }
}

// write(fd, data, position, enc, callback)
// Wrapper for write(2).
//
// 0 fd        integer. file descriptor
// 1 buffer    the data to write
// 2 offset    where in the buffer to start from
// 3 length    how much to write
// 4 position  if integer, position to write at in the file.
//             if null, write from the current position
//
//           - OR -
//
// 0 fd        integer. file descriptor
// 1 string    the data to write
// 2 position  if integer, position to write at in the file.
//             if null, write from the current position
// 3 encoding
static Handle<Value> Write(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  off_t pos;
  ssize_t len;
  char * buf;
  ssize_t written;

  Local<Value> cb;
  bool legacy;

  if (Buffer::HasInstance(args[1])) {
    legacy = false;
    // buffer
    //
    // 0 fd        integer. file descriptor
    // 1 buffer    the data to write
    // 2 offset    where in the buffer to start from
    // 3 length    how much to write
    // 4 position  if integer, position to write at in the file.
    //             if null, write from the current position

    Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[1]->ToObject());

    size_t off = args[2]->Int32Value();
    if (off >= buffer->length()) {
      return ThrowException(Exception::Error(
            String::New("Offset is out of bounds")));
    }

    len = args[3]->Int32Value();
    if (off + len > buffer->length()) {
      return ThrowException(Exception::Error(
            String::New("Length is extends beyond buffer")));
    }

    pos = args[4]->IsNumber() ? args[4]->IntegerValue() : -1;

    buf = (char*)buffer->data() + off;

    cb = args[5];

  } else {
    legacy = true;
    // legacy interface.. args[1] is a string

    pos = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

    enum encoding enc = ParseEncoding(args[3]);

    len = DecodeBytes(args[1], enc);
    if (len < 0) {
      Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
      return ThrowException(exception);
    }

    buf = new char[len];
    written = DecodeWrite(buf, len, args[1], enc);
    assert(written == len);

    cb = args[4];
  }

  if (cb->IsFunction()) {
    // WARNING: HACK AHEAD, PROCEED WITH CAUTION
    // Normally here I would do
    //   ASYNC_CALL(write, cb, fd, buf, len, pos)
    // however, I'm trying to support a legacy interface; where in the
    // String version a buffer is allocated to encode into. In the After()
    // function it is freed. However in the other version, we just increase
    // the reference count to a buffer. We have to let After() know which
    // version is being done so it can know if it needs to free 'buf' or
    // not. We do that by using req->int3.
    //   req->int3 == 1 legacy, String version. Need to free `buf`.
    //   req->int3 == 0 Buffer version. Don't do anything.
    eio_req *req = eio_write(fd, buf, len, pos,
                             EIO_PRI_DEFAULT,
                             After,
                             cb_persist(cb));
    assert(req);
    req->int3 = legacy ? 1 : 0;
    ev_ref(EV_DEFAULT_UC);
    return Undefined();

  } else {
    if (pos < 0) {
      written = write(fd, buf, len);
    } else {
      written = pwrite(fd, buf, len, pos);
    }
    if (written < 0) return ThrowException(errno_exception(errno));
    return scope.Close(Integer::New(written));
  }
}

/* node.fs.read(fd, length, position, encoding)
 * Wrapper for read(2).
 *
 * 0 fd        integer. file descriptor
 * 1 length    integer. length to read
 * 2 position  if integer, position to read from in the file.
 *             if null, read from the current position
 * 3 encoding
 */
static Handle<Value> Read(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsNumber()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();
  size_t len = args[1]->IntegerValue();
  off_t offset = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;
  enum encoding encoding = ParseEncoding(args[3]);

  if (args[4]->IsFunction()) {
    Local<Object> obj = args[4]->ToObject();
    obj->SetHiddenValue(encoding_symbol, args[3]);
    ASYNC_CALL(read, args[4], fd, NULL, len, offset)
  } else {
#define READ_BUF_LEN (16*1024)
    char *buf[READ_BUF_LEN];
    ssize_t ret;
    if (offset < 0) {
      ret = read(fd, buf, MIN(len, READ_BUF_LEN));
    } else {
      ret = pread(fd, buf, MIN(len, READ_BUF_LEN), offset);
    }
    if (ret < 0) return ThrowException(errno_exception(errno));
    Local<Array> a = Array::New(2);
    a->Set(Integer::New(0), Encode(buf, ret, encoding));
    a->Set(Integer::New(1), Integer::New(ret));
    return scope.Close(a);
  }
}

/* node.fs.chmod(fd, mode);
 * Wrapper for chmod(1) / EIO_CHMOD
 */
static Handle<Value> Chmod(const Arguments& args){
  HandleScope scope;
  
  if(args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  String::Utf8Value path(args[0]->ToString());
  mode_t mode = static_cast<mode_t>(args[1]->Int32Value());
  
  if(args[2]->IsFunction()) {
    ASYNC_CALL(chmod, args[2], *path, mode);
  } else {
    int ret = chmod(*path, mode);
    if (ret != 0) return ThrowException(errno_exception(errno));
    return Undefined();
  }
}


void File::Initialize(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "open", Open);
  NODE_SET_METHOD(target, "read", Read);
  NODE_SET_METHOD(target, "rename", Rename);
  NODE_SET_METHOD(target, "truncate", Truncate);
  NODE_SET_METHOD(target, "rmdir", RMDir);
  NODE_SET_METHOD(target, "mkdir", MKDir);
  NODE_SET_METHOD(target, "sendfile", SendFile);
  NODE_SET_METHOD(target, "readdir", ReadDir);
  NODE_SET_METHOD(target, "stat", Stat);
  NODE_SET_METHOD(target, "lstat", LStat);
  NODE_SET_METHOD(target, "link", Link);
  NODE_SET_METHOD(target, "symlink", Symlink);
  NODE_SET_METHOD(target, "readlink", ReadLink);
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "write", Write);
  
  NODE_SET_METHOD(target, "chmod", Chmod);

  errno_symbol = NODE_PSYMBOL("errno");
  encoding_symbol = NODE_PSYMBOL("node:encoding");
}

}  // end namespace node
