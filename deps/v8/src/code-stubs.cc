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

#include "v8.h"

#include "bootstrapper.h"
#include "code-stubs.h"
#include "factory.h"
#include "macro-assembler.h"
#include "oprofile-agent.h"

namespace v8 {
namespace internal {

bool CodeStub::FindCodeInCache(Code** code_out) {
  if (has_custom_cache()) return GetCustomCache(code_out);
  int index = Heap::code_stubs()->FindEntry(GetKey());
  if (index != NumberDictionary::kNotFound) {
    *code_out = Code::cast(Heap::code_stubs()->ValueAt(index));
    return true;
  }
  return false;
}


void CodeStub::GenerateCode(MacroAssembler* masm) {
  // Update the static counter each time a new code stub is generated.
  Counters::code_stubs.Increment();
  // Nested stubs are not allowed for leafs.
  masm->set_allow_stub_calls(AllowsStubCalls());
  // Generate the code for the stub.
  masm->set_generating_stub(true);
  Generate(masm);
}


void CodeStub::RecordCodeGeneration(Code* code, MacroAssembler* masm) {
  code->set_major_key(MajorKey());

  OPROFILE(CreateNativeCodeRegion(GetName(),
                                  code->instruction_start(),
                                  code->instruction_size()));
  PROFILE(CodeCreateEvent(Logger::STUB_TAG, code, GetName()));
  Counters::total_stubs_code_size.Increment(code->instruction_size());

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
  Code* code;
  if (!FindCodeInCache(&code)) {
    v8::HandleScope scope;

    // Generate the new code.
    MacroAssembler masm(NULL, 256);
    GenerateCode(&masm);

    // Create the code object.
    CodeDesc desc;
    masm.GetCode(&desc);

    // Copy the generated code into a heap object.
    Code::Flags flags = Code::ComputeFlags(
        static_cast<Code::Kind>(GetCodeKind()),
        InLoop(),
        GetICState());
    Handle<Code> new_object = Factory::NewCode(desc, flags, masm.CodeObject());
    RecordCodeGeneration(*new_object, &masm);

    if (has_custom_cache()) {
      SetCustomCache(*new_object);
    } else {
      // Update the dictionary and the root in Heap.
      Handle<NumberDictionary> dict =
          Factory::DictionaryAtNumberPut(
              Handle<NumberDictionary>(Heap::code_stubs()),
              GetKey(),
              new_object);
      Heap::public_set_code_stubs(*dict);
    }
    code = *new_object;
  }

  return Handle<Code>(code);
}


MaybeObject* CodeStub::TryGetCode() {
  Code* code;
  if (!FindCodeInCache(&code)) {
    // Generate the new code.
    MacroAssembler masm(NULL, 256);
    GenerateCode(&masm);

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
          Heap::CreateCode(desc, flags, masm.CodeObject());
      if (!maybe_new_object->ToObject(&new_object)) return maybe_new_object;
    }
    code = Code::cast(new_object);
    RecordCodeGeneration(code, &masm);

    if (has_custom_cache()) {
      SetCustomCache(code);
    } else {
      // Try to update the code cache but do not fail if unable.
      MaybeObject* maybe_new_object =
          Heap::code_stubs()->AtNumberPut(GetKey(), code);
      if (maybe_new_object->ToObject(&new_object)) {
        Heap::public_set_code_stubs(NumberDictionary::cast(new_object));
      }
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


} }  // namespace v8::internal
