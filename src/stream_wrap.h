// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef STREAM_WRAP_H_
#define STREAM_WRAP_H_

#include "v8.h"
#include "node.h"
#include "handle_wrap.h"

namespace node {


enum WriteEncoding {
  kAscii,
  kUtf8,
  kUcs2
};


class StreamWrap : public HandleWrap {
 public:
  uv_stream_t* GetStream() { return stream_; }

  static void Initialize(v8::Handle<v8::Object> target);

  static v8::Handle<v8::Value> GetFD(v8::Local<v8::String>,
                                     const v8::AccessorInfo&);

  // JavaScript functions
  static v8::Handle<v8::Value> ReadStart(const v8::Arguments& args);
  static v8::Handle<v8::Value> ReadStop(const v8::Arguments& args);
  static v8::Handle<v8::Value> Shutdown(const v8::Arguments& args);

  static v8::Handle<v8::Value> WriteBuffer(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteAsciiString(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteUtf8String(const v8::Arguments& args);
  static v8::Handle<v8::Value> WriteUcs2String(const v8::Arguments& args);

 protected:
  StreamWrap(v8::Handle<v8::Object> object, uv_stream_t* stream);
  virtual void SetHandle(uv_handle_t* h);
  void StateChange() { }
  void UpdateWriteQueueSize();

 private:
  static inline char* NewSlab(v8::Handle<v8::Object> global, v8::Handle<v8::Object> wrap_obj);

  // Callbacks for libuv
  static void AfterWrite(uv_write_t* req, int status);
  static uv_buf_t OnAlloc(uv_handle_t* handle, size_t suggested_size);
  static void AfterShutdown(uv_shutdown_t* req, int status);

  static void OnRead(uv_stream_t* handle, ssize_t nread, uv_buf_t buf);
  static void OnRead2(uv_pipe_t* handle, ssize_t nread, uv_buf_t buf,
      uv_handle_type pending);
  static void OnReadCommon(uv_stream_t* handle, ssize_t nread,
      uv_buf_t buf, uv_handle_type pending);

  template <enum WriteEncoding encoding>
  static v8::Handle<v8::Value> WriteStringImpl(const v8::Arguments& args);

  size_t slab_offset_;
  uv_stream_t* stream_;
};


}  // namespace node


#endif  // STREAM_WRAP_H_
