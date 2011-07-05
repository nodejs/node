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

namespace v8 {
namespace internal {


class BootstrapperActive BASE_EMBEDDED {
 public:
  BootstrapperActive() { nesting_++; }
  ~BootstrapperActive() { nesting_--; }

  // Support for thread preemption.
  static int ArchiveSpacePerThread();
  static char* ArchiveState(char* to);
  static char* RestoreState(char* from);

 private:
  static bool IsActive() { return nesting_ != 0; }
  static int nesting_;
  friend class Bootstrapper;
};


// The Boostrapper is the public interface for creating a JavaScript global
// context.
class Bootstrapper : public AllStatic {
 public:
  // Requires: Heap::Setup has been called.
  static void Initialize(bool create_heap_objects);
  static void TearDown();

  // Creates a JavaScript Global Context with initial object graph.
  // The returned value is a global handle casted to V8Environment*.
  static Handle<Context> CreateEnvironment(
      Handle<Object> global_object,
      v8::Handle<v8::ObjectTemplate> global_template,
      v8::ExtensionConfiguration* extensions);

  // Detach the environment from its outer global object.
  static void DetachGlobal(Handle<Context> env);

  // Reattach an outer global object to an environment.
  static void ReattachGlobal(Handle<Context> env, Handle<Object> global_object);

  // Traverses the pointers for memory management.
  static void Iterate(ObjectVisitor* v);

  // Accessor for the native scripts source code.
  static Handle<String> NativesSourceLookup(int index);

  // Tells whether bootstrapping is active.
  static bool IsActive() { return BootstrapperActive::IsActive(); }

  // Support for thread preemption.
  static int ArchiveSpacePerThread();
  static char* ArchiveState(char* to);
  static char* RestoreState(char* from);
  static void FreeThreadResources();

  // This will allocate a char array that is deleted when V8 is shut down.
  // It should only be used for strictly finite allocations.
  static char* AllocateAutoDeletedArray(int bytes);

  // Used for new context creation.
  static bool InstallExtensions(Handle<Context> global_context,
                                v8::ExtensionConfiguration* extensions);
};


class NativesExternalStringResource
    : public v8::String::ExternalAsciiStringResource {
 public:
  explicit NativesExternalStringResource(const char* source);

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
