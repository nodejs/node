// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_HANDLES_H_
#define V8_HANDLES_H_

#include "allocation.h"
#include "apiutils.h"

namespace v8 {
namespace internal {

// ----------------------------------------------------------------------------
// A Handle provides a reference to an object that survives relocation by
// the garbage collector.
// Handles are only valid within a HandleScope.
// When a handle is created for an object a cell is allocated in the heap.

template<typename T>
class Handle {
 public:
  INLINE(explicit Handle(T** location)) { location_ = location; }
  INLINE(explicit Handle(T* obj));
  INLINE(Handle(T* obj, Isolate* isolate));

  INLINE(Handle()) : location_(NULL) {}

  // Constructor for handling automatic up casting.
  // Ex. Handle<JSFunction> can be passed when Handle<Object> is expected.
  template <class S> Handle(Handle<S> handle) {
#ifdef DEBUG
    T* a = NULL;
    S* b = NULL;
    a = b;  // Fake assignment to enforce type checks.
    USE(a);
#endif
    location_ = reinterpret_cast<T**>(handle.location_);
  }

  INLINE(T* operator ->() const) { return operator*(); }

  // Check if this handle refers to the exact same object as the other handle.
  INLINE(bool is_identical_to(const Handle<T> other) const);

  // Provides the C++ dereference operator.
  INLINE(T* operator*() const);

  // Returns the address to where the raw pointer is stored.
  INLINE(T** location() const);

  template <class S> static Handle<T> cast(Handle<S> that) {
    T::cast(*reinterpret_cast<T**>(that.location_));
    return Handle<T>(reinterpret_cast<T**>(that.location_));
  }

  static Handle<T> null() { return Handle<T>(); }
  bool is_null() const { return location_ == NULL; }

  // Closes the given scope, but lets this handle escape. See
  // implementation in api.h.
  inline Handle<T> EscapeFrom(v8::HandleScope* scope);

#ifdef DEBUG
  bool IsDereferenceAllowed(bool allow_deferred) const;
#endif  // DEBUG

 private:
  T** location_;

  // Handles of different classes are allowed to access each other's location_.
  template<class S> friend class Handle;
};


// Convenience wrapper.
template<class T>
inline Handle<T> handle(T* t, Isolate* isolate) {
  return Handle<T>(t, isolate);
}


class DeferredHandles;
class HandleScopeImplementer;


// A stack-allocated class that governs a number of local handles.
// After a handle scope has been created, all local handles will be
// allocated within that handle scope until either the handle scope is
// deleted or another handle scope is created.  If there is already a
// handle scope and a new one is created, all allocations will take
// place in the new handle scope until it is deleted.  After that,
// new handles will again be allocated in the original handle scope.
//
// After the handle scope of a local handle has been deleted the
// garbage collector will no longer track the object stored in the
// handle and may deallocate it.  The behavior of accessing a handle
// for which the handle scope has been deleted is undefined.
class HandleScope {
 public:
  explicit inline HandleScope(Isolate* isolate);

  inline ~HandleScope();

  // Counts the number of allocated handles.
  static int NumberOfHandles(Isolate* isolate);

  // Creates a new handle with the given value.
  template <typename T>
  static inline T** CreateHandle(Isolate* isolate, T* value);

  // Deallocates any extensions used by the current scope.
  static void DeleteExtensions(Isolate* isolate);

  static Address current_next_address(Isolate* isolate);
  static Address current_limit_address(Isolate* isolate);
  static Address current_level_address(Isolate* isolate);

  // Closes the HandleScope (invalidating all handles
  // created in the scope of the HandleScope) and returns
  // a Handle backed by the parent scope holding the
  // value of the argument handle.
  template <typename T>
  Handle<T> CloseAndEscape(Handle<T> handle_value);

  Isolate* isolate() { return isolate_; }

 private:
  // Prevent heap allocation or illegal handle scopes.
  HandleScope(const HandleScope&);
  void operator=(const HandleScope&);
  void* operator new(size_t size);
  void operator delete(void* size_t);

  inline void CloseScope();

  Isolate* isolate_;
  Object** prev_next_;
  Object** prev_limit_;

  // Extend the handle scope making room for more handles.
  static internal::Object** Extend(Isolate* isolate);

#ifdef ENABLE_EXTRA_CHECKS
  // Zaps the handles in the half-open interval [start, end).
  static void ZapRange(internal::Object** start, internal::Object** end);
#endif

  friend class v8::HandleScope;
  friend class v8::internal::DeferredHandles;
  friend class v8::internal::HandleScopeImplementer;
  friend class v8::internal::Isolate;
};


class DeferredHandles;


class DeferredHandleScope {
 public:
  explicit DeferredHandleScope(Isolate* isolate);
  // The DeferredHandles object returned stores the Handles created
  // since the creation of this DeferredHandleScope.  The Handles are
  // alive as long as the DeferredHandles object is alive.
  DeferredHandles* Detach();
  ~DeferredHandleScope();

 private:
  Object** prev_limit_;
  Object** prev_next_;
  HandleScopeImplementer* impl_;

#ifdef DEBUG
  bool handles_detached_;
  int prev_level_;
#endif

  friend class HandleScopeImplementer;
};


// ----------------------------------------------------------------------------
// Handle operations.
// They might invoke garbage collection. The result is an handle to
// an object of expected type, or the handle is an error if running out
// of space or encountering an internal error.

// Flattens a string.
void FlattenString(Handle<String> str);

// Flattens a string and returns the underlying external or sequential
// string.
Handle<String> FlattenGetString(Handle<String> str);

Handle<Object> SetProperty(Isolate* isolate,
                           Handle<Object> object,
                           Handle<Object> key,
                           Handle<Object> value,
                           PropertyAttributes attributes,
                           StrictModeFlag strict_mode);

Handle<Object> ForceSetProperty(Handle<JSObject> object,
                                Handle<Object> key,
                                Handle<Object> value,
                                PropertyAttributes attributes);

Handle<Object> DeleteProperty(Handle<JSObject> object, Handle<Object> key);

Handle<Object> ForceDeleteProperty(Handle<JSObject> object, Handle<Object> key);

Handle<Object> HasProperty(Handle<JSReceiver> obj, Handle<Object> key);

Handle<Object> GetProperty(Handle<JSReceiver> obj, const char* name);

Handle<Object> GetProperty(Isolate* isolate,
                           Handle<Object> obj,
                           Handle<Object> key);

Handle<Object> SetPrototype(Handle<JSObject> obj, Handle<Object> value);

Handle<Object> LookupSingleCharacterStringFromCode(Isolate* isolate,
                                                   uint32_t index);

Handle<JSObject> Copy(Handle<JSObject> obj);

Handle<JSObject> DeepCopy(Handle<JSObject> obj);

Handle<Object> SetAccessor(Handle<JSObject> obj, Handle<AccessorInfo> info);

Handle<FixedArray> AddKeysFromJSArray(Handle<FixedArray>,
                                      Handle<JSArray> array);

// Get the JS object corresponding to the given script; create it
// if none exists.
Handle<JSValue> GetScriptWrapper(Handle<Script> script);

// Script line number computations. Note that the line number is zero-based.
void InitScriptLineEnds(Handle<Script> script);
// For string calculates an array of line end positions. If the string
// does not end with a new line character, this character may optionally be
// imagined.
Handle<FixedArray> CalculateLineEnds(Handle<String> string,
                                     bool with_imaginary_last_new_line);
int GetScriptLineNumber(Handle<Script> script, int code_position);
// The safe version does not make heap allocations but may work much slower.
int GetScriptLineNumberSafe(Handle<Script> script, int code_position);
int GetScriptColumnNumber(Handle<Script> script, int code_position);
Handle<Object> GetScriptNameOrSourceURL(Handle<Script> script);

// Computes the enumerable keys from interceptors. Used for debug mirrors and
// by GetKeysInFixedArrayFor below.
v8::Handle<v8::Array> GetKeysForNamedInterceptor(Handle<JSReceiver> receiver,
                                                 Handle<JSObject> object);
v8::Handle<v8::Array> GetKeysForIndexedInterceptor(Handle<JSReceiver> receiver,
                                                   Handle<JSObject> object);

enum KeyCollectionType { LOCAL_ONLY, INCLUDE_PROTOS };

// Computes the enumerable keys for a JSObject. Used for implementing
// "for (n in object) { }".
Handle<FixedArray> GetKeysInFixedArrayFor(Handle<JSReceiver> object,
                                          KeyCollectionType type,
                                          bool* threw);
Handle<JSArray> GetKeysFor(Handle<JSReceiver> object, bool* threw);
Handle<FixedArray> ReduceFixedArrayTo(Handle<FixedArray> array, int length);
Handle<FixedArray> GetEnumPropertyKeys(Handle<JSObject> object,
                                       bool cache_result);

// Computes the union of keys and return the result.
// Used for implementing "for (n in object) { }"
Handle<FixedArray> UnionOfKeys(Handle<FixedArray> first,
                               Handle<FixedArray> second);

Handle<String> SubString(Handle<String> str,
                         int start,
                         int end,
                         PretenureFlag pretenure = NOT_TENURED);

// Sets the expected number of properties for the function's instances.
void SetExpectedNofProperties(Handle<JSFunction> func, int nof);

// Sets the prototype property for a function instance.
void SetPrototypeProperty(Handle<JSFunction> func, Handle<JSObject> value);

// Sets the expected number of properties based on estimate from compiler.
void SetExpectedNofPropertiesFromEstimate(Handle<SharedFunctionInfo> shared,
                                          int estimate);


Handle<JSGlobalProxy> ReinitializeJSGlobalProxy(
    Handle<JSFunction> constructor,
    Handle<JSGlobalProxy> global);

Handle<Object> SetPrototype(Handle<JSFunction> function,
                            Handle<Object> prototype);

Handle<ObjectHashSet> ObjectHashSetAdd(Handle<ObjectHashSet> table,
                                       Handle<Object> key);

Handle<ObjectHashSet> ObjectHashSetRemove(Handle<ObjectHashSet> table,
                                          Handle<Object> key);

Handle<ObjectHashTable> PutIntoObjectHashTable(Handle<ObjectHashTable> table,
                                               Handle<Object> key,
                                               Handle<Object> value);

class NoHandleAllocation BASE_EMBEDDED {
 public:
#ifndef DEBUG
  explicit NoHandleAllocation(Isolate* isolate) {}
  ~NoHandleAllocation() {}
#else
  explicit inline NoHandleAllocation(Isolate* isolate);
  inline ~NoHandleAllocation();
 private:
  Isolate* isolate_;
  int level_;
  bool active_;
#endif
};


class HandleDereferenceGuard BASE_EMBEDDED {
 public:
  enum State { ALLOW, DISALLOW, DISALLOW_DEFERRED };
#ifndef DEBUG
  HandleDereferenceGuard(Isolate* isolate, State state) { }
  ~HandleDereferenceGuard() { }
#else
  inline HandleDereferenceGuard(Isolate* isolate,  State state);
  inline ~HandleDereferenceGuard();
 private:
  Isolate* isolate_;
  State old_state_;
#endif
};

#ifdef DEBUG
#define ALLOW_HANDLE_DEREF(isolate, why_this_is_safe)                          \
  HandleDereferenceGuard allow_deref(isolate,                                  \
                                     HandleDereferenceGuard::ALLOW);
#else
#define ALLOW_HANDLE_DEREF(isolate, why_this_is_safe)
#endif  // DEBUG

} }  // namespace v8::internal

#endif  // V8_HANDLES_H_
