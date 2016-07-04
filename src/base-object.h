#ifndef SRC_BASE_OBJECT_H_
#define SRC_BASE_OBJECT_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

namespace node {

class Environment;

class BaseObject {
 public:
  inline BaseObject(Environment* env, v8::Local<v8::Object> handle);
  inline virtual ~BaseObject();

  // Returns the wrapped object.  Returns an empty handle when
  // persistent.IsEmpty() is true.
  inline v8::Local<v8::Object> object();

  // The parent class is responsible for calling .Reset() on destruction
  // when the persistent handle is strong because there is no way for
  // BaseObject to know when the handle goes out of scope.
  // Weak handles have been reset by the time the destructor runs but
  // calling .Reset() again is harmless.
  inline v8::Persistent<v8::Object>& persistent();

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

  v8::Persistent<v8::Object> persistent_handle_;
  Environment* env_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_H_
