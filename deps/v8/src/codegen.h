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

#ifndef V8_CODEGEN_H_
#define V8_CODEGEN_H_

#include "ast.h"
#include "code-stubs.h"
#include "runtime.h"

// Include the declaration of the architecture defined class CodeGenerator.
// The contract  to the shared code is that the the CodeGenerator is a subclass
// of Visitor and that the following methods are available publicly:
//   MakeCode
//   SetFunctionInfo
//   masm
//   frame
//   has_valid_frame
//   SetFrame
//   DeleteFrame
//   allocator
//   AddDeferred
//   in_spilled_code
//   set_in_spilled_code
//
// These methods are either used privately by the shared code or implemented as
// shared code:
//   CodeGenerator
//   ~CodeGenerator
//   ProcessDeferred
//   GenCode
//   BuildBoilerplate
//   ComputeCallInitialize
//   ComputeCallInitializeInLoop
//   ProcessDeclarations
//   DeclareGlobals
//   FindInlineRuntimeLUT
//   CheckForInlineRuntimeCall
//   PatchInlineRuntimeEntry
//   GenerateFastCaseSwitchStatement
//   GenerateFastCaseSwitchCases
//   TryGenerateFastCaseSwitchStatement
//   GenerateFastCaseSwitchJumpTable
//   FastCaseSwitchMinCaseCount
//   FastCaseSwitchMaxOverheadFactor
//   CodeForFunctionPosition
//   CodeForReturnPosition
//   CodeForStatementPosition
//   CodeForSourcePosition


// Mode to overwrite BinaryExpression values.
enum OverwriteMode { NO_OVERWRITE, OVERWRITE_LEFT, OVERWRITE_RIGHT };


#if V8_TARGET_ARCH_IA32
#include "ia32/codegen-ia32.h"
#elif V8_TARGET_ARCH_X64
#include "x64/codegen-x64.h"
#elif V8_TARGET_ARCH_ARM
#include "arm/codegen-arm.h"
#endif

#include "register-allocator.h"

namespace v8 {
namespace internal {


// Code generation can be nested.  Code generation scopes form a stack
// of active code generators.
class CodeGeneratorScope BASE_EMBEDDED {
 public:
  explicit CodeGeneratorScope(CodeGenerator* cgen) {
    previous_ = top_;
    top_ = cgen;
  }

  ~CodeGeneratorScope() {
    top_ = previous_;
  }

  static CodeGenerator* Current() {
    ASSERT(top_ != NULL);
    return top_;
  }

 private:
  static CodeGenerator* top_;
  CodeGenerator* previous_;
};


// Deferred code objects are small pieces of code that are compiled
// out of line. They are used to defer the compilation of uncommon
// paths thereby avoiding expensive jumps around uncommon code parts.
class DeferredCode: public ZoneObject {
 public:
  DeferredCode();
  virtual ~DeferredCode() { }

  virtual void Generate() = 0;

  MacroAssembler* masm() { return masm_; }

  int statement_position() const { return statement_position_; }
  int position() const { return position_; }

  Label* entry_label() { return &entry_label_; }
  Label* exit_label() { return &exit_label_; }

#ifdef DEBUG
  void set_comment(const char* comment) { comment_ = comment; }
  const char* comment() const { return comment_; }
#else
  void set_comment(const char* comment) { }
  const char* comment() const { return ""; }
#endif

  inline void Jump();
  inline void Branch(Condition cc);
  void BindExit() { masm_->bind(&exit_label_); }

  void SaveRegisters();
  void RestoreRegisters();

 protected:
  MacroAssembler* masm_;

 private:
  // Constants indicating special actions.  They should not be multiples
  // of kPointerSize so they will not collide with valid offsets from
  // the frame pointer.
  static const int kIgnore = -1;
  static const int kPush = 1;

  // This flag is ored with a valid offset from the frame pointer, so
  // it should fit in the low zero bits of a valid offset.
  static const int kSyncedFlag = 2;

  int statement_position_;
  int position_;

  Label entry_label_;
  Label exit_label_;

  int registers_[RegisterAllocator::kNumRegisters];

#ifdef DEBUG
  const char* comment_;
#endif
  DISALLOW_COPY_AND_ASSIGN(DeferredCode);
};


// RuntimeStub models code stubs calling entry points in the Runtime class.
class RuntimeStub : public CodeStub {
 public:
  explicit RuntimeStub(Runtime::FunctionId id, int num_arguments)
      : id_(id), num_arguments_(num_arguments) { }

  void Generate(MacroAssembler* masm);

  // Disassembler support.  It is useful to be able to print the name
  // of the runtime function called through this stub.
  static const char* GetNameFromMinorKey(int minor_key) {
    return Runtime::FunctionForId(IdField::decode(minor_key))->stub_name;
  }

 private:
  Runtime::FunctionId id_;
  int num_arguments_;

  class ArgumentField: public BitField<int,  0, 16> {};
  class IdField: public BitField<Runtime::FunctionId, 16, kMinorBits - 16> {};

  Major MajorKey() { return Runtime; }
  int MinorKey() {
    return IdField::encode(id_) | ArgumentField::encode(num_arguments_);
  }

  const char* GetName();

#ifdef DEBUG
  void Print() {
    PrintF("RuntimeStub (id %s)\n", Runtime::FunctionForId(id_)->name);
  }
#endif
};


class StackCheckStub : public CodeStub {
 public:
  StackCheckStub() { }

  void Generate(MacroAssembler* masm);

 private:

  const char* GetName() { return "StackCheckStub"; }

  Major MajorKey() { return StackCheck; }
  int MinorKey() { return 0; }
};


class InstanceofStub: public CodeStub {
 public:
  InstanceofStub() { }

  void Generate(MacroAssembler* masm);

 private:
  Major MajorKey() { return Instanceof; }
  int MinorKey() { return 0; }
};


class UnarySubStub : public CodeStub {
 public:
  explicit UnarySubStub(bool overwrite)
      : overwrite_(overwrite) { }

 private:
  bool overwrite_;
  Major MajorKey() { return UnarySub; }
  int MinorKey() { return overwrite_ ? 1 : 0; }
  void Generate(MacroAssembler* masm);

  const char* GetName() { return "UnarySubStub"; }
};


class CEntryStub : public CodeStub {
 public:
  CEntryStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, false); }

 protected:
  void GenerateBody(MacroAssembler* masm, bool is_debug_break);
  void GenerateCore(MacroAssembler* masm,
                    Label* throw_normal_exception,
                    Label* throw_out_of_memory_exception,
                    StackFrame::Type frame_type,
                    bool do_gc,
                    bool always_allocate_scope);
  void GenerateThrowTOS(MacroAssembler* masm);
  void GenerateThrowOutOfMemory(MacroAssembler* masm);

 private:
  Major MajorKey() { return CEntry; }
  int MinorKey() { return 0; }

  const char* GetName() { return "CEntryStub"; }
};


class CEntryDebugBreakStub : public CEntryStub {
 public:
  CEntryDebugBreakStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, true); }

 private:
  int MinorKey() { return 1; }

  const char* GetName() { return "CEntryDebugBreakStub"; }
};


class JSEntryStub : public CodeStub {
 public:
  JSEntryStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, false); }

 protected:
  void GenerateBody(MacroAssembler* masm, bool is_construct);

 private:
  Major MajorKey() { return JSEntry; }
  int MinorKey() { return 0; }

  const char* GetName() { return "JSEntryStub"; }
};


class JSConstructEntryStub : public JSEntryStub {
 public:
  JSConstructEntryStub() { }

  void Generate(MacroAssembler* masm) { GenerateBody(masm, true); }

 private:
  int MinorKey() { return 1; }

  const char* GetName() { return "JSConstructEntryStub"; }
};


class ArgumentsAccessStub: public CodeStub {
 public:
  enum Type {
    READ_LENGTH,
    READ_ELEMENT,
    NEW_OBJECT
  };

  explicit ArgumentsAccessStub(Type type) : type_(type) { }

 private:
  Type type_;

  Major MajorKey() { return ArgumentsAccess; }
  int MinorKey() { return type_; }

  void Generate(MacroAssembler* masm);
  void GenerateReadLength(MacroAssembler* masm);
  void GenerateReadElement(MacroAssembler* masm);
  void GenerateNewObject(MacroAssembler* masm);

  const char* GetName() { return "ArgumentsAccessStub"; }

#ifdef DEBUG
  void Print() {
    PrintF("ArgumentsAccessStub (type %d)\n", type_);
  }
#endif
};


}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_H_
