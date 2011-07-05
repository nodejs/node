// Copyright 2010 the V8 project authors. All rights reserved.
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

#ifndef V8_GDB_JIT_H_
#define V8_GDB_JIT_H_

#include "allocation.h"

//
// Basic implementation of GDB JIT Interface client.
// GBD JIT Interface is supported in GDB 7.0 and above.
// Currently on x64 and ia32 architectures and Linux OS are supported.
//

#ifdef ENABLE_GDB_JIT_INTERFACE
#include "v8.h"
#include "factory.h"

namespace v8 {
namespace internal {

class CompilationInfo;

#define CODE_TAGS_LIST(V)                       \
  V(LOAD_IC)                                    \
  V(KEYED_LOAD_IC)                              \
  V(STORE_IC)                                   \
  V(KEYED_STORE_IC)                             \
  V(CALL_IC)                                    \
  V(CALL_INITIALIZE)                            \
  V(CALL_PRE_MONOMORPHIC)                       \
  V(CALL_NORMAL)                                \
  V(CALL_MEGAMORPHIC)                           \
  V(CALL_MISS)                                  \
  V(STUB)                                       \
  V(BUILTIN)                                    \
  V(SCRIPT)                                     \
  V(EVAL)                                       \
  V(FUNCTION)

class GDBJITLineInfo : public Malloced {
 public:
  GDBJITLineInfo()
      : pc_info_(10) { }

  void SetPosition(intptr_t pc, int pos, bool is_statement) {
    AddPCInfo(PCInfo(pc, pos, is_statement));
  }

  struct PCInfo {
    PCInfo(intptr_t pc, int pos, bool is_statement)
        : pc_(pc), pos_(pos), is_statement_(is_statement) { }

    intptr_t pc_;
    int pos_;
    bool is_statement_;
  };

  List<PCInfo>* pc_info() {
    return &pc_info_;
  }

 private:
  void AddPCInfo(const PCInfo& pc_info) {
    pc_info_.Add(pc_info);
  }

  List<PCInfo> pc_info_;
};


class GDBJITInterface: public AllStatic {
 public:
  enum CodeTag {
#define V(x) x,
    CODE_TAGS_LIST(V)
#undef V
    TAG_COUNT
  };

  static const char* Tag2String(CodeTag tag) {
    switch (tag) {
#define V(x) case x: return #x;
      CODE_TAGS_LIST(V)
#undef V
      default:
        return NULL;
    }
  }

  static void AddCode(const char* name,
                      Code* code,
                      CodeTag tag,
                      Script* script,
                      CompilationInfo* info);

  static void AddCode(Handle<String> name,
                      Handle<Script> script,
                      Handle<Code> code,
                      CompilationInfo* info);

  static void AddCode(CodeTag tag, String* name, Code* code);

  static void AddCode(CodeTag tag, const char* name, Code* code);

  static void AddCode(CodeTag tag, Code* code);

  static void RemoveCode(Code* code);

  static void RegisterDetailedLineInfo(Code* code, GDBJITLineInfo* line_info);

 private:
  static Mutex* mutex_;
};

#define GDBJIT(action) GDBJITInterface::action

} }   // namespace v8::internal
#else
#define GDBJIT(action) ((void) 0)
#endif

#endif
