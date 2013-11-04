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

#ifndef SRC_NODE_STAT_WATCHER_H_
#define SRC_NODE_STAT_WATCHER_H_

#include "node.h"
#include "async-wrap.h"
#include "env.h"
#include "uv.h"
#include "v8.h"

namespace node {

class StatWatcher : public AsyncWrap {
 public:
  virtual ~StatWatcher();

  static void Initialize(v8::Handle<v8::Object> target);

 protected:
  StatWatcher(Environment* env, v8::Local<v8::Object> wrap);

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Start(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Stop(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  static void Callback(uv_fs_poll_t* handle,
                       int status,
                       const uv_stat_t* prev,
                       const uv_stat_t* curr);
  void Stop();

  uv_fs_poll_t* watcher_;
};

}  // namespace node
#endif  // SRC_NODE_STAT_WATCHER_H_
