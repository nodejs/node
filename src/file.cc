#include "node.h"
#include "file.h"
#include "events.h"
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

using namespace v8;
using namespace node;

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

void
EIOPromise::Attach (void)
{
  ev_ref(EV_DEFAULT_UC);
  Promise::Attach();
}

void
EIOPromise::Detach (void)
{
  Promise::Detach();
  ev_unref(EV_DEFAULT_UC);
}

EIOPromise*
EIOPromise::Create (void)
{
  HandleScope scope;

  Local<Object> handle =
    Promise::constructor_template->GetFunction()->NewInstance();

  EIOPromise *promise = new EIOPromise();
  promise->Wrap(handle);

  promise->Attach();

  return promise;
}

#define NODE_UNIXTIME(t) v8::Date::New(1000*static_cast<double>(t))
int
EIOPromise::After (eio_req *req)
{
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
      argc = 0;
      break;

    case EIO_OPEN:
    case EIO_WRITE:
      argc = 1;
      argv[0] = Integer::New(req->result);
      break;

    case EIO_STAT:
    {
      Local<Object> stats = Object::New();
      struct stat *s = reinterpret_cast<struct stat*>(req->ptr2);
      stats->Set(DEV_SYMBOL, Integer::New(s->st_dev)); /* ID of device containing file */
      stats->Set(INO_SYMBOL, Integer::New(s->st_ino)); /* inode number */
      stats->Set(MODE_SYMBOL, Integer::New(s->st_mode)); /* protection */
      stats->Set(NLINK_SYMBOL, Integer::New(s->st_nlink)); /* number of hard links */
      stats->Set(UID_SYMBOL, Integer::New(s->st_uid)); /* user ID of owner */
      stats->Set(GID_SYMBOL, Integer::New(s->st_gid)); /* group ID of owner */
      stats->Set(RDEV_SYMBOL, Integer::New(s->st_rdev)); /* device ID (if special file) */
      stats->Set(SIZE_SYMBOL, Integer::New(s->st_size)); /* total size, in bytes */
      stats->Set(BLKSIZE_SYMBOL, Integer::New(s->st_blksize)); /* blocksize for filesystem I/O */
      stats->Set(BLOCKS_SYMBOL, Integer::New(s->st_blocks)); /* number of blocks allocated */
      stats->Set(ATIME_SYMBOL, NODE_UNIXTIME(s->st_atime)); /* time of last access */
      stats->Set(MTIME_SYMBOL, NODE_UNIXTIME(s->st_mtime)); /* time of last modification */
      stats->Set(CTIME_SYMBOL, NODE_UNIXTIME(s->st_ctime)); /* time of last status change */
      argc = 1;
      argv[0] = stats;
      break;
    }

    case EIO_READ:
    {
      argc = 2;
      // FIXME the following is really ugly!
      if (promise->encoding_ == RAW) {
        if (req->result == 0) {
          argv[0] = Local<Value>::New(Null());
          argv[1] = Integer::New(0);
        } else {
          char *buf = reinterpret_cast<char*>(req->ptr2);
          size_t len = req->result;
          Local<Array> array = Array::New(len);
          for (unsigned int i = 0; i < len; i++) {
            unsigned char val = reinterpret_cast<const unsigned char*>(buf)[i];
            array->Set(Integer::New(i), Integer::New(val));
          }
          argv[0] = array;
          argv[1] = Integer::New(req->result);
        }
      } else {
        // UTF8
        if (req->result == 0) {
          // eof
          argv[0] = Local<Value>::New(Null());
          argv[1] = Integer::New(0);
        } else {
          char *buf = reinterpret_cast<char*>(req->ptr2);
          argv[0] = String::New(buf, req->result);
          argv[1] = Integer::New(req->result);
        }
      }
      break;
    }

    default:
      assert(0 && "Unhandled eio response");
  }

  promise->EmitSuccess(argc, argv);

  return 0;
}

static Handle<Value>
Close (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsInt32()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  int fd = args[0]->Int32Value();

  return scope.Close(EIOPromise::Close(fd));
}

static Handle<Value>
Stat (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());

  return scope.Close(EIOPromise::Stat(*path));
}

static Handle<Value>
Rename (const Arguments& args)
{
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

static Handle<Value> Unlink (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  return scope.Close(EIOPromise::Unlink(*path));
}

static Handle<Value>
RMDir (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsString()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  String::Utf8Value path(args[0]->ToString());
  return scope.Close(EIOPromise::RMDir(*path));
}

static Handle<Value>
Open (const Arguments& args)
{
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
 */
static Handle<Value>
Write (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  int fd = args[0]->Int32Value();
  off_t offset = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

  char *buf = NULL;
  size_t len = 0;

  if (args[1]->IsString()) {
    // utf8 encoding
    Local<String> string = args[1]->ToString();
    len = string->Utf8Length();
    buf = reinterpret_cast<char*>(malloc(len));
    string->WriteUtf8(buf, len);

  } else if (args[1]->IsArray()) {
    // raw encoding
    Local<Array> array = Local<Array>::Cast(args[1]);
    len = array->Length();
    buf = reinterpret_cast<char*>(malloc(len));
    for (unsigned int i = 0; i < len; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf[i] = int_value->Int32Value();
    }

  } else {
    return ThrowException(BAD_ARGUMENTS);
  }

  return scope.Close(EIOPromise::Write(fd, buf, len, offset));
}

/* node.fs.read(fd, length, position, encoding)
 * Wrapper for read(2).
 *
 * 0 fd        integer. file descriptor
 * 1 length    integer. length to read
 * 2 position  if integer, position to read from in the file.
 *             if null, read from the current position
 * 3 encoding  either node.UTF8 or node.RAW
 */
static Handle<Value>
Read (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 2 || !args[0]->IsInt32() || !args[1]->IsNumber()) {
    return ThrowException(BAD_ARGUMENTS);
  }

  int fd = args[0]->Int32Value();
  size_t len = args[1]->IntegerValue();
  off_t pos = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

  enum encoding encoding = RAW;
  if (args[3]->IsInt32()) {
    encoding = static_cast<enum encoding>(args[3]->Int32Value());
  }

  return scope.Close(EIOPromise::Read(fd, len, pos, encoding));
}

void
File::Initialize (Handle<Object> target)
{
  HandleScope scope;

  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "open", Open);
  NODE_SET_METHOD(target, "read", Read);
  NODE_SET_METHOD(target, "rename", Rename);
  NODE_SET_METHOD(target, "rmdir", RMDir);
  NODE_SET_METHOD(target, "stat", Stat);
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "write", Write);
}
