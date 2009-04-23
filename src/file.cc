#include "node.h"
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

using namespace v8;

#define FD_SYMBOL v8::String::NewSymbol("fd")
#define ACTION_QUEUE_SYMBOL v8::String::NewSymbol("_actionQueue")

// This is the file system object which contains methods
// for accessing the file system (like rename, mkdir, etC). 
// In javascript it is called "File".
static Persistent<Object> fs;

class FileSystem {
public:
  static Handle<Value> Rename (const Arguments& args);
  static int AfterRename (eio_req *req);

  static Handle<Value> Stat (const Arguments& args);
  static int AfterStat (eio_req *req);

  static Handle<Value> StrError (const Arguments& args);
};

class File {
public:
  File (Handle<Object> handle);
  ~File ();

  static Handle<Value> New (const Arguments& args);

  static Handle<Value> Open (const Arguments& args);
  static int AfterOpen (eio_req *req);

  static Handle<Value> Close (const Arguments& args); 
  static int AfterClose (eio_req *req);

  static Handle<Value> Write (const Arguments& args);
  static int AfterWrite (eio_req *req);

  static Handle<Value> Read (const Arguments& args);
  static int AfterRead (eio_req *req);

private:
  static File* Unwrap (Handle<Object> handle);
  bool HasUtf8Encoding (void);
  int GetFD (void);
  static void MakeWeak (Persistent<Value> _, void *data);
  Persistent<Object> handle_;
};

static void
CallTopCallback (Handle<Object> handle, const int argc, Handle<Value> argv[])
{
  HandleScope scope;

  Local<Value> queue_value = handle->Get(ACTION_QUEUE_SYMBOL);
  assert(queue_value->IsArray());

  Local<Array> queue = Local<Array>::Cast(queue_value);
  Local<Value> top_value = queue->Get(Integer::New(0));
  if (top_value->IsObject()) {
    Local<Object> top = top_value->ToObject();
    Local<Value> callback_value = top->Get(String::NewSymbol("callback"));
    if (callback_value->IsFunction()) {
      Handle<Function> callback = Handle<Function>::Cast(callback_value);

      TryCatch try_catch;
      callback->Call(handle, argc, argv);
      if(try_catch.HasCaught()) {
        node_fatal_exception(try_catch);
        return;
      }
    }
  }

  // poll_actions
  Local<Value> poll_actions_value = handle->Get(String::NewSymbol("_pollActions"));
  assert(poll_actions_value->IsFunction());  
  Handle<Function> poll_actions = Handle<Function>::Cast(poll_actions_value);

  poll_actions->Call(handle, 0, NULL);
}

static void
InitActionQueue (Handle<Object> handle)
{
  handle->Set(ACTION_QUEUE_SYMBOL, Array::New());
}

Handle<Value>
FileSystem::Rename (const Arguments& args)
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());

  node_eio_warmup();
  eio_req *req = eio_rename(*path, *new_path, EIO_PRI_DEFAULT, AfterRename, NULL);

  return Undefined();
}

int
FileSystem::AfterRename (eio_req *req)
{
  HandleScope scope;
  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  CallTopCallback(fs, argc, argv);
  return 0;
}

Handle<Value>
FileSystem::Stat (const Arguments& args)
{
  if (args.Length() < 1)
    return v8::Undefined();

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());

  node_eio_warmup();
  eio_req *req = eio_stat(*path, EIO_PRI_DEFAULT, AfterStat, NULL);

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
    struct stat *s = static_cast<struct stat*>(req->ptr2);

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

  CallTopCallback(fs, argc, argv);
    
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
{
  HandleScope scope;
  handle_ = Persistent<Object>::New(handle);

  InitActionQueue(handle);

  Handle<External> external = External::New(this);
  handle_->SetInternalField(0, external);
  handle_.MakeWeak(this, File::MakeWeak);
}

File::~File ()
{
  // XXX call close?
  handle_->SetInternalField(0, Undefined());
  handle_.Dispose();
  handle_.Clear(); 
}

File*
File::Unwrap (Handle<Object> handle)
{
  HandleScope scope;
  Handle<External> field = Handle<External>::Cast(handle->GetInternalField(0));
  File* file = static_cast<File*>(field->Value());
  return file;
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
}


void
File::MakeWeak (Persistent<Value> _, void *data)
{
  File *file = static_cast<File*> (data);
  delete file;
}

Handle<Value>
File::Close (const Arguments& args) 
{
  HandleScope scope;

  File *file = File::Unwrap(args.Holder());  

  int fd = file->GetFD();

  node_eio_warmup();
  eio_req *req = eio_close (fd, EIO_PRI_DEFAULT, File::AfterClose, file);

  return Undefined();
}

int
File::AfterClose (eio_req *req)
{
  File *file = static_cast<File*>(req->data);

  if (req->result == 0) {
    file->handle_->Delete(FD_SYMBOL);
  }

  const int argc = 1;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  CallTopCallback(file->handle_, argc, argv);

  return 0;
}

Handle<Value>
File::Open (const Arguments& args)
{
  /* check arguments */
  if (args.Length() < 1) return Undefined();
  if (!args[0]->IsString()) return Undefined();

  HandleScope scope;

  File *file = File::Unwrap(args.Holder());  

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
  node_eio_warmup();
  eio_req *req = eio_open (*path, flags, 0666, EIO_PRI_DEFAULT, File::AfterOpen, file);

  return Undefined();
}

int
File::AfterOpen (eio_req *req)
{
  File *file = static_cast<File*>(req->data);
  HandleScope scope;

  if(req->result >= 0) {
    file->handle_->Set(FD_SYMBOL, Integer::New(req->result));
  }

  const int argc = 1;
  Handle<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  CallTopCallback(file->handle_, argc, argv);

  return 0;
}

Handle<Value>
File::Write (const Arguments& args)
{
  if (args.Length() < 2) return Undefined();
  if (!args[1]->IsNumber()) return Undefined();

  HandleScope scope;

  File *file = File::Unwrap(args.Holder());

  char *buf = NULL; 
  size_t length = 0;

  if (args[0]->IsString()) {
    // utf8 encoding
    Local<String> string = args[0]->ToString();
    length = string->Utf8Length();
    buf = static_cast<char*>(malloc(length));
    string->WriteUtf8(buf, length);
    
  } else if (args[0]->IsArray()) {
    // raw encoding
    Local<Array> array = Local<Array>::Cast(args[0]);
    length = array->Length();
    buf = static_cast<char*>(malloc(length));
    for (int i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf[i] = int_value->Int32Value();
    }

  } else {
    // bad arguments. raise error?
    return Undefined();
  }

  size_t pos = args[1]->Uint32Value();

  if (file->handle_->Has(FD_SYMBOL) == false) {
    printf("trying to write to a bad fd!\n");  
    return Undefined();
  }

  int fd = file->GetFD();

  node_eio_warmup();
  eio_req *req = eio_write(fd, buf, length, pos, EIO_PRI_DEFAULT, File::AfterWrite, file);

  return Undefined();
}

int
File::AfterWrite (eio_req *req)
{
  File *file = static_cast<File*>(req->data);

  //char *buf = static_cast<char*>(req->ptr2);
  free(req->ptr2);
  size_t written = req->result;

  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);
  argv[1] = written >= 0 ? Integer::New(written) : Integer::New(0);
  CallTopCallback(file->handle_, argc, argv);

  return 0;
}

Handle<Value>
File::Read (const Arguments& args)
{
  if (args.Length() < 1) return Undefined();
  if (!args[0]->IsNumber()) return Undefined();
  if (!args[1]->IsNumber()) return Undefined();

  HandleScope scope;
  File *file = File::Unwrap(args.Holder());
  size_t length = args[0]->IntegerValue();
  size_t pos = args[1]->Uint32Value();

  int fd = file->GetFD();

  // NOTE: NULL pointer tells eio to allocate it itself
  node_eio_warmup();
  eio_req *req = eio_read(fd, NULL, length, pos, EIO_PRI_DEFAULT, File::AfterRead, file);
  assert(req);

  return Undefined();
}

int
File::AfterRead (eio_req *req)
{
  File *file = static_cast<File*>(req->data);
  HandleScope scope;

  const int argc = 2;
  Local<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  char *buf = static_cast<char*>(req->ptr2);

  if(req->result == 0) { 
    // eof 
    argv[1] = Local<Value>::New(Null());
  } else {
    size_t length = req->result;
    if (file->HasUtf8Encoding()) {
      // utf8 encoding
      argv[1] = String::New(buf, req->result);
    } else {
      // raw encoding
      Local<Array> array = Array::New(length);
      for (int i = 0; i < length; i++) {
        array->Set(Integer::New(i), Integer::New(buf[i]));
      }
      argv[1] = array;
    }
  }
  CallTopCallback(file->handle_, argc, argv);
  return 0;
}

Handle<Value>
File::New(const Arguments& args)
{
  HandleScope scope;
  File *file = new File(args.Holder());
  if(file == NULL)
    return Undefined(); // XXX raise error?

  return args.This();
}

void
NodeInit_file (Handle<Object> target)
{
  if (!fs.IsEmpty())
    return;

  HandleScope scope;

  Local<FunctionTemplate> file_template = FunctionTemplate::New(File::New);
  file_template->InstanceTemplate()->SetInternalFieldCount(1);

  fs = Persistent<Object>::New(file_template->GetFunction());
  InitActionQueue(fs);

  target->Set(String::NewSymbol("File"), fs);

  // file system methods
  NODE_SET_METHOD(fs, "_ffi_rename", FileSystem::Rename);
  NODE_SET_METHOD(fs, "_ffi_stat", FileSystem::Stat);
  NODE_SET_METHOD(fs, "strerror", FileSystem::StrError);
  fs->Set(String::NewSymbol("STDIN_FILENO"), Integer::New(STDIN_FILENO));
  fs->Set(String::NewSymbol("STDOUT_FILENO"), Integer::New(STDOUT_FILENO));
  fs->Set(String::NewSymbol("STDERR_FILENO"), Integer::New(STDERR_FILENO));

  // file methods
  NODE_SET_METHOD(file_template->InstanceTemplate(), "_ffi_open", File::Open);
  NODE_SET_METHOD(file_template->InstanceTemplate(), "_ffi_close", File::Close);
  NODE_SET_METHOD(file_template->InstanceTemplate(), "_ffi_write", File::Write);
  NODE_SET_METHOD(file_template->InstanceTemplate(), "_ffi_read", File::Read);
}
