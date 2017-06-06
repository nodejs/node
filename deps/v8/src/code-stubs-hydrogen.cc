// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include <memory>

#include "src/assembler-inl.h"
#include "src/bailout-reason.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/crankshaft/hydrogen.h"
#include "src/crankshaft/lithium.h"
#include "src/field-index.h"
#include "src/ic/ic.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {


static LChunk* OptimizeGraph(HGraph* graph) {
  DisallowHeapAllocation no_allocation;
  DisallowHandleAllocation no_handles;
  DisallowHandleDereference no_deref;

  DCHECK(graph != NULL);
  BailoutReason bailout_reason = kNoReason;
  if (!graph->Optimize(&bailout_reason)) {
    FATAL(GetBailoutReason(bailout_reason));
  }
  LChunk* chunk = LChunk::NewChunk(graph);
  if (chunk == NULL) {
    FATAL(GetBailoutReason(graph->info()->bailout_reason()));
  }
  return chunk;
}


class CodeStubGraphBuilderBase : public HGraphBuilder {
 public:
  explicit CodeStubGraphBuilderBase(CompilationInfo* info, CodeStub* code_stub)
      : HGraphBuilder(info, code_stub->GetCallInterfaceDescriptor(), false),
        arguments_length_(NULL),
        info_(info),
        code_stub_(code_stub),
        descriptor_(code_stub),
        context_(NULL) {
    int parameter_count = GetParameterCount();
    parameters_.reset(new HParameter*[parameter_count]);
  }
  virtual bool BuildGraph();

 protected:
  virtual HValue* BuildCodeStub() = 0;
  int GetParameterCount() const { return descriptor_.GetParameterCount(); }
  int GetRegisterParameterCount() const {
    return descriptor_.GetRegisterParameterCount();
  }
  HParameter* GetParameter(int parameter) {
    DCHECK(parameter < GetParameterCount());
    return parameters_[parameter];
  }
  Representation GetParameterRepresentation(int parameter) {
    return RepresentationFromMachineType(
        descriptor_.GetParameterType(parameter));
  }
  bool IsParameterCountRegister(int index) const {
    return descriptor_.GetRegisterParameter(index)
        .is(descriptor_.stack_parameter_count());
  }
  HValue* GetArgumentsLength() {
    // This is initialized in BuildGraph()
    DCHECK(arguments_length_ != NULL);
    return arguments_length_;
  }
  CompilationInfo* info() { return info_; }
  CodeStub* stub() { return code_stub_; }
  HContext* context() { return context_; }
  Isolate* isolate() { return info_->isolate(); }

 private:
  std::unique_ptr<HParameter* []> parameters_;
  HValue* arguments_length_;
  CompilationInfo* info_;
  CodeStub* code_stub_;
  CodeStubDescriptor descriptor_;
  HContext* context_;
};


bool CodeStubGraphBuilderBase::BuildGraph() {
  // Update the static counter each time a new code stub is generated.
  isolate()->counters()->code_stubs()->Increment();

  if (FLAG_trace_hydrogen_stubs) {
    const char* name = CodeStub::MajorName(stub()->MajorKey());
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling stub %s using hydrogen\n", name);
    isolate()->GetHTracer()->TraceCompilation(info());
  }

  int param_count = GetParameterCount();
  int register_param_count = GetRegisterParameterCount();
  HEnvironment* start_environment = graph()->start_environment();
  HBasicBlock* next_block = CreateBasicBlock(start_environment);
  Goto(next_block);
  next_block->SetJoinId(BailoutId::StubEntry());
  set_current_block(next_block);

  bool runtime_stack_params = descriptor_.stack_parameter_count().is_valid();
  HInstruction* stack_parameter_count = NULL;
  for (int i = 0; i < param_count; ++i) {
    Representation r = GetParameterRepresentation(i);
    HParameter* param;
    if (i >= register_param_count) {
      param = Add<HParameter>(i - register_param_count,
                              HParameter::STACK_PARAMETER, r);
    } else {
      param = Add<HParameter>(i, HParameter::REGISTER_PARAMETER, r);
    }
    start_environment->Bind(i, param);
    parameters_[i] = param;
    if (i < register_param_count && IsParameterCountRegister(i)) {
      param->set_type(HType::Smi());
      stack_parameter_count = param;
      arguments_length_ = stack_parameter_count;
    }
  }

  DCHECK(!runtime_stack_params || arguments_length_ != NULL);
  if (!runtime_stack_params) {
    stack_parameter_count =
        Add<HConstant>(param_count - register_param_count - 1);
    // graph()->GetConstantMinus1();
    arguments_length_ = graph()->GetConstant0();
  }

  context_ = Add<HContext>();
  start_environment->BindContext(context_);
  start_environment->Bind(param_count, context_);

  Add<HSimulate>(BailoutId::StubEntry());

  NoObservableSideEffectsScope no_effects(this);

  HValue* return_value = BuildCodeStub();

  // We might have extra expressions to pop from the stack in addition to the
  // arguments above.
  HInstruction* stack_pop_count = stack_parameter_count;
  if (descriptor_.function_mode() == JS_FUNCTION_STUB_MODE) {
    if (!stack_parameter_count->IsConstant() &&
        descriptor_.hint_stack_parameter_count() < 0) {
      HInstruction* constant_one = graph()->GetConstant1();
      stack_pop_count = AddUncasted<HAdd>(stack_parameter_count, constant_one);
      stack_pop_count->ClearFlag(HValue::kCanOverflow);
      // TODO(mvstanton): verify that stack_parameter_count+1 really fits in a
      // smi.
    } else {
      int count = descriptor_.hint_stack_parameter_count();
      stack_pop_count = Add<HConstant>(count);
    }
  }

  if (current_block() != NULL) {
    HReturn* hreturn_instruction = New<HReturn>(return_value,
                                                stack_pop_count);
    FinishCurrentBlock(hreturn_instruction);
  }
  return true;
}


template <class Stub>
class CodeStubGraphBuilder: public CodeStubGraphBuilderBase {
 public:
  explicit CodeStubGraphBuilder(CompilationInfo* info, CodeStub* stub)
      : CodeStubGraphBuilderBase(info, stub) {}

  typedef typename Stub::Descriptor Descriptor;

 protected:
  virtual HValue* BuildCodeStub() {
    if (casted_stub()->IsUninitialized()) {
      return BuildCodeUninitializedStub();
    } else {
      return BuildCodeInitializedStub();
    }
  }

  virtual HValue* BuildCodeInitializedStub() {
    UNIMPLEMENTED();
    return NULL;
  }

  virtual HValue* BuildCodeUninitializedStub() {
    // Force a deopt that falls back to the runtime.
    HValue* undefined = graph()->GetConstantUndefined();
    IfBuilder builder(this);
    builder.IfNot<HCompareObjectEqAndBranch, HValue*>(undefined, undefined);
    builder.Then();
    builder.ElseDeopt(DeoptimizeReason::kForcedDeoptToRuntime);
    return undefined;
  }

  Stub* casted_stub() { return static_cast<Stub*>(stub()); }
};


Handle<Code> HydrogenCodeStub::GenerateLightweightMissCode(
    ExternalReference miss) {
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
    GenerateLightweightMiss(&masm, miss);
  }

  // Create the code object.
  CodeDesc desc;
  masm.GetCode(&desc);

  // Copy the generated code into a heap object.
  Handle<Code> new_object = factory->NewCode(
      desc, GetCodeFlags(), masm.CodeObject(), NeedsImmovableCode());
  return new_object;
}

Handle<Code> HydrogenCodeStub::GenerateRuntimeTailCall(
    CodeStubDescriptor* descriptor) {
  const char* name = CodeStub::MajorName(MajorKey());
  Zone zone(isolate()->allocator(), ZONE_NAME);
  CallInterfaceDescriptor interface_descriptor(GetCallInterfaceDescriptor());
  compiler::CodeAssemblerState state(isolate(), &zone, interface_descriptor,
                                     GetCodeFlags(), name);
  CodeStubAssembler assembler(&state);
  int total_params = interface_descriptor.GetStackParameterCount() +
                     interface_descriptor.GetRegisterParameterCount();
  switch (total_params) {
    case 0:
      assembler.TailCallRuntime(descriptor->miss_handler_id(),
                                assembler.Parameter(0));
      break;
    case 1:
      assembler.TailCallRuntime(descriptor->miss_handler_id(),
                                assembler.Parameter(1), assembler.Parameter(0));
      break;
    case 2:
      assembler.TailCallRuntime(descriptor->miss_handler_id(),
                                assembler.Parameter(2), assembler.Parameter(0),
                                assembler.Parameter(1));
      break;
    case 3:
      assembler.TailCallRuntime(descriptor->miss_handler_id(),
                                assembler.Parameter(3), assembler.Parameter(0),
                                assembler.Parameter(1), assembler.Parameter(2));
      break;
    case 4:
      assembler.TailCallRuntime(descriptor->miss_handler_id(),
                                assembler.Parameter(4), assembler.Parameter(0),
                                assembler.Parameter(1), assembler.Parameter(2),
                                assembler.Parameter(3));
      break;
    default:
      UNIMPLEMENTED();
      break;
  }
  return compiler::CodeAssembler::GenerateCode(&state);
}

template <class Stub>
static Handle<Code> DoGenerateCode(Stub* stub) {
  Isolate* isolate = stub->isolate();
  CodeStubDescriptor descriptor(stub);

  if (FLAG_minimal && descriptor.has_miss_handler()) {
    return stub->GenerateRuntimeTailCall(&descriptor);
  }

  // If we are uninitialized we can use a light-weight stub to enter
  // the runtime that is significantly faster than using the standard
  // stub-failure deopt mechanism.
  if (stub->IsUninitialized() && descriptor.has_miss_handler()) {
    DCHECK(!descriptor.stack_parameter_count().is_valid());
    return stub->GenerateLightweightMissCode(descriptor.miss_handler());
  }
  base::ElapsedTimer timer;
  if (FLAG_profile_hydrogen_code_stub_compilation) {
    timer.Start();
  }
  Zone zone(isolate->allocator(), ZONE_NAME);
  CompilationInfo info(CStrVector(CodeStub::MajorName(stub->MajorKey())),
                       isolate, &zone, stub->GetCodeFlags());
  // Parameter count is number of stack parameters.
  int parameter_count = descriptor.GetStackParameterCount();
  if (descriptor.function_mode() == NOT_JS_FUNCTION_STUB_MODE) {
    parameter_count--;
  }
  info.set_parameter_count(parameter_count);
  CodeStubGraphBuilder<Stub> builder(&info, stub);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  Handle<Code> code = chunk->Codegen();
  if (FLAG_profile_hydrogen_code_stub_compilation) {
    OFStream os(stdout);
    os << "[Lazy compilation of " << stub << " took "
       << timer.Elapsed().InMillisecondsF() << " ms]" << std::endl;
  }
  return code;
}

template <>
HValue* CodeStubGraphBuilder<TransitionElementsKindStub>::BuildCodeStub() {
  ElementsKind const from_kind = casted_stub()->from_kind();
  ElementsKind const to_kind = casted_stub()->to_kind();
  HValue* const object = GetParameter(Descriptor::kObject);
  HValue* const map = GetParameter(Descriptor::kMap);

  // The {object} is known to be a JSObject (otherwise it wouldn't have elements
  // anyways).
  object->set_type(HType::JSObject());

  info()->MarkAsSavesCallerDoubles();

  DCHECK_IMPLIES(IsFastHoleyElementsKind(from_kind),
                 IsFastHoleyElementsKind(to_kind));

  if (AllocationSite::GetMode(from_kind, to_kind) == TRACK_ALLOCATION_SITE) {
    Add<HTrapAllocationMemento>(object);
  }

  if (!IsSimpleMapChangeTransition(from_kind, to_kind)) {
    HInstruction* elements = AddLoadElements(object);

    IfBuilder if_objecthaselements(this);
    if_objecthaselements.IfNot<HCompareObjectEqAndBranch>(
        elements, Add<HConstant>(isolate()->factory()->empty_fixed_array()));
    if_objecthaselements.Then();
    {
      // Determine the elements capacity.
      HInstruction* elements_length = AddLoadFixedArrayLength(elements);

      // Determine the effective (array) length.
      IfBuilder if_objectisarray(this);
      if_objectisarray.If<HHasInstanceTypeAndBranch>(object, JS_ARRAY_TYPE);
      if_objectisarray.Then();
      {
        // The {object} is a JSArray, load the special "length" property.
        Push(Add<HLoadNamedField>(object, nullptr,
                                  HObjectAccess::ForArrayLength(from_kind)));
      }
      if_objectisarray.Else();
      {
        // The {object} is some other JSObject.
        Push(elements_length);
      }
      if_objectisarray.End();
      HValue* length = Pop();

      BuildGrowElementsCapacity(object, elements, from_kind, to_kind, length,
                                elements_length);
    }
    if_objecthaselements.End();
  }

  Add<HStoreNamedField>(object, HObjectAccess::ForMap(), map);

  return object;
}


Handle<Code> TransitionElementsKindStub::GenerateCode() {
  return DoGenerateCode(this);
}

template <>
HValue* CodeStubGraphBuilder<BinaryOpICStub>::BuildCodeInitializedStub() {
  BinaryOpICState state = casted_stub()->state();

  HValue* left = GetParameter(Descriptor::kLeft);
  HValue* right = GetParameter(Descriptor::kRight);

  AstType* left_type = state.GetLeftType();
  AstType* right_type = state.GetRightType();
  AstType* result_type = state.GetResultType();

  DCHECK(!left_type->Is(AstType::None()) && !right_type->Is(AstType::None()) &&
         (state.HasSideEffects() || !result_type->Is(AstType::None())));

  HValue* result = NULL;
  HAllocationMode allocation_mode(NOT_TENURED);
  if (state.op() == Token::ADD && (left_type->Maybe(AstType::String()) ||
                                   right_type->Maybe(AstType::String())) &&
      !left_type->Is(AstType::String()) && !right_type->Is(AstType::String())) {
    // For the generic add stub a fast case for string addition is performance
    // critical.
    if (left_type->Maybe(AstType::String())) {
      IfBuilder if_leftisstring(this);
      if_leftisstring.If<HIsStringAndBranch>(left);
      if_leftisstring.Then();
      {
        Push(BuildBinaryOperation(state.op(), left, right, AstType::String(),
                                  right_type, result_type,
                                  state.fixed_right_arg(), allocation_mode));
      }
      if_leftisstring.Else();
      {
        Push(BuildBinaryOperation(state.op(), left, right, left_type,
                                  right_type, result_type,
                                  state.fixed_right_arg(), allocation_mode));
      }
      if_leftisstring.End();
      result = Pop();
    } else {
      IfBuilder if_rightisstring(this);
      if_rightisstring.If<HIsStringAndBranch>(right);
      if_rightisstring.Then();
      {
        Push(BuildBinaryOperation(state.op(), left, right, left_type,
                                  AstType::String(), result_type,
                                  state.fixed_right_arg(), allocation_mode));
      }
      if_rightisstring.Else();
      {
        Push(BuildBinaryOperation(state.op(), left, right, left_type,
                                  right_type, result_type,
                                  state.fixed_right_arg(), allocation_mode));
      }
      if_rightisstring.End();
      result = Pop();
    }
  } else {
    result = BuildBinaryOperation(state.op(), left, right, left_type,
                                  right_type, result_type,
                                  state.fixed_right_arg(), allocation_mode);
  }

  // If we encounter a generic argument, the number conversion is
  // observable, thus we cannot afford to bail out after the fact.
  if (!state.HasSideEffects()) {
    result = EnforceNumberType(result, result_type);
  }

  return result;
}


Handle<Code> BinaryOpICStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<BinaryOpWithAllocationSiteStub>::BuildCodeStub() {
  BinaryOpICState state = casted_stub()->state();

  HValue* allocation_site = GetParameter(Descriptor::kAllocationSite);
  HValue* left = GetParameter(Descriptor::kLeft);
  HValue* right = GetParameter(Descriptor::kRight);

  AstType* left_type = state.GetLeftType();
  AstType* right_type = state.GetRightType();
  AstType* result_type = state.GetResultType();
  HAllocationMode allocation_mode(allocation_site);

  return BuildBinaryOperation(state.op(), left, right, left_type, right_type,
                              result_type, state.fixed_right_arg(),
                              allocation_mode);
}


Handle<Code> BinaryOpWithAllocationSiteStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ToBooleanICStub>::BuildCodeInitializedStub() {
  ToBooleanICStub* stub = casted_stub();
  IfBuilder if_true(this);
  if_true.If<HBranch>(GetParameter(Descriptor::kArgument), stub->hints());
  if_true.Then();
  if_true.Return(graph()->GetConstantTrue());
  if_true.Else();
  if_true.End();
  return graph()->GetConstantFalse();
}

Handle<Code> ToBooleanICStub::GenerateCode() { return DoGenerateCode(this); }

}  // namespace internal
}  // namespace v8
