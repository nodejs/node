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

#include "v8.h"

#include "bootstrapper.h"
#include "code-stubs.h"
#include "stub-cache.h"
#include "factory.h"
#include "gdb-jit.h"
#include "macro-assembler.h"

namespace v8 {
namespace internal {

bool CodeStub::FindCodeInCache(Code** code_out) {
  Heap* heap = Isolate::Current()->heap();
  int index = heap->code_stubs()->FindEntry(GetKey());
  if (index != NumberDictionary::kNotFound) {
    *code_out = Code::cast(heap->code_stubs()->ValueAt(index));
    return true;
  }
  return false;
}


void CodeStub::GenerateCode(MacroAssembler* masm) {
  // Update the static counter each time a new code stub is generated.
  masm->isolate()->counters()->code_stubs()->Increment();

  // Nested stubs are not allowed for leafs.
  AllowStubCallsScope allow_scope(masm, AllowsStubCalls());

  // Generate the code for the stub.
  masm->set_generating_stub(true);
  Generate(masm);
}


void CodeStub::RecordCodeGeneration(Code* code, MacroAssembler* masm) {
  code->set_major_key(MajorKey());

  Isolate* isolate = masm->isolate();
  PROFILE(isolate, CodeCreateEvent(Logger::STUB_TAG, code, GetName()));
  GDBJIT(AddCode(GDBJITInterface::STUB, GetName(), code));
  Counters* counters = isolate->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs) {
#ifdef DEBUG
    Print();
#endif
    code->Disassemble(GetName());
    PrintF("\n");
  }
#endif
}


int CodeStub::GetCodeKind() {
  return Code::STUB;
}


Handle<Code> CodeStub::GetCode() {
  Isolate* isolate = Isolate::Current();
  Factory* factory = isolate->factory();
  Heap* heap = isolate->heap();
  Code* code;
  if (!FindCodeInCache(&code)) {
    HandleScope scope(isolate);

    // Generate the new code.
    MacroAssembler masm(isolate, NULL, 256);
    GenerateCode(&masm);

    // Create the code object.
    CodeDesc desc;
    masm.GetCode(&desc);

    // Copy the generated code into a heap object.
    Code::Flags flags = Code::ComputeFlags(
        static_cast<Code::Kind>(GetCodeKind()),
        InLoop(),
        GetICState());
    Handle<Code> new_object = factory->NewCode(
        desc, flags, masm.CodeObject(), NeedsImmovableCode());
    RecordCodeGeneration(*new_object, &masm);
    FinishCode(*new_object);

    // Update the dictionary and the root in Heap.
    Handle<NumberDictionary> dict =
        factory->DictionaryAtNumberPut(
            Handle<NumberDictionary>(heap->code_stubs()),
            GetKey(),
            new_object);
    heap->public_set_code_stubs(*dict);

    code = *new_object;
  }

  ASSERT(!NeedsImmovableCode() || heap->lo_space()->Contains(code));
  return Handle<Code>(code, isolate);
}


MaybeObject* CodeStub::TryGetCode() {
  Code* code;
  if (!FindCodeInCache(&code)) {
    // Generate the new code.
    MacroAssembler masm(Isolate::Current(), NULL, 256);
    GenerateCode(&masm);
    Heap* heap = masm.isolate()->heap();

    // Create the code object.
    CodeDesc desc;
    masm.GetCode(&desc);

    // Try to copy the generated code into a heap object.
    Code::Flags flags = Code::ComputeFlags(
        static_cast<Code::Kind>(GetCodeKind()),
        InLoop(),
        GetICState());
    Object* new_object;
    { MaybeObject* maybe_new_object =
          heap->CreateCode(desc, flags, masm.CodeObject());
      if (!maybe_new_object->ToObject(&new_object)) return maybe_new_object;
    }
    code = Code::cast(new_object);
    RecordCodeGeneration(code, &masm);
    FinishCode(code);

    // Try to update the code cache but do not fail if unable.
    MaybeObject* maybe_new_object =
        heap->code_stubs()->AtNumberPut(GetKey(), code);
    if (maybe_new_object->ToObject(&new_object)) {
      heap->public_set_code_stubs(NumberDictionary::cast(new_object));
    }
  }

  return code;
}


const char* CodeStub::MajorName(CodeStub::Major major_key,
                                bool allow_unknown_keys) {
  switch (major_key) {
#define DEF_CASE(name) case name: return #name;
    CODE_STUB_LIST(DEF_CASE)
#undef DEF_CASE
    default:
      if (!allow_unknown_keys) {
        UNREACHABLE();
      }
      return NULL;
  }
}


int ICCompareStub::MinorKey() {
  return OpField::encode(op_ - Token::EQ) | StateField::encode(state_);
}


void ICCompareStub::Generate(MacroAssembler* masm) {
  switch (state_) {
    case CompareIC::UNINITIALIZED:
      GenerateMiss(masm);
      break;
    case CompareIC::SMIS:
      GenerateSmis(masm);
      break;
    case CompareIC::HEAP_NUMBERS:
      GenerateHeapNumbers(masm);
      break;
    case CompareIC::STRINGS:
      GenerateStrings(masm);
      break;
    case CompareIC::SYMBOLS:
      GenerateSymbols(masm);
      break;
    case CompareIC::OBJECTS:
      GenerateObjects(masm);
      break;
    default:
      UNREACHABLE();
  }
}


const char* InstanceofStub::GetName() {
  if (name_ != NULL) return name_;
  const int kMaxNameLength = 100;
  name_ = Isolate::Current()->bootstrapper()->AllocateAutoDeletedArray(
      kMaxNameLength);
  if (name_ == NULL) return "OOM";

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

  OS::SNPrintF(Vector<char>(name_, kMaxNameLength),
               "InstanceofStub%s%s%s",
               args,
               inline_check,
               return_true_false_object);
  return name_;
}


void KeyedLoadFastElementStub::Generate(MacroAssembler* masm) {
  KeyedLoadStubCompiler::GenerateLoadFastElement(masm);
}


void KeyedStoreFastElementStub::Generate(MacroAssembler* masm) {
  KeyedStoreStubCompiler::GenerateStoreFastElement(masm, is_js_array_);
}


void KeyedLoadExternalArrayStub::Generate(MacroAssembler* masm) {
  KeyedLoadStubCompiler::GenerateLoadExternalArray(masm, elements_kind_);
}


void KeyedStoreExternalArrayStub::Generate(MacroAssembler* masm) {
  KeyedStoreStubCompiler::GenerateStoreExternalArray(masm, elements_kind_);
}


} }  // namespace v8::internal
