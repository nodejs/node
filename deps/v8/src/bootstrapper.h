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

  // Traverses the pointers for memory management.
  static void Iterate(ObjectVisitor* v);

  // Accessors for the native scripts cache. Used in lazy loading.
  static Handle<String> NativesSourceLookup(int index);
  static bool NativesCacheLookup(Vector<const char> name,
                                 Handle<JSFunction>* handle);
  static void NativesCacheAdd(Vector<const char> name, Handle<JSFunction> fun);

  // Append code that needs fixup at the end of boot strapping.
  static void AddFixup(Code* code, MacroAssembler* masm);

  // Tells whether bootstrapping is active.
  static bool IsActive();

  // Encoding/decoding support for fixup flags.
  class FixupFlagsUseCodeObject: public BitField<bool, 0, 1> {};
  class FixupFlagsArgumentsCount: public BitField<uint32_t, 1, 32-1> {};

  // Support for thread preemption.
  static int ArchiveSpacePerThread();
  static char* ArchiveState(char* to);
  static char* RestoreState(char* from);
  static void FreeThreadResources();
};

}}  // namespace v8::internal

#endif  // V8_BOOTSTRAPPER_H_
