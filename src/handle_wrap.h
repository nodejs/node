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

#ifndef SRC_HANDLE_WRAP_H_
#define SRC_HANDLE_WRAP_H_

#include "async-wrap.h"
#include "env.h"
#include "node.h"
#include "queue.h"
#include "uv.h"
#include "v8.h"

namespace node {

// Rules:
//
// - Do not throw from handle methods. Set errno.
//
// - MakeCallback may only be made directly off the event loop.
//   That is there can be no JavaScript stack frames underneath it.
//   (Is there any way to assert that?)
//
// - No use of v8::WeakReferenceCallback. The close callback signifies that
//   we're done with a handle - external resources can be freed.
//
// - Reusable?
//
// - The uv_close_cb is used to free the c++ object. The close callback
//   is not made into javascript land.
//
// - uv_ref, uv_unref counts are managed at this layer to avoid needless
//   js/c++ boundary crossing. At the javascript layer that should all be
//   taken care of.

class HandleWrap : public AsyncWrap {
 public:
  static void Close(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);

  inline uv_handle_t* GetHandle() { return handle__; }

 protected:
  HandleWrap(Environment* env,
             v8::Handle<v8::Object> object,
             uv_handle_t* handle,
             AsyncWrap::ProviderType provider,
             AsyncWrap* parent = NULL);
  virtual ~HandleWrap();

 private:
  friend void GetActiveHandles(const v8::FunctionCallbackInfo<v8::Value>&);
  static void OnClose(uv_handle_t* handle);
  QUEUE handle_wrap_queue_;
  unsigned int flags_;
  // Using double underscore due to handle_ member in tcp_wrap. Probably
  // tcp_wrap should rename it's member to 'handle'.
  uv_handle_t* handle__;

  static const unsigned int kUnref = 1;
  static const unsigned int kCloseCallback = 2;
};


}  // namespace node


#endif  // SRC_HANDLE_WRAP_H_
