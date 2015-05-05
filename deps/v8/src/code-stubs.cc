// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include <sstream>

#include "src/bootstrapper.h"
#include "src/cpu-profiler.h"
#include "src/factory.h"
#include "src/gdb-jit.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


CodeStubDescriptor::CodeStubDescriptor(CodeStub* stub)
    : call_descriptor_(stub->GetCallInterfaceDescriptor()),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(NULL),
      handler_arguments_mode_(DONT_PASS_ARGUMENTS),
      miss_handler_(),
      has_miss_handler_(false) {
  stub->InitializeDescriptor(this);
}


CodeStubDescriptor::CodeStubDescriptor(Isolate* isolate, uint32_t stub_key)
    : stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(NULL),
      handler_arguments_mode_(DONT_PASS_ARGUMENTS),
      miss_handler_(),
      has_miss_handler_(false) {
  CodeStub::InitializeDescriptor(isolate, stub_key, this);
}


void CodeStubDescriptor::Initialize(Address deoptimization_handler,
                                    int hint_stack_parameter_count,
                                    StubFunctionMode function_mode) {
  deoptimization_handler_ = deoptimization_handler;
  hint_stack_parameter_count_ = hint_stack_parameter_count;
  function_mode_ = function_mode;
}


void CodeStubDescriptor::Initialize(Register stack_parameter_count,
                                    Address deoptimization_handler,
                                    int hint_stack_parameter_count,
                                    StubFunctionMode function_mode,
                                    HandlerArgumentsMode handler_mode) {
  Initialize(deoptimization_handler, hint_stack_parameter_count, function_mode);
  stack_parameter_count_ = stack_parameter_count;
  handler_arguments_mode_ = handler_mode;
}


bool CodeStub::FindCodeInCache(Code** code_out) {
  UnseededNumberDictionary* stubs = isolate()->heap()->code_stubs();
  int index = stubs->FindEntry(GetKey());
  if (index != UnseededNumberDictionary::kNotFound) {
    *code_out = Code::cast(stubs->ValueAt(index));
    return true;
  }
  return false;
}


void CodeStub::RecordCodeGeneration(Handle<Code> code) {
  std::ostringstream os;
  os << *this;
  PROFILE(isolate(),
          CodeCreateEvent(Logger::STUB_TAG, *code, os.str().c_str()));
  Counters* counters = isolate()->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());
#ifdef DEBUG
  code->VerifyEmbeddedObjects();
#endif
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
    // TODO(yangguo): remove this once we can serialize IC stubs.
    masm.enable_serializer();
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


Handle<Code> CodeStub::GetCode() {
  Heap* heap = isolate()->heap();
  Code* code;
  if (UseSpecialCache() ? FindCodeInSpecialCache(&code)
                        : FindCodeInCache(&code)) {
    DCHECK(GetCodeKind() == code->kind());
    return Handle<Code>(code);
  }

  {
    HandleScope scope(isolate());

    Handle<Code> new_object = GenerateCode();
    new_object->set_stub_key(GetKey());
    FinishCode(new_object);
    RecordCodeGeneration(new_object);

#ifdef ENABLE_DISASSEMBLER
    if (FLAG_print_code_stubs) {
      CodeTracer::Scope trace_scope(isolate()->GetCodeTracer());
      OFStream os(trace_scope.file());
      std::ostringstream name;
      name << *this;
      new_object->Disassemble(name.str().c_str(), os);
      os << "\n";
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
  DCHECK(!NeedsImmovableCode() ||
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
    case NoCache:
      return "<NoCache>Stub";
    case NUMBER_OF_IDS:
      UNREACHABLE();
      return NULL;
  }
  return NULL;
}


void CodeStub::PrintBaseName(std::ostream& os) const {  // NOLINT
  os << MajorName(MajorKey(), false);
}


void CodeStub::PrintName(std::ostream& os) const {  // NOLINT
  PrintBaseName(os);
  PrintState(os);
}


void CodeStub::Dispatch(Isolate* isolate, uint32_t key, void** value_out,
                        DispatchedCall call) {
  switch (MajorKeyFromKey(key)) {
#define DEF_CASE(NAME)             \
  case NAME: {                     \
    NAME##Stub stub(key, isolate); \
    CodeStub* pstub = &stub;       \
    call(pstub, value_out);        \
    break;                         \
  }
    CODE_STUB_LIST(DEF_CASE)
#undef DEF_CASE
    case NUMBER_OF_IDS:
    case NoCache:
      UNREACHABLE();
      break;
  }
}


static void InitializeDescriptorDispatchedCall(CodeStub* stub,
                                               void** value_out) {
  CodeStubDescriptor* descriptor_out =
      reinterpret_cast<CodeStubDescriptor*>(value_out);
  stub->InitializeDescriptor(descriptor_out);
  descriptor_out->set_call_descriptor(stub->GetCallInterfaceDescriptor());
}


void CodeStub::InitializeDescriptor(Isolate* isolate, uint32_t key,
                                    CodeStubDescriptor* desc) {
  void** value_out = reinterpret_cast<void**>(desc);
  Dispatch(isolate, key, value_out, &InitializeDescriptorDispatchedCall);
}


void CodeStub::GetCodeDispatchCall(CodeStub* stub, void** value_out) {
  Handle<Code>* code_out = reinterpret_cast<Handle<Code>*>(value_out);
  // Code stubs with special cache cannot be recreated from stub key.
  *code_out = stub->UseSpecialCache() ? Handle<Code>() : stub->GetCode();
}


MaybeHandle<Code> CodeStub::GetCode(Isolate* isolate, uint32_t key) {
  HandleScope scope(isolate);
  Handle<Code> code;
  void** value_out = reinterpret_cast<void**>(&code);
  Dispatch(isolate, key, value_out, &GetCodeDispatchCall);
  return scope.CloseAndEscape(code);
}


// static
void BinaryOpICStub::GenerateAheadOfTime(Isolate* isolate) {
  // Generate the uninitialized versions of the stub.
  for (int op = Token::BIT_OR; op <= Token::MOD; ++op) {
    BinaryOpICStub stub(isolate, static_cast<Token::Value>(op));
    stub.GetCode();
  }

  // Generate special versions of the stub.
  BinaryOpICState::GenerateAheadOfTime(isolate, &GenerateAheadOfTime);
}


void BinaryOpICStub::PrintState(std::ostream& os) const {  // NOLINT
  os << state();
}


// static
void BinaryOpICStub::GenerateAheadOfTime(Isolate* isolate,
                                         const BinaryOpICState& state) {
  BinaryOpICStub stub(isolate, state);
  stub.GetCode();
}


// static
void BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(Isolate* isolate) {
  // Generate special versions of the stub.
  BinaryOpICState::GenerateAheadOfTime(isolate, &GenerateAheadOfTime);
}


void BinaryOpICWithAllocationSiteStub::PrintState(
    std::ostream& os) const {  // NOLINT
  os << state();
}


// static
void BinaryOpICWithAllocationSiteStub::GenerateAheadOfTime(
    Isolate* isolate, const BinaryOpICState& state) {
  if (state.CouldCreateAllocationMementos()) {
    BinaryOpICWithAllocationSiteStub stub(isolate, state);
    stub.GetCode();
  }
}


void StringAddStub::PrintBaseName(std::ostream& os) const {  // NOLINT
  os << "StringAddStub";
  if ((flags() & STRING_ADD_CHECK_BOTH) == STRING_ADD_CHECK_BOTH) {
    os << "_CheckBoth";
  } else if ((flags() & STRING_ADD_CHECK_LEFT) == STRING_ADD_CHECK_LEFT) {
    os << "_CheckLeft";
  } else if ((flags() & STRING_ADD_CHECK_RIGHT) == STRING_ADD_CHECK_RIGHT) {
    os << "_CheckRight";
  }
  if (pretenure_flag() == TENURED) {
    os << "_Tenured";
  }
}


InlineCacheState CompareICStub::GetICState() const {
  CompareICState::State state = Max(left(), right());
  switch (state) {
    case CompareICState::UNINITIALIZED:
      return ::v8::internal::UNINITIALIZED;
    case CompareICState::SMI:
    case CompareICState::NUMBER:
    case CompareICState::INTERNALIZED_STRING:
    case CompareICState::STRING:
    case CompareICState::UNIQUE_NAME:
    case CompareICState::OBJECT:
    case CompareICState::KNOWN_OBJECT:
      return MONOMORPHIC;
    case CompareICState::GENERIC:
      return ::v8::internal::GENERIC;
  }
  UNREACHABLE();
  return ::v8::internal::UNINITIALIZED;
}


Condition CompareICStub::GetCondition() const {
  return CompareIC::ComputeCondition(op());
}


void CompareICStub::AddToSpecialCache(Handle<Code> new_object) {
  DCHECK(*known_map_ != NULL);
  Isolate* isolate = new_object->GetIsolate();
  Factory* factory = isolate->factory();
  return Map::UpdateCodeCache(known_map_,
                              strict() ?
                                  factory->strict_compare_ic_string() :
                                  factory->compare_ic_string(),
                              new_object);
}


bool CompareICStub::FindCodeInSpecialCache(Code** code_out) {
  Factory* factory = isolate()->factory();
  Code::Flags flags = Code::ComputeFlags(
      GetCodeKind(),
      UNINITIALIZED);
  DCHECK(op() == Token::EQ || op() == Token::EQ_STRICT);
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
    CompareICStub decode((*code_out)->stub_key(), isolate());
    DCHECK(op() == decode.op());
    DCHECK(left() == decode.left());
    DCHECK(right() == decode.right());
    DCHECK(state() == decode.state());
#endif
    return true;
  }
  return false;
}


void CompareICStub::Generate(MacroAssembler* masm) {
  switch (state()) {
    case CompareICState::UNINITIALIZED:
      GenerateMiss(masm);
      break;
    case CompareICState::SMI:
      GenerateSmis(masm);
      break;
    case CompareICState::NUMBER:
      GenerateNumbers(masm);
      break;
    case CompareICState::STRING:
      GenerateStrings(masm);
      break;
    case CompareICState::INTERNALIZED_STRING:
      GenerateInternalizedStrings(masm);
      break;
    case CompareICState::UNIQUE_NAME:
      GenerateUniqueNames(masm);
      break;
    case CompareICState::OBJECT:
      GenerateObjects(masm);
      break;
    case CompareICState::KNOWN_OBJECT:
      DCHECK(*known_map_ != NULL);
      GenerateKnownObjects(masm);
      break;
    case CompareICState::GENERIC:
      GenerateGeneric(masm);
      break;
  }
}


void CompareNilICStub::UpdateStatus(Handle<Object> object) {
  State state = this->state();
  DCHECK(!state.Contains(GENERIC));
  State old_state = state;
  if (object->IsNull()) {
    state.Add(NULL_TYPE);
  } else if (object->IsUndefined()) {
    state.Add(UNDEFINED);
  } else if (object->IsUndetectableObject() ||
             object->IsOddball() ||
             !object->IsHeapObject()) {
    state.RemoveAll();
    state.Add(GENERIC);
  } else if (IsMonomorphic()) {
    state.RemoveAll();
    state.Add(GENERIC);
  } else {
    state.Add(MONOMORPHIC_MAP);
  }
  TraceTransition(old_state, state);
  set_sub_minor_key(TypesBits::update(sub_minor_key(), state.ToIntegral()));
}


template<class StateType>
void HydrogenCodeStub::TraceTransition(StateType from, StateType to) {
  // Note: Although a no-op transition is semantically OK, it is hinting at a
  // bug somewhere in our state transition machinery.
  DCHECK(from != to);
  if (!FLAG_trace_ic) return;
  OFStream os(stdout);
  os << "[";
  PrintBaseName(os);
  os << ": " << from << "=>" << to << "]" << std::endl;
}


void CompareNilICStub::PrintBaseName(std::ostream& os) const {  // NOLINT
  CodeStub::PrintBaseName(os);
  os << ((nil_value() == kNullValue) ? "(NullValue)" : "(UndefinedValue)");
}


void CompareNilICStub::PrintState(std::ostream& os) const {  // NOLINT
  os << state();
}


// TODO(svenpanne) Make this a real infix_ostream_iterator.
class SimpleListPrinter {
 public:
  explicit SimpleListPrinter(std::ostream& os) : os_(os), first_(true) {}

  void Add(const char* s) {
    if (first_) {
      first_ = false;
    } else {
      os_ << ",";
    }
    os_ << s;
  }

 private:
  std::ostream& os_;
  bool first_;
};


std::ostream& operator<<(std::ostream& os, const CompareNilICStub::State& s) {
  os << "(";
  SimpleListPrinter p(os);
  if (s.IsEmpty()) p.Add("None");
  if (s.Contains(CompareNilICStub::UNDEFINED)) p.Add("Undefined");
  if (s.Contains(CompareNilICStub::NULL_TYPE)) p.Add("Null");
  if (s.Contains(CompareNilICStub::MONOMORPHIC_MAP)) p.Add("MonomorphicMap");
  if (s.Contains(CompareNilICStub::GENERIC)) p.Add("Generic");
  return os << ")";
}


Type* CompareNilICStub::GetType(Zone* zone, Handle<Map> map) {
  State state = this->state();
  if (state.Contains(CompareNilICStub::GENERIC)) return Type::Any(zone);

  Type* result = Type::None(zone);
  if (state.Contains(CompareNilICStub::UNDEFINED)) {
    result = Type::Union(result, Type::Undefined(zone), zone);
  }
  if (state.Contains(CompareNilICStub::NULL_TYPE)) {
    result = Type::Union(result, Type::Null(zone), zone);
  }
  if (state.Contains(CompareNilICStub::MONOMORPHIC_MAP)) {
    Type* type =
        map.is_null() ? Type::Detectable(zone) : Type::Class(map, zone);
    result = Type::Union(result, type, zone);
  }

  return result;
}


Type* CompareNilICStub::GetInputType(Zone* zone, Handle<Map> map) {
  Type* output_type = GetType(zone, map);
  Type* nil_type =
      nil_value() == kNullValue ? Type::Null(zone) : Type::Undefined(zone);
  return Type::Union(output_type, nil_type, zone);
}


void CallIC_ArrayStub::PrintState(std::ostream& os) const {  // NOLINT
  os << state() << " (Array)";
}


void CallICStub::PrintState(std::ostream& os) const {  // NOLINT
  os << state();
}


void InstanceofStub::PrintName(std::ostream& os) const {  // NOLINT
  os << "InstanceofStub";
  if (HasArgsInRegisters()) os << "_REGS";
  if (HasCallSiteInlineCheck()) os << "_INLINE";
  if (ReturnTrueFalseObject()) os << "_TRUEFALSE";
}


void JSEntryStub::FinishCode(Handle<Code> code) {
  Handle<FixedArray> handler_table =
      code->GetIsolate()->factory()->NewFixedArray(1, TENURED);
  handler_table->set(0, Smi::FromInt(handler_offset_));
  code->set_handler_table(*handler_table);
}


void LoadFastElementStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure));
}


void LoadDictionaryElementStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure));
}


void KeyedLoadGenericStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kKeyedGetProperty)->entry);
}


void HandlerStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  if (kind() == Code::STORE_IC) {
    descriptor->Initialize(FUNCTION_ADDR(StoreIC_MissFromStubFailure));
  } else if (kind() == Code::KEYED_LOAD_IC) {
    descriptor->Initialize(FUNCTION_ADDR(KeyedLoadIC_MissFromStubFailure));
  }
}


CallInterfaceDescriptor HandlerStub::GetCallInterfaceDescriptor() {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    if (FLAG_vector_ics) {
      return VectorLoadICDescriptor(isolate());
    }
    return LoadDescriptor(isolate());
  } else {
    DCHECK_EQ(Code::STORE_IC, kind());
    return StoreDescriptor(isolate());
  }
}


void StoreFastElementStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(KeyedStoreIC_MissFromStubFailure));
}


void ElementsTransitionAndStoreStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(ElementsTransitionAndStoreIC_Miss));
}


CallInterfaceDescriptor StoreTransitionStub::GetCallInterfaceDescriptor() {
  return StoreTransitionDescriptor(isolate());
}


void MegamorphicLoadStub::InitializeDescriptor(CodeStubDescriptor* d) {}


void FastNewClosureStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kNewClosureFromStubFailure)->entry);
}


void FastNewContextStub::InitializeDescriptor(CodeStubDescriptor* d) {}


void NumberToStringStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  NumberToStringDescriptor call_descriptor(isolate());
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kNumberToStringRT)->entry);
}


void FastCloneShallowArrayStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  FastCloneShallowArrayDescriptor call_descriptor(isolate());
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kCreateArrayLiteralStubBailout)->entry);
}


void FastCloneShallowObjectStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  FastCloneShallowObjectDescriptor call_descriptor(isolate());
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kCreateObjectLiteral)->entry);
}


void CreateAllocationSiteStub::InitializeDescriptor(CodeStubDescriptor* d) {}


void CreateWeakCellStub::InitializeDescriptor(CodeStubDescriptor* d) {}


void RegExpConstructResultStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kRegExpConstructResultRT)->entry);
}


void TransitionElementsKindStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kTransitionElementsKind)->entry);
}


void AllocateHeapNumberStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kAllocateHeapNumber)->entry);
}


void CompareNilICStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(CompareNilIC_Miss));
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kCompareNilIC_Miss), isolate()));
}


void ToBooleanStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(ToBooleanIC_Miss));
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kToBooleanIC_Miss), isolate()));
}


void BinaryOpICStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(BinaryOpIC_Miss));
  descriptor->SetMissHandler(
      ExternalReference(IC_Utility(IC::kBinaryOpIC_Miss), isolate()));
}


void BinaryOpWithAllocationSiteStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(BinaryOpIC_MissWithAllocationSite));
}


void StringAddStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(Runtime::FunctionForId(Runtime::kStringAddRT)->entry);
}


void CreateAllocationSiteStub::GenerateAheadOfTime(Isolate* isolate) {
  CreateAllocationSiteStub stub(isolate);
  stub.GetCode();
}


void CreateWeakCellStub::GenerateAheadOfTime(Isolate* isolate) {
  CreateWeakCellStub stub(isolate);
  stub.GetCode();
}


void StoreElementStub::Generate(MacroAssembler* masm) {
  switch (elements_kind()) {
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
      ElementHandlerCompiler::GenerateStoreSlow(masm);
      break;
    case SLOPPY_ARGUMENTS_ELEMENTS:
      UNREACHABLE();
      break;
  }
}


void ArgumentsAccessStub::Generate(MacroAssembler* masm) {
  switch (type()) {
    case READ_ELEMENT:
      GenerateReadElement(masm);
      break;
    case NEW_SLOPPY_FAST:
      GenerateNewSloppyFast(masm);
      break;
    case NEW_SLOPPY_SLOW:
      GenerateNewSloppySlow(masm);
      break;
    case NEW_STRICT:
      GenerateNewStrict(masm);
      break;
  }
}


void RestParamAccessStub::Generate(MacroAssembler* masm) {
  GenerateNew(masm);
}


void ArgumentsAccessStub::PrintName(std::ostream& os) const {  // NOLINT
  os << "ArgumentsAccessStub_";
  switch (type()) {
    case READ_ELEMENT:
      os << "ReadElement";
      break;
    case NEW_SLOPPY_FAST:
      os << "NewSloppyFast";
      break;
    case NEW_SLOPPY_SLOW:
      os << "NewSloppySlow";
      break;
    case NEW_STRICT:
      os << "NewStrict";
      break;
  }
  return;
}


void RestParamAccessStub::PrintName(std::ostream& os) const {  // NOLINT
  os << "RestParamAccessStub_";
}


void CallFunctionStub::PrintName(std::ostream& os) const {  // NOLINT
  os << "CallFunctionStub_Args" << argc();
}


void CallConstructStub::PrintName(std::ostream& os) const {  // NOLINT
  os << "CallConstructStub";
  if (RecordCallTarget()) os << "_Recording";
}


void ArrayConstructorStub::PrintName(std::ostream& os) const {  // NOLINT
  os << "ArrayConstructorStub";
  switch (argument_count()) {
    case ANY:
      os << "_Any";
      break;
    case NONE:
      os << "_None";
      break;
    case ONE:
      os << "_One";
      break;
    case MORE_THAN_ONE:
      os << "_More_Than_One";
      break;
  }
  return;
}


std::ostream& ArrayConstructorStubBase::BasePrintName(
    std::ostream& os,  // NOLINT
    const char* name) const {
  os << name << "_" << ElementsKindToString(elements_kind());
  if (override_mode() == DISABLE_ALLOCATION_SITES) {
    os << "_DISABLE_ALLOCATION_SITES";
  }
  return os;
}


bool ToBooleanStub::UpdateStatus(Handle<Object> object) {
  Types new_types = types();
  Types old_types = new_types;
  bool to_boolean_value = new_types.UpdateStatus(object);
  TraceTransition(old_types, new_types);
  set_sub_minor_key(TypesBits::update(sub_minor_key(), new_types.ToByte()));
  return to_boolean_value;
}


void ToBooleanStub::PrintState(std::ostream& os) const {  // NOLINT
  os << types();
}


std::ostream& operator<<(std::ostream& os, const ToBooleanStub::Types& s) {
  os << "(";
  SimpleListPrinter p(os);
  if (s.IsEmpty()) p.Add("None");
  if (s.Contains(ToBooleanStub::UNDEFINED)) p.Add("Undefined");
  if (s.Contains(ToBooleanStub::BOOLEAN)) p.Add("Bool");
  if (s.Contains(ToBooleanStub::NULL_TYPE)) p.Add("Null");
  if (s.Contains(ToBooleanStub::SMI)) p.Add("Smi");
  if (s.Contains(ToBooleanStub::SPEC_OBJECT)) p.Add("SpecObject");
  if (s.Contains(ToBooleanStub::STRING)) p.Add("String");
  if (s.Contains(ToBooleanStub::SYMBOL)) p.Add("Symbol");
  if (s.Contains(ToBooleanStub::HEAP_NUMBER)) p.Add("HeapNumber");
  return os << ")";
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
    DCHECK(!object->IsUndetectableObject());
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
  DCHECK(entry_hook != NULL);
  entry_hook(function, stack_pointer);
}


ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate)
    : PlatformCodeStub(isolate) {
  minor_key_ = ArgumentCountBits::encode(ANY);
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate,
                                           int argument_count)
    : PlatformCodeStub(isolate) {
  if (argument_count == 0) {
    minor_key_ = ArgumentCountBits::encode(NONE);
  } else if (argument_count == 1) {
    minor_key_ = ArgumentCountBits::encode(ONE);
  } else if (argument_count >= 2) {
    minor_key_ = ArgumentCountBits::encode(MORE_THAN_ONE);
  } else {
    UNREACHABLE();
  }
  ArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


InternalArrayConstructorStub::InternalArrayConstructorStub(
    Isolate* isolate) : PlatformCodeStub(isolate) {
  InternalArrayConstructorStubBase::GenerateStubsAheadOfTime(isolate);
}


} }  // namespace v8::internal
