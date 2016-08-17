// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include <sstream>

#include "src/bootstrapper.h"
#include "src/code-factory.h"
#include "src/compiler/code-stub-assembler.h"
#include "src/factory.h"
#include "src/gdb-jit.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/parsing/parser.h"
#include "src/profiler/cpu-profiler.h"

namespace v8 {
namespace internal {


RUNTIME_FUNCTION(UnexpectedStubMiss) {
  FATAL("Unexpected deopt of a stub");
  return Smi::FromInt(0);
}


CodeStubDescriptor::CodeStubDescriptor(CodeStub* stub)
    : call_descriptor_(stub->GetCallInterfaceDescriptor()),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(NULL),
      miss_handler_(),
      has_miss_handler_(false) {
  stub->InitializeDescriptor(this);
}


CodeStubDescriptor::CodeStubDescriptor(Isolate* isolate, uint32_t stub_key)
    : stack_parameter_count_(no_reg),
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
          CodeCreateEvent(Logger::STUB_TAG, AbstractCode::cast(*code),
                          os.str().c_str()));
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
  return Code::ComputeFlags(GetCodeKind(), GetICState(), GetExtraICState(),
                            GetStubType());
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


std::ostream& operator<<(std::ostream& os, const StringAddFlags& flags) {
  switch (flags) {
    case STRING_ADD_CHECK_NONE:
      return os << "CheckNone";
    case STRING_ADD_CHECK_LEFT:
      return os << "CheckLeft";
    case STRING_ADD_CHECK_RIGHT:
      return os << "CheckRight";
    case STRING_ADD_CHECK_BOTH:
      return os << "CheckBoth";
    case STRING_ADD_CONVERT_LEFT:
      return os << "ConvertLeft";
    case STRING_ADD_CONVERT_RIGHT:
      return os << "ConvertRight";
    case STRING_ADD_CONVERT:
      break;
  }
  UNREACHABLE();
  return os;
}


void StringAddStub::PrintBaseName(std::ostream& os) const {  // NOLINT
  os << "StringAddStub_" << flags() << "_" << pretenure_flag();
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
  Zone zone(isolate()->allocator());
  CallInterfaceDescriptor descriptor(GetCallInterfaceDescriptor());
  compiler::CodeStubAssembler assembler(isolate(), &zone, descriptor,
                                        GetCodeFlags(), name);
  GenerateAssembly(&assembler);
  return assembler.GenerateCode();
}

void AllocateHeapNumberStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* result = assembler->AllocateHeapNumber();
  assembler->Return(result);
}

void AllocateMutableHeapNumberStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;

  Node* result = assembler->Allocate(HeapNumber::kSize);
  assembler->StoreMapNoWriteBarrier(
      result,
      assembler->HeapConstant(isolate()->factory()->mutable_heap_number_map()));
  assembler->Return(result);
}

#define SIMD128_GEN_ASM(TYPE, Type, type, lane_count, lane_type)            \
  void Allocate##Type##Stub::GenerateAssembly(                              \
      compiler::CodeStubAssembler* assembler) const {                       \
    compiler::Node* result = assembler->Allocate(                           \
        Simd128Value::kSize, compiler::CodeStubAssembler::kNone);           \
    compiler::Node* map_offset =                                            \
        assembler->IntPtrConstant(HeapObject::kMapOffset - kHeapObjectTag); \
    compiler::Node* map = assembler->IntPtrAdd(result, map_offset);         \
    assembler->StoreNoWriteBarrier(                                         \
        MachineRepresentation::kTagged, map,                                \
        assembler->HeapConstant(isolate()->factory()->type##_map()));       \
    assembler->Return(result);                                              \
  }
SIMD128_TYPES(SIMD128_GEN_ASM)
#undef SIMD128_GEN_ASM

void StringLengthStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  compiler::Node* value = assembler->Parameter(0);
  compiler::Node* string =
      assembler->LoadObjectField(value, JSValue::kValueOffset);
  compiler::Node* result =
      assembler->LoadObjectField(string, String::kLengthOffset);
  assembler->Return(result);
}

void AddStub::GenerateAssembly(compiler::CodeStubAssembler* assembler) const {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(2);

  // Shared entry for floating point addition.
  Label do_fadd(assembler);
  Variable var_fadd_lhs(assembler, MachineRepresentation::kFloat64),
      var_fadd_rhs(assembler, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive, ToString and/or
  // ToNumber conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_vars);
  var_lhs.Bind(assembler->Parameter(0));
  var_rhs.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
    assembler->Branch(assembler->WordIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    assembler->Bind(&if_lhsissmi);
    {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      {
        // Try fast Smi addition first.
        Node* pair = assembler->SmiAddWithOverflow(lhs, rhs);
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
        assembler->Return(assembler->Projection(0, pair));
      }

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = assembler->LoadObjectField(rhs, HeapObject::kMapOffset);

        // Check if the {rhs} is a HeapNumber.
        Label if_rhsisnumber(assembler),
            if_rhsisnotnumber(assembler, Label::kDeferred);
        Node* number_map = assembler->HeapNumberMapConstant();
        assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                          &if_rhsisnumber, &if_rhsisnotnumber);

        assembler->Bind(&if_rhsisnumber);
        {
          var_fadd_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fadd_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
          assembler->Goto(&do_fadd);
        }

        assembler->Bind(&if_rhsisnotnumber);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = assembler->LoadMapInstanceType(rhs_map);

          // Check if the {rhs} is a String.
          Label if_rhsisstring(assembler, Label::kDeferred),
              if_rhsisnotstring(assembler, Label::kDeferred);
          assembler->Branch(assembler->Int32LessThan(
                                rhs_instance_type,
                                assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                            &if_rhsisstring, &if_rhsisnotstring);

          assembler->Bind(&if_rhsisstring);
          {
            // Convert {lhs}, which is a Smi, to a String and concatenate the
            // resulting string with the String {rhs}.
            Callable callable = CodeFactory::StringAdd(
                assembler->isolate(), STRING_ADD_CONVERT_LEFT, NOT_TENURED);
            assembler->TailCallStub(callable, context, lhs, rhs);
          }

          assembler->Bind(&if_rhsisnotstring);
          {
            // Check if {rhs} is a JSReceiver.
            Label if_rhsisreceiver(assembler, Label::kDeferred),
                if_rhsisnotreceiver(assembler, Label::kDeferred);
            assembler->Branch(
                assembler->Int32LessThanOrEqual(
                    assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                    rhs_instance_type),
                &if_rhsisreceiver, &if_rhsisnotreceiver);

            assembler->Bind(&if_rhsisreceiver);
            {
              // Convert {rhs} to a primitive first passing no hint.
              // TODO(bmeurer): Hook up ToPrimitiveStub here, once it's there.
              var_rhs.Bind(
                  assembler->CallRuntime(Runtime::kToPrimitive, context, rhs));
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_rhsisnotreceiver);
            {
              // Convert {rhs} to a Number first.
              Callable callable =
                  CodeFactory::NonNumberToNumber(assembler->isolate());
              var_rhs.Bind(assembler->CallStub(callable, context, rhs));
              assembler->Goto(&loop);
            }
          }
        }
      }
    }

    assembler->Bind(&if_lhsisnotsmi);
    {
      // Load the map and instance type of {lhs}.
      Node* lhs_instance_type = assembler->LoadInstanceType(lhs);

      // Check if {lhs} is a String.
      Label if_lhsisstring(assembler), if_lhsisnotstring(assembler);
      assembler->Branch(assembler->Int32LessThan(
                            lhs_instance_type,
                            assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                        &if_lhsisstring, &if_lhsisnotstring);

      assembler->Bind(&if_lhsisstring);
      {
        // Convert {rhs} to a String (using the sequence of ToPrimitive with
        // no hint followed by ToString) and concatenate the strings.
        Callable callable = CodeFactory::StringAdd(
            assembler->isolate(), STRING_ADD_CONVERT_RIGHT, NOT_TENURED);
        assembler->TailCallStub(callable, context, lhs, rhs);
      }

      assembler->Bind(&if_lhsisnotstring);
      {
        // Check if {rhs} is a Smi.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        {
          // Check if {lhs} is a Number.
          Label if_lhsisnumber(assembler),
              if_lhsisnotnumber(assembler, Label::kDeferred);
          assembler->Branch(assembler->Word32Equal(
                                lhs_instance_type,
                                assembler->Int32Constant(HEAP_NUMBER_TYPE)),
                            &if_lhsisnumber, &if_lhsisnotnumber);

          assembler->Bind(&if_lhsisnumber);
          {
            // The {lhs} is a HeapNumber, the {rhs} is a Smi, just add them.
            var_fadd_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
            var_fadd_rhs.Bind(assembler->SmiToFloat64(rhs));
            assembler->Goto(&do_fadd);
          }

          assembler->Bind(&if_lhsisnotnumber);
          {
            // The {lhs} is neither a Number nor a String, and the {rhs} is a
            // Smi.
            Label if_lhsisreceiver(assembler, Label::kDeferred),
                if_lhsisnotreceiver(assembler, Label::kDeferred);
            assembler->Branch(
                assembler->Int32LessThanOrEqual(
                    assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                    lhs_instance_type),
                &if_lhsisreceiver, &if_lhsisnotreceiver);

            assembler->Bind(&if_lhsisreceiver);
            {
              // Convert {lhs} to a primitive first passing no hint.
              // TODO(bmeurer): Hook up ToPrimitiveStub here, once it's there.
              var_lhs.Bind(
                  assembler->CallRuntime(Runtime::kToPrimitive, context, lhs));
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_lhsisnotreceiver);
            {
              // Convert {lhs} to a Number first.
              Callable callable =
                  CodeFactory::NonNumberToNumber(assembler->isolate());
              var_lhs.Bind(assembler->CallStub(callable, context, lhs));
              assembler->Goto(&loop);
            }
          }
        }

        assembler->Bind(&if_rhsisnotsmi);
        {
          // Load the instance type of {rhs}.
          Node* rhs_instance_type = assembler->LoadInstanceType(rhs);

          // Check if {rhs} is a String.
          Label if_rhsisstring(assembler), if_rhsisnotstring(assembler);
          assembler->Branch(assembler->Int32LessThan(
                                rhs_instance_type,
                                assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                            &if_rhsisstring, &if_rhsisnotstring);

          assembler->Bind(&if_rhsisstring);
          {
            // Convert {lhs} to a String (using the sequence of ToPrimitive with
            // no hint followed by ToString) and concatenate the strings.
            Callable callable = CodeFactory::StringAdd(
                assembler->isolate(), STRING_ADD_CONVERT_LEFT, NOT_TENURED);
            assembler->TailCallStub(callable, context, lhs, rhs);
          }

          assembler->Bind(&if_rhsisnotstring);
          {
            // Check if {lhs} is a HeapNumber.
            Label if_lhsisnumber(assembler), if_lhsisnotnumber(assembler);
            assembler->Branch(assembler->Word32Equal(
                                  lhs_instance_type,
                                  assembler->Int32Constant(HEAP_NUMBER_TYPE)),
                              &if_lhsisnumber, &if_lhsisnotnumber);

            assembler->Bind(&if_lhsisnumber);
            {
              // Check if {rhs} is also a HeapNumber.
              Label if_rhsisnumber(assembler),
                  if_rhsisnotnumber(assembler, Label::kDeferred);
              assembler->Branch(assembler->Word32Equal(
                                    rhs_instance_type,
                                    assembler->Int32Constant(HEAP_NUMBER_TYPE)),
                                &if_rhsisnumber, &if_rhsisnotnumber);

              assembler->Bind(&if_rhsisnumber);
              {
                // Perform a floating point addition.
                var_fadd_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
                var_fadd_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
                assembler->Goto(&do_fadd);
              }

              assembler->Bind(&if_rhsisnotnumber);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(assembler, Label::kDeferred),
                    if_rhsisnotreceiver(assembler, Label::kDeferred);
                assembler->Branch(
                    assembler->Int32LessThanOrEqual(
                        assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                        rhs_instance_type),
                    &if_rhsisreceiver, &if_rhsisnotreceiver);

                assembler->Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  // TODO(bmeurer): Hook up ToPrimitiveStub here too.
                  var_rhs.Bind(assembler->CallRuntime(Runtime::kToPrimitive,
                                                      context, rhs));
                  assembler->Goto(&loop);
                }

                assembler->Bind(&if_rhsisnotreceiver);
                {
                  // Convert {rhs} to a Number first.
                  Callable callable =
                      CodeFactory::NonNumberToNumber(assembler->isolate());
                  var_rhs.Bind(assembler->CallStub(callable, context, rhs));
                  assembler->Goto(&loop);
                }
              }
            }

            assembler->Bind(&if_lhsisnotnumber);
            {
              // Check if {lhs} is a JSReceiver.
              Label if_lhsisreceiver(assembler, Label::kDeferred),
                  if_lhsisnotreceiver(assembler);
              assembler->Branch(
                  assembler->Int32LessThanOrEqual(
                      assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                      lhs_instance_type),
                  &if_lhsisreceiver, &if_lhsisnotreceiver);

              assembler->Bind(&if_lhsisreceiver);
              {
                // Convert {lhs} to a primitive first passing no hint.
                // TODO(bmeurer): Hook up ToPrimitiveStub here, once it's there.
                var_lhs.Bind(assembler->CallRuntime(Runtime::kToPrimitive,
                                                    context, lhs));
                assembler->Goto(&loop);
              }

              assembler->Bind(&if_lhsisnotreceiver);
              {
                // Check if {rhs} is a JSReceiver.
                Label if_rhsisreceiver(assembler, Label::kDeferred),
                    if_rhsisnotreceiver(assembler, Label::kDeferred);
                assembler->Branch(
                    assembler->Int32LessThanOrEqual(
                        assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                        rhs_instance_type),
                    &if_rhsisreceiver, &if_rhsisnotreceiver);

                assembler->Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first passing no hint.
                  // TODO(bmeurer): Hook up ToPrimitiveStub here too.
                  var_rhs.Bind(assembler->CallRuntime(Runtime::kToPrimitive,
                                                      context, rhs));
                  assembler->Goto(&loop);
                }

                assembler->Bind(&if_rhsisnotreceiver);
                {
                  // Convert {lhs} to a Number first.
                  Callable callable =
                      CodeFactory::NonNumberToNumber(assembler->isolate());
                  var_lhs.Bind(assembler->CallStub(callable, context, lhs));
                  assembler->Goto(&loop);
                }
              }
            }
          }
        }
      }
    }
  }

  assembler->Bind(&do_fadd);
  {
    Node* lhs_value = var_fadd_lhs.value();
    Node* rhs_value = var_fadd_rhs.value();
    Node* value = assembler->Float64Add(lhs_value, rhs_value);
    Node* result = assembler->ChangeFloat64ToTagged(value);
    assembler->Return(result);
  }
}

void SubtractStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(2);

  // Shared entry for floating point subtraction.
  Label do_fsub(assembler);
  Variable var_fsub_lhs(assembler, MachineRepresentation::kFloat64),
      var_fsub_rhs(assembler, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_vars);
  var_lhs.Bind(assembler->Parameter(0));
  var_rhs.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
    assembler->Branch(assembler->WordIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    assembler->Bind(&if_lhsissmi);
    {
      // Check if the {rhs} is also a Smi.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      {
        // Try a fast Smi subtraction first.
        Node* pair = assembler->SmiSubWithOverflow(lhs, rhs);
        Node* overflow = assembler->Projection(1, pair);

        // Check if the Smi subtraction overflowed.
        Label if_overflow(assembler), if_notoverflow(assembler);
        assembler->Branch(overflow, &if_overflow, &if_notoverflow);

        assembler->Bind(&if_overflow);
        {
          // The result doesn't fit into Smi range.
          var_fsub_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fsub_rhs.Bind(assembler->SmiToFloat64(rhs));
          assembler->Goto(&do_fsub);
        }

        assembler->Bind(&if_notoverflow);
        assembler->Return(assembler->Projection(0, pair));
      }

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = assembler->LoadMap(rhs);

        // Check if {rhs} is a HeapNumber.
        Label if_rhsisnumber(assembler),
            if_rhsisnotnumber(assembler, Label::kDeferred);
        Node* number_map = assembler->HeapNumberMapConstant();
        assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                          &if_rhsisnumber, &if_rhsisnotnumber);

        assembler->Bind(&if_rhsisnumber);
        {
          // Perform a floating point subtraction.
          var_fsub_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fsub_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
          assembler->Goto(&do_fsub);
        }

        assembler->Bind(&if_rhsisnotnumber);
        {
          // Convert the {rhs} to a Number first.
          Callable callable = CodeFactory::NonNumberToNumber(isolate());
          var_rhs.Bind(assembler->CallStub(callable, context, rhs));
          assembler->Goto(&loop);
        }
      }
    }

    assembler->Bind(&if_lhsisnotsmi);
    {
      // Load the map of the {lhs}.
      Node* lhs_map = assembler->LoadMap(lhs);

      // Check if the {lhs} is a HeapNumber.
      Label if_lhsisnumber(assembler),
          if_lhsisnotnumber(assembler, Label::kDeferred);
      Node* number_map = assembler->HeapNumberMapConstant();
      assembler->Branch(assembler->WordEqual(lhs_map, number_map),
                        &if_lhsisnumber, &if_lhsisnotnumber);

      assembler->Bind(&if_lhsisnumber);
      {
        // Check if the {rhs} is a Smi.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
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
          Label if_rhsisnumber(assembler),
              if_rhsisnotnumber(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                            &if_rhsisnumber, &if_rhsisnotnumber);

          assembler->Bind(&if_rhsisnumber);
          {
            // Perform a floating point subtraction.
            var_fsub_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
            var_fsub_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
            assembler->Goto(&do_fsub);
          }

          assembler->Bind(&if_rhsisnotnumber);
          {
            // Convert the {rhs} to a Number first.
            Callable callable = CodeFactory::NonNumberToNumber(isolate());
            var_rhs.Bind(assembler->CallStub(callable, context, rhs));
            assembler->Goto(&loop);
          }
        }
      }

      assembler->Bind(&if_lhsisnotnumber);
      {
        // Convert the {lhs} to a Number first.
        Callable callable = CodeFactory::NonNumberToNumber(isolate());
        var_lhs.Bind(assembler->CallStub(callable, context, lhs));
        assembler->Goto(&loop);
      }
    }
  }

  assembler->Bind(&do_fsub);
  {
    Node* lhs_value = var_fsub_lhs.value();
    Node* rhs_value = var_fsub_rhs.value();
    Node* value = assembler->Float64Sub(lhs_value, rhs_value);
    Node* result = assembler->ChangeFloat64ToTagged(value);
    assembler->Return(result);
  }
}

void BitwiseAndStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  using compiler::Node;

  Node* lhs = assembler->Parameter(0);
  Node* rhs = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);
  Node* lhs_value = assembler->TruncateTaggedToWord32(context, lhs);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, rhs);
  Node* value = assembler->Word32And(lhs_value, rhs_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void BitwiseOrStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  using compiler::Node;

  Node* lhs = assembler->Parameter(0);
  Node* rhs = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);
  Node* lhs_value = assembler->TruncateTaggedToWord32(context, lhs);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, rhs);
  Node* value = assembler->Word32Or(lhs_value, rhs_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

void BitwiseXorStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  using compiler::Node;

  Node* lhs = assembler->Parameter(0);
  Node* rhs = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);
  Node* lhs_value = assembler->TruncateTaggedToWord32(context, lhs);
  Node* rhs_value = assembler->TruncateTaggedToWord32(context, rhs);
  Node* value = assembler->Word32Xor(lhs_value, rhs_value);
  Node* result = assembler->ChangeInt32ToTagged(value);
  assembler->Return(result);
}

namespace {

enum RelationalComparisonMode {
  kLessThan,
  kLessThanOrEqual,
  kGreaterThan,
  kGreaterThanOrEqual
};

void GenerateAbstractRelationalComparison(
    compiler::CodeStubAssembler* assembler, RelationalComparisonMode mode) {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(2);

  Label return_true(assembler), return_false(assembler);

  // Shared entry for floating point comparison.
  Label do_fcmp(assembler);
  Variable var_fcmp_lhs(assembler, MachineRepresentation::kFloat64),
      var_fcmp_rhs(assembler, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_vars);
  var_lhs.Bind(assembler->Parameter(0));
  var_rhs.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if the {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
    assembler->Branch(assembler->WordIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    assembler->Bind(&if_lhsissmi);
    {
      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      {
        // Both {lhs} and {rhs} are Smi, so just perform a fast Smi comparison.
        switch (mode) {
          case kLessThan:
            assembler->BranchIfSmiLessThan(lhs, rhs, &return_true,
                                           &return_false);
            break;
          case kLessThanOrEqual:
            assembler->BranchIfSmiLessThanOrEqual(lhs, rhs, &return_true,
                                                  &return_false);
            break;
          case kGreaterThan:
            assembler->BranchIfSmiLessThan(rhs, lhs, &return_true,
                                           &return_false);
            break;
          case kGreaterThanOrEqual:
            assembler->BranchIfSmiLessThanOrEqual(rhs, lhs, &return_true,
                                                  &return_false);
            break;
        }
      }

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = assembler->LoadMap(rhs);

        // Check if the {rhs} is a HeapNumber.
        Node* number_map = assembler->HeapNumberMapConstant();
        Label if_rhsisnumber(assembler),
            if_rhsisnotnumber(assembler, Label::kDeferred);
        assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                          &if_rhsisnumber, &if_rhsisnotnumber);

        assembler->Bind(&if_rhsisnumber);
        {
          // Convert the {lhs} and {rhs} to floating point values, and
          // perform a floating point comparison.
          var_fcmp_lhs.Bind(assembler->SmiToFloat64(lhs));
          var_fcmp_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
          assembler->Goto(&do_fcmp);
        }

        assembler->Bind(&if_rhsisnotnumber);
        {
          // Convert the {rhs} to a Number; we don't need to perform the
          // dedicated ToPrimitive(rhs, hint Number) operation, as the
          // ToNumber(rhs) will by itself already invoke ToPrimitive with
          // a Number hint.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_rhs.Bind(assembler->CallStub(callable, context, rhs));
          assembler->Goto(&loop);
        }
      }
    }

    assembler->Bind(&if_lhsisnotsmi);
    {
      // Load the HeapNumber map for later comparisons.
      Node* number_map = assembler->HeapNumberMapConstant();

      // Load the map of {lhs}.
      Node* lhs_map = assembler->LoadMap(lhs);

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      {
        // Check if the {lhs} is a HeapNumber.
        Label if_lhsisnumber(assembler),
            if_lhsisnotnumber(assembler, Label::kDeferred);
        assembler->Branch(assembler->WordEqual(lhs_map, number_map),
                          &if_lhsisnumber, &if_lhsisnotnumber);

        assembler->Bind(&if_lhsisnumber);
        {
          // Convert the {lhs} and {rhs} to floating point values, and
          // perform a floating point comparison.
          var_fcmp_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
          var_fcmp_rhs.Bind(assembler->SmiToFloat64(rhs));
          assembler->Goto(&do_fcmp);
        }

        assembler->Bind(&if_lhsisnotnumber);
        {
          // Convert the {lhs} to a Number; we don't need to perform the
          // dedicated ToPrimitive(lhs, hint Number) operation, as the
          // ToNumber(lhs) will by itself already invoke ToPrimitive with
          // a Number hint.
          Callable callable =
              CodeFactory::NonNumberToNumber(assembler->isolate());
          var_lhs.Bind(assembler->CallStub(callable, context, lhs));
          assembler->Goto(&loop);
        }
      }

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of {rhs}.
        Node* rhs_map = assembler->LoadMap(rhs);

        // Check if {lhs} is a HeapNumber.
        Label if_lhsisnumber(assembler), if_lhsisnotnumber(assembler);
        assembler->Branch(assembler->WordEqual(lhs_map, number_map),
                          &if_lhsisnumber, &if_lhsisnotnumber);

        assembler->Bind(&if_lhsisnumber);
        {
          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(assembler),
              if_rhsisnotnumber(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(lhs_map, rhs_map),
                            &if_rhsisnumber, &if_rhsisnotnumber);

          assembler->Bind(&if_rhsisnumber);
          {
            // Convert the {lhs} and {rhs} to floating point values, and
            // perform a floating point comparison.
            var_fcmp_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
            var_fcmp_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
            assembler->Goto(&do_fcmp);
          }

          assembler->Bind(&if_rhsisnotnumber);
          {
            // Convert the {rhs} to a Number; we don't need to perform
            // dedicated ToPrimitive(rhs, hint Number) operation, as the
            // ToNumber(rhs) will by itself already invoke ToPrimitive with
            // a Number hint.
            Callable callable =
                CodeFactory::NonNumberToNumber(assembler->isolate());
            var_rhs.Bind(assembler->CallStub(callable, context, rhs));
            assembler->Goto(&loop);
          }
        }

        assembler->Bind(&if_lhsisnotnumber);
        {
          // Load the instance type of {lhs}.
          Node* lhs_instance_type = assembler->LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(assembler),
              if_lhsisnotstring(assembler, Label::kDeferred);
          assembler->Branch(assembler->Int32LessThan(
                                lhs_instance_type,
                                assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                            &if_lhsisstring, &if_lhsisnotstring);

          assembler->Bind(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = assembler->LoadMapInstanceType(rhs_map);

            // Check if {rhs} is also a String.
            Label if_rhsisstring(assembler),
                if_rhsisnotstring(assembler, Label::kDeferred);
            assembler->Branch(assembler->Int32LessThan(
                                  rhs_instance_type, assembler->Int32Constant(
                                                         FIRST_NONSTRING_TYPE)),
                              &if_rhsisstring, &if_rhsisnotstring);

            assembler->Bind(&if_rhsisstring);
            {
              // Both {lhs} and {rhs} are strings.
              switch (mode) {
                case kLessThan:
                  assembler->TailCallStub(
                      CodeFactory::StringLessThan(assembler->isolate()),
                      context, lhs, rhs);
                  break;
                case kLessThanOrEqual:
                  assembler->TailCallStub(
                      CodeFactory::StringLessThanOrEqual(assembler->isolate()),
                      context, lhs, rhs);
                  break;
                case kGreaterThan:
                  assembler->TailCallStub(
                      CodeFactory::StringGreaterThan(assembler->isolate()),
                      context, lhs, rhs);
                  break;
                case kGreaterThanOrEqual:
                  assembler->TailCallStub(CodeFactory::StringGreaterThanOrEqual(
                                              assembler->isolate()),
                                          context, lhs, rhs);
                  break;
              }
            }

            assembler->Bind(&if_rhsisnotstring);
            {
              // The {lhs} is a String, while {rhs} is neither a Number nor a
              // String, so we need to call ToPrimitive(rhs, hint Number) if
              // {rhs} is a receiver or ToNumber(lhs) and ToNumber(rhs) in the
              // other cases.
              STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
              Label if_rhsisreceiver(assembler, Label::kDeferred),
                  if_rhsisnotreceiver(assembler, Label::kDeferred);
              assembler->Branch(
                  assembler->Int32LessThanOrEqual(
                      assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                      rhs_instance_type),
                  &if_rhsisreceiver, &if_rhsisnotreceiver);

              assembler->Bind(&if_rhsisreceiver);
              {
                // Convert {rhs} to a primitive first passing Number hint.
                // TODO(bmeurer): Hook up ToPrimitiveStub here, once it's there.
                var_rhs.Bind(assembler->CallRuntime(
                    Runtime::kToPrimitive_Number, context, rhs));
                assembler->Goto(&loop);
              }

              assembler->Bind(&if_rhsisnotreceiver);
              {
                // Convert both {lhs} and {rhs} to Number.
                Callable callable = CodeFactory::ToNumber(assembler->isolate());
                var_lhs.Bind(assembler->CallStub(callable, context, lhs));
                var_rhs.Bind(assembler->CallStub(callable, context, rhs));
                assembler->Goto(&loop);
              }
            }
          }

          assembler->Bind(&if_lhsisnotstring);
          {
            // The {lhs} is neither a Number nor a String, so we need to call
            // ToPrimitive(lhs, hint Number) if {lhs} is a receiver or
            // ToNumber(lhs) and ToNumber(rhs) in the other cases.
            STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
            Label if_lhsisreceiver(assembler, Label::kDeferred),
                if_lhsisnotreceiver(assembler, Label::kDeferred);
            assembler->Branch(
                assembler->Int32LessThanOrEqual(
                    assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                    lhs_instance_type),
                &if_lhsisreceiver, &if_lhsisnotreceiver);

            assembler->Bind(&if_lhsisreceiver);
            {
              // Convert {lhs} to a primitive first passing Number hint.
              // TODO(bmeurer): Hook up ToPrimitiveStub here, once it's there.
              var_lhs.Bind(assembler->CallRuntime(Runtime::kToPrimitive_Number,
                                                  context, lhs));
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_lhsisnotreceiver);
            {
              // Convert both {lhs} and {rhs} to Number.
              Callable callable = CodeFactory::ToNumber(assembler->isolate());
              var_lhs.Bind(assembler->CallStub(callable, context, lhs));
              var_rhs.Bind(assembler->CallStub(callable, context, rhs));
              assembler->Goto(&loop);
            }
          }
        }
      }
    }
  }

  assembler->Bind(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    switch (mode) {
      case kLessThan:
        assembler->BranchIfFloat64LessThan(lhs, rhs, &return_true,
                                           &return_false);
        break;
      case kLessThanOrEqual:
        assembler->BranchIfFloat64LessThanOrEqual(lhs, rhs, &return_true,
                                                  &return_false);
        break;
      case kGreaterThan:
        assembler->BranchIfFloat64GreaterThan(lhs, rhs, &return_true,
                                              &return_false);
        break;
      case kGreaterThanOrEqual:
        assembler->BranchIfFloat64GreaterThanOrEqual(lhs, rhs, &return_true,
                                                     &return_false);
        break;
    }
  }

  assembler->Bind(&return_true);
  assembler->Return(assembler->BooleanConstant(true));

  assembler->Bind(&return_false);
  assembler->Return(assembler->BooleanConstant(false));
}

enum ResultMode { kDontNegateResult, kNegateResult };

void GenerateEqual_Same(compiler::CodeStubAssembler* assembler,
                        compiler::Node* value,
                        compiler::CodeStubAssembler::Label* if_equal,
                        compiler::CodeStubAssembler::Label* if_notequal) {
  // In case of abstract or strict equality checks, we need additional checks
  // for NaN values because they are not considered equal, even if both the
  // left and the right hand side reference exactly the same value.
  // TODO(bmeurer): This seems to violate the SIMD.js specification, but it
  // seems to be what is tested in the current SIMD.js testsuite.

  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  // Check if {value} is a Smi or a HeapObject.
  Label if_valueissmi(assembler), if_valueisnotsmi(assembler);
  assembler->Branch(assembler->WordIsSmi(value), &if_valueissmi,
                    &if_valueisnotsmi);

  assembler->Bind(&if_valueisnotsmi);
  {
    // Load the map of {value}.
    Node* value_map = assembler->LoadMap(value);

    // Check if {value} (and therefore {rhs}) is a HeapNumber.
    Node* number_map = assembler->HeapNumberMapConstant();
    Label if_valueisnumber(assembler), if_valueisnotnumber(assembler);
    assembler->Branch(assembler->WordEqual(value_map, number_map),
                      &if_valueisnumber, &if_valueisnotnumber);

    assembler->Bind(&if_valueisnumber);
    {
      // Convert {value} (and therefore {rhs}) to floating point value.
      Node* value_value = assembler->LoadHeapNumberValue(value);

      // Check if the HeapNumber value is a NaN.
      assembler->BranchIfFloat64IsNaN(value_value, if_notequal, if_equal);
    }

    assembler->Bind(&if_valueisnotnumber);
    assembler->Goto(if_equal);
  }

  assembler->Bind(&if_valueissmi);
  assembler->Goto(if_equal);
}

void GenerateEqual_Simd128Value_HeapObject(
    compiler::CodeStubAssembler* assembler, compiler::Node* lhs,
    compiler::Node* lhs_map, compiler::Node* rhs, compiler::Node* rhs_map,
    compiler::CodeStubAssembler::Label* if_equal,
    compiler::CodeStubAssembler::Label* if_notequal) {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  // Check if {lhs} and {rhs} have the same map.
  Label if_mapsame(assembler), if_mapnotsame(assembler);
  assembler->Branch(assembler->WordEqual(lhs_map, rhs_map), &if_mapsame,
                    &if_mapnotsame);

  assembler->Bind(&if_mapsame);
  {
    // Both {lhs} and {rhs} are Simd128Values with the same map, need special
    // handling for Float32x4 because of NaN comparisons.
    Label if_float32x4(assembler), if_notfloat32x4(assembler);
    Node* float32x4_map =
        assembler->HeapConstant(assembler->factory()->float32x4_map());
    assembler->Branch(assembler->WordEqual(lhs_map, float32x4_map),
                      &if_float32x4, &if_notfloat32x4);

    assembler->Bind(&if_float32x4);
    {
      // Both {lhs} and {rhs} are Float32x4, compare the lanes individually
      // using a floating point comparison.
      for (int offset = Float32x4::kValueOffset - kHeapObjectTag;
           offset < Float32x4::kSize - kHeapObjectTag;
           offset += sizeof(float)) {
        // Load the floating point values for {lhs} and {rhs}.
        Node* lhs_value = assembler->Load(MachineType::Float32(), lhs,
                                          assembler->IntPtrConstant(offset));
        Node* rhs_value = assembler->Load(MachineType::Float32(), rhs,
                                          assembler->IntPtrConstant(offset));

        // Perform a floating point comparison.
        Label if_valueequal(assembler), if_valuenotequal(assembler);
        assembler->Branch(assembler->Float32Equal(lhs_value, rhs_value),
                          &if_valueequal, &if_valuenotequal);
        assembler->Bind(&if_valuenotequal);
        assembler->Goto(if_notequal);
        assembler->Bind(&if_valueequal);
      }

      // All 4 lanes match, {lhs} and {rhs} considered equal.
      assembler->Goto(if_equal);
    }

    assembler->Bind(&if_notfloat32x4);
    {
      // For other Simd128Values we just perform a bitwise comparison.
      for (int offset = Simd128Value::kValueOffset - kHeapObjectTag;
           offset < Simd128Value::kSize - kHeapObjectTag;
           offset += kPointerSize) {
        // Load the word values for {lhs} and {rhs}.
        Node* lhs_value = assembler->Load(MachineType::Pointer(), lhs,
                                          assembler->IntPtrConstant(offset));
        Node* rhs_value = assembler->Load(MachineType::Pointer(), rhs,
                                          assembler->IntPtrConstant(offset));

        // Perform a bitwise word-comparison.
        Label if_valueequal(assembler), if_valuenotequal(assembler);
        assembler->Branch(assembler->WordEqual(lhs_value, rhs_value),
                          &if_valueequal, &if_valuenotequal);
        assembler->Bind(&if_valuenotequal);
        assembler->Goto(if_notequal);
        assembler->Bind(&if_valueequal);
      }

      // Bitwise comparison succeeded, {lhs} and {rhs} considered equal.
      assembler->Goto(if_equal);
    }
  }

  assembler->Bind(&if_mapnotsame);
  assembler->Goto(if_notequal);
}

// ES6 section 7.2.12 Abstract Equality Comparison
void GenerateEqual(compiler::CodeStubAssembler* assembler, ResultMode mode) {
  // This is a slightly optimized version of Object::Equals represented as
  // scheduled TurboFan graph utilizing the CodeStubAssembler. Whenever you
  // change something functionality wise in here, remember to update the
  // Object::Equals method as well.
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(2);

  Label if_equal(assembler), if_notequal(assembler);

  // Shared entry for floating point comparison.
  Label do_fcmp(assembler);
  Variable var_fcmp_lhs(assembler, MachineRepresentation::kFloat64),
      var_fcmp_rhs(assembler, MachineRepresentation::kFloat64);

  // We might need to loop several times due to ToPrimitive and/or ToNumber
  // conversions.
  Variable var_lhs(assembler, MachineRepresentation::kTagged),
      var_rhs(assembler, MachineRepresentation::kTagged);
  Variable* loop_vars[2] = {&var_lhs, &var_rhs};
  Label loop(assembler, 2, loop_vars);
  var_lhs.Bind(assembler->Parameter(0));
  var_rhs.Bind(assembler->Parameter(1));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Load the current {lhs} and {rhs} values.
    Node* lhs = var_lhs.value();
    Node* rhs = var_rhs.value();

    // Check if {lhs} and {rhs} refer to the same object.
    Label if_same(assembler), if_notsame(assembler);
    assembler->Branch(assembler->WordEqual(lhs, rhs), &if_same, &if_notsame);

    assembler->Bind(&if_same);
    {
      // The {lhs} and {rhs} reference the exact same value, yet we need special
      // treatment for HeapNumber, as NaN is not equal to NaN.
      GenerateEqual_Same(assembler, lhs, &if_equal, &if_notequal);
    }

    assembler->Bind(&if_notsame);
    {
      // Check if {lhs} is a Smi or a HeapObject.
      Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
      assembler->Branch(assembler->WordIsSmi(lhs), &if_lhsissmi,
                        &if_lhsisnotsmi);

      assembler->Bind(&if_lhsissmi);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        assembler->Goto(&if_notequal);

        assembler->Bind(&if_rhsisnotsmi);
        {
          // Load the map of {rhs}.
          Node* rhs_map = assembler->LoadMap(rhs);

          // Check if {rhs} is a HeapNumber.
          Node* number_map = assembler->HeapNumberMapConstant();
          Label if_rhsisnumber(assembler),
              if_rhsisnotnumber(assembler, Label::kDeferred);
          assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                            &if_rhsisnumber, &if_rhsisnotnumber);

          assembler->Bind(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values, and
            // perform a floating point comparison.
            var_fcmp_lhs.Bind(assembler->SmiToFloat64(lhs));
            var_fcmp_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
            assembler->Goto(&do_fcmp);
          }

          assembler->Bind(&if_rhsisnotnumber);
          {
            // Load the instance type of the {rhs}.
            Node* rhs_instance_type = assembler->LoadMapInstanceType(rhs_map);

            // Check if the {rhs} is a String.
            Label if_rhsisstring(assembler, Label::kDeferred),
                if_rhsisnotstring(assembler, Label::kDeferred);
            assembler->Branch(assembler->Int32LessThan(
                                  rhs_instance_type, assembler->Int32Constant(
                                                         FIRST_NONSTRING_TYPE)),
                              &if_rhsisstring, &if_rhsisnotstring);

            assembler->Bind(&if_rhsisstring);
            {
              // Convert the {rhs} to a Number.
              Callable callable =
                  CodeFactory::StringToNumber(assembler->isolate());
              var_rhs.Bind(assembler->CallStub(callable, context, rhs));
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_rhsisnotstring);
            {
              // Check if the {rhs} is a Boolean.
              Node* boolean_map = assembler->BooleanMapConstant();
              Label if_rhsisboolean(assembler, Label::kDeferred),
                  if_rhsisnotboolean(assembler, Label::kDeferred);
              assembler->Branch(assembler->WordEqual(rhs_map, boolean_map),
                                &if_rhsisboolean, &if_rhsisnotboolean);

              assembler->Bind(&if_rhsisboolean);
              {
                // The {rhs} is a Boolean, load its number value.
                var_rhs.Bind(
                    assembler->LoadObjectField(rhs, Oddball::kToNumberOffset));
                assembler->Goto(&loop);
              }

              assembler->Bind(&if_rhsisnotboolean);
              {
                // Check if the {rhs} is a Receiver.
                STATIC_ASSERT(LAST_JS_RECEIVER_TYPE == LAST_TYPE);
                Label if_rhsisreceiver(assembler, Label::kDeferred),
                    if_rhsisnotreceiver(assembler, Label::kDeferred);
                assembler->Branch(
                    assembler->Int32LessThanOrEqual(
                        assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                        rhs_instance_type),
                    &if_rhsisreceiver, &if_rhsisnotreceiver);

                assembler->Bind(&if_rhsisreceiver);
                {
                  // Convert {rhs} to a primitive first (passing no hint).
                  // TODO(bmeurer): Hook up ToPrimitiveStub here once it exists.
                  var_rhs.Bind(assembler->CallRuntime(Runtime::kToPrimitive,
                                                      context, rhs));
                  assembler->Goto(&loop);
                }

                assembler->Bind(&if_rhsisnotreceiver);
                assembler->Goto(&if_notequal);
              }
            }
          }
        }
      }

      assembler->Bind(&if_lhsisnotsmi);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        {
          // The {lhs} is a HeapObject and the {rhs} is a Smi; swapping {lhs}
          // and {rhs} is not observable and doesn't matter for the result, so
          // we can just swap them and use the Smi handling above (for {lhs}
          // being a Smi).
          var_lhs.Bind(rhs);
          var_rhs.Bind(lhs);
          assembler->Goto(&loop);
        }

        assembler->Bind(&if_rhsisnotsmi);
        {
          Label if_lhsisstring(assembler), if_lhsisnumber(assembler),
              if_lhsissymbol(assembler), if_lhsissimd128value(assembler),
              if_lhsisoddball(assembler), if_lhsisreceiver(assembler);

          // Both {lhs} and {rhs} are HeapObjects, load their maps
          // and their instance types.
          Node* lhs_map = assembler->LoadMap(lhs);
          Node* rhs_map = assembler->LoadMap(rhs);

          // Load the instance types of {lhs} and {rhs}.
          Node* lhs_instance_type = assembler->LoadMapInstanceType(lhs_map);
          Node* rhs_instance_type = assembler->LoadMapInstanceType(rhs_map);

          // Dispatch based on the instance type of {lhs}.
          size_t const kNumCases = FIRST_NONSTRING_TYPE + 4;
          Label* case_labels[kNumCases];
          int32_t case_values[kNumCases];
          for (int32_t i = 0; i < FIRST_NONSTRING_TYPE; ++i) {
            case_labels[i] = new Label(assembler);
            case_values[i] = i;
          }
          case_labels[FIRST_NONSTRING_TYPE + 0] = &if_lhsisnumber;
          case_values[FIRST_NONSTRING_TYPE + 0] = HEAP_NUMBER_TYPE;
          case_labels[FIRST_NONSTRING_TYPE + 1] = &if_lhsissymbol;
          case_values[FIRST_NONSTRING_TYPE + 1] = SYMBOL_TYPE;
          case_labels[FIRST_NONSTRING_TYPE + 2] = &if_lhsissimd128value;
          case_values[FIRST_NONSTRING_TYPE + 2] = SIMD128_VALUE_TYPE;
          case_labels[FIRST_NONSTRING_TYPE + 3] = &if_lhsisoddball;
          case_values[FIRST_NONSTRING_TYPE + 3] = ODDBALL_TYPE;
          assembler->Switch(lhs_instance_type, &if_lhsisreceiver, case_values,
                            case_labels, arraysize(case_values));
          for (int32_t i = 0; i < FIRST_NONSTRING_TYPE; ++i) {
            assembler->Bind(case_labels[i]);
            assembler->Goto(&if_lhsisstring);
            delete case_labels[i];
          }

          assembler->Bind(&if_lhsisstring);
          {
            // Check if {rhs} is also a String.
            Label if_rhsisstring(assembler),
                if_rhsisnotstring(assembler, Label::kDeferred);
            assembler->Branch(assembler->Int32LessThan(
                                  rhs_instance_type, assembler->Int32Constant(
                                                         FIRST_NONSTRING_TYPE)),
                              &if_rhsisstring, &if_rhsisnotstring);

            assembler->Bind(&if_rhsisstring);
            {
              // Both {lhs} and {rhs} are of type String, just do the
              // string comparison then.
              Callable callable =
                  (mode == kDontNegateResult)
                      ? CodeFactory::StringEqual(assembler->isolate())
                      : CodeFactory::StringNotEqual(assembler->isolate());
              assembler->TailCallStub(callable, context, lhs, rhs);
            }

            assembler->Bind(&if_rhsisnotstring);
            {
              // The {lhs} is a String and the {rhs} is some other HeapObject.
              // Swapping {lhs} and {rhs} is not observable and doesn't matter
              // for the result, so we can just swap them and use the String
              // handling below (for {rhs} being a String).
              var_lhs.Bind(rhs);
              var_rhs.Bind(lhs);
              assembler->Goto(&loop);
            }
          }

          assembler->Bind(&if_lhsisnumber);
          {
            // Check if {rhs} is also a HeapNumber.
            Label if_rhsisnumber(assembler),
                if_rhsisnotnumber(assembler, Label::kDeferred);
            assembler->Branch(
                assembler->Word32Equal(lhs_instance_type, rhs_instance_type),
                &if_rhsisnumber, &if_rhsisnotnumber);

            assembler->Bind(&if_rhsisnumber);
            {
              // Convert {lhs} and {rhs} to floating point values, and
              // perform a floating point comparison.
              var_fcmp_lhs.Bind(assembler->LoadHeapNumberValue(lhs));
              var_fcmp_rhs.Bind(assembler->LoadHeapNumberValue(rhs));
              assembler->Goto(&do_fcmp);
            }

            assembler->Bind(&if_rhsisnotnumber);
            {
              // The {lhs} is a Number, the {rhs} is some other HeapObject.
              Label if_rhsisstring(assembler, Label::kDeferred),
                  if_rhsisnotstring(assembler);
              assembler->Branch(
                  assembler->Int32LessThan(
                      rhs_instance_type,
                      assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                  &if_rhsisstring, &if_rhsisnotstring);

              assembler->Bind(&if_rhsisstring);
              {
                // The {rhs} is a String and the {lhs} is a HeapNumber; we need
                // to convert the {rhs} to a Number and compare the output to
                // the Number on the {lhs}.
                Callable callable =
                    CodeFactory::StringToNumber(assembler->isolate());
                var_rhs.Bind(assembler->CallStub(callable, context, rhs));
                assembler->Goto(&loop);
              }

              assembler->Bind(&if_rhsisnotstring);
              {
                // Check if the {rhs} is a JSReceiver.
                Label if_rhsisreceiver(assembler, Label::kDeferred),
                    if_rhsisnotreceiver(assembler);
                STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
                assembler->Branch(
                    assembler->Int32LessThanOrEqual(
                        assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                        rhs_instance_type),
                    &if_rhsisreceiver, &if_rhsisnotreceiver);

                assembler->Bind(&if_rhsisreceiver);
                {
                  // The {lhs} is a Primitive and the {rhs} is a JSReceiver.
                  // Swapping {lhs} and {rhs} is not observable and doesn't
                  // matter for the result, so we can just swap them and use
                  // the JSReceiver handling below (for {lhs} being a
                  // JSReceiver).
                  var_lhs.Bind(rhs);
                  var_rhs.Bind(lhs);
                  assembler->Goto(&loop);
                }

                assembler->Bind(&if_rhsisnotreceiver);
                {
                  // Check if {rhs} is a Boolean.
                  Label if_rhsisboolean(assembler),
                      if_rhsisnotboolean(assembler);
                  Node* boolean_map = assembler->BooleanMapConstant();
                  assembler->Branch(assembler->WordEqual(rhs_map, boolean_map),
                                    &if_rhsisboolean, &if_rhsisnotboolean);

                  assembler->Bind(&if_rhsisboolean);
                  {
                    // The {rhs} is a Boolean, convert it to a Smi first.
                    var_rhs.Bind(assembler->LoadObjectField(
                        rhs, Oddball::kToNumberOffset));
                    assembler->Goto(&loop);
                  }

                  assembler->Bind(&if_rhsisnotboolean);
                  assembler->Goto(&if_notequal);
                }
              }
            }
          }

          assembler->Bind(&if_lhsisoddball);
          {
            // The {lhs} is an Oddball and {rhs} is some other HeapObject.
            Label if_lhsisboolean(assembler), if_lhsisnotboolean(assembler);
            Node* boolean_map = assembler->BooleanMapConstant();
            assembler->Branch(assembler->WordEqual(lhs_map, boolean_map),
                              &if_lhsisboolean, &if_lhsisnotboolean);

            assembler->Bind(&if_lhsisboolean);
            {
              // The {lhs} is a Boolean, check if {rhs} is also a Boolean.
              Label if_rhsisboolean(assembler), if_rhsisnotboolean(assembler);
              assembler->Branch(assembler->WordEqual(rhs_map, boolean_map),
                                &if_rhsisboolean, &if_rhsisnotboolean);

              assembler->Bind(&if_rhsisboolean);
              {
                // Both {lhs} and {rhs} are distinct Boolean values.
                assembler->Goto(&if_notequal);
              }

              assembler->Bind(&if_rhsisnotboolean);
              {
                // Convert the {lhs} to a Number first.
                var_lhs.Bind(
                    assembler->LoadObjectField(lhs, Oddball::kToNumberOffset));
                assembler->Goto(&loop);
              }
            }

            assembler->Bind(&if_lhsisnotboolean);
            {
              // The {lhs} is either Null or Undefined; check if the {rhs} is
              // undetectable (i.e. either also Null or Undefined or some
              // undetectable JSReceiver).
              Node* rhs_bitfield = assembler->LoadMapBitField(rhs_map);
              assembler->BranchIfWord32Equal(
                  assembler->Word32And(
                      rhs_bitfield,
                      assembler->Int32Constant(1 << Map::kIsUndetectable)),
                  assembler->Int32Constant(0), &if_notequal, &if_equal);
            }
          }

          assembler->Bind(&if_lhsissymbol);
          {
            // Check if the {rhs} is a JSReceiver.
            Label if_rhsisreceiver(assembler, Label::kDeferred),
                if_rhsisnotreceiver(assembler);
            STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
            assembler->Branch(
                assembler->Int32LessThanOrEqual(
                    assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                    rhs_instance_type),
                &if_rhsisreceiver, &if_rhsisnotreceiver);

            assembler->Bind(&if_rhsisreceiver);
            {
              // The {lhs} is a Primitive and the {rhs} is a JSReceiver.
              // Swapping {lhs} and {rhs} is not observable and doesn't
              // matter for the result, so we can just swap them and use
              // the JSReceiver handling below (for {lhs} being a JSReceiver).
              var_lhs.Bind(rhs);
              var_rhs.Bind(lhs);
              assembler->Goto(&loop);
            }

            assembler->Bind(&if_rhsisnotreceiver);
            {
              // The {rhs} is not a JSReceiver and also not the same Symbol
              // as the {lhs}, so this is equality check is considered false.
              assembler->Goto(&if_notequal);
            }
          }

          assembler->Bind(&if_lhsissimd128value);
          {
            // Check if the {rhs} is also a Simd128Value.
            Label if_rhsissimd128value(assembler),
                if_rhsisnotsimd128value(assembler);
            assembler->Branch(
                assembler->Word32Equal(lhs_instance_type, rhs_instance_type),
                &if_rhsissimd128value, &if_rhsisnotsimd128value);

            assembler->Bind(&if_rhsissimd128value);
            {
              // Both {lhs} and {rhs} is a Simd128Value.
              GenerateEqual_Simd128Value_HeapObject(assembler, lhs, lhs_map,
                                                    rhs, rhs_map, &if_equal,
                                                    &if_notequal);
            }

            assembler->Bind(&if_rhsisnotsimd128value);
            {
              // Check if the {rhs} is a JSReceiver.
              Label if_rhsisreceiver(assembler, Label::kDeferred),
                  if_rhsisnotreceiver(assembler);
              STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
              assembler->Branch(
                  assembler->Int32LessThanOrEqual(
                      assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                      rhs_instance_type),
                  &if_rhsisreceiver, &if_rhsisnotreceiver);

              assembler->Bind(&if_rhsisreceiver);
              {
                // The {lhs} is a Primitive and the {rhs} is a JSReceiver.
                // Swapping {lhs} and {rhs} is not observable and doesn't
                // matter for the result, so we can just swap them and use
                // the JSReceiver handling below (for {lhs} being a JSReceiver).
                var_lhs.Bind(rhs);
                var_rhs.Bind(lhs);
                assembler->Goto(&loop);
              }

              assembler->Bind(&if_rhsisnotreceiver);
              {
                // The {rhs} is some other Primitive.
                assembler->Goto(&if_notequal);
              }
            }
          }

          assembler->Bind(&if_lhsisreceiver);
          {
            // Check if the {rhs} is also a JSReceiver.
            Label if_rhsisreceiver(assembler), if_rhsisnotreceiver(assembler);
            STATIC_ASSERT(LAST_TYPE == LAST_JS_RECEIVER_TYPE);
            assembler->Branch(
                assembler->Int32LessThanOrEqual(
                    assembler->Int32Constant(FIRST_JS_RECEIVER_TYPE),
                    rhs_instance_type),
                &if_rhsisreceiver, &if_rhsisnotreceiver);

            assembler->Bind(&if_rhsisreceiver);
            {
              // Both {lhs} and {rhs} are different JSReceiver references, so
              // this cannot be considered equal.
              assembler->Goto(&if_notequal);
            }

            assembler->Bind(&if_rhsisnotreceiver);
            {
              // Check if {rhs} is Null or Undefined (an undetectable check
              // is sufficient here, since we already know that {rhs} is not
              // a JSReceiver).
              Label if_rhsisundetectable(assembler),
                  if_rhsisnotundetectable(assembler, Label::kDeferred);
              Node* rhs_bitfield = assembler->LoadMapBitField(rhs_map);
              assembler->BranchIfWord32Equal(
                  assembler->Word32And(
                      rhs_bitfield,
                      assembler->Int32Constant(1 << Map::kIsUndetectable)),
                  assembler->Int32Constant(0), &if_rhsisnotundetectable,
                  &if_rhsisundetectable);

              assembler->Bind(&if_rhsisundetectable);
              {
                // Check if {lhs} is an undetectable JSReceiver.
                Node* lhs_bitfield = assembler->LoadMapBitField(lhs_map);
                assembler->BranchIfWord32Equal(
                    assembler->Word32And(
                        lhs_bitfield,
                        assembler->Int32Constant(1 << Map::kIsUndetectable)),
                    assembler->Int32Constant(0), &if_notequal, &if_equal);
              }

              assembler->Bind(&if_rhsisnotundetectable);
              {
                // The {rhs} is some Primitive different from Null and
                // Undefined, need to convert {lhs} to Primitive first.
                // TODO(bmeurer): Hook up ToPrimitiveStub here once it exists.
                var_lhs.Bind(assembler->CallRuntime(Runtime::kToPrimitive,
                                                    context, lhs));
                assembler->Goto(&loop);
              }
            }
          }
        }
      }
    }
  }

  assembler->Bind(&do_fcmp);
  {
    // Load the {lhs} and {rhs} floating point values.
    Node* lhs = var_fcmp_lhs.value();
    Node* rhs = var_fcmp_rhs.value();

    // Perform a fast floating point comparison.
    assembler->BranchIfFloat64Equal(lhs, rhs, &if_equal, &if_notequal);
  }

  assembler->Bind(&if_equal);
  assembler->Return(assembler->BooleanConstant(mode == kDontNegateResult));

  assembler->Bind(&if_notequal);
  assembler->Return(assembler->BooleanConstant(mode == kNegateResult));
}

void GenerateStrictEqual(compiler::CodeStubAssembler* assembler,
                         ResultMode mode) {
  // Here's pseudo-code for the algorithm below in case of kDontNegateResult
  // mode; for kNegateResult mode we properly negate the result.
  //
  // if (lhs == rhs) {
  //   if (lhs->IsHeapNumber()) return HeapNumber::cast(lhs)->value() != NaN;
  //   return true;
  // }
  // if (!lhs->IsSmi()) {
  //   if (lhs->IsHeapNumber()) {
  //     if (rhs->IsSmi()) {
  //       return Smi::cast(rhs)->value() == HeapNumber::cast(lhs)->value();
  //     } else if (rhs->IsHeapNumber()) {
  //       return HeapNumber::cast(rhs)->value() ==
  //       HeapNumber::cast(lhs)->value();
  //     } else {
  //       return false;
  //     }
  //   } else {
  //     if (rhs->IsSmi()) {
  //       return false;
  //     } else {
  //       if (lhs->IsString()) {
  //         if (rhs->IsString()) {
  //           return %StringEqual(lhs, rhs);
  //         } else {
  //           return false;
  //         }
  //       } else if (lhs->IsSimd128()) {
  //         if (rhs->IsSimd128()) {
  //           return %StrictEqual(lhs, rhs);
  //         }
  //       } else {
  //         return false;
  //       }
  //     }
  //   }
  // } else {
  //   if (rhs->IsSmi()) {
  //     return false;
  //   } else {
  //     if (rhs->IsHeapNumber()) {
  //       return Smi::cast(lhs)->value() == HeapNumber::cast(rhs)->value();
  //     } else {
  //       return false;
  //     }
  //   }
  // }

  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;

  Node* lhs = assembler->Parameter(0);
  Node* rhs = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  Label if_equal(assembler), if_notequal(assembler);

  // Check if {lhs} and {rhs} refer to the same object.
  Label if_same(assembler), if_notsame(assembler);
  assembler->Branch(assembler->WordEqual(lhs, rhs), &if_same, &if_notsame);

  assembler->Bind(&if_same);
  {
    // The {lhs} and {rhs} reference the exact same value, yet we need special
    // treatment for HeapNumber, as NaN is not equal to NaN.
    GenerateEqual_Same(assembler, lhs, &if_equal, &if_notequal);
  }

  assembler->Bind(&if_notsame);
  {
    // The {lhs} and {rhs} reference different objects, yet for Smi, HeapNumber,
    // String and Simd128Value they can still be considered equal.
    Node* number_map = assembler->HeapNumberMapConstant();

    // Check if {lhs} is a Smi or a HeapObject.
    Label if_lhsissmi(assembler), if_lhsisnotsmi(assembler);
    assembler->Branch(assembler->WordIsSmi(lhs), &if_lhsissmi, &if_lhsisnotsmi);

    assembler->Bind(&if_lhsisnotsmi);
    {
      // Load the map of {lhs}.
      Node* lhs_map = assembler->LoadMap(lhs);

      // Check if {lhs} is a HeapNumber.
      Label if_lhsisnumber(assembler), if_lhsisnotnumber(assembler);
      assembler->Branch(assembler->WordEqual(lhs_map, number_map),
                        &if_lhsisnumber, &if_lhsisnotnumber);

      assembler->Bind(&if_lhsisnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        {
          // Convert {lhs} and {rhs} to floating point values.
          Node* lhs_value = assembler->LoadHeapNumberValue(lhs);
          Node* rhs_value = assembler->SmiToFloat64(rhs);

          // Perform a floating point comparison of {lhs} and {rhs}.
          assembler->BranchIfFloat64Equal(lhs_value, rhs_value, &if_equal,
                                          &if_notequal);
        }

        assembler->Bind(&if_rhsisnotsmi);
        {
          // Load the map of {rhs}.
          Node* rhs_map = assembler->LoadMap(rhs);

          // Check if {rhs} is also a HeapNumber.
          Label if_rhsisnumber(assembler), if_rhsisnotnumber(assembler);
          assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                            &if_rhsisnumber, &if_rhsisnotnumber);

          assembler->Bind(&if_rhsisnumber);
          {
            // Convert {lhs} and {rhs} to floating point values.
            Node* lhs_value = assembler->LoadHeapNumberValue(lhs);
            Node* rhs_value = assembler->LoadHeapNumberValue(rhs);

            // Perform a floating point comparison of {lhs} and {rhs}.
            assembler->BranchIfFloat64Equal(lhs_value, rhs_value, &if_equal,
                                            &if_notequal);
          }

          assembler->Bind(&if_rhsisnotnumber);
          assembler->Goto(&if_notequal);
        }
      }

      assembler->Bind(&if_lhsisnotnumber);
      {
        // Check if {rhs} is a Smi or a HeapObject.
        Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
        assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                          &if_rhsisnotsmi);

        assembler->Bind(&if_rhsissmi);
        assembler->Goto(&if_notequal);

        assembler->Bind(&if_rhsisnotsmi);
        {
          // Load the instance type of {lhs}.
          Node* lhs_instance_type = assembler->LoadMapInstanceType(lhs_map);

          // Check if {lhs} is a String.
          Label if_lhsisstring(assembler), if_lhsisnotstring(assembler);
          assembler->Branch(assembler->Int32LessThan(
                                lhs_instance_type,
                                assembler->Int32Constant(FIRST_NONSTRING_TYPE)),
                            &if_lhsisstring, &if_lhsisnotstring);

          assembler->Bind(&if_lhsisstring);
          {
            // Load the instance type of {rhs}.
            Node* rhs_instance_type = assembler->LoadInstanceType(rhs);

            // Check if {rhs} is also a String.
            Label if_rhsisstring(assembler), if_rhsisnotstring(assembler);
            assembler->Branch(assembler->Int32LessThan(
                                  rhs_instance_type, assembler->Int32Constant(
                                                         FIRST_NONSTRING_TYPE)),
                              &if_rhsisstring, &if_rhsisnotstring);

            assembler->Bind(&if_rhsisstring);
            {
              Callable callable =
                  (mode == kDontNegateResult)
                      ? CodeFactory::StringEqual(assembler->isolate())
                      : CodeFactory::StringNotEqual(assembler->isolate());
              assembler->TailCallStub(callable, context, lhs, rhs);
            }

            assembler->Bind(&if_rhsisnotstring);
            assembler->Goto(&if_notequal);
          }

          assembler->Bind(&if_lhsisnotstring);
          {
            // Check if {lhs} is a Simd128Value.
            Label if_lhsissimd128value(assembler),
                if_lhsisnotsimd128value(assembler);
            assembler->Branch(assembler->Word32Equal(
                                  lhs_instance_type,
                                  assembler->Int32Constant(SIMD128_VALUE_TYPE)),
                              &if_lhsissimd128value, &if_lhsisnotsimd128value);

            assembler->Bind(&if_lhsissimd128value);
            {
              // Load the map of {rhs}.
              Node* rhs_map = assembler->LoadMap(rhs);

              // Check if {rhs} is also a Simd128Value that is equal to {lhs}.
              GenerateEqual_Simd128Value_HeapObject(assembler, lhs, lhs_map,
                                                    rhs, rhs_map, &if_equal,
                                                    &if_notequal);
            }

            assembler->Bind(&if_lhsisnotsimd128value);
            assembler->Goto(&if_notequal);
          }
        }
      }
    }

    assembler->Bind(&if_lhsissmi);
    {
      // We already know that {lhs} and {rhs} are not reference equal, and {lhs}
      // is a Smi; so {lhs} and {rhs} can only be strictly equal if {rhs} is a
      // HeapNumber with an equal floating point value.

      // Check if {rhs} is a Smi or a HeapObject.
      Label if_rhsissmi(assembler), if_rhsisnotsmi(assembler);
      assembler->Branch(assembler->WordIsSmi(rhs), &if_rhsissmi,
                        &if_rhsisnotsmi);

      assembler->Bind(&if_rhsissmi);
      assembler->Goto(&if_notequal);

      assembler->Bind(&if_rhsisnotsmi);
      {
        // Load the map of the {rhs}.
        Node* rhs_map = assembler->LoadMap(rhs);

        // The {rhs} could be a HeapNumber with the same value as {lhs}.
        Label if_rhsisnumber(assembler), if_rhsisnotnumber(assembler);
        assembler->Branch(assembler->WordEqual(rhs_map, number_map),
                          &if_rhsisnumber, &if_rhsisnotnumber);

        assembler->Bind(&if_rhsisnumber);
        {
          // Convert {lhs} and {rhs} to floating point values.
          Node* lhs_value = assembler->SmiToFloat64(lhs);
          Node* rhs_value = assembler->LoadHeapNumberValue(rhs);

          // Perform a floating point comparison of {lhs} and {rhs}.
          assembler->BranchIfFloat64Equal(lhs_value, rhs_value, &if_equal,
                                          &if_notequal);
        }

        assembler->Bind(&if_rhsisnotnumber);
        assembler->Goto(&if_notequal);
      }
    }
  }

  assembler->Bind(&if_equal);
  assembler->Return(assembler->BooleanConstant(mode == kDontNegateResult));

  assembler->Bind(&if_notequal);
  assembler->Return(assembler->BooleanConstant(mode == kNegateResult));
}

void GenerateStringRelationalComparison(compiler::CodeStubAssembler* assembler,
                                        RelationalComparisonMode mode) {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* lhs = assembler->Parameter(0);
  Node* rhs = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  Label if_less(assembler), if_equal(assembler), if_greater(assembler);

  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  Label if_same(assembler), if_notsame(assembler);
  assembler->Branch(assembler->WordEqual(lhs, rhs), &if_same, &if_notsame);

  assembler->Bind(&if_same);
  assembler->Goto(&if_equal);

  assembler->Bind(&if_notsame);
  {
    // Load instance types of {lhs} and {rhs}.
    Node* lhs_instance_type = assembler->LoadInstanceType(lhs);
    Node* rhs_instance_type = assembler->LoadInstanceType(rhs);

    // Combine the instance types into a single 16-bit value, so we can check
    // both of them at once.
    Node* both_instance_types = assembler->Word32Or(
        lhs_instance_type,
        assembler->Word32Shl(rhs_instance_type, assembler->Int32Constant(8)));

    // Check that both {lhs} and {rhs} are flat one-byte strings.
    int const kBothSeqOneByteStringMask =
        kStringEncodingMask | kStringRepresentationMask |
        ((kStringEncodingMask | kStringRepresentationMask) << 8);
    int const kBothSeqOneByteStringTag =
        kOneByteStringTag | kSeqStringTag |
        ((kOneByteStringTag | kSeqStringTag) << 8);
    Label if_bothonebyteseqstrings(assembler),
        if_notbothonebyteseqstrings(assembler);
    assembler->Branch(assembler->Word32Equal(
                          assembler->Word32And(both_instance_types,
                                               assembler->Int32Constant(
                                                   kBothSeqOneByteStringMask)),
                          assembler->Int32Constant(kBothSeqOneByteStringTag)),
                      &if_bothonebyteseqstrings, &if_notbothonebyteseqstrings);

    assembler->Bind(&if_bothonebyteseqstrings);
    {
      // Load the length of {lhs} and {rhs}.
      Node* lhs_length = assembler->LoadObjectField(lhs, String::kLengthOffset);
      Node* rhs_length = assembler->LoadObjectField(rhs, String::kLengthOffset);

      // Determine the minimum length.
      Node* length = assembler->SmiMin(lhs_length, rhs_length);

      // Compute the effective offset of the first character.
      Node* begin = assembler->IntPtrConstant(SeqOneByteString::kHeaderSize -
                                              kHeapObjectTag);

      // Compute the first offset after the string from the length.
      Node* end = assembler->IntPtrAdd(begin, assembler->SmiUntag(length));

      // Loop over the {lhs} and {rhs} strings to see if they are equal.
      Variable var_offset(assembler, MachineType::PointerRepresentation());
      Label loop(assembler, &var_offset);
      var_offset.Bind(begin);
      assembler->Goto(&loop);
      assembler->Bind(&loop);
      {
        // Check if {offset} equals {end}.
        Node* offset = var_offset.value();
        Label if_done(assembler), if_notdone(assembler);
        assembler->Branch(assembler->WordEqual(offset, end), &if_done,
                          &if_notdone);

        assembler->Bind(&if_notdone);
        {
          // Load the next characters from {lhs} and {rhs}.
          Node* lhs_value = assembler->Load(MachineType::Uint8(), lhs, offset);
          Node* rhs_value = assembler->Load(MachineType::Uint8(), rhs, offset);

          // Check if the characters match.
          Label if_valueissame(assembler), if_valueisnotsame(assembler);
          assembler->Branch(assembler->Word32Equal(lhs_value, rhs_value),
                            &if_valueissame, &if_valueisnotsame);

          assembler->Bind(&if_valueissame);
          {
            // Advance to next character.
            var_offset.Bind(
                assembler->IntPtrAdd(offset, assembler->IntPtrConstant(1)));
          }
          assembler->Goto(&loop);

          assembler->Bind(&if_valueisnotsame);
          assembler->BranchIf(assembler->Uint32LessThan(lhs_value, rhs_value),
                              &if_less, &if_greater);
        }

        assembler->Bind(&if_done);
        {
          // All characters up to the min length are equal, decide based on
          // string length.
          Label if_lengthisequal(assembler), if_lengthisnotequal(assembler);
          assembler->Branch(assembler->SmiEqual(lhs_length, rhs_length),
                            &if_lengthisequal, &if_lengthisnotequal);

          assembler->Bind(&if_lengthisequal);
          assembler->Goto(&if_equal);

          assembler->Bind(&if_lengthisnotequal);
          assembler->BranchIfSmiLessThan(lhs_length, rhs_length, &if_less,
                                         &if_greater);
        }
      }
    }

    assembler->Bind(&if_notbothonebyteseqstrings);
    {
      // TODO(bmeurer): Add fast case support for flattened cons strings;
      // also add support for two byte string relational comparisons.
      switch (mode) {
        case kLessThan:
          assembler->TailCallRuntime(Runtime::kStringLessThan, context, lhs,
                                     rhs);
          break;
        case kLessThanOrEqual:
          assembler->TailCallRuntime(Runtime::kStringLessThanOrEqual, context,
                                     lhs, rhs);
          break;
        case kGreaterThan:
          assembler->TailCallRuntime(Runtime::kStringGreaterThan, context, lhs,
                                     rhs);
          break;
        case kGreaterThanOrEqual:
          assembler->TailCallRuntime(Runtime::kStringGreaterThanOrEqual,
                                     context, lhs, rhs);
          break;
      }
    }
  }

  assembler->Bind(&if_less);
  switch (mode) {
    case kLessThan:
    case kLessThanOrEqual:
      assembler->Return(assembler->BooleanConstant(true));
      break;

    case kGreaterThan:
    case kGreaterThanOrEqual:
      assembler->Return(assembler->BooleanConstant(false));
      break;
  }

  assembler->Bind(&if_equal);
  switch (mode) {
    case kLessThan:
    case kGreaterThan:
      assembler->Return(assembler->BooleanConstant(false));
      break;

    case kLessThanOrEqual:
    case kGreaterThanOrEqual:
      assembler->Return(assembler->BooleanConstant(true));
      break;
  }

  assembler->Bind(&if_greater);
  switch (mode) {
    case kLessThan:
    case kLessThanOrEqual:
      assembler->Return(assembler->BooleanConstant(false));
      break;

    case kGreaterThan:
    case kGreaterThanOrEqual:
      assembler->Return(assembler->BooleanConstant(true));
      break;
  }
}

void GenerateStringEqual(compiler::CodeStubAssembler* assembler,
                         ResultMode mode) {
  // Here's pseudo-code for the algorithm below in case of kDontNegateResult
  // mode; for kNegateResult mode we properly negate the result.
  //
  // if (lhs == rhs) return true;
  // if (lhs->length() != rhs->length()) return false;
  // if (lhs->IsInternalizedString() && rhs->IsInternalizedString()) {
  //   return false;
  // }
  // if (lhs->IsSeqOneByteString() && rhs->IsSeqOneByteString()) {
  //   for (i = 0; i != lhs->length(); ++i) {
  //     if (lhs[i] != rhs[i]) return false;
  //   }
  //   return true;
  // }
  // return %StringEqual(lhs, rhs);

  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* lhs = assembler->Parameter(0);
  Node* rhs = assembler->Parameter(1);
  Node* context = assembler->Parameter(2);

  Label if_equal(assembler), if_notequal(assembler);

  // Fast check to see if {lhs} and {rhs} refer to the same String object.
  Label if_same(assembler), if_notsame(assembler);
  assembler->Branch(assembler->WordEqual(lhs, rhs), &if_same, &if_notsame);

  assembler->Bind(&if_same);
  assembler->Goto(&if_equal);

  assembler->Bind(&if_notsame);
  {
    // The {lhs} and {rhs} don't refer to the exact same String object.

    // Load the length of {lhs} and {rhs}.
    Node* lhs_length = assembler->LoadObjectField(lhs, String::kLengthOffset);
    Node* rhs_length = assembler->LoadObjectField(rhs, String::kLengthOffset);

    // Check if the lengths of {lhs} and {rhs} are equal.
    Label if_lengthisequal(assembler), if_lengthisnotequal(assembler);
    assembler->Branch(assembler->WordEqual(lhs_length, rhs_length),
                      &if_lengthisequal, &if_lengthisnotequal);

    assembler->Bind(&if_lengthisequal);
    {
      // Load instance types of {lhs} and {rhs}.
      Node* lhs_instance_type = assembler->LoadInstanceType(lhs);
      Node* rhs_instance_type = assembler->LoadInstanceType(rhs);

      // Combine the instance types into a single 16-bit value, so we can check
      // both of them at once.
      Node* both_instance_types = assembler->Word32Or(
          lhs_instance_type,
          assembler->Word32Shl(rhs_instance_type, assembler->Int32Constant(8)));

      // Check if both {lhs} and {rhs} are internalized.
      int const kBothInternalizedMask =
          kIsNotInternalizedMask | (kIsNotInternalizedMask << 8);
      int const kBothInternalizedTag =
          kInternalizedTag | (kInternalizedTag << 8);
      Label if_bothinternalized(assembler), if_notbothinternalized(assembler);
      assembler->Branch(assembler->Word32Equal(
                            assembler->Word32And(both_instance_types,
                                                 assembler->Int32Constant(
                                                     kBothInternalizedMask)),
                            assembler->Int32Constant(kBothInternalizedTag)),
                        &if_bothinternalized, &if_notbothinternalized);

      assembler->Bind(&if_bothinternalized);
      {
        // Fast negative check for internalized-to-internalized equality.
        assembler->Goto(&if_notequal);
      }

      assembler->Bind(&if_notbothinternalized);
      {
        // Check that both {lhs} and {rhs} are flat one-byte strings.
        int const kBothSeqOneByteStringMask =
            kStringEncodingMask | kStringRepresentationMask |
            ((kStringEncodingMask | kStringRepresentationMask) << 8);
        int const kBothSeqOneByteStringTag =
            kOneByteStringTag | kSeqStringTag |
            ((kOneByteStringTag | kSeqStringTag) << 8);
        Label if_bothonebyteseqstrings(assembler),
            if_notbothonebyteseqstrings(assembler);
        assembler->Branch(
            assembler->Word32Equal(
                assembler->Word32And(
                    both_instance_types,
                    assembler->Int32Constant(kBothSeqOneByteStringMask)),
                assembler->Int32Constant(kBothSeqOneByteStringTag)),
            &if_bothonebyteseqstrings, &if_notbothonebyteseqstrings);

        assembler->Bind(&if_bothonebyteseqstrings);
        {
          // Compute the effective offset of the first character.
          Node* begin = assembler->IntPtrConstant(
              SeqOneByteString::kHeaderSize - kHeapObjectTag);

          // Compute the first offset after the string from the length.
          Node* end =
              assembler->IntPtrAdd(begin, assembler->SmiUntag(lhs_length));

          // Loop over the {lhs} and {rhs} strings to see if they are equal.
          Variable var_offset(assembler, MachineType::PointerRepresentation());
          Label loop(assembler, &var_offset);
          var_offset.Bind(begin);
          assembler->Goto(&loop);
          assembler->Bind(&loop);
          {
            // Check if {offset} equals {end}.
            Node* offset = var_offset.value();
            Label if_done(assembler), if_notdone(assembler);
            assembler->Branch(assembler->WordEqual(offset, end), &if_done,
                              &if_notdone);

            assembler->Bind(&if_notdone);
            {
              // Load the next characters from {lhs} and {rhs}.
              Node* lhs_value =
                  assembler->Load(MachineType::Uint8(), lhs, offset);
              Node* rhs_value =
                  assembler->Load(MachineType::Uint8(), rhs, offset);

              // Check if the characters match.
              Label if_valueissame(assembler), if_valueisnotsame(assembler);
              assembler->Branch(assembler->Word32Equal(lhs_value, rhs_value),
                                &if_valueissame, &if_valueisnotsame);

              assembler->Bind(&if_valueissame);
              {
                // Advance to next character.
                var_offset.Bind(
                    assembler->IntPtrAdd(offset, assembler->IntPtrConstant(1)));
              }
              assembler->Goto(&loop);

              assembler->Bind(&if_valueisnotsame);
              assembler->Goto(&if_notequal);
            }

            assembler->Bind(&if_done);
            assembler->Goto(&if_equal);
          }
        }

        assembler->Bind(&if_notbothonebyteseqstrings);
        {
          // TODO(bmeurer): Add fast case support for flattened cons strings;
          // also add support for two byte string equality checks.
          Runtime::FunctionId function_id = (mode == kDontNegateResult)
                                                ? Runtime::kStringEqual
                                                : Runtime::kStringNotEqual;
          assembler->TailCallRuntime(function_id, context, lhs, rhs);
        }
      }
    }

    assembler->Bind(&if_lengthisnotequal);
    {
      // Mismatch in length of {lhs} and {rhs}, cannot be equal.
      assembler->Goto(&if_notequal);
    }
  }

  assembler->Bind(&if_equal);
  assembler->Return(assembler->BooleanConstant(mode == kDontNegateResult));

  assembler->Bind(&if_notequal);
  assembler->Return(assembler->BooleanConstant(mode == kNegateResult));
}

}  // namespace

void LessThanStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateAbstractRelationalComparison(assembler, kLessThan);
}

void LessThanOrEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateAbstractRelationalComparison(assembler, kLessThanOrEqual);
}

void GreaterThanStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateAbstractRelationalComparison(assembler, kGreaterThan);
}

void GreaterThanOrEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateAbstractRelationalComparison(assembler, kGreaterThanOrEqual);
}

void EqualStub::GenerateAssembly(compiler::CodeStubAssembler* assembler) const {
  GenerateEqual(assembler, kDontNegateResult);
}

void NotEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateEqual(assembler, kNegateResult);
}

void StrictEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStrictEqual(assembler, kDontNegateResult);
}

void StrictNotEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStrictEqual(assembler, kNegateResult);
}

void StringEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStringEqual(assembler, kDontNegateResult);
}

void StringNotEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStringEqual(assembler, kNegateResult);
}

void StringLessThanStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStringRelationalComparison(assembler, kLessThan);
}

void StringLessThanOrEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStringRelationalComparison(assembler, kLessThanOrEqual);
}

void StringGreaterThanStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStringRelationalComparison(assembler, kGreaterThan);
}

void StringGreaterThanOrEqualStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  GenerateStringRelationalComparison(assembler, kGreaterThanOrEqual);
}

void ToLengthStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(1);

  // We might need to loop once for ToNumber conversion.
  Variable var_len(assembler, MachineRepresentation::kTagged);
  Label loop(assembler, &var_len);
  var_len.Bind(assembler->Parameter(0));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Shared entry points.
    Label return_len(assembler),
        return_two53minus1(assembler, Label::kDeferred),
        return_zero(assembler, Label::kDeferred);

    // Load the current {len} value.
    Node* len = var_len.value();

    // Check if {len} is a positive Smi.
    assembler->GotoIf(assembler->WordIsPositiveSmi(len), &return_len);

    // Check if {len} is a (negative) Smi.
    assembler->GotoIf(assembler->WordIsSmi(len), &return_zero);

    // Check if {len} is a HeapNumber.
    Label if_lenisheapnumber(assembler),
        if_lenisnotheapnumber(assembler, Label::kDeferred);
    assembler->Branch(assembler->WordEqual(assembler->LoadMap(len),
                                           assembler->HeapNumberMapConstant()),
                      &if_lenisheapnumber, &if_lenisnotheapnumber);

    assembler->Bind(&if_lenisheapnumber);
    {
      // Load the floating-point value of {len}.
      Node* len_value = assembler->LoadHeapNumberValue(len);

      // Check if {len} is not greater than zero.
      assembler->GotoUnless(assembler->Float64GreaterThan(
                                len_value, assembler->Float64Constant(0.0)),
                            &return_zero);

      // Check if {len} is greater than or equal to 2^53-1.
      assembler->GotoIf(
          assembler->Float64GreaterThanOrEqual(
              len_value, assembler->Float64Constant(kMaxSafeInteger)),
          &return_two53minus1);

      // Round the {len} towards -Infinity.
      Node* value = assembler->Float64Floor(len_value);
      Node* result = assembler->ChangeFloat64ToTagged(value);
      assembler->Return(result);
    }

    assembler->Bind(&if_lenisnotheapnumber);
    {
      // Need to convert {len} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(assembler->isolate());
      var_len.Bind(assembler->CallStub(callable, context, len));
      assembler->Goto(&loop);
    }

    assembler->Bind(&return_len);
    assembler->Return(var_len.value());

    assembler->Bind(&return_two53minus1);
    assembler->Return(assembler->NumberConstant(kMaxSafeInteger));

    assembler->Bind(&return_zero);
    assembler->Return(assembler->SmiConstant(Smi::FromInt(0)));
  }
}

void ToBooleanStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Label Label;

  Node* value = assembler->Parameter(0);
  Label if_valueissmi(assembler), if_valueisnotsmi(assembler);

  // Check if {value} is a Smi or a HeapObject.
  assembler->Branch(assembler->WordIsSmi(value), &if_valueissmi,
                    &if_valueisnotsmi);

  assembler->Bind(&if_valueissmi);
  {
    // The {value} is a Smi, only need to check against zero.
    Label if_valueiszero(assembler), if_valueisnotzero(assembler);
    assembler->Branch(assembler->SmiEqual(value, assembler->SmiConstant(0)),
                      &if_valueiszero, &if_valueisnotzero);

    assembler->Bind(&if_valueiszero);
    assembler->Return(assembler->BooleanConstant(false));

    assembler->Bind(&if_valueisnotzero);
    assembler->Return(assembler->BooleanConstant(true));
  }

  assembler->Bind(&if_valueisnotsmi);
  {
    Label if_valueisstring(assembler), if_valueisheapnumber(assembler),
        if_valueisoddball(assembler), if_valueisother(assembler);

    // The {value} is a HeapObject, load its map.
    Node* value_map = assembler->LoadMap(value);

    // Load the {value}s instance type.
    Node* value_instance_type = assembler->Load(
        MachineType::Uint8(), value_map,
        assembler->IntPtrConstant(Map::kInstanceTypeOffset - kHeapObjectTag));

    // Dispatch based on the instance type; we distinguish all String instance
    // types, the HeapNumber type and the Oddball type.
    size_t const kNumCases = FIRST_NONSTRING_TYPE + 2;
    Label* case_labels[kNumCases];
    int32_t case_values[kNumCases];
    for (int32_t i = 0; i < FIRST_NONSTRING_TYPE; ++i) {
      case_labels[i] = new Label(assembler);
      case_values[i] = i;
    }
    case_labels[FIRST_NONSTRING_TYPE + 0] = &if_valueisheapnumber;
    case_values[FIRST_NONSTRING_TYPE + 0] = HEAP_NUMBER_TYPE;
    case_labels[FIRST_NONSTRING_TYPE + 1] = &if_valueisoddball;
    case_values[FIRST_NONSTRING_TYPE + 1] = ODDBALL_TYPE;
    assembler->Switch(value_instance_type, &if_valueisother, case_values,
                      case_labels, arraysize(case_values));
    for (int32_t i = 0; i < FIRST_NONSTRING_TYPE; ++i) {
      assembler->Bind(case_labels[i]);
      assembler->Goto(&if_valueisstring);
      delete case_labels[i];
    }

    assembler->Bind(&if_valueisstring);
    {
      // Load the string length field of the {value}.
      Node* value_length =
          assembler->LoadObjectField(value, String::kLengthOffset);

      // Check if the {value} is the empty string.
      Label if_valueisempty(assembler), if_valueisnotempty(assembler);
      assembler->Branch(
          assembler->SmiEqual(value_length, assembler->SmiConstant(0)),
          &if_valueisempty, &if_valueisnotempty);

      assembler->Bind(&if_valueisempty);
      assembler->Return(assembler->BooleanConstant(false));

      assembler->Bind(&if_valueisnotempty);
      assembler->Return(assembler->BooleanConstant(true));
    }

    assembler->Bind(&if_valueisheapnumber);
    {
      Node* value_value = assembler->Load(
          MachineType::Float64(), value,
          assembler->IntPtrConstant(HeapNumber::kValueOffset - kHeapObjectTag));

      Label if_valueispositive(assembler), if_valueisnotpositive(assembler),
          if_valueisnegative(assembler), if_valueisnanorzero(assembler);
      assembler->Branch(assembler->Float64LessThan(
                            assembler->Float64Constant(0.0), value_value),
                        &if_valueispositive, &if_valueisnotpositive);

      assembler->Bind(&if_valueispositive);
      assembler->Return(assembler->BooleanConstant(true));

      assembler->Bind(&if_valueisnotpositive);
      assembler->Branch(assembler->Float64LessThan(
                            value_value, assembler->Float64Constant(0.0)),
                        &if_valueisnegative, &if_valueisnanorzero);

      assembler->Bind(&if_valueisnegative);
      assembler->Return(assembler->BooleanConstant(true));

      assembler->Bind(&if_valueisnanorzero);
      assembler->Return(assembler->BooleanConstant(false));
    }

    assembler->Bind(&if_valueisoddball);
    {
      // The {value} is an Oddball, and every Oddball knows its boolean value.
      Node* value_toboolean =
          assembler->LoadObjectField(value, Oddball::kToBooleanOffset);
      assembler->Return(value_toboolean);
    }

    assembler->Bind(&if_valueisother);
    {
      Node* value_map_bitfield = assembler->Load(
          MachineType::Uint8(), value_map,
          assembler->IntPtrConstant(Map::kBitFieldOffset - kHeapObjectTag));
      Node* value_map_undetectable = assembler->Word32And(
          value_map_bitfield,
          assembler->Int32Constant(1 << Map::kIsUndetectable));

      // Check if the {value} is undetectable.
      Label if_valueisundetectable(assembler),
          if_valueisnotundetectable(assembler);
      assembler->Branch(assembler->Word32Equal(value_map_undetectable,
                                               assembler->Int32Constant(0)),
                        &if_valueisnotundetectable, &if_valueisundetectable);

      assembler->Bind(&if_valueisundetectable);
      assembler->Return(assembler->BooleanConstant(false));

      assembler->Bind(&if_valueisnotundetectable);
      assembler->Return(assembler->BooleanConstant(true));
    }
  }
}

void ToIntegerStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Variable Variable;

  Node* context = assembler->Parameter(1);

  // We might need to loop once for ToNumber conversion.
  Variable var_arg(assembler, MachineRepresentation::kTagged);
  Label loop(assembler, &var_arg);
  var_arg.Bind(assembler->Parameter(0));
  assembler->Goto(&loop);
  assembler->Bind(&loop);
  {
    // Shared entry points.
    Label return_arg(assembler), return_zero(assembler, Label::kDeferred);

    // Load the current {arg} value.
    Node* arg = var_arg.value();

    // Check if {arg} is a Smi.
    assembler->GotoIf(assembler->WordIsSmi(arg), &return_arg);

    // Check if {arg} is a HeapNumber.
    Label if_argisheapnumber(assembler),
        if_argisnotheapnumber(assembler, Label::kDeferred);
    assembler->Branch(assembler->WordEqual(assembler->LoadMap(arg),
                                           assembler->HeapNumberMapConstant()),
                      &if_argisheapnumber, &if_argisnotheapnumber);

    assembler->Bind(&if_argisheapnumber);
    {
      // Load the floating-point value of {arg}.
      Node* arg_value = assembler->LoadHeapNumberValue(arg);

      // Check if {arg} is NaN.
      assembler->GotoUnless(assembler->Float64Equal(arg_value, arg_value),
                            &return_zero);

      // Truncate {arg} towards zero.
      Node* value = assembler->Float64Trunc(arg_value);
      var_arg.Bind(assembler->ChangeFloat64ToTagged(value));
      assembler->Goto(&return_arg);
    }

    assembler->Bind(&if_argisnotheapnumber);
    {
      // Need to convert {arg} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(assembler->isolate());
      var_arg.Bind(assembler->CallStub(callable, context, arg));
      assembler->Goto(&loop);
    }

    assembler->Bind(&return_arg);
    assembler->Return(var_arg.value());

    assembler->Bind(&return_zero);
    assembler->Return(assembler->SmiConstant(Smi::FromInt(0)));
  }
}

void StoreInterceptorStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  Node* receiver = assembler->Parameter(0);
  Node* name = assembler->Parameter(1);
  Node* value = assembler->Parameter(2);
  Node* context = assembler->Parameter(3);
  assembler->TailCallRuntime(Runtime::kStorePropertyWithInterceptor, context,
                             receiver, name, value);
}

void LoadIndexedInterceptorStub::GenerateAssembly(
    compiler::CodeStubAssembler* assembler) const {
  typedef compiler::Node Node;
  typedef compiler::CodeStubAssembler::Label Label;
  Node* receiver = assembler->Parameter(0);
  Node* key = assembler->Parameter(1);
  Node* slot = assembler->Parameter(2);
  Node* vector = assembler->Parameter(3);
  Node* context = assembler->Parameter(4);

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


void KeyedLoadGenericStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kKeyedGetProperty)->entry);
}


void HandlerStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  if (kind() == Code::STORE_IC) {
    descriptor->Initialize(FUNCTION_ADDR(Runtime_StoreIC_MissFromStubFailure));
  } else if (kind() == Code::KEYED_LOAD_IC) {
    descriptor->Initialize(
        FUNCTION_ADDR(Runtime_KeyedLoadIC_MissFromStubFailure));
  } else if (kind() == Code::KEYED_STORE_IC) {
    descriptor->Initialize(
        FUNCTION_ADDR(Runtime_KeyedStoreIC_MissFromStubFailure));
  }
}


CallInterfaceDescriptor HandlerStub::GetCallInterfaceDescriptor() const {
  if (kind() == Code::LOAD_IC || kind() == Code::KEYED_LOAD_IC) {
    return LoadWithVectorDescriptor(isolate());
  } else {
    DCHECK(kind() == Code::STORE_IC || kind() == Code::KEYED_STORE_IC);
    return VectorStoreICDescriptor(isolate());
  }
}


void StoreFastElementStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      FUNCTION_ADDR(Runtime_KeyedStoreIC_MissFromStubFailure));
}


void ElementsTransitionAndStoreStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      FUNCTION_ADDR(Runtime_ElementsTransitionAndStoreIC_Miss));
}


void ToObjectStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(Runtime::FunctionForId(Runtime::kToObject)->entry);
}


CallInterfaceDescriptor StoreTransitionStub::GetCallInterfaceDescriptor()
    const {
  return VectorStoreTransitionDescriptor(isolate());
}


CallInterfaceDescriptor
ElementsTransitionAndStoreStub::GetCallInterfaceDescriptor() const {
  return VectorStoreTransitionDescriptor(isolate());
}


void FastNewClosureStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(Runtime::FunctionForId(Runtime::kNewClosure)->entry);
}


void FastNewContextStub::InitializeDescriptor(CodeStubDescriptor* d) {}


void TypeofStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {}


void NumberToStringStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kNumberToString)->entry);
}


void FastCloneRegExpStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  FastCloneRegExpDescriptor call_descriptor(isolate());
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kCreateRegExpLiteral)->entry);
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
      Runtime::FunctionForId(Runtime::kRegExpConstructResult)->entry);
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


void AllocateMutableHeapNumberStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize();
}

#define SIMD128_INIT_DESC(TYPE, Type, type, lane_count, lane_type) \
  void Allocate##Type##Stub::InitializeDescriptor(                 \
      CodeStubDescriptor* descriptor) {                            \
    descriptor->Initialize(                                        \
        Runtime::FunctionForId(Runtime::kCreate##Type)->entry);    \
  }
SIMD128_TYPES(SIMD128_INIT_DESC)
#undef SIMD128_INIT_DESC

void AllocateInNewSpaceStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize();
}

void ToBooleanICStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(Runtime_ToBooleanIC_Miss));
  descriptor->SetMissHandler(ExternalReference(
      Runtime::FunctionForId(Runtime::kToBooleanIC_Miss), isolate()));
}


void BinaryOpICStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(FUNCTION_ADDR(Runtime_BinaryOpIC_Miss));
  descriptor->SetMissHandler(ExternalReference(
      Runtime::FunctionForId(Runtime::kBinaryOpIC_Miss), isolate()));
}


void BinaryOpWithAllocationSiteStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      FUNCTION_ADDR(Runtime_BinaryOpIC_MissWithAllocationSite));
}


void StringAddStub::InitializeDescriptor(CodeStubDescriptor* descriptor) {
  descriptor->Initialize(Runtime::FunctionForId(Runtime::kStringAdd)->entry);
}


void GrowArrayElementsStub::InitializeDescriptor(
    CodeStubDescriptor* descriptor) {
  descriptor->Initialize(
      Runtime::FunctionForId(Runtime::kGrowArrayElements)->entry);
}


void TypeofStub::GenerateAheadOfTime(Isolate* isolate) {
  TypeofStub stub(isolate);
  stub.GetCode();
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
  ElementHandlerCompiler::GenerateStoreSlow(masm);
}


// static
void StoreFastElementStub::GenerateAheadOfTime(Isolate* isolate) {
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

bool ToBooleanICStub::UpdateStatus(Handle<Object> object) {
  Types new_types = types();
  Types old_types = new_types;
  bool to_boolean_value = new_types.UpdateStatus(object);
  TraceTransition(old_types, new_types);
  set_sub_minor_key(TypesBits::update(sub_minor_key(), new_types.ToIntegral()));
  return to_boolean_value;
}

void ToBooleanICStub::PrintState(std::ostream& os) const {  // NOLINT
  os << types();
}

std::ostream& operator<<(std::ostream& os, const ToBooleanICStub::Types& s) {
  os << "(";
  SimpleListPrinter p(os);
  if (s.IsEmpty()) p.Add("None");
  if (s.Contains(ToBooleanICStub::UNDEFINED)) p.Add("Undefined");
  if (s.Contains(ToBooleanICStub::BOOLEAN)) p.Add("Bool");
  if (s.Contains(ToBooleanICStub::NULL_TYPE)) p.Add("Null");
  if (s.Contains(ToBooleanICStub::SMI)) p.Add("Smi");
  if (s.Contains(ToBooleanICStub::SPEC_OBJECT)) p.Add("SpecObject");
  if (s.Contains(ToBooleanICStub::STRING)) p.Add("String");
  if (s.Contains(ToBooleanICStub::SYMBOL)) p.Add("Symbol");
  if (s.Contains(ToBooleanICStub::HEAP_NUMBER)) p.Add("HeapNumber");
  if (s.Contains(ToBooleanICStub::SIMD_VALUE)) p.Add("SimdValue");
  return os << ")";
}

bool ToBooleanICStub::Types::UpdateStatus(Handle<Object> object) {
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
  } else if (object->IsJSReceiver()) {
    Add(SPEC_OBJECT);
    return !object->IsUndetectable();
  } else if (object->IsString()) {
    DCHECK(!object->IsUndetectable());
    Add(STRING);
    return String::cast(*object)->length() != 0;
  } else if (object->IsSymbol()) {
    Add(SYMBOL);
    return true;
  } else if (object->IsHeapNumber()) {
    DCHECK(!object->IsUndetectable());
    Add(HEAP_NUMBER);
    double value = HeapNumber::cast(*object)->value();
    return value != 0 && !std::isnan(value);
  } else if (object->IsSimd128Value()) {
    Add(SIMD_VALUE);
    return true;
  } else {
    // We should never see an internal object at runtime here!
    UNREACHABLE();
    return true;
  }
}

bool ToBooleanICStub::Types::NeedsMap() const {
  return Contains(ToBooleanICStub::SPEC_OBJECT) ||
         Contains(ToBooleanICStub::STRING) ||
         Contains(ToBooleanICStub::SYMBOL) ||
         Contains(ToBooleanICStub::HEAP_NUMBER) ||
         Contains(ToBooleanICStub::SIMD_VALUE);
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


Representation RepresentationFromType(Type* type) {
  if (type->Is(Type::UntaggedIntegral())) {
    return Representation::Integer32();
  }

  if (type->Is(Type::TaggedSigned())) {
    return Representation::Smi();
  }

  if (type->Is(Type::UntaggedPointer())) {
    return Representation::External();
  }

  DCHECK(!type->Is(Type::Untagged()));
  return Representation::Tagged();
}

}  // namespace internal
}  // namespace v8
