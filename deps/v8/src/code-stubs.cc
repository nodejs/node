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

namespace v8 {
namespace internal {

Handle<Code> CodeStub::GetCode() {
  bool custom_cache = has_custom_cache();

  int index = 0;
  uint32_t key = 0;
  if (custom_cache) {
    Code* cached;
    if (GetCustomCache(&cached)) {
      return Handle<Code>(cached);
    } else {
      index = NumberDictionary::kNotFound;
    }
  } else {
    key = GetKey();
    index = Heap::code_stubs()->FindEntry(key);
    if (index != NumberDictionary::kNotFound)
      return Handle<Code>(Code::cast(Heap::code_stubs()->ValueAt(index)));
  }

  Code* result;
  {
    v8::HandleScope scope;

    // Update the static counter each time a new code stub is generated.
    Counters::code_stubs.Increment();

    // Generate the new code.
    MacroAssembler masm(NULL, 256);

    // Nested stubs are not allowed for leafs.
    masm.set_allow_stub_calls(AllowsStubCalls());

    // Generate the code for the stub.
    masm.set_generating_stub(true);
    Generate(&masm);

    // Create the code object.
    CodeDesc desc;
    masm.GetCode(&desc);

    // Copy the generated code into a heap object, and store the major key.
    Code::Flags flags = Code::ComputeFlags(Code::STUB, InLoop());
    Handle<Code> code = Factory::NewCode(desc, NULL, flags, masm.CodeObject());
    code->set_major_key(MajorKey());

    // Add unresolved entries in the code to the fixup list.
    Bootstrapper::AddFixup(*code, &masm);

    LOG(CodeCreateEvent(Logger::STUB_TAG, *code, GetName()));
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

    if (custom_cache) {
      SetCustomCache(*code);
    } else {
      // Update the dictionary and the root in Heap.
      Handle<NumberDictionary> dict =
          Factory::DictionaryAtNumberPut(
              Handle<NumberDictionary>(Heap::code_stubs()),
              key,
              code);
      Heap::public_set_code_stubs(*dict);
    }
    result = *code;
  }

  return Handle<Code>(result);
}


const char* CodeStub::MajorName(CodeStub::Major major_key) {
  switch (major_key) {
#define DEF_CASE(name) case name: return #name;
    CODE_STUB_LIST(DEF_CASE)
#undef DEF_CASE
    default:
      UNREACHABLE();
      return NULL;
  }
}


} }  // namespace v8::internal
