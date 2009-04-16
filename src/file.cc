#include "node.h"
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>


using namespace v8;

#define ON_OPEN v8::String::NewSymbol("onOpen")


class File;
class Callback {
  public:
    Callback(Handle<Value> v);
    ~Callback();
    Local<Value> Call(Handle<Object> recv, int argc, Handle<Value> argv[]);
    File *file;
  private:
    Persistent<Function> handle_;
};

Callback::Callback (Handle<Value> v)
{
  HandleScope scope;
  Handle<Function> f = Handle<Function>::Cast(v);
  handle_ = Persistent<Function>::New(f);
}

Callback::~Callback ()
{
  handle_.Dispose();
  handle_.Clear(); // necessary? 
}

Local<Value>
Callback::Call (Handle<Object> recv, int argc, Handle<Value> argv[])
{
  HandleScope scope;
  Local<Value> r = handle_->Call(recv, argc, argv);
  return scope.Close(r);
}

static int
AfterRename (eio_req *req)
{
  Callback *callback = static_cast<Callback*>(req->data);
  if (callback != NULL) {
    HandleScope scope;
    const int argc = 2;
    Local<Value> argv[argc];

    argv[0] = Integer::New(req->errorno);
    argv[1] = String::New(strerror(req->errorno));
    
    callback->Call(Context::GetCurrent()->Global(), argc, argv);
    delete callback;
  }
  return 0;
}

JS_METHOD(rename) 
{
  if (args.Length() < 2)
    return Undefined();

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());
  String::Utf8Value new_path(args[1]->ToString());

  Callback *callback = NULL;
  if (!args[2]->IsUndefined()) callback = new Callback(args[2]);

  eio_req *req = eio_rename(*path, *new_path, EIO_PRI_DEFAULT, AfterRename, callback);
  node_eio_submit(req);

  return Undefined();
}

static int
AfterStat (eio_req *req)
{
  Callback *callback = static_cast<Callback*>(req->data);
  if (callback != NULL) {
    HandleScope scope;
    const int argc = 3;
    Local<Value> argv[argc];

    Local<Object> stats = Object::New();
    argv[0] = stats;
    argv[1] = Integer::New(req->errorno);
    argv[2] = String::New(strerror(req->errorno));

    if (req->result == 0) {
      struct stat *s = static_cast<struct stat*>(req->ptr2);

      /* ID of device containing file */
      stats->Set(JS_SYMBOL("dev"), Integer::New(s->st_dev));
      /* inode number */
      stats->Set(JS_SYMBOL("ino"), Integer::New(s->st_ino));
      /* protection */
      stats->Set(JS_SYMBOL("mode"), Integer::New(s->st_mode));
      /* number of hard links */
      stats->Set(JS_SYMBOL("nlink"), Integer::New(s->st_nlink));
      /* user ID of owner */
      stats->Set(JS_SYMBOL("uid"), Integer::New(s->st_uid));
      /* group ID of owner */
      stats->Set(JS_SYMBOL("gid"), Integer::New(s->st_gid));
      /* device ID (if special file) */
      stats->Set(JS_SYMBOL("rdev"), Integer::New(s->st_rdev));
      /* total size, in bytes */
      stats->Set(JS_SYMBOL("size"), Integer::New(s->st_size));
      /* blocksize for filesystem I/O */
      stats->Set(JS_SYMBOL("blksize"), Integer::New(s->st_blksize));
      /* number of blocks allocated */
      stats->Set(JS_SYMBOL("blocks"), Integer::New(s->st_blocks));
      /* time of last access */
      stats->Set(JS_SYMBOL("atime"), Date::New(1000*static_cast<double>(s->st_atime)));
      /* time of last modification */
      stats->Set(JS_SYMBOL("mtime"), Date::New(1000*static_cast<double>(s->st_mtime)));
      /* time of last status change */
      stats->Set(JS_SYMBOL("ctime"), Date::New(1000*static_cast<double>(s->st_ctime)));
    }
    
    callback->Call(Context::GetCurrent()->Global(), argc, argv);
    delete callback;
  }
  return 0;
}

JS_METHOD(stat) 
{
  if (args.Length() < 1)
    return v8::Undefined();

  HandleScope scope;

  String::Utf8Value path(args[0]->ToString());

  Callback *callback = NULL;
  if (!args[1]->IsUndefined()) callback = new Callback(args[1]);

  eio_req *req = eio_stat(*path, EIO_PRI_DEFAULT, AfterStat, callback);
  node_eio_submit(req);

  return Undefined();
}

///////////////////// FILE ///////////////////// 

class File {
public:
  File (Handle<Object> handle);
  ~File ();

  static File* Unwrap (Handle<Object> obj);

  Handle<Value> Open (const char *path, const char *mode, Handle<Value> callback);
  static int AfterOpen (eio_req *req);

  void Close ();
  static int AfterClose (eio_req *req);

  void Write (char *buf, size_t length, Callback *callback);
  static int AfterWrite (eio_req *req);


private:
  static void MakeWeak (Persistent<Value> _, void *data);
  int fd_;
  Persistent<Object> handle_;
};

File::File (Handle<Object> handle)
{
  HandleScope scope;
  fd_ = -1;
  handle_ = Persistent<Object>::New(handle);
  Handle<External> external = External::New(this);
  handle_->SetInternalField(0, external);
  handle_.MakeWeak(this, File::MakeWeak);
}

File::~File ()
{
  Close();
  handle_->SetInternalField(0, Undefined());
  handle_.Dispose();
  handle_.Clear(); 
}

File*
File::Unwrap (Handle<Object> obj)
{
  HandleScope scope;
  Handle<External> field = Handle<External>::Cast(obj->GetInternalField(0));
  File* file = static_cast<File*>(field->Value());
  return file;
}

void
File::MakeWeak (Persistent<Value> _, void *data)
{
  File *file = static_cast<File*> (data);
  delete file;
}

int
File::AfterClose (eio_req *req)
{
  File *file = static_cast<File*>(req->data);

  if (req->result == 0) {
    file->fd_ = -1;
    file->handle_->Delete(String::NewSymbol("fd"));
  }

  // TODO
  printf("after close\n");
  return 0;
}

void
File::Close () 
{
  eio_req *req = eio_close (fd_, EIO_PRI_DEFAULT, File::AfterClose, this);
  node_eio_submit(req);
}

int
File::AfterOpen (eio_req *req)
{
  File *file = static_cast<File*>(req->data);
  HandleScope scope;

  if(req->result >= 0) {
    file->fd_ = static_cast<int>(req->result);
    file->handle_->Set(String::NewSymbol("fd"), Integer::New(file->fd_));
  }

  Handle<Value> callback_value = file->handle_->Get(ON_OPEN);
  if (!callback_value->IsFunction())
    return 0; 
  Handle<Function> callback = Handle<Function>::Cast(callback_value);
  file->handle_->Delete(ON_OPEN);

  const int argc = 1;
  Handle<Value> argv[argc];
  argv[0] = Integer::New(req->errorno);

  TryCatch try_catch;
  callback->Call(file->handle_, argc, argv);
  if(try_catch.HasCaught())
    node_fatal_exception(try_catch);

  return 0;
}

Handle<Value>
File::Open (const char *path, const char *mode, Handle<Value> callback)
{
  // make sure that we don't already have a pending open
  if (handle_->Has(ON_OPEN)) {
    return ThrowException(String::New("File object is already being opened."));
  }

  if (callback->IsFunction()) handle_->Set(ON_OPEN, callback);

  // Get the current umask
  mode_t mask = umask(0); 
  umask(mask);
  
  int flags = O_RDONLY; // default
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

  eio_req *req = eio_open (path, flags, mask, EIO_PRI_DEFAULT, File::AfterOpen, this);
  node_eio_submit(req);

  return Undefined();
}

int
File::AfterWrite (eio_req *req)
{
  Callback *callback = static_cast<Callback*>(req->data);
  HandleScope scope;

  char *buf = static_cast<char*>(req->ptr2);
  delete buf;
  size_t written = req->result;

  if (callback) {
    const int argc = 2;
    Local<Value> argv[argc];
    argv[0] = Integer::New(req->errorno);
    argv[1] = written >= 0 ? Integer::New(written) : Integer::New(0);

    callback->Call(callback->file->handle_, argc, argv);

    delete callback;
  }
  return 0;
}

void
File::Write (char *buf, size_t length, Callback *callback)
{
  if (callback) 
    callback->file = this;
  // NOTE: -1 offset in eio_write() invokes write() instead of pwrite()
  if (fd_ < 0) {
    printf("trying to write to a bad fd!\n");  
    return;
  }
  eio_req *req = eio_write(fd_, buf, length, -1, EIO_PRI_DEFAULT, File::AfterWrite, callback);
  node_eio_submit(req);
}

static Handle<Value>
NewFile (const Arguments& args)
{
  HandleScope scope;
  File *file = new File(args.Holder());
  if(file == NULL)
    return Undefined(); // XXX raise error?

  return args.This();
}

JS_METHOD(file_open) 
{
  if (args.Length() < 1) return Undefined();
  if (!args[0]->IsString()) return Undefined();

  HandleScope scope;

  File *file = File::Unwrap(args.Holder());  
  String::Utf8Value path(args[0]->ToString());
  if (args[1]->IsString()) {
    String::AsciiValue mode(args[1]->ToString());
    return file->Open(*path, *mode, args[2]);
  } else {
    return file->Open(*path, "r", args[1]);
  }
}

JS_METHOD(file_close) 
{
  HandleScope scope;

  File *file = File::Unwrap(args.Holder());  
  file->Close();

  return Undefined();
}

JS_METHOD(file_write) 
{
  if (args.Length() < 1) return Undefined();
  if (!args[0]->IsString()) 

  HandleScope scope;

  File *file = File::Unwrap(args.Holder());

  char *buf = NULL; 
  size_t length = 0;

  if (args[0]->IsString()) {
    // utf8 encoded data
    Local<String> string = args[0]->ToString();
    length = string->Utf8Length();
    buf = new char[length];
    string->WriteUtf8(buf, length);
    
  } else if (args[0]->IsArray()) {
    // binary data
    Local<Array> array = Local<Array>::Cast(args[0]);
    length = array->Length();
    buf = new char[length];
    for (int i = 0; i < length; i++) {
      Local<Value> int_value = array->Get(Integer::New(i));
      buf[i] = int_value->Int32Value();
    }

  } else {
    // bad arguments. raise error?
    return Undefined();
  }
  
  Callback *callback = args[1]->IsFunction() ? new Callback(args[1]) : NULL;

  file->Write(buf, length, callback);

  return Undefined();
}


void
NodeInit_file (Handle<Object> target)
{
  HandleScope scope;

  Local<Object> fs = Object::New();
  target->Set(String::NewSymbol("fs"), fs);
  
  JS_SET_METHOD(fs, "rename", rename);
  JS_SET_METHOD(fs, "stat", stat);

  Local<FunctionTemplate> file_template = FunctionTemplate::New(NewFile);
  file_template->InstanceTemplate()->SetInternalFieldCount(1);
  target->Set(String::NewSymbol("File"), file_template->GetFunction());

  JS_SET_METHOD(file_template->InstanceTemplate(), "open", file_open);
  JS_SET_METHOD(file_template->InstanceTemplate(), "close", file_close);
  JS_SET_METHOD(file_template->InstanceTemplate(), "write", file_write);
}
