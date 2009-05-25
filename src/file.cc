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

#define FD_SYMBOL           String::NewSymbol("fd")
#define ACTION_QUEUE_SYMBOL String::NewSymbol("_actionQueue")
#define ENCODING_SYMBOL     String::NewSymbol("encoding")
#define CALLBACK_SYMBOL     String::NewSymbol("callbaccallback")
#define POLL_ACTIONS_SYMBOL String::NewSymbol("_pollActions")

#define UTF8_SYMBOL         String::NewSymbol("utf8")
#define RAW_SYMBOL          String::NewSymbol("raw")

void
File::Initialize (Handle<Object> target)
{
  HandleScope scope;

  // file system methods
  NODE_SET_METHOD(target, "rename",   FileSystem::Rename);
  NODE_SET_METHOD(target, "stat",     FileSystem::Stat);
  NODE_SET_METHOD(target, "strerror", FileSystem::StrError);

  target->Set(String::NewSymbol("STDIN_FILENO"), Integer::New(STDIN_FILENO));
  target->Set(String::NewSymbol("STDOUT_FILENO"), Integer::New(STDOUT_FILENO));
  target->Set(String::NewSymbol("STDERR_FILENO"), Integer::New(STDERR_FILENO));


  Local<FunctionTemplate> file_template = FunctionTemplate::New(File::New);
  file_template->InstanceTemplate()->SetInternalFieldCount(1);

  // file methods
  NODE_SET_PROTOTYPE_METHOD(file_template, "_ffi_open", File::Open);
  NODE_SET_PROTOTYPE_METHOD(file_template, "_ffi_close", File::Close);
  NODE_SET_PROTOTYPE_METHOD(file_template, "_ffi_write", File::Write);
  NODE_SET_PROTOTYPE_METHOD(file_template, "_ffi_read", File::Read);
  file_template->InstanceTemplate()->SetAccessor(ENCODING_SYMBOL, File::GetEncoding, File::SetEncoding);
  target->Set(String::NewSymbol("File"), file_template->GetFunction());
}

Handle<Value>
File::GetEncoding (Local<String> property, const AccessorInfo& info) 
{
  File *file = NODE_UNWRAP(File, info.This());

  if (file->encoding_ == UTF8)
    return UTF8_SYMBOL;
  else
    return RAW_SYMBOL;
}
    
void
File::SetEncoding (Local<String> property, Local<Value> value, const AccessorInfo& info) 
{
  File *file = NODE_UNWRAP(File, info.This());

  if (value->IsString()) {
    Local<String> encoding_string = value->ToString();
    char buf[5]; // need enough room for "utf8" or "raw"
    encoding_string->WriteAscii(buf, 0, 4);
    buf[4] = '\0';
    file->encoding_ = strcasecmp(buf, "utf8") == 0 ? UTF8 : RAW;
  } else {
    file->encoding_ = RAW;
  }
}

static void
CallTopCallback (Handle<Object> handle, const int argc, Handle<Value> argv[])
{
  HandleScope scope;

  // poll_actions
  Local<Value> poll_actions_value = handle->Get(POLL_ACTIONS_SYMBOL);
  assert(poll_actions_value->IsFunction());  
  Handle<Function> poll_actions = Handle<Function>::Cast(poll_actions_value);

  TryCatch try_catch;
  poll_actions->Call(handle, argc, argv);
  if(try_catch.HasCaught())
    node::fatal_exception(try_catch);
}

Handle<Value>
FileSystem::Rename (const Arguments& args)
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());

  Persistent<Function> *callback = NULL;
  if (args[2]->IsFunction()) {
    Local<Function> l = Local<Function>::Cast(args[2]);
    callback = new Persistent<Function>();
    *callback = Persistent<Function>::New(l);
  }

  node::eio_warmup();
  eio_rename(*path, *new_path, EIO_PRI_DEFAULT, AfterRename, NULL);

  return Undefined();
}

int
FileSystem::AfterRename (eio_req *req)
{
  HandleScope scope;
  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  if (req->data) {
    Persistent<Function> *callback = reinterpret_cast<Persistent<Function>*>(req->data);

    TryCatch try_catch;
    (*callback)->Call(Context::GetCurrent()->Global(), argc, argv);
    if(try_catch.HasCaught())
      node::fatal_exception(try_catch);

    free(callback);
  }

  return 0;
}

Handle<Value>
FileSystem::Stat (const Arguments& args)
{
  if (args.Length() < 1)
    return v8::Undefined();

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());

  Persistent<Function> *callback = NULL;
  if (args[1]->IsFunction()) {
    Local<Function> l = Local<Function>::Cast(args[1]);
    callback = new Persistent<Function>();
    *callback = Persistent<Function>::New(l);
  }

  node::eio_warmup();
  eio_stat(*path, EIO_PRI_DEFAULT, AfterStat, callback);

  return Undefined();
}

int
FileSystem::AfterStat (eio_req *req)
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
    stats->Set(NODE_SYMBOL("dev"), Integer::New(s->st_dev));
    /* inode number */
    stats->Set(NODE_SYMBOL("ino"), Integer::New(s->st_ino));
    /* protection */
    stats->Set(NODE_SYMBOL("mode"), Integer::New(s->st_mode));
    /* number of hard links */
    stats->Set(NODE_SYMBOL("nlink"), Integer::New(s->st_nlink));
    /* user ID of owner */
    stats->Set(NODE_SYMBOL("uid"), Integer::New(s->st_uid));
    /* group ID of owner */
    stats->Set(NODE_SYMBOL("gid"), Integer::New(s->st_gid));
    /* device ID (if special file) */
    stats->Set(NODE_SYMBOL("rdev"), Integer::New(s->st_rdev));
    /* total size, in bytes */
    stats->Set(NODE_SYMBOL("size"), Integer::New(s->st_size));
    /* blocksize for filesystem I/O */
    stats->Set(NODE_SYMBOL("blksize"), Integer::New(s->st_blksize));
    /* number of blocks allocated */
    stats->Set(NODE_SYMBOL("blocks"), Integer::New(s->st_blocks));
    /* time of last access */
    stats->Set(NODE_SYMBOL("atime"), Date::New(1000*static_cast<double>(s->st_atime)));
    /* time of last modification */
    stats->Set(NODE_SYMBOL("mtime"), Date::New(1000*static_cast<double>(s->st_mtime)));
    /* time of last status change */
    stats->Set(NODE_SYMBOL("ctime"), Date::New(1000*static_cast<double>(s->st_ctime)));
  }

  if (req->data) {
    Persistent<Function> *callback = reinterpret_cast<Persistent<Function>*>(req->data);

    TryCatch try_catch;
    (*callback)->Call(Context::GetCurrent()->Global(), argc, argv);
    if(try_catch.HasCaught())
      node::fatal_exception(try_catch);

    free(callback);
  }

  return 0;
}

Handle<Value>
FileSystem::StrError (const Arguments& args)
{
  if (args.Length() < 1) return v8::Undefined();
  if (!args[0]->IsNumber()) return v8::Undefined();

  HandleScope scope;

  int errorno = args[0]->IntegerValue();

  Local<String> message = String::New(strerror(errorno));

  return scope.Close(message);
}

///////////////////// FILE ///////////////////// 

File::File (Handle<Object> handle)
  : ObjectWrap(handle)
{
  HandleScope scope;
  encoding_ = RAW;
  handle->Set(ACTION_QUEUE_SYMBOL, Array::New());
}

File::~File ()
{
  ; // XXX call close?
}

bool
File::HasUtf8Encoding (void)
{
  return false;
}

int
File::GetFD (void)
{
  Handle<Value> fd_value = handle_->Get(FD_SYMBOL);
  int fd = fd_value->IntegerValue();
  return fd;
}

Handle<Value>
File::Close (const Arguments& args) 
{
  HandleScope scope;

  File *file = NODE_UNWRAP(File, args.Holder());

  int fd = file->GetFD();

  node::eio_warmup();
  eio_close (fd, EIO_PRI_DEFAULT, File::AfterClose, file);
  file->Attach();
  return Undefined();
}

int
File::AfterClose (eio_req *req)
{
  File *file = reinterpret_cast<File*>(req->data);

  if (req->result == 0) {
    file->handle_->Delete(FD_SYMBOL);
  }

  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  CallTopCallback(file->handle_, argc, argv);
  file->Detach();
  return 0;
}

Handle<Value>
File::Open (const Arguments& args)
{
  /* check arguments */
  if (args.Length() < 1) return Undefined();
  if (!args[0]->IsString()) return Undefined();

  HandleScope scope;

  File *file = NODE_UNWRAP(File, args.Holder());

  // make sure that we don't already have a pending open
  if (file->handle_->Has(FD_SYMBOL)) {
    return ThrowException(String::New("File object is opened."));
  }

  String::Utf8Value path(args[0]->ToString());

  int flags = O_RDONLY; // default
  if (args[1]->IsString()) {
    String::AsciiValue mode_v(args[1]->ToString());
    char *mode = *mode_v;
    // XXX is this interpretation of the mode correct?
    // I don't want to to use fopen() directly because eio doesn't support it.
    switch(mode[0]) {
      case 'r':
        flags = (mode[1] == '+' ? O_RDWR : O_RDONLY); 
        break;
      case 'w':
        flags = O_CREAT | O_TRUNC | (mode[1] == '+' ? O_RDWR : O_WRONLY); 
        break;
      case 'a':
        flags = O_APPEND | O_CREAT | (mode[1] == '+' ? O_RDWR : O_WRONLY); 
        break;
    }
  }

  // TODO how should the mode be set?
  node::eio_warmup();
  eio_open (*path, flags, 0666, EIO_PRI_DEFAULT, File::AfterOpen, file);
  file->Attach();
  return Undefined();
}

int
File::AfterOpen (eio_req *req)
{
  File *file = reinterpret_cast<File*>(req->data);
  HandleScope scope;

  if(req->result >= 0) {
    file->handle_->Set(FD_SYMBOL, Integer::New(req->result));
  }

  const int argc = 1;
  Handle<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  CallTopCallback(file->handle_, argc, argv);

  file->Detach();
  return 0;
}

Handle<Value>
File::Write (const Arguments& args)
{
  if (args.Length() < 2) return Undefined();
  if (!args[1]->IsNumber()) return Undefined();

  HandleScope scope;

  File *file = NODE_UNWRAP(File, args.Holder());

  off_t pos = args[1]->IntegerValue();

  char *buf = NULL; 
  size_t length = 0;

  if (args[0]->IsString()) {
    // utf8 encoding
    Local<String> string = args[0]->ToString();
    length = string->Utf8Length();
    buf = reinterpret_cast<char*>(malloc(length));
    string->WriteUtf8(buf, length);
    
  } else if (args[0]->IsArray()) {
    // raw encoding
    Local<Array> array = Local<Array>::Cast(args[0]);
    length = array->Length();
    buf = reinterpret_cast<char*>(malloc(length));
    for (unsigned int i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf[i] = int_value->Int32Value();
    }

  } else {
    // bad arguments. raise error?
    return Undefined();
  }

  if (file->handle_->Has(FD_SYMBOL) == false) {
    printf("trying to write to a bad fd!\n");  
    return Undefined();
  }

  int fd = file->GetFD();

  node::eio_warmup();
  eio_write(fd, buf, length, pos, EIO_PRI_DEFAULT, File::AfterWrite, file);

  file->Attach();
  return Undefined();
}

int
File::AfterWrite (eio_req *req)
{
  File *file = reinterpret_cast<File*>(req->data);

  //char *buf = reinterpret_cast<char*>(req->ptr2);
  free(req->ptr2);
  ssize_t written = req->result;

  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  argv[1] = written >= 0 ? Integer::New(written) : Integer::New(0);
  CallTopCallback(file->handle_, argc, argv);

  file->Detach();
  return 0;
}

Handle<Value>
File::Read (const Arguments& args)
{
  HandleScope scope;

  if (args.Length() < 1 || !args[0]->IsNumber() || !args[1]->IsNumber()) {
    return ThrowException(String::New("Bad parameter for _ffi_read()"));
  }

  File *file = NODE_UNWRAP(File, args.Holder());
  size_t len = args[0]->IntegerValue();
  off_t pos  = args[1]->IntegerValue();

  int fd = file->GetFD();
  assert(fd >= 0);

  // NOTE: 2nd param: NULL pointer tells eio to allocate it itself
  node::eio_warmup();
  eio_read(fd, NULL, len, pos, EIO_PRI_DEFAULT, File::AfterRead, file);

  file->Attach();
  return Undefined();
}

int
File::AfterRead (eio_req *req)
{
  File *file = reinterpret_cast<File*>(req->data);
  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  char *buf = reinterpret_cast<char*>(req->ptr2);

  if (req->result == 0) { 
    // eof 
    argv[1] = Local<Value>::New(Null());
  } else {
    size_t len = req->result;
    if (file->encoding_ == UTF8) {
      // utf8 encoding
      argv[1] = String::New(buf, req->result);
    } else {
      // raw encoding
      Local<Array> array = Array::New(len);
      for (unsigned int i = 0; i < len; i++) {
        array->Set(Integer::New(i), Integer::New(buf[i]));
      }
      argv[1] = array;
    }
  }

  CallTopCallback(file->handle_, argc, argv);

  file->Detach();
  return 0;
}

Handle<Value>
File::New(const Arguments& args)
{
  HandleScope scope;

  File *f = new File(args.Holder());
  ObjectWrap::InformV8ofAllocation(f);

  return args.This();
}

