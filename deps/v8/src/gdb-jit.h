// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_GDB_JIT_H_
#define V8_GDB_JIT_H_

#include "src/allocation.h"

//
// Basic implementation of GDB JIT Interface client.
// GBD JIT Interface is supported in GDB 7.0 and above.
// Currently on x64 and ia32 architectures and Linux OS are supported.
//

#ifdef ENABLE_GDB_JIT_INTERFACE
#include "src/v8.h"

#include "src/factory.h"

namespace v8 {
namespace internal {

class CompilationInfo;

class GDBJITInterface: public AllStatic {
 public:
  enum CodeTag { NON_FUNCTION, FUNCTION };

  // Main entry point into GDB JIT realized as a JitCodeEventHandler.
  static void EventHandler(const v8::JitCodeEvent* event);

  static void AddCode(Handle<Name> name,
                      Handle<Script> script,
                      Handle<Code> code,
                      CompilationInfo* info);

  static void RemoveCodeRange(Address start, Address end);

 private:
  static void AddCode(const char* name, Code* code, CodeTag tag, Script* script,
                      CompilationInfo* info);

  static void RemoveCode(Code* code);
};

#define GDBJIT(action) GDBJITInterface::action

} }   // namespace v8::internal
#else
#define GDBJIT(action) ((void) 0)
#endif

#endif
