// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_H_
#define V8_API_H_

#include "include/v8-testing.h"
#include "src/contexts.h"
#include "src/debug/debug-interface.h"
#include "src/detachable-vector.h"
#include "src/factory.h"
#include "src/isolate.h"
#include "src/objects/js-collection.h"

namespace v8 {

// Constants used in the implementation of the API.  The most natural thing
// would usually be to place these with the classes that use them, but
// we want to keep them out of v8.h because it is an externally
// visible file.
class Consts {
 public:
  enum TemplateType {
    FUNCTION_TEMPLATE = 0,
    OBJECT_TEMPLATE = 1
  };
};

template <typename T> inline T ToCData(v8::internal::Object* obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == v8::internal::Smi::kZero) return nullptr;
  return reinterpret_cast<T>(
      reinterpret_cast<intptr_t>(
          v8::internal::Foreign::cast(obj)->foreign_address()));
}


template <typename T>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, T obj) {
  STATIC_ASSERT(sizeof(T) == sizeof(v8::internal::Address));
  if (obj == nullptr) return handle(v8::internal::Smi::kZero, isolate);
  return isolate->factory()->NewForeign(
      reinterpret_cast<v8::internal::Address>(reinterpret_cast<intptr_t>(obj)));
}


class ApiFunction {
 public:
  explicit ApiFunction(v8::internal::Address addr) : addr_(addr) { }
  v8::internal::Address address() { return addr_; }
 private:
  v8::internal::Address addr_;
};



class RegisteredExtension {
 public:
  explicit RegisteredExtension(Extension* extension);
  static void Register(RegisteredExtension* that);
  static void UnregisterAll();
  Extension* extension() { return extension_; }
  RegisteredExtension* next() { return next_; }
  static RegisteredExtension* first_extension() { return first_extension_; }
 private:
  Extension* extension_;
  RegisteredExtension* next_;
  static RegisteredExtension* first_extension_;
};

#define OPEN_HANDLE_LIST(V)                    \
  V(Template, TemplateInfo)                    \
  V(FunctionTemplate, FunctionTemplateInfo)    \
  V(ObjectTemplate, ObjectTemplateInfo)        \
  V(Signature, FunctionTemplateInfo)           \
  V(AccessorSignature, FunctionTemplateInfo)   \
  V(Data, Object)                              \
  V(RegExp, JSRegExp)                          \
  V(Object, JSReceiver)                        \
  V(Array, JSArray)                            \
  V(Map, JSMap)                                \
  V(Set, JSSet)                                \
  V(ArrayBuffer, JSArrayBuffer)                \
  V(ArrayBufferView, JSArrayBufferView)        \
  V(TypedArray, JSTypedArray)                  \
  V(Uint8Array, JSTypedArray)                  \
  V(Uint8ClampedArray, JSTypedArray)           \
  V(Int8Array, JSTypedArray)                   \
  V(Uint16Array, JSTypedArray)                 \
  V(Int16Array, JSTypedArray)                  \
  V(Uint32Array, JSTypedArray)                 \
  V(Int32Array, JSTypedArray)                  \
  V(Float32Array, JSTypedArray)                \
  V(Float64Array, JSTypedArray)                \
  V(DataView, JSDataView)                      \
  V(SharedArrayBuffer, JSArrayBuffer)          \
  V(Name, Name)                                \
  V(String, String)                            \
  V(Symbol, Symbol)                            \
  V(Script, JSFunction)                        \
  V(UnboundScript, SharedFunctionInfo)         \
  V(Module, Module)                            \
  V(Function, JSReceiver)                      \
  V(Message, JSMessageObject)                  \
  V(Context, Context)                          \
  V(External, Object)                          \
  V(StackTrace, FixedArray)                    \
  V(StackFrame, StackFrameInfo)                \
  V(Proxy, JSProxy)                            \
  V(debug::GeneratorObject, JSGeneratorObject) \
  V(debug::Script, Script)                     \
  V(Promise, JSPromise)                        \
  V(Primitive, Object)                         \
  V(PrimitiveArray, FixedArray)                \
  V(ScriptOrModule, Script)

class Utils {
 public:
  static inline bool ApiCheck(bool condition,
                              const char* location,
                              const char* message) {
    if (!condition) Utils::ReportApiFailure(location, message);
    return condition;
  }
  static void ReportOOMFailure(const char* location, bool is_heap_oom);

  static inline Local<Context> ToLocal(
      v8::internal::Handle<v8::internal::Context> obj);
  static inline Local<Value> ToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Module> ToLocal(
      v8::internal::Handle<v8::internal::Module> obj);
  static inline Local<Name> ToLocal(
      v8::internal::Handle<v8::internal::Name> obj);
  static inline Local<String> ToLocal(
      v8::internal::Handle<v8::internal::String> obj);
  static inline Local<Symbol> ToLocal(
      v8::internal::Handle<v8::internal::Symbol> obj);
  static inline Local<RegExp> ToLocal(
      v8::internal::Handle<v8::internal::JSRegExp> obj);
  static inline Local<Object> ToLocal(
      v8::internal::Handle<v8::internal::JSReceiver> obj);
  static inline Local<Object> ToLocal(
      v8::internal::Handle<v8::internal::JSObject> obj);
  static inline Local<Function> ToLocal(
      v8::internal::Handle<v8::internal::JSFunction> obj);
  static inline Local<Array> ToLocal(
      v8::internal::Handle<v8::internal::JSArray> obj);
  static inline Local<Map> ToLocal(
      v8::internal::Handle<v8::internal::JSMap> obj);
  static inline Local<Set> ToLocal(
      v8::internal::Handle<v8::internal::JSSet> obj);
  static inline Local<Proxy> ToLocal(
      v8::internal::Handle<v8::internal::JSProxy> obj);
  static inline Local<ArrayBuffer> ToLocal(
      v8::internal::Handle<v8::internal::JSArrayBuffer> obj);
  static inline Local<ArrayBufferView> ToLocal(
      v8::internal::Handle<v8::internal::JSArrayBufferView> obj);
  static inline Local<DataView> ToLocal(
      v8::internal::Handle<v8::internal::JSDataView> obj);
  static inline Local<TypedArray> ToLocal(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Uint8Array> ToLocalUint8Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Uint8ClampedArray> ToLocalUint8ClampedArray(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Int8Array> ToLocalInt8Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Uint16Array> ToLocalUint16Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Int16Array> ToLocalInt16Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Uint32Array> ToLocalUint32Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Int32Array> ToLocalInt32Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Float32Array> ToLocalFloat32Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<Float64Array> ToLocalFloat64Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);

  static inline Local<SharedArrayBuffer> ToLocalShared(
      v8::internal::Handle<v8::internal::JSArrayBuffer> obj);

  static inline Local<Message> MessageToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Promise> PromiseToLocal(
      v8::internal::Handle<v8::internal::JSObject> obj);
  static inline Local<StackTrace> StackTraceToLocal(
      v8::internal::Handle<v8::internal::FixedArray> obj);
  static inline Local<StackFrame> StackFrameToLocal(
      v8::internal::Handle<v8::internal::StackFrameInfo> obj);
  static inline Local<Number> NumberToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Integer> IntegerToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<Uint32> Uint32ToLocal(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<FunctionTemplate> ToLocal(
      v8::internal::Handle<v8::internal::FunctionTemplateInfo> obj);
  static inline Local<ObjectTemplate> ToLocal(
      v8::internal::Handle<v8::internal::ObjectTemplateInfo> obj);
  static inline Local<Signature> SignatureToLocal(
      v8::internal::Handle<v8::internal::FunctionTemplateInfo> obj);
  static inline Local<AccessorSignature> AccessorSignatureToLocal(
      v8::internal::Handle<v8::internal::FunctionTemplateInfo> obj);
  static inline Local<External> ExternalToLocal(
      v8::internal::Handle<v8::internal::JSObject> obj);
  static inline Local<Function> CallableToLocal(
      v8::internal::Handle<v8::internal::JSReceiver> obj);
  static inline Local<Primitive> ToLocalPrimitive(
      v8::internal::Handle<v8::internal::Object> obj);
  static inline Local<PrimitiveArray> ToLocal(
      v8::internal::Handle<v8::internal::FixedArray> obj);
  static inline Local<ScriptOrModule> ScriptOrModuleToLocal(
      v8::internal::Handle<v8::internal::Script> obj);

#define DECLARE_OPEN_HANDLE(From, To) \
  static inline v8::internal::Handle<v8::internal::To> \
      OpenHandle(const From* that, bool allow_empty_handle = false);

OPEN_HANDLE_LIST(DECLARE_OPEN_HANDLE)

#undef DECLARE_OPEN_HANDLE

  template<class From, class To>
  static inline Local<To> Convert(v8::internal::Handle<From> obj) {
    DCHECK(obj.is_null() ||
           (obj->IsSmi() ||
            !obj->IsTheHole(i::HeapObject::cast(*obj)->GetIsolate())));
    return Local<To>(reinterpret_cast<To*>(obj.location()));
  }

  template <class T>
  static inline v8::internal::Handle<v8::internal::Object> OpenPersistent(
      const v8::Persistent<T>& persistent) {
    return v8::internal::Handle<v8::internal::Object>(
        reinterpret_cast<v8::internal::Object**>(persistent.val_));
  }

  template <class T>
  static inline v8::internal::Handle<v8::internal::Object> OpenPersistent(
      v8::Persistent<T>* persistent) {
    return OpenPersistent(*persistent);
  }

  template <class From, class To>
  static inline v8::internal::Handle<To> OpenHandle(v8::Local<From> handle) {
    return OpenHandle(*handle);
  }

 private:
  static void ReportApiFailure(const char* location, const char* message);
};


template <class T>
inline T* ToApi(v8::internal::Handle<v8::internal::Object> obj) {
  return reinterpret_cast<T*>(obj.location());
}

template <class T>
inline v8::Local<T> ToApiHandle(
    v8::internal::Handle<v8::internal::Object> obj) {
  return Utils::Convert<v8::internal::Object, T>(obj);
}


template <class T>
inline bool ToLocal(v8::internal::MaybeHandle<v8::internal::Object> maybe,
                    Local<T>* local) {
  v8::internal::Handle<v8::internal::Object> handle;
  if (maybe.ToHandle(&handle)) {
    *local = Utils::Convert<v8::internal::Object, T>(handle);
    return true;
  }
  return false;
}


// Implementations of ToLocal

#define MAKE_TO_LOCAL(Name, From, To)                                       \
  Local<v8::To> Utils::Name(v8::internal::Handle<v8::internal::From> obj) { \
    return Convert<v8::internal::From, v8::To>(obj);  \
  }


#define MAKE_TO_LOCAL_TYPED_ARRAY(Type, typeName, TYPE, ctype, size)  \
  Local<v8::Type##Array> Utils::ToLocal##Type##Array(                 \
      v8::internal::Handle<v8::internal::JSTypedArray> obj) {         \
    DCHECK(obj->type() == v8::internal::kExternal##Type##Array);      \
    return Convert<v8::internal::JSTypedArray, v8::Type##Array>(obj); \
  }


MAKE_TO_LOCAL(ToLocal, Context, Context)
MAKE_TO_LOCAL(ToLocal, Object, Value)
MAKE_TO_LOCAL(ToLocal, Module, Module)
MAKE_TO_LOCAL(ToLocal, Name, Name)
MAKE_TO_LOCAL(ToLocal, String, String)
MAKE_TO_LOCAL(ToLocal, Symbol, Symbol)
MAKE_TO_LOCAL(ToLocal, JSRegExp, RegExp)
MAKE_TO_LOCAL(ToLocal, JSReceiver, Object)
MAKE_TO_LOCAL(ToLocal, JSObject, Object)
MAKE_TO_LOCAL(ToLocal, JSFunction, Function)
MAKE_TO_LOCAL(ToLocal, JSArray, Array)
MAKE_TO_LOCAL(ToLocal, JSMap, Map)
MAKE_TO_LOCAL(ToLocal, JSSet, Set)
MAKE_TO_LOCAL(ToLocal, JSProxy, Proxy)
MAKE_TO_LOCAL(ToLocal, JSArrayBuffer, ArrayBuffer)
MAKE_TO_LOCAL(ToLocal, JSArrayBufferView, ArrayBufferView)
MAKE_TO_LOCAL(ToLocal, JSDataView, DataView)
MAKE_TO_LOCAL(ToLocal, JSTypedArray, TypedArray)
MAKE_TO_LOCAL(ToLocalShared, JSArrayBuffer, SharedArrayBuffer)

TYPED_ARRAYS(MAKE_TO_LOCAL_TYPED_ARRAY)

MAKE_TO_LOCAL(ToLocal, FunctionTemplateInfo, FunctionTemplate)
MAKE_TO_LOCAL(ToLocal, ObjectTemplateInfo, ObjectTemplate)
MAKE_TO_LOCAL(SignatureToLocal, FunctionTemplateInfo, Signature)
MAKE_TO_LOCAL(AccessorSignatureToLocal, FunctionTemplateInfo, AccessorSignature)
MAKE_TO_LOCAL(MessageToLocal, Object, Message)
MAKE_TO_LOCAL(PromiseToLocal, JSObject, Promise)
MAKE_TO_LOCAL(StackTraceToLocal, FixedArray, StackTrace)
MAKE_TO_LOCAL(StackFrameToLocal, StackFrameInfo, StackFrame)
MAKE_TO_LOCAL(NumberToLocal, Object, Number)
MAKE_TO_LOCAL(IntegerToLocal, Object, Integer)
MAKE_TO_LOCAL(Uint32ToLocal, Object, Uint32)
MAKE_TO_LOCAL(ExternalToLocal, JSObject, External)
MAKE_TO_LOCAL(CallableToLocal, JSReceiver, Function)
MAKE_TO_LOCAL(ToLocalPrimitive, Object, Primitive)
MAKE_TO_LOCAL(ToLocal, FixedArray, PrimitiveArray)
MAKE_TO_LOCAL(ScriptOrModuleToLocal, Script, ScriptOrModule)

#undef MAKE_TO_LOCAL_TYPED_ARRAY
#undef MAKE_TO_LOCAL


// Implementations of OpenHandle

#define MAKE_OPEN_HANDLE(From, To)                                             \
  v8::internal::Handle<v8::internal::To> Utils::OpenHandle(                    \
      const v8::From* that, bool allow_empty_handle) {                         \
    DCHECK(allow_empty_handle || that != nullptr);                             \
    DCHECK(that == nullptr ||                                                  \
           (*reinterpret_cast<v8::internal::Object* const*>(that))->Is##To()); \
    return v8::internal::Handle<v8::internal::To>(                             \
        reinterpret_cast<v8::internal::To**>(const_cast<v8::From*>(that)));    \
  }

OPEN_HANDLE_LIST(MAKE_OPEN_HANDLE)

#undef MAKE_OPEN_HANDLE
#undef OPEN_HANDLE_LIST

extern Isolate* IsolateNewImpl(internal::Isolate* isolate,
                               const Isolate::CreateParams& params);

namespace internal {

class V8_EXPORT_PRIVATE DeferredHandles {
 public:
  ~DeferredHandles();

 private:
  DeferredHandles(Object** first_block_limit, Isolate* isolate)
      : next_(nullptr),
        previous_(nullptr),
        first_block_limit_(first_block_limit),
        isolate_(isolate) {
    isolate->LinkDeferredHandles(this);
  }

  void Iterate(RootVisitor* v);

  std::vector<Object**> blocks_;
  DeferredHandles* next_;
  DeferredHandles* previous_;
  Object** first_block_limit_;
  Isolate* isolate_;

  friend class HandleScopeImplementer;
  friend class Isolate;
};


// This class is here in order to be able to declare it a friend of
// HandleScope.  Moving these methods to be members of HandleScope would be
// neat in some ways, but it would expose internal implementation details in
// our public header file, which is undesirable.
//
// An isolate has a single instance of this class to hold the current thread's
// data. In multithreaded V8 programs this data is copied in and out of storage
// so that the currently executing thread always has its own copy of this
// data.
class HandleScopeImplementer {
 public:
  explicit HandleScopeImplementer(Isolate* isolate)
      : isolate_(isolate),
        microtask_context_(nullptr),
        spare_(nullptr),
        call_depth_(0),
        microtasks_depth_(0),
        microtasks_suppressions_(0),
        entered_contexts_count_(0),
        entered_context_count_during_microtasks_(0),
#ifdef DEBUG
        debug_microtasks_depth_(0),
#endif
        microtasks_policy_(v8::MicrotasksPolicy::kAuto),
        last_handle_before_deferred_block_(nullptr) {
  }

  ~HandleScopeImplementer() {
    DeleteArray(spare_);
  }

  // Threading support for handle data.
  static int ArchiveSpacePerThread();
  char* RestoreThread(char* from);
  char* ArchiveThread(char* to);
  void FreeThreadResources();

  // Garbage collection support.
  void Iterate(v8::internal::RootVisitor* v);
  static char* Iterate(v8::internal::RootVisitor* v, char* data);

  inline internal::Object** GetSpareOrNewBlock();
  inline void DeleteExtensions(internal::Object** prev_limit);

  // Call depth represents nested v8 api calls.
  inline void IncrementCallDepth() {call_depth_++;}
  inline void DecrementCallDepth() {call_depth_--;}
  inline bool CallDepthIsZero() { return call_depth_ == 0; }

  // Microtasks scope depth represents nested scopes controlling microtasks
  // invocation, which happens when depth reaches zero.
  inline void IncrementMicrotasksScopeDepth() {microtasks_depth_++;}
  inline void DecrementMicrotasksScopeDepth() {microtasks_depth_--;}
  inline int GetMicrotasksScopeDepth() { return microtasks_depth_; }

  // Possibly nested microtasks suppression scopes prevent microtasks
  // from running.
  inline void IncrementMicrotasksSuppressions() {microtasks_suppressions_++;}
  inline void DecrementMicrotasksSuppressions() {microtasks_suppressions_--;}
  inline bool HasMicrotasksSuppressions() { return !!microtasks_suppressions_; }

#ifdef DEBUG
  // In debug we check that calls not intended to invoke microtasks are
  // still correctly wrapped with microtask scopes.
  inline void IncrementDebugMicrotasksScopeDepth() {debug_microtasks_depth_++;}
  inline void DecrementDebugMicrotasksScopeDepth() {debug_microtasks_depth_--;}
  inline bool DebugMicrotasksScopeDepthIsZero() {
    return debug_microtasks_depth_ == 0;
  }
#endif

  inline void set_microtasks_policy(v8::MicrotasksPolicy policy);
  inline v8::MicrotasksPolicy microtasks_policy() const;

  inline void EnterContext(Handle<Context> context);
  inline void LeaveContext();
  inline bool LastEnteredContextWas(Handle<Context> context);

  // Returns the last entered context or an empty handle if no
  // contexts have been entered.
  inline Handle<Context> LastEnteredContext();

  inline void EnterMicrotaskContext(Handle<Context> context);
  inline void LeaveMicrotaskContext();
  inline Handle<Context> MicrotaskContext();
  inline bool MicrotaskContextIsLastEnteredContext() const {
    return microtask_context_ &&
           entered_context_count_during_microtasks_ == entered_contexts_.size();
  }

  inline void SaveContext(Context* context);
  inline Context* RestoreContext();
  inline bool HasSavedContexts();

  inline DetachableVector<Object**>* blocks() { return &blocks_; }
  Isolate* isolate() const { return isolate_; }

  void ReturnBlock(Object** block) {
    DCHECK_NOT_NULL(block);
    if (spare_ != nullptr) DeleteArray(spare_);
    spare_ = block;
  }

 private:
  void ResetAfterArchive() {
    blocks_.detach();
    entered_contexts_.detach();
    saved_contexts_.detach();
    microtask_context_ = nullptr;
    entered_context_count_during_microtasks_ = 0;
    spare_ = nullptr;
    last_handle_before_deferred_block_ = nullptr;
    call_depth_ = 0;
  }

  void Free() {
    DCHECK(blocks_.empty());
    DCHECK(entered_contexts_.empty());
    DCHECK(saved_contexts_.empty());
    DCHECK(!microtask_context_);

    blocks_.free();
    entered_contexts_.free();
    saved_contexts_.free();
    if (spare_ != nullptr) {
      DeleteArray(spare_);
      spare_ = nullptr;
    }
    DCHECK_EQ(call_depth_, 0);
  }

  void BeginDeferredScope();
  DeferredHandles* Detach(Object** prev_limit);

  Isolate* isolate_;
  DetachableVector<Object**> blocks_;
  // Used as a stack to keep track of entered contexts.
  DetachableVector<Context*> entered_contexts_;
  // Used as a stack to keep track of saved contexts.
  DetachableVector<Context*> saved_contexts_;
  Context* microtask_context_;
  Object** spare_;
  int call_depth_;
  int microtasks_depth_;
  int microtasks_suppressions_;
  size_t entered_contexts_count_;
  size_t entered_context_count_during_microtasks_;
#ifdef DEBUG
  int debug_microtasks_depth_;
#endif
  v8::MicrotasksPolicy microtasks_policy_;
  Object** last_handle_before_deferred_block_;
  // This is only used for threading support.
  HandleScopeData handle_scope_data_;

  void IterateThis(RootVisitor* v);
  char* RestoreThreadHelper(char* from);
  char* ArchiveThreadHelper(char* to);

  friend class DeferredHandles;
  friend class DeferredHandleScope;
  friend class HandleScopeImplementerOffsets;

  DISALLOW_COPY_AND_ASSIGN(HandleScopeImplementer);
};

class HandleScopeImplementerOffsets {
 public:
  enum Offsets {
    kMicrotaskContext = offsetof(HandleScopeImplementer, microtask_context_),
    kEnteredContexts = offsetof(HandleScopeImplementer, entered_contexts_),
    kEnteredContextsCount =
        offsetof(HandleScopeImplementer, entered_contexts_count_),
    kEnteredContextCountDuringMicrotasks = offsetof(
        HandleScopeImplementer, entered_context_count_during_microtasks_)
  };

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(HandleScopeImplementerOffsets);
};

const int kHandleBlockSize = v8::internal::KB - 2;  // fit in one page


void HandleScopeImplementer::set_microtasks_policy(
    v8::MicrotasksPolicy policy) {
  microtasks_policy_ = policy;
}


v8::MicrotasksPolicy HandleScopeImplementer::microtasks_policy() const {
  return microtasks_policy_;
}


void HandleScopeImplementer::SaveContext(Context* context) {
  saved_contexts_.push_back(context);
}


Context* HandleScopeImplementer::RestoreContext() {
  Context* last_context = saved_contexts_.back();
  saved_contexts_.pop_back();
  return last_context;
}


bool HandleScopeImplementer::HasSavedContexts() {
  return !saved_contexts_.empty();
}


void HandleScopeImplementer::EnterContext(Handle<Context> context) {
  entered_contexts_.push_back(*context);
  entered_contexts_count_ = entered_contexts_.size();
}

void HandleScopeImplementer::LeaveContext() {
  entered_contexts_.pop_back();
  entered_contexts_count_ = entered_contexts_.size();
}

bool HandleScopeImplementer::LastEnteredContextWas(Handle<Context> context) {
  return !entered_contexts_.empty() && entered_contexts_.back() == *context;
}


Handle<Context> HandleScopeImplementer::LastEnteredContext() {
  if (entered_contexts_.empty()) return Handle<Context>::null();
  return Handle<Context>(entered_contexts_.back());
}

void HandleScopeImplementer::EnterMicrotaskContext(Handle<Context> context) {
  DCHECK(!microtask_context_);
  microtask_context_ = *context;
  entered_context_count_during_microtasks_ = entered_contexts_.size();
}

void HandleScopeImplementer::LeaveMicrotaskContext() {
  DCHECK(microtask_context_);
  microtask_context_ = nullptr;
  entered_context_count_during_microtasks_ = 0;
}

Handle<Context> HandleScopeImplementer::MicrotaskContext() {
  if (microtask_context_) return Handle<Context>(microtask_context_);
  return Handle<Context>::null();
}

// If there's a spare block, use it for growing the current scope.
internal::Object** HandleScopeImplementer::GetSpareOrNewBlock() {
  internal::Object** block =
      (spare_ != nullptr) ? spare_
                          : NewArray<internal::Object*>(kHandleBlockSize);
  spare_ = nullptr;
  return block;
}


void HandleScopeImplementer::DeleteExtensions(internal::Object** prev_limit) {
  while (!blocks_.empty()) {
    internal::Object** block_start = blocks_.back();
    internal::Object** block_limit = block_start + kHandleBlockSize;

    // SealHandleScope may make the prev_limit to point inside the block.
    if (block_start <= prev_limit && prev_limit <= block_limit) {
#ifdef ENABLE_HANDLE_ZAPPING
      internal::HandleScope::ZapRange(prev_limit, block_limit);
#endif
      break;
    }

    blocks_.pop_back();
#ifdef ENABLE_HANDLE_ZAPPING
    internal::HandleScope::ZapRange(block_start, block_limit);
#endif
    if (spare_ != nullptr) {
      DeleteArray(spare_);
    }
    spare_ = block_start;
  }
  DCHECK((blocks_.empty() && prev_limit == nullptr) ||
         (!blocks_.empty() && prev_limit != nullptr));
}

// Interceptor functions called from generated inline caches to notify
// CPU profiler that external callbacks are invoked.
void InvokeAccessorGetterCallback(
    v8::Local<v8::Name> property,
    const v8::PropertyCallbackInfo<v8::Value>& info,
    v8::AccessorNameGetterCallback getter);

void InvokeFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>& info,
                            v8::FunctionCallback callback);

class Testing {
 public:
  static v8::Testing::StressType stress_type() { return stress_type_; }
  static void set_stress_type(v8::Testing::StressType stress_type) {
    stress_type_ = stress_type;
  }

 private:
  static v8::Testing::StressType stress_type_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_API_H_
