// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

  static void AddCode(Handle<Name> name,
                      Handle<Script> script,
                      Handle<Code> code,
                      CompilationInfo* info);

  static void AddCode(CodeTag tag, Name* name, Code* code);

  static void AddCode(CodeTag tag, const char* name, Code* code);

  static void AddCode(CodeTag tag, Code* code);

  static void RemoveCode(Code* code);

  static void RemoveCodeRange(Address start, Address end);

  static void RegisterDetailedLineInfo(Code* code, GDBJITLineInfo* line_info);
};

#define GDBJIT(action) GDBJITInterface::action

} }   // namespace v8::internal
#else
#define GDBJIT(action) ((void) 0)
#endif

#endif
