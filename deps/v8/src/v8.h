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

//
// Top include for all V8 .cc files.
//

#ifndef V8_V8_H_
#define V8_V8_H_

#if defined(GOOGLE3)
// Google3 special flag handling.
#if defined(DEBUG) && defined(NDEBUG)
// If both are defined in Google3, then we are building an optimized v8 with
// assertions enabled.
#undef NDEBUG
#elif !defined(DEBUG) && !defined(NDEBUG)
// If neither is defined in Google3, then we are building a debug v8. Mark it
// as such.
#define DEBUG
#endif
#endif  // defined(GOOGLE3)

// V8 only uses DEBUG, but included external files
// may use NDEBUG - make sure they are consistent.
#if defined(DEBUG) && defined(NDEBUG)
#error both DEBUG and NDEBUG are set
#endif

// Basic includes
#include "../include/v8.h"
#include "globals.h"
#include "checks.h"
#include "allocation.h"
#include "utils.h"
#include "flags.h"

// Objects & heap
#include "objects.h"
#include "spaces.h"
#include "heap.h"
#include "objects-inl.h"
#include "spaces-inl.h"
#include "heap-inl.h"
#include "messages.h"

namespace v8 { namespace internal {

class V8 : public AllStatic {
 public:
  // Global actions.

  // If Initialize is called with des == NULL, the
  // initial state is created from scratch. If a non-null Deserializer
  // is given, the initial state is created by reading the
  // deserialized data into an empty heap.
  static bool Initialize(Deserializer* des);
  static void TearDown();
  static bool HasBeenSetup() { return has_been_setup_; }
  static bool HasBeenDisposed() { return has_been_disposed_; }

  // Report process out of memory. Implementation found in api.cc.
  static void FatalProcessOutOfMemory(const char* location);
 private:
  static bool has_been_setup_;
  static bool has_been_disposed_;
};

} }  // namespace v8::internal

namespace i = v8::internal;

#endif  // V8_V8_H_
