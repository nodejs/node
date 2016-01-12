// Copyright 2012 the V8 project authors. All rights reserved.
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

#pragma once

// Stops windows.h from including winsock.h (conflicting with winsock2.h).
#ifndef _WINSOCKAPI_
#define _WINSOCKAPI_
#endif

#if defined(_M_ARM)
#define CHAKRA_MIN_WIN32_WINNT _WIN32_WINNT_WIN10
#define CHAKRA_MIN_WIN32_WINNT_STR "_WIN32_WINNT_WIN10"
#else
#define CHAKRA_MIN_WIN32_WINNT _WIN32_WINNT_WIN7
#define CHAKRA_MIN_WIN32_WINNT_STR "_WIN32_WINNT_WIN7"
#endif

#if defined(_WIN32_WINNT) && (_WIN32_WINNT < CHAKRA_MIN_WIN32_WINNT)
#pragma message("warning: chakrashim requires minimum " \
                CHAKRA_MIN_WIN32_WINNT_STR \
                ". Redefine _WIN32_WINNT to " \
                CHAKRA_MIN_WIN32_WINNT_STR ".")
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT CHAKRA_MIN_WIN32_WINNT
#endif

#ifndef USE_EDGEMODE_JSRT
#define USE_EDGEMODE_JSRT     // Only works with edge JSRT
#endif

#include "chakracore.h"

#include <stdio.h>
#include <stdint.h>
#include <memory>
#include "v8-version.h"
#include "v8config.h"

#ifdef BUILDING_CHAKRASHIM
#define V8_EXPORT __declspec(dllexport)
#else
#define V8_EXPORT __declspec(dllimport)
#endif

#define TYPE_CHECK(T, S)                                       \
  while (false) {                                              \
    *(static_cast<T* volatile*>(0)) = static_cast<S*>(0);      \
  }

namespace v8 {

class AccessorSignature;
class Array;
class Value;
class External;
class Primitive;
class Boolean;
class BooleanObject;
class Context;
class CpuProfiler;
class EscapableHandleScope;
class Function;
class FunctionTemplate;
class HeapProfiler;
class Int32;
class Integer;
class Isolate;
class Name;
class Number;
class NumberObject;
class Object;
class ObjectTemplate;
class Platform;
class ResourceConstraints;
class RegExp;
class Promise;
class Script;
class Signature;
class StartupData;
class StackFrame;
class String;
class StringObject;
class Uint32;
template <class T> class Local;
template <class T> class Maybe;
template <class T> class MaybeLocal;
template <class T> class NonCopyablePersistentTraits;
template <class T> class PersistentBase;
template <class T, class M = NonCopyablePersistentTraits<T> > class Persistent;
template <typename T> class FunctionCallbackInfo;
template <typename T> class PropertyCallbackInfo;

class JitCodeEvent;
class RetainedObjectInfo;
struct ExternalArrayData;

enum PropertyAttribute {
  None = 0,
  ReadOnly = 1 << 0,
  DontEnum = 1 << 1,
  DontDelete = 1 << 2,
};

enum ExternalArrayType {
  kExternalInt8Array = 1,
  kExternalUint8Array,
  kExternalInt16Array,
  kExternalUint16Array,
  kExternalInt32Array,
  kExternalUint32Array,
  kExternalFloat32Array,
  kExternalFloat64Array,
  kExternalUint8ClampedArray,

  // Legacy constant names
  kExternalByteArray = kExternalInt8Array,
  kExternalUnsignedByteArray = kExternalUint8Array,
  kExternalShortArray = kExternalInt16Array,
  kExternalUnsignedShortArray = kExternalUint16Array,
  kExternalIntArray = kExternalInt32Array,
  kExternalUnsignedIntArray = kExternalUint32Array,
  kExternalFloatArray = kExternalFloat32Array,
  kExternalDoubleArray = kExternalFloat64Array,
  kExternalPixelArray = kExternalUint8ClampedArray
};

enum AccessControl {
  DEFAULT = 0,
  ALL_CAN_READ = 1,
  ALL_CAN_WRITE = 1 << 1,
  PROHIBITS_OVERWRITING = 1 << 2,
};

enum JitCodeEventOptions {
  kJitCodeEventDefault = 0,
  kJitCodeEventEnumExisting = 1,
};

typedef void (*AccessorGetterCallback)(
  Local<String> property,
  const PropertyCallbackInfo<Value>& info);
typedef void (*AccessorNameGetterCallback)(
  Local<Name> property,
  const PropertyCallbackInfo<Value>& info);

typedef void (*AccessorSetterCallback)(
  Local<String> property,
  Local<Value> value,
  const PropertyCallbackInfo<void>& info);
typedef void (*AccessorNameSetterCallback)(
  Local<Name> property,
  Local<Value> value,
  const PropertyCallbackInfo<void>& info);

typedef void (*NamedPropertyGetterCallback)(
  Local<String> property, const PropertyCallbackInfo<Value>& info);
typedef void (*NamedPropertySetterCallback)(
  Local<String> property,
  Local<Value> value,
  const PropertyCallbackInfo<Value>& info);
typedef void (*NamedPropertyQueryCallback)(
  Local<String> property, const PropertyCallbackInfo<Integer>& info);
typedef void (*NamedPropertyDeleterCallback)(
  Local<String> property, const PropertyCallbackInfo<Boolean>& info);
typedef void (*NamedPropertyEnumeratorCallback)(
  const PropertyCallbackInfo<Array>& info);

typedef void (*GenericNamedPropertyGetterCallback)(
  Local<Name> property, const PropertyCallbackInfo<Value>& info);
typedef void (*GenericNamedPropertySetterCallback)(
  Local<Name> property, Local<Value> value,
  const PropertyCallbackInfo<Value>& info);
typedef void (*GenericNamedPropertyQueryCallback)(
  Local<Name> property, const PropertyCallbackInfo<Integer>& info);
typedef void (*GenericNamedPropertyDeleterCallback)(
  Local<Name> property, const PropertyCallbackInfo<Boolean>& info);
typedef void (*GenericNamedPropertyEnumeratorCallback)(
  const PropertyCallbackInfo<Array>& info);

typedef void (*IndexedPropertyGetterCallback)(
  uint32_t index, const PropertyCallbackInfo<Value>& info);
typedef void (*IndexedPropertySetterCallback)(
  uint32_t index, Local<Value> value, const PropertyCallbackInfo<Value>& info);
typedef void (*IndexedPropertyQueryCallback)(
  uint32_t index, const PropertyCallbackInfo<Integer>& info);
typedef void (*IndexedPropertyDeleterCallback)(
  uint32_t index, const PropertyCallbackInfo<Boolean>& info);
typedef void (*IndexedPropertyEnumeratorCallback)(
  const PropertyCallbackInfo<Array>& info);

typedef bool (*EntropySource)(unsigned char* buffer, size_t length);
typedef void (*FatalErrorCallback)(const char *location, const char *message);
typedef void (*JitCodeEventHandler)(const JitCodeEvent *event);


template <class T>
class Local {
 public:
  V8_INLINE Local() : val_(0) {}

  template <class S>
  V8_INLINE Local(Local<S> that)
      : val_(reinterpret_cast<T*>(*that)) {
    TYPE_CHECK(T, S);
  }

  V8_INLINE bool IsEmpty() const { return val_ == 0; }
  V8_INLINE void Clear() { val_ = 0; }
  V8_INLINE T* operator->() const { return val_; }
  V8_INLINE T* operator*() const { return val_; }

  template <class S>
  V8_INLINE bool operator==(const Local<S>& that) const {
    return val_ == that.val_;
  }

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const {
    return val_ == that.val_;
  }

  template <class S>
  V8_INLINE bool operator!=(const Local<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const PersistentBase<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE static Local<T> Cast(Local<S> that) {
    return Local<T>(T::Cast(*that));
  }

  template <class S>
  V8_INLINE Local<S> As() {
    return Local<S>::Cast(*this);
  }

  V8_INLINE static Local<T> New(Isolate* isolate, Local<T> that);
  V8_INLINE static Local<T> New(Isolate* isolate,
                                const PersistentBase<T>& that);

 private:
  friend struct AcessorExternalDataType;
  friend class AccessorSignature;
  friend class Array;
  friend class ArrayBuffer;
  friend class ArrayBufferView;
  friend class Boolean;
  friend class BooleanObject;
  friend class Context;
  friend class Date;
  friend class Debug;
  friend class External;
  friend class Function;
  friend struct FunctionCallbackData;
  friend class FunctionTemplate;
  friend struct FunctionTemplateData;
  friend class HandleScope;
  friend class Integer;
  friend class Number;
  friend class NumberObject;
  friend class Object;
  friend struct ObjectData;
  friend class ObjectTemplate;
  friend class Signature;
  friend class Script;
  friend class StackFrame;
  friend class StackTrace;
  friend class String;
  friend class StringObject;
  friend class Utils;
  friend class TryCatch;
  friend class UnboundScript;
  friend class Value;
  template <class F> friend class FunctionCallbackInfo;
  template <class F> friend class MaybeLocal;
  template <class F> friend class PersistentBase;
  template <class F, class M> friend class Persistent;
  friend V8_EXPORT Local<Primitive> Undefined(Isolate* isolate);
  friend V8_EXPORT Local<Primitive> Null(Isolate* isolate);
  friend V8_EXPORT Local<Boolean> True(Isolate* isolate);
  friend V8_EXPORT Local<Boolean> False(Isolate* isolate);

  template <class S>
  V8_INLINE Local(S* that)
      : val_(that) {}
  V8_INLINE static Local<T> New(Isolate* isolate, T* that) {
    return New(that);
  }

  V8_INLINE Local(JsValueRef that)
    : val_(static_cast<T*>(that)) {}
  V8_INLINE Local(const PersistentBase<T>& that)
    : val_(that.val_) {
  }
  V8_INLINE static Local<T> New(T* that);
  V8_INLINE static Local<T> New(JsValueRef ref) {
    return New(static_cast<T*>(ref));
  }

  T* val_;
};


// Handle is an alias for Local for historical reasons.
template <class T>
using Handle = Local<T>;


template <class T>
class MaybeLocal {
 public:
  MaybeLocal() : val_(nullptr) {}
  template <class S>
  MaybeLocal(Local<S> that)
    : val_(reinterpret_cast<T*>(*that)) {
    TYPE_CHECK(T, S);
  }

  bool IsEmpty() const { return val_ == nullptr; }

  template <class S>
  bool ToLocal(Local<S>* out) const {
    out->val_ = IsEmpty() ? nullptr : this->val_;
    return !IsEmpty();
  }

  Local<T> ToLocalChecked() {
    if (V8_UNLIKELY(val_ == nullptr)) V8::ToLocalEmpty();
    return Local<T>(val_);
  }

  template <class S>
  Local<S> FromMaybe(Local<S> default_value) const {
    return IsEmpty() ? default_value : Local<S>(val_);
  }

 private:
  T* val_;
};


static const int kInternalFieldsInWeakCallback = 2;


template <typename T>
class WeakCallbackInfo {
 public:
  typedef void (*Callback)(const WeakCallbackInfo<T>& data);

  WeakCallbackInfo(Isolate* isolate, T* parameter,
                   void* internal_fields[kInternalFieldsInWeakCallback],
                   Callback* callback)
      : isolate_(isolate), parameter_(parameter), callback_(callback) {
    for (int i = 0; i < kInternalFieldsInWeakCallback; ++i) {
      internal_fields_[i] = internal_fields[i];
    }
  }

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE T* GetParameter() const { return parameter_; }
  V8_INLINE void* GetInternalField(int index) const {
    return internal_fields_[index];
  }

  V8_INLINE V8_DEPRECATE_SOON("use indexed version",
                              void* GetInternalField1()) const {
    return internal_fields_[0];
  }
  V8_INLINE V8_DEPRECATE_SOON("use indexed version",
                              void* GetInternalField2()) const {
    return internal_fields_[1];
  }

  bool IsFirstPass() const { return callback_ != nullptr; }
  void SetSecondPassCallback(Callback callback) const { *callback_ = callback; }

 private:
  Isolate* isolate_;
  T* parameter_;
  Callback* callback_;
  void* internal_fields_[kInternalFieldsInWeakCallback];
};


template <class T, class P>
class WeakCallbackData {
 public:
  typedef void (*Callback)(const WeakCallbackData<T, P>& data);

  WeakCallbackData(Isolate* isolate, P* parameter, Local<T> handle)
      : isolate_(isolate), parameter_(parameter), handle_(handle) {}

  V8_INLINE Isolate* GetIsolate() const { return isolate_; }
  V8_INLINE P* GetParameter() const { return parameter_; }
  V8_INLINE Local<T> GetValue() const { return handle_; }

 private:
  Isolate* isolate_;
  P* parameter_;
  Local<T> handle_;
};


namespace chakrashim {
struct WeakReferenceCallbackWrapper {
  void *parameters;
  union {
    WeakCallbackInfo<void>::Callback infoCallback;
    WeakCallbackData<Value, void>::Callback dataCallback;
  };
  bool isWeakCallbackInfo;
};
template class V8_EXPORT std::shared_ptr<WeakReferenceCallbackWrapper>;

// A helper method for setting an object with a WeakReferenceCallback. The
// callback will be called before the object is released.
V8_EXPORT void SetObjectWeakReferenceCallback(
  JsValueRef object,
  WeakCallbackInfo<void>::Callback callback,
  void* parameters,
  std::shared_ptr<WeakReferenceCallbackWrapper>* weakWrapper);
V8_EXPORT void SetObjectWeakReferenceCallback(
  JsValueRef object,
  WeakCallbackData<Value, void>::Callback callback,
  void* parameters,
  std::shared_ptr<WeakReferenceCallbackWrapper>* weakWrapper);
// A helper method for turning off the WeakReferenceCallback that was set using
// the previous method
V8_EXPORT void ClearObjectWeakReferenceCallback(JsValueRef object, bool revive);
}

enum class WeakCallbackType { kParameter, kInternalFields };

template <class T>
class PersistentBase {
 public:
  V8_INLINE void Reset();

  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const Handle<S>& other);

  template <class S>
  V8_INLINE void Reset(Isolate* isolate, const PersistentBase<S>& other);

  V8_INLINE bool IsEmpty() const { return val_ == NULL; }
  V8_INLINE void Empty() { Reset(); }

  template <class S>
  V8_INLINE bool operator==(const PersistentBase<S>& that) const {
    return val_ == that.val_;
  }

  template <class S>
  V8_INLINE bool operator==(const Handle<S>& that) const {
    return val_ == that.val_;
  }

  template <class S>
  V8_INLINE bool operator!=(const PersistentBase<S>& that) const {
    return !operator==(that);
  }

  template <class S>
  V8_INLINE bool operator!=(const Handle<S>& that) const {
    return !operator==(that);
  }

  template <typename P>
  V8_INLINE V8_DEPRECATE_SOON(
      "use WeakCallbackInfo version",
      void SetWeak(P* parameter,
                   typename WeakCallbackData<T, P>::Callback callback));

  template <typename P>
  V8_INLINE void SetWeak(P* parameter,
                         typename WeakCallbackInfo<P>::Callback callback,
                         WeakCallbackType type);

  template<typename P>
  V8_INLINE P* ClearWeak();

  V8_INLINE void ClearWeak() { ClearWeak<void>(); }
  V8_INLINE void MarkIndependent();
  V8_INLINE void MarkPartiallyDependent();
  V8_INLINE bool IsIndependent() const;
  V8_INLINE bool IsNearDeath() const;
  V8_INLINE bool IsWeak() const;
  V8_INLINE void SetWrapperClassId(uint16_t class_id);

 private:
  template<class F> friend class Local;
  template<class F1, class F2> friend class Persistent;

  explicit V8_INLINE PersistentBase(T* val) : val_(val) {}
  PersistentBase(PersistentBase& other) = delete;  // NOLINT
  void operator=(PersistentBase&) = delete;
  V8_INLINE static T* New(Isolate* isolate, T* that);

  template <typename P, typename Callback>
  void SetWeakCommon(P* parameter, Callback callback);

  T* val_;
  std::shared_ptr<chakrashim::WeakReferenceCallbackWrapper> _weakWrapper;
};


template<class T>
class NonCopyablePersistentTraits {
 public:
  typedef Persistent<T, NonCopyablePersistentTraits<T> > NonCopyablePersistent;
  static const bool kResetInDestructor = true;  // chakra: changed to true!
  template<class S, class M>
  V8_INLINE static void Copy(const Persistent<S, M>& source,
                             NonCopyablePersistent* dest) {
    Uncompilable<Object>();
  }
  template<class O> V8_INLINE static void Uncompilable() {
    TYPE_CHECK(O, Primitive);
  }
};


template<class T>
struct CopyablePersistentTraits {
  typedef Persistent<T, CopyablePersistentTraits<T> > CopyablePersistent;
  static const bool kResetInDestructor = true;
  template<class S, class M>
  static V8_INLINE void Copy(const Persistent<S, M>& source,
                             CopyablePersistent* dest) {
    // do nothing, just allow copy
  }
};


template <class T, class M>
class Persistent : public PersistentBase<T> {
 public:
  V8_INLINE Persistent() : PersistentBase<T>(0) { }

  template <class S>
  V8_INLINE Persistent(Isolate* isolate, Handle<S> that)
      : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }

  template <class S, class M2>
  V8_INLINE Persistent(Isolate* isolate, const Persistent<S, M2>& that)
    : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }

  V8_INLINE Persistent(const Persistent& that) : PersistentBase<T>(0) {
    Copy(that);
  }

  template <class S, class M2>
  V8_INLINE Persistent(const Persistent<S, M2>& that) : PersistentBase<T>(0) {
    Copy(that);
  }

  V8_INLINE Persistent& operator=(const Persistent& that) { // NOLINT
    Copy(that);
    return *this;
  }

  template <class S, class M2>
  V8_INLINE Persistent& operator=(const Persistent<S, M2>& that) { // NOLINT
    Copy(that);
    return *this;
  }

  V8_INLINE ~Persistent() {
    if (M::kResetInDestructor) this->Reset();
  }

  template <class S>
  V8_INLINE static Persistent<T>& Cast(Persistent<S>& that) { // NOLINT
    return reinterpret_cast<Persistent<T>&>(that);
  }

  template <class S>
  V8_INLINE Persistent<S>& As() { // NOLINT
    return Persistent<S>::Cast(*this);
  }

 private:
  friend class Object;
  friend struct ObjectData;
  friend class ObjectTemplate;
  friend struct ObjectTemplateData;
  friend struct TemplateData;
  friend struct FunctionCallbackData;
  friend class FunctionTemplate;
  friend struct FunctionTemplateData;
  friend class Utils;
  template <class F> friend class Local;
  template <class F> friend class ReturnValue;

  V8_INLINE Persistent(T* that)
    : PersistentBase<T>(PersistentBase<T>::New(nullptr, that)) { }

  V8_INLINE T* operator*() const { return this->val_; }
  V8_INLINE T* operator->() const { return this->val_; }

  template<class S, class M2>
  V8_INLINE void Copy(const Persistent<S, M2>& that);

  template <class S>
  V8_INLINE Persistent& operator=(const Local<S>& other) {
    Reset(nullptr, other);
    return *this;
  }
  V8_INLINE Persistent& operator=(JsRef other) {
    return operator=(Local<T>(static_cast<T*>(other)));
  }
};


template <class T>
class Global : public PersistentBase<T> {
 public:
  V8_INLINE Global() : PersistentBase<T>(nullptr) {}

  template <class S>
  V8_INLINE Global(Isolate* isolate, Handle<S> that)
    : PersistentBase<T>(PersistentBase<T>::New(isolate, *that)) {
    TYPE_CHECK(T, S);
  }

  template <class S>
  V8_INLINE Global(Isolate* isolate, const PersistentBase<S>& that)
    : PersistentBase<T>(PersistentBase<T>::New(isolate, that.val_)) {
    TYPE_CHECK(T, S);
  }

  V8_INLINE Global(Global&& other) : PersistentBase<T>(other.val_) {
    this._weakWrapper = other._weakWrapper;
    other.val_ = nullptr;
    other._weakWrapper.reset();
  }

  V8_INLINE ~Global() { this->Reset(); }

  template <class S>
  V8_INLINE Global& operator=(Global<S>&& rhs) {
    TYPE_CHECK(T, S);
    if (this != &rhs) {
      this->Reset();
      this->val_ = rhs.val_;
      this->_weakWrapper = rhs._weakWrapper;
      rhs.val_ = nullptr;
      rhs._weakWrapper.reset();
    }
    return *this;
  }

  Global Pass() { return static_cast<Global&&>(*this); }

 private:
  Global(Global&) = delete;
  void operator=(Global&) = delete;
};


template <class T>
class Eternal : private Persistent<T> {
 public:
  Eternal() {}

  template<class S>
  Eternal(Isolate* isolate, Local<S> handle) {
    Set(isolate, handle);
  }

  Local<T> Get(Isolate* isolate) {
    return Local<T>::New(isolate, *this);
  }

  template<class S> void Set(Isolate* isolate, Local<S> handle) {
    Reset(isolate, handle);
  }
};

// CHAKRA: Chakra's GC behavior does not exactly match up with V8's GC behavior.
// V8 uses a HandleScope to keep Local references alive, which means that as
// long as the HandleScope is on the stack, the Local references will not be
// collected. Chakra, on the other hand, directly walks the stack and has no
// HandleScope mechanism. It requires hosts to keep "local" references on the
// stack or else turn them into "persistent" references through
// JsAddRef/JsRelease. To paper over this difference, the bridge HandleScope
// will create a JS array and will hold that reference on the stack. Any local
// values created will then be added to that array. So the GC will see the array
// on the stack and then keep those local references alive.
class V8_EXPORT HandleScope {
 public:
  HandleScope(Isolate* isolate);
  ~HandleScope();

  static int NumberOfHandles(Isolate* isolate);

 private:
  friend class EscapableHandleScope;
  template <class T> friend class Local;
  static const int kOnStackLocals = 5;  // Arbitrary number of refs on stack

  JsValueRef _locals[kOnStackLocals];   // Save some refs on stack
  JsValueRef _refs;                     // More refs go to a JS array
  int _count;
  HandleScope *_prev;
  JsContextRef _contextRef;
  struct AddRefRecord {
    JsRef _ref;
    AddRefRecord *  _next;
  } *_addRefRecordHead;

  bool AddLocal(JsValueRef value);
  bool AddLocalContext(JsContextRef value);
  bool AddLocalAddRef(JsRef value);

  static HandleScope *GetCurrent();

  template <class T>
  Local<T> Close(Handle<T> value);
};

class V8_EXPORT EscapableHandleScope : public HandleScope {
 public:
  EscapableHandleScope(Isolate* isolate) : HandleScope(isolate) {}

  template <class T>
  Local<T> Escape(Handle<T> value) { return Close(value); }
};

typedef HandleScope SealHandleScope;

class V8_EXPORT Data {
 public:
};

class ScriptOrigin {
 public:
  explicit ScriptOrigin(
    Local<Value> resource_name,
    Local<Integer> resource_line_offset = Local<Integer>(),
    Local<Integer> resource_column_offset = Local<Integer>()) :
      resource_name_(resource_name),
      resource_line_offset_(resource_line_offset),
      resource_column_offset_(resource_column_offset) {}
  Local<Value> ResourceName() const {
    return resource_name_;
  }
  Local<Integer> ResourceLineOffset() const {
    return resource_line_offset_;
  }
  Local<Integer> ResourceColumnOffset() const {
    return resource_column_offset_;
  }
 private:
  Local<Value> resource_name_;
  Local<Integer> resource_line_offset_;
  Local<Integer> resource_column_offset_;
};

class V8_EXPORT UnboundScript {
 public:
  Local<Script> BindToCurrentContext();
};

class V8_EXPORT Script {
 public:
  static V8_DEPRECATE_SOON(
      "Use maybe version",
      Local<Script> Compile(Handle<String> source,
                            ScriptOrigin* origin = nullptr));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
      Local<Context> context, Handle<String> source,
      ScriptOrigin* origin = nullptr);

  static Local<Script> V8_DEPRECATE_SOON("Use maybe version",
                                         Compile(Handle<String> source,
                                                 Handle<String> file_name));

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Run());
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Run(Local<Context> context);

  Local<UnboundScript> GetUnboundScript();
};

class V8_EXPORT ScriptCompiler {
 public:
  struct CachedData {
    // CHAKRA-TODO: Not implemented
   private:
    CachedData();  // Make sure it is not constructed as it is not implemented.
  };

  class Source {
   public:
    Source(
      Local<String> source_string,
      const ScriptOrigin& origin,
      CachedData * cached_data = NULL)
      : source_string(source_string), resource_name(origin.ResourceName()) {
    }

    Source(Local<String> source_string, CachedData * cached_data = NULL)
      : source_string(source_string) {
    }

   private:
    friend ScriptCompiler;
    Local<String> source_string;
    Handle<Value> resource_name;
  };

  enum CompileOptions {
    kNoCompileOptions = 0,
  };

  static V8_DEPRECATE_SOON("Use maybe version",
                           Local<UnboundScript> CompileUnbound(
                             Isolate* isolate, Source* source,
                             CompileOptions options = kNoCompileOptions));
  static V8_WARN_UNUSED_RESULT MaybeLocal<UnboundScript> CompileUnboundScript(
    Isolate* isolate, Source* source,
    CompileOptions options = kNoCompileOptions);

  static V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<Script> Compile(Isolate* isolate, Source* source,
                          CompileOptions options = kNoCompileOptions));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Script> Compile(
    Local<Context> context, Source* source,
    CompileOptions options = kNoCompileOptions);
};

class V8_EXPORT Message {
 public:
  V8_DEPRECATE_SOON("Use maybe version", Local<String> GetSourceLine()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> GetSourceLine(
      Local<Context> context) const;

  Handle<Value> GetScriptResourceName() const;

  V8_DEPRECATE_SOON("Use maybe version", int GetLineNumber()) const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetLineNumber(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", int GetStartColumn()) const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetStartColumn(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", int GetEndColumn()) const;
  V8_WARN_UNUSED_RESULT Maybe<int> GetEndColumn(Local<Context> context) const;

  static const int kNoLineNumberInfo = 0;
  static const int kNoColumnInfo = 0;
  static const int kNoScriptIdInfo = 0;
};

typedef void (*MessageCallback)(Handle<Message> message, Handle<Value> error);

class V8_EXPORT StackTrace {
 public:
  enum StackTraceOptions {
    kLineNumber = 1,
    kColumnOffset = 1 << 1 | kLineNumber,
    kScriptName = 1 << 2,
    kFunctionName = 1 << 3,
    kIsEval = 1 << 4,
    kIsConstructor = 1 << 5,
    kScriptNameOrSourceURL = 1 << 6,
    kScriptId = 1 << 7,
    kExposeFramesAcrossSecurityOrigins = 1 << 8,
    kOverview = kLineNumber | kColumnOffset | kScriptName | kFunctionName,
    kDetailed = kOverview | kIsEval | kIsConstructor | kScriptNameOrSourceURL
  };

  Local<StackFrame> GetFrame(uint32_t index) const;
  int GetFrameCount() const;
  Local<Array> AsArray();

  static Local<StackTrace> CurrentStackTrace(
    Isolate* isolate,
    int frame_limit,
    StackTraceOptions options = kOverview);
};

class V8_EXPORT StackFrame {
 public:
  int GetLineNumber() const;
  int GetColumn() const;
  int GetScriptId() const;
  Local<String> GetScriptName() const;
  Local<String> GetScriptNameOrSourceURL() const;
  Local<String> GetFunctionName() const;
  bool IsEval() const;
  bool IsConstructor() const;
};

class V8_EXPORT Value : public Data {
 public:
  bool IsUndefined() const;
  bool IsNull() const;
  bool IsTrue() const;
  bool IsFalse() const;
  bool IsString() const;
  bool IsFunction() const;
  bool IsArray() const;
  bool IsObject() const;
  bool IsBoolean() const;
  bool IsNumber() const;
  bool IsInt32() const;
  bool IsUint32() const;
  bool IsDate() const;
  bool IsBooleanObject() const;
  bool IsNumberObject() const;
  bool IsStringObject() const;
  bool IsNativeError() const;
  bool IsRegExp() const;
  bool IsExternal() const;
  bool IsArrayBuffer() const;
  bool IsTypedArray() const;
  bool IsUint8Array() const;
  bool IsUint8ClampedArray() const;
  bool IsInt8Array() const;
  bool IsUint16Array() const;
  bool IsInt16Array() const;
  bool IsUint32Array() const;
  bool IsInt32Array() const;
  bool IsFloat32Array() const;
  bool IsFloat64Array() const;
  bool IsDataView() const;
  bool IsMapIterator() const;
  bool IsSetIterator() const;
  bool IsMap() const;
  bool IsSet() const;
  bool IsPromise() const;

  V8_WARN_UNUSED_RESULT MaybeLocal<Boolean> ToBoolean(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Number> ToNumber(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToString(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ToDetailString(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> ToObject(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Integer> ToInteger(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToUint32(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Int32> ToInt32(Local<Context> context) const;

  Local<Boolean> ToBoolean(Isolate* isolate = nullptr) const;
  Local<Number> ToNumber(Isolate* isolate = nullptr) const;
  Local<String> ToString(Isolate* isolate = nullptr) const;
  Local<String> ToDetailString(Isolate* isolate = nullptr) const;
  Local<Object> ToObject(Isolate* isolate = nullptr) const;
  Local<Integer> ToInteger(Isolate* isolate = nullptr) const;
  Local<Uint32> ToUint32(Isolate* isolate = nullptr) const;
  Local<Int32> ToInt32(Isolate* isolate = nullptr) const;

  V8_DEPRECATE_SOON("Use maybe version", Local<Uint32> ToArrayIndex()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Uint32> ToArrayIndex(
    Local<Context> context) const;

  V8_WARN_UNUSED_RESULT Maybe<bool> BooleanValue(Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<double> NumberValue(Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<int64_t> IntegerValue(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<uint32_t> Uint32Value(
    Local<Context> context) const;
  V8_WARN_UNUSED_RESULT Maybe<int32_t> Int32Value(Local<Context> context) const;

  V8_DEPRECATE_SOON("Use maybe version", bool BooleanValue()) const;
  V8_DEPRECATE_SOON("Use maybe version", double NumberValue()) const;
  V8_DEPRECATE_SOON("Use maybe version", int64_t IntegerValue()) const;
  V8_DEPRECATE_SOON("Use maybe version", uint32_t Uint32Value()) const;
  V8_DEPRECATE_SOON("Use maybe version", int32_t Int32Value()) const;

  V8_DEPRECATE_SOON("Use maybe version", bool Equals(Handle<Value> that)) const;
  V8_WARN_UNUSED_RESULT Maybe<bool> Equals(Local<Context> context,
                                           Handle<Value> that) const;

  bool StrictEquals(Handle<Value> that) const;

  template <class T> static Value* Cast(T* value) {
    return static_cast<Value*>(value);
  }
};

class V8_EXPORT Primitive : public Value {
 public:
};

class V8_EXPORT Boolean : public Primitive {
 public:
  bool Value() const;
  static Handle<Boolean> New(Isolate* isolate, bool value);

 private:
  friend class BooleanObject;
  template <class F> friend class ReturnValue;
  static Local<Boolean> From(bool value);
};

class V8_EXPORT Name : public Primitive {
 public:
  int GetIdentityHash();
  static Name* Cast(v8::Value* obj);
 private:
  static void CheckCast(v8::Value* obj);
};

enum class NewStringType { kNormal, kInternalized };

class V8_EXPORT String : public Name {
 public:
  static const int kMaxLength = (1 << 28) - 16;

  int Length() const;
  int Utf8Length() const;
  bool IsOneByte() const { return false; }
  bool ContainsOnlyOneByte() const { return false; }

  enum WriteOptions {
    NO_OPTIONS = 0,
    HINT_MANY_WRITES_EXPECTED = 1,
    NO_NULL_TERMINATION = 2,
    PRESERVE_ONE_BYTE_NULL = 4,
    REPLACE_INVALID_UTF8 = 8
  };

  int Write(uint16_t* buffer,
            int start = 0,
            int length = -1,
            int options = NO_OPTIONS) const;
  int WriteOneByte(uint8_t* buffer,
                   int start = 0,
                   int length = -1,
                   int options = NO_OPTIONS) const;
  int WriteUtf8(char* buffer,
                int length = -1,
                int* nchars_ref = NULL,
                int options = NO_OPTIONS) const;

  static Local<String> Empty(Isolate* isolate);
  bool IsExternal() const { return false; }
  bool IsExternalOneByte() const { return false; }

  class V8_EXPORT ExternalOneByteStringResource {
   public:
    virtual ~ExternalOneByteStringResource() {}
    virtual const char *data() const = 0;
    virtual size_t length() const = 0;
  };

  class V8_EXPORT ExternalStringResource {
   public:
    virtual ~ExternalStringResource() {}
    virtual const uint16_t* data() const = 0;
    virtual size_t length() const = 0;
  };

  ExternalStringResource* GetExternalStringResource() const { return nullptr; }
  const ExternalOneByteStringResource*
    GetExternalOneByteStringResource() const {
    return nullptr;
  }

  static String *Cast(v8::Value *obj);

  enum NewStringType {
    kNormalString = static_cast<int>(v8::NewStringType::kNormal),
    kInternalizedString = static_cast<int>(v8::NewStringType::kInternalized)
  };

  static V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<String> NewFromUtf8(Isolate* isolate, const char* data,
                              NewStringType type = kNormalString,
                              int length = -1));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromUtf8(
    Isolate* isolate, const char* data, v8::NewStringType type,
    int length = -1);

  static V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<String> NewFromOneByte(Isolate* isolate, const uint8_t* data,
                                 NewStringType type = kNormalString,
                                 int length = -1));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromOneByte(
    Isolate* isolate, const uint8_t* data, v8::NewStringType type,
    int length = -1);

  static V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<String> NewFromTwoByte(Isolate* isolate, const uint16_t* data,
                                 NewStringType type = kNormalString,
                                 int length = -1));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewFromTwoByte(
    Isolate* isolate, const uint16_t* data, v8::NewStringType type,
    int length = -1);

  static Local<String> Concat(Handle<String> left, Handle<String> right);

  static V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<String> NewExternal(Isolate* isolate,
                              ExternalStringResource* resource));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewExternalTwoByte(
    Isolate* isolate, ExternalStringResource* resource);

  static V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<String> NewExternal(Isolate* isolate,
                              ExternalOneByteStringResource* resource));
  static V8_WARN_UNUSED_RESULT MaybeLocal<String> NewExternalOneByte(
    Isolate* isolate, ExternalOneByteStringResource* resource);

  class V8_EXPORT Utf8Value {
   public:
    explicit Utf8Value(Handle<v8::Value> obj);
    ~Utf8Value();
    char *operator*() { return _str; }
    const char *operator*() const { return _str; }
    int length() const { return static_cast<int>(_length); }
   private:
    Utf8Value(const Utf8Value&);
    void operator=(const Utf8Value&);

    char* _str;
    int _length;
  };

  class V8_EXPORT Value {
   public:
    explicit Value(Handle<v8::Value> obj);
    ~Value();
    uint16_t *operator*() { return _str; }
    const uint16_t *operator*() const { return _str; }
    int length() const { return _length; }
   private:
    Value(const Value&);
    void operator=(const Value&);

    uint16_t* _str;
    int _length;
  };

 private:
  template <class ToWide>
  static MaybeLocal<String> New(const ToWide& toWide,
                                const char *data, int length = -1);
  static MaybeLocal<String> New(const wchar_t *data, int length = -1);
};

class V8_EXPORT Number : public Primitive {
 public:
  double Value() const;
  static Local<Number> New(Isolate* isolate, double value);
  static Number *Cast(v8::Value *obj);

 private:
  friend class Integer;
  template <class F> friend class ReturnValue;
  static Local<Number> From(double value);
};

class V8_EXPORT Integer : public Number {
 public:
  static Local<Integer> New(Isolate* isolate, int32_t value);
  static Local<Integer> NewFromUnsigned(Isolate* isolate, uint32_t value);
  static Integer *Cast(v8::Value *obj);

  int64_t Value() const;

 private:
  friend class Utils;
  template <class F> friend class ReturnValue;
  static Local<Integer> From(int32_t value);
  static Local<Integer> From(uint32_t value);
};

class V8_EXPORT Int32 : public Integer {
 public:
  int32_t Value() const;
  static Int32* Cast(v8::Value* obj);
};

class V8_EXPORT Uint32 : public Integer {
 public:
  uint32_t Value() const;
  static Uint32* Cast(v8::Value* obj);
};

class V8_EXPORT Object : public Value {
 public:
  V8_DEPRECATE_SOON("Use maybe version",
                    bool Set(Handle<Value> key, Handle<Value> value));
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context,
                                        Local<Value> key, Local<Value> value);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool Set(uint32_t index, Handle<Value> value));
  V8_WARN_UNUSED_RESULT Maybe<bool> Set(Local<Context> context, uint32_t index,
                                        Local<Value> value);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool ForceSet(Handle<Value> key, Handle<Value> value,
                                  PropertyAttribute attribs = None));
  V8_WARN_UNUSED_RESULT Maybe<bool> ForceSet(Local<Context> context,
                                             Local<Value> key,
                                             Local<Value> value,
                                             PropertyAttribute attribs = None);

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Get(Handle<Value> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", Local<Value> Get(uint32_t index));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Get(Local<Context> context,
                                              uint32_t index);

  V8_DEPRECATE_SOON("Use maybe version",
                    PropertyAttribute GetPropertyAttributes(Handle<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetPropertyAttributes(
      Local<Context> context, Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> GetOwnPropertyDescriptor(Local<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetOwnPropertyDescriptor(
    Local<Context> context, Local<String> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Has(Handle<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context,
                                        Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Delete(Handle<Value> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           Local<Value> key);

  V8_DEPRECATE_SOON("Use maybe version", bool Has(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> Has(Local<Context> context, uint32_t index);

  V8_DEPRECATE_SOON("Use maybe version", bool Delete(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> Delete(Local<Context> context,
                                           uint32_t index);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool SetAccessor(Handle<String> name,
                                     AccessorGetterCallback getter,
                                     AccessorSetterCallback setter = 0,
                                     Handle<Value> data = Handle<Value>(),
                                     AccessControl settings = DEFAULT,
                                     PropertyAttribute attribute = None));
  V8_DEPRECATE_SOON("Use maybe version",
                    bool SetAccessor(Handle<Name> name,
                                     AccessorNameGetterCallback getter,
                                     AccessorNameSetterCallback setter = 0,
                                     Handle<Value> data = Handle<Value>(),
                                     AccessControl settings = DEFAULT,
                                     PropertyAttribute attribute = None));
  V8_WARN_UNUSED_RESULT
  Maybe<bool> SetAccessor(Local<Context> context,
                          Local<Name> name,
                          AccessorNameGetterCallback getter,
                          AccessorNameSetterCallback setter = 0,
                          MaybeLocal<Value> data = MaybeLocal<Value>(),
                          AccessControl settings = DEFAULT,
                          PropertyAttribute attribute = None);

  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetPropertyNames());
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetPropertyNames(
    Local<Context> context);

  V8_DEPRECATE_SOON("Use maybe version", Local<Array> GetOwnPropertyNames());
  V8_WARN_UNUSED_RESULT MaybeLocal<Array> GetOwnPropertyNames(
    Local<Context> context);

  Local<Value> GetPrototype();

  V8_DEPRECATE_SOON("Use maybe version",
                    bool SetPrototype(Handle<Value> prototype));
  V8_WARN_UNUSED_RESULT Maybe<bool> SetPrototype(Local<Context> context,
                                                 Local<Value> prototype);

  V8_DEPRECATE_SOON("Use maybe version", Local<String> ObjectProtoToString());
  V8_WARN_UNUSED_RESULT MaybeLocal<String> ObjectProtoToString(
    Local<Context> context);

  Local<String> GetConstructorName();
  int InternalFieldCount();
  Local<Value> GetInternalField(int index);
  void SetInternalField(int index, Handle<Value> value);
  void* GetAlignedPointerFromInternalField(int index);
  void SetAlignedPointerInInternalField(int index, void* value);

  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasOwnProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasOwnProperty(Local<Context> context,
                                                   Local<Name> key);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedProperty(Local<Context> context,
                                                         Local<Name> key);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealIndexedProperty(uint32_t index));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealIndexedProperty(
    Local<Context> context, uint32_t index);
  V8_DEPRECATE_SOON("Use maybe version",
                    bool HasRealNamedCallbackProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<bool> HasRealNamedCallbackProperty(
    Local<Context> context, Local<Name> key);

  V8_DEPRECATE_SOON(
    "Use maybe version",
    Local<Value> GetRealNamedPropertyInPrototypeChain(Handle<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedPropertyInPrototypeChain(
    Local<Context> context, Local<Name> key);

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> GetRealNamedProperty(Handle<String> key));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> GetRealNamedProperty(
    Local<Context> context, Local<Name> key);

  V8_DEPRECATE_SOON("Use maybe version",
                    Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
                      Handle<String> key));
  V8_WARN_UNUSED_RESULT Maybe<PropertyAttribute> GetRealNamedPropertyAttributes(
    Local<Context> context, Local<Name> key);

  bool SetHiddenValue(Handle<String> key, Handle<Value> value);
  Local<Value> GetHiddenValue(Handle<String> key);

  Local<Object> Clone();
  Local<Context> CreationContext();

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> CallAsFunction(Handle<Value> recv, int argc,
                                                Handle<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsFunction(Local<Context> context,
                                                         Handle<Value> recv,
                                                         int argc,
                                                         Handle<Value> argv[]);
  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> CallAsConstructor(int argc,
                                                   Handle<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> CallAsConstructor(
    Local<Context> context, int argc, Local<Value> argv[]);

  Isolate* GetIsolate();
  static Local<Object> New(Isolate* isolate = nullptr);
  static Object *Cast(Value *obj);

 private:
  friend struct ObjectData;
  friend class ObjectTemplate;
  friend class Utils;

  Maybe<bool> Set(Handle<Value> key, Handle<Value> value,
                  PropertyAttribute attribs, bool force);
  Maybe<bool> SetAccessor(Handle<Name> name,
                          AccessorNameGetterCallback getter,
                          AccessorNameSetterCallback setter,
                          Handle<Value> data,
                          AccessControl settings,
                          PropertyAttribute attribute,
                          Handle<AccessorSignature> signature);

  JsErrorCode GetObjectData(struct ObjectData** objectData);
  ObjectTemplate* GetObjectTemplate();
};

class V8_EXPORT Array : public Object {
 public:
  uint32_t Length() const;

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Object> CloneElementAt(uint32_t index));
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> CloneElementAt(
    Local<Context> context, uint32_t index);

  static Local<Array> New(Isolate* isolate = nullptr, int length = 0);
  static Array *Cast(Value *obj);
};

class V8_EXPORT BooleanObject : public Object {
 public:
  static Local<Value> New(bool value);
  bool ValueOf() const;
  static BooleanObject* Cast(Value* obj);
};


class V8_EXPORT StringObject : public Object {
 public:
  static Local<Value> New(Handle<String> value);
  Local<String> ValueOf() const;
  static StringObject* Cast(Value* obj);
};

class V8_EXPORT NumberObject : public Object {
 public:
  static Local<Value> New(Isolate * isolate, double value);
  double ValueOf() const;
  static NumberObject* Cast(Value* obj);
};

class V8_EXPORT Date : public Object {
 public:
  static V8_DEPRECATE_SOON("Use maybe version.",
                           Local<Value> New(Isolate* isolate, double time));
  static V8_WARN_UNUSED_RESULT MaybeLocal<Value> New(Local<Context> context,
                                                     double time);

  static Date *Cast(Value *obj);
};

class V8_EXPORT RegExp : public Object {
 public:
  enum Flags {
    kNone = 0,
    kGlobal = 1,
    kIgnoreCase = 2,
    kMultiline = 4
  };

  static V8_DEPRECATE_SOON("Use maybe version",
                           Local<RegExp> New(Handle<String> pattern,
                                             Flags flags));
  static V8_WARN_UNUSED_RESULT MaybeLocal<RegExp> New(Local<Context> context,
                                                      Handle<String> pattern,
                                                      Flags flags);
  Local<String> GetSource() const;
  static RegExp *Cast(v8::Value *obj);
};

template<typename T>
class ReturnValue {
 public:
  // Handle setters
  template <typename S> void Set(const Persistent<S>& handle) {
    *_value = static_cast<Value*>(*handle);
  }
  template <typename S> void Set(const Handle<S> handle) {
    *_value = static_cast<Value*>(*handle);
  }
  // Fast primitive setters
  void Set(bool value) { Set(Boolean::From(value)); }
  void Set(double value) { Set(Number::From(value)); }
  void Set(int32_t value) { Set(Integer::From(value)); }
  void Set(uint32_t value) { Set(Integer::From(value)); }
  // Fast JS primitive setters
  void SetNull() { Set(Null(nullptr)); }
  void SetUndefined() { Set(Undefined(nullptr)); }
  void SetEmptyString() { Set(String::Empty(nullptr)); }
  // Convenience getter for Isolate
  Isolate* GetIsolate() { return Isolate::GetCurrent(); }

  Value* Get() const { return *_value; }
 private:
  explicit ReturnValue(Value** value)
    : _value(value) {
  }

  Value** _value;
  template <typename F> friend class FunctionCallbackInfo;
  template <typename F> friend class PropertyCallbackInfo;
};

template<typename T>
class FunctionCallbackInfo {
 public:
  int Length() const { return _length; }
  Local<Value> operator[](int i) const {
    return (i >= 0 && i < _length) ?
      _args[i] : Undefined(nullptr).As<Value>();
  }
  Local<Function> Callee() const { return _callee; }
  Local<Object> This() const { return _thisPointer; }
  Local<Object> Holder() const { return _holder; }
  bool IsConstructCall() const { return _isConstructorCall; }
  Local<Value> Data() const { return _data; }
  Isolate* GetIsolate() const { return Isolate::GetCurrent(); }
  ReturnValue<T> GetReturnValue() const {
    return ReturnValue<T>(
      &(const_cast<FunctionCallbackInfo<T>*>(this)->_returnValue));
  }

  FunctionCallbackInfo(
    Value** args,
    int length,
    Local<Object> _this,
    Local<Object> holder,
    bool isConstructorCall,
    Local<Value> data,
    Local<Function> callee)
       : _args(args),
         _length(length),
         _thisPointer(_this),
         _holder(holder),
         _isConstructorCall(isConstructorCall),
         _data(data),
         _callee(callee),
         _returnValue(static_cast<Value*>(JS_INVALID_REFERENCE)) {
  }

 private:
  int _length;
  Local<Object> _thisPointer;
  Local<Object> _holder;
  Local<Function> _callee;
  Local<Value> _data;
  bool _isConstructorCall;
  Value** _args;
  Value* _returnValue;
};


template<typename T>
class PropertyCallbackInfo {
 public:
  Isolate* GetIsolate() const { return Isolate::GetCurrent(); }
  Local<Value> Data() const { return _data; }
  Local<Object> This() const { return _thisObject; }
  Local<Object> Holder() const { return _holder; }
  ReturnValue<T> GetReturnValue() const {
    return ReturnValue<T>(
      &(const_cast<PropertyCallbackInfo<T>*>(this)->_returnValue));
  }

  PropertyCallbackInfo(
    Local<Value> data, Local<Object> thisObject, Local<Object> holder)
       : _data(data),
         _thisObject(thisObject),
         _holder(holder),
         _returnValue(static_cast<Value*>(JS_INVALID_REFERENCE)) {
  }
 private:
  Local<Value> _data;
  Local<Object> _thisObject;
  Local<Object> _holder;
  Value* _returnValue;
};

typedef void (*FunctionCallback)(const FunctionCallbackInfo<Value>& info);

class V8_EXPORT Function : public Object {
 public:
  static Local<Function> New(Isolate* isolate,
                             FunctionCallback callback,
                             Local<Value> data = Local<Value>(),
                             int length = 0);

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Object> NewInstance(int argc,
                                              Handle<Value> argv[])) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
    Local<Context> context, int argc, Handle<Value> argv[]) const;

  V8_DEPRECATE_SOON("Use maybe version", Local<Object> NewInstance()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(
    Local<Context> context) const {
    return NewInstance(context, 0, nullptr);
  }

  V8_DEPRECATE_SOON("Use maybe version",
                    Local<Value> Call(Handle<Value> recv, int argc,
                                      Handle<Value> argv[]));
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> Call(Local<Context> context,
                                               Handle<Value> recv, int argc,
                                               Handle<Value> argv[]);

  void SetName(Handle<String> name);
  // Handle<Value> GetName() const;

  static Function *Cast(Value *obj);
};

class V8_EXPORT Promise : public Object {
 public:
  class V8_EXPORT Resolver : public Object {
   public:
    static Local<Resolver> New(Isolate* isolate);
    Local<Promise> GetPromise();
    void Resolve(Handle<Value> value);
    void Reject(Handle<Value> value);
    static Resolver* Cast(Value* obj);
   private:
    Resolver();
    static void CheckCast(Value* obj);
  };

  Local<Promise> Chain(Handle<Function> handler);
  Local<Promise> Catch(Handle<Function> handler);
  Local<Promise> Then(Handle<Function> handler);

  bool HasHandler();
  static Promise* Cast(Value* obj);
 private:
  Promise();
};


enum class ArrayBufferCreationMode { kInternalized, kExternalized };

class V8_EXPORT ArrayBuffer : public Object {
 public:
  class V8_EXPORT Allocator {  // NOLINT
   public:
    virtual ~Allocator() {}
    virtual void* Allocate(size_t length) = 0;
    virtual void* AllocateUninitialized(size_t length) = 0;
    virtual void Free(void* data, size_t length) = 0;
  };

  class V8_EXPORT Contents {  // NOLINT
   public:
    Contents() : data_(NULL), byte_length_(0) {}
    void* Data() const { return data_; }
    size_t ByteLength() const { return byte_length_; }

   private:
    void* data_;
    size_t byte_length_;
    friend class ArrayBuffer;
  };

  size_t ByteLength() const;
  static Local<ArrayBuffer> New(Isolate* isolate, size_t byte_length);
  static Local<ArrayBuffer> New(
    Isolate* isolate, void* data, size_t byte_length,
    ArrayBufferCreationMode mode = ArrayBufferCreationMode::kExternalized);

  bool IsExternal() const;
  bool IsNeuterable() const;
  void Neuter();
  Contents Externalize();
  Contents GetContents();

  static ArrayBuffer* Cast(Value* obj);
 private:
  ArrayBuffer();
};

class V8_EXPORT ArrayBufferView : public Object {
 public:
  Local<ArrayBuffer> Buffer();
  size_t ByteOffset();
  size_t ByteLength();
  size_t CopyContents(void* dest, size_t byte_length);
  bool HasBuffer() const;

  static ArrayBufferView* Cast(Value* obj);
 private:
  ArrayBufferView();
};

class V8_EXPORT TypedArray : public ArrayBufferView {
 public:
  size_t Length();
  static TypedArray* Cast(Value* obj);
 private:
  TypedArray();
};

class V8_EXPORT Uint8Array : public TypedArray {
 public:
  static Local<Uint8Array> New(Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Uint8Array* Cast(Value* obj);
 private:
  Uint8Array();
};

class V8_EXPORT Uint8ClampedArray : public TypedArray {
 public:
  static Local<Uint8ClampedArray> New(Handle<ArrayBuffer> array_buffer,
                                      size_t byte_offset, size_t length);
  static Uint8ClampedArray* Cast(Value* obj);
 private:
  Uint8ClampedArray();
};

class V8_EXPORT Int8Array : public TypedArray {
 public:
  static Local<Int8Array> New(Handle<ArrayBuffer> array_buffer,
                              size_t byte_offset, size_t length);
  static Int8Array* Cast(Value* obj);
 private:
  Int8Array();
};

class V8_EXPORT Uint16Array : public TypedArray {
 public:
  static Local<Uint16Array> New(Handle<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Uint16Array* Cast(Value* obj);
 private:
  Uint16Array();
};

class V8_EXPORT Int16Array : public TypedArray {
 public:
  static Local<Int16Array> New(Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Int16Array* Cast(Value* obj);
 private:
  Int16Array();
};

class V8_EXPORT Uint32Array : public TypedArray {
 public:
  static Local<Uint32Array> New(Handle<ArrayBuffer> array_buffer,
                                size_t byte_offset, size_t length);
  static Uint32Array* Cast(Value* obj);
 private:
  Uint32Array();
};

class V8_EXPORT Int32Array : public TypedArray {
 public:
  static Local<Int32Array> New(Handle<ArrayBuffer> array_buffer,
                               size_t byte_offset, size_t length);
  static Int32Array* Cast(Value* obj);
 private:
  Int32Array();
};

class V8_EXPORT Float32Array : public TypedArray {
 public:
  static Local<Float32Array> New(Handle<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Float32Array* Cast(Value* obj);
 private:
  Float32Array();
};

class V8_EXPORT Float64Array : public TypedArray {
 public:
  static Local<Float64Array> New(Handle<ArrayBuffer> array_buffer,
                                 size_t byte_offset, size_t length);
  static Float64Array* Cast(Value* obj);
 private:
  Float64Array();
};

enum AccessType {
  ACCESS_GET,
  ACCESS_SET,
  ACCESS_HAS,
  ACCESS_DELETE,
  ACCESS_KEYS
};

typedef bool (*NamedSecurityCallback)(
  Local<Object> host, Local<Value> key, AccessType type, Local<Value> data);
typedef bool (*IndexedSecurityCallback)(
  Local<Object> host, uint32_t index, AccessType type, Local<Value> data);

class V8_EXPORT Template : public Data {
 public:
  void Set(Handle<String> name,
           Handle<Data> value,
           PropertyAttribute attributes = None);
  void Set(Isolate* isolate, const char* name, Handle<Data> value) {
    Set(v8::String::NewFromUtf8(isolate, name), value);
  }
 private:
  Template();
};

class V8_EXPORT FunctionTemplate : public Template {
 public:
  static Local<FunctionTemplate> New(
    Isolate* isolate,
    FunctionCallback callback = 0,
    Handle<Value> data = Handle<Value>(),
    Handle<Signature> signature = Handle<Signature>(),
    int length = 0);

  V8_DEPRECATE_SOON("Use maybe version", Local<Function> GetFunction());
  V8_WARN_UNUSED_RESULT MaybeLocal<Function> GetFunction(
    Local<Context> context);

  Local<ObjectTemplate> InstanceTemplate();
  Local<ObjectTemplate> PrototypeTemplate();
  void SetClassName(Handle<String> name);
  void SetHiddenPrototype(bool value);
  void SetCallHandler(FunctionCallback callback,
                      Handle<Value> data = Handle<Value>());
  bool HasInstance(Handle<Value> object);
  void Inherit(Handle<FunctionTemplate> parent);
};

enum class PropertyHandlerFlags {
  kNone = 0,
  kAllCanRead = 1,
  kNonMasking = 1 << 1,
  kOnlyInterceptStrings = 1 << 2,
};

struct NamedPropertyHandlerConfiguration {
  NamedPropertyHandlerConfiguration(
    GenericNamedPropertyGetterCallback getter = 0,
    GenericNamedPropertySetterCallback setter = 0,
    GenericNamedPropertyQueryCallback query = 0,
    GenericNamedPropertyDeleterCallback deleter = 0,
    GenericNamedPropertyEnumeratorCallback enumerator = 0,
    Handle<Value> data = Handle<Value>(),
    PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
    : getter(getter),
      setter(setter),
      query(query),
      deleter(deleter),
      enumerator(enumerator),
      data(data),
      flags(flags) {}

  GenericNamedPropertyGetterCallback getter;
  GenericNamedPropertySetterCallback setter;
  GenericNamedPropertyQueryCallback query;
  GenericNamedPropertyDeleterCallback deleter;
  GenericNamedPropertyEnumeratorCallback enumerator;
  Handle<Value> data;
  PropertyHandlerFlags flags;
};

struct IndexedPropertyHandlerConfiguration {
  IndexedPropertyHandlerConfiguration(
    IndexedPropertyGetterCallback getter = 0,
    IndexedPropertySetterCallback setter = 0,
    IndexedPropertyQueryCallback query = 0,
    IndexedPropertyDeleterCallback deleter = 0,
    IndexedPropertyEnumeratorCallback enumerator = 0,
    Handle<Value> data = Handle<Value>(),
    PropertyHandlerFlags flags = PropertyHandlerFlags::kNone)
    : getter(getter),
      setter(setter),
      query(query),
      deleter(deleter),
      enumerator(enumerator),
      data(data),
      flags(flags) {}

  IndexedPropertyGetterCallback getter;
  IndexedPropertySetterCallback setter;
  IndexedPropertyQueryCallback query;
  IndexedPropertyDeleterCallback deleter;
  IndexedPropertyEnumeratorCallback enumerator;
  Handle<Value> data;
  PropertyHandlerFlags flags;
};

class V8_EXPORT ObjectTemplate : public Template {
 public:
  static Local<ObjectTemplate> New(Isolate* isolate);

  V8_DEPRECATE_SOON("Use maybe version", Local<Object> NewInstance());
  V8_WARN_UNUSED_RESULT MaybeLocal<Object> NewInstance(Local<Context> context);

  void SetClassName(Handle<String> name);

  void SetAccessor(Handle<String> name,
                   AccessorGetterCallback getter,
                   AccessorSetterCallback setter = 0,
                   Handle<Value> data = Handle<Value>(),
                   AccessControl settings = DEFAULT,
                   PropertyAttribute attribute = None,
                   Handle<AccessorSignature> signature =
                       Handle<AccessorSignature>());
  void SetAccessor(Handle<Name> name,
                   AccessorNameGetterCallback getter,
                   AccessorNameSetterCallback setter = 0,
                   Handle<Value> data = Handle<Value>(),
                   AccessControl settings = DEFAULT,
                   PropertyAttribute attribute = None,
                   Handle<AccessorSignature> signature =
                       Handle<AccessorSignature>());

  void SetNamedPropertyHandler(
    NamedPropertyGetterCallback getter,
    NamedPropertySetterCallback setter = 0,
    NamedPropertyQueryCallback query = 0,
    NamedPropertyDeleterCallback deleter = 0,
    NamedPropertyEnumeratorCallback enumerator = 0,
    Handle<Value> data = Handle<Value>());
  void SetHandler(const NamedPropertyHandlerConfiguration& configuration);

  void SetHandler(const IndexedPropertyHandlerConfiguration& configuration);
  void SetIndexedPropertyHandler(
    IndexedPropertyGetterCallback getter,
    IndexedPropertySetterCallback setter = 0,
    IndexedPropertyQueryCallback query = 0,
    IndexedPropertyDeleterCallback deleter = 0,
    IndexedPropertyEnumeratorCallback enumerator = 0,
    Handle<Value> data = Handle<Value>());

  void SetAccessCheckCallbacks(
    NamedSecurityCallback named_handler,
    IndexedSecurityCallback indexed_handler,
    Handle<Value> data = Handle<Value>(),
    bool turned_on_by_default = true);

  void SetInternalFieldCount(int value);
  void SetCallAsFunctionHandler(FunctionCallback callback,
                                Handle<Value> data = Handle<Value>());

 private:
  friend struct FunctionCallbackData;
  friend struct FunctionTemplateData;
  friend class Utils;

  Local<Object> NewInstance(Handle<Object> prototype);
  Handle<String> GetClassName();
};

class V8_EXPORT External : public Value {
 public:
  static Local<Value> Wrap(void* data);
  static inline void* Unwrap(Handle<Value> obj);
  static bool IsExternal(const Value* obj);

  static Local<External> New(Isolate* isolate, void* value);
  static External* Cast(Value* obj);
  void* Value() const;
};

class V8_EXPORT Signature : public Data {
 public:
  static Local<Signature> New(Isolate* isolate,
                              Handle<FunctionTemplate> receiver =
                                Handle<FunctionTemplate>(),
                              int argc = 0,
                              Handle<FunctionTemplate> argv[] = nullptr);
 private:
  Signature();
};

class V8_EXPORT AccessorSignature : public Data {
 public:
  static Local<AccessorSignature> New(
    Isolate* isolate,
    Handle<FunctionTemplate> receiver = Handle<FunctionTemplate>());
};


V8_EXPORT Handle<Primitive> Undefined(Isolate* isolate);
V8_EXPORT Handle<Primitive> Null(Isolate* isolate);
V8_EXPORT Handle<Boolean> True(Isolate* isolate);
V8_EXPORT Handle<Boolean> False(Isolate* isolate);
V8_EXPORT bool SetResourceConstraints(ResourceConstraints *constraints);


class V8_EXPORT ResourceConstraints {
 public:
  void set_stack_limit(uint32_t *value) {}
};

class V8_EXPORT Exception {
 public:
  static Local<Value> RangeError(Handle<String> message);
  static Local<Value> ReferenceError(Handle<String> message);
  static Local<Value> SyntaxError(Handle<String> message);
  static Local<Value> TypeError(Handle<String> message);
  static Local<Value> Error(Handle<String> message);
};

enum GCType {
  kGCTypeScavenge = 1 << 0,
  kGCTypeMarkSweepCompact = 1 << 1,
  kGCTypeAll = kGCTypeScavenge | kGCTypeMarkSweepCompact
};

enum GCCallbackFlags {
  kNoGCCallbackFlags = 0,
  kGCCallbackFlagCompacted = 1 << 0,
  kGCCallbackFlagConstructRetainedObjectInfos = 1 << 1,
  kGCCallbackFlagForced = 1 << 2
};

typedef void (*GCPrologueCallback)(GCType type, GCCallbackFlags flags);
typedef void (*GCEpilogueCallback)(GCType type, GCCallbackFlags flags);

// --- Promise Reject Callback ---
enum PromiseRejectEvent {
  kPromiseRejectWithNoHandler = 0,
  kPromiseHandlerAddedAfterReject = 1
};

class PromiseRejectMessage {
 public:
  PromiseRejectMessage(Handle<Promise> promise, PromiseRejectEvent event,
                       Handle<Value> value, Handle<StackTrace> stack_trace)
    : promise_(promise),
    event_(event),
    value_(value),
    stack_trace_(stack_trace) {
  }

  Handle<Promise> GetPromise() const { return promise_; }
  PromiseRejectEvent GetEvent() const { return event_; }
  Handle<Value> GetValue() const { return value_; }

  // DEPRECATED. Use v8::Exception::CreateMessage(GetValue())->GetStackTrace()
  Handle<StackTrace> GetStackTrace() const { return stack_trace_; }

 private:
  Handle<Promise> promise_;
  PromiseRejectEvent event_;
  Handle<Value> value_;
  Handle<StackTrace> stack_trace_;
};

typedef void (*PromiseRejectCallback)(PromiseRejectMessage message);

class V8_EXPORT HeapStatistics {
 private:
  size_t heapSize;

 public:
  void set_heap_size(size_t heapSize) {
    this->heapSize = heapSize;
  }

  size_t total_heap_size() { return this->heapSize; }
  size_t total_heap_size_executable() { return 0; }
  size_t total_physical_size() { return 0; }
  size_t total_available_size() { return 0; }
  size_t used_heap_size() { return this->heapSize; }
  size_t heap_size_limit() { return 0; }
};

typedef void(*FunctionEntryHook)(uintptr_t function,
                                 uintptr_t return_addr_location);
typedef int* (*CounterLookupCallback)(const char* name);
typedef void* (*CreateHistogramCallback)(
  const char* name, int min, int max, size_t buckets);
typedef void (*AddHistogramSampleCallback)(void* histogram, int sample);

class V8_EXPORT Isolate {
 public:
  struct CreateParams {
    CreateParams()
       : entry_hook(NULL),
         code_event_handler(NULL),
         snapshot_blob(NULL),
         counter_lookup_callback(NULL),
         create_histogram_callback(NULL),
         add_histogram_sample_callback(NULL),
         array_buffer_allocator(NULL) {
    }

    FunctionEntryHook entry_hook;
    JitCodeEventHandler code_event_handler;
    ResourceConstraints constraints;
    StartupData* snapshot_blob;
    CounterLookupCallback counter_lookup_callback;
    CreateHistogramCallback create_histogram_callback;
    AddHistogramSampleCallback add_histogram_sample_callback;
    ArrayBuffer::Allocator* array_buffer_allocator;
  };

  class V8_EXPORT Scope {
   public:
    explicit Scope(Isolate* isolate) : isolate_(isolate) { isolate->Enter(); }
    ~Scope() { isolate_->Exit(); }
   private:
    Isolate* const isolate_;
    Scope(const Scope&);
    Scope& operator=(const Scope&);
  };

  static Isolate* New(const CreateParams& params);
  static Isolate* New();
  static Isolate* GetCurrent();
  typedef bool(*AbortOnUncaughtExceptionCallback)(Isolate*);
  void SetAbortOnUncaughtExceptionCallback(
    AbortOnUncaughtExceptionCallback callback);

  void Enter();
  void Exit();
  void Dispose();

  void GetHeapStatistics(HeapStatistics *heap_statistics);
  int64_t AdjustAmountOfExternalAllocatedMemory(int64_t change_in_bytes);
  void SetData(uint32_t slot, void* data);
  void* GetData(uint32_t slot);
  static uint32_t GetNumberOfDataSlots();
  Local<Context> GetCurrentContext();
  void SetPromiseRejectCallback(PromiseRejectCallback callback);
  void RunMicrotasks();
  void SetAutorunMicrotasks(bool autorun);
  void SetFatalErrorHandler(FatalErrorCallback that);
  void SetJitCodeEventHandler(
    JitCodeEventOptions options, JitCodeEventHandler event_handler);
  bool AddMessageListener(
    MessageCallback that, Handle<Value> data = Handle<Value>());
  void RemoveMessageListeners(MessageCallback that);
  Local<Value> ThrowException(Local<Value> exception);
  HeapProfiler* GetHeapProfiler();
  CpuProfiler* GetCpuProfiler();

  typedef void (*GCPrologueCallback)(
    Isolate* isolate, GCType type, GCCallbackFlags flags);
  typedef void (*GCEpilogueCallback)(
    Isolate* isolate, GCType type, GCCallbackFlags flags);
  typedef void(*GCCallback)(Isolate* isolate, GCType type,
                            GCCallbackFlags flags);
  void AddGCPrologueCallback(
    GCCallback callback, GCType gc_type_filter = kGCTypeAll);
  void RemoveGCPrologueCallback(GCCallback callback);
  void AddGCEpilogueCallback(
    GCCallback callback, GCType gc_type_filter = kGCTypeAll);
  void RemoveGCEpilogueCallback(GCCallback callback);

  void SetCounterFunction(CounterLookupCallback);
  void SetCreateHistogramFunction(CreateHistogramCallback);
  void SetAddHistogramSampleFunction(AddHistogramSampleCallback);

  bool IdleNotificationDeadline(double deadline_in_seconds);
  V8_DEPRECATE_SOON("use IdleNotificationDeadline()",
                    bool IdleNotification(int idle_time_in_ms));

  void LowMemoryNotification();
  int ContextDisposedNotification();
};

class V8_EXPORT JitCodeEvent {
 public:
  enum EventType {
    CODE_ADDED,
    CODE_MOVED,
    CODE_REMOVED,
  };

  EventType type;
  void * code_start;
  size_t code_len;
  union {
    struct {
      const char* str;
      size_t len;
    } name;
    void* new_code_start;
  };
};

class V8_EXPORT StartupData {
};

class V8_EXPORT V8 {
 public:
  static void SetArrayBufferAllocator(ArrayBuffer::Allocator* allocator);
  static bool IsDead();
  static void SetFlagsFromString(const char* str, int length);
  static void SetFlagsFromCommandLine(
    int *argc, char **argv, bool remove_flags);
  static const char *GetVersion();
  static bool Initialize();
  static void SetEntropySource(EntropySource source);
  static void TerminateExecution(Isolate* isolate);
  static bool IsExeuctionDisabled(Isolate* isolate = nullptr);
  static void CancelTerminateExecution(Isolate* isolate);
  static bool Dispose();
  static void InitializePlatform(Platform* platform) {}
  static void FromJustIsNothing();
  static void ToLocalEmpty();
};

template <class T>
class Maybe {
 public:
  bool IsNothing() const { return !has_value; }
  bool IsJust() const { return has_value; }

  // Will crash if the Maybe<> is nothing.
  T FromJust() const {
    if (!IsJust()) {
      V8::FromJustIsNothing();
    }
    return value;
  }

  T FromMaybe(const T& default_value) const {
    return has_value ? value : default_value;
  }

  bool operator==(const Maybe& other) const {
    return (IsJust() == other.IsJust()) &&
      (!IsJust() || FromJust() == other.FromJust());
  }

  bool operator!=(const Maybe& other) const {
    return !operator==(other);
  }

 private:
  Maybe() : has_value(false) {}
  explicit Maybe(const T& t) : has_value(true), value(t) {}

  bool has_value;
  T value;

  template <class U>
  friend Maybe<U> Nothing();
  template <class U>
  friend Maybe<U> Just(const U& u);
};

template <class T>
inline Maybe<T> Nothing() {
  return Maybe<T>();
}


template <class T>
inline Maybe<T> Just(const T& t) {
  return Maybe<T>(t);
}

class V8_EXPORT TryCatch {
 public:
  TryCatch(Isolate* isolate = nullptr);
  ~TryCatch();

  bool HasCaught() const;
  bool CanContinue() const;
  bool HasTerminated() const;
  Handle<Value> ReThrow();
  Local<Value> Exception() const;

  V8_DEPRECATE_SOON("Use maybe version.", Local<Value> StackTrace()) const;
  V8_WARN_UNUSED_RESULT MaybeLocal<Value> StackTrace(
    Local<Context> context) const;

  Local<v8::Message> Message() const;
  void Reset();
  void SetVerbose(bool value);
  void SetCaptureMessage(bool value);

 private:
  friend class Function;

  void GetAndClearException();
  void CheckReportExternalException();

  JsValueRef error;
  TryCatch* prev;
  bool rethrow;
  bool verbose;
};

class V8_EXPORT ExtensionConfiguration {
};

class V8_EXPORT Context {
 public:
  class V8_EXPORT Scope {
   private:
    Scope * previous;
    void * context;
   public:
    Scope(Handle<Context> context);
    ~Scope();
  };

  Local<Object> Global();

  static Local<Context> New(
    Isolate* isolate,
    ExtensionConfiguration* extensions = NULL,
    Handle<ObjectTemplate> global_template = Handle<ObjectTemplate>(),
    Handle<Value> global_object = Handle<Value>());
  static Local<Context> GetCurrent();

  Isolate* GetIsolate();
  void* GetAlignedPointerFromEmbedderData(int index);
  void SetAlignedPointerInEmbedderData(int index, void* value);
  void SetSecurityToken(Handle<Value> token);
  Handle<Value> GetSecurityToken();
};

class V8_EXPORT Locker {
  // Don't need to implement this for Chakra
 public:
  explicit Locker(Isolate* isolate) {}
};


//
// Local<T> members
//

template <class T>
Local<T> Local<T>::New(T* that) {
  if (!HandleScope::GetCurrent()->AddLocal(that)) {
    return Local<T>();
  }
  return Local<T>(that);
}

// Context are not javascript values, so we need to specialize them
template <>
Local<Context> Local<Context>::New(Context* that) {
  if (!HandleScope::GetCurrent()->AddLocalContext(that)) {
    return Local<Context>();
  }
  return Local<Context>(that);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, Local<T> that) {
  return New(isolate, *that);
}

template <class T>
Local<T> Local<T>::New(Isolate* isolate, const PersistentBase<T>& that) {
  return New(isolate, that.val_);
}

//
// Persistent<T> members
//

template <class T>
T* PersistentBase<T>::New(Isolate* isolate, T* that) {
  if (that) {
    JsAddRef(static_cast<JsRef>(that), nullptr);
  }
  return that;
}

template <class T, class M>
template <class S, class M2>
void Persistent<T, M>::Copy(const Persistent<S, M2>& that) {
  TYPE_CHECK(T, S);
  this->Reset();
  if (that.IsEmpty()) return;

  this->val_ = that.val_;
  this->_weakWrapper = that._weakWrapper;
  if (val_ && !IsWeak()) {
    JsAddRef(val_, nullptr);
  }

  M::Copy(that, this);
}

template <class T>
bool PersistentBase<T>::IsNearDeath() const {
  return true;
}

template <class T>
bool PersistentBase<T>::IsWeak() const {
  return static_cast<bool>(_weakWrapper);
}

template <class T>
void PersistentBase<T>::Reset() {
  if (this->IsEmpty()) return;

  if (IsWeak()) {
    if (_weakWrapper.unique()) {
      chakrashim::ClearObjectWeakReferenceCallback(val_, /*revive*/false);
    }
    _weakWrapper.reset();
  } else {
    JsRelease(val_, nullptr);
  }

  val_ = nullptr;
}

template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate, const Handle<S>& other) {
  TYPE_CHECK(T, S);
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = New(isolate, other.val_);
}

template <class T>
template <class S>
void PersistentBase<T>::Reset(Isolate* isolate,
                              const PersistentBase<S>& other) {
  TYPE_CHECK(T, S);
  Reset();
  if (other.IsEmpty()) return;
  this->val_ = New(isolate, other.val_);
}

template <class T>
template <typename P, typename Callback>
void PersistentBase<T>::SetWeakCommon(P* parameter, Callback callback) {
  if (this->IsEmpty()) return;

  bool wasStrong = !IsWeak();
  chakrashim::SetObjectWeakReferenceCallback(val_, callback, parameter,
                                             &_weakWrapper);
  if (wasStrong) {
    JsRelease(val_, nullptr);
  }
}

template <class T>
template <typename P>
void PersistentBase<T>::SetWeak(
    P* parameter,
    typename WeakCallbackData<T, P>::Callback callback) {
  typedef typename WeakCallbackData<Value, void>::Callback Callback;
  SetWeakCommon(parameter, reinterpret_cast<Callback>(callback));
}

template <class T>
template <typename P>
void PersistentBase<T>::SetWeak(P* parameter,
                                typename WeakCallbackInfo<P>::Callback callback,
                                WeakCallbackType type) {
  typedef typename WeakCallbackInfo<void>::Callback Callback;
  SetWeakCommon(parameter, reinterpret_cast<Callback>(callback));
}

template <class T>
template <typename P>
P* PersistentBase<T>::ClearWeak() {
  if (!IsWeak()) return nullptr;

  P* parameters = reinterpret_cast<P*>(_weakWrapper->parameters);
  if (_weakWrapper.unique()) {
    chakrashim::ClearObjectWeakReferenceCallback(val_, /*revive*/true);
  }
  _weakWrapper.reset();

  JsAddRef(val_, nullptr);
  return parameters;
}

template <class T>
void PersistentBase<T>::MarkIndependent() {
}

template <class T>
void PersistentBase<T>::SetWrapperClassId(uint16_t class_id) {
}


//
// HandleScope template members
//

template <class T>
Local<T> HandleScope::Close(Handle<T> value) {
  if (_prev == nullptr || !_prev->AddLocal(*value)) {
    return Local<T>();
  }

  return Local<T>(*value);
}

// Context are not javascript values, so we need to specialize them
template <>
inline Local<Context> HandleScope::Close(Handle<Context> value) {
  if (_prev == nullptr || !_prev->AddLocalContext(*value)) {
    return Local<Context>();
  }

  return Local<Context>(*value);
}

}  // namespace v8
