// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_V8_CONTEXT_H_
#define INCLUDE_V8_CONTEXT_H_

#include <stdint.h>

#include "v8-data.h"          // NOLINT(build/include_directory)
#include "v8-local-handle.h"  // NOLINT(build/include_directory)
#include "v8-snapshot.h"      // NOLINT(build/include_directory)
#include "v8config.h"         // NOLINT(build/include_directory)

namespace v8 {

class Function;
class MicrotaskQueue;
class Object;
class ObjectTemplate;
class Value;
class String;

/**
 * A container for extension names.
 */
class V8_EXPORT ExtensionConfiguration {
 public:
  ExtensionConfiguration() : name_count_(0), names_(nullptr) {}
  ExtensionConfiguration(int name_count, const char* names[])
      : name_count_(name_count), names_(names) {}

  const char** begin() const { return &names_[0]; }
  const char** end() const { return &names_[name_count_]; }

 private:
  const int name_count_;
  const char** names_;
};

/**
 * A sandboxed execution context with its own set of built-in objects
 * and functions.
 */
class V8_EXPORT Context : public Data {
 public:
  /**
   * Returns the global proxy object.
   *
   * Global proxy object is a thin wrapper whose prototype points to actual
   * context's global object with the properties like Object, etc. This is done
   * that way for security reasons (for more details see
   * https://wiki.mozilla.org/Gecko:SplitWindow).
   *
   * Please note that changes to global proxy object prototype most probably
   * would break VM---v8 expects only global object as a prototype of global
   * proxy object.
   */
  Local<Object> Global();

  /**
   * Detaches the global object from its context before
   * the global object can be reused to create a new context.
   */
  void DetachGlobal();

  /**
   * Creates a new context and returns a handle to the newly allocated
   * context.
   *
   * \param isolate The isolate in which to create the context.
   *
   * \param extensions An optional extension configuration containing
   * the extensions to be installed in the newly created context.
   *
   * \param global_template An optional object template from which the
   * global object for the newly created context will be created.
   *
   * \param global_object An optional global object to be reused for
   * the newly created context. This global object must have been
   * created by a previous call to Context::New with the same global
   * template. The state of the global object will be completely reset
   * and only object identify will remain.
   */
  static Local<Context> New(
      Isolate* isolate, ExtensionConfiguration* extensions = nullptr,
      MaybeLocal<ObjectTemplate> global_template = MaybeLocal<ObjectTemplate>(),
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      DeserializeInternalFieldsCallback internal_fields_deserializer =
          DeserializeInternalFieldsCallback(),
      MicrotaskQueue* microtask_queue = nullptr);

  /**
   * Create a new context from a (non-default) context snapshot. There
   * is no way to provide a global object template since we do not create
   * a new global object from template, but we can reuse a global object.
   *
   * \param isolate See v8::Context::New.
   *
   * \param context_snapshot_index The index of the context snapshot to
   * deserialize from. Use v8::Context::New for the default snapshot.
   *
   * \param embedder_fields_deserializer Optional callback to deserialize
   * internal fields. It should match the SerializeInternalFieldCallback used
   * to serialize.
   *
   * \param extensions See v8::Context::New.
   *
   * \param global_object See v8::Context::New.
   */
  static MaybeLocal<Context> FromSnapshot(
      Isolate* isolate, size_t context_snapshot_index,
      DeserializeInternalFieldsCallback embedder_fields_deserializer =
          DeserializeInternalFieldsCallback(),
      ExtensionConfiguration* extensions = nullptr,
      MaybeLocal<Value> global_object = MaybeLocal<Value>(),
      MicrotaskQueue* microtask_queue = nullptr);

  /**
   * Returns an global object that isn't backed by an actual context.
   *
   * The global template needs to have access checks with handlers installed.
   * If an existing global object is passed in, the global object is detached
   * from its context.
   *
   * Note that this is different from a detached context where all accesses to
   * the global proxy will fail. Instead, the access check handlers are invoked.
   *
   * It is also not possible to detach an object returned by this method.
   * Instead, the access check handlers need to return nothing to achieve the
   * same effect.
   *
   * It is possible, however, to create a new context from the global object
   * returned by this method.
   */
  static MaybeLocal<Object> NewRemoteContext(
      Isolate* isolate, Local<ObjectTemplate> global_template,
      MaybeLocal<Value> global_object = MaybeLocal<Value>());

  /**
   * Sets the security token for the context.  To access an object in
   * another context, the security tokens must match.
   */
  void SetSecurityToken(Local<Value> token);

  /** Restores the security token to the default value. */
  void UseDefaultSecurityToken();

  /** Returns the security token of this context.*/
  Local<Value> GetSecurityToken();

  /**
   * Enter this context.  After entering a context, all code compiled
   * and run is compiled and run in this context.  If another context
   * is already entered, this old context is saved so it can be
   * restored when the new context is exited.
   */
  void Enter();

  /**
   * Exit this context.  Exiting the current context restores the
   * context that was in place when entering the current context.
   */
  void Exit();

  /** Returns the isolate associated with a current context. */
  Isolate* GetIsolate();

  /** Returns the microtask queue associated with a current context. */
  MicrotaskQueue* GetMicrotaskQueue();

  /**
   * The field at kDebugIdIndex used to be reserved for the inspector.
   * It now serves no purpose.
   */
  enum EmbedderDataFields { kDebugIdIndex = 0 };

  /**
   * Return the number of fields allocated for embedder data.
   */
  uint32_t GetNumberOfEmbedderDataFields();

  /**
   * Gets the embedder data with the given index, which must have been set by a
   * previous call to SetEmbedderData with the same index.
   */
  V8_INLINE Local<Value> GetEmbedderData(int index);

  /**
   * Gets the binding object used by V8 extras. Extra natives get a reference
   * to this object and can use it to "export" functionality by adding
   * properties. Extra natives can also "import" functionality by accessing
   * properties added by the embedder using the V8 API.
   */
  Local<Object> GetExtrasBindingObject();

  /**
   * Sets the embedder data with the given index, growing the data as
   * needed. Note that index 0 currently has a special meaning for Chrome's
   * debugger.
   */
  void SetEmbedderData(int index, Local<Value> value);

  /**
   * Gets a 2-byte-aligned native pointer from the embedder data with the given
   * index, which must have been set by a previous call to
   * SetAlignedPointerInEmbedderData with the same index. Note that index 0
   * currently has a special meaning for Chrome's debugger.
   */
  V8_INLINE void* GetAlignedPointerFromEmbedderData(int index);

  /**
   * Sets a 2-byte-aligned native pointer in the embedder data with the given
   * index, growing the data as needed. Note that index 0 currently has a
   * special meaning for Chrome's debugger.
   */
  void SetAlignedPointerInEmbedderData(int index, void* value);

  /**
   * Control whether code generation from strings is allowed. Calling
   * this method with false will disable 'eval' and the 'Function'
   * constructor for code running in this context. If 'eval' or the
   * 'Function' constructor are used an exception will be thrown.
   *
   * If code generation from strings is not allowed the
   * V8::AllowCodeGenerationFromStrings callback will be invoked if
   * set before blocking the call to 'eval' or the 'Function'
   * constructor. If that callback returns true, the call will be
   * allowed, otherwise an exception will be thrown. If no callback is
   * set an exception will be thrown.
   */
  void AllowCodeGenerationFromStrings(bool allow);

  /**
   * Returns true if code generation from strings is allowed for the context.
   * For more details see AllowCodeGenerationFromStrings(bool) documentation.
   */
  bool IsCodeGenerationFromStringsAllowed() const;

  /**
   * Sets the error description for the exception that is thrown when
   * code generation from strings is not allowed and 'eval' or the 'Function'
   * constructor are called.
   */
  void SetErrorMessageForCodeGenerationFromStrings(Local<String> message);

  /**
   * Sets the error description for the exception that is thrown when
   * wasm code generation is not allowed.
   */
  void SetErrorMessageForWasmCodeGeneration(Local<String> message);

  /**
   * Return data that was previously attached to the context snapshot via
   * SnapshotCreator, and removes the reference to it.
   * Repeated call with the same index returns an empty MaybeLocal.
   */
  template <class T>
  V8_INLINE MaybeLocal<T> GetDataFromSnapshotOnce(size_t index);

  /**
   * If callback is set, abort any attempt to execute JavaScript in this
   * context, call the specified callback, and throw an exception.
   * To unset abort, pass nullptr as callback.
   */
  using AbortScriptExecutionCallback = void (*)(Isolate* isolate,
                                                Local<Context> context);
  void SetAbortScriptExecution(AbortScriptExecutionCallback callback);

  /**
   * Returns the value that was set or restored by
   * SetContinuationPreservedEmbedderData(), if any.
   */
  Local<Value> GetContinuationPreservedEmbedderData() const;

  /**
   * Sets a value that will be stored on continuations and reset while the
   * continuation runs.
   */
  void SetContinuationPreservedEmbedderData(Local<Value> context);

  /**
   * Set or clear hooks to be invoked for promise lifecycle operations.
   * To clear a hook, set it to an empty v8::Function. Each function will
   * receive the observed promise as the first argument. If a chaining
   * operation is used on a promise, the init will additionally receive
   * the parent promise as the second argument.
   */
  void SetPromiseHooks(Local<Function> init_hook, Local<Function> before_hook,
                       Local<Function> after_hook,
                       Local<Function> resolve_hook);

  /**
   * Stack-allocated class which sets the execution context for all
   * operations executed within a local scope.
   */
  class V8_NODISCARD Scope {
   public:
    explicit V8_INLINE Scope(Local<Context> context) : context_(context) {
      context_->Enter();
    }
    V8_INLINE ~Scope() { context_->Exit(); }

   private:
    Local<Context> context_;
  };

  /**
   * Stack-allocated class to support the backup incumbent settings object
   * stack.
   * https://html.spec.whatwg.org/multipage/webappapis.html#backup-incumbent-settings-object-stack
   */
  class V8_EXPORT V8_NODISCARD BackupIncumbentScope final {
   public:
    /**
     * |backup_incumbent_context| is pushed onto the backup incumbent settings
     * object stack.
     */
    explicit BackupIncumbentScope(Local<Context> backup_incumbent_context);
    ~BackupIncumbentScope();

   private:
    friend class internal::Isolate;

    uintptr_t JSStackComparableAddressPrivate() const {
      return js_stack_comparable_address_;
    }

    Local<Context> backup_incumbent_context_;
    uintptr_t js_stack_comparable_address_ = 0;
    const BackupIncumbentScope* prev_ = nullptr;
  };

  V8_INLINE static Context* Cast(Data* data);

 private:
  friend class Value;
  friend class Script;
  friend class Object;
  friend class Function;

  static void CheckCast(Data* obj);

  internal::Address* GetDataFromSnapshotOnce(size_t index);
  Local<Value> SlowGetEmbedderData(int index);
  void* SlowGetAlignedPointerFromEmbedderData(int index);
};

// --- Implementation ---

Local<Value> Context::GetEmbedderData(int index) {
#ifndef V8_ENABLE_CHECKS
  using A = internal::Address;
  using I = internal::Internals;
  A ctx = *reinterpret_cast<const A*>(this);
  A embedder_data =
      I::ReadTaggedPointerField(ctx, I::kNativeContextEmbedderDataOffset);
  int value_offset =
      I::kEmbedderDataArrayHeaderSize + (I::kEmbedderDataSlotSize * index);
  A value = I::ReadRawField<A>(embedder_data, value_offset);
#ifdef V8_COMPRESS_POINTERS
  // We read the full pointer value and then decompress it in order to avoid
  // dealing with potential endiannes issues.
  value =
      I::DecompressTaggedAnyField(embedder_data, static_cast<uint32_t>(value));
#endif
  internal::Isolate* isolate = internal::IsolateFromNeverReadOnlySpaceObject(
      *reinterpret_cast<A*>(this));
  A* result = HandleScope::CreateHandle(isolate, value);
  return Local<Value>(reinterpret_cast<Value*>(result));
#else
  return SlowGetEmbedderData(index);
#endif
}

void* Context::GetAlignedPointerFromEmbedderData(int index) {
#if !defined(V8_ENABLE_CHECKS)
  using A = internal::Address;
  using I = internal::Internals;
  A ctx = *reinterpret_cast<const A*>(this);
  A embedder_data =
      I::ReadTaggedPointerField(ctx, I::kNativeContextEmbedderDataOffset);
  int value_offset = I::kEmbedderDataArrayHeaderSize +
                     (I::kEmbedderDataSlotSize * index) +
                     I::kEmbedderDataSlotExternalPointerOffset;
  Isolate* isolate = I::GetIsolateForSandbox(ctx);
  return reinterpret_cast<void*>(
      I::ReadExternalPointerField<internal::kEmbedderDataSlotPayloadTag>(
          isolate, embedder_data, value_offset));
#else
  return SlowGetAlignedPointerFromEmbedderData(index);
#endif
}

template <class T>
MaybeLocal<T> Context::GetDataFromSnapshotOnce(size_t index) {
  T* data = reinterpret_cast<T*>(GetDataFromSnapshotOnce(index));
  if (data) internal::PerformCastCheck(data);
  return Local<T>(data);
}

Context* Context::Cast(v8::Data* data) {
#ifdef V8_ENABLE_CHECKS
  CheckCast(data);
#endif
  return static_cast<Context*>(data);
}

}  // namespace v8

#endif  // INCLUDE_V8_CONTEXT_H_
