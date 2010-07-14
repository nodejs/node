// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <node.h>
#include <node_file.h>
#include <node_buffer.h>
#include <node_stat_watcher.h>

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

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(*(a)))

namespace node {

using namespace v8;

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define THROW_BAD_ARGS \
  ThrowException(Exception::TypeError(String::New("Bad argument")))
static Persistent<String> encoding_symbol;
static Persistent<String> errno_symbol;

// Buffer for readlink()  and other misc callers; keep this scoped at
// file-level rather than method-level to avoid excess stack usage.
static char getbuf[PATH_MAX + 1];

static int After(eio_req *req) {
  HandleScope scope;

  Persistent<Function> *callback = cb_unwrap(req->data);

  ev_unref(EV_DEFAULT_UC);

  int argc = 0;
  Local<Value> argv[6];  // 6 is the maximum number of args

  if (req->errorno != 0) {
    argc = 1;
    switch (req->type) {
      case EIO_STAT:
      case EIO_LSTAT:
      case EIO_LINK:
      case EIO_UNLINK:
      case EIO_RMDIR:
      case EIO_RENAME:
      case EIO_READLINK:
      case EIO_OPEN:
      case EIO_CHMOD:
      case EIO_CHOWN:
      case EIO_MKDIR:
        argv[0] = ErrnoException(req->errorno, NULL, "", static_cast<const char*>(req->ptr1));
        break;
      default:
        argv[0] = ErrnoException(req->errorno);
    }
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
      case EIO_FSYNC:
      case EIO_FDATASYNC:
      case EIO_LINK:
      case EIO_SYMLINK:
      case EIO_CHMOD:
      case EIO_CHOWN:
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
      case EIO_FSTAT:
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
        // Buffer interface
        argv[1] = Integer::New(req->result);
        argc = 2;
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

Local<Object> BuildStatsObject(struct stat * s) {
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

  /* blocksize for filesystem I/O */
  stats->Set(blksize_symbol, Integer::New(s->st_blksize));

  /* number of blocks allocated */
  stats->Set(blocks_symbol, Integer::New(s->st_blocks));

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
    struct stat s;
    int ret = stat(*path, &s);
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return scope.Close(BuildStatsObject(&s));
  }
}

static Handle<Value> FStat(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return THROW_BAD_ARGS;
  }

  int fd = args[0]->Int32Value();

  if (args[1]->IsFunction()) {
    ASYNC_CALL(fstat, args[1], fd)
  } else {
    struct stat s;
    int ret = fstat(fd, &s);
    if (ret != 0) return ThrowException(ErrnoException(errno));
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
    if (ret != 0) return ThrowException(ErrnoException(errno));
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *orig_path));
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
    ssize_t bz = readlink(*path, getbuf, ARRAY_SIZE(getbuf) - 1);
    if (bz == -1) return ThrowException(ErrnoException(errno, NULL, "", *path));
    getbuf[ARRAY_SIZE(getbuf) - 1] = '\0';
    return scope.Close(String::New(getbuf, bz));
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
    int ret = fsync(fd);
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
    int ret = mkdir(*path, mode);
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

  Buffer * buffer = ObjectWrap::Unwrap<Buffer>(args[1]->ToObject());

  size_t off = args[2]->Int32Value();
  if (off >= buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Offset is out of bounds")));
  }

  ssize_t len = args[3]->Int32Value();
  if (off + len > buffer->length()) {
    return ThrowException(Exception::Error(
          String::New("Length is extends beyond buffer")));
  }

  off_t pos = GET_OFFSET(args[4]);

  char * buf = (char*)buffer->data() + off;
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

  pos = GET_OFFSET(args[4]);

  buf = (char*)buffer->data() + off;

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


/* fs.chmod(fd, mode);
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return Undefined();
  }
}


/* fs.chown(fd, uid, gid);
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
    if (ret != 0) return ThrowException(ErrnoException(errno, NULL, "", *path));
    return Undefined();
  }
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
  NODE_SET_METHOD(target, "lstat", LStat);
  NODE_SET_METHOD(target, "fstat", FStat);
  NODE_SET_METHOD(target, "link", Link);
  NODE_SET_METHOD(target, "symlink", Symlink);
  NODE_SET_METHOD(target, "readlink", ReadLink);
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "write", Write);

  NODE_SET_METHOD(target, "chmod", Chmod);
  NODE_SET_METHOD(target, "chown", Chown);

  errno_symbol = NODE_PSYMBOL("errno");
  encoding_symbol = NODE_PSYMBOL("node:encoding");
}

void InitFs(Handle<Object> target) {
  HandleScope scope;
  // Initialize the stats object
  Local<FunctionTemplate> stat_templ = FunctionTemplate::New();
  stats_constructor_template = Persistent<FunctionTemplate>::New(stat_templ);
  target->Set(String::NewSymbol("Stats"),
               stats_constructor_template->GetFunction());
  StatWatcher::Initialize(target);
  File::Initialize(target);
}

}  // end namespace node

NODE_MODULE(node_fs, node::InitFs);
