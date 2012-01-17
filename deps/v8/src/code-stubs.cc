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
  if (index != UnseededNumberDictionary::kNotFound) {
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


SmartArrayPointer<const char> CodeStub::GetName() {
  char buffer[100];
  NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
  StringStream stream(&allocator);
  PrintName(&stream);
  return stream.ToCString();
}


void CodeStub::RecordCodeGeneration(Code* code, MacroAssembler* masm) {
  code->set_major_key(MajorKey());

  Isolate* isolate = masm->isolate();
  SmartArrayPointer<const char> name = GetName();
  PROFILE(isolate, CodeCreateEvent(Logger::STUB_TAG, code, *name));
  GDBJIT(AddCode(GDBJITInterface::STUB, *name, code));
  Counters* counters = isolate->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());

#ifdef ENABLE_DISASSEMBLER
  if (FLAG_print_code_stubs) {
    code->Disassemble(*name);
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
        GetICState());
    Handle<Code> new_object = factory->NewCode(
        desc, flags, masm.CodeObject(), NeedsImmovableCode());
    RecordCodeGeneration(*new_object, &masm);
    FinishCode(*new_object);

    // Update the dictionary and the root in Heap.
    Handle<UnseededNumberDictionary> dict =
        factory->DictionaryAtNumberPut(
            Handle<UnseededNumberDictionary>(heap->code_stubs()),
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
      heap->public_set_code_stubs(UnseededNumberDictionary::cast(new_object));
    }
  }

  return code;
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


void KeyedLoadElementStub::Generate(MacroAssembler* masm) {
  switch (elements_kind_) {
    case FAST_ELEMENTS:
      KeyedLoadStubCompiler::GenerateLoadFastElement(masm);
      break;
    case FAST_DOUBLE_ELEMENTS:
      KeyedLoadStubCompiler::GenerateLoadFastDoubleElement(masm);
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      KeyedLoadStubCompiler::GenerateLoadExternalArray(masm, elements_kind_);
      break;
    case DICTIONARY_ELEMENTS:
      KeyedLoadStubCompiler::GenerateLoadDictionaryElement(masm);
      break;
    case NON_STRICT_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }
}


void KeyedStoreElementStub::Generate(MacroAssembler* masm) {
  switch (elements_kind_) {
    case FAST_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreFastElement(masm, is_js_array_);
      break;
    case FAST_DOUBLE_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreFastDoubleElement(masm,
                                                             is_js_array_);
      break;
    case EXTERNAL_BYTE_ELEMENTS:
    case EXTERNAL_UNSIGNED_BYTE_ELEMENTS:
    case EXTERNAL_SHORT_ELEMENTS:
    case EXTERNAL_UNSIGNED_SHORT_ELEMENTS:
    case EXTERNAL_INT_ELEMENTS:
    case EXTERNAL_UNSIGNED_INT_ELEMENTS:
    case EXTERNAL_FLOAT_ELEMENTS:
    case EXTERNAL_DOUBLE_ELEMENTS:
    case EXTERNAL_PIXEL_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreExternalArray(masm, elements_kind_);
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
  const char* type_name = NULL;  // Make g++ happy.
  switch (type_) {
    case READ_ELEMENT: type_name = "ReadElement"; break;
    case NEW_NON_STRICT_FAST: type_name = "NewNonStrictFast"; break;
    case NEW_NON_STRICT_SLOW: type_name = "NewNonStrictSlow"; break;
    case NEW_STRICT: type_name = "NewStrict"; break;
  }
  stream->Add("ArgumentsAccessStub_%s", type_name);
}


void CallFunctionStub::PrintName(StringStream* stream) {
  const char* flags_name = NULL;  // Make g++ happy.
  switch (flags_) {
    case NO_CALL_FUNCTION_FLAGS: flags_name = ""; break;
    case RECEIVER_MIGHT_BE_IMPLICIT: flags_name = "_Implicit"; break;
  }
  stream->Add("CallFunctionStub_Args%d%s", argc_, flags_name);
}


void ToBooleanStub::PrintName(StringStream* stream) {
  stream->Add("ToBooleanStub_");
  types_.Print(stream);
}


void ToBooleanStub::Types::Print(StringStream* stream) const {
  if (IsEmpty()) stream->Add("None");
  if (Contains(UNDEFINED)) stream->Add("Undefined");
  if (Contains(BOOLEAN)) stream->Add("Bool");
  if (Contains(NULL_TYPE)) stream->Add("Null");
  if (Contains(SMI)) stream->Add("Smi");
  if (Contains(SPEC_OBJECT)) stream->Add("SpecObject");
  if (Contains(STRING)) stream->Add("String");
  if (Contains(HEAP_NUMBER)) stream->Add("HeapNumber");
}


void ToBooleanStub::Types::TraceTransition(Types to) const {
  if (!FLAG_trace_ic) return;
  char buffer[100];
  NoAllocationStringAllocator allocator(buffer,
                                        static_cast<unsigned>(sizeof(buffer)));
  StringStream stream(&allocator);
  stream.Add("[ToBooleanIC (");
  Print(&stream);
  stream.Add("->");
  to.Print(&stream);
  stream.Add(")]\n");
  stream.OutputToStdOut();
}


bool ToBooleanStub::Types::Record(Handle<Object> object) {
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
  } else if (object->IsHeapNumber()) {
    ASSERT(!object->IsUndetectableObject());
    Add(HEAP_NUMBER);
    double value = HeapNumber::cast(*object)->value();
    return value != 0 && !isnan(value);
  } else {
    // We should never see an internal object at runtime here!
    UNREACHABLE();
    return true;
  }
}


bool ToBooleanStub::Types::NeedsMap() const {
  return Contains(ToBooleanStub::SPEC_OBJECT)
      || Contains(ToBooleanStub::STRING)
      || Contains(ToBooleanStub::HEAP_NUMBER);
}


bool ToBooleanStub::Types::CanBeUndetectable() const {
  return Contains(ToBooleanStub::SPEC_OBJECT)
      || Contains(ToBooleanStub::STRING);
}


} }  // namespace v8::internal
