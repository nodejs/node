// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef NODE_IO_H_
#define NODE_IO_H_

#include <node_object_wrap.h>
#include <ev.h>

namespace node {

class IOWatcher {
 public:
  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  static v8::Persistent<v8::FunctionTemplate> constructor_template;

  IOWatcher() {
    ev_init(&watcher_, IOWatcher::Callback);
    watcher_.data = this;
  }

  ~IOWatcher() {
    assert(!ev_is_active(&watcher_));
  }

  static v8::Handle<v8::Value> New(const v8::Arguments& args);
  static v8::Handle<v8::Value> Start(const v8::Arguments& args);
  static v8::Handle<v8::Value> Stop(const v8::Arguments& args);
  static v8::Handle<v8::Value> Set(const v8::Arguments& args);

  inline void Wrap(v8::Handle<v8::Object> handle) {
    assert(handle_.IsEmpty());
    assert(handle->InternalFieldCount() > 0);
    handle_ = v8::Persistent<v8::Object>::New(handle);
    handle_->SetInternalField(0, v8::External::New(this));
    MakeWeak();
  }

  inline void MakeWeak(void) {
    handle_.MakeWeak(this, WeakCallback);
  }



 private:
  static void Callback(EV_P_ ev_io *watcher, int revents);

  static void WeakCallback (v8::Persistent<v8::Value> value, void *data)
  {
    IOWatcher *io = static_cast<IOWatcher*>(data);
    assert(value == io->handle_);
    if (!ev_is_active(&io->watcher_)) {
      value.Dispose();
      delete io;
    } else {
      //value.ClearWeak();
      io->MakeWeak();
    }
  }

  static IOWatcher* Unwrap(v8::Handle<v8::Object> handle) {
    assert(!handle.IsEmpty());
    assert(handle->InternalFieldCount() > 0);
    return static_cast<IOWatcher*>(v8::Handle<v8::External>::Cast(
        handle->GetInternalField(0))->Value());
  }

  void Stop();

  ev_io watcher_;
  v8::Persistent<v8::Object> handle_;
};

}  // namespace node
#endif  // NODE_IO_H_

