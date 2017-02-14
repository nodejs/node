// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include <sstream>

#include "src/ast/ast.h"
#include "src/bootstrapper.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/factory.h"
#include "src/gdb-jit.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"

namespace v8 {
namespace internal {


RUNTIME_FUNCTION(UnexpectedStubMiss) {
  FATAL("Unexpected deopt of a stub");
  return Smi::kZero;
}

CodeStubDescriptor::CodeStubDescriptor(CodeStub* stub)
    : isolate_(stub->isolate()),
      call_descriptor_(stub->GetCallInterfaceDescriptor()),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(NULL),
      miss_handler_(),
      has_miss_handler_(false) {
  stub->InitializeDescriptor(this);
}

CodeStubDescriptor::CodeStubDescriptor(Isolate* isolate, uint32_t stub_key)
    : isolate_(isolate),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(NULL),
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
                                    StubFunctionMode function_mode) {
  Initialize(deoptimization_handler, hint_stack_parameter_count, function_mode);
  stack_parameter_count_ = stack_parameter_count;
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
          CodeCreateEvent(CodeEventListener::STUB_TAG,
                          AbstractCode::cast(*code), os.str().c_str()));
  Counters* counters = isolate()->counters();
  counters->total_stubs_code_size()->Increment(code->instruction_size());
#ifdef DEBUG
  code->VerifyEmbeddedObjects();
#endif
}


Code::Kind CodeStub::GetCodeKind() const {
  return Code::STUB;
}


Code::Flags CodeStub::GetCodeFlags() const {
  return Code::ComputeFlags(GetCodeKind(), GetExtraICState());
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
  MacroAssembler masm(isolate(), NULL, 256, CodeObjectRequired::kYes);

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
  Code::Flags flags = Code::ComputeFlags(GetCodeKind(), GetExtraICState());
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
      heap->SetRootCodeStubs(*dict);
    }
    code = *new_object;
  }

  Activate(code);
  DCHECK(!NeedsImmovableCode() ||
         heap->lo_space()->Contains(code) ||
         heap->code_space()->FirstPage()->Contains(code->address()));
  return Handle<Code>(code, isolate());
}


const char* CodeStub::MajorName(CodeStub::Major major_key) {
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
  os << MajorName(MajorKey());
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
  if (FLAG_minimal) return;
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
  if (FLAG_minimal) return;
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
  os << "StringAddStub_" << flags() << "_" << pretenure_flag();
}

void StringAddStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* left = assembler->Parameter(Descriptor::kLeft);
  Node* right = assembler->Parameter(Descriptor::kRight);
  Node* context = assembler->Parameter(Descriptor::kContext);

  if ((flags() & STRING_ADD_CHECK_LEFT) != 0) {
    DCHECK((flags() & STRING_ADD_CONVERT) != 0);
    // TODO(danno): The ToString and JSReceiverToPrimitive below could be
    // combined to avoid duplicate smi and instance type checks.
    left = assembler->ToString(context,
                               assembler->JSReceiverToPrimitive(context, left));
  }
  if ((flags() & STRING_ADD_CHECK_RIGHT) != 0) {
    DCHECK((flags() & STRING_ADD_CONVERT) != 0);
    // TODO(danno): The ToString and JSReceiverToPrimitive below could be
    // combined to avoid duplicate smi and instance type checks.
    right = assembler->ToString(
        context, assembler->JSReceiverToPrimitive(context, right));
  }

  if ((flags() & STRING_ADD_CHECK_BOTH) == 0) {
    CodeStubAssembler::AllocationFlag flags =
        (pretenure_flag() == TENURED) ? CodeStubAssembler::kPretenured
                                      : CodeStubAssembler::kNone;
    assembler->Return(assembler->StringAdd(context, left, right, flags));
  } else {
    Callable callable = CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE,
                                               pretenure_flag());
    assembler->TailCallStub(callable, context, left, right);
  }
}

InlineCacheState CompareICStub::GetICState() const {
  CompareICState::State state = Max(left(), right());
  switch (state) {
    case CompareICState::UNINITIALIZED:
      return ::v8::internal::UNINITIALIZED;
    case CompareICState::BOOLEAN:
    case CompareICState::SMI:
    case CompareICState::NUMBER:
    case CompareICState::INTERNALIZED_STRING:
    case CompareICState::STRING:
    case CompareICState::UNIQUE_NAME:
    case CompareICState::RECEIVER:
    case CompareICState::KNOWN_RECEIVER:
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


void CompareICStub::Generate(MacroAssembler* masm) {
  switch (state()) {
    case CompareICState::UNINITIALIZED:
      GenerateMiss(masm);
      break;
    case CompareICState::BOOLEAN:
      GenerateBooleans(masm);
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
    case CompareICState::RECEIVER:
      GenerateReceivers(masm);
      break;
    case CompareICState::KNOWN_RECEIVER:
      DCHECK(*known_map_ != NULL);
      GenerateKnownReceivers(masm);
      break;
    case CompareICState::GENERIC:
      GenerateGeneric(masm);
      break;
  }
}

Handle<Code> TurboFanCodeStub::GenerateCode() {
  const char* name = CodeStub::MajorName(MajorKey());
  Zone zone(isolate()->allocator(), ZONE_NAME);
  CallInterfaceDescriptor descriptor(GetCallInterfaceDescriptor());
  CodeStubAssembler assembler(isolate(), &zone, descriptor, GetCodeFlags(),
                              name);
  GenerateAssembly(&assembler);
  return assembler.GenerateCode();
}

void LoadICTrampolineStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* vector = assembler->LoadTypeFeedbackVectorForStub();

  CodeStubAssembler::LoadICParameters p(context, receiver, name, slot, vector);
  assembler->LoadIC(&p);
}

void LoadICStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::LoadICParameters p(context, receiver, name, slot, vector);
  assembler->LoadIC(&p);
}

void LoadICProtoArrayStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* handler = assembler->Parameter(Descriptor::kHandler);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::LoadICParameters p(context, receiver, name, slot, vector);
  assembler->LoadICProtoArray(&p, handler);
}

void LoadGlobalICTrampolineStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* vector = assembler->LoadTypeFeedbackVectorForStub();

  CodeStubAssembler::LoadICParameters p(context, nullptr, nullptr, slot,
                                        vector);
  assembler->LoadGlobalIC(&p);
}

void LoadGlobalICStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::LoadICParameters p(context, nullptr, nullptr, slot,
                                        vector);
  assembler->LoadGlobalIC(&p);
}

void KeyedLoadICTrampolineTFStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* vector = assembler->LoadTypeFeedbackVectorForStub();

  CodeStubAssembler::LoadICParameters p(context, receiver, name, slot, vector);
  assembler->KeyedLoadIC(&p);
}

void KeyedLoadICTFStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::LoadICParameters p(context, receiver, name, slot, vector);
  assembler->KeyedLoadIC(&p);
}

void StoreICTrampolineStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* vector = assembler->LoadTypeFeedbackVectorForStub();

  CodeStubAssembler::StoreICParameters p(context, receiver, name, value, slot,
                                         vector);
  assembler->StoreIC(&p);
}

void StoreICStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::StoreICParameters p(context, receiver, name, value, slot,
                                         vector);
  assembler->StoreIC(&p);
}

void KeyedStoreICTrampolineTFStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* vector = assembler->LoadTypeFeedbackVectorForStub();

  CodeStubAssembler::StoreICParameters p(context, receiver, name, value, slot,
                                         vector);
  assembler->KeyedStoreIC(&p, StoreICState::GetLanguageMode(GetExtraICState()));
}

void KeyedStoreICTFStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  CodeStubAssembler::StoreICParameters p(context, receiver, name, value, slot,
                                         vector);
  assembler->KeyedStoreIC(&p, StoreICState::GetLanguageMode(GetExtraICState()));
}

void StoreMapStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* map = assembler->Parameter(Descriptor::kMap);
  Node* value = assembler->Parameter(Descriptor::kValue);

  assembler->StoreObjectField(receiver, JSObject::kMapOffset, map);
  assembler->Return(value);
}

void StoreTransitionStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* offset =
      assembler->SmiUntag(assembler->Parameter(Descriptor::kFieldOffset));
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* map = assembler->Parameter(Descriptor::kMap);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label miss(assembler);

  Representation representation = this->representation();
  assembler->Comment("StoreTransitionStub: is_inobject: %d: representation: %s",
                     is_inobject(), representation.Mnemonic());

  Node* prepared_value =
      assembler->PrepareValueForWrite(value, representation, &miss);

  if (store_mode() == StoreTransitionStub::ExtendStorageAndStoreMapAndValue) {
    assembler->Comment("Extend storage");
    assembler->ExtendPropertiesBackingStore(receiver);
  } else {
    DCHECK(store_mode() == StoreTransitionStub::StoreMapAndValue);
  }

  // Store the new value into the "extended" object.
  assembler->Comment("Store value");
  assembler->StoreNamedField(receiver, offset, is_inobject(), representation,
                             prepared_value, true);

  // And finally update the map.
  assembler->Comment("Store map");
  assembler->StoreObjectField(receiver, JSObject::kMapOffset, map);
  assembler->Return(value);

  // Only store to tagged field never bails out.
  if (!representation.IsTagged()) {
    assembler->Bind(&miss);
    {
      assembler->Comment("Miss");
      assembler->TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot,
                                 vector, receiver, name);
    }
  }
}

void ElementsTransitionAndStoreStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* key = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* map = assembler->Parameter(Descriptor::kMap);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Comment(
      "ElementsTransitionAndStoreStub: from_kind=%s, to_kind=%s,"
      " is_jsarray=%d, store_mode=%d",
      ElementsKindToString(from_kind()), ElementsKindToString(to_kind()),
      is_jsarray(), store_mode());

  Label miss(assembler);

  if (FLAG_trace_elements_transitions) {
    // Tracing elements transitions is the job of the runtime.
    assembler->Goto(&miss);
  } else {
    assembler->TransitionElementsKind(receiver, map, from_kind(), to_kind(),
                                      is_jsarray(), &miss);
    assembler->EmitElementStore(receiver, key, value, is_jsarray(), to_kind(),
                                store_mode(), &miss);
    assembler->Return(value);
  }

  assembler->Bind(&miss);
  {
    assembler->Comment("Miss");
    assembler->TailCallRuntime(Runtime::kElementsTransitionAndStoreIC_Miss,
                               context, receiver, key, value, map, slot,
                               vector);
  }
}

void AllocateHeapNumberStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* result = assembler->AllocateHeapNumber();
  assembler->Return(result);
}

#define SIMD128_GEN_ASM(TYPE, Type, type, lane_count, lane_type)            \
  void Allocate##Type##Stub::GenerateAssembly(CodeStubAssembler* assembler) \
      const {                                                               \
    compiler::Node* result =                                                \
        assembler->Allocate(Simd128Value::kSize, CodeStubAssembler::kNone); \
    compiler::Node* map = assembler->LoadMap(result);                       \
    assembler->StoreNoWriteBarrier(                                         \
        MachineRepresentation::kTagged, map,                                \
        assembler->HeapConstant(isolate()->factory()->type##_map()));       \
    assembler->Return(result);                                              \
  }
SIMD128_TYPES(SIMD128_GEN_ASM)
#undef SIMD128_GEN_ASM

void StringLengthStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  compiler::Node* value = assembler->Parameter(0);
  compiler::Node* string = assembler->LoadJSValueValue(value);
  compiler::Node* result = assembler->LoadStringLength(string);
  assembler->Return(result);
}

// static
compiler::Node* AddWithFeedbackStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* lhs, compiler::Node* rhs,
    compiler::Node* slot_id, compiler::Node* type_feedback_vector,
    compiler::Node* context) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry for floating point addition.
  Label do_fadd(assembler), if_lhsisnotnumber(assembler, Label::kDeferred),
      check_rhsisoddball(assembler, Label::kDeferred),
      call_with_oddball_feedback(assembler), call_with_any_feedback(assembler),
      call_add_stub(assembler), end(assembler);
  Variable var_fadd_lhs(assembler, MachineRepresentation::kFloat64),
      var_fadd_rhs(assembler, MachineRepresentation::kFloat64),
      var_type_feedback(assembler, MachineRepresentation::kWord32),
      var_result(assembler, MachineRepresentation::kTagged);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

  assembler->Bind(&if_lhsissmi);
  {
    // Check if the {rhs} is also a Smi.
    Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                      &if_rhsisnotsmi);

    assembler->Bind(&if_rhsissmi);
    {
      // Try fast Smi addition first.
      Node* pair =
          assembler->IntPtrAddWithOverflow(assembler->BitcastTaggedToWord(lhs),
                                           assembler->BitcastTaggedToWord(rhs));
      Node* overflow = assembler->Projection(1, pair);

      // Check if the Smi additon overflowed.
      Label if_overflow(assembler), if_notoverflow(assembler);
      assembler->Branch(overflow, &if_overflow, &if_notoverflow);

      assembler->Bind(&if_overflow);
      {
        var_fadd_lhs.Bind(assembler->SmiToFloat64(lhs));
        var_fadd_rhs.Bind(assembler->SmiToFloat64(rhs));
        assembler->Goto(&do_fadd);
      }

      assembler->Bind(&if_notoverflow);
      {
        var_type_feedback.Bind(
            assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall));
        var_result.Bind(assembler->BitcastWordToTaggedSigned(
            assembler->Projection(0, pair)));
        assembler->Goto(&end);
      }
    }

    assembler->Bind(&if_rhsisnotsmi);
    {
      // Load the map of {rhs}.
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if the {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->IsHeapNumberMap(rhs_map),
                            &check_rhsisoddball);

      var_fadd_lhs.Bind(assembler->SmiToFloat64(lhs));
      var_fadd_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
      assembler->Goto(&do_fadd);
    }
  }

  assembler->Bind(&if_lhsisnotsmi);
  {
    // Load the map of {lhs}.
    Node* lhs_map = assembler->LoadMap(lhs);

    // Check if {lhs} is a HeapNumber.
    assembler->GotoUnless(assembler->IsHeapNumberMap(lhs_map),
                          &if_lhsisnotnumber);

    // Check if the {rhs} is Smi.
    Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                      &if_rhsisnotsmi);

    assembler->Bind(&if_rhsissmi);
    {
      var_fadd_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
      var_fadd_rhs.Bind(assembler->SmiToFloat64(rhs));
      assembler->Goto(&do_fadd);
    }

    assembler->Bind(&if_rhsisnotsmi);
    {
      // Load the map of {rhs}.
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if the {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->IsHeapNumberMap(rhs_map),
                            &check_rhsisoddball);

      var_fadd_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
      var_fadd_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
      assembler->Goto(&do_fadd);
    }
  }

  assembler->Bind(&do_fadd);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumber));
    Node* value =
        assembler->Float64Add(var_fadd_lhs.value(), var_fadd_rhs.value());
    Node* result = assembler->AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    assembler->Goto(&end);
  }

  assembler->Bind(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    Label if_lhsisoddball(assembler), if_lhsisnotoddball(assembler);
    Node* lhs_instance_type = assembler->LoadInstanceType(lhs);
    Node* lhs_is_oddball = assembler->Word32Equal(
        lhs_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->Branch(lhs_is_oddball, &if_lhsisoddball, &if_lhsisnotoddball);

    assembler->Bind(&if_lhsisoddball);
    {
      assembler->GotoIf(assembler->TaggedIsSmi(rhs),
                        &call_with_oddball_feedback);

      // Load the map of the {rhs}.
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      assembler->Branch(assembler->IsHeapNumberMap(rhs_map),
                        &call_with_oddball_feedback, &check_rhsisoddball);
    }

    assembler->Bind(&if_lhsisnotoddball);
    {
      // Exit unless {lhs} is a string
      assembler->GotoUnless(assembler->IsStringInstanceType(lhs_instance_type),
                            &call_with_any_feedback);

      // Check if the {rhs} is a smi, and exit the string check early if it is.
      assembler->GotoIf(assembler->TaggedIsSmi(rhs), &call_with_any_feedback);

      Node* rhs_instance_type = assembler->LoadInstanceType(rhs);

      // Exit unless {rhs} is a string. Since {lhs} is a string we no longer
      // need an Oddball check.
      assembler->GotoUnless(assembler->IsStringInstanceType(rhs_instance_type),
                            &call_with_any_feedback);

      var_type_feedback.Bind(
          assembler->Int32Constant(BinaryOperationFeedback::kString));
      Callable callable = CodeFactory::StringAdd(
          assembler->isolate(), STRING_ADD_CHECK_NONE, NOT_TENURED);
      var_result.Bind(assembler->CallStub(callable, context, lhs, rhs));

      assembler->Goto(&end);
    }
  }

  assembler->Bind(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = assembler->LoadInstanceType(rhs);
    Node* rhs_is_oddball = assembler->Word32Equal(
        rhs_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->Branch(rhs_is_oddball, &call_with_oddball_feedback,
                      &call_with_any_feedback);
  }

  assembler->Bind(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
    assembler->Goto(&call_add_stub);
  }

  assembler->Bind(&call_with_any_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kAny));
    assembler->Goto(&call_add_stub);
  }

  assembler->Bind(&call_add_stub);
  {
    Callable callable = CodeFactory::Add(assembler->isolate());
    var_result.Bind(assembler->CallStub(callable, context, lhs, rhs));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return var_result.value();
}

// static
compiler::Node* SubtractWithFeedbackStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* lhs, compiler::Node* rhs,
    compiler::Node* slot_id, compiler::Node* type_feedback_vector,
    compiler::Node* context) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry for floating point subtraction.
  Label do_fsub(assembler), end(assembler), call_subtract_stub(assembler),
      if_lhsisnotnumber(assembler), check_rhsisoddball(assembler),
      call_with_any_feedback(assembler);
  Variable var_fsub_lhs(assembler, MachineRepresentation::kFloat64),
      var_fsub_rhs(assembler, MachineRepresentation::kFloat64),
      var_type_feedback(assembler, MachineRepresentation::kWord32),
      var_result(assembler, MachineRepresentation::kTagged);

  // Check if the {lhs} is a Smi or a HeapObject.
  Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

  assembler->Bind(&if_lhsissmi);
  {
    // Check if the {rhs} is also a Smi.
    Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                      &if_rhsisnotsmi);

    assembler->Bind(&if_rhsissmi);
    {
      // Try a fast Smi subtraction first.
      Node* pair =
          assembler->IntPtrSubWithOverflow(assembler->BitcastTaggedToWord(lhs),
                                           assembler->BitcastTaggedToWord(rhs));
      Node* overflow = assembler->Projection(1, pair);

      // Check if the Smi subtraction overflowed.
      Label if_overflow(assembler), if_notoverflow(assembler);
      assembler->Branch(overflow, &if_overflow, &if_notoverflow);

      assembler->Bind(&if_overflow);
      {
        // lhs, rhs - smi and result - number. combined - number.
        // The result doesn't fit into Smi range.
        var_fsub_lhs.Bind(assembler->SmiToFloat64(lhs));
        var_fsub_rhs.Bind(assembler->SmiToFloat64(rhs));
        assembler->Goto(&do_fsub);
      }

      assembler->Bind(&if_notoverflow);
      // lhs, rhs, result smi. combined - smi.
      var_type_feedback.Bind(
          assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall));
      var_result.Bind(
          assembler->BitcastWordToTaggedSigned(assembler->Projection(0, pair)));
      assembler->Goto(&end);
    }

    assembler->Bind(&if_rhsisnotsmi);
    {
      // Load the map of the {rhs}.
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->IsHeapNumberMap(rhs_map),
                            &check_rhsisoddball);

      // Perform a floating point subtraction.
      var_fsub_lhs.Bind(assembler->SmiToFloat64(lhs));
      var_fsub_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
      assembler->Goto(&do_fsub);
    }
  }

  assembler->Bind(&if_lhsisnotsmi);
  {
    // Load the map of the {lhs}.
    Node* lhs_map = assembler->LoadMap(lhs);

    // Check if the {lhs} is a HeapNumber.
    assembler->GotoUnless(assembler->IsHeapNumberMap(lhs_map),
                          &if_lhsisnotnumber);

    // Check if the {rhs} is a Smi.
    Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                      &if_rhsisnotsmi);

    assembler->Bind(&if_rhsissmi);
    {
      // Perform a floating point subtraction.
      var_fsub_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
      var_fsub_rhs.Bind(assembler->SmiToFloat64(rhs));
      assembler->Goto(&do_fsub);
    }

    assembler->Bind(&if_rhsisnotsmi);
    {
      // Load the map of the {rhs}.
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if the {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->IsHeapNumberMap(rhs_map),
                            &check_rhsisoddball);

      // Perform a floating point subtraction.
      var_fsub_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
      var_fsub_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
      assembler->Goto(&do_fsub);
    }
  }

  assembler->Bind(&do_fsub);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumber));
    Node* lhs_value = var_fsub_lhs.value();
    Node* rhs_value = var_fsub_rhs.value();
    Node* value = assembler->Float64Sub(lhs_value, rhs_value);
    var_result.Bind(assembler->AllocateHeapNumberWithValue(value));
    assembler->Goto(&end);
  }

  assembler->Bind(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    // Check if lhs is an oddball.
    Node* lhs_instance_type = assembler->LoadInstanceType(lhs);
    Node* lhs_is_oddball = assembler->Word32Equal(
        lhs_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->GotoUnless(lhs_is_oddball, &call_with_any_feedback);

    Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &if_rhsissmi,
                      &if_rhsisnotsmi);

    assembler->Bind(&if_rhsissmi);
    {
      var_type_feedback.Bind(
          assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
      assembler->Goto(&call_subtract_stub);
    }

    assembler->Bind(&if_rhsisnotsmi);
    {
      // Load the map of the {rhs}.
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->IsHeapNumberMap(rhs_map),
                            &check_rhsisoddball);

      var_type_feedback.Bind(
          assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
      assembler->Goto(&call_subtract_stub);
    }
  }

  assembler->Bind(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = assembler->LoadInstanceType(rhs);
    Node* rhs_is_oddball = assembler->Word32Equal(
        rhs_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->GotoUnless(rhs_is_oddball, &call_with_any_feedback);

    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
    assembler->Goto(&call_subtract_stub);
  }

  assembler->Bind(&call_with_any_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kAny));
    assembler->Goto(&call_subtract_stub);
  }

  assembler->Bind(&call_subtract_stub);
  {
    Callable callable = CodeFactory::Subtract(assembler->isolate());
    var_result.Bind(assembler->CallStub(callable, context, lhs, rhs));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return var_result.value();
}


// static
compiler::Node* MultiplyWithFeedbackStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* lhs, compiler::Node* rhs,
    compiler::Node* slot_id, compiler::Node* type_feedback_vector,
    compiler::Node* context) {
  using compiler::Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry point for floating point multiplication.
  Label do_fmul(assembler), if_lhsisnotnumber(assembler, Label::kDeferred),
      check_rhsisoddball(assembler, Label::kDeferred),
      call_with_oddball_feedback(assembler), call_with_any_feedback(assembler),
      call_multiply_stub(assembler), end(assembler);
  Variable var_lhs_float64(assembler, MachineRepresentation::kFloat64),
      var_rhs_float64(assembler, MachineRepresentation::kFloat64),
      var_result(assembler, MachineRepresentation::kTagged),
      var_type_feedback(assembler, MachineRepresentation::kWord32);

  Node* number_map = assembler->HeapNumberMapConstant();

  Label lhs_is_smi(assembler), lhs_is_not_smi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(lhs), &lhs_is_smi, &lhs_is_not_smi);

  assembler->Bind(&lhs_is_smi);
  {
    Label rhs_is_smi(assembler), rhs_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &rhs_is_smi,
                      &rhs_is_not_smi);

    assembler->Bind(&rhs_is_smi);
    {
      // Both {lhs} and {rhs} are Smis. The result is not necessarily a smi,
      // in case of overflow.
      var_result.Bind(assembler->SmiMul(lhs, rhs));
      var_type_feedback.Bind(assembler->Select(
          assembler->TaggedIsSmi(var_result.value()),
          assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall),
          assembler->Int32Constant(BinaryOperationFeedback::kNumber),
          MachineRepresentation::kWord32));
      assembler->Goto(&end);
    }

    assembler->Bind(&rhs_is_not_smi);
    {
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->WordEqual(rhs_map, number_map),
                            &check_rhsisoddball);

      // Convert {lhs} to a double and multiply it with the value of {rhs}.
      var_lhs_float64.Bind(assembler->SmiToFloat64(lhs));
      var_rhs_float64.Bind(assembler->LoadHeapNumberValue(rhs));
      assembler->Goto(&do_fmul);
    }
  }

  assembler->Bind(&lhs_is_not_smi);
  {
    Node* lhs_map = assembler->LoadMap(lhs);

    // Check if {lhs} is a HeapNumber.
    assembler->GotoUnless(assembler->WordEqual(lhs_map, number_map),
                          &if_lhsisnotnumber);

    // Check if {rhs} is a Smi.
    Label rhs_is_smi(assembler), rhs_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(rhs), &rhs_is_smi,
                      &rhs_is_not_smi);

    assembler->Bind(&rhs_is_smi);
    {
      // Convert {rhs} to a double and multiply it with the value of {lhs}.
      var_lhs_float64.Bind(assembler->LoadHeapNumberValue(lhs));
      var_rhs_float64.Bind(assembler->SmiToFloat64(rhs));
      assembler->Goto(&do_fmul);
    }

    assembler->Bind(&rhs_is_not_smi);
    {
      Node* rhs_map = assembler->LoadMap(rhs);

      // Check if {rhs} is a HeapNumber.
      assembler->GotoUnless(assembler->WordEqual(rhs_map, number_map),
                            &check_rhsisoddball);

      // Both {lhs} and {rhs} are HeapNumbers. Load their values and
      // multiply them.
      var_lhs_float64.Bind(assembler->LoadHeapNumberValue(lhs));
      var_rhs_float64.Bind(assembler->LoadHeapNumberValue(rhs));
      assembler->Goto(&do_fmul);
    }
  }

  assembler->Bind(&do_fmul);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumber));
    Node* value =
        assembler->Float64Mul(var_lhs_float64.value(), var_rhs_float64.value());
    Node* result = assembler->AllocateHeapNumberWithValue(value);
    var_result.Bind(result);
    assembler->Goto(&end);
  }

  assembler->Bind(&if_lhsisnotnumber);
  {
    // No checks on rhs are done yet. We just know lhs is not a number or Smi.
    // Check if lhs is an oddball.
    Node* lhs_instance_type = assembler->LoadInstanceType(lhs);
    Node* lhs_is_oddball = assembler->Word32Equal(
        lhs_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->GotoUnless(lhs_is_oddball, &call_with_any_feedback);

    assembler->GotoIf(assembler->TaggedIsSmi(rhs), &call_with_oddball_feedback);

    // Load the map of the {rhs}.
    Node* rhs_map = assembler->LoadMap(rhs);

    // Check if {rhs} is a HeapNumber.
    assembler->Branch(assembler->IsHeapNumberMap(rhs_map),
                      &call_with_oddball_feedback, &check_rhsisoddball);
  }

  assembler->Bind(&check_rhsisoddball);
  {
    // Check if rhs is an oddball. At this point we know lhs is either a
    // Smi or number or oddball and rhs is not a number or Smi.
    Node* rhs_instance_type = assembler->LoadInstanceType(rhs);
    Node* rhs_is_oddball = assembler->Word32Equal(
        rhs_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->Branch(rhs_is_oddball, &call_with_oddball_feedback,
                      &call_with_any_feedback);
  }

  assembler->Bind(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
    assembler->Goto(&call_multiply_stub);
  }

  assembler->Bind(&call_with_any_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kAny));
    assembler->Goto(&call_multiply_stub);
  }

  assembler->Bind(&call_multiply_stub);
  {
    Callable callable = CodeFactory::Multiply(assembler->isolate());
    var_result.Bind(assembler->CallStub(callable, context, lhs, rhs));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return var_result.value();
}


// static
compiler::Node* DivideWithFeedbackStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* dividend,
    compiler::Node* divisor, compiler::Node* slot_id,
    compiler::Node* type_feedback_vector, compiler::Node* context) {
  using compiler::Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry point for floating point division.
  Label do_fdiv(assembler), dividend_is_not_number(assembler, Label::kDeferred),
      check_divisor_for_oddball(assembler, Label::kDeferred),
      call_with_oddball_feedback(assembler), call_with_any_feedback(assembler),
      call_divide_stub(assembler), end(assembler);
  Variable var_dividend_float64(assembler, MachineRepresentation::kFloat64),
      var_divisor_float64(assembler, MachineRepresentation::kFloat64),
      var_result(assembler, MachineRepresentation::kTagged),
      var_type_feedback(assembler, MachineRepresentation::kWord32);

  Node* number_map = assembler->HeapNumberMapConstant();

  Label dividend_is_smi(assembler), dividend_is_not_smi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(dividend), &dividend_is_smi,
                    &dividend_is_not_smi);

  assembler->Bind(&dividend_is_smi);
  {
    Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                      &divisor_is_not_smi);

    assembler->Bind(&divisor_is_smi);
    {
      Label bailout(assembler);

      // Do floating point division if {divisor} is zero.
      assembler->GotoIf(
          assembler->WordEqual(divisor, assembler->IntPtrConstant(0)),
          &bailout);

      // Do floating point division {dividend} is zero and {divisor} is
      // negative.
      Label dividend_is_zero(assembler), dividend_is_not_zero(assembler);
      assembler->Branch(
          assembler->WordEqual(dividend, assembler->IntPtrConstant(0)),
          &dividend_is_zero, &dividend_is_not_zero);

      assembler->Bind(&dividend_is_zero);
      {
        assembler->GotoIf(
            assembler->IntPtrLessThan(divisor, assembler->IntPtrConstant(0)),
            &bailout);
        assembler->Goto(&dividend_is_not_zero);
      }
      assembler->Bind(&dividend_is_not_zero);

      Node* untagged_divisor = assembler->SmiUntag(divisor);
      Node* untagged_dividend = assembler->SmiUntag(dividend);

      // Do floating point division if {dividend} is kMinInt (or kMinInt - 1
      // if the Smi size is 31) and {divisor} is -1.
      Label divisor_is_minus_one(assembler),
          divisor_is_not_minus_one(assembler);
      assembler->Branch(assembler->Word32Equal(untagged_divisor,
                                               assembler->Int32Constant(-1)),
                        &divisor_is_minus_one, &divisor_is_not_minus_one);

      assembler->Bind(&divisor_is_minus_one);
      {
        assembler->GotoIf(
            assembler->Word32Equal(
                untagged_dividend,
                assembler->Int32Constant(kSmiValueSize == 32 ? kMinInt
                                                             : (kMinInt >> 1))),
            &bailout);
        assembler->Goto(&divisor_is_not_minus_one);
      }
      assembler->Bind(&divisor_is_not_minus_one);

      Node* untagged_result =
          assembler->Int32Div(untagged_dividend, untagged_divisor);
      Node* truncated = assembler->Int32Mul(untagged_result, untagged_divisor);
      // Do floating point division if the remainder is not 0.
      assembler->GotoIf(assembler->Word32NotEqual(untagged_dividend, truncated),
                        &bailout);
      var_type_feedback.Bind(
          assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall));
      var_result.Bind(assembler->SmiTag(untagged_result));
      assembler->Goto(&end);

      // Bailout: convert {dividend} and {divisor} to double and do double
      // division.
      assembler->Bind(&bailout);
      {
        var_dividend_float64.Bind(assembler->SmiToFloat64(dividend));
        var_divisor_float64.Bind(assembler->SmiToFloat64(divisor));
        assembler->Goto(&do_fdiv);
      }
    }

    assembler->Bind(&divisor_is_not_smi);
    {
      Node* divisor_map = assembler->LoadMap(divisor);

      // Check if {divisor} is a HeapNumber.
      assembler->GotoUnless(assembler->WordEqual(divisor_map, number_map),
                            &check_divisor_for_oddball);

      // Convert {dividend} to a double and divide it with the value of
      // {divisor}.
      var_dividend_float64.Bind(assembler->SmiToFloat64(dividend));
      var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
      assembler->Goto(&do_fdiv);
    }

    assembler->Bind(&dividend_is_not_smi);
    {
      Node* dividend_map = assembler->LoadMap(dividend);

      // Check if {dividend} is a HeapNumber.
      assembler->GotoUnless(assembler->WordEqual(dividend_map, number_map),
                            &dividend_is_not_number);

      // Check if {divisor} is a Smi.
      Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
      assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                        &divisor_is_not_smi);

      assembler->Bind(&divisor_is_smi);
      {
        // Convert {divisor} to a double and use it for a floating point
        // division.
        var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
        var_divisor_float64.Bind(assembler->SmiToFloat64(divisor));
        assembler->Goto(&do_fdiv);
      }

      assembler->Bind(&divisor_is_not_smi);
      {
        Node* divisor_map = assembler->LoadMap(divisor);

        // Check if {divisor} is a HeapNumber.
        assembler->GotoUnless(assembler->WordEqual(divisor_map, number_map),
                              &check_divisor_for_oddball);

        // Both {dividend} and {divisor} are HeapNumbers. Load their values
        // and divide them.
        var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
        var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
        assembler->Goto(&do_fdiv);
      }
    }
  }

  assembler->Bind(&do_fdiv);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumber));
    Node* value = assembler->Float64Div(var_dividend_float64.value(),
                                        var_divisor_float64.value());
    var_result.Bind(assembler->AllocateHeapNumberWithValue(value));
    assembler->Goto(&end);
  }

  assembler->Bind(&dividend_is_not_number);
  {
    // We just know dividend is not a number or Smi. No checks on divisor yet.
    // Check if dividend is an oddball.
    Node* dividend_instance_type = assembler->LoadInstanceType(dividend);
    Node* dividend_is_oddball = assembler->Word32Equal(
        dividend_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->GotoUnless(dividend_is_oddball, &call_with_any_feedback);

    assembler->GotoIf(assembler->TaggedIsSmi(divisor),
                      &call_with_oddball_feedback);

    // Load the map of the {divisor}.
    Node* divisor_map = assembler->LoadMap(divisor);

    // Check if {divisor} is a HeapNumber.
    assembler->Branch(assembler->IsHeapNumberMap(divisor_map),
                      &call_with_oddball_feedback, &check_divisor_for_oddball);
  }

  assembler->Bind(&check_divisor_for_oddball);
  {
    // Check if divisor is an oddball. At this point we know dividend is either
    // a Smi or number or oddball and divisor is not a number or Smi.
    Node* divisor_instance_type = assembler->LoadInstanceType(divisor);
    Node* divisor_is_oddball = assembler->Word32Equal(
        divisor_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->Branch(divisor_is_oddball, &call_with_oddball_feedback,
                      &call_with_any_feedback);
  }

  assembler->Bind(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
    assembler->Goto(&call_divide_stub);
  }

  assembler->Bind(&call_with_any_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kAny));
    assembler->Goto(&call_divide_stub);
  }

  assembler->Bind(&call_divide_stub);
  {
    Callable callable = CodeFactory::Divide(assembler->isolate());
    var_result.Bind(assembler->CallStub(callable, context, dividend, divisor));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return var_result.value();
}

// static
compiler::Node* ModulusWithFeedbackStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* dividend,
    compiler::Node* divisor, compiler::Node* slot_id,
    compiler::Node* type_feedback_vector, compiler::Node* context) {
  using compiler::Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry point for floating point division.
  Label do_fmod(assembler), dividend_is_not_number(assembler, Label::kDeferred),
      check_divisor_for_oddball(assembler, Label::kDeferred),
      call_with_oddball_feedback(assembler), call_with_any_feedback(assembler),
      call_modulus_stub(assembler), end(assembler);
  Variable var_dividend_float64(assembler, MachineRepresentation::kFloat64),
      var_divisor_float64(assembler, MachineRepresentation::kFloat64),
      var_result(assembler, MachineRepresentation::kTagged),
      var_type_feedback(assembler, MachineRepresentation::kWord32);

  Node* number_map = assembler->HeapNumberMapConstant();

  Label dividend_is_smi(assembler), dividend_is_not_smi(assembler);
  assembler->Branch(assembler->TaggedIsSmi(dividend), &dividend_is_smi,
                    &dividend_is_not_smi);

  assembler->Bind(&dividend_is_smi);
  {
    Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                      &divisor_is_not_smi);

    assembler->Bind(&divisor_is_smi);
    {
      var_result.Bind(assembler->SmiMod(dividend, divisor));
      var_type_feedback.Bind(assembler->Select(
          assembler->TaggedIsSmi(var_result.value()),
          assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall),
          assembler->Int32Constant(BinaryOperationFeedback::kNumber)));
      assembler->Goto(&end);
    }

    assembler->Bind(&divisor_is_not_smi);
    {
      Node* divisor_map = assembler->LoadMap(divisor);

      // Check if {divisor} is a HeapNumber.
      assembler->GotoUnless(assembler->WordEqual(divisor_map, number_map),
                            &check_divisor_for_oddball);

      // Convert {dividend} to a double and divide it with the value of
      // {divisor}.
      var_dividend_float64.Bind(assembler->SmiToFloat64(dividend));
      var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
      assembler->Goto(&do_fmod);
    }
  }

  assembler->Bind(&dividend_is_not_smi);
  {
    Node* dividend_map = assembler->LoadMap(dividend);

    // Check if {dividend} is a HeapNumber.
    assembler->GotoUnless(assembler->WordEqual(dividend_map, number_map),
                          &dividend_is_not_number);

    // Check if {divisor} is a Smi.
    Label divisor_is_smi(assembler), divisor_is_not_smi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(divisor), &divisor_is_smi,
                      &divisor_is_not_smi);

    assembler->Bind(&divisor_is_smi);
    {
      // Convert {divisor} to a double and use it for a floating point
      // division.
      var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
      var_divisor_float64.Bind(assembler->SmiToFloat64(divisor));
      assembler->Goto(&do_fmod);
    }

    assembler->Bind(&divisor_is_not_smi);
    {
      Node* divisor_map = assembler->LoadMap(divisor);

      // Check if {divisor} is a HeapNumber.
      assembler->GotoUnless(assembler->WordEqual(divisor_map, number_map),
                            &check_divisor_for_oddball);

      // Both {dividend} and {divisor} are HeapNumbers. Load their values
      // and divide them.
      var_dividend_float64.Bind(assembler->LoadHeapNumberValue(dividend));
      var_divisor_float64.Bind(assembler->LoadHeapNumberValue(divisor));
      assembler->Goto(&do_fmod);
    }
  }

  assembler->Bind(&do_fmod);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumber));
    Node* value = assembler->Float64Mod(var_dividend_float64.value(),
                                        var_divisor_float64.value());
    var_result.Bind(assembler->AllocateHeapNumberWithValue(value));
    assembler->Goto(&end);
  }

  assembler->Bind(&dividend_is_not_number);
  {
    // No checks on divisor yet. We just know dividend is not a number or Smi.
    // Check if dividend is an oddball.
    Node* dividend_instance_type = assembler->LoadInstanceType(dividend);
    Node* dividend_is_oddball = assembler->Word32Equal(
        dividend_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->GotoUnless(dividend_is_oddball, &call_with_any_feedback);

    assembler->GotoIf(assembler->TaggedIsSmi(divisor),
                      &call_with_oddball_feedback);

    // Load the map of the {divisor}.
    Node* divisor_map = assembler->LoadMap(divisor);

    // Check if {divisor} is a HeapNumber.
    assembler->Branch(assembler->IsHeapNumberMap(divisor_map),
                      &call_with_oddball_feedback, &check_divisor_for_oddball);
  }

  assembler->Bind(&check_divisor_for_oddball);
  {
    // Check if divisor is an oddball. At this point we know dividend is either
    // a Smi or number or oddball and divisor is not a number or Smi.
    Node* divisor_instance_type = assembler->LoadInstanceType(divisor);
    Node* divisor_is_oddball = assembler->Word32Equal(
        divisor_instance_type, assembler->Int32Constant(ODDBALL_TYPE));
    assembler->Branch(divisor_is_oddball, &call_with_oddball_feedback,
                      &call_with_any_feedback);
  }

  assembler->Bind(&call_with_oddball_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kNumberOrOddball));
    assembler->Goto(&call_modulus_stub);
  }

  assembler->Bind(&call_with_any_feedback);
  {
    var_type_feedback.Bind(
        assembler->Int32Constant(BinaryOperationFeedback::kAny));
    assembler->Goto(&call_modulus_stub);
  }

  assembler->Bind(&call_modulus_stub);
  {
    Callable callable = CodeFactory::Modulus(assembler->isolate());
    var_result.Bind(assembler->CallStub(callable, context, dividend, divisor));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return var_result.value();
}

// static
compiler::Node* IncStub::Generate(CodeStubAssembler* assembler,
                                  compiler::Node* value,
                                  compiler::Node* context,
                                  compiler::Node* type_feedback_vector,
                                  compiler::Node* slot_id) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry for floating point increment.
  Label do_finc(assembler), end(assembler);
  Variable var_finc_value(assembler, MachineRepresentation::kFloat64);

  // We might need to try again due to ToNumber conversion.
  Variable value_var(assembler, MachineRepresentation::kTagged);
  Variable result_var(assembler, MachineRepresentation::kTagged);
  Variable var_type_feedback(assembler, MachineRepresentation::kWord32);
  Variable* loop_vars[] = {&value_var, &var_type_feedback};
  Label start(assembler, 2, loop_vars);
  value_var.Bind(value);
  var_type_feedback.Bind(
      assembler->Int32Constant(BinaryOperationFeedback::kNone));
  assembler->Goto(&start);
  assembler->Bind(&start);
  {
    value = value_var.value();

    Label if_issmi(assembler), if_isnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

    assembler->Bind(&if_issmi);
    {
      // Try fast Smi addition first.
      Node* one = assembler->SmiConstant(Smi::FromInt(1));
      Node* pair = assembler->IntPtrAddWithOverflow(
          assembler->BitcastTaggedToWord(value),
          assembler->BitcastTaggedToWord(one));
      Node* overflow = assembler->Projection(1, pair);

      // Check if the Smi addition overflowed.
      Label if_overflow(assembler), if_notoverflow(assembler);
      assembler->Branch(overflow, &if_overflow, &if_notoverflow);

      assembler->Bind(&if_notoverflow);
      var_type_feedback.Bind(assembler->Word32Or(
          var_type_feedback.value(),
          assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall)));
      result_var.Bind(
          assembler->BitcastWordToTaggedSigned(assembler->Projection(0, pair)));
      assembler->Goto(&end);

      assembler->Bind(&if_overflow);
      {
        var_finc_value.Bind(assembler->SmiToFloat64(value));
        assembler->Goto(&do_finc);
      }
    }

    assembler->Bind(&if_isnotsmi);
    {
      // Check if the value is a HeapNumber.
      Label if_valueisnumber(assembler),
          if_valuenotnumber(assembler, Label::kDeferred);
      Node* value_map = assembler->LoadMap(value);
      assembler->Branch(assembler->IsHeapNumberMap(value_map),
                        &if_valueisnumber, &if_valuenotnumber);

      assembler->Bind(&if_valueisnumber);
      {
        // Load the HeapNumber value.
        var_finc_value.Bind(assembler->LoadHeapNumberValue(value));
        assembler->Goto(&do_finc);
      }

      assembler->Bind(&if_valuenotnumber);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(assembler,
                   assembler->Word32Equal(var_type_feedback.value(),
                                          assembler->Int32Constant(
                                              BinaryOperationFeedback::kNone)));

        Label if_valueisoddball(assembler), if_valuenotoddball(assembler);
        Node* instance_type = assembler->LoadMapInstanceType(value_map);
        Node* is_oddball = assembler->Word32Equal(
            instance_type, assembler->Int32Constant(ODDBALL_TYPE));
        assembler->Branch(is_oddball, &if_valueisoddball, &if_valuenotoddball);

        assembler->Bind(&if_valueisoddball);
        {
          // Convert Oddball to Number and check again.
          value_var.Bind(
              assembler->LoadObjectField(value, Oddball::kToNumberOffset));
          var_type_feedback.Bind(assembler->Int32Constant(
              BinaryOperationFeedback::kNumberOrOddball));
          assembler->Goto(&start);
        }

        assembler->Bind(&if_valuenotoddball);
        {
          // Convert to a Number first and try again.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_type_feedback.Bind(
              assembler->Int32Constant(BinaryOperationFeedback::kAny));
          value_var.Bind(assembler->CallStub(callable, context, value));
          assembler->Goto(&start);
        }
      }
    }
  }

  assembler->Bind(&do_finc);
  {
    Node* finc_value = var_finc_value.value();
    Node* one = assembler->Float64Constant(1.0);
    Node* finc_result = assembler->Float64Add(finc_value, one);
    var_type_feedback.Bind(assembler->Word32Or(
        var_type_feedback.value(),
        assembler->Int32Constant(BinaryOperationFeedback::kNumber)));
    result_var.Bind(assembler->AllocateHeapNumberWithValue(finc_result));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return result_var.value();
}

void NumberToStringStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* argument = assembler->Parameter(Descriptor::kArgument);
  Node* context = assembler->Parameter(Descriptor::kContext);
  assembler->Return(assembler->NumberToString(context, argument));
}

// static
compiler::Node* DecStub::Generate(CodeStubAssembler* assembler,
                                  compiler::Node* value,
                                  compiler::Node* context,
                                  compiler::Node* type_feedback_vector,
                                  compiler::Node* slot_id) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;

  // Shared entry for floating point decrement.
  Label do_fdec(assembler), end(assembler);
  Variable var_fdec_value(assembler, MachineRepresentation::kFloat64);

  // We might need to try again due to ToNumber conversion.
  Variable value_var(assembler, MachineRepresentation::kTagged);
  Variable result_var(assembler, MachineRepresentation::kTagged);
  Variable var_type_feedback(assembler, MachineRepresentation::kWord32);
  Variable* loop_vars[] = {&value_var, &var_type_feedback};
  Label start(assembler, 2, loop_vars);
  var_type_feedback.Bind(
      assembler->Int32Constant(BinaryOperationFeedback::kNone));
  value_var.Bind(value);
  assembler->Goto(&start);
  assembler->Bind(&start);
  {
    value = value_var.value();

    Label if_issmi(assembler), if_isnotsmi(assembler);
    assembler->Branch(assembler->TaggedIsSmi(value), &if_issmi, &if_isnotsmi);

    assembler->Bind(&if_issmi);
    {
      // Try fast Smi subtraction first.
      Node* one = assembler->SmiConstant(Smi::FromInt(1));
      Node* pair = assembler->IntPtrSubWithOverflow(
          assembler->BitcastTaggedToWord(value),
          assembler->BitcastTaggedToWord(one));
      Node* overflow = assembler->Projection(1, pair);

      // Check if the Smi subtraction overflowed.
      Label if_overflow(assembler), if_notoverflow(assembler);
      assembler->Branch(overflow, &if_overflow, &if_notoverflow);

      assembler->Bind(&if_notoverflow);
      var_type_feedback.Bind(assembler->Word32Or(
          var_type_feedback.value(),
          assembler->Int32Constant(BinaryOperationFeedback::kSignedSmall)));
      result_var.Bind(
          assembler->BitcastWordToTaggedSigned(assembler->Projection(0, pair)));
      assembler->Goto(&end);

      assembler->Bind(&if_overflow);
      {
        var_fdec_value.Bind(assembler->SmiToFloat64(value));
        assembler->Goto(&do_fdec);
      }
    }

    assembler->Bind(&if_isnotsmi);
    {
      // Check if the value is a HeapNumber.
      Label if_valueisnumber(assembler),
          if_valuenotnumber(assembler, Label::kDeferred);
      Node* value_map = assembler->LoadMap(value);
      assembler->Branch(assembler->IsHeapNumberMap(value_map),
                        &if_valueisnumber, &if_valuenotnumber);

      assembler->Bind(&if_valueisnumber);
      {
        // Load the HeapNumber value.
        var_fdec_value.Bind(assembler->LoadHeapNumberValue(value));
        assembler->Goto(&do_fdec);
      }

      assembler->Bind(&if_valuenotnumber);
      {
        // We do not require an Or with earlier feedback here because once we
        // convert the value to a number, we cannot reach this path. We can
        // only reach this path on the first pass when the feedback is kNone.
        CSA_ASSERT(assembler,
                   assembler->Word32Equal(var_type_feedback.value(),
                                          assembler->Int32Constant(
                                              BinaryOperationFeedback::kNone)));

        Label if_valueisoddball(assembler), if_valuenotoddball(assembler);
        Node* instance_type = assembler->LoadMapInstanceType(value_map);
        Node* is_oddball = assembler->Word32Equal(
            instance_type, assembler->Int32Constant(ODDBALL_TYPE));
        assembler->Branch(is_oddball, &if_valueisoddball, &if_valuenotoddball);

        assembler->Bind(&if_valueisoddball);
        {
          // Convert Oddball to Number and check again.
          value_var.Bind(
              assembler->LoadObjectField(value, Oddball::kToNumberOffset));
          var_type_feedback.Bind(assembler->Int32Constant(
              BinaryOperationFeedback::kNumberOrOddball));
          assembler->Goto(&start);
        }

        assembler->Bind(&if_valuenotoddball);
        {
          // Convert to a Number first and try again.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_type_feedback.Bind(
              assembler->Int32Constant(BinaryOperationFeedback::kAny));
          value_var.Bind(assembler->CallStub(callable, context, value));
          assembler->Goto(&start);
        }
      }
    }
  }

  assembler->Bind(&do_fdec);
  {
    Node* fdec_value = var_fdec_value.value();
    Node* one = assembler->Float64Constant(1.0);
    Node* fdec_result = assembler->Float64Sub(fdec_value, one);
    var_type_feedback.Bind(assembler->Word32Or(
        var_type_feedback.value(),
        assembler->Int32Constant(BinaryOperationFeedback::kNumber)));
    result_var.Bind(assembler->AllocateHeapNumberWithValue(fdec_result));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->UpdateFeedback(var_type_feedback.value(), type_feedback_vector,
                            slot_id);
  return result_var.value();
}

// ES6 section 21.1.3.19 String.prototype.substring ( start, end )
compiler::Node* SubStringStub::Generate(CodeStubAssembler* assembler,
                                        compiler::Node* string,
                                        compiler::Node* from,
                                        compiler::Node* to,
                                        compiler::Node* context) {
  return assembler->SubString(context, string, from, to);
}

void LoadApiGetterStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* context = assembler->Parameter(Descriptor::kContext);
  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  // For now we only support receiver_is_holder.
  DCHECK(receiver_is_holder());
  Node* holder = receiver;
  Node* map = assembler->LoadMap(receiver);
  Node* descriptors = assembler->LoadMapDescriptors(map);
  Node* value_index =
      assembler->IntPtrConstant(DescriptorArray::ToValueIndex(index()));
  Node* callback = assembler->LoadFixedArrayElement(
      descriptors, value_index, 0, CodeStubAssembler::INTPTR_PARAMETERS);
  assembler->TailCallStub(CodeFactory::ApiGetter(isolate()), context, receiver,
                          holder, callback);
}

void StoreFieldStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  FieldIndex index = this->index();
  Representation representation = this->representation();

  assembler->Comment("StoreFieldStub: inobject=%d, offset=%d, rep=%s",
                     index.is_inobject(), index.offset(),
                     representation.Mnemonic());

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label miss(assembler);

  Node* prepared_value =
      assembler->PrepareValueForWrite(value, representation, &miss);
  assembler->StoreNamedField(receiver, index, representation, prepared_value,
                             false);
  assembler->Return(value);

  // Only stores to tagged field can't bailout.
  if (!representation.IsTagged()) {
    assembler->Bind(&miss);
    {
      assembler->Comment("Miss");
      assembler->TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot,
                                 vector, receiver, name);
    }
  }
}

void StoreGlobalStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  assembler->Comment(
      "StoreGlobalStub: cell_type=%d, constant_type=%d, check_global=%d",
      cell_type(), PropertyCellType::kConstantType == cell_type()
                       ? static_cast<int>(constant_type())
                       : -1,
      check_global());

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label miss(assembler);

  if (check_global()) {
    // Check that the map of the global has not changed: use a placeholder map
    // that will be replaced later with the global object's map.
    Node* proxy_map = assembler->LoadMap(receiver);
    Node* global = assembler->LoadObjectField(proxy_map, Map::kPrototypeOffset);
    Node* map_cell = assembler->HeapConstant(isolate()->factory()->NewWeakCell(
        StoreGlobalStub::global_map_placeholder(isolate())));
    Node* expected_map = assembler->LoadWeakCellValueUnchecked(map_cell);
    Node* map = assembler->LoadMap(global);
    assembler->GotoIf(assembler->WordNotEqual(expected_map, map), &miss);
  }

  Node* weak_cell = assembler->HeapConstant(isolate()->factory()->NewWeakCell(
      StoreGlobalStub::property_cell_placeholder(isolate())));
  Node* cell = assembler->LoadWeakCellValue(weak_cell);
  assembler->GotoIf(assembler->TaggedIsSmi(cell), &miss);

  // Load the payload of the global parameter cell. A hole indicates that the
  // cell has been invalidated and that the store must be handled by the
  // runtime.
  Node* cell_contents =
      assembler->LoadObjectField(cell, PropertyCell::kValueOffset);

  PropertyCellType cell_type = this->cell_type();
  if (cell_type == PropertyCellType::kConstant ||
      cell_type == PropertyCellType::kUndefined) {
    // This is always valid for all states a cell can be in.
    assembler->GotoIf(assembler->WordNotEqual(cell_contents, value), &miss);
  } else {
    assembler->GotoIf(assembler->IsTheHole(cell_contents), &miss);

    // When dealing with constant types, the type may be allowed to change, as
    // long as optimized code remains valid.
    bool value_is_smi = false;
    if (cell_type == PropertyCellType::kConstantType) {
      switch (constant_type()) {
        case PropertyCellConstantType::kSmi:
          assembler->GotoUnless(assembler->TaggedIsSmi(value), &miss);
          value_is_smi = true;
          break;
        case PropertyCellConstantType::kStableMap: {
          // It is sufficient here to check that the value and cell contents
          // have identical maps, no matter if they are stable or not or if they
          // are the maps that were originally in the cell or not. If optimized
          // code will deopt when a cell has a unstable map and if it has a
          // dependency on a stable map, it will deopt if the map destabilizes.
          assembler->GotoIf(assembler->TaggedIsSmi(value), &miss);
          assembler->GotoIf(assembler->TaggedIsSmi(cell_contents), &miss);
          Node* expected_map = assembler->LoadMap(cell_contents);
          Node* map = assembler->LoadMap(value);
          assembler->GotoIf(assembler->WordNotEqual(expected_map, map), &miss);
          break;
        }
      }
    }
    if (value_is_smi) {
      assembler->StoreObjectFieldNoWriteBarrier(
          cell, PropertyCell::kValueOffset, value);
    } else {
      assembler->StoreObjectField(cell, PropertyCell::kValueOffset, value);
    }
  }

  assembler->Return(value);

  assembler->Bind(&miss);
  {
    assembler->Comment("Miss");
    assembler->TailCallRuntime(Runtime::kStoreIC_Miss, context, value, slot,
                               vector, receiver, name);
  }
}

void KeyedLoadSloppyArgumentsStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* key = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label miss(assembler);

  Node* result = assembler->LoadKeyedSloppyArguments(receiver, key, &miss);
  assembler->Return(result);

  assembler->Bind(&miss);
  {
    assembler->Comment("Miss");
    assembler->TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver,
                               key, slot, vector);
  }
}

void KeyedStoreSloppyArgumentsStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* key = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label miss(assembler);

  assembler->StoreKeyedSloppyArguments(receiver, key, value, &miss);
  assembler->Return(value);

  assembler->Bind(&miss);
  {
    assembler->Comment("Miss");
    assembler->TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value,
                               slot, vector, receiver, key);
  }
}

void LoadScriptContextFieldStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  assembler->Comment("LoadScriptContextFieldStub: context_index=%d, slot=%d",
                     context_index(), slot_index());

  Node* context = assembler->Parameter(Descriptor::kContext);

  Node* script_context = assembler->LoadScriptContext(context, context_index());
  Node* result = assembler->LoadFixedArrayElement(
      script_context, assembler->IntPtrConstant(slot_index()), 0,
      CodeStubAssembler::INTPTR_PARAMETERS);
  assembler->Return(result);
}

void StoreScriptContextFieldStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  assembler->Comment("StoreScriptContextFieldStub: context_index=%d, slot=%d",
                     context_index(), slot_index());

  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Node* script_context = assembler->LoadScriptContext(context, context_index());
  assembler->StoreFixedArrayElement(
      script_context, assembler->IntPtrConstant(slot_index()), value,
      UPDATE_WRITE_BARRIER, CodeStubAssembler::INTPTR_PARAMETERS);
  assembler->Return(value);
}

void StoreInterceptorStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* name = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* context = assembler->Parameter(Descriptor::kContext);
  assembler->TailCallRuntime(Runtime::kStorePropertyWithInterceptor, context,
                             receiver, name, value);
}

void LoadIndexedInterceptorStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* key = assembler->Parameter(Descriptor::kName);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label if_keyispositivesmi(assembler), if_keyisinvalid(assembler);
  assembler->Branch(assembler->WordIsPositiveSmi(key), &if_keyispositivesmi,
                    &if_keyisinvalid);
  assembler->Bind(&if_keyispositivesmi);
  assembler->TailCallRuntime(Runtime::kLoadElementWithInterceptor, context,
                             receiver, key);

  assembler->Bind(&if_keyisinvalid);
  assembler->TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key,
                             slot, vector);
}

// static
bool FastCloneShallowObjectStub::IsSupported(ObjectLiteral* expr) {
  // FastCloneShallowObjectStub doesn't copy elements, and object literals don't
  // support copy-on-write (COW) elements for now.
  // TODO(mvstanton): make object literals support COW elements.
  return expr->fast_elements() && expr->has_shallow_properties() &&
         expr->properties_count() <= kMaximumClonedProperties;
}

// static
int FastCloneShallowObjectStub::PropertiesCount(int literal_length) {
  // This heuristic of setting empty literals to have
  // kInitialGlobalObjectUnusedPropertiesCount must remain in-sync with the
  // runtime.
  // TODO(verwaest): Unify this with the heuristic in the runtime.
  return literal_length == 0
             ? JSObject::kInitialGlobalObjectUnusedPropertiesCount
             : literal_length;
}

// static
compiler::Node* FastCloneShallowObjectStub::GenerateFastPath(
    CodeStubAssembler* assembler, compiler::CodeAssembler::Label* call_runtime,
    compiler::Node* closure, compiler::Node* literals_index,
    compiler::Node* properties_count) {
  typedef compiler::Node Node;
  typedef compiler::CodeAssembler::Label Label;
  typedef compiler::CodeAssembler::Variable Variable;

  Node* literals_array =
      assembler->LoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* allocation_site = assembler->LoadFixedArrayElement(
      literals_array, literals_index,
      LiteralsArray::kFirstLiteralIndex * kPointerSize,
      CodeStubAssembler::SMI_PARAMETERS);
  assembler->GotoIf(assembler->IsUndefined(allocation_site), call_runtime);

  // Calculate the object and allocation size based on the properties count.
  Node* object_size = assembler->IntPtrAdd(
      assembler->WordShl(properties_count, kPointerSizeLog2),
      assembler->IntPtrConstant(JSObject::kHeaderSize));
  Node* allocation_size = object_size;
  if (FLAG_allocation_site_pretenuring) {
    allocation_size = assembler->IntPtrAdd(
        object_size, assembler->IntPtrConstant(AllocationMemento::kSize));
  }
  Node* boilerplate = assembler->LoadObjectField(
      allocation_site, AllocationSite::kTransitionInfoOffset);
  Node* boilerplate_map = assembler->LoadMap(boilerplate);
  Node* instance_size = assembler->LoadMapInstanceSize(boilerplate_map);
  Node* size_in_words = assembler->WordShr(object_size, kPointerSizeLog2);
  assembler->GotoUnless(assembler->Word32Equal(instance_size, size_in_words),
                        call_runtime);

  Node* copy = assembler->Allocate(allocation_size);

  // Copy boilerplate elements.
  Variable offset(assembler, MachineType::PointerRepresentation());
  offset.Bind(assembler->IntPtrConstant(-kHeapObjectTag));
  Node* end_offset = assembler->IntPtrAdd(object_size, offset.value());
  Label loop_body(assembler, &offset), loop_check(assembler, &offset);
  // We should always have an object size greater than zero.
  assembler->Goto(&loop_body);
  assembler->Bind(&loop_body);
  {
    // The Allocate above guarantees that the copy lies in new space. This
    // allows us to skip write barriers. This is necessary since we may also be
    // copying unboxed doubles.
    Node* field =
        assembler->Load(MachineType::IntPtr(), boilerplate, offset.value());
    assembler->StoreNoWriteBarrier(MachineType::PointerRepresentation(), copy,
                                   offset.value(), field);
    assembler->Goto(&loop_check);
  }
  assembler->Bind(&loop_check);
  {
    offset.Bind(assembler->IntPtrAdd(offset.value(),
                                     assembler->IntPtrConstant(kPointerSize)));
    assembler->GotoUnless(
        assembler->IntPtrGreaterThanOrEqual(offset.value(), end_offset),
        &loop_body);
  }

  if (FLAG_allocation_site_pretenuring) {
    Node* memento = assembler->InnerAllocate(copy, object_size);
    assembler->StoreObjectFieldNoWriteBarrier(
        memento, HeapObject::kMapOffset,
        assembler->LoadRoot(Heap::kAllocationMementoMapRootIndex));
    assembler->StoreObjectFieldNoWriteBarrier(
        memento, AllocationMemento::kAllocationSiteOffset, allocation_site);
    Node* memento_create_count = assembler->LoadObjectField(
        allocation_site, AllocationSite::kPretenureCreateCountOffset);
    memento_create_count = assembler->SmiAdd(
        memento_create_count, assembler->SmiConstant(Smi::FromInt(1)));
    assembler->StoreObjectFieldNoWriteBarrier(
        allocation_site, AllocationSite::kPretenureCreateCountOffset,
        memento_create_count);
  }

  // TODO(verwaest): Allocate and fill in double boxes.
  return copy;
}

void FastCloneShallowObjectStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  Label call_runtime(assembler);
  Node* closure = assembler->Parameter(0);
  Node* literals_index = assembler->Parameter(1);

  Node* properties_count =
      assembler->IntPtrConstant(PropertiesCount(this->length()));
  Node* copy = GenerateFastPath(assembler, &call_runtime, closure,
                                literals_index, properties_count);
  assembler->Return(copy);

  assembler->Bind(&call_runtime);
  Node* constant_properties = assembler->Parameter(2);
  Node* flags = assembler->Parameter(3);
  Node* context = assembler->Parameter(4);
  assembler->TailCallRuntime(Runtime::kCreateObjectLiteral, context, closure,
                             literals_index, constant_properties, flags);
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

void CallICStub::PrintState(std::ostream& os) const {  // NOLINT
  os << state();
}


void JSEntryStub::FinishCode(Handle<Code> code) {
  Handle<FixedArray> handler_table =
      code->GetIsolate()->factory()->NewFixedArray(1, TENURED);
  handler_table->set(0, Smi::FromInt(handler_offset_));
  code->set_handler_table(*handler_table);
}


void LoadDictionaryElementStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      FUNCTION_ADDR(Runtime_KeyedLoadIC_MissFromStubFailure));
}

void HandlerStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  DCHECK(kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC);
  if (kind() == Code::KEYED_LOAD_IC) {
    descriptor->Initialize(
        FUNCTION_ADDR(Runtime_KeyedLoadIC_MissFromStubFailure));
  }
}


CallInterfaceDescriptor HandlerStub::GetCallInterfaceDescriptor() const {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    return LoadWithVectorDescriptor(isolate());
  } else {
    DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
    return StoreWithVectorDescriptor(isolate());
  }
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


#define SIMD128_INIT_DESC(TYPE, Type, type, lane_count, lane_type) \
  void Allocate##Type##Stub::InitializeDescriptor(                 \
      CodeStubDescriptor* descriptor) {                            \
    descriptor->Initialize(                                        \
        Runtime::FunctionForId(Runtime::kCreate##Type)->entry);    \
  }
SIMD128_TYPES(SIMD128_INIT_DESC)
#undef SIMD128_INIT_DESC

void ToBooleanICStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(Runtime_ToBooleanIC_Miss));
  descriptor->SetMissHandler(Runtime::kToBooleanIC_Miss);
}


void BinaryOpICStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(Runtime_BinaryOpIC_Miss));
  descriptor->SetMissHandler(Runtime::kBinaryOpIC_Miss);
}


void BinaryOpWithAllocationSiteStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      FUNCTION_ADDR(Runtime_BinaryOpIC_MissWithAllocationSite));
}

void GetPropertyStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;

  Label call_runtime(assembler, Label::kDeferred), return_undefined(assembler),
      end(assembler);

  Node* object = assembler->Parameter(0);
  Node* key = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);
  Variable var_result(assembler, MachineRepresentation::kTagged);

  CodeStubAssembler::LookupInHolder lookup_property_in_holder =
      [assembler, context, &var_result, &end](
          Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* unique_name, Label* next_holder,
          Label* if_bailout) {
        Variable var_value(assembler, MachineRepresentation::kTagged);
        Label if_found(assembler);
        assembler->TryGetOwnProperty(
            context, receiver, holder, holder_map, holder_instance_type,
            unique_name, &if_found, &var_value, next_holder, if_bailout);
        assembler->Bind(&if_found);
        {
          var_result.Bind(var_value.value());
          assembler->Goto(&end);
        }
      };

  CodeStubAssembler::LookupInHolder lookup_element_in_holder =
      [assembler, context, &var_result, &end](
          Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* index, Label* next_holder,
          Label* if_bailout) {
        // Not supported yet.
        assembler->Use(next_holder);
        assembler->Goto(if_bailout);
      };

  assembler->TryPrototypeChainLookup(object, key, lookup_property_in_holder,
                                     lookup_element_in_holder,
                                     &return_undefined, &call_runtime);

  assembler->Bind(&return_undefined);
  {
    var_result.Bind(assembler->UndefinedConstant());
    assembler->Goto(&end);
  }

  assembler->Bind(&call_runtime);
  {
    var_result.Bind(
        assembler->CallRuntime(Runtime::kGetProperty, context, object, key));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  assembler->Return(var_result.value());
}

// static
compiler::Node* FastNewClosureStub::Generate(CodeStubAssembler* assembler,
                                             compiler::Node* shared_info,
                                             compiler::Node* context) {
  typedef compiler::Node Node;
  typedef compiler::CodeAssembler::Label Label;
  typedef compiler::CodeAssembler::Variable Variable;

  Isolate* isolate = assembler->isolate();
  Factory* factory = assembler->isolate()->factory();
  assembler->IncrementCounter(isolate->counters()->fast_new_closure_total(), 1);

  // Create a new closure from the given function info in new space
  Node* result = assembler->Allocate(JSFunction::kSize);

  // Calculate the index of the map we should install on the function based on
  // the FunctionKind and LanguageMode of the function.
  // Note: Must be kept in sync with Context::FunctionMapIndex
  Node* compiler_hints = assembler->LoadObjectField(
      shared_info, SharedFunctionInfo::kCompilerHintsOffset,
      MachineType::Uint32());
  Node* is_strict = assembler->Word32And(
      compiler_hints,
      assembler->Int32Constant(1 << SharedFunctionInfo::kStrictModeBit));

  Label if_normal(assembler), if_generator(assembler), if_async(assembler),
      if_class_constructor(assembler), if_function_without_prototype(assembler),
      load_map(assembler);
  Variable map_index(assembler, MachineType::PointerRepresentation());

  STATIC_ASSERT(FunctionKind::kNormalFunction == 0);
  Node* is_not_normal = assembler->Word32And(
      compiler_hints,
      assembler->Int32Constant(SharedFunctionInfo::kAllFunctionKindBitsMask));
  assembler->GotoUnless(is_not_normal, &if_normal);

  Node* is_generator = assembler->Word32And(
      compiler_hints,
      assembler->Int32Constant(FunctionKind::kGeneratorFunction
                               << SharedFunctionInfo::kFunctionKindShift));
  assembler->GotoIf(is_generator, &if_generator);

  Node* is_async = assembler->Word32And(
      compiler_hints,
      assembler->Int32Constant(FunctionKind::kAsyncFunction
                               << SharedFunctionInfo::kFunctionKindShift));
  assembler->GotoIf(is_async, &if_async);

  Node* is_class_constructor = assembler->Word32And(
      compiler_hints,
      assembler->Int32Constant(FunctionKind::kClassConstructor
                               << SharedFunctionInfo::kFunctionKindShift));
  assembler->GotoIf(is_class_constructor, &if_class_constructor);

  if (FLAG_debug_code) {
    // Function must be a function without a prototype.
    CSA_ASSERT(assembler, assembler->Word32And(
                              compiler_hints,
                              assembler->Int32Constant(
                                  (FunctionKind::kAccessorFunction |
                                   FunctionKind::kArrowFunction |
                                   FunctionKind::kConciseMethod)
                                  << SharedFunctionInfo::kFunctionKindShift)));
  }
  assembler->Goto(&if_function_without_prototype);

  assembler->Bind(&if_normal);
  {
    map_index.Bind(assembler->Select(
        is_strict,
        assembler->IntPtrConstant(Context::STRICT_FUNCTION_MAP_INDEX),
        assembler->IntPtrConstant(Context::SLOPPY_FUNCTION_MAP_INDEX)));
    assembler->Goto(&load_map);
  }

  assembler->Bind(&if_generator);
  {
    map_index.Bind(assembler->Select(
        is_strict,
        assembler->IntPtrConstant(Context::STRICT_GENERATOR_FUNCTION_MAP_INDEX),
        assembler->IntPtrConstant(
            Context::SLOPPY_GENERATOR_FUNCTION_MAP_INDEX)));
    assembler->Goto(&load_map);
  }

  assembler->Bind(&if_async);
  {
    map_index.Bind(assembler->Select(
        is_strict,
        assembler->IntPtrConstant(Context::STRICT_ASYNC_FUNCTION_MAP_INDEX),
        assembler->IntPtrConstant(Context::SLOPPY_ASYNC_FUNCTION_MAP_INDEX)));
    assembler->Goto(&load_map);
  }

  assembler->Bind(&if_class_constructor);
  {
    map_index.Bind(
        assembler->IntPtrConstant(Context::STRICT_FUNCTION_MAP_INDEX));
    assembler->Goto(&load_map);
  }

  assembler->Bind(&if_function_without_prototype);
  {
    map_index.Bind(assembler->IntPtrConstant(
        Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX));
    assembler->Goto(&load_map);
  }

  assembler->Bind(&load_map);

  // Get the function map in the current native context and set that
  // as the map of the allocated object.
  Node* native_context = assembler->LoadNativeContext(context);
  Node* map_slot_value =
      assembler->LoadFixedArrayElement(native_context, map_index.value(), 0,
                                       CodeStubAssembler::INTPTR_PARAMETERS);
  assembler->StoreMapNoWriteBarrier(result, map_slot_value);

  // Initialize the rest of the function.
  Node* empty_fixed_array =
      assembler->HeapConstant(factory->empty_fixed_array());
  Node* empty_literals_array =
      assembler->HeapConstant(factory->empty_literals_array());
  assembler->StoreObjectFieldNoWriteBarrier(result, JSObject::kPropertiesOffset,
                                            empty_fixed_array);
  assembler->StoreObjectFieldNoWriteBarrier(result, JSObject::kElementsOffset,
                                            empty_fixed_array);
  assembler->StoreObjectFieldNoWriteBarrier(result, JSFunction::kLiteralsOffset,
                                            empty_literals_array);
  assembler->StoreObjectFieldNoWriteBarrier(
      result, JSFunction::kPrototypeOrInitialMapOffset,
      assembler->TheHoleConstant());
  assembler->StoreObjectFieldNoWriteBarrier(
      result, JSFunction::kSharedFunctionInfoOffset, shared_info);
  assembler->StoreObjectFieldNoWriteBarrier(result, JSFunction::kContextOffset,
                                            context);
  Handle<Code> lazy_builtin_handle(
      assembler->isolate()->builtins()->builtin(Builtins::kCompileLazy));
  Node* lazy_builtin = assembler->HeapConstant(lazy_builtin_handle);
  Node* lazy_builtin_entry = assembler->IntPtrAdd(
      lazy_builtin,
      assembler->IntPtrConstant(Code::kHeaderSize - kHeapObjectTag));
  assembler->StoreObjectFieldNoWriteBarrier(
      result, JSFunction::kCodeEntryOffset, lazy_builtin_entry);
  assembler->StoreObjectFieldNoWriteBarrier(result,
                                            JSFunction::kNextFunctionLinkOffset,
                                            assembler->UndefinedConstant());

  return result;
}

void FastNewClosureStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  assembler->Return(
      Generate(assembler, assembler->Parameter(0), assembler->Parameter(1)));
}

// static
compiler::Node* FastNewFunctionContextStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* function,
    compiler::Node* slots, compiler::Node* context) {
  typedef compiler::Node Node;

  Node* min_context_slots =
      assembler->Int32Constant(Context::MIN_CONTEXT_SLOTS);
  Node* length = assembler->Int32Add(slots, min_context_slots);
  Node* size = assembler->Int32Add(
      assembler->Word32Shl(length, assembler->Int32Constant(kPointerSizeLog2)),
      assembler->Int32Constant(FixedArray::kHeaderSize));

  // Create a new closure from the given function info in new space
  Node* function_context = assembler->Allocate(size);

  Isolate* isolate = assembler->isolate();
  assembler->StoreMapNoWriteBarrier(
      function_context,
      assembler->HeapConstant(isolate->factory()->function_context_map()));
  assembler->StoreObjectFieldNoWriteBarrier(function_context,
                                            Context::kLengthOffset,
                                            assembler->SmiFromWord32(length));

  // Set up the fixed slots.
  assembler->StoreFixedArrayElement(
      function_context, assembler->Int32Constant(Context::CLOSURE_INDEX),
      function, SKIP_WRITE_BARRIER);
  assembler->StoreFixedArrayElement(
      function_context, assembler->Int32Constant(Context::PREVIOUS_INDEX),
      context, SKIP_WRITE_BARRIER);
  assembler->StoreFixedArrayElement(
      function_context, assembler->Int32Constant(Context::EXTENSION_INDEX),
      assembler->TheHoleConstant(), SKIP_WRITE_BARRIER);

  // Copy the native context from the previous context.
  Node* native_context = assembler->LoadNativeContext(context);
  assembler->StoreFixedArrayElement(
      function_context, assembler->Int32Constant(Context::NATIVE_CONTEXT_INDEX),
      native_context, SKIP_WRITE_BARRIER);

  // Initialize the rest of the slots to undefined.
  Node* undefined = assembler->UndefinedConstant();
  assembler->BuildFastFixedArrayForEach(
      function_context, FAST_ELEMENTS, min_context_slots, length,
      [undefined](CodeStubAssembler* assembler, Node* context, Node* offset) {
        assembler->StoreNoWriteBarrier(MachineType::PointerRepresentation(),
                                       context, offset, undefined);
      });

  return function_context;
}

void FastNewFunctionContextStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* function = assembler->Parameter(Descriptor::kFunction);
  Node* slots = assembler->Parameter(FastNewFunctionContextDescriptor::kSlots);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(Generate(assembler, function, slots, context));
}

// static
compiler::Node* FastCloneRegExpStub::Generate(CodeStubAssembler* assembler,
                                              compiler::Node* closure,
                                              compiler::Node* literal_index,
                                              compiler::Node* pattern,
                                              compiler::Node* flags,
                                              compiler::Node* context) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  typedef compiler::Node Node;

  Label call_runtime(assembler, Label::kDeferred), end(assembler);

  Variable result(assembler, MachineRepresentation::kTagged);

  Node* literals_array =
      assembler->LoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* boilerplate = assembler->LoadFixedArrayElement(
      literals_array, literal_index,
      LiteralsArray::kFirstLiteralIndex * kPointerSize,
      CodeStubAssembler::SMI_PARAMETERS);
  assembler->GotoIf(assembler->IsUndefined(boilerplate), &call_runtime);

  {
    int size = JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
    Node* copy = assembler->Allocate(size);
    for (int offset = 0; offset < size; offset += kPointerSize) {
      Node* value = assembler->LoadObjectField(boilerplate, offset);
      assembler->StoreObjectFieldNoWriteBarrier(copy, offset, value);
    }
    result.Bind(copy);
    assembler->Goto(&end);
  }

  assembler->Bind(&call_runtime);
  {
    result.Bind(assembler->CallRuntime(Runtime::kCreateRegExpLiteral, context,
                                       closure, literal_index, pattern, flags));
    assembler->Goto(&end);
  }

  assembler->Bind(&end);
  return result.value();
}

void FastCloneRegExpStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* closure = assembler->Parameter(Descriptor::kClosure);
  Node* literal_index = assembler->Parameter(Descriptor::kLiteralIndex);
  Node* pattern = assembler->Parameter(Descriptor::kPattern);
  Node* flags = assembler->Parameter(Descriptor::kFlags);
  Node* context = assembler->Parameter(Descriptor::kContext);

  assembler->Return(
      Generate(assembler, closure, literal_index, pattern, flags, context));
}

namespace {

compiler::Node* NonEmptyShallowClone(CodeStubAssembler* assembler,
                                     compiler::Node* boilerplate,
                                     compiler::Node* boilerplate_map,
                                     compiler::Node* boilerplate_elements,
                                     compiler::Node* allocation_site,
                                     compiler::Node* capacity,
                                     ElementsKind kind) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::ParameterMode ParameterMode;

  ParameterMode param_mode = CodeStubAssembler::SMI_PARAMETERS;

  Node* length = assembler->LoadJSArrayLength(boilerplate);

  if (assembler->Is64()) {
    capacity = assembler->SmiUntag(capacity);
    param_mode = CodeStubAssembler::INTEGER_PARAMETERS;
  }

  Node *array, *elements;
  std::tie(array, elements) =
      assembler->AllocateUninitializedJSArrayWithElements(
          kind, boilerplate_map, length, allocation_site, capacity, param_mode);

  assembler->Comment("copy elements header");
  for (int offset = 0; offset < FixedArrayBase::kHeaderSize;
       offset += kPointerSize) {
    Node* value = assembler->LoadObjectField(boilerplate_elements, offset);
    assembler->StoreObjectField(elements, offset, value);
  }

  if (assembler->Is64()) {
    length = assembler->SmiUntag(length);
  }

  assembler->Comment("copy boilerplate elements");
  assembler->CopyFixedArrayElements(kind, boilerplate_elements, elements,
                                    length, SKIP_WRITE_BARRIER, param_mode);
  assembler->IncrementCounter(
      assembler->isolate()->counters()->inlined_copied_elements(), 1);

  return array;
}

}  // namespace

// static
compiler::Node* FastCloneShallowArrayStub::Generate(
    CodeStubAssembler* assembler, compiler::Node* closure,
    compiler::Node* literal_index, compiler::Node* context,
    CodeStubAssembler::Label* call_runtime,
    AllocationSiteMode allocation_site_mode) {
  typedef CodeStubAssembler::Label Label;
  typedef CodeStubAssembler::Variable Variable;
  typedef compiler::Node Node;

  Label zero_capacity(assembler), cow_elements(assembler),
      fast_elements(assembler), return_result(assembler);
  Variable result(assembler, MachineRepresentation::kTagged);

  Node* literals_array =
      assembler->LoadObjectField(closure, JSFunction::kLiteralsOffset);
  Node* allocation_site = assembler->LoadFixedArrayElement(
      literals_array, literal_index,
      LiteralsArray::kFirstLiteralIndex * kPointerSize,
      CodeStubAssembler::SMI_PARAMETERS);

  assembler->GotoIf(assembler->IsUndefined(allocation_site), call_runtime);
  allocation_site = assembler->LoadFixedArrayElement(
      literals_array, literal_index,
      LiteralsArray::kFirstLiteralIndex * kPointerSize,
      CodeStubAssembler::SMI_PARAMETERS);

  Node* boilerplate = assembler->LoadObjectField(
      allocation_site, AllocationSite::kTransitionInfoOffset);
  Node* boilerplate_map = assembler->LoadMap(boilerplate);
  Node* boilerplate_elements = assembler->LoadElements(boilerplate);
  Node* capacity = assembler->LoadFixedArrayBaseLength(boilerplate_elements);
  allocation_site =
      allocation_site_mode == TRACK_ALLOCATION_SITE ? allocation_site : nullptr;

  Node* zero = assembler->SmiConstant(Smi::kZero);
  assembler->GotoIf(assembler->SmiEqual(capacity, zero), &zero_capacity);

  Node* elements_map = assembler->LoadMap(boilerplate_elements);
  assembler->GotoIf(assembler->IsFixedCOWArrayMap(elements_map), &cow_elements);

  assembler->GotoIf(assembler->IsFixedArrayMap(elements_map), &fast_elements);
  {
    assembler->Comment("fast double elements path");
    if (FLAG_debug_code) {
      Label correct_elements_map(assembler), abort(assembler, Label::kDeferred);
      assembler->Branch(assembler->IsFixedDoubleArrayMap(elements_map),
                        &correct_elements_map, &abort);

      assembler->Bind(&abort);
      {
        Node* abort_id = assembler->SmiConstant(
            Smi::FromInt(BailoutReason::kExpectedFixedDoubleArrayMap));
        assembler->CallRuntime(Runtime::kAbort, context, abort_id);
        result.Bind(assembler->UndefinedConstant());
        assembler->Goto(&return_result);
      }
      assembler->Bind(&correct_elements_map);
    }

    Node* array = NonEmptyShallowClone(assembler, boilerplate, boilerplate_map,
                                       boilerplate_elements, allocation_site,
                                       capacity, FAST_DOUBLE_ELEMENTS);
    result.Bind(array);
    assembler->Goto(&return_result);
  }

  assembler->Bind(&fast_elements);
  {
    assembler->Comment("fast elements path");
    Node* array = NonEmptyShallowClone(assembler, boilerplate, boilerplate_map,
                                       boilerplate_elements, allocation_site,
                                       capacity, FAST_ELEMENTS);
    result.Bind(array);
    assembler->Goto(&return_result);
  }

  Variable length(assembler, MachineRepresentation::kTagged),
      elements(assembler, MachineRepresentation::kTagged);
  Label allocate_without_elements(assembler);

  assembler->Bind(&cow_elements);
  {
    assembler->Comment("fixed cow path");
    length.Bind(assembler->LoadJSArrayLength(boilerplate));
    elements.Bind(boilerplate_elements);

    assembler->Goto(&allocate_without_elements);
  }

  assembler->Bind(&zero_capacity);
  {
    assembler->Comment("zero capacity path");
    length.Bind(zero);
    elements.Bind(assembler->LoadRoot(Heap::kEmptyFixedArrayRootIndex));

    assembler->Goto(&allocate_without_elements);
  }

  assembler->Bind(&allocate_without_elements);
  {
    Node* array = assembler->AllocateUninitializedJSArrayWithoutElements(
        FAST_ELEMENTS, boilerplate_map, length.value(), allocation_site);
    assembler->StoreObjectField(array, JSObject::kElementsOffset,
                                elements.value());
    result.Bind(array);
    assembler->Goto(&return_result);
  }

  assembler->Bind(&return_result);
  return result.value();
}

void FastCloneShallowArrayStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;
  Node* closure = assembler->Parameter(Descriptor::kClosure);
  Node* literal_index = assembler->Parameter(Descriptor::kLiteralIndex);
  Node* constant_elements = assembler->Parameter(Descriptor::kConstantElements);
  Node* context = assembler->Parameter(Descriptor::kContext);
  Label call_runtime(assembler, Label::kDeferred);
  assembler->Return(Generate(assembler, closure, literal_index, context,
                             &call_runtime, allocation_site_mode()));

  assembler->Bind(&call_runtime);
  {
    assembler->Comment("call runtime");
    Node* flags = assembler->SmiConstant(
        Smi::FromInt(ArrayLiteral::kShallowElements |
                     (allocation_site_mode() == TRACK_ALLOCATION_SITE
                          ? 0
                          : ArrayLiteral::kDisableMementos)));
    assembler->Return(assembler->CallRuntime(Runtime::kCreateArrayLiteral,
                                             context, closure, literal_index,
                                             constant_elements, flags));
  }
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
  DCHECK_EQ(DICTIONARY_ELEMENTS, elements_kind());
  KeyedStoreIC::GenerateSlow(masm);
}

void StoreFastElementStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  assembler->Comment(
      "StoreFastElementStub: js_array=%d, elements_kind=%s, store_mode=%d",
      is_js_array(), ElementsKindToString(elements_kind()), store_mode());

  Node* receiver = assembler->Parameter(Descriptor::kReceiver);
  Node* key = assembler->Parameter(Descriptor::kName);
  Node* value = assembler->Parameter(Descriptor::kValue);
  Node* slot = assembler->Parameter(Descriptor::kSlot);
  Node* vector = assembler->Parameter(Descriptor::kVector);
  Node* context = assembler->Parameter(Descriptor::kContext);

  Label miss(assembler);

  assembler->EmitElementStore(receiver, key, value, is_js_array(),
                              elements_kind(), store_mode(), &miss);
  assembler->Return(value);

  assembler->Bind(&miss);
  {
    assembler->Comment("Miss");
    assembler->TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value,
                               slot, vector, receiver, key);
  }
}

// static
void StoreFastElementStub::GenerateAheadOfTime(Isolate* isolate) {
  if (FLAG_minimal) return;
  StoreFastElementStub(isolate, false, FAST_HOLEY_ELEMENTS, STANDARD_STORE)
      .GetCode();
  StoreFastElementStub(isolate, false, FAST_HOLEY_ELEMENTS,
                       STORE_AND_GROW_NO_TRANSITION).GetCode();
  for (int i = FIRST_FAST_ELEMENTS_KIND; i <= LAST_FAST_ELEMENTS_KIND; i++) {
    ElementsKind kind = static_cast<ElementsKind>(i);
    StoreFastElementStub(isolate, true, kind, STANDARD_STORE).GetCode();
    StoreFastElementStub(isolate, true, kind, STORE_AND_GROW_NO_TRANSITION)
        .GetCode();
  }
}

bool ToBooleanICStub::UpdateStatus(Handle<Object> object) {
  ToBooleanHints old_hints = hints();
  ToBooleanHints new_hints = old_hints;
  bool to_boolean_value = false;  // Dummy initialization.
  if (object->IsUndefined(isolate())) {
    new_hints |= ToBooleanHint::kUndefined;
    to_boolean_value = false;
  } else if (object->IsBoolean()) {
    new_hints |= ToBooleanHint::kBoolean;
    to_boolean_value = object->IsTrue(isolate());
  } else if (object->IsNull(isolate())) {
    new_hints |= ToBooleanHint::kNull;
    to_boolean_value = false;
  } else if (object->IsSmi()) {
    new_hints |= ToBooleanHint::kSmallInteger;
    to_boolean_value = Smi::cast(*object)->value() != 0;
  } else if (object->IsJSReceiver()) {
    new_hints |= ToBooleanHint::kReceiver;
    to_boolean_value = !object->IsUndetectable();
  } else if (object->IsString()) {
    DCHECK(!object->IsUndetectable());
    new_hints |= ToBooleanHint::kString;
    to_boolean_value = String::cast(*object)->length() != 0;
  } else if (object->IsSymbol()) {
    new_hints |= ToBooleanHint::kSymbol;
    to_boolean_value = true;
  } else if (object->IsHeapNumber()) {
    DCHECK(!object->IsUndetectable());
    new_hints |= ToBooleanHint::kHeapNumber;
    double value = HeapNumber::cast(*object)->value();
    to_boolean_value = value != 0 && !std::isnan(value);
  } else if (object->IsSimd128Value()) {
    new_hints |= ToBooleanHint::kSimdValue;
    to_boolean_value = true;
  } else {
    // We should never see an internal object at runtime here!
    UNREACHABLE();
    to_boolean_value = true;
  }
  TraceTransition(old_hints, new_hints);
  set_sub_minor_key(HintsBits::update(sub_minor_key(), new_hints));
  return to_boolean_value;
}

void ToBooleanICStub::PrintState(std::ostream& os) const {  // NOLINT
  os << hints();
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

void CreateAllocationSiteStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  assembler->Return(assembler->CreateAllocationSiteInFeedbackVector(
      assembler->Parameter(Descriptor::kVector),
      assembler->Parameter(Descriptor::kSlot)));
}

void CreateWeakCellStub::GenerateAssembly(CodeStubAssembler* assembler) const {
  assembler->Return(assembler->CreateWeakCellInFeedbackVector(
      assembler->Parameter(Descriptor::kVector),
      assembler->Parameter(Descriptor::kSlot),
      assembler->Parameter(Descriptor::kValue)));
}

void ArrayNoArgumentConstructorStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* native_context = assembler->LoadObjectField(
      assembler->Parameter(Descriptor::kFunction), JSFunction::kContextOffset);
  bool track_allocation_site =
      AllocationSite::GetMode(elements_kind()) == TRACK_ALLOCATION_SITE &&
      override_mode() != DISABLE_ALLOCATION_SITES;
  Node* allocation_site =
      track_allocation_site ? assembler->Parameter(Descriptor::kAllocationSite)
                            : nullptr;
  Node* array_map =
      assembler->LoadJSArrayElementsMap(elements_kind(), native_context);
  Node* array = assembler->AllocateJSArray(
      elements_kind(), array_map,
      assembler->IntPtrConstant(JSArray::kPreallocatedArrayElements),
      assembler->SmiConstant(Smi::kZero), allocation_site);
  assembler->Return(array);
}

void InternalArrayNoArgumentConstructorStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* array_map =
      assembler->LoadObjectField(assembler->Parameter(Descriptor::kFunction),
                                 JSFunction::kPrototypeOrInitialMapOffset);
  Node* array = assembler->AllocateJSArray(
      elements_kind(), array_map,
      assembler->IntPtrConstant(JSArray::kPreallocatedArrayElements),
      assembler->SmiConstant(Smi::kZero), nullptr);
  assembler->Return(array);
}

namespace {

template <typename Descriptor>
void SingleArgumentConstructorCommon(CodeStubAssembler* assembler,
                                     ElementsKind elements_kind,
                                     compiler::Node* array_map,
                                     compiler::Node* allocation_site,
                                     AllocationSiteMode mode) {
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Label Label;

  Label ok(assembler);
  Label smi_size(assembler);
  Label small_smi_size(assembler);
  Label call_runtime(assembler, Label::kDeferred);

  Node* size = assembler->Parameter(Descriptor::kArraySizeSmiParameter);
  assembler->Branch(assembler->TaggedIsSmi(size), &smi_size, &call_runtime);

  assembler->Bind(&smi_size);

  if (IsFastPackedElementsKind(elements_kind)) {
    Label abort(assembler, Label::kDeferred);
    assembler->Branch(
        assembler->SmiEqual(size, assembler->SmiConstant(Smi::kZero)),
        &small_smi_size, &abort);

    assembler->Bind(&abort);
    Node* reason =
        assembler->SmiConstant(Smi::FromInt(kAllocatingNonEmptyPackedArray));
    Node* context = assembler->Parameter(Descriptor::kContext);
    assembler->TailCallRuntime(Runtime::kAbort, context, reason);
  } else {
    int element_size =
        IsFastDoubleElementsKind(elements_kind) ? kDoubleSize : kPointerSize;
    int max_fast_elements =
        (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - JSArray::kSize -
         AllocationMemento::kSize) /
        element_size;
    assembler->Branch(
        assembler->SmiAboveOrEqual(
            size, assembler->SmiConstant(Smi::FromInt(max_fast_elements))),
        &call_runtime, &small_smi_size);
  }

  assembler->Bind(&small_smi_size);
  {
    Node* array = assembler->AllocateJSArray(
        elements_kind, array_map, size, size,
        mode == DONT_TRACK_ALLOCATION_SITE ? nullptr : allocation_site,
        CodeStubAssembler::SMI_PARAMETERS);
    assembler->Return(array);
  }

  assembler->Bind(&call_runtime);
  {
    Node* context = assembler->Parameter(Descriptor::kContext);
    Node* function = assembler->Parameter(Descriptor::kFunction);
    Node* array_size = assembler->Parameter(Descriptor::kArraySizeSmiParameter);
    Node* allocation_site = assembler->Parameter(Descriptor::kAllocationSite);
    assembler->TailCallRuntime(Runtime::kNewArray, context, function,
                               array_size, function, allocation_site);
  }
}
}  // namespace

void ArraySingleArgumentConstructorStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* function = assembler->Parameter(Descriptor::kFunction);
  Node* native_context =
      assembler->LoadObjectField(function, JSFunction::kContextOffset);
  Node* array_map =
      assembler->LoadJSArrayElementsMap(elements_kind(), native_context);
  AllocationSiteMode mode = override_mode() == DISABLE_ALLOCATION_SITES
                                ? DONT_TRACK_ALLOCATION_SITE
                                : AllocationSite::GetMode(elements_kind());
  Node* allocation_site = assembler->Parameter(Descriptor::kAllocationSite);
  SingleArgumentConstructorCommon<Descriptor>(assembler, elements_kind(),
                                              array_map, allocation_site, mode);
}

void InternalArraySingleArgumentConstructorStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* function = assembler->Parameter(Descriptor::kFunction);
  Node* array_map = assembler->LoadObjectField(
      function, JSFunction::kPrototypeOrInitialMapOffset);
  SingleArgumentConstructorCommon<Descriptor>(
      assembler, elements_kind(), array_map, assembler->UndefinedConstant(),
      DONT_TRACK_ALLOCATION_SITE);
}

void GrowArrayElementsStub::GenerateAssembly(
    CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  CodeStubAssembler::Label runtime(assembler,
                                   CodeStubAssembler::Label::kDeferred);

  Node* object = assembler->Parameter(Descriptor::kObject);
  Node* key = assembler->Parameter(Descriptor::kKey);
  Node* context = assembler->Parameter(Descriptor::kContext);
  ElementsKind kind = elements_kind();

  Node* elements = assembler->LoadElements(object);
  Node* new_elements =
      assembler->TryGrowElementsCapacity(object, elements, kind, key, &runtime);
  assembler->Return(new_elements);

  assembler->Bind(&runtime);
  // TODO(danno): Make this a tail call when the stub is only used from TurboFan
  // code. This musn't be a tail call for now, since the caller site in lithium
  // creates a safepoint. This safepoint musn't have a different number of
  // arguments on the stack in the case that a GC happens from the slow-case
  // allocation path (zero, since all the stubs inputs are in registers) and
  // when the call happens (it would be two in the tail call case due to the
  // tail call pushing the arguments on the stack for the runtime call). By not
  // tail-calling, the runtime call case also has zero arguments on the stack
  // for the stub frame.
  assembler->Return(assembler->CallRuntime(Runtime::kGrowArrayElements, context,
                                           object, key));
}

ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate)
    : PlatformCodeStub(isolate) {}

InternalArrayConstructorStub::InternalArrayConstructorStub(Isolate* isolate)
    : PlatformCodeStub(isolate) {}

}  // namespace internal
}  // namespace v8
