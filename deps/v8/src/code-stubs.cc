// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "v8.h"

#include "bootstrapper.h"
#include "code-stubs.h"
#include "cpu-profiler.h"
#include "stub-cache.h"
#include "factory.h"
#include "gdb-jit.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {


CodeStubInterfaceDescriptor::CodeStubInterfaceDescriptor()
    : register_param_count_(-1),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      register_params_(NULL),
      deoptimization_handler_(NULL),
      miss_handler_(),
      has_miss_handler_(false) { }


bool CodeStub::FindCodeInCache(Code** code_out, Isolate* isolate) {
  UnseededNumberDictionary* stubs = isolate->heap()->code_stubs();
  int index = stubs->FindEntry(GetKey());
  if (index != UnseededNumberDictionary::kNotFound) {
    *code_out = Code::cast(stubs->ValueAt(index));
    return true;
  }
  return false;
}


SmartArrayPointer<const char> CodeStub::GetName() {
  char buffer[100];
  NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
  StringStream stream(&allocator);
  PrintName(&stream);
  return stream.ToCString();
}


void CodeStub::RecordCodeGeneration(Code* code, Isolate* isolate) {
  SmartArrayPointer<const char> name = GetName();
  PROFILE(isolate, CodeCreateEvent(Logger::STUB_TAG, code, *name));
  GDBJIT(AddCode(GDBJITInterface::STUB, *name, code));
  Counters* counters = isolate->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());
}


Code::Kind CodeStub::GetCodeKind() const {
  return Code::STUB;
}


Handle<Code> CodeStub::GetCodeCopyFromTemplate(Isolate* isolate) {
  Handle<Code> ic = GetCode(isolate);
  ic = isolate->factory()->CopyCode(ic);
  RecordCodeGeneration(*ic, isolate);
  return ic;
}


Handle<Code> PlatformCodeStub::GenerateCode(Isolate* isolate) {
  Factory* factory = isolate->factory();

  // Generate the new code.
  MacroAssembler masm(isolate, NULL, 256);

  {
    // Update the static counter each time a new code stub is generated.
    isolate->counters()->code_stubs()->Increment();

    // Nested stubs are not allowed for leaves.
    AllowStubCallsScope allow_scope(&masm, false);

    // Generate the code for the stub.
    masm.set_generating_stub(true);
    NoCurrentFrameScope scope(&masm);
    Generate(&masm);
  }

  // Create the code object.
  CodeDesc desc;
  masm.GetCode(&desc);

  // Copy the generated code into a heap object.
  Code::Flags flags = Code::ComputeFlags(
      GetCodeKind(),
      GetICState(),
      GetExtraICState(),
      GetStubType(),
      GetStubFlags());
  Handle<Code> new_object = factory->NewCode(
      desc, flags, masm.CodeObject(), NeedsImmovableCode());
  return new_object;
}


void CodeStub::VerifyPlatformFeatures(Isolate* isolate) {
  ASSERT(CpuFeatures::VerifyCrossCompiling());
}


Handle<Code> CodeStub::GetCode(Isolate* isolate) {
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  Code* code;
  if (UseSpecialCache()
      ? FindCodeInSpecialCache(&code, isolate)
      : FindCodeInCache(&code, isolate)) {
    ASSERT(IsPregenerated(isolate) == code->is_pregenerated());
    ASSERT(GetCodeKind() == code->kind());
    return Handle<Code>(code);
  }

#ifdef DEBUG
  VerifyPlatformFeatures(isolate);
#endif

  {
    HandleScope scope(isolate);

    Handle<Code> new_object = GenerateCode(isolate);
    new_object->set_major_key(MajorKey());
    FinishCode(new_object);
    RecordCodeGeneration(*new_object, isolate);

#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_code_stubs) {
      new_object->Disassemble(*GetName());
      PrintF("\n");
    }
#endif

    if (UseSpecialCache()) {
      AddToSpecialCache(new_object);
    } else {
      // Update the dictionary and the root in Heap.
      Handle<UnseededNumberDictionary> dict =
          factory->DictionaryAtNumberPut(
              Handle<UnseededNumberDictionary>(heap->code_stubs()),
              GetKey(),
              new_object);
      heap->public_set_code_stubs(*dict);
    }
    code = *new_object;
  }

  Activate(code);
  ASSERT(!NeedsImmovableCode() ||
         heap->lo_space()->Contains(code) ||
         heap->code_space()->FirstPage()->Contains(code->address()));
  return Handle<Code>(code, isolate);
}


const char* CodeStub::MajorName(CodeStub::Major major_key,
                                bool allow_unknown_keys) {
  switch (major_key) {
#define DEF_CASE(name) case name: return #name "Stub";
    CODE_STUB_LIST(DEF_CASE)
#undef DEF_CASE
    default:
      if (!allow_unknown_keys) {
        UNREACHABLE();
      }
      return NULL;
  }
}


void CodeStub::PrintBaseName(StringStream* stream) {
  stream->Add("%s", MajorName(MajorKey(), false));
}


void CodeStub::PrintName(StringStream* stream) {
  PrintBaseName(stream);
  PrintState(stream);
}


void BinaryOpStub::PrintBaseName(StringStream* stream) {
  const char* op_name = Token::Name(op_);
  const char* ovr = "";
  if (mode_ == OVERWRITE_LEFT) ovr = "_ReuseLeft";
  if (mode_ == OVERWRITE_RIGHT) ovr = "_ReuseRight";
  stream->Add("BinaryOpStub_%s%s", op_name, ovr);
}


void BinaryOpStub::PrintState(StringStream* stream) {
  stream->Add("(");
  stream->Add(StateToName(left_state_));
  stream->Add("*");
  if (fixed_right_arg_.has_value) {
    stream->Add("%d", fixed_right_arg_.value);
  } else {
    stream->Add(StateToName(right_state_));
  }
  stream->Add("->");
  stream->Add(StateToName(result_state_));
  stream->Add(")");
}


Maybe<Handle<Object> > BinaryOpStub::Result(Handle<Object> left,
                                            Handle<Object> right,
                                            Isolate* isolate) {
  Handle<JSBuiltinsObject> builtins(isolate->js_builtins_object());
  Builtins::JavaScript func = BinaryOpIC::TokenToJSBuiltin(op_);
  Object* builtin = builtins->javascript_builtin(func);
  Handle<JSFunction> builtin_function =
      Handle<JSFunction>(JSFunction::cast(builtin), isolate);
  bool caught_exception;
  Handle<Object> result = Execution::Call(isolate, builtin_function, left,
      1, &right, &caught_exception);
  return Maybe<Handle<Object> >(!caught_exception, result);
}


void BinaryOpStub::Initialize() {
  fixed_right_arg_.has_value = false;
  left_state_ = right_state_ = result_state_ = NONE;
}


void BinaryOpStub::Generate(Token::Value op,
                            State left,
                            State right,
                            State result,
                            OverwriteMode mode,
                            Isolate* isolate) {
  BinaryOpStub stub(INITIALIZED);
  stub.op_ = op;
  stub.left_state_ = left;
  stub.right_state_ = right;
  stub.result_state_ = result;
  stub.mode_ = mode;
  stub.GetCode(isolate);
}


void BinaryOpStub::Generate(Token::Value op,
                            State left,
                            int right,
                            State result,
                            OverwriteMode mode,
                            Isolate* isolate) {
  BinaryOpStub stub(INITIALIZED);
  stub.op_ = op;
  stub.left_state_ = left;
  stub.fixed_right_arg_.has_value = true;
  stub.fixed_right_arg_.value = right;
  stub.right_state_ = SMI;
  stub.result_state_ = result;
  stub.mode_ = mode;
  stub.GetCode(isolate);
}


void BinaryOpStub::GenerateAheadOfTime(Isolate* isolate) {
  Token::Value binop[] = {Token::SUB, Token::MOD, Token::DIV, Token::MUL,
                          Token::ADD, Token::SAR, Token::BIT_OR, Token::BIT_AND,
                          Token::BIT_XOR, Token::SHL, Token::SHR};
  for (unsigned i = 0; i < ARRAY_SIZE(binop); i++) {
    BinaryOpStub stub(UNINITIALIZED);
    stub.op_ = binop[i];
    stub.GetCode(isolate);
  }

  // TODO(olivf) We should investigate why adding stubs to the snapshot is so
  // expensive at runtime. When solved we should be able to add most binops to
  // the snapshot instead of hand-picking them.
  // Generated list of commonly used stubs
  Generate(Token::ADD, INT32, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::ADD, INT32, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, INT32, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, INT32, INT32, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, INT32, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, INT32, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, INT32, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::ADD, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::ADD, INT32, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::ADD, NUMBER, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, NUMBER, INT32, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, NUMBER, INT32, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::ADD, NUMBER, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::ADD, NUMBER, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, NUMBER, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::ADD, SMI, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::ADD, SMI, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, SMI, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, SMI, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::ADD, SMI, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::ADD, SMI, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::ADD, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, INT32, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, INT32, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_AND, INT32, INT32, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, INT32, INT32, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, INT32, INT32, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, INT32, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, INT32, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_AND, INT32, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, NUMBER, INT32, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, NUMBER, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, NUMBER, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, SMI, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, SMI, INT32, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, SMI, NUMBER, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_AND, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_AND, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_AND, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, INT32, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, INT32, INT32, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, INT32, INT32, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_OR, INT32, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, INT32, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_OR, INT32, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, NUMBER, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_OR, NUMBER, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, NUMBER, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, NUMBER, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_OR, NUMBER, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, SMI, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, SMI, INT32, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, SMI, INT32, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_OR, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_OR, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_XOR, INT32, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, INT32, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_XOR, INT32, INT32, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_XOR, INT32, INT32, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, INT32, INT32, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_XOR, INT32, NUMBER, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, INT32, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_XOR, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::BIT_XOR, NUMBER, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, NUMBER, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, NUMBER, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, SMI, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, SMI, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_XOR, SMI, INT32, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_XOR, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::BIT_XOR, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::BIT_XOR, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::DIV, INT32, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::DIV, INT32, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, INT32, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, INT32, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::DIV, INT32, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, NUMBER, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, NUMBER, INT32, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, NUMBER, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::DIV, NUMBER, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, NUMBER, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, SMI, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::DIV, SMI, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, SMI, INT32, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, SMI, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, SMI, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::DIV, SMI, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::DIV, SMI, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, SMI, SMI, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::DIV, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::DIV, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::DIV, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::MOD, NUMBER, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MOD, SMI, 16, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::MOD, SMI, 2, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MOD, SMI, 2048, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MOD, SMI, 32, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MOD, SMI, 4, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MOD, SMI, 4, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::MOD, SMI, 8, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MOD, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MOD, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, INT32, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::MUL, INT32, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, INT32, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, INT32, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::MUL, INT32, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, INT32, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, NUMBER, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, NUMBER, INT32, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, NUMBER, INT32, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::MUL, NUMBER, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, NUMBER, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, NUMBER, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::MUL, SMI, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::MUL, SMI, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, SMI, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, SMI, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, SMI, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::MUL, SMI, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::MUL, SMI, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::MUL, SMI, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::MUL, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::MUL, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SAR, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::SAR, INT32, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SAR, INT32, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SAR, NUMBER, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SAR, NUMBER, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SAR, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::SAR, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHL, INT32, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::SHL, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHL, INT32, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SHL, INT32, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHL, NUMBER, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHL, SMI, SMI, INT32, NO_OVERWRITE, isolate);
  Generate(Token::SHL, SMI, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::SHL, SMI, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHL, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SHL, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::SHL, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHR, INT32, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SHR, INT32, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::SHR, INT32, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHR, NUMBER, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SHR, NUMBER, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::SHR, NUMBER, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::SHR, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SHR, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::SHR, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
  Generate(Token::SUB, INT32, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::SUB, INT32, INT32, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, INT32, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::SUB, INT32, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::SUB, INT32, SMI, INT32, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, INT32, SMI, INT32, OVERWRITE_RIGHT, isolate);
  Generate(Token::SUB, NUMBER, INT32, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::SUB, NUMBER, INT32, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, NUMBER, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::SUB, NUMBER, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, NUMBER, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::SUB, NUMBER, SMI, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::SUB, NUMBER, SMI, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, NUMBER, SMI, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::SUB, SMI, INT32, INT32, NO_OVERWRITE, isolate);
  Generate(Token::SUB, SMI, NUMBER, NUMBER, NO_OVERWRITE, isolate);
  Generate(Token::SUB, SMI, NUMBER, NUMBER, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, SMI, NUMBER, NUMBER, OVERWRITE_RIGHT, isolate);
  Generate(Token::SUB, SMI, SMI, SMI, NO_OVERWRITE, isolate);
  Generate(Token::SUB, SMI, SMI, SMI, OVERWRITE_LEFT, isolate);
  Generate(Token::SUB, SMI, SMI, SMI, OVERWRITE_RIGHT, isolate);
}


bool BinaryOpStub::can_encode_arg_value(int32_t value) const {
  return op_ == Token::MOD && value > 0 && IsPowerOf2(value) &&
         FixedRightArgValueBits::is_valid(WhichPowerOf2(value));
}


int BinaryOpStub::encode_arg_value(int32_t value) const {
  ASSERT(can_encode_arg_value(value));
  return WhichPowerOf2(value);
}


int32_t BinaryOpStub::decode_arg_value(int value)  const {
  return 1 << value;
}


int BinaryOpStub::encode_token(Token::Value op) const {
  ASSERT(op >= FIRST_TOKEN && op <= LAST_TOKEN);
  return op - FIRST_TOKEN;
}


Token::Value BinaryOpStub::decode_token(int op) const {
  int res = op + FIRST_TOKEN;
  ASSERT(res >= FIRST_TOKEN && res <= LAST_TOKEN);
  return static_cast<Token::Value>(res);
}


const char* BinaryOpStub::StateToName(State state) {
  switch (state) {
    case NONE:
      return "None";
    case SMI:
      return "Smi";
    case INT32:
      return "Int32";
    case NUMBER:
      return "Number";
    case STRING:
      return "String";
    case GENERIC:
      return "Generic";
  }
  return "";
}


void BinaryOpStub::UpdateStatus(Handle<Object> left,
                                Handle<Object> right,
                                Maybe<Handle<Object> > result) {
  int old_state = GetExtraICState();

  UpdateStatus(left, &left_state_);
  UpdateStatus(right, &right_state_);

  int32_t value;
  bool new_has_fixed_right_arg =
      right->ToInt32(&value) && can_encode_arg_value(value) &&
      (left_state_ == SMI || left_state_ == INT32) &&
      (result_state_ == NONE || !fixed_right_arg_.has_value);

  fixed_right_arg_ = Maybe<int32_t>(new_has_fixed_right_arg, value);

  if (result.has_value) UpdateStatus(result.value, &result_state_);

  State max_input = Max(left_state_, right_state_);

  if (!has_int_result() && op_ != Token::SHR &&
      max_input <= NUMBER && max_input > result_state_) {
    result_state_ = max_input;
  }

  ASSERT(result_state_ <= (has_int_result() ? INT32 : NUMBER) ||
         op_ == Token::ADD);

  if (old_state == GetExtraICState()) {
    // Tagged operations can lead to non-truncating HChanges
    if (left->IsUndefined() || left->IsBoolean()) {
      left_state_ = GENERIC;
    } else if (right->IsUndefined() || right->IsBoolean()) {
      right_state_ = GENERIC;
    } else {
      // Since the fpu is to precise, we might bail out on numbers which
      // actually would truncate with 64 bit precision.
      ASSERT(!CpuFeatures::IsSupported(SSE2) &&
             result_state_ <= INT32);
      result_state_ = NUMBER;
    }
  }
}


void BinaryOpStub::UpdateStatus(Handle<Object> object,
                                State* state) {
  bool is_truncating = (op_ == Token::BIT_AND || op_ == Token::BIT_OR ||
                        op_ == Token::BIT_XOR || op_ == Token::SAR ||
                        op_ == Token::SHL || op_ == Token::SHR);
  v8::internal::TypeInfo type = v8::internal::TypeInfo::FromValue(object);
  if (object->IsBoolean() && is_truncating) {
    // Booleans are converted by truncating by HChange.
    type = TypeInfo::Integer32();
  }
  if (object->IsUndefined()) {
    // Undefined will be automatically truncated for us by HChange.
    type = is_truncating ? TypeInfo::Integer32() : TypeInfo::Double();
  }
  State int_state = SmiValuesAre32Bits() ? NUMBER : INT32;
  State new_state = NONE;
  if (type.IsSmi()) {
    new_state = SMI;
  } else if (type.IsInteger32()) {
    new_state = int_state;
  } else if (type.IsNumber()) {
    new_state = NUMBER;
  } else if (object->IsString() && operation() == Token::ADD) {
    new_state = STRING;
  } else {
    new_state = GENERIC;
  }
  if ((new_state <= NUMBER && *state >  NUMBER) ||
      (new_state >  NUMBER && *state <= NUMBER && *state != NONE)) {
    new_state = GENERIC;
  }
  *state = Max(*state, new_state);
}


Handle<Type> BinaryOpStub::StateToType(State state,
                                       Isolate* isolate) {
  Handle<Type> t = handle(Type::None(), isolate);
  switch (state) {
    case NUMBER:
      t = handle(Type::Union(t, handle(Type::Double(), isolate)), isolate);
      // Fall through.
    case INT32:
      t = handle(Type::Union(t, handle(Type::Signed32(), isolate)), isolate);
      // Fall through.
    case SMI:
      t = handle(Type::Union(t, handle(Type::Smi(), isolate)), isolate);
      break;

    case STRING:
      t = handle(Type::Union(t, handle(Type::String(), isolate)), isolate);
      break;
    case GENERIC:
      return handle(Type::Any(), isolate);
      break;
    case NONE:
      break;
  }
  return t;
}


Handle<Type> BinaryOpStub::GetLeftType(Isolate* isolate) const {
  return StateToType(left_state_, isolate);
}


Handle<Type> BinaryOpStub::GetRightType(Isolate* isolate) const {
  return StateToType(right_state_, isolate);
}


Handle<Type> BinaryOpStub::GetResultType(Isolate* isolate) const {
  if (HasSideEffects(isolate)) return StateToType(NONE, isolate);
  if (result_state_ == GENERIC && op_ == Token::ADD) {
    return handle(Type::Union(handle(Type::Number(), isolate),
                              handle(Type::String(), isolate)), isolate);
  }
  ASSERT(result_state_ != GENERIC);
  if (result_state_ == NUMBER && op_ == Token::SHR) {
    return handle(Type::Unsigned32(), isolate);
  }
  return StateToType(result_state_, isolate);
}


InlineCacheState ICCompareStub::GetICState() {
  CompareIC::State state = Max(left_, right_);
  switch (state) {
    case CompareIC::UNINITIALIZED:
      return ::v8::internal::UNINITIALIZED;
    case CompareIC::SMI:
    case CompareIC::NUMBER:
    case CompareIC::INTERNALIZED_STRING:
    case CompareIC::STRING:
    case CompareIC::UNIQUE_NAME:
    case CompareIC::OBJECT:
    case CompareIC::KNOWN_OBJECT:
      return MONOMORPHIC;
    case CompareIC::GENERIC:
      return ::v8::internal::GENERIC;
  }
  UNREACHABLE();
  return ::v8::internal::UNINITIALIZED;
}


void ICCompareStub::AddToSpecialCache(Handle<Code> new_object) {
  ASSERT(*known_map_ != NULL);
  Isolate* isolate = new_object->GetIsolate();
  Factory* factory = isolate->factory();
  return Map::UpdateCodeCache(known_map_,
                              strict() ?
                                  factory->strict_compare_ic_string() :
                                  factory->compare_ic_string(),
                              new_object);
}


bool ICCompareStub::FindCodeInSpecialCache(Code** code_out, Isolate* isolate) {
  Factory* factory = isolate->factory();
  Code::Flags flags = Code::ComputeFlags(
      GetCodeKind(),
      UNINITIALIZED);
  ASSERT(op_ == Token::EQ || op_ == Token::EQ_STRICT);
  Handle<Object> probe(
      known_map_->FindInCodeCache(
        strict() ?
            *factory->strict_compare_ic_string() :
            *factory->compare_ic_string(),
        flags),
      isolate);
  if (probe->IsCode()) {
    *code_out = Code::cast(*probe);
#ifdef DEBUG
    Token::Value cached_op;
    ICCompareStub::DecodeMinorKey((*code_out)->stub_info(), NULL, NULL, NULL,
                                  &cached_op);
    ASSERT(op_ == cached_op);
#endif
    return true;
  }
  return false;
}


int ICCompareStub::MinorKey() {
  return OpField::encode(op_ - Token::EQ) |
         LeftStateField::encode(left_) |
         RightStateField::encode(right_) |
         HandlerStateField::encode(state_);
}


void ICCompareStub::DecodeMinorKey(int minor_key,
                                   CompareIC::State* left_state,
                                   CompareIC::State* right_state,
                                   CompareIC::State* handler_state,
                                   Token::Value* op) {
  if (left_state) {
    *left_state =
        static_cast<CompareIC::State>(LeftStateField::decode(minor_key));
  }
  if (right_state) {
    *right_state =
        static_cast<CompareIC::State>(RightStateField::decode(minor_key));
  }
  if (handler_state) {
    *handler_state =
        static_cast<CompareIC::State>(HandlerStateField::decode(minor_key));
  }
  if (op) {
    *op = static_cast<Token::Value>(OpField::decode(minor_key) + Token::EQ);
  }
}


void ICCompareStub::Generate(MacroAssembler* masm) {
  switch (state_) {
    case CompareIC::UNINITIALIZED:
      GenerateMiss(masm);
      break;
    case CompareIC::SMI:
      GenerateSmis(masm);
      break;
    case CompareIC::NUMBER:
      GenerateNumbers(masm);
      break;
    case CompareIC::STRING:
      GenerateStrings(masm);
      break;
    case CompareIC::INTERNALIZED_STRING:
      GenerateInternalizedStrings(masm);
      break;
    case CompareIC::UNIQUE_NAME:
      GenerateUniqueNames(masm);
      break;
    case CompareIC::OBJECT:
      GenerateObjects(masm);
      break;
    case CompareIC::KNOWN_OBJECT:
      ASSERT(*known_map_ != NULL);
      GenerateKnownObjects(masm);
      break;
    case CompareIC::GENERIC:
      GenerateGeneric(masm);
      break;
  }
}


void CompareNilICStub::UpdateStatus(Handle<Object> object) {
  ASSERT(!state_.Contains(GENERIC));
  State old_state(state_);
  if (object->IsNull()) {
    state_.Add(NULL_TYPE);
  } else if (object->IsUndefined()) {
    state_.Add(UNDEFINED);
  } else if (object->IsUndetectableObject() ||
             object->IsOddball() ||
             !object->IsHeapObject()) {
    state_.RemoveAll();
    state_.Add(GENERIC);
  } else if (IsMonomorphic()) {
    state_.RemoveAll();
    state_.Add(GENERIC);
  } else {
    state_.Add(MONOMORPHIC_MAP);
  }
  TraceTransition(old_state, state_);
}


template<class StateType>
void HydrogenCodeStub::TraceTransition(StateType from, StateType to) {
  // Note: Although a no-op transition is semantically OK, it is hinting at a
  // bug somewhere in our state transition machinery.
  ASSERT(from != to);
  #ifdef DEBUG
  if (!FLAG_trace_ic) return;
  char buffer[100];
  NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
  StringStream stream(&allocator);
  stream.Add("[");
  PrintBaseName(&stream);
  stream.Add(": ");
  from.Print(&stream);
  stream.Add("=>");
  to.Print(&stream);
  stream.Add("]\n");
  stream.OutputToStdOut();
  #endif
}


void CompareNilICStub::PrintBaseName(StringStream* stream) {
  CodeStub::PrintBaseName(stream);
  stream->Add((nil_value_ == kNullValue) ? "(NullValue)":
                                           "(UndefinedValue)");
}


void CompareNilICStub::PrintState(StringStream* stream) {
  state_.Print(stream);
}


void CompareNilICStub::State::Print(StringStream* stream) const {
  stream->Add("(");
  SimpleListPrinter printer(stream);
  if (IsEmpty()) printer.Add("None");
  if (Contains(UNDEFINED)) printer.Add("Undefined");
  if (Contains(NULL_TYPE)) printer.Add("Null");
  if (Contains(MONOMORPHIC_MAP)) printer.Add("MonomorphicMap");
  if (Contains(GENERIC)) printer.Add("Generic");
  stream->Add(")");
}


Handle<Type> CompareNilICStub::GetType(
    Isolate* isolate,
    Handle<Map> map) {
  if (state_.Contains(CompareNilICStub::GENERIC)) {
    return handle(Type::Any(), isolate);
  }

  Handle<Type> result(Type::None(), isolate);
  if (state_.Contains(CompareNilICStub::UNDEFINED)) {
    result = handle(Type::Union(result, handle(Type::Undefined(), isolate)),
                    isolate);
  }
  if (state_.Contains(CompareNilICStub::NULL_TYPE)) {
    result = handle(Type::Union(result, handle(Type::Null(), isolate)),
                    isolate);
  }
  if (state_.Contains(CompareNilICStub::MONOMORPHIC_MAP)) {
    Type* type = map.is_null() ? Type::Detectable() : Type::Class(map);
    result = handle(Type::Union(result, handle(type, isolate)), isolate);
  }

  return result;
}


Handle<Type> CompareNilICStub::GetInputType(
    Isolate* isolate,
    Handle<Map> map) {
  Handle<Type> output_type = GetType(isolate, map);
  Handle<Type> nil_type = handle(nil_value_ == kNullValue
      ? Type::Null() : Type::Undefined(), isolate);
  return handle(Type::Union(output_type, nil_type), isolate);
}


void InstanceofStub::PrintName(StringStream* stream) {
  const char* args = "";
  if (HasArgsInRegisters()) {
    args = "_REGS";
  }

  const char* inline_check = "";
  if (HasCallSiteInlineCheck()) {
    inline_check = "_INLINE";
  }

  const char* return_true_false_object = "";
  if (ReturnTrueFalseObject()) {
    return_true_false_object = "_TRUEFALSE";
  }

  stream->Add("InstanceofStub%s%s%s",
              args,
              inline_check,
              return_true_false_object);
}


void JSEntryStub::FinishCode(Handle<Code> code) {
  Handle<FixedArray> handler_table =
      code->GetIsolate()->factory()->NewFixedArray(1, TENURED);
  handler_table->set(0, Smi::FromInt(handler_offset_));
  code->set_handler_table(*handler_table);
}


void KeyedLoadDictionaryElementStub::Generate(MacroAssembler* masm) {
  KeyedLoadStubCompiler::GenerateLoadDictionaryElement(masm);
}


void CreateAllocationSiteStub::GenerateAheadOfTime(Isolate* isolate) {
  CreateAllocationSiteStub stub;
  stub.GetCode(isolate)->set_is_pregenerated(true);
}


void KeyedStoreElementStub::Generate(MacroAssembler* masm) {
  switch (elements_kind_) {
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      UNREACHABLE();
      break;
    case DICTIONARY_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreDictionaryElement(masm);
      break;
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }
}


void ArgumentsAccessStub::PrintName(StringStream* stream) {
  stream->Add("ArgumentsAccessStub_");
  switch (type_) {
    case READ_ELEMENT: stream->Add("ReadElement"); break;
    case NEW_NON_STRICT_FAST: stream->Add("NewNonStrictFast"); break;
    case NEW_NON_STRICT_SLOW: stream->Add("NewNonStrictSlow"); break;
    case NEW_STRICT: stream->Add("NewStrict"); break;
  }
}


void CallFunctionStub::PrintName(StringStream* stream) {
  stream->Add("CallFunctionStub_Args%d", argc_);
  if (ReceiverMightBeImplicit()) stream->Add("_Implicit");
  if (RecordCallTarget()) stream->Add("_Recording");
}


void CallConstructStub::PrintName(StringStream* stream) {
  stream->Add("CallConstructStub");
  if (RecordCallTarget()) stream->Add("_Recording");
}


bool ToBooleanStub::UpdateStatus(Handle<Object> object) {
  Types old_types(types_);
  bool to_boolean_value = types_.UpdateStatus(object);
  TraceTransition(old_types, types_);
  return to_boolean_value;
}


void ToBooleanStub::PrintState(StringStream* stream) {
  types_.Print(stream);
}


void ToBooleanStub::Types::Print(StringStream* stream) const {
  stream->Add("(");
  SimpleListPrinter printer(stream);
  if (IsEmpty()) printer.Add("None");
  if (Contains(UNDEFINED)) printer.Add("Undefined");
  if (Contains(BOOLEAN)) printer.Add("Bool");
  if (Contains(NULL_TYPE)) printer.Add("Null");
  if (Contains(SMI)) printer.Add("Smi");
  if (Contains(SPEC_OBJECT)) printer.Add("SpecObject");
  if (Contains(STRING)) printer.Add("String");
  if (Contains(SYMBOL)) printer.Add("Symbol");
  if (Contains(HEAP_NUMBER)) printer.Add("HeapNumber");
  stream->Add(")");
}


bool ToBooleanStub::Types::UpdateStatus(Handle<Object> object) {
  if (object->IsUndefined()) {
    Add(UNDEFINED);
    return false;
  } else if (object->IsBoolean()) {
    Add(BOOLEAN);
    return object->IsTrue();
  } else if (object->IsNull()) {
    Add(NULL_TYPE);
    return false;
  } else if (object->IsSmi()) {
    Add(SMI);
    return Smi::cast(*object)->value() != 0;
  } else if (object->IsSpecObject()) {
    Add(SPEC_OBJECT);
    return !object->IsUndetectableObject();
  } else if (object->IsString()) {
    Add(STRING);
    return !object->IsUndetectableObject() &&
        String::cast(*object)->length() != 0;
  } else if (object->IsSymbol()) {
    Add(SYMBOL);
    return true;
  } else if (object->IsHeapNumber()) {
    ASSERT(!object->IsUndetectableObject());
    Add(HEAP_NUMBER);
    double value = HeapNumber::cast(*object)->value();
    return value != 0 && !std::isnan(value);
  } else {
    // We should never see an internal object at runtime here!
    UNREACHABLE();
    return true;
  }
}


bool ToBooleanStub::Types::NeedsMap() const {
  return Contains(ToBooleanStub::SPEC_OBJECT)
      || Contains(ToBooleanStub::STRING)
      || Contains(ToBooleanStub::SYMBOL)
      || Contains(ToBooleanStub::HEAP_NUMBER);
}


bool ToBooleanStub::Types::CanBeUndetectable() const {
  return Contains(ToBooleanStub::SPEC_OBJECT)
      || Contains(ToBooleanStub::STRING);
}


void StubFailureTrampolineStub::GenerateAheadOfTime(Isolate* isolate) {
  StubFailureTrampolineStub stub1(NOT_JS_FUNCTION_STUB_MODE);
  StubFailureTrampolineStub stub2(JS_FUNCTION_STUB_MODE);
  stub1.GetCode(isolate)->set_is_pregenerated(true);
  stub2.GetCode(isolate)->set_is_pregenerated(true);
}


void ProfileEntryHookStub::EntryHookTrampoline(intptr_t function,
                                               intptr_t stack_pointer,
                                               Isolate* isolate) {
  FunctionEntryHook entry_hook = isolate->function_entry_hook();
  ASSERT(entry_hook != NULL);
  entry_hook(function, stack_pointer);
}


static void InstallDescriptor(Isolate* isolate, HydrogenCodeStub* stub) {
  int major_key = stub->MajorKey();
  CodeStubInterfaceDescriptor* descriptor =
      isolate->code_stub_interface_descriptor(major_key);
  if (!descriptor->initialized()) {
    stub->InitializeInterfaceDescriptor(isolate, descriptor);
  }
}


void ArrayConstructorStubBase::InstallDescriptors(Isolate* isolate) {
  ArrayNoArgumentConstructorStub stub1(GetInitialFastElementsKind());
  InstallDescriptor(isolate, &stub1);
  ArraySingleArgumentConstructorStub stub2(GetInitialFastElementsKind());
  InstallDescriptor(isolate, &stub2);
  ArrayNArgumentsConstructorStub stub3(GetInitialFastElementsKind());
  InstallDescriptor(isolate, &stub3);
}


void NumberToStringStub::InstallDescriptors(Isolate* isolate) {
  NumberToStringStub stub;
  InstallDescriptor(isolate, &stub);
}


void FastNewClosureStub::InstallDescriptors(Isolate* isolate) {
  FastNewClosureStub stub(STRICT_MODE, false);
  InstallDescriptor(isolate, &stub);
}


ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate)
    : argument_count_(ANY) {
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate,
                                           int argument_count) {
  if (argument_count == 0) {
    argument_count_ = NONE;
  } else if (argument_count == 1) {
    argument_count_ = ONE;
  } else if (argument_count >= 2) {
    argument_count_ = MORE_THAN_ONE;
  } else {
    UNREACHABLE();
  }
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


void InternalArrayConstructorStubBase::InstallDescriptors(Isolate* isolate) {
  InternalArrayNoArgumentConstructorStub stub1(FAST_ELEMENTS);
  InstallDescriptor(isolate, &stub1);
  InternalArraySingleArgumentConstructorStub stub2(FAST_ELEMENTS);
  InstallDescriptor(isolate, &stub2);
  InternalArrayNArgumentsConstructorStub stub3(FAST_ELEMENTS);
  InstallDescriptor(isolate, &stub3);
}

InternalArrayConstructorStub::InternalArrayConstructorStub(
    Isolate* isolate) {
  InternalArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


} }  // namespace v8::internal
