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

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "async_wrap.h"
#include "util.h"
#include "uv.h"
#include "v8.h"

namespace node {

class Environment;

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
  static void HasRef(const v8::FunctionCallbackInfo<v8::Value>& args);

  static inline bool IsAlive(const HandleWrap* wrap) {
    return wrap != nullptr && wrap->state_ != kClosed;
  }

  static inline bool HasRef(const HandleWrap* wrap) {
    return IsAlive(wrap) && uv_has_ref(wrap->GetHandle());
  }

  inline uv_handle_t* GetHandle() const { return handle_; }

  virtual void Close(
      v8::Local<v8::Value> close_callback = v8::Local<v8::Value>());

  static v8::Local<v8::FunctionTemplate> GetConstructorTemplate(
      Environment* env);

 protected:
  HandleWrap(Environment* env,
             v8::Local<v8::Object> object,
             uv_handle_t* handle,
             AsyncWrap::ProviderType provider);
  virtual void OnClose() {}

  void MarkAsInitialized();
  void MarkAsUninitialized();

  inline bool IsHandleClosing() const {
    return state_ == kClosing || state_ == kClosed;
  }

 private:
  friend class Environment;
  friend void GetActiveHandles(const v8::FunctionCallbackInfo<v8::Value>&);
  static void OnClose(uv_handle_t* handle);

  // handle_wrap_queue_ needs to be at a fixed offset from the start of the
  // class because it is used by src/node_postmortem_metadata.cc to calculate
  // offsets and generate debug symbols for HandleWrap, which assumes that the
  // position of members in memory are predictable. For more information please
  // refer to `doc/guides/node-postmortem-support.md`
  friend int GenDebugSymbols();
  ListNode<HandleWrap> handle_wrap_queue_;
  enum { kInitialized, kClosing, kClosed } state_;
  uv_handle_t* const handle_;
};


}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_HANDLE_WRAP_H_
