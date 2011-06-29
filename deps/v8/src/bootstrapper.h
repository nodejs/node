// Copyright 2006-2008 the V8 project authors. All rights reserved.
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


#ifndef V8_BOOTSTRAPPER_H_
#define V8_BOOTSTRAPPER_H_

#include "allocation.h"

namespace v8 {
namespace internal {


// A SourceCodeCache uses a FixedArray to store pairs of
// (AsciiString*, JSFunction*), mapping names of native code files
// (runtime.js, etc.) to precompiled functions. Instead of mapping
// names to functions it might make sense to let the JS2C tool
// generate an index for each native JS file.
class SourceCodeCache BASE_EMBEDDED {
 public:
  explicit SourceCodeCache(Script::Type type): type_(type), cache_(NULL) { }

  void Initialize(bool create_heap_objects) {
    cache_ = create_heap_objects ? HEAP->empty_fixed_array() : NULL;
  }

  void Iterate(ObjectVisitor* v) {
    v->VisitPointer(BitCast<Object**, FixedArray**>(&cache_));
  }

  bool Lookup(Vector<const char> name, Handle<SharedFunctionInfo>* handle) {
    for (int i = 0; i < cache_->length(); i+=2) {
      SeqAsciiString* str = SeqAsciiString::cast(cache_->get(i));
      if (str->IsEqualTo(name)) {
        *handle = Handle<SharedFunctionInfo>(
            SharedFunctionInfo::cast(cache_->get(i + 1)));
        return true;
      }
    }
    return false;
  }

  void Add(Vector<const char> name, Handle<SharedFunctionInfo> shared) {
    HandleScope scope;
    int length = cache_->length();
    Handle<FixedArray> new_array =
        FACTORY->NewFixedArray(length + 2, TENURED);
    cache_->CopyTo(0, *new_array, 0, cache_->length());
    cache_ = *new_array;
    Handle<String> str = FACTORY->NewStringFromAscii(name, TENURED);
    cache_->set(length, *str);
    cache_->set(length + 1, *shared);
    Script::cast(shared->script())->set_type(Smi::FromInt(type_));
  }

 private:
  Script::Type type_;
  FixedArray* cache_;
  DISALLOW_COPY_AND_ASSIGN(SourceCodeCache);
};


// The Boostrapper is the public interface for creating a JavaScript global
// context.
class Bootstrapper {
 public:
  // Requires: Heap::Setup has been called.
  void Initialize(bool create_heap_objects);
  void TearDown();

  // Creates a JavaScript Global Context with initial object graph.
  // The returned value is a global handle casted to V8Environment*.
  Handle<Context> CreateEnvironment(
      Isolate* isolate,
      Handle<Object> global_object,
      v8::Handle<v8::ObjectTemplate> global_template,
      v8::ExtensionConfiguration* extensions);

  // Detach the environment from its outer global object.
  void DetachGlobal(Handle<Context> env);

  // Reattach an outer global object to an environment.
  void ReattachGlobal(Handle<Context> env, Handle<Object> global_object);

  // Traverses the pointers for memory management.
  void Iterate(ObjectVisitor* v);

  // Accessor for the native scripts source code.
  Handle<String> NativesSourceLookup(int index);

  // Tells whether bootstrapping is active.
  bool IsActive() const { return nesting_ != 0; }

  // Support for thread preemption.
  static int ArchiveSpacePerThread();
  char* ArchiveState(char* to);
  char* RestoreState(char* from);
  void FreeThreadResources();

  // This will allocate a char array that is deleted when V8 is shut down.
  // It should only be used for strictly finite allocations.
  char* AllocateAutoDeletedArray(int bytes);

  // Used for new context creation.
  bool InstallExtensions(Handle<Context> global_context,
                         v8::ExtensionConfiguration* extensions);

  SourceCodeCache* extensions_cache() { return &extensions_cache_; }

 private:
  typedef int NestingCounterType;
  NestingCounterType nesting_;
  SourceCodeCache extensions_cache_;
  // This is for delete, not delete[].
  List<char*>* delete_these_non_arrays_on_tear_down_;
  // This is for delete[]
  List<char*>* delete_these_arrays_on_tear_down_;

  friend class BootstrapperActive;
  friend class Isolate;
  friend class NativesExternalStringResource;

  Bootstrapper();

  DISALLOW_COPY_AND_ASSIGN(Bootstrapper);
};


class BootstrapperActive BASE_EMBEDDED {
 public:
  BootstrapperActive() {
    ++Isolate::Current()->bootstrapper()->nesting_;
  }

  ~BootstrapperActive() {
    --Isolate::Current()->bootstrapper()->nesting_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BootstrapperActive);
};


class NativesExternalStringResource
    : public v8::String::ExternalAsciiStringResource {
 public:
  NativesExternalStringResource(Bootstrapper* bootstrapper,
                                const char* source,
                                size_t length);

  const char* data() const {
    return data_;
  }

  size_t length() const {
    return length_;
  }
 private:
  const char* data_;
  size_t length_;
};

}}  // namespace v8::internal

#endif  // V8_BOOTSTRAPPER_H_
