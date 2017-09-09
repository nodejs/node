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
#include <type_traits>  // std::remove_reference

namespace node {

class Environment;

class BaseObject {
 public:
  // Associates this object with `object`. It uses the 0th internal field for
  // that, and in particular aborts if there is no such field.
  inline BaseObject(Environment* env, v8::Local<v8::Object> object);
  virtual inline ~BaseObject();

  // Returns the wrapped object.  Returns an empty handle when
  // persistent.IsEmpty() is true.
  inline v8::Local<v8::Object> object();

  inline Persistent<v8::Object>& persistent();

  inline Environment* env() const;

  // Get a BaseObject* pointer, or subclass pointer, for the JS object that
  // was also passed to the `BaseObject()` constructor initially.
  // This may return `nullptr` if the C++ object has not been constructed yet,
  // e.g. when the JS object used `MakeLazilyInitializedJSTemplate`.
  static inline BaseObject* FromJSObject(v8::Local<v8::Object> object);
  template <typename T>
  static inline T* FromJSObject(v8::Local<v8::Object> object);

  // Make the `Persistent` a weak reference and, `delete` this object once
  // the JS object has been garbage collected.
  inline void MakeWeak();

  // Undo `MakeWeak()`, i.e. turn this into a strong reference.
  inline void ClearWeak();

  // Utility to create a FunctionTemplate with one internal field (used for
  // the `BaseObject*` pointer) and a constructor that initializes that field
  // to `nullptr`.
  static inline v8::Local<v8::FunctionTemplate> MakeLazilyInitializedJSTemplate(
      Environment* env);

 private:
  BaseObject();

  static inline void DeleteMe(void* data);

  // persistent_handle_ needs to be at a fixed offset from the start of the
  // class because it is used by src/node_postmortem_metadata.cc to calculate
  // offsets and generate debug symbols for BaseObject, which assumes that the
  // position of members in memory are predictable. For more information please
  // refer to `doc/guides/node-postmortem-support.md`
  friend int GenDebugSymbols();
  Persistent<v8::Object> persistent_handle_;
  Environment* env_;
};


// Global alias for FromJSObject() to avoid churn.
template <typename T>
inline T* Unwrap(v8::Local<v8::Object> obj) {
  return BaseObject::FromJSObject<T>(obj);
}


#define ASSIGN_OR_RETURN_UNWRAP(ptr, obj, ...)                                \
  do {                                                                        \
    *ptr = static_cast<typename std::remove_reference<decltype(*ptr)>::type>( \
        BaseObject::FromJSObject(obj));                                       \
    if (*ptr == nullptr)                                                      \
      return __VA_ARGS__;                                                     \
  } while (0)

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_H_
