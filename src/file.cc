// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#include <file.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

namespace node {

using namespace v8;

#define DEV_SYMBOL         String::NewSymbol("dev")
#define INO_SYMBOL         String::NewSymbol("ino")
#define MODE_SYMBOL        String::NewSymbol("mode")
#define NLINK_SYMBOL       String::NewSymbol("nlink")
#define UID_SYMBOL         String::NewSymbol("uid")
#define GID_SYMBOL         String::NewSymbol("gid")
#define RDEV_SYMBOL        String::NewSymbol("rdev")
#define SIZE_SYMBOL        String::NewSymbol("size")
#define BLKSIZE_SYMBOL     String::NewSymbol("blksize")
#define BLOCKS_SYMBOL      String::NewSymbol("blocks")
#define ATIME_SYMBOL       String::NewSymbol("atime")
#define MTIME_SYMBOL       String::NewSymbol("mtime")
#define CTIME_SYMBOL       String::NewSymbol("ctime")
#define BAD_ARGUMENTS      Exception::TypeError(String::New("Bad argument"))

void EIOPromise::Attach(void) {
  ev_ref(EV_DEFAULT_UC);
  Promise::Attach();
}

void EIOPromise::Detach(void) {
  Promise::Detach();
  ev_unref(EV_DEFAULT_UC);
}

Handle<Value> EIOPromise::New(const v8::Arguments& args) {
  HandleScope scope;

  EIOPromise *promise = new EIOPromise();
  promise->Wrap(args.This());

  promise->Attach();

  return args.This();
}

EIOPromise* EIOPromise::Create() {
  HandleScope scope;

  Local<Object> handle =
    EIOPromise::constructor_template->GetFunction()->NewInstance();

  return ObjectWrap::Unwrap<EIOPromise>(handle);
}

static Persistent<FunctionTemplate> stats_constructor_template;

static Local<Object> BuildStatsObject(struct stat * s) {
  HandleScope scope;

  Local<Object> stats =
    stats_constructor_template->GetFunction()->NewInstance();

  /* ID of device containing file */
  stats->Set(DEV_SYMBOL, Integer::New(s->st_dev));

  /* inode number */
  stats->Set(INO_SYMBOL, Integer::New(s->st_ino));

  /* protection */
  stats->Set(MODE_SYMBOL, Integer::New(s->st_mode));

  /* number of hard links */
  stats->Set(NLINK_SYMBOL, Integer::New(s->st_nlink));

  /* user ID of owner */
  stats->Set(UID_SYMBOL, Integer::New(s->st_uid));

  /* group ID of owner */
  stats->Set(GID_SYMBOL, Integer::New(s->st_gid));

  /* device ID (if special file) */
  stats->Set(RDEV_SYMBOL, Integer::New(s->st_rdev));

  /* total size, in bytes */
  stats->Set(SIZE_SYMBOL, Integer::New(s->st_size));

  /* blocksize for filesystem I/O */
  stats->Set(BLKSIZE_SYMBOL, Integer::New(s->st_blksize));

  /* number of blocks allocated */
  stats->Set(BLOCKS_SYMBOL, Integer::New(s->st_blocks));

  /* time of last access */
  stats->Set(ATIME_SYMBOL, NODE_UNIXTIME_V8(s->st_atime));

  /* time of last modification */
  stats->Set(MTIME_SYMBOL, NODE_UNIXTIME_V8(s->st_mtime));

  /* time of last status change */
  stats->Set(CTIME_SYMBOL, NODE_UNIXTIME_V8(s->st_ctime));

  return scope.Close(stats);
}

int EIOPromise::After(eio_req *req) {
  HandleScope scope;

  EIOPromise *promise = reinterpret_cast<EIOPromise*>(req->data);
  assert(req == promise->req_);

  if (req->errorno != 0) {
    Local<Value> exception = Exception::Error(
        String::NewSymbol(strerror(req->errorno)));
    promise->EmitError(1, &exception);
    return 0;
  }

  int argc = 0;
  Local<Value> argv[5];  // 5 is the maximum number of args

  switch (req->type) {
    case EIO_CLOSE:
    case EIO_RENAME:
    case EIO_UNLINK:
    case EIO_RMDIR:
    case EIO_MKDIR:
      argc = 0;
      break;

    case EIO_OPEN:
    case EIO_SENDFILE:
      argc = 1;
      argv[0] = Integer::New(req->result);
      break;

    case EIO_WRITE:
      argc = 1;
      argv[0] = Integer::New(req->result);
      assert(req->ptr2);
      delete [] req->ptr2;
      break;

    case EIO_STAT:
    {
      struct stat *s = reinterpret_cast<struct stat*>(req->ptr2);
      argc = 1;
      argv[0] = BuildStatsObject(s);
      break;
    }

    case EIO_READ:
    {
      argc = 2;
      argv[0] = Encode(req->ptr2, req->result, promise->encoding_);
      argv[1] = Integer::New(req->result);
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

      argc = 1;
      argv[0] = names;
      break;
    }

    default:
      assert(0 && "Unhandled eio response");
  }

  promise->EmitSuccess(argc, argv);

  return 0;
}

static Handle<Value> Close(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  int fd = args[0]->Int32Value();

  return scope.Close(EIOPromise::Close(fd));
}

static Handle<Value> Stat(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());

  return scope.Close(EIOPromise::Stat(*path));
}

static Handle<Value> Rename(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());

  return scope.Close(EIOPromise::Rename(*path, *new_path));
  Promise *promise = EIOPromise::Create();

  return scope.Close(promise->Handle());
}

static Handle<Value> Unlink(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  return scope.Close(EIOPromise::Unlink(*path));
}

static Handle<Value> RMDir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  return scope.Close(EIOPromise::RMDir(*path));
}

static Handle<Value> MKDir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsInt32()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  mode_t mode = static_cast<mode_t>(args[1]->Int32Value());

  return scope.Close(EIOPromise::MKDir(*path, mode));
}

static Handle<Value> SendFile(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 4 || !args[0]->IsInt32() || !args[1]->IsInt32() || !args[3]->IsNumber()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  int out_fd = args[0]->Int32Value();
  int in_fd = args[1]->Int32Value();
  off_t in_offset = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;
  size_t length = args[3]->IntegerValue();

  return scope.Close(EIOPromise::SendFile(out_fd, in_fd, in_offset, length));
}

static Handle<Value> ReadDir(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  return scope.Close(EIOPromise::ReadDir(*path));
}

static Handle<Value> Open(const Arguments& args) {
  HandleScope scope;

  if ( args.Length() < 3
    || !args[0]->IsString()
    || !args[1]->IsInt32()
    || !args[2]->IsInt32()
     ) return ThrowException(BAD_ARGUMENTS);

  String::Utf8Value path(args[0]->ToString());
  int flags = args[1]->Int32Value();
  mode_t mode = static_cast<mode_t>(args[2]->Int32Value());

  return scope.Close(EIOPromise::Open(*path, flags, mode));
}

/* node.fs.write(fd, data, position=null)
 * Wrapper for write(2).
 *
 * 0 fd        integer. file descriptor
 * 1 data      the data to write (string = utf8, array = raw)
 * 2 position  if integer, position to write at in the file.
 *             if null, write from the current position
 * 3 encoding  
 */
static Handle<Value> Write(const Arguments& args) {
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  int fd = args[0]->Int32Value();
  off_t offset = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

  enum encoding enc = ParseEncoding(args[3]);
  ssize_t len = DecodeBytes(args[1], enc);
  if (len < 0) {
    Local<Value> exception = Exception::TypeError(String::New("Bad argument"));
    return ThrowException(exception);
  }

  char * buf = new char[len];
  ssize_t written = DecodeWrite(buf, len, args[1], enc);
  assert(written == len);

  return scope.Close(EIOPromise::Write(fd, buf, len, offset));
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
    return ThrowException(BAD_ARGUMENTS);
  }

  int fd = args[0]->Int32Value();
  size_t len = args[1]->IntegerValue();
  off_t pos = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

  enum encoding encoding = ParseEncoding(args[3]);

  return scope.Close(EIOPromise::Read(fd, len, pos, encoding));
}

Persistent<FunctionTemplate> EIOPromise::constructor_template;

void File::Initialize(Handle<Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "open", Open);
  NODE_SET_METHOD(target, "read", Read);
  NODE_SET_METHOD(target, "rename", Rename);
  NODE_SET_METHOD(target, "rmdir", RMDir);
  NODE_SET_METHOD(target, "mkdir", MKDir);
  NODE_SET_METHOD(target, "sendfile", SendFile);
  NODE_SET_METHOD(target, "readdir", ReadDir);
  NODE_SET_METHOD(target, "stat", Stat);
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "write", Write);

  Local<FunctionTemplate> t = FunctionTemplate::New();
  stats_constructor_template = Persistent<FunctionTemplate>::New(t);
  target->Set(String::NewSymbol("Stats"),
      stats_constructor_template->GetFunction());


  Local<FunctionTemplate> t2 = FunctionTemplate::New(EIOPromise::New);
  EIOPromise::constructor_template = Persistent<FunctionTemplate>::New(t2);
  EIOPromise::constructor_template->Inherit(
      Promise::constructor_template);
  EIOPromise::constructor_template->InstanceTemplate()->
    SetInternalFieldCount(1);
  EIOPromise::constructor_template->SetClassName(
      String::NewSymbol("EIOPromise"));
  target->Set(String::NewSymbol("EIOPromise"),
      EIOPromise::constructor_template->GetFunction());
}

}  // end namespace node
