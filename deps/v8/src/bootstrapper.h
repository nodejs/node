// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BOOTSTRAPPER_H_
#define V8_BOOTSTRAPPER_H_

#include "src/factory.h"

namespace v8 {
namespace internal {

// A SourceCodeCache uses a FixedArray to store pairs of
// (OneByteString*, JSFunction*), mapping names of native code files
// (runtime.js, etc.) to precompiled functions. Instead of mapping
// names to functions it might make sense to let the JS2C tool
// generate an index for each native JS file.
class SourceCodeCache final BASE_EMBEDDED {
 public:
  explicit SourceCodeCache(Script::Type type): type_(type), cache_(NULL) { }

  void Initialize(Isolate* isolate, bool create_heap_objects) {
    cache_ = create_heap_objects ? isolate->heap()->empty_fixed_array() : NULL;
  }

  void Iterate(ObjectVisitor* v) {
    v->VisitPointer(bit_cast<Object**, FixedArray**>(&cache_));
  }

  bool Lookup(Vector<const char> name, Handle<SharedFunctionInfo>* handle) {
    for (int i = 0; i < cache_->length(); i+=2) {
      SeqOneByteString* str = SeqOneByteString::cast(cache_->get(i));
      if (str->IsUtf8EqualTo(name)) {
        *handle = Handle<SharedFunctionInfo>(
            SharedFunctionInfo::cast(cache_->get(i + 1)));
        return true;
      }
    }
    return false;
  }

  void Add(Vector<const char> name, Handle<SharedFunctionInfo> shared) {
    Isolate* isolate = shared->GetIsolate();
    Factory* factory = isolate->factory();
    HandleScope scope(isolate);
    int length = cache_->length();
    Handle<FixedArray> new_array = factory->NewFixedArray(length + 2, TENURED);
    cache_->CopyTo(0, *new_array, 0, cache_->length());
    cache_ = *new_array;
    Handle<String> str =
        factory->NewStringFromAscii(name, TENURED).ToHandleChecked();
    DCHECK(!str.is_null());
    cache_->set(length, *str);
    cache_->set(length + 1, *shared);
    Script::cast(shared->script())->set_type(type_);
  }

 private:
  Script::Type type_;
  FixedArray* cache_;
  DISALLOW_COPY_AND_ASSIGN(SourceCodeCache);
};

enum ContextType { FULL_CONTEXT, THIN_CONTEXT, DEBUG_CONTEXT };

// The Boostrapper is the public interface for creating a JavaScript global
// context.
class Bootstrapper final {
 public:
  static void InitializeOncePerProcess();
  static void TearDownExtensions();

  // Requires: Heap::SetUp has been called.
  void Initialize(bool create_heap_objects);
  void TearDown();

  // Creates a JavaScript Global Context with initial object graph.
  // The returned value is a global handle casted to V8Environment*.
  Handle<Context> CreateEnvironment(
      MaybeHandle<JSGlobalProxy> maybe_global_proxy,
      v8::Local<v8::ObjectTemplate> global_object_template,
      v8::ExtensionConfiguration* extensions,
      ContextType context_type = FULL_CONTEXT);

  // Detach the environment from its outer global object.
  void DetachGlobal(Handle<Context> env);

  // Traverses the pointers for memory management.
  void Iterate(ObjectVisitor* v);

  // Accessor for the native scripts source code.
  template <class Source>
  Handle<String> SourceLookup(int index);

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
                            Handle<Object> argv[]);
  static bool CompileBuiltin(Isolate* isolate, int index);
  static bool CompileExperimentalBuiltin(Isolate* isolate, int index);
  static bool CompileExtraBuiltin(Isolate* isolate, int index);
  static bool CompileExperimentalExtraBuiltin(Isolate* isolate, int index);

  static void ExportFromRuntime(Isolate* isolate, Handle<JSObject> container);
  static void ExportExperimentalFromRuntime(Isolate* isolate,
                                            Handle<JSObject> container);

 private:
  Isolate* isolate_;
  typedef int NestingCounterType;
  NestingCounterType nesting_;
  SourceCodeCache extensions_cache_;

  friend class BootstrapperActive;
  friend class Isolate;
  friend class NativesExternalStringResource;

  explicit Bootstrapper(Isolate* isolate);

  static v8::Extension* free_buffer_extension_;
  static v8::Extension* gc_extension_;
  static v8::Extension* externalize_string_extension_;
  static v8::Extension* statistics_extension_;
  static v8::Extension* trigger_failure_extension_;

  DISALLOW_COPY_AND_ASSIGN(Bootstrapper);
};


class BootstrapperActive final BASE_EMBEDDED {
 public:
  explicit BootstrapperActive(Bootstrapper* bootstrapper)
      : bootstrapper_(bootstrapper) {
    ++bootstrapper_->nesting_;
  }

  ~BootstrapperActive() {
    --bootstrapper_->nesting_;
  }

 private:
  Bootstrapper* bootstrapper_;

  DISALLOW_COPY_AND_ASSIGN(BootstrapperActive);
};


class NativesExternalStringResource final
    : public v8::String::ExternalOneByteStringResource {
 public:
  NativesExternalStringResource(const char* source, size_t length)
      : data_(source), length_(length) {}
  const char* data() const override { return data_; }
  size_t length() const override { return length_; }

 private:
  const char* data_;
  size_t length_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BOOTSTRAPPER_H_
