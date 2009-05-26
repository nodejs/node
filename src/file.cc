#include "node.h"
#include "file.h"
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
#define BAD_ARGUMENTS      String::New("Bad argument")

#define MAKE_CALLBACK_PTR                                             \
  Persistent<Function> *callback = NULL;                              \
  Local<Value> last_arg = args[args.Length()-1];                      \
  if (last_arg->IsFunction()) {                                       \
    Local<Function> l = Local<Function>::Cast(last_arg);              \
    callback = new Persistent<Function>();                            \
    *callback = Persistent<Function>::New(l);                         \
  }                                                                   \
  node::eio_warmup();                                                 \

#define CALL_CALLBACK_PTR(req, argc, argv)                            \
do {                                                                  \
  if (req->data) {                                                    \
    Persistent<Function> *callback =                                  \
      reinterpret_cast<Persistent<Function>*>(req->data);             \
    TryCatch try_catch;                                               \
    (*callback)->Call(Context::GetCurrent()->Global(), argc, argv);   \
    if(try_catch.HasCaught())                                         \
      node::FatalException(try_catch);                               \
    free(callback);                                                   \
  }                                                                   \
} while(0)

#define DEFINE_SIMPLE_CB(name)                                        \
static int After##name (eio_req *req)                                 \
{                                                                     \
  HandleScope scope;                                                  \
  Local<Value> argv[] = { Integer::New(req->errorno) };               \
  CALL_CALLBACK_PTR(req, 1, argv);                                    \
  return 0;                                                           \
}                                                                     \

DEFINE_SIMPLE_CB(Close)
static Handle<Value> Close (const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsInt32())
    return ThrowException(BAD_ARGUMENTS);
  HandleScope scope;
  int fd = args[0]->Int32Value();
  MAKE_CALLBACK_PTR
  eio_close(fd, EIO_PRI_DEFAULT, AfterClose, callback);
  return Undefined();
}

DEFINE_SIMPLE_CB(Rename)
static Handle<Value> Rename (const Arguments& args)
{
  if (args.Length() < 2 || !args[0]->IsString() || !args[1]->IsString())
    return ThrowException(BAD_ARGUMENTS);
  HandleScope scope;
  String::Utf8Value path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());
  MAKE_CALLBACK_PTR
  eio_rename(*path, *new_path, EIO_PRI_DEFAULT, AfterRename, callback);
  return Undefined();
}

DEFINE_SIMPLE_CB(Unlink)
static Handle<Value> Unlink (const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsString())
    return ThrowException(BAD_ARGUMENTS);
  HandleScope scope;
  String::Utf8Value path(args[0]->ToString());
  MAKE_CALLBACK_PTR
  eio_unlink(*path, EIO_PRI_DEFAULT, AfterUnlink, callback);
  return Undefined();
}

DEFINE_SIMPLE_CB(RMDir)
static Handle<Value> RMDir (const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsString())
    return ThrowException(BAD_ARGUMENTS);
  HandleScope scope;
  String::Utf8Value path(args[0]->ToString());
  MAKE_CALLBACK_PTR
  eio_rmdir(*path, EIO_PRI_DEFAULT, AfterRMDir, callback);
  return Undefined();
}

static int
AfterOpen (eio_req *req)
{
  HandleScope scope;
  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  argv[1] = Integer::New(req->result);
  CALL_CALLBACK_PTR(req, argc, argv);
  return 0;
}

static Handle<Value>
Open (const Arguments& args)
{
  if ( args.Length() < 3 
    || !args[0]->IsString() 
    || !args[1]->IsInt32() 
    || !args[2]->IsInt32()
     ) return ThrowException(BAD_ARGUMENTS);

  HandleScope scope;
  String::Utf8Value path(args[0]->ToString());
  int flags = args[1]->Int32Value();
  mode_t mode = static_cast<mode_t>(args[2]->Int32Value());

  MAKE_CALLBACK_PTR

  eio_open(*path, flags, mode, EIO_PRI_DEFAULT, AfterOpen, callback);
  return Undefined();
}

static int
AfterWrite (eio_req *req)
{
  HandleScope scope;

  free(req->ptr2);

  ssize_t written = req->result;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  argv[1] = written >= 0 ? Integer::New(written) : Integer::New(0);

  CALL_CALLBACK_PTR(req, argc, argv);
  return 0;
}


/* node.fs.write(fd, data, position, callback)
 * Wrapper for write(2). 
 *
 * 0 fd        integer. file descriptor
 * 1 data      the data to write (string = utf8, array = raw)
 * 2 position  if integer, position to write at in the file.
 *             if null, write from the current position
 *
 * 3 callback(errorno, written)
 */
static Handle<Value>
Write (const Arguments& args)
{
  if ( args.Length() < 3 
    || !args[0]->IsInt32() 
     ) return ThrowException(BAD_ARGUMENTS);

  HandleScope scope;

  int fd = args[0]->Int32Value();
  off_t pos = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

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

  MAKE_CALLBACK_PTR
  eio_write(fd, buf, len, pos, EIO_PRI_DEFAULT, AfterWrite, callback);
  return Undefined();
}

static int
AfterUtf8Read (eio_req *req)
{
  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  char *buf = reinterpret_cast<char*>(req->ptr2);
  if (req->result == 0) { 
    // eof 
    argv[1] = Local<Value>::New(Null());
  } else {
    argv[1] = String::New(buf, req->result);
  }

  CALL_CALLBACK_PTR(req, argc, argv);
  return 0;
}

static int
AfterRawRead(eio_req *req)
{
  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  char *buf = reinterpret_cast<char*>(req->ptr2);

  if (req->result == 0) { 
    // eof 
    argv[1] = Local<Value>::New(Null());
  } else {
    // raw encoding
    size_t len = req->result;
    Local<Array> array = Array::New(len);
    for (unsigned int i = 0; i < len; i++) {
      array->Set(Integer::New(i), Integer::New(buf[i]));
    }
    argv[1] = array;
  }

  CALL_CALLBACK_PTR(req, argc, argv);
  return 0;
}

/* node.fs.read(fd, length, position, encoding, callback)
 * Wrapper for read(2). 
 *
 * 0 fd        integer. file descriptor
 * 1 length    integer. length to read
 * 2 position  if integer, position to read from in the file.
 *             if null, read from the current position
 * 3 encoding  either node.fs.UTF8 or node.fs.RAW
 *
 * 4 callback(errorno, data)
 */
static Handle<Value>
Read (const Arguments& args)
{
  if ( args.Length() < 3 
    || !args[0]->IsInt32()   // fd
    || !args[1]->IsNumber()  // len
     ) return ThrowException(BAD_ARGUMENTS);

  HandleScope scope;

  int fd = args[0]->Int32Value();
  size_t len = args[1]->IntegerValue();
  off_t pos = args[2]->IsNumber() ? args[2]->IntegerValue() : -1;

  enum encoding encoding = RAW;
  if (args[3]->IsInt32()) {
    encoding = static_cast<enum encoding>(args[3]->Int32Value());
  }

  MAKE_CALLBACK_PTR
  // NOTE: 2nd param: NULL pointer tells eio to allocate it itself
  eio_read(fd, NULL, len, pos, EIO_PRI_DEFAULT, 
      encoding == UTF8 ? AfterUtf8Read : AfterRawRead, callback);
  return Undefined();
}

static int
AfterStat (eio_req *req)
{
  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  Local<Object> stats = Object::New();
  argv[1] = stats;

  if (req->result == 0) {
    struct stat *s = reinterpret_cast<struct stat*>(req->ptr2);

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
    stats->Set(ATIME_SYMBOL, Date::New(1000*static_cast<double>(s->st_atime)));
    /* time of last modification */
    stats->Set(MTIME_SYMBOL, Date::New(1000*static_cast<double>(s->st_mtime)));
    /* time of last status change */
    stats->Set(CTIME_SYMBOL, Date::New(1000*static_cast<double>(s->st_ctime)));
  }
  CALL_CALLBACK_PTR(req, argc, argv);                                    \
  return 0;
}

static Handle<Value>
Stat (const Arguments& args)
{
  if (args.Length() < 1 || !args[0]->IsString())
    return ThrowException(BAD_ARGUMENTS);

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());

  MAKE_CALLBACK_PTR

  eio_stat(*path, EIO_PRI_DEFAULT, AfterStat, callback);

  return Undefined();
}

static Handle<Value>
StrError (const Arguments& args)
{
  if (args.Length() < 1) return v8::Undefined();
  if (!args[0]->IsNumber()) return v8::Undefined();

  HandleScope scope;

  int errorno = args[0]->IntegerValue();

  Local<String> message = String::New(strerror(errorno));

  return scope.Close(message);
}

void
File::Initialize (Handle<Object> target)
{
  HandleScope scope;

  // POSIX Wrappers
  NODE_SET_METHOD(target, "close", Close);
  NODE_SET_METHOD(target, "open", Open);
  NODE_SET_METHOD(target, "read", Read);
  NODE_SET_METHOD(target, "rename", Rename);
  NODE_SET_METHOD(target, "rmdir", RMDir);
  NODE_SET_METHOD(target, "stat", Stat);
  NODE_SET_METHOD(target, "unlink", Unlink);
  NODE_SET_METHOD(target, "write", Write);

  NODE_SET_METHOD(target, "strerror",  StrError);
}
