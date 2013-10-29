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

#ifndef SRC_ASYNC_WRAP_H_
#define SRC_ASYNC_WRAP_H_

#include "env.h"
#include "v8.h"

namespace node {

class AsyncWrap {
 public:
  enum AsyncFlags {
    NO_OPTIONS = 0,
    ASYNC_LISTENERS = 1
  };

  inline AsyncWrap(Environment* env, v8::Handle<v8::Object> object);

  inline ~AsyncWrap();

  template <typename Type>
  static inline void AddMethods(v8::Handle<v8::FunctionTemplate> t);

  inline uint32_t async_flags() const;

  inline void set_flag(unsigned int flag);

  inline void remove_flag(unsigned int flag);

  inline bool has_async_queue();

  inline Environment* env() const;

  // Returns the wrapped object.  Illegal to call in your destructor.
  inline v8::Local<v8::Object> object();

  // Parent class is responsible to Dispose.
  inline v8::Persistent<v8::Object>& persistent();

  // Only call these within a valid HandleScope.
  inline v8::Handle<v8::Value> MakeCallback(const v8::Handle<v8::Function> cb,
                                            int argc,
                                            v8::Handle<v8::Value>* argv);
  inline v8::Handle<v8::Value> MakeCallback(const v8::Handle<v8::String> symbol,
                                            int argc,
                                            v8::Handle<v8::Value>* argv);
  inline v8::Handle<v8::Value> MakeCallback(uint32_t index,
                                            int argc,
                                            v8::Handle<v8::Value>* argv);

 private:
  // Add an async listener to an existing handle.
  template <typename Type>
  static inline void AddAsyncListener(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  // Remove an async listener to an existing handle.
  template <typename Type>
  static inline void RemoveAsyncListener(
      const v8::FunctionCallbackInfo<v8::Value>& args);

  v8::Persistent<v8::Object> object_;
  Environment* const env_;
  uint32_t async_flags_;
};

}  // namespace node


#endif  // SRC_ASYNC_WRAP_H_
