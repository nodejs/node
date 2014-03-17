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

//
// Top include for all V8 .cc files.
//

#ifndef V8_V8_H_
#define V8_V8_H_

#if defined(GOOGLE3)
// Google3 special flag handling.
#if defined(DEBUG) && defined(NDEBUG)
// V8 only uses DEBUG and whenever it is set we are building a debug
// version of V8. We do not use NDEBUG and simply undef it here for
// consistency.
#undef NDEBUG
#endif
#endif  // defined(GOOGLE3)

// V8 only uses DEBUG, but included external files
// may use NDEBUG - make sure they are consistent.
#if defined(DEBUG) && defined(NDEBUG)
#error both DEBUG and NDEBUG are set
#endif

// Basic includes
#include "../include/v8.h"
#include "../include/v8-platform.h"
#include "v8globals.h"
#include "v8checks.h"
#include "allocation.h"
#include "assert-scope.h"
#include "v8utils.h"
#include "flags.h"

// Objects & heap
#include "objects-inl.h"
#include "spaces-inl.h"
#include "heap-inl.h"
#include "incremental-marking-inl.h"
#include "mark-compact-inl.h"
#include "log-inl.h"
#include "handles-inl.h"
#include "zone-inl.h"

namespace v8 {
namespace internal {

class Deserializer;

class V8 : public AllStatic {
 public:
  // Global actions.

  // If Initialize is called with des == NULL, the initial state is
  // created from scratch. If a non-null Deserializer is given, the
  // initial state is created by reading the deserialized data into an
  // empty heap.
  static bool Initialize(Deserializer* des);
  static void TearDown();

  // Report process out of memory. Implementation found in api.cc.
  static void FatalProcessOutOfMemory(const char* location,
                                      bool take_snapshot = false);

  // Allows an entropy source to be provided for use in random number
  // generation.
  static void SetEntropySource(EntropySource source);
  // Support for return-address rewriting profilers.
  static void SetReturnAddressLocationResolver(
      ReturnAddressLocationResolver resolver);
  // Support for entry hooking JITed code.
  static void SetFunctionEntryHook(FunctionEntryHook entry_hook);

  static void AddCallCompletedCallback(CallCompletedCallback callback);
  static void RemoveCallCompletedCallback(CallCompletedCallback callback);
  static void FireCallCompletedCallback(Isolate* isolate);

  static v8::ArrayBuffer::Allocator* ArrayBufferAllocator() {
    return array_buffer_allocator_;
  }

  static void SetArrayBufferAllocator(v8::ArrayBuffer::Allocator *allocator) {
    CHECK_EQ(NULL, array_buffer_allocator_);
    array_buffer_allocator_ = allocator;
  }

  static void InitializePlatform(v8::Platform* platform);
  static void ShutdownPlatform();
  static v8::Platform* GetCurrentPlatform();

 private:
  static void InitializeOncePerProcessImpl();
  static void InitializeOncePerProcess();

  // List of callbacks when a Call completes.
  static List<CallCompletedCallback>* call_completed_callbacks_;
  // Allocator for external array buffers.
  static v8::ArrayBuffer::Allocator* array_buffer_allocator_;
  // v8::Platform to use.
  static v8::Platform* platform_;
};


// JavaScript defines two kinds of 'nil'.
enum NilValue { kNullValue, kUndefinedValue };


} }  // namespace v8::internal

namespace i = v8::internal;

#endif  // V8_V8_H_
