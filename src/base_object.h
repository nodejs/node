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

#ifndef SRC_BASE_OBJECT_H_
#define SRC_BASE_OBJECT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "node_persistent.h"
#include "v8.h"

namespace node {

class Environment;

class BaseObject {
 public:
  inline BaseObject(Environment* env, v8::Local<v8::Object> handle);
  virtual ~BaseObject() = default;

  // Returns the wrapped object.  Returns an empty handle when
  // persistent.IsEmpty() is true.
  inline v8::Local<v8::Object> object();

  inline v8::MaybeLocal<v8::Value> GetField(std::string const& field);

  inline Persistent<v8::Object>& persistent();

  inline Environment* env() const;

  // The handle_ must have an internal field count > 0, and the first
  // index is reserved for a pointer to this class. This is an
  // implicit requirement, but Node does not have a case where it's
  // required that MakeWeak() be called and the internal field not
  // be set.
  template <typename Type>
  inline void MakeWeak(Type* ptr);

  inline void ClearWeak();

 private:
  BaseObject();

  template <typename Type>
  static inline void WeakCallback(
      const v8::WeakCallbackInfo<Type>& data);

  // persistent_handle_ needs to be at a fixed offset from the start of the
  // class because it is used by src/node_postmortem_metadata.cc to calculate
  // offsets and generate debug symbols for BaseObject, which assumes that the
  // position of members in memory are predictable. For more information please
  // refer to `doc/guides/node-postmortem-support.md`
  friend int GenDebugSymbols();
  Persistent<v8::Object> persistent_handle_;
  Environment* env_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_H_
