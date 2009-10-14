// Copyright 2009 Ryan Dahl <ry@tinyclouds.org>
#ifndef SRC_FILE_H_
#define SRC_FILE_H_

#include <node.h>
#include <events.h>
#include <v8.h>

namespace node {

/* Are you missing your favorite POSIX function? It might be very easy to
 * add a wrapper. Take a look in deps/libeio/eio.h at the list of wrapper
 * functions.  If your function is in that list, just follow the lead of
 * EIOPromise::Open. You'll need to add two functions, one static function
 * in EIOPromise, and one static function which interprets the javascript
 * args in src/file.cc. Then just a reference to that function in
 * File::Initialize() and you should be good to go.
 * Don't forget to add a test to test/mjsunit.
 */

#define NODE_V8_METHOD_DECLARE(name) \
  static v8::Handle<v8::Value> name(const v8::Arguments& args)

class File {
 public:
  static void Initialize(v8::Handle<v8::Object> target);
};

class EIOPromise : public Promise {
 public:
  static EIOPromise* Create();
  static v8::Persistent<v8::FunctionTemplate> constructor_template;
  static v8::Handle<v8::Value> New(const v8::Arguments& args);

  static v8::Handle<v8::Object> Open(const char *path, int flags, mode_t mode) {
    EIOPromise *p = Create();
    p->req_ = eio_open(path, flags, mode, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> Close(int fd) {
    EIOPromise *p = Create();
    p->req_ = eio_close(fd, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> Write(int fd,
                                      void *buf,
                                      size_t count,
                                      off_t offset) {
    EIOPromise *p = Create();
    p->req_ = eio_write(fd, buf, count, offset, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> Read(int fd,
                                     size_t count,
                                     off_t offset,
                                     enum encoding encoding) {
    EIOPromise *p = Create();
    p->encoding_ = encoding;
    // NOTE: 2nd param: NULL pointer tells eio to allocate it itself
    p->req_ = eio_read(fd, NULL, count, offset, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> Stat(const char *path) {
    EIOPromise *p = Create();
    p->req_ = eio_stat(path, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> Rename(const char *path, const char *new_path) {
    EIOPromise *p = Create();
    p->req_ = eio_rename(path, new_path, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> Unlink(const char *path) {
    EIOPromise *p = Create();
    p->req_ = eio_unlink(path, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> RMDir(const char *path) {
    EIOPromise *p = Create();
    p->req_ = eio_rmdir(path, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> MKDir(const char *path, mode_t mode) {
    EIOPromise *p = Create();
    p->req_ = eio_mkdir(path, mode, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> ReadDir(const char *path) {
    EIOPromise *p = Create();
    // By using EIO_READDIR_DENTS (or other flags), more complex results can
    // be had from eio_readdir. Doesn't seem that we need that though.
    p->req_ = eio_readdir(path, 0, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

  static v8::Handle<v8::Object> SendFile(int out_fd, int in_fd, off_t in_offset, size_t length) {
    EIOPromise *p = Create();
    p->req_ = eio_sendfile(out_fd, in_fd, in_offset, length, EIO_PRI_DEFAULT, After, p);
    return p->handle_;
  }

 protected:

  void Attach();
  void Detach();

  EIOPromise() : Promise() { }

 private:

  static int After(eio_req *req);

  eio_req *req_;
  enum encoding encoding_;
};

}  // namespace node
#endif  // SRC_FILE_H_
