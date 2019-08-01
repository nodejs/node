// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INIT_BOOTSTRAPPER_H_
#define V8_INIT_BOOTSTRAPPER_H_

#include "src/heap/factory.h"
#include "src/objects/fixed-array.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/visitors.h"
#include "src/snapshot/natives.h"

namespace v8 {
namespace internal {

// A SourceCodeCache uses a FixedArray to store pairs of
// (OneByteString, SharedFunctionInfo), mapping names of native code files
// (array.js, etc.) to precompiled functions. Instead of mapping
// names to functions it might make sense to let the JS2C tool
// generate an index for each native JS file.
class SourceCodeCache final {
 public:
  explicit SourceCodeCache(Script::Type type) : type_(type) {}

  void Initialize(Isolate* isolate, bool create_heap_objects);

  void Iterate(RootVisitor* v);

  bool Lookup(Isolate* isolate, Vector<const char> name,
              Handle<SharedFunctionInfo>* handle);

  void Add(Isolate* isolate, Vector<const char> name,
           Handle<SharedFunctionInfo> shared);

 private:
  Script::Type type_;
  FixedArray cache_;
  DISALLOW_COPY_AND_ASSIGN(SourceCodeCache);
};

// The Boostrapper is the public interface for creating a JavaScript global
// context.
class Bootstrapper final {
 public:
  static void InitializeOncePerProcess();

  // Requires: Heap::SetUp has been called.
  void Initialize(bool create_heap_objects);
  void TearDown();

  // Creates a JavaScript Global Context with initial object graph.
  // The returned value is a global handle casted to V8Environment*.
  Handle<Context> CreateEnvironment(
      MaybeHandle<JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_object_template,
      v8::ExtensionConfiguration* extensions, size_t context_snapshot_index,
      v8::DeserializeEmbedderFieldsCallback embedder_fields_deserializer,
      v8::MicrotaskQueue* microtask_queue);

  Handle<JSGlobalProxy> NewRemoteContext(
      MaybeHandle<JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_object_template);

  // Detach the environment from its outer global object.
  void DetachGlobal(Handle<Context> env);

  // Traverses the pointers for memory management.
  void Iterate(RootVisitor* v);

  // Accessor for the native scripts source code.
  Handle<String> GetNativeSource(NativeType type, int index);

  // Tells whether bootstrapping is active.
  bool IsActive() const { return nesting_ != 0; }

  // Support for thread preemption.
  static int ArchiveSpacePerThread();
  char* ArchiveState(char* to);
  char* RestoreState(char* from);
  void FreeThreadResources();

  // Used for new context creation.
  bool InstallExtensions(Handle<Context> native_context,
                         v8::ExtensionConfiguration* extensions);

  SourceCodeCache* extensions_cache() { return &extensions_cache_; }

  static bool CompileNative(Isolate* isolate, Vector<const char> name,
                            Handle<String> source, int argc,
                            Handle<Object> argv[], NativesFlag natives_flag);
  static bool CompileExtraBuiltin(Isolate* isolate, int index);
  static bool CompileExperimentalExtraBuiltin(Isolate* isolate, int index);

 private:
  // Log newly created Map objects if no snapshot was used.
  void LogAllMaps();

  Isolate* isolate_;
  using NestingCounterType = int;
  NestingCounterType nesting_;
  SourceCodeCache extensions_cache_;

  friend class BootstrapperActive;
  friend class Isolate;
  friend class NativesExternalStringResource;

  explicit Bootstrapper(Isolate* isolate);

  DISALLOW_COPY_AND_ASSIGN(Bootstrapper);
};

class BootstrapperActive final {
 public:
  explicit BootstrapperActive(Bootstrapper* bootstrapper)
      : bootstrapper_(bootstrapper) {
    ++bootstrapper_->nesting_;
  }

  ~BootstrapperActive() { --bootstrapper_->nesting_; }

 private:
  Bootstrapper* bootstrapper_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapperActive);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INIT_BOOTSTRAPPER_H_
