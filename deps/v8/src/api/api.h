// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_H_
#define V8_API_API_H_

#include <memory>

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/bigint.h"
#include "src/objects/contexts.h"
#include "src/objects/js-collection.h"
#include "src/objects/js-generator.h"
#include "src/objects/js-promise.h"
#include "src/objects/js-proxy.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/source-text-module.h"
#include "src/utils/detachable-vector.h"

#include "src/objects/templates.h"

namespace v8 {

namespace internal {
class JSArrayBufferView;
class JSFinalizationRegistry;
}  // namespace internal

namespace debug {
class AccessorPair;
class GeneratorObject;
class Script;
class WeakMap;
}  // namespace debug

// Constants used in the implementation of the API.  The most natural thing
// would usually be to place these with the classes that use them, but
// we want to keep them out of v8.h because it is an externally
// visible file.
class Consts {
 public:
  enum TemplateType { FUNCTION_TEMPLATE = 0, OBJECT_TEMPLATE = 1 };
};

template <typename T>
inline T ToCData(v8::internal::Object obj);

template <>
inline v8::internal::Address ToCData(v8::internal::Object obj);

template <typename T>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, T obj);

template <>
inline v8::internal::Handle<v8::internal::Object> FromCData(
    v8::internal::Isolate* isolate, v8::internal::Address obj);

class ApiFunction {
 public:
  explicit ApiFunction(v8::internal::Address addr) : addr_(addr) {}
  v8::internal::Address address() { return addr_; }

 private:
  v8::internal::Address addr_;
};

class RegisteredExtension {
 public:
  static void Register(std::unique_ptr<Extension>);
  static void UnregisterAll();
  Extension* extension() const { return extension_.get(); }
  RegisteredExtension* next() const { return next_; }
  static RegisteredExtension* first_extension() { return first_extension_; }

 private:
  explicit RegisteredExtension(Extension*);
  explicit RegisteredExtension(std::unique_ptr<Extension>);
  std::unique_ptr<Extension> extension_;
  RegisteredExtension* next_ = nullptr;
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
  V(UnboundModuleScript, SharedFunctionInfo)   \
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
  V(debug::WeakMap, JSWeakMap)                 \
  V(debug::AccessorPair, AccessorPair)         \
  V(Promise, JSPromise)                        \
  V(Primitive, Object)                         \
  V(PrimitiveArray, FixedArray)                \
  V(BigInt, BigInt)                            \
  V(ScriptOrModule, Script)                    \
  V(FixedArray, FixedArray)                    \
  V(ModuleRequest, ModuleRequest)              \
  IF_WASM(V, WasmMemoryObject, WasmMemoryObject)

class Utils {
 public:
  static inline bool ApiCheck(bool condition, const char* location,
                              const char* message) {
    if (!condition) Utils::ReportApiFailure(location, message);
    return condition;
  }
  static void ReportOOMFailure(v8::internal::Isolate* isolate,
                               const char* location, bool is_heap_oom);

  static inline Local<debug::AccessorPair> ToLocal(
      v8::internal::Handle<v8::internal::AccessorPair> obj);
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
  static inline Local<BigInt64Array> ToLocalBigInt64Array(
      v8::internal::Handle<v8::internal::JSTypedArray> obj);
  static inline Local<BigUint64Array> ToLocalBigUint64Array(
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
  static inline Local<BigInt> ToLocal(
      v8::internal::Handle<v8::internal::BigInt> obj);
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
  static inline Local<FixedArray> FixedArrayToLocal(
      v8::internal::Handle<v8::internal::FixedArray> obj);
  static inline Local<PrimitiveArray> PrimitiveArrayToLocal(
      v8::internal::Handle<v8::internal::FixedArray> obj);
  static inline Local<ScriptOrModule> ScriptOrModuleToLocal(
      v8::internal::Handle<v8::internal::Script> obj);

#define DECLARE_OPEN_HANDLE(From, To)                              \
  static inline v8::internal::Handle<v8::internal::To> OpenHandle( \
      const From* that, bool allow_empty_handle = false);

  OPEN_HANDLE_LIST(DECLARE_OPEN_HANDLE)

#undef DECLARE_OPEN_HANDLE

  template <class From, class To>
  static inline Local<To> Convert(v8::internal::Handle<From> obj);

  template <class T, class M>
  static inline v8::internal::Handle<v8::internal::Object> OpenPersistent(
      const v8::Persistent<T, M>& persistent) {
    return v8::internal::Handle<v8::internal::Object>(
        reinterpret_cast<v8::internal::Address*>(persistent.val_));
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

namespace internal {

class PersistentHandles;

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
  class V8_NODISCARD EnteredContextRewindScope {
   public:
    explicit EnteredContextRewindScope(HandleScopeImplementer* hsi)
        : hsi_(hsi), saved_entered_context_count_(hsi->EnteredContextCount()) {}

    ~EnteredContextRewindScope() {
      DCHECK_LE(saved_entered_context_count_, hsi_->EnteredContextCount());
      while (saved_entered_context_count_ < hsi_->EnteredContextCount())
        hsi_->LeaveContext();
    }

   private:
    HandleScopeImplementer* hsi_;
    size_t saved_entered_context_count_;
  };

  explicit HandleScopeImplementer(Isolate* isolate)
      : isolate_(isolate),
        spare_(nullptr),
        last_handle_before_deferred_block_(nullptr) {}

  ~HandleScopeImplementer() { DeleteArray(spare_); }

  HandleScopeImplementer(const HandleScopeImplementer&) = delete;
  HandleScopeImplementer& operator=(const HandleScopeImplementer&) = delete;

  // Threading support for handle data.
  static int ArchiveSpacePerThread();
  char* RestoreThread(char* from);
  char* ArchiveThread(char* to);
  void FreeThreadResources();

  // Garbage collection support.
  V8_EXPORT_PRIVATE void Iterate(v8::internal::RootVisitor* v);
  V8_EXPORT_PRIVATE static char* Iterate(v8::internal::RootVisitor* v,
                                         char* data);

  inline internal::Address* GetSpareOrNewBlock();
  inline void DeleteExtensions(internal::Address* prev_limit);

  inline void EnterContext(Context context);
  inline void LeaveContext();
  inline bool LastEnteredContextWas(Context context);
  inline size_t EnteredContextCount() const { return entered_contexts_.size(); }

  inline void EnterMicrotaskContext(Context context);

  // Returns the last entered context or an empty handle if no
  // contexts have been entered.
  inline Handle<Context> LastEnteredContext();
  inline Handle<Context> LastEnteredOrMicrotaskContext();

  inline void SaveContext(Context context);
  inline Context RestoreContext();
  inline bool HasSavedContexts();

  inline DetachableVector<Address*>* blocks() { return &blocks_; }
  Isolate* isolate() const { return isolate_; }

  void ReturnBlock(Address* block) {
    DCHECK_NOT_NULL(block);
    if (spare_ != nullptr) DeleteArray(spare_);
    spare_ = block;
  }

  static const size_t kEnteredContextsOffset;
  static const size_t kIsMicrotaskContextOffset;

 private:
  void ResetAfterArchive() {
    blocks_.detach();
    entered_contexts_.detach();
    is_microtask_context_.detach();
    saved_contexts_.detach();
    spare_ = nullptr;
    last_handle_before_deferred_block_ = nullptr;
  }

  void Free() {
    DCHECK(blocks_.empty());
    DCHECK(entered_contexts_.empty());
    DCHECK(is_microtask_context_.empty());
    DCHECK(saved_contexts_.empty());

    blocks_.free();
    entered_contexts_.free();
    is_microtask_context_.free();
    saved_contexts_.free();
    if (spare_ != nullptr) {
      DeleteArray(spare_);
      spare_ = nullptr;
    }
    DCHECK(isolate_->thread_local_top()->CallDepthIsZero());
  }

  void BeginDeferredScope();
  std::unique_ptr<PersistentHandles> DetachPersistent(Address* prev_limit);

  Isolate* isolate_;
  DetachableVector<Address*> blocks_;

  // Used as a stack to keep track of entered contexts.
  // If |i|th item of |entered_contexts_| is added by EnterMicrotaskContext,
  // `is_microtask_context_[i]` is 1.
  // TODO(tzik): Remove |is_microtask_context_| after the deprecated
  // v8::Isolate::GetEnteredContext() is removed.
  DetachableVector<Context> entered_contexts_;
  DetachableVector<int8_t> is_microtask_context_;

  // Used as a stack to keep track of saved contexts.
  DetachableVector<Context> saved_contexts_;
  Address* spare_;
  Address* last_handle_before_deferred_block_;
  // This is only used for threading support.
  HandleScopeData handle_scope_data_;

  void IterateThis(RootVisitor* v);
  char* RestoreThreadHelper(char* from);
  char* ArchiveThreadHelper(char* to);

  friend class HandleScopeImplementerOffsets;
  friend class PersistentHandlesScope;
};

const int kHandleBlockSize = v8::internal::KB - 2;  // fit in one page

void HandleScopeImplementer::SaveContext(Context context) {
  saved_contexts_.push_back(context);
}

Context HandleScopeImplementer::RestoreContext() {
  Context last_context = saved_contexts_.back();
  saved_contexts_.pop_back();
  return last_context;
}

bool HandleScopeImplementer::HasSavedContexts() {
  return !saved_contexts_.empty();
}

void HandleScopeImplementer::EnterContext(Context context) {
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());
  entered_contexts_.push_back(context);
  is_microtask_context_.push_back(0);
}

void HandleScopeImplementer::LeaveContext() {
  DCHECK(!entered_contexts_.empty());
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());
  entered_contexts_.pop_back();
  is_microtask_context_.pop_back();
}

bool HandleScopeImplementer::LastEnteredContextWas(Context context) {
  return !entered_contexts_.empty() && entered_contexts_.back() == context;
}

void HandleScopeImplementer::EnterMicrotaskContext(Context context) {
  DCHECK_EQ(entered_contexts_.size(), is_microtask_context_.size());
  entered_contexts_.push_back(context);
  is_microtask_context_.push_back(1);
}

// If there's a spare block, use it for growing the current scope.
internal::Address* HandleScopeImplementer::GetSpareOrNewBlock() {
  internal::Address* block =
      (spare_ != nullptr) ? spare_
                          : NewArray<internal::Address>(kHandleBlockSize);
  spare_ = nullptr;
  return block;
}

void HandleScopeImplementer::DeleteExtensions(internal::Address* prev_limit) {
  while (!blocks_.empty()) {
    internal::Address* block_start = blocks_.back();
    internal::Address* block_limit = block_start + kHandleBlockSize;

    // SealHandleScope may make the prev_limit to point inside the block.
    // Cast possibly-unrelated pointers to plain Addres before comparing them
    // to avoid undefined behavior.
    if (reinterpret_cast<Address>(block_start) <=
            reinterpret_cast<Address>(prev_limit) &&
        reinterpret_cast<Address>(prev_limit) <=
            reinterpret_cast<Address>(block_limit)) {
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

void InvokeFinalizationRegistryCleanupFromTask(
    Handle<Context> context,
    Handle<JSFinalizationRegistry> finalization_registry,
    Handle<Object> callback);

template <typename T>
EXPORT_TEMPLATE_DECLARE(V8_EXPORT_PRIVATE)
T ConvertDouble(double d);

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_H_
