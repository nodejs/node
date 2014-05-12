// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
      handler_arguments_mode_(DONT_PASS_ARGUMENTS),
      miss_handler_(),
      has_miss_handler_(false) { }


bool CodeStub::FindCodeInCache(Code** code_out) {
  UnseededNumberDictionary* stubs = isolate()->heap()->code_stubs();
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


void CodeStub::RecordCodeGeneration(Handle<Code> code) {
  IC::RegisterWeakMapDependency(code);
  SmartArrayPointer<const char> name = GetName();
  PROFILE(isolate(), CodeCreateEvent(Logger::STUB_TAG, *code, name.get()));
  GDBJIT(AddCode(GDBJITInterface::STUB, name.get(), *code));
  Counters* counters = isolate()->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());
}


Code::Kind CodeStub::GetCodeKind() const {
  return Code::STUB;
}


Handle<Code> CodeStub::GetCodeCopy(const Code::FindAndReplacePattern& pattern) {
  Handle<Code> ic = GetCode();
  ic = isolate()->factory()->CopyCode(ic);
  ic->FindAndReplace(pattern);
  RecordCodeGeneration(ic);
  return ic;
}


Handle<Code> PlatformCodeStub::GenerateCode() {
  Factory* factory = isolate()->factory();

  // Generate the new code.
  MacroAssembler masm(isolate(), NULL, 256);

  {
    // Update the static counter each time a new code stub is generated.
    isolate()->counters()->code_stubs()->Increment();

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
      GetStubType());
  Handle<Code> new_object = factory->NewCode(
      desc, flags, masm.CodeObject(), NeedsImmovableCode());
  return new_object;
}


void CodeStub::VerifyPlatformFeatures() {
  ASSERT(CpuFeatures::VerifyCrossCompiling());
}


Handle<Code> CodeStub::GetCode() {
  Heap* heap = isolate()->heap();
  Code* code;
  if (UseSpecialCache()
      ? FindCodeInSpecialCache(&code)
      : FindCodeInCache(&code)) {
    ASSERT(GetCodeKind() == code->kind());
    return Handle<Code>(code);
  }

#ifdef DEBUG
  VerifyPlatformFeatures();
#endif

  {
    HandleScope scope(isolate());

    Handle<Code> new_object = GenerateCode();
    new_object->set_major_key(MajorKey());
    FinishCode(new_object);
    RecordCodeGeneration(new_object);

#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_code_stubs) {
      CodeTracer::Scope trace_scope(isolate()->GetCodeTracer());
      new_object->Disassemble(GetName().get(), trace_scope.file());
      PrintF(trace_scope.file(), "\n");
    }
#endif

    if (UseSpecialCache()) {
      AddToSpecialCache(new_object);
    } else {
      // Update the dictionary and the root in Heap.
      Handle<UnseededNumberDictionary> dict =
          UnseededNumberDictionary::AtNumberPut(
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
  return Handle<Code>(code, isolate());
}


const char* CodeStub::MajorName(CodeStub::Major major_key,
                                bool allow_unknown_keys) {
  switch (major_key) {
#define DEF_CASE(name) case name: return #name "Stub";
    CODE_STUB_LIST(DEF_CASE)
#undef DEF_CASE
    case UninitializedMajorKey: return "<UninitializedMajorKey>Stub";
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


// static
void BinaryOpICStub::GenerateAheadOfTime(Isolate* isolate) {
  // Generate the uninitialized versions of the stub.
  for (int op = Token::BIT_OR; op <= Token::MOD; ++op) {
    for (int mode = NO_OVERWRITE; mode <= OVERWRITE_RIGHT; ++mode) {
      BinaryOpICStub stub(isolate,
                          static_cast<Token::Value>(op),
                          static_cast<OverwriteMode>(mode));
      stub.GetCode();
    }
  }

  // Generate special versions of the stub.
  BinaryOpIC::State::GenerateAheadOfTime(isolate, &GenerateAheadOfTime);
}


void BinaryOpICStub::PrintState(StringStream* stream) {
  state_.Print(stream);
}


// static
void BinaryOpICStub::GenerateAheadOfTime(Isolate* isolate,
                                         const BinaryOpIC::State& state) {
  BinaryOpICStub stub(isolate, state);
  stub.GetCode();
}


// static
void BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(Isolate* isolate) {
  // Generate special versions of the stub.
  BinaryOpIC::State::GenerateAheadOfTime(isolate, &GenerateAheadOfTime);
}


void BinaryOpICWithAllocationSiteStub::PrintState(StringStream* stream) {
  state_.Print(stream);
}


// static
void BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(
    Isolate* isolate, const BinaryOpIC::State& state) {
  if (state.CouldCreateAllocationMementos()) {
    BinaryOpICWithAllocationSiteStub stub(isolate, state);
    stub.GetCode();
  }
}


void StringAddStub::PrintBaseName(StringStream* stream) {
  stream->Add("StringAddStub");
  if ((flags() & STRING_ADD_CHECK_BOTH) == STRING_ADD_CHECK_BOTH) {
    stream->Add("_CheckBoth");
  } else if ((flags() & STRING_ADD_CHECK_LEFT) == STRING_ADD_CHECK_LEFT) {
    stream->Add("_CheckLeft");
  } else if ((flags() & STRING_ADD_CHECK_RIGHT) == STRING_ADD_CHECK_RIGHT) {
    stream->Add("_CheckRight");
  }
  if (pretenure_flag() == TENURED) {
    stream->Add("_Tenured");
  }
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


bool ICCompareStub::FindCodeInSpecialCache(Code** code_out) {
  Factory* factory = isolate()->factory();
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
      isolate());
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


Type* CompareNilICStub::GetType(Zone* zone, Handle<Map> map) {
  if (state_.Contains(CompareNilICStub::GENERIC)) {
    return Type::Any(zone);
  }

  Type* result = Type::None(zone);
  if (state_.Contains(CompareNilICStub::UNDEFINED)) {
    result = Type::Union(result, Type::Undefined(zone), zone);
  }
  if (state_.Contains(CompareNilICStub::NULL_TYPE)) {
    result = Type::Union(result, Type::Null(zone), zone);
  }
  if (state_.Contains(CompareNilICStub::MONOMORPHIC_MAP)) {
    Type* type =
        map.is_null() ? Type::Detectable(zone) : Type::Class(map, zone);
    result = Type::Union(result, type, zone);
  }

  return result;
}


Type* CompareNilICStub::GetInputType(Zone* zone, Handle<Map> map) {
  Type* output_type = GetType(zone, map);
  Type* nil_type =
      nil_value_ == kNullValue ? Type::Null(zone) : Type::Undefined(zone);
  return Type::Union(output_type, nil_type, zone);
}


void CallICStub::PrintState(StringStream* stream) {
  state_.Print(stream);
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


void KeyedLoadDictionaryElementPlatformStub::Generate(
    MacroAssembler* masm) {
  KeyedLoadStubCompiler::GenerateLoadDictionaryElement(masm);
}


void CreateAllocationSiteStub::GenerateAheadOfTime(Isolate* isolate) {
  CreateAllocationSiteStub stub(isolate);
  stub.GetCode();
}


void KeyedStoreElementStub::Generate(MacroAssembler* masm) {
  switch (elements_kind_) {
    case FAST_ELEMENTS:
    case FAST_HOLEY_ELEMENTS:
    case FAST_SMI_ELEMENTS:
    case FAST_HOLEY_SMI_ELEMENTS:
    case FAST_DOUBLE_ELEMENTS:
    case FAST_HOLEY_DOUBLE_ELEMENTS:
#define TYPED_ARRAY_CASE(Type, type, TYPE, ctype, size) \
    case EXTERNAL_##TYPE##_ELEMENTS:                    \
    case TYPE##_ELEMENTS:

    TYPED_ARRAYS(TYPED_ARRAY_CASE)
#undef TYPED_ARRAY_CASE
      UNREACHABLE();
      break;
    case DICTIONARY_ELEMENTS:
      KeyedStoreStubCompiler::GenerateStoreDictionaryElement(masm);
      break;
    case SLOPPY_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }
}


void ArgumentsAccessStub::PrintName(StringStream* stream) {
  stream->Add("ArgumentsAccessStub_");
  switch (type_) {
    case READ_ELEMENT: stream->Add("ReadElement"); break;
    case NEW_SLOPPY_FAST: stream->Add("NewSloppyFast"); break;
    case NEW_SLOPPY_SLOW: stream->Add("NewSloppySlow"); break;
    case NEW_STRICT: stream->Add("NewStrict"); break;
  }
}


void CallFunctionStub::PrintName(StringStream* stream) {
  stream->Add("CallFunctionStub_Args%d", argc_);
}


void CallConstructStub::PrintName(StringStream* stream) {
  stream->Add("CallConstructStub");
  if (RecordCallTarget()) stream->Add("_Recording");
}


void ArrayConstructorStub::PrintName(StringStream* stream) {
  stream->Add("ArrayConstructorStub");
  switch (argument_count_) {
    case ANY: stream->Add("_Any"); break;
    case NONE: stream->Add("_None"); break;
    case ONE: stream->Add("_One"); break;
    case MORE_THAN_ONE: stream->Add("_More_Than_One"); break;
  }
}


void ArrayConstructorStubBase::BasePrintName(const char* name,
                                             StringStream* stream) {
  stream->Add(name);
  stream->Add("_");
  stream->Add(ElementsKindToString(elements_kind()));
  if (override_mode() == DISABLE_ALLOCATION_SITES) {
    stream->Add("_DISABLE_ALLOCATION_SITES");
  }
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
  StubFailureTrampolineStub stub1(isolate, NOT_JS_FUNCTION_STUB_MODE);
  StubFailureTrampolineStub stub2(isolate, JS_FUNCTION_STUB_MODE);
  stub1.GetCode();
  stub2.GetCode();
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
    stub->InitializeInterfaceDescriptor(descriptor);
  }
}


void ArrayConstructorStubBase::InstallDescriptors(Isolate* isolate) {
  ArrayNoArgumentConstructorStub stub1(isolate, GetInitialFastElementsKind());
  InstallDescriptor(isolate, &stub1);
  ArraySingleArgumentConstructorStub stub2(isolate,
                                           GetInitialFastElementsKind());
  InstallDescriptor(isolate, &stub2);
  ArrayNArgumentsConstructorStub stub3(isolate, GetInitialFastElementsKind());
  InstallDescriptor(isolate, &stub3);
}


void NumberToStringStub::InstallDescriptors(Isolate* isolate) {
  NumberToStringStub stub(isolate);
  InstallDescriptor(isolate, &stub);
}


void FastNewClosureStub::InstallDescriptors(Isolate* isolate) {
  FastNewClosureStub stub(isolate, STRICT, false);
  InstallDescriptor(isolate, &stub);
}


void FastNewContextStub::InstallDescriptors(Isolate* isolate) {
  FastNewContextStub stub(isolate, FastNewContextStub::kMaximumSlots);
  InstallDescriptor(isolate, &stub);
}


// static
void FastCloneShallowArrayStub::InstallDescriptors(Isolate* isolate) {
  FastCloneShallowArrayStub stub(isolate,
                                 FastCloneShallowArrayStub::CLONE_ELEMENTS,
                                 DONT_TRACK_ALLOCATION_SITE, 0);
  InstallDescriptor(isolate, &stub);
}


// static
void BinaryOpICStub::InstallDescriptors(Isolate* isolate) {
  BinaryOpICStub stub(isolate, Token::ADD, NO_OVERWRITE);
  InstallDescriptor(isolate, &stub);
}


// static
void BinaryOpWithAllocationSiteStub::InstallDescriptors(Isolate* isolate) {
  BinaryOpWithAllocationSiteStub stub(isolate, Token::ADD, NO_OVERWRITE);
  InstallDescriptor(isolate, &stub);
}


// static
void StringAddStub::InstallDescriptors(Isolate* isolate) {
  StringAddStub stub(isolate, STRING_ADD_CHECK_NONE, NOT_TENURED);
  InstallDescriptor(isolate, &stub);
}


// static
void RegExpConstructResultStub::InstallDescriptors(Isolate* isolate) {
  RegExpConstructResultStub stub(isolate);
  InstallDescriptor(isolate, &stub);
}


ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate)
    : PlatformCodeStub(isolate), argument_count_(ANY) {
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate,
                                           int argument_count)
    : PlatformCodeStub(isolate) {
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
  InternalArrayNoArgumentConstructorStub stub1(isolate, FAST_ELEMENTS);
  InstallDescriptor(isolate, &stub1);
  InternalArraySingleArgumentConstructorStub stub2(isolate, FAST_ELEMENTS);
  InstallDescriptor(isolate, &stub2);
  InternalArrayNArgumentsConstructorStub stub3(isolate, FAST_ELEMENTS);
  InstallDescriptor(isolate, &stub3);
}

InternalArrayConstructorStub::InternalArrayConstructorStub(
    Isolate* isolate) : PlatformCodeStub(isolate) {
  InternalArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


} }  // namespace v8::internal
