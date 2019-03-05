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

#include "memory_tracker.h"
#include "v8.h"
#include <type_traits>  // std::remove_reference

namespace node {

class Environment;
template <typename T, bool kIsWeak>
class BaseObjectPtrImpl;

class BaseObject : public MemoryRetainer {
 public:
  enum InternalFields { kSlot, kInternalFieldCount };

  // Associates this object with `object`. It uses the 0th internal field for
  // that, and in particular aborts if there is no such field.
  inline BaseObject(Environment* env, v8::Local<v8::Object> object);
  inline ~BaseObject() override;

  BaseObject() = delete;

  // Returns the wrapped object.  Returns an empty handle when
  // persistent.IsEmpty() is true.
  inline v8::Local<v8::Object> object() const;

  // Same as the above, except it additionally verifies that this object
  // is associated with the passed Isolate in debug mode.
  inline v8::Local<v8::Object> object(v8::Isolate* isolate) const;

  inline v8::Global<v8::Object>& persistent();

  inline Environment* env() const;

  // Get a BaseObject* pointer, or subclass pointer, for the JS object that
  // was also passed to the `BaseObject()` constructor initially.
  // This may return `nullptr` if the C++ object has not been constructed yet,
  // e.g. when the JS object used `MakeLazilyInitializedJSTemplate`.
  static inline BaseObject* FromJSObject(v8::Local<v8::Value> object);
  template <typename T>
  static inline T* FromJSObject(v8::Local<v8::Value> object);

  // Make the `v8::Global` a weak reference and, `delete` this object once
  // the JS object has been garbage collected and there are no (strong)
  // BaseObjectPtr references to it.
  inline void MakeWeak();

  // Undo `MakeWeak()`, i.e. turn this into a strong reference that is a GC
  // root and will not be touched by the garbage collector.
  inline void ClearWeak();

  // Utility to create a FunctionTemplate with one internal field (used for
  // the `BaseObject*` pointer) and a constructor that initializes that field
  // to `nullptr`.
  static inline v8::Local<v8::FunctionTemplate> MakeLazilyInitializedJSTemplate(
      Environment* env);

  // Setter/Getter pair for internal fields that can be passed to SetAccessor.
  template <int Field>
  static void InternalFieldGet(v8::Local<v8::String> property,
                               const v8::PropertyCallbackInfo<v8::Value>& info);
  template <int Field, bool (v8::Value::* typecheck)() const>
  static void InternalFieldSet(v8::Local<v8::String> property,
                               v8::Local<v8::Value> value,
                               const v8::PropertyCallbackInfo<void>& info);

  // This is a bit of a hack. See the override in async_wrap.cc for details.
  virtual bool IsDoneInitializing() const;

  // Can be used to avoid this object keepling itself alive as a GC root
  // indefinitely, for example when this object is owned and deleted by another
  // BaseObject once that is torn down. This can only be called when there is
  // a BaseObjectPtr to this object.
  inline void Detach();

 protected:
  virtual inline void OnGCCollect();

 private:
  v8::Local<v8::Object> WrappedObject() const override;
  static void DeleteMe(void* data);

  // persistent_handle_ needs to be at a fixed offset from the start of the
  // class because it is used by src/node_postmortem_metadata.cc to calculate
  // offsets and generate debug symbols for BaseObject, which assumes that the
  // position of members in memory are predictable. For more information please
  // refer to `doc/guides/node-postmortem-support.md`
  friend int GenDebugSymbols();
  friend class CleanupHookCallback;
  template <typename T, bool kIsWeak>
  friend class BaseObjectPtrImpl;

  v8::Global<v8::Object> persistent_handle_;

  // Metadata that is associated with this BaseObject if there are BaseObjectPtr
  // or BaseObjectWeakPtr references to it.
  // This object is deleted when the BaseObject itself is destroyed, and there
  // are no weak references to it.
  struct PointerData {
    // Number of BaseObjectPtr instances that refer to this object. If this
    // is non-zero, the BaseObject is always a GC root and will not be destroyed
    // during cleanup until the count drops to zero again.
    unsigned int strong_ptr_count = 0;
    // Number of BaseObjectWeakPtr instances that refer to this object.
    unsigned int weak_ptr_count = 0;
    // Indicates whether MakeWeak() has been called.
    bool wants_weak_jsobj = false;
    // Indicates whether Detach() has been called. If that is the case, this
    // object will be destryoed once the strong pointer count drops to zero.
    bool is_detached = false;
    // Reference to the original BaseObject. This is used by weak pointers.
    BaseObject* self = nullptr;
  };

  inline bool has_pointer_data() const;
  // This creates a PointerData struct if none was associated with this
  // BaseObject before.
  inline PointerData* pointer_data();

  // Functions that adjust the strong pointer count.
  inline void decrease_refcount();
  inline void increase_refcount();

  Environment* env_;
  PointerData* pointer_data_ = nullptr;
};

// Global alias for FromJSObject() to avoid churn.
template <typename T>
inline T* Unwrap(v8::Local<v8::Value> obj) {
  return BaseObject::FromJSObject<T>(obj);
}


#define ASSIGN_OR_RETURN_UNWRAP(ptr, obj, ...)                                \
  do {                                                                        \
    *ptr = static_cast<typename std::remove_reference<decltype(*ptr)>::type>( \
        BaseObject::FromJSObject(obj));                                       \
    if (*ptr == nullptr)                                                      \
      return __VA_ARGS__;                                                     \
  } while (0)

// Implementation of a generic strong or weak pointer to a BaseObject.
// If strong, this will keep the target BaseObject alive regardless of other
// circumstances such as the GC or Environment cleanup.
// If weak, destruction behaviour is not affected, but the pointer will be
// reset to nullptr once the BaseObject is destroyed.
// The API matches std::shared_ptr closely.
template <typename T, bool kIsWeak>
class BaseObjectPtrImpl final {
 public:
  inline BaseObjectPtrImpl();
  inline ~BaseObjectPtrImpl();
  inline explicit BaseObjectPtrImpl(T* target);

  // Copy and move constructors. Note that the templated version is not a copy
  // or move constructor in the C++ sense of the word, so an identical
  // untemplated version is provided.
  template <typename U, bool kW>
  inline BaseObjectPtrImpl(const BaseObjectPtrImpl<U, kW>& other);
  inline BaseObjectPtrImpl(const BaseObjectPtrImpl& other);
  template <typename U, bool kW>
  inline BaseObjectPtrImpl& operator=(const BaseObjectPtrImpl<U, kW>& other);
  inline BaseObjectPtrImpl& operator=(const BaseObjectPtrImpl& other);
  inline BaseObjectPtrImpl(BaseObjectPtrImpl&& other);
  inline BaseObjectPtrImpl& operator=(BaseObjectPtrImpl&& other);

  inline void reset(T* ptr = nullptr);
  inline T* get() const;
  inline T& operator*() const;
  inline T* operator->() const;
  inline operator bool() const;

 private:
  union {
    BaseObject* target;  // Used for strong pointers.
    BaseObject::PointerData* pointer_data;  // Used for weak pointers.
  } data_;

  inline BaseObject* get_base_object() const;
  inline BaseObject::PointerData* pointer_data() const;
};

template <typename T>
using BaseObjectPtr = BaseObjectPtrImpl<T, false>;
template <typename T>
using BaseObjectWeakPtr = BaseObjectPtrImpl<T, true>;

// Create a BaseObject instance and return a pointer to it.
// This variant leaves the object as a GC root by default.
template <typename T, typename... Args>
inline BaseObjectPtr<T> MakeBaseObject(Args&&... args);
// Create a BaseObject instance and return a pointer to it.
// This variant detaches the object by default, meaning that the caller fully
// owns it, and once the last BaseObjectPtr to it is destroyed, the object
// itself is also destroyed.
template <typename T, typename... Args>
inline BaseObjectPtr<T> MakeDetachedBaseObject(Args&&... args);

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_BASE_OBJECT_H_
