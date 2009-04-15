#include "node.h"
#include <string.h>

using namespace v8;

class Callback {
  public:
    Callback(Handle<Value> v);
    ~Callback();
    Local<Value> Call(Handle<Object> recv, int argc, Handle<Value> argv[]);
  private:
    Persistent<Function> handle;
};


Callback::Callback (Handle<Value> v)
{
  HandleScope scope;
  Handle<Function> f = Handle<Function>::Cast(v);
  handle = Persistent<Function>::New(f);
}

Callback::~Callback ()
{
  handle.Dispose();
  handle.Clear(); // necessary? 
}

Local<Value>
Callback::Call (Handle<Object> recv, int argc, Handle<Value> argv[])
{
  HandleScope scope;
  Local<Value> r = handle->Call(recv, argc, argv);
  return scope.Close(r);
}

static int
after_rename (eio_req *req)
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

  eio_req *req = eio_rename(*path, *new_path, EIO_PRI_DEFAULT, after_rename, callback);
  node_eio_submit(req);

  return Undefined();
}

static int
after_stat (eio_req *req)
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

  eio_req *req = eio_stat(*path, EIO_PRI_DEFAULT, after_stat, callback);
  node_eio_submit(req);

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
}
