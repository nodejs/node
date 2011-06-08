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

#include <node.h>
#include <node_file.h>
#include <node_buffer.h>
#ifdef __POSIX__
# include <node_stat_watcher.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef __MINGW32__
# include <platform_win32.h>
#endif

/* used for readlink, AIX doesn't provide it */
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* HACK to use pread/pwrite from eio because MINGW32 doesn't have it */
/* TODO fixme */
#ifdef __MINGW32__
# define pread  eio__pread
# define pwrite eio__pwrite
#endif

namespace node {

using namespace v8;

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define THROW_BAD_ARGS \
  ThrowException(Exception::TypeError(String::New("Bad argument")))

static Persistent<String> encoding_symbol;
static Persistent<String> errno_symbol;
static Persistent<String> buf_symbol;

// Buffer for readlink()  and other misc callers; keep this scoped at
// file-level rather than method-level to avoid excess stack usage.
// Not used on windows atm
#ifdef __POSIX__
  static char getbuf[PATH_MAX + 1];
#endif


static inline bool SetCloseOnExec(int fd) {
#ifdef __POSIX__
  return (fcntl(fd, F_SETFD, FD_CLOEXEC) != -1);
#else // __MINGW32__
  return SetHandleInformation(reinterpret_cast<HANDLE>(_get_osfhandle(fd)),
                              HANDLE_FLAG_INHERIT, 0) != 0;
#endif
}


static int After(eio_req *req) {
  HandleScope scope;

  Persistent<Function> *callback = cb_unwrap(req->data);

  uv_unref();

  // there is always at least one argument. "error"
  int argc = 1;

  // Allocate space for two args. We may only use one depending on the case.
  // (Feel free to increase this if you need more)
  Local<Value> argv[2];

  // NOTE: This may be needed to be changed if something returns a -1
  // for a success, which is possible.
  if (req->result == -1) {
    // If the request doesn't have a path parameter set.
    if (!req->ptr1) {
      argv[0] = ErrnoException(req->errorno);
    } else {
      argv[0] = ErrnoException(req->errorno, NULL, "", static_cast<const char*>(req->ptr1));
    }
  } else {
    // error value is empty or null for non-error.
    argv[0] = Local<Value>::New(Null());

    // All have at least two args now.
    argc = 2;

    switch (req->type) {
      // These all have no data to pass.
      case EIO_CLOSE:
      case EIO_RENAME:
      case EIO_UNLINK:
      case EIO_RMDIR:
      case EIO_MKDIR:
      case EIO_FTRUNCATE:
      case EIO_FSYNC:
      case EIO_FDATASYNC:
      case EIO_LINK:
      case EIO_SYMLINK:
      case EIO_CHMOD:
      case EIO_FCHMOD:
      case EIO_CHOWN:
      case EIO_FCHOWN:
        // These, however, don't.
        argc = 1;
        break;

      case EIO_UTIME:
      case EIO_FUTIME:
        argc = 0;
        break;

      case EIO_OPEN:
        SetCloseOnExec(req->result);
        /* pass thru */
      case EIO_SENDFILE:
        argv[1] = Integer::New(req->result);
        break;

      case EIO_WRITE:
        argv[1] = Integer::New(req->result);
        break;

      case EIO_STAT:
      case EIO_LSTAT:
      case EIO_FSTAT:
        {
          NODE_STAT_STRUCT *s = reinterpret_cast<NODE_STAT_STRUCT*>(req->ptr2);
          argv[1] = BuildStatsObject(s);
        }
        break;

      case EIO_READLINK:
        argv[1] = String::New(static_cast<char*>(req->ptr2), req->result);
        break;

      case EIO_READ:
        // Buffer interface
        argv[1] = Integer::New(req->result);
        break;

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

          argv[1] = names;
        }
        break;

      default:
        assert(0 && "Unhandled eio response");
    }
  }

  TryCatch try_catch;

  (*callback)->Call(v8::Context::GetCurrent()->Global(), argc, argv);

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
  uv_ref();                                          \
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
    if (ret != 0) return ThrowException(ErrnoException(errno));
    return Undefined();
  }
}


static Persistent<FunctionTemplate> stats_constructor_template;

static Persistent<String> dev_symbol;
static Persistent<String> ino_symbol;
static Persistent<String> mode_symbol;
static Persistent<String> nlink_symbol;
static Persistent<String> uid_symbol;
static Persistent<String> gid_symbol;
static Persistent<String> rdev_symbol;
static Persistent<String> size_symbol;
static Persistent<String> blksize_symbol;
static Persistent<String> blocks_symbol;
static Persistent<String> atime_symbol;
static Persistent<String> mtime_symbol;
static Persistent<String> ctime_symbol;

Local<Object> BuildStatsObject(NODE_STAT_STRUCT *s) {
  HandleScope scope;

  if (dev_symbol.IsEmpty()) {
    dev_symbol = NODE_PSYMBOL("dev");
    ino_symbol = NODE_PSYMBOL("ino");
    mode_symbol = NODE_PSYMBOL("mode");
    nlink_symbol = NODE_PSYMBOL("nlink");
    uid_symbol = NODE_PSYMBOL("uid");
    gid_symbol = NODE_PSYMBOL("gid");
    rdev_symbol = NODE_PSYMBOL("rdev");
    size_symbol = NODE_PSYMBOL("size");
    blksize_symbol = NODE_PSYMBOL("blksize");
    blocks_symbol = NODE_PSYMBOL("blocks");
    atime_symbol = NODE_PSYMBOL("atime");
    mtime_symbol = NODE_PSYMBOL("mtime");
    ctime_symbol = NODE_PSYMBOL("ctime");
  }

  Local<Object> stats =
    stats_constructor_template->GetFunction()->NewInstance();

  /* ID of device containing file */
  stats->Set(dev_symbol, Integer::New(s->st_dev));

  /* inode number */
  stats->Set(ino_symbol, Integer::New(s->st_ino));

  /* protection */
  stats->Set(mode_symbol, Integer::New(s->st_mode));

  /* number of hard links */
  stats->Set(nlink_symbol, Integer::New(s->st_nlink));

  /* user ID of owner */
  stats->Set(uid_symbol, Integer::New(s->st_uid));

  /* group ID of owner */
  stats->Set(gid_symbol, Integer::New(s->st_gid));

  /* device ID (if special file) */
  stats->Set(rdev_symbol, Integer::New(s->st_rdev));

  /* total size, in bytes */
  stats->Set(size_symbol, Number::New(s->st_size));

#ifdef __POSIX__
  /* blocksize for filesystem I/O */
  stats->Set(blksize_symbol, Integer::New(s->st_blksize));

  /* number of blocks allocated */
  stats->Set(blocks_symbol, Integer::New(s->st_blocks));
#endif

  /* time of last access */
  stats->Set(atime_symbol, NODE_UNIXTIME_V8(s->st_atime));

  /* time of last modification */
  stats->Set(mtime_symbol, NODE_UNIXTIME_V8(s->st_mtime));

  /* time of last status change */
  stats->Set(ctime_symbol, NODE_UNIXTIME_V8(s->st_ctime));

  return scope.Close(stats);
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
    NODE_STAT_STRUCT s;
    int ret = NODE_STAT(*path, &s);
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return scope.Close(BuildStatsObject(&s));
  }
}

#ifdef __POSIX__
static Handle<Value> LStat(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(lstat, args[1], *path)
  } else {
    NODE_STAT_STRUCT s;
    int ret = lstat(*path, &s);
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return scope.Close(BuildStatsObject(&s));
  }
}
#endif // __POSIX__

static Handle<Value> FStat(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fstat, args[1], fd)
  } else {
    NODE_STAT_STRUCT s;
    int ret = NODE_FSTAT(fd, &s);
    if (ret != 0) return ThrowException(ErrnoException(errno));
    return scope.Close(BuildStatsObject(&s));
  }
}

#ifdef __POSIX__
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
    if (ret != 0) return ThrowException(ErrnoException(errno));
    return Undefined();
  }
}
#endif // __POSIX__

#ifdef __POSIX__
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *orig_path));
    return Undefined();
  }
}
#endif // __POSIX__

#ifdef __POSIX__
static Handle<Value> ReadLink(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  String::Utf8Value path(args[0]->ToString());

  if (args[1]->IsFunction()) {
    ASYNC_CALL(readlink, args[1], *path)
  } else {
    ssize_t bz = readlink(*path, getbuf, ARRAY_SIZE(getbuf) - 1);
    if (bz == -1) return ThrowException(ErrnoException(errno, NULL, "", *path));
    getbuf[ARRAY_SIZE(getbuf) - 1] = '\0';
    return scope.Close(String::New(getbuf, bz));
  }
}
#endif // __POSIX__

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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *old_path));
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
    if (ret != 0) return ThrowException(ErrnoException(errno));
    return Undefined();
  }
}

static Handle<Value> Fdatasync(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fdatasync, args[1], fd)
  } else {
#if HAVE_FDATASYNC
    int ret = fdatasync(fd);
#elif defined(__MINGW32__)
    int ret = FlushFileBuffers((HANDLE)_get_osfhandle(fd)) ? 0 : -1;
#else
    int ret = fsync(fd);
#endif
    if (ret != 0) return ThrowException(ErrnoException(errno));
    return Undefined();
  }
}

static Handle<Value> Fsync(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fsync, args[1], fd)
  } else {
#ifdef __MINGW32__
    int ret = FlushFileBuffers((HANDLE)_get_osfhandle(fd)) ? 0 : -1;
#else
    int ret = fsync(fd);
#endif
    if (ret != 0) return ThrowException(ErrnoException(errno));
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
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
#ifdef __MINGW32__
    int ret = mkdir(*path);
#else
    int ret = mkdir(*path, mode);
#endif
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return Undefined();
  }
}

static Handle<Value> SendFile(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4 ||
      !args[0]->IsUint32() ||
      !args[1]->IsUint32() ||
      !args[2]->IsUint32() ||
      !args[3]->IsUint32()) {
    return THROW_BAD_ARGS;
  }

  int out_fd = args[0]->Uint32Value();
  int in_fd = args[1]->Uint32Value();
  off_t in_offset = args[2]->Uint32Value();
  size_t length = args[3]->Uint32Value();

  if (args[4]->IsFunction()) {
    ASYNC_CALL(sendfile, args[4], out_fd, in_fd, in_offset, length)
  } else {
    ssize_t sent = eio_sendfile_sync (out_fd, in_fd, in_offset, length);
    // XXX is this the right errno to use?
    if (sent < 0) return ThrowException(ErrnoException(errno));
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
    if (!dir) return ThrowException(ErrnoException(errno, NULL, "", *path));

    struct dirent *ent;

    Local<Array> files = Array::New();
    char *name;
    int i = 0;

    while ((ent = readdir(dir))) {
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
    SetCloseOnExec(fd);
    if (fd < 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return scope.Close(Integer::New(fd));
  }
}

#define GET_OFFSET(a) (a)->IsInt32() ? (a)->IntegerValue() : -1;

// bytesWritten = write(fd, data, position, enc, callback)
// Wrapper for write(2).
//
// 0 fd        integer. file descriptor
// 1 buffer    the data to write
// 2 offset    where in the buffer to start from
// 3 length    how much to write
// 4 position  if integer, position to write at in the file.
//             if null, write from the current position
static Handle<Value> Write(const Arguments& args) {
  HandleScope scope;

  if (!args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (!Buffer::HasInstance(args[1])) {
    return ThrowException(Exception::Error(
                String::New("Second argument needs to be a buffer")));
  }

  Local<Object> buffer_obj = args[1]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[2]->Int32Value();
  if (off >= buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  ssize_t len = args[3]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  off_t pos = GET_OFFSET(args[4]);

  char * buf = (char*)buffer_data + off;
  Local<Value> cb = args[5];

  if (cb->IsFunction()) {

    ASYNC_CALL(write, cb, fd, buf, len, pos)
  } else {
    ssize_t written = pos < 0 ? write(fd, buf, len) : pwrite(fd, buf, len, pos);
    if (written < 0) return ThrowException(ErrnoException(errno, "write"));
    return scope.Close(Integer::New(written));
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
static Handle<Value> Read(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  Local<Value> cb;

  size_t len;
  off_t pos;

  char * buf = NULL;

  if (!Buffer::HasInstance(args[1])) {
    return ThrowException(Exception::Error(
                String::New("Second argument needs to be a buffer")));
  }

  Local<Object> buffer_obj = args[1]->ToObject();
  char *buffer_data = Buffer::Data(buffer_obj);
  size_t buffer_length = Buffer::Length(buffer_obj);

  size_t off = args[2]->Int32Value();
  if (off >= buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  len = args[3]->Int32Value();
  if (off + len > buffer_length) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  pos = GET_OFFSET(args[4]);

  buf = buffer_data + off;

  cb = args[5];

  if (cb->IsFunction()) {

    ASYNC_CALL(read, cb, fd, buf, len, pos);
  } else {
    // SYNC
    ssize_t ret;

    ret = pos < 0 ? read(fd, buf, len) : pread(fd, buf, len, pos);
    if (ret < 0) return ThrowException(ErrnoException(errno));
    Local<Integer> bytesRead = Integer::New(ret);
    return scope.Close(bytesRead);
  }
}


/* fs.chmod(path, mode);
 * Wrapper for chmod(1) / EIO_CHMOD
 */
static Handle<Value> Chmod(const Arguments& args) {
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
    if (ret != 0) return ThrowException(ErrnoException(errno, "chmod", "", *path));
    return Undefined();
  }
}


#ifdef __POSIX__
/* fs.fchmod(fd, mode);
 * Wrapper for fchmod(1) / EIO_FCHMOD
 */
static Handle<Value> FChmod(const Arguments& args) {
  HandleScope scope;

  if(args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsInt32()) {
    return THROW_BAD_ARGS;
  }
  int fd = args[0]->Int32Value();
  mode_t mode = static_cast<mode_t>(args[1]->Int32Value());

  if(args[2]->IsFunction()) {
    ASYNC_CALL(fchmod, args[2], fd, mode);
  } else {
    int ret = fchmod(fd, mode);
    if (ret != 0) return ThrowException(ErrnoException(errno, "fchmod", "", 0));
    return Undefined();
  }
}
#endif // __POSIX__


#ifdef __POSIX__
/* fs.chown(path, uid, gid);
 * Wrapper for chown(1) / EIO_CHOWN
 */
static Handle<Value> Chown(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3 || !args[0]->IsString()) {
    return THROW_BAD_ARGS;
  }

  if (!args[1]->IsInt32() || !args[2]->IsInt32()) {
    return ThrowException(Exception::Error(String::New("User and Group IDs must be an integer.")));
  }

  String::Utf8Value path(args[0]->ToString());
  uid_t uid = static_cast<uid_t>(args[1]->Int32Value());
  gid_t gid = static_cast<gid_t>(args[2]->Int32Value());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(chown, args[3], *path, uid, gid);
  } else {
    int ret = chown(*path, uid, gid);
    if (ret != 0) return ThrowException(ErrnoException(errno, "chown", "", *path));
    return Undefined();
  }
}
#endif // __POSIX__


#ifdef __POSIX__
/* fs.fchown(fd, uid, gid);
 * Wrapper for fchown(1) / EIO_FCHOWN
 */
static Handle<Value> FChown(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  if (!args[1]->IsInt32() || !args[2]->IsInt32()) {
    return ThrowException(Exception::Error(String::New("User and Group IDs must be an integer.")));
  }

  int fd = args[0]->Int32Value();
  uid_t uid = static_cast<uid_t>(args[1]->Int32Value());
  gid_t gid = static_cast<gid_t>(args[2]->Int32Value());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(fchown, args[3], fd, uid, gid);
  } else {
    int ret = fchown(fd, uid, gid);
    if (ret != 0) return ThrowException(ErrnoException(errno, "fchown", "", 0));
    return Undefined();
  }
}
#endif // __POSIX__


// Utimes() and Futimes() helper function, converts 123.456 timestamps to timevals
static inline void ToTimevals(eio_tstamp atime,
                              eio_tstamp mtime,
                              timeval times[2]) {
  times[0].tv_sec  = atime;
  times[0].tv_usec = 10e5 * (atime - (long) atime);
  times[1].tv_sec  = mtime;
  times[1].tv_usec = 10e5 * (mtime - (long) mtime);
}


#ifdef __POSIX__
static Handle<Value> UTimes(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3
      || !args[0]->IsString()
      || !args[1]->IsNumber()
      || !args[2]->IsNumber())
  {
    return THROW_BAD_ARGS;
  }

  const String::Utf8Value path(args[0]->ToString());
  const eio_tstamp atime = static_cast<eio_tstamp>(args[1]->NumberValue());
  const eio_tstamp mtime = static_cast<eio_tstamp>(args[2]->NumberValue());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(utime, args[3], *path, atime, mtime);
  } else {
    timeval times[2];

    ToTimevals(atime, mtime, times);
    if (utimes(*path, times) == -1) {
      return ThrowException(ErrnoException(errno, "utimes", "", *path));
    }
  }

  return Undefined();
}
#endif // __POSIX__


static Handle<Value> FUTimes(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 3
      || !args[0]->IsInt32()
      || !args[1]->IsNumber()
      || !args[2]->IsNumber())
  {
    return THROW_BAD_ARGS;
  }

  const int fd = args[0]->Int32Value();
  const eio_tstamp atime = static_cast<eio_tstamp>(args[1]->NumberValue());
  const eio_tstamp mtime = static_cast<eio_tstamp>(args[2]->NumberValue());

  if (args[3]->IsFunction()) {
    ASYNC_CALL(futime, args[3], fd, atime, mtime);
  } else {
#ifndef futimes
    // Some systems do not have futimes
    return ThrowException(ErrnoException(ENOSYS, "futimes", "", 0));
#else
    timeval times[2];

    ToTimevals(atime, mtime, times);
    if (futimes(fd, times) == -1) {
      return ThrowException(ErrnoException(errno, "futimes", "", 0));
    }
#endif  // futimes
  }

  return Undefined();
}


void File::Initialize(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "open", Open);
  NODE_SET_METHOD(target, "read", Read);
  NODE_SET_METHOD(target, "fdatasync", Fdatasync);
  NODE_SET_METHOD(target, "fsync", Fsync);
  NODE_SET_METHOD(target, "rename", Rename);
  NODE_SET_METHOD(target, "truncate", Truncate);
  NODE_SET_METHOD(target, "rmdir", RMDir);
  NODE_SET_METHOD(target, "mkdir", MKDir);
  NODE_SET_METHOD(target, "sendfile", SendFile);
  NODE_SET_METHOD(target, "readdir", ReadDir);
  NODE_SET_METHOD(target, "stat", Stat);
#ifdef __POSIX__
  NODE_SET_METHOD(target, "lstat", LStat);
#endif // __POSIX__
  NODE_SET_METHOD(target, "fstat", FStat);
#ifdef __POSIX__
  NODE_SET_METHOD(target, "link", Link);
  NODE_SET_METHOD(target, "symlink", Symlink);
  NODE_SET_METHOD(target, "readlink", ReadLink);
#endif // __POSIX__
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "write", Write);

  NODE_SET_METHOD(target, "chmod", Chmod);
#ifdef __POSIX__
  NODE_SET_METHOD(target, "fchmod", FChmod);
  //NODE_SET_METHOD(target, "lchmod", LChmod);

  NODE_SET_METHOD(target, "chown", Chown);
  NODE_SET_METHOD(target, "fchown", FChown);
  //NODE_SET_METHOD(target, "lchown", LChown);

  NODE_SET_METHOD(target, "utimes", UTimes);
#endif // __POSIX__
  NODE_SET_METHOD(target, "futimes", FUTimes);

  errno_symbol = NODE_PSYMBOL("errno");
  encoding_symbol = NODE_PSYMBOL("node:encoding");
  buf_symbol = NODE_PSYMBOL("__buf");
}

void InitFs(Handle<Object> target) {
  HandleScope scope;
  // Initialize the stats object
  Local<FunctionTemplate> stat_templ = FunctionTemplate::New();
  stats_constructor_template = Persistent<FunctionTemplate>::New(stat_templ);
  target->Set(String::NewSymbol("Stats"),
               stats_constructor_template->GetFunction());
  File::Initialize(target);

#ifdef __POSIX__
  StatWatcher::Initialize(target);
#endif

#ifdef __MINGW32__
  // Open files in binary mode by default
  _fmode = _O_BINARY;
#endif
}

}  // end namespace node

NODE_MODULE(node_fs, node::InitFs);
