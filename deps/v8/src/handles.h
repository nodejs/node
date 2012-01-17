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
    location_ = reinterpret_cast<T**>(handle.location());
  }

  INLINE(T* operator ->() const) { return operator*(); }

  // Check if this handle refers to the exact same object as the other handle.
  bool is_identical_to(const Handle<T> other) const {
    return operator*() == *other;
  }

  // Provides the C++ dereference operator.
  INLINE(T* operator*() const);

  // Returns the address to where the raw pointer is stored.
  T** location() const {
    ASSERT(location_ == NULL ||
           reinterpret_cast<Address>(*location_) != kZapValue);
    return location_;
  }

  template <class S> static Handle<T> cast(Handle<S> that) {
    T::cast(*that);
    return Handle<T>(reinterpret_cast<T**>(that.location()));
  }

  static Handle<T> null() { return Handle<T>(); }
  bool is_null() const { return location_ == NULL; }

  // Closes the given scope, but lets this handle escape. See
  // implementation in api.h.
  inline Handle<T> EscapeFrom(v8::HandleScope* scope);

 private:
  T** location_;
};


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
  inline HandleScope();
  explicit inline HandleScope(Isolate* isolate);

  inline ~HandleScope();

  // Counts the number of allocated handles.
  static int NumberOfHandles();

  // Creates a new handle with the given value.
  template <typename T>
  static inline T** CreateHandle(T* value, Isolate* isolate);

  // Deallocates any extensions used by the current scope.
  static void DeleteExtensions(Isolate* isolate);

  static Address current_next_address();
  static Address current_limit_address();
  static Address current_level_address();

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
  static internal::Object** Extend();

  // Zaps the handles in the half-open interval [start, end).
  static void ZapRange(internal::Object** start, internal::Object** end);

  friend class v8::HandleScope;
  friend class v8::ImplementationUtilities;
};


// ----------------------------------------------------------------------------
// Handle operations.
// They might invoke garbage collection. The result is an handle to
// an object of expected type, or the handle is an error if running out
// of space or encountering an internal error.

void NormalizeProperties(Handle<JSObject> object,
                         PropertyNormalizationMode mode,
                         int expected_additional_properties);
Handle<SeededNumberDictionary> NormalizeElements(Handle<JSObject> object);
void TransformToFastProperties(Handle<JSObject> object,
                               int unused_property_fields);
MUST_USE_RESULT Handle<SeededNumberDictionary> SeededNumberDictionarySet(
    Handle<SeededNumberDictionary> dictionary,
    uint32_t index,
    Handle<Object> value,
    PropertyDetails details);

// Flattens a string.
void FlattenString(Handle<String> str);

// Flattens a string and returns the underlying external or sequential
// string.
Handle<String> FlattenGetString(Handle<String> str);

Handle<Object> SetProperty(Handle<JSReceiver> object,
                           Handle<String> key,
                           Handle<Object> value,
                           PropertyAttributes attributes,
                           StrictModeFlag strict_mode);

Handle<Object> SetProperty(Handle<Object> object,
                           Handle<Object> key,
                           Handle<Object> value,
                           PropertyAttributes attributes,
                           StrictModeFlag strict_mode);

Handle<Object> ForceSetProperty(Handle<JSObject> object,
                                Handle<Object> key,
                                Handle<Object> value,
                                PropertyAttributes attributes);

Handle<Object> SetNormalizedProperty(Handle<JSObject> object,
                                     Handle<String> key,
                                     Handle<Object> value,
                                     PropertyDetails details);

Handle<Object> ForceDeleteProperty(Handle<JSObject> object,
                                   Handle<Object> key);

Handle<Object> SetLocalPropertyIgnoreAttributes(
    Handle<JSObject> object,
    Handle<String> key,
    Handle<Object> value,
    PropertyAttributes attributes);

// Used to set local properties on the object we totally control
// and which therefore has no accessors and alikes.
void SetLocalPropertyNoThrow(Handle<JSObject> object,
                             Handle<String> key,
                             Handle<Object> value,
                             PropertyAttributes attributes = NONE);

Handle<Object> SetPropertyWithInterceptor(Handle<JSObject> object,
                                          Handle<String> key,
                                          Handle<Object> value,
                                          PropertyAttributes attributes,
                                          StrictModeFlag strict_mode);

MUST_USE_RESULT Handle<Object> SetElement(Handle<JSObject> object,
                                          uint32_t index,
                                          Handle<Object> value,
                                          StrictModeFlag strict_mode);

Handle<Object> SetOwnElement(Handle<JSObject> object,
                             uint32_t index,
                             Handle<Object> value,
                             StrictModeFlag strict_mode);

Handle<Object> GetProperty(Handle<JSReceiver> obj,
                           const char* name);

Handle<Object> GetProperty(Handle<Object> obj,
                           Handle<Object> key);

Handle<Object> GetProperty(Handle<JSReceiver> obj,
                           Handle<String> name,
                           LookupResult* result);


Handle<Object> GetElement(Handle<Object> obj,
                          uint32_t index);

Handle<Object> GetPropertyWithInterceptor(Handle<JSObject> receiver,
                                          Handle<JSObject> holder,
                                          Handle<String> name,
                                          PropertyAttributes* attributes);

Handle<Object> GetPrototype(Handle<Object> obj);

Handle<Object> SetPrototype(Handle<JSObject> obj, Handle<Object> value);

// Return the object's hidden properties object. If the object has no hidden
// properties and HiddenPropertiesFlag::ALLOW_CREATION is passed, then a new
// hidden property object will be allocated. Otherwise Heap::undefined_value
// is returned.
Handle<Object> GetHiddenProperties(Handle<JSObject> obj,
                                   JSObject::HiddenPropertiesFlag flag);

int GetIdentityHash(Handle<JSObject> obj);

Handle<Object> DeleteElement(Handle<JSObject> obj, uint32_t index);
Handle<Object> DeleteProperty(Handle<JSObject> obj, Handle<String> prop);

Handle<Object> LookupSingleCharacterStringFromCode(uint32_t index);

Handle<JSObject> Copy(Handle<JSObject> obj);

Handle<Object> SetAccessor(Handle<JSObject> obj, Handle<AccessorInfo> info);

Handle<FixedArray> AddKeysFromJSArray(Handle<FixedArray>,
                                      Handle<JSArray> array);

// Get the JS object corresponding to the given script; create it
// if none exists.
Handle<JSValue> GetScriptWrapper(Handle<Script> script);

// Script line number computations.
void InitScriptLineEnds(Handle<Script> script);
// For string calculates an array of line end positions. If the string
// does not end with a new line character, this character may optionally be
// imagined.
Handle<FixedArray> CalculateLineEnds(Handle<String> string,
                                     bool with_imaginary_last_new_line);
int GetScriptLineNumber(Handle<Script> script, int code_position);
// The safe version does not make heap allocations but may work much slower.
int GetScriptLineNumberSafe(Handle<Script> script, int code_position);

// Computes the enumerable keys from interceptors. Used for debug mirrors and
// by GetKeysInFixedArrayFor below.
v8::Handle<v8::Array> GetKeysForNamedInterceptor(Handle<JSObject> receiver,
                                                 Handle<JSObject> object);
v8::Handle<v8::Array> GetKeysForIndexedInterceptor(Handle<JSObject> receiver,
                                                   Handle<JSObject> object);

enum KeyCollectionType { LOCAL_ONLY, INCLUDE_PROTOS };

// Computes the enumerable keys for a JSObject. Used for implementing
// "for (n in object) { }".
Handle<FixedArray> GetKeysInFixedArrayFor(Handle<JSObject> object,
                                          KeyCollectionType type);
Handle<JSArray> GetKeysFor(Handle<JSObject> object);
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

Handle<Object> PreventExtensions(Handle<JSObject> object);

Handle<ObjectHashTable> PutIntoObjectHashTable(Handle<ObjectHashTable> table,
                                               Handle<JSObject> key,
                                               Handle<Object> value);

// Does lazy compilation of the given function. Returns true on success and
// false if the compilation resulted in a stack overflow.
enum ClearExceptionFlag { KEEP_EXCEPTION, CLEAR_EXCEPTION };

bool EnsureCompiled(Handle<SharedFunctionInfo> shared,
                    ClearExceptionFlag flag);

bool CompileLazyShared(Handle<SharedFunctionInfo> shared,
                       ClearExceptionFlag flag);

bool CompileLazy(Handle<JSFunction> function, ClearExceptionFlag flag);

bool CompileOptimized(Handle<JSFunction> function,
                      int osr_ast_id,
                      ClearExceptionFlag flag);

class NoHandleAllocation BASE_EMBEDDED {
 public:
#ifndef DEBUG
  NoHandleAllocation() {}
  ~NoHandleAllocation() {}
#else
  inline NoHandleAllocation();
  inline ~NoHandleAllocation();
 private:
  int level_;
#endif
};

} }  // namespace v8::internal

#endif  // V8_HANDLES_H_
