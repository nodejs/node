// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include <memory>

#include "src/bailout-reason.h"
#include "src/code-factory.h"
#include "src/crankshaft/hydrogen.h"
#include "src/crankshaft/lithium.h"
#include "src/field-index.h"
#include "src/ic/ic.h"

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

  HLoadNamedField* BuildLoadNamedField(HValue* object, FieldIndex index);
  void BuildStoreNamedField(HValue* object, HValue* value, FieldIndex index,
                            Representation representation,
                            bool transition_to_field);

  HValue* BuildPushElement(HValue* object, HValue* argc,
                           HValue* argument_elements, ElementsKind kind);

  HValue* BuildToString(HValue* input, bool convert);
  HValue* BuildToPrimitive(HValue* input, HValue* input_map);

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
  Zone zone(isolate()->allocator());
  CallInterfaceDescriptor interface_descriptor(GetCallInterfaceDescriptor());
  CodeStubAssembler assembler(isolate(), &zone, interface_descriptor,
                              GetCodeFlags(), name);
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
  return assembler.GenerateCode();
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
  Zone zone(isolate->allocator());
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
HValue* CodeStubGraphBuilder<NumberToStringStub>::BuildCodeStub() {
  info()->MarkAsSavesCallerDoubles();
  HValue* number = GetParameter(Descriptor::kArgument);
  return BuildNumberToString(number, AstType::Number());
}


Handle<Code> NumberToStringStub::GenerateCode() {
  return DoGenerateCode(this);
}

HValue* CodeStubGraphBuilderBase::BuildPushElement(HValue* object, HValue* argc,
                                                   HValue* argument_elements,
                                                   ElementsKind kind) {
  // Precheck whether all elements fit into the array.
  if (!IsFastObjectElementsKind(kind)) {
    LoopBuilder builder(this, context(), LoopBuilder::kPostIncrement);
    HValue* start = graph()->GetConstant0();
    HValue* key = builder.BeginBody(start, argc, Token::LT);
    {
      HInstruction* argument =
          Add<HAccessArgumentsAt>(argument_elements, argc, key);
      IfBuilder can_store(this);
      can_store.IfNot<HIsSmiAndBranch>(argument);
      if (IsFastDoubleElementsKind(kind)) {
        can_store.And();
        can_store.IfNot<HCompareMap>(argument,
                                     isolate()->factory()->heap_number_map());
      }
      can_store.ThenDeopt(DeoptimizeReason::kFastPathFailed);
      can_store.End();
    }
    builder.EndBody();
  }

  HValue* length = Add<HLoadNamedField>(object, nullptr,
                                        HObjectAccess::ForArrayLength(kind));
  HValue* new_length = AddUncasted<HAdd>(length, argc);
  HValue* max_key = AddUncasted<HSub>(new_length, graph()->GetConstant1());

  HValue* elements = Add<HLoadNamedField>(object, nullptr,
                                          HObjectAccess::ForElementsPointer());
  elements = BuildCheckForCapacityGrow(object, elements, kind, length, max_key,
                                       true, STORE);

  LoopBuilder builder(this, context(), LoopBuilder::kPostIncrement);
  HValue* start = graph()->GetConstant0();
  HValue* key = builder.BeginBody(start, argc, Token::LT);
  {
    HValue* argument = Add<HAccessArgumentsAt>(argument_elements, argc, key);
    HValue* index = AddUncasted<HAdd>(key, length);
    AddElementAccess(elements, index, argument, object, nullptr, kind, STORE);
  }
  builder.EndBody();
  return new_length;
}

template <>
HValue* CodeStubGraphBuilder<FastArrayPushStub>::BuildCodeStub() {
  // TODO(verwaest): Fix deoptimizer messages.
  HValue* argc = GetArgumentsLength();

  HInstruction* argument_elements = Add<HArgumentsElements>(false, false);
  HInstruction* object = Add<HAccessArgumentsAt>(argument_elements, argc,
                                                 graph()->GetConstantMinus1());
  BuildCheckHeapObject(object);
  HValue* map = Add<HLoadNamedField>(object, nullptr, HObjectAccess::ForMap());
  Add<HCheckInstanceType>(object, HCheckInstanceType::IS_JS_ARRAY);

  // Disallow pushing onto prototypes. It might be the JSArray prototype.
  // Disallow pushing onto non-extensible objects.
  {
    HValue* bit_field2 =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField2());
    HValue* mask =
        Add<HConstant>(static_cast<int>(Map::IsPrototypeMapBits::kMask) |
                       (1 << Map::kIsExtensible));
    HValue* bits = AddUncasted<HBitwise>(Token::BIT_AND, bit_field2, mask);
    IfBuilder check(this);
    check.If<HCompareNumericAndBranch>(
        bits, Add<HConstant>(1 << Map::kIsExtensible), Token::NE);
    check.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    check.End();
  }

  // Disallow pushing onto arrays in dictionary named property mode. We need to
  // figure out whether the length property is still writable.
  {
    HValue* bit_field3 =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField3());
    HValue* mask = Add<HConstant>(static_cast<int>(Map::DictionaryMap::kMask));
    HValue* bit = AddUncasted<HBitwise>(Token::BIT_AND, bit_field3, mask);
    IfBuilder check(this);
    check.If<HCompareNumericAndBranch>(bit, mask, Token::EQ);
    check.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    check.End();
  }

  // Check whether the length property is writable. The length property is the
  // only default named property on arrays. It's nonconfigurable, hence is
  // guaranteed to stay the first property.
  {
    HValue* descriptors =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapDescriptors());
    HValue* details = Add<HLoadKeyed>(
        descriptors, Add<HConstant>(DescriptorArray::ToDetailsIndex(0)),
        nullptr, nullptr, FAST_SMI_ELEMENTS);
    HValue* mask =
        Add<HConstant>(READ_ONLY << PropertyDetails::AttributesField::kShift);
    HValue* bit = AddUncasted<HBitwise>(Token::BIT_AND, details, mask);
    IfBuilder readonly(this);
    readonly.If<HCompareNumericAndBranch>(bit, mask, Token::EQ);
    readonly.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    readonly.End();
  }

  HValue* null = Add<HLoadRoot>(Heap::kNullValueRootIndex);
  HValue* empty = Add<HLoadRoot>(Heap::kEmptyFixedArrayRootIndex);
  environment()->Push(map);
  LoopBuilder check_prototypes(this);
  check_prototypes.BeginBody(1);
  {
    HValue* parent_map = environment()->Pop();
    HValue* prototype = Add<HLoadNamedField>(parent_map, nullptr,
                                             HObjectAccess::ForPrototype());

    IfBuilder is_null(this);
    is_null.If<HCompareObjectEqAndBranch>(prototype, null);
    is_null.Then();
    check_prototypes.Break();
    is_null.End();

    HValue* prototype_map =
        Add<HLoadNamedField>(prototype, nullptr, HObjectAccess::ForMap());
    HValue* instance_type = Add<HLoadNamedField>(
        prototype_map, nullptr, HObjectAccess::ForMapInstanceType());
    IfBuilder check_instance_type(this);
    check_instance_type.If<HCompareNumericAndBranch>(
        instance_type, Add<HConstant>(LAST_CUSTOM_ELEMENTS_RECEIVER),
        Token::LTE);
    check_instance_type.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    check_instance_type.End();

    HValue* elements = Add<HLoadNamedField>(
        prototype, nullptr, HObjectAccess::ForElementsPointer());
    IfBuilder no_elements(this);
    no_elements.IfNot<HCompareObjectEqAndBranch>(elements, empty);
    no_elements.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    no_elements.End();

    environment()->Push(prototype_map);
  }
  check_prototypes.EndBody();

  HValue* bit_field2 =
      Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField2());
  HValue* kind = BuildDecodeField<Map::ElementsKindBits>(bit_field2);

  // Below we only check the upper bound of the relevant ranges to include both
  // holey and non-holey versions. We check them in order smi, object, double
  // since smi < object < double.
  STATIC_ASSERT(FAST_SMI_ELEMENTS < FAST_HOLEY_SMI_ELEMENTS);
  STATIC_ASSERT(FAST_HOLEY_SMI_ELEMENTS < FAST_HOLEY_ELEMENTS);
  STATIC_ASSERT(FAST_ELEMENTS < FAST_HOLEY_ELEMENTS);
  STATIC_ASSERT(FAST_HOLEY_ELEMENTS < FAST_HOLEY_DOUBLE_ELEMENTS);
  STATIC_ASSERT(FAST_DOUBLE_ELEMENTS < FAST_HOLEY_DOUBLE_ELEMENTS);
  IfBuilder has_smi_elements(this);
  has_smi_elements.If<HCompareNumericAndBranch>(
      kind, Add<HConstant>(FAST_HOLEY_SMI_ELEMENTS), Token::LTE);
  has_smi_elements.Then();
  {
    HValue* new_length = BuildPushElement(object, argc, argument_elements,
                                          FAST_HOLEY_SMI_ELEMENTS);
    environment()->Push(new_length);
  }
  has_smi_elements.Else();
  {
    IfBuilder has_object_elements(this);
    has_object_elements.If<HCompareNumericAndBranch>(
        kind, Add<HConstant>(FAST_HOLEY_ELEMENTS), Token::LTE);
    has_object_elements.Then();
    {
      HValue* new_length = BuildPushElement(object, argc, argument_elements,
                                            FAST_HOLEY_ELEMENTS);
      environment()->Push(new_length);
    }
    has_object_elements.Else();
    {
      IfBuilder has_double_elements(this);
      has_double_elements.If<HCompareNumericAndBranch>(
          kind, Add<HConstant>(FAST_HOLEY_DOUBLE_ELEMENTS), Token::LTE);
      has_double_elements.Then();
      {
        HValue* new_length = BuildPushElement(object, argc, argument_elements,
                                              FAST_HOLEY_DOUBLE_ELEMENTS);
        environment()->Push(new_length);
      }
      has_double_elements.ElseDeopt(DeoptimizeReason::kFastPathFailed);
      has_double_elements.End();
    }
    has_object_elements.End();
  }
  has_smi_elements.End();

  return environment()->Pop();
}

Handle<Code> FastArrayPushStub::GenerateCode() { return DoGenerateCode(this); }

template <>
HValue* CodeStubGraphBuilder<FastFunctionBindStub>::BuildCodeStub() {
  // TODO(verwaest): Fix deoptimizer messages.
  HValue* argc = GetArgumentsLength();
  HInstruction* argument_elements = Add<HArgumentsElements>(false, false);
  HInstruction* object = Add<HAccessArgumentsAt>(argument_elements, argc,
                                                 graph()->GetConstantMinus1());
  BuildCheckHeapObject(object);
  HValue* map = Add<HLoadNamedField>(object, nullptr, HObjectAccess::ForMap());
  Add<HCheckInstanceType>(object, HCheckInstanceType::IS_JS_FUNCTION);

  // Disallow binding of slow-mode functions. We need to figure out whether the
  // length and name property are in the original state.
  {
    HValue* bit_field3 =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField3());
    HValue* mask = Add<HConstant>(static_cast<int>(Map::DictionaryMap::kMask));
    HValue* bit = AddUncasted<HBitwise>(Token::BIT_AND, bit_field3, mask);
    IfBuilder check(this);
    check.If<HCompareNumericAndBranch>(bit, mask, Token::EQ);
    check.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    check.End();
  }

  // Check whether the length and name properties are still present as
  // AccessorInfo objects. In that case, their value can be recomputed even if
  // the actual value on the object changes.
  {
    HValue* descriptors =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapDescriptors());

    HValue* descriptors_length = Add<HLoadNamedField>(
        descriptors, nullptr, HObjectAccess::ForFixedArrayLength());
    IfBuilder range(this);
    range.If<HCompareNumericAndBranch>(descriptors_length,
                                       graph()->GetConstant1(), Token::LTE);
    range.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    range.End();

    // Verify .length.
    const int length_index = JSFunction::kLengthDescriptorIndex;
    HValue* maybe_length = Add<HLoadKeyed>(
        descriptors, Add<HConstant>(DescriptorArray::ToKeyIndex(length_index)),
        nullptr, nullptr, FAST_ELEMENTS);
    Unique<Name> length_string = Unique<Name>::CreateUninitialized(
        isolate()->factory()->length_string());
    Add<HCheckValue>(maybe_length, length_string, false);

    HValue* maybe_length_accessor = Add<HLoadKeyed>(
        descriptors,
        Add<HConstant>(DescriptorArray::ToValueIndex(length_index)), nullptr,
        nullptr, FAST_ELEMENTS);
    BuildCheckHeapObject(maybe_length_accessor);
    Add<HCheckMaps>(maybe_length_accessor,
                    isolate()->factory()->accessor_info_map());

    // Verify .name.
    const int name_index = JSFunction::kNameDescriptorIndex;
    HValue* maybe_name = Add<HLoadKeyed>(
        descriptors, Add<HConstant>(DescriptorArray::ToKeyIndex(name_index)),
        nullptr, nullptr, FAST_ELEMENTS);
    Unique<Name> name_string =
        Unique<Name>::CreateUninitialized(isolate()->factory()->name_string());
    Add<HCheckValue>(maybe_name, name_string, false);

    HValue* maybe_name_accessor = Add<HLoadKeyed>(
        descriptors, Add<HConstant>(DescriptorArray::ToValueIndex(name_index)),
        nullptr, nullptr, FAST_ELEMENTS);
    BuildCheckHeapObject(maybe_name_accessor);
    Add<HCheckMaps>(maybe_name_accessor,
                    isolate()->factory()->accessor_info_map());
  }

  // Choose the right bound function map based on whether the target is
  // constructable.
  {
    HValue* bit_field =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField());
    HValue* mask = Add<HConstant>(static_cast<int>(1 << Map::kIsConstructor));
    HValue* bits = AddUncasted<HBitwise>(Token::BIT_AND, bit_field, mask);

    HValue* native_context = BuildGetNativeContext();
    IfBuilder is_constructor(this);
    is_constructor.If<HCompareNumericAndBranch>(bits, mask, Token::EQ);
    is_constructor.Then();
    {
      HValue* map = Add<HLoadNamedField>(
          native_context, nullptr,
          HObjectAccess::ForContextSlot(
              Context::BOUND_FUNCTION_WITH_CONSTRUCTOR_MAP_INDEX));
      environment()->Push(map);
    }
    is_constructor.Else();
    {
      HValue* map = Add<HLoadNamedField>(
          native_context, nullptr,
          HObjectAccess::ForContextSlot(
              Context::BOUND_FUNCTION_WITHOUT_CONSTRUCTOR_MAP_INDEX));
      environment()->Push(map);
    }
    is_constructor.End();
  }
  HValue* bound_function_map = environment()->Pop();

  // Verify that __proto__ matches that of a the target bound function.
  {
    HValue* prototype =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForPrototype());
    HValue* expected_prototype = Add<HLoadNamedField>(
        bound_function_map, nullptr, HObjectAccess::ForPrototype());
    IfBuilder equal_prototype(this);
    equal_prototype.IfNot<HCompareObjectEqAndBranch>(prototype,
                                                     expected_prototype);
    equal_prototype.ThenDeopt(DeoptimizeReason::kFastPathFailed);
    equal_prototype.End();
  }

  // Allocate the arguments array.
  IfBuilder empty_args(this);
  empty_args.If<HCompareNumericAndBranch>(argc, graph()->GetConstant1(),
                                          Token::LTE);
  empty_args.Then();
  { environment()->Push(Add<HLoadRoot>(Heap::kEmptyFixedArrayRootIndex)); }
  empty_args.Else();
  {
    HValue* elements_length = AddUncasted<HSub>(argc, graph()->GetConstant1());
    HValue* elements =
        BuildAllocateAndInitializeArray(FAST_ELEMENTS, elements_length);

    LoopBuilder builder(this, context(), LoopBuilder::kPostIncrement);
    HValue* start = graph()->GetConstant1();
    HValue* key = builder.BeginBody(start, argc, Token::LT);
    {
      HValue* argument = Add<HAccessArgumentsAt>(argument_elements, argc, key);
      HValue* index = AddUncasted<HSub>(key, graph()->GetConstant1());
      AddElementAccess(elements, index, argument, elements, nullptr,
                       FAST_ELEMENTS, STORE);
    }
    builder.EndBody();
    environment()->Push(elements);
  }
  empty_args.End();
  HValue* elements = environment()->Pop();

  // Find the 'this' to bind.
  IfBuilder no_receiver(this);
  no_receiver.If<HCompareNumericAndBranch>(argc, graph()->GetConstant0(),
                                           Token::EQ);
  no_receiver.Then();
  { environment()->Push(Add<HLoadRoot>(Heap::kUndefinedValueRootIndex)); }
  no_receiver.Else();
  {
    environment()->Push(Add<HAccessArgumentsAt>(argument_elements, argc,
                                                graph()->GetConstant0()));
  }
  no_receiver.End();
  HValue* receiver = environment()->Pop();

  // Allocate the resulting bound function.
  HValue* size = Add<HConstant>(JSBoundFunction::kSize);
  HValue* bound_function =
      Add<HAllocate>(size, HType::JSObject(), NOT_TENURED,
                     JS_BOUND_FUNCTION_TYPE, graph()->GetConstant0());
  Add<HStoreNamedField>(bound_function, HObjectAccess::ForMap(),
                        bound_function_map);
  HValue* empty_fixed_array = Add<HLoadRoot>(Heap::kEmptyFixedArrayRootIndex);
  Add<HStoreNamedField>(bound_function, HObjectAccess::ForPropertiesPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(bound_function, HObjectAccess::ForElementsPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(bound_function, HObjectAccess::ForBoundTargetFunction(),
                        object);

  Add<HStoreNamedField>(bound_function, HObjectAccess::ForBoundThis(),
                        receiver);
  Add<HStoreNamedField>(bound_function, HObjectAccess::ForBoundArguments(),
                        elements);

  return bound_function;
}

Handle<Code> FastFunctionBindStub::GenerateCode() {
  return DoGenerateCode(this);
}

template <>
HValue* CodeStubGraphBuilder<LoadFastElementStub>::BuildCodeStub() {
  LoadKeyedHoleMode hole_mode = casted_stub()->convert_hole_to_undefined()
                                    ? CONVERT_HOLE_TO_UNDEFINED
                                    : NEVER_RETURN_HOLE;

  HInstruction* load = BuildUncheckedMonomorphicElementAccess(
      GetParameter(Descriptor::kReceiver), GetParameter(Descriptor::kName),
      NULL, casted_stub()->is_js_array(), casted_stub()->elements_kind(), LOAD,
      hole_mode, STANDARD_STORE);
  return load;
}


Handle<Code> LoadFastElementStub::GenerateCode() {
  return DoGenerateCode(this);
}


HLoadNamedField* CodeStubGraphBuilderBase::BuildLoadNamedField(
    HValue* object, FieldIndex index) {
  Representation representation = index.is_double()
      ? Representation::Double()
      : Representation::Tagged();
  int offset = index.offset();
  HObjectAccess access = index.is_inobject()
      ? HObjectAccess::ForObservableJSObjectOffset(offset, representation)
      : HObjectAccess::ForBackingStoreOffset(offset, representation);
  if (index.is_double() &&
      (!FLAG_unbox_double_fields || !index.is_inobject())) {
    // Load the heap number.
    object = Add<HLoadNamedField>(
        object, nullptr, access.WithRepresentation(Representation::Tagged()));
    // Load the double value from it.
    access = HObjectAccess::ForHeapNumberValue();
  }
  return Add<HLoadNamedField>(object, nullptr, access);
}


template<>
HValue* CodeStubGraphBuilder<LoadFieldStub>::BuildCodeStub() {
  return BuildLoadNamedField(GetParameter(Descriptor::kReceiver),
                             casted_stub()->index());
}


Handle<Code> LoadFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<LoadConstantStub>::BuildCodeStub() {
  HValue* map = AddLoadMap(GetParameter(Descriptor::kReceiver), NULL);
  HObjectAccess descriptors_access = HObjectAccess::ForObservableJSObjectOffset(
      Map::kDescriptorsOffset, Representation::Tagged());
  HValue* descriptors = Add<HLoadNamedField>(map, nullptr, descriptors_access);
  HObjectAccess value_access = HObjectAccess::ForObservableJSObjectOffset(
      DescriptorArray::GetValueOffset(casted_stub()->constant_index()));
  return Add<HLoadNamedField>(descriptors, nullptr, value_access);
}


Handle<Code> LoadConstantStub::GenerateCode() { return DoGenerateCode(this); }


void CodeStubGraphBuilderBase::BuildStoreNamedField(
    HValue* object, HValue* value, FieldIndex index,
    Representation representation, bool transition_to_field) {
  DCHECK(!index.is_double() || representation.IsDouble());
  int offset = index.offset();
  HObjectAccess access =
      index.is_inobject()
          ? HObjectAccess::ForObservableJSObjectOffset(offset, representation)
          : HObjectAccess::ForBackingStoreOffset(offset, representation);

  if (representation.IsDouble()) {
    if (!FLAG_unbox_double_fields || !index.is_inobject()) {
      HObjectAccess heap_number_access =
          access.WithRepresentation(Representation::Tagged());
      if (transition_to_field) {
        // The store requires a mutable HeapNumber to be allocated.
        NoObservableSideEffectsScope no_side_effects(this);
        HInstruction* heap_number_size = Add<HConstant>(HeapNumber::kSize);

        // TODO(hpayer): Allocation site pretenuring support.
        HInstruction* heap_number =
            Add<HAllocate>(heap_number_size, HType::HeapObject(), NOT_TENURED,
                           MUTABLE_HEAP_NUMBER_TYPE, graph()->GetConstant0());
        AddStoreMapConstant(heap_number,
                            isolate()->factory()->mutable_heap_number_map());
        Add<HStoreNamedField>(heap_number, HObjectAccess::ForHeapNumberValue(),
                              value);
        // Store the new mutable heap number into the object.
        access = heap_number_access;
        value = heap_number;
      } else {
        // Load the heap number.
        object = Add<HLoadNamedField>(object, nullptr, heap_number_access);
        // Store the double value into it.
        access = HObjectAccess::ForHeapNumberValue();
      }
    }
  } else if (representation.IsHeapObject()) {
    BuildCheckHeapObject(value);
  }

  Add<HStoreNamedField>(object, access, value, INITIALIZING_STORE);
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


HValue* CodeStubGraphBuilderBase::BuildToString(HValue* input, bool convert) {
  if (!convert) return BuildCheckString(input);
  IfBuilder if_inputissmi(this);
  HValue* inputissmi = if_inputissmi.If<HIsSmiAndBranch>(input);
  if_inputissmi.Then();
  {
    // Convert the input smi to a string.
    Push(BuildNumberToString(input, AstType::SignedSmall()));
  }
  if_inputissmi.Else();
  {
    HValue* input_map =
        Add<HLoadNamedField>(input, inputissmi, HObjectAccess::ForMap());
    HValue* input_instance_type = Add<HLoadNamedField>(
        input_map, inputissmi, HObjectAccess::ForMapInstanceType());
    IfBuilder if_inputisstring(this);
    if_inputisstring.If<HCompareNumericAndBranch>(
        input_instance_type, Add<HConstant>(FIRST_NONSTRING_TYPE), Token::LT);
    if_inputisstring.Then();
    {
      // The input is already a string.
      Push(input);
    }
    if_inputisstring.Else();
    {
      // Convert to primitive first (if necessary), see
      // ES6 section 12.7.3 The Addition operator.
      IfBuilder if_inputisprimitive(this);
      STATIC_ASSERT(FIRST_PRIMITIVE_TYPE == FIRST_TYPE);
      if_inputisprimitive.If<HCompareNumericAndBranch>(
          input_instance_type, Add<HConstant>(LAST_PRIMITIVE_TYPE), Token::LTE);
      if_inputisprimitive.Then();
      {
        // The input is already a primitive.
        Push(input);
      }
      if_inputisprimitive.Else();
      {
        // Convert the input to a primitive.
        Push(BuildToPrimitive(input, input_map));
      }
      if_inputisprimitive.End();
      // Convert the primitive to a string value.
      HValue* values[] = {context(), Pop()};
      Callable toString = CodeFactory::ToString(isolate());
      Push(AddUncasted<HCallWithDescriptor>(Add<HConstant>(toString.code()), 0,
                                            toString.descriptor(),
                                            ArrayVector(values)));
    }
    if_inputisstring.End();
  }
  if_inputissmi.End();
  return Pop();
}


HValue* CodeStubGraphBuilderBase::BuildToPrimitive(HValue* input,
                                                   HValue* input_map) {
  // Get the native context of the caller.
  HValue* native_context = BuildGetNativeContext();

  // Determine the initial map of the %ObjectPrototype%.
  HValue* object_function_prototype_map =
      Add<HLoadNamedField>(native_context, nullptr,
                           HObjectAccess::ForContextSlot(
                               Context::OBJECT_FUNCTION_PROTOTYPE_MAP_INDEX));

  // Determine the initial map of the %StringPrototype%.
  HValue* string_function_prototype_map =
      Add<HLoadNamedField>(native_context, nullptr,
                           HObjectAccess::ForContextSlot(
                               Context::STRING_FUNCTION_PROTOTYPE_MAP_INDEX));

  // Determine the initial map of the String function.
  HValue* string_function = Add<HLoadNamedField>(
      native_context, nullptr,
      HObjectAccess::ForContextSlot(Context::STRING_FUNCTION_INDEX));
  HValue* string_function_initial_map = Add<HLoadNamedField>(
      string_function, nullptr, HObjectAccess::ForPrototypeOrInitialMap());

  // Determine the map of the [[Prototype]] of {input}.
  HValue* input_prototype =
      Add<HLoadNamedField>(input_map, nullptr, HObjectAccess::ForPrototype());
  HValue* input_prototype_map =
      Add<HLoadNamedField>(input_prototype, nullptr, HObjectAccess::ForMap());

  // For string wrappers (JSValue instances with [[StringData]] internal
  // fields), we can shortcirciut the ToPrimitive if
  //
  //  (a) the {input} map matches the initial map of the String function,
  //  (b) the {input} [[Prototype]] is the unmodified %StringPrototype% (i.e.
  //      no one monkey-patched toString, @@toPrimitive or valueOf), and
  //  (c) the %ObjectPrototype% (i.e. the [[Prototype]] of the
  //      %StringPrototype%) is also unmodified, that is no one sneaked a
  //      @@toPrimitive into the %ObjectPrototype%.
  //
  // If all these assumptions hold, we can just take the [[StringData]] value
  // and return it.
  // TODO(bmeurer): This just repairs a regression introduced by removing the
  // weird (and broken) intrinsic %_IsStringWrapperSafeForDefaultValue, which
  // was intendend to something similar to this, although less efficient and
  // wrong in the presence of @@toPrimitive. Long-term we might want to move
  // into the direction of having a ToPrimitiveStub that can do common cases
  // while staying in JavaScript land (i.e. not going to C++).
  IfBuilder if_inputisstringwrapper(this);
  if_inputisstringwrapper.If<HCompareObjectEqAndBranch>(
      input_map, string_function_initial_map);
  if_inputisstringwrapper.And();
  if_inputisstringwrapper.If<HCompareObjectEqAndBranch>(
      input_prototype_map, string_function_prototype_map);
  if_inputisstringwrapper.And();
  if_inputisstringwrapper.If<HCompareObjectEqAndBranch>(
      Add<HLoadNamedField>(Add<HLoadNamedField>(input_prototype_map, nullptr,
                                                HObjectAccess::ForPrototype()),
                           nullptr, HObjectAccess::ForMap()),
      object_function_prototype_map);
  if_inputisstringwrapper.Then();
  {
    Push(BuildLoadNamedField(
        input, FieldIndex::ForInObjectOffset(JSValue::kValueOffset)));
  }
  if_inputisstringwrapper.Else();
  {
    // TODO(bmeurer): Add support for fast ToPrimitive conversion using
    // a dedicated ToPrimitiveStub.
    Add<HPushArguments>(input);
    Push(Add<HCallRuntime>(Runtime::FunctionForId(Runtime::kToPrimitive), 1));
  }
  if_inputisstringwrapper.End();
  return Pop();
}


template <>
HValue* CodeStubGraphBuilder<StringAddStub>::BuildCodeInitializedStub() {
  StringAddStub* stub = casted_stub();
  StringAddFlags flags = stub->flags();
  PretenureFlag pretenure_flag = stub->pretenure_flag();

  HValue* left = GetParameter(Descriptor::kLeft);
  HValue* right = GetParameter(Descriptor::kRight);

  // Make sure that both arguments are strings if not known in advance.
  if ((flags & STRING_ADD_CHECK_LEFT) == STRING_ADD_CHECK_LEFT) {
    left =
        BuildToString(left, (flags & STRING_ADD_CONVERT) == STRING_ADD_CONVERT);
  }
  if ((flags & STRING_ADD_CHECK_RIGHT) == STRING_ADD_CHECK_RIGHT) {
    right = BuildToString(right,
                          (flags & STRING_ADD_CONVERT) == STRING_ADD_CONVERT);
  }

  return BuildStringAdd(left, right, HAllocationMode(pretenure_flag));
}


Handle<Code> StringAddStub::GenerateCode() {
  return DoGenerateCode(this);
}

template <>
HValue* CodeStubGraphBuilder<ToBooleanICStub>::BuildCodeInitializedStub() {
  ToBooleanICStub* stub = casted_stub();
  IfBuilder if_true(this);
  if_true.If<HBranch>(GetParameter(Descriptor::kArgument), stub->types());
  if_true.Then();
  if_true.Return(graph()->GetConstantTrue());
  if_true.Else();
  if_true.End();
  return graph()->GetConstantFalse();
}

Handle<Code> ToBooleanICStub::GenerateCode() { return DoGenerateCode(this); }

template <>
HValue* CodeStubGraphBuilder<LoadDictionaryElementStub>::BuildCodeStub() {
  HValue* receiver = GetParameter(Descriptor::kReceiver);
  HValue* key = GetParameter(Descriptor::kName);

  Add<HCheckSmi>(key);

  HValue* elements = AddLoadElements(receiver);

  HValue* hash = BuildElementIndexHash(key);

  return BuildUncheckedDictionaryElementLoad(receiver, elements, key, hash);
}


Handle<Code> LoadDictionaryElementStub::GenerateCode() {
  return DoGenerateCode(this);
}


template<>
HValue* CodeStubGraphBuilder<RegExpConstructResultStub>::BuildCodeStub() {
  // Determine the parameters.
  HValue* length = GetParameter(Descriptor::kLength);
  HValue* index = GetParameter(Descriptor::kIndex);
  HValue* input = GetParameter(Descriptor::kInput);

  // TODO(turbofan): This codestub has regressed to need a frame on ia32 at some
  // point and wasn't caught since it wasn't built in the snapshot. We should
  // probably just replace with a TurboFan stub rather than fixing it.
#if !(V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X87)
  info()->MarkMustNotHaveEagerFrame();
#endif

  return BuildRegExpConstructResult(length, index, input);
}


Handle<Code> RegExpConstructResultStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
class CodeStubGraphBuilder<KeyedLoadGenericStub>
    : public CodeStubGraphBuilderBase {
 public:
  explicit CodeStubGraphBuilder(CompilationInfo* info, CodeStub* stub)
      : CodeStubGraphBuilderBase(info, stub) {}

  typedef KeyedLoadGenericStub::Descriptor Descriptor;

 protected:
  virtual HValue* BuildCodeStub();

  void BuildElementsKindLimitCheck(HGraphBuilder::IfBuilder* if_builder,
                                   HValue* bit_field2,
                                   ElementsKind kind);

  void BuildFastElementLoad(HGraphBuilder::IfBuilder* if_builder,
                            HValue* receiver,
                            HValue* key,
                            HValue* instance_type,
                            HValue* bit_field2,
                            ElementsKind kind);

  KeyedLoadGenericStub* casted_stub() {
    return static_cast<KeyedLoadGenericStub*>(stub());
  }
};


void CodeStubGraphBuilder<KeyedLoadGenericStub>::BuildElementsKindLimitCheck(
    HGraphBuilder::IfBuilder* if_builder, HValue* bit_field2,
    ElementsKind kind) {
  ElementsKind next_kind = static_cast<ElementsKind>(kind + 1);
  HValue* kind_limit = Add<HConstant>(
      static_cast<int>(Map::ElementsKindBits::encode(next_kind)));

  if_builder->If<HCompareNumericAndBranch>(bit_field2, kind_limit, Token::LT);
  if_builder->Then();
}


void CodeStubGraphBuilder<KeyedLoadGenericStub>::BuildFastElementLoad(
    HGraphBuilder::IfBuilder* if_builder, HValue* receiver, HValue* key,
    HValue* instance_type, HValue* bit_field2, ElementsKind kind) {
  BuildElementsKindLimitCheck(if_builder, bit_field2, kind);

  IfBuilder js_array_check(this);
  js_array_check.If<HCompareNumericAndBranch>(
      instance_type, Add<HConstant>(JS_ARRAY_TYPE), Token::EQ);
  js_array_check.Then();
  Push(BuildUncheckedMonomorphicElementAccess(receiver, key, NULL,
                                              true, kind,
                                              LOAD, NEVER_RETURN_HOLE,
                                              STANDARD_STORE));
  js_array_check.Else();
  Push(BuildUncheckedMonomorphicElementAccess(receiver, key, NULL,
                                              false, kind,
                                              LOAD, NEVER_RETURN_HOLE,
                                              STANDARD_STORE));
  js_array_check.End();
}


HValue* CodeStubGraphBuilder<KeyedLoadGenericStub>::BuildCodeStub() {
  HValue* receiver = GetParameter(Descriptor::kReceiver);
  HValue* key = GetParameter(Descriptor::kName);
  // Split into a smi/integer case and unique string case.
  HIfContinuation index_name_split_continuation(graph()->CreateBasicBlock(),
                                                graph()->CreateBasicBlock());

  BuildKeyedIndexCheck(key, &index_name_split_continuation);

  IfBuilder index_name_split(this, &index_name_split_continuation);
  index_name_split.Then();
  {
    // Key is an index (number)
    key = Pop();

    int bit_field_mask = (1 << Map::kIsAccessCheckNeeded) |
      (1 << Map::kHasIndexedInterceptor);
    BuildJSObjectCheck(receiver, bit_field_mask);

    HValue* map =
        Add<HLoadNamedField>(receiver, nullptr, HObjectAccess::ForMap());

    HValue* instance_type =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapInstanceType());

    HValue* bit_field2 =
        Add<HLoadNamedField>(map, nullptr, HObjectAccess::ForMapBitField2());

    IfBuilder kind_if(this);
    BuildFastElementLoad(&kind_if, receiver, key, instance_type, bit_field2,
                         FAST_HOLEY_ELEMENTS);

    kind_if.Else();
    {
      BuildFastElementLoad(&kind_if, receiver, key, instance_type, bit_field2,
                           FAST_HOLEY_DOUBLE_ELEMENTS);
    }
    kind_if.Else();

    // The DICTIONARY_ELEMENTS check generates a "kind_if.Then"
    BuildElementsKindLimitCheck(&kind_if, bit_field2, DICTIONARY_ELEMENTS);
    {
      HValue* elements = AddLoadElements(receiver);

      HValue* hash = BuildElementIndexHash(key);

      Push(BuildUncheckedDictionaryElementLoad(receiver, elements, key, hash));
    }
    kind_if.Else();

    // The SLOW_SLOPPY_ARGUMENTS_ELEMENTS check generates a "kind_if.Then"
    STATIC_ASSERT(FAST_SLOPPY_ARGUMENTS_ELEMENTS <
                  SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    BuildElementsKindLimitCheck(&kind_if, bit_field2,
                                SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    // Non-strict elements are not handled.
    Add<HDeoptimize>(DeoptimizeReason::kNonStrictElementsInKeyedLoadGenericStub,
                     Deoptimizer::EAGER);
    Push(graph()->GetConstant0());

    kind_if.ElseDeopt(
        DeoptimizeReason::kElementsKindUnhandledInKeyedLoadGenericStub);

    kind_if.End();
  }
  index_name_split.Else();
  {
    // Key is a unique string.
    key = Pop();

    int bit_field_mask = (1 << Map::kIsAccessCheckNeeded) |
        (1 << Map::kHasNamedInterceptor);
    BuildJSObjectCheck(receiver, bit_field_mask);

    HIfContinuation continuation;
    BuildTestForDictionaryProperties(receiver, &continuation);
    IfBuilder if_dict_properties(this, &continuation);
    if_dict_properties.Then();
    {
      //  Key is string, properties are dictionary mode
      BuildNonGlobalObjectCheck(receiver);

      HValue* properties = Add<HLoadNamedField>(
          receiver, nullptr, HObjectAccess::ForPropertiesPointer());

      HValue* hash =
          Add<HLoadNamedField>(key, nullptr, HObjectAccess::ForNameHashField());

      hash = AddUncasted<HShr>(hash, Add<HConstant>(Name::kHashShift));

      HValue* value =
          BuildUncheckedDictionaryElementLoad(receiver, properties, key, hash);
      Push(value);
    }
    if_dict_properties.Else();
    {
      // TODO(dcarney): don't use keyed lookup cache, but convert to use
      // megamorphic stub cache.
      UNREACHABLE();
      //  Key is string, properties are fast mode
      HValue* hash = BuildKeyedLookupCacheHash(receiver, key);

      ExternalReference cache_keys_ref =
          ExternalReference::keyed_lookup_cache_keys(isolate());
      HValue* cache_keys = Add<HConstant>(cache_keys_ref);

      HValue* map =
          Add<HLoadNamedField>(receiver, nullptr, HObjectAccess::ForMap());
      HValue* base_index = AddUncasted<HMul>(hash, Add<HConstant>(2));
      base_index->ClearFlag(HValue::kCanOverflow);

      HIfContinuation inline_or_runtime_continuation(
          graph()->CreateBasicBlock(), graph()->CreateBasicBlock());
      {
        IfBuilder lookup_ifs[KeyedLookupCache::kEntriesPerBucket];
        for (int probe = 0; probe < KeyedLookupCache::kEntriesPerBucket;
             ++probe) {
          IfBuilder* lookup_if = &lookup_ifs[probe];
          lookup_if->Initialize(this);
          int probe_base = probe * KeyedLookupCache::kEntryLength;
          HValue* map_index = AddUncasted<HAdd>(
              base_index,
              Add<HConstant>(probe_base + KeyedLookupCache::kMapIndex));
          map_index->ClearFlag(HValue::kCanOverflow);
          HValue* key_index = AddUncasted<HAdd>(
              base_index,
              Add<HConstant>(probe_base + KeyedLookupCache::kKeyIndex));
          key_index->ClearFlag(HValue::kCanOverflow);
          HValue* map_to_check =
              Add<HLoadKeyed>(cache_keys, map_index, nullptr, nullptr,
                              FAST_ELEMENTS, NEVER_RETURN_HOLE, 0);
          lookup_if->If<HCompareObjectEqAndBranch>(map_to_check, map);
          lookup_if->And();
          HValue* key_to_check =
              Add<HLoadKeyed>(cache_keys, key_index, nullptr, nullptr,
                              FAST_ELEMENTS, NEVER_RETURN_HOLE, 0);
          lookup_if->If<HCompareObjectEqAndBranch>(key_to_check, key);
          lookup_if->Then();
          {
            ExternalReference cache_field_offsets_ref =
                ExternalReference::keyed_lookup_cache_field_offsets(isolate());
            HValue* cache_field_offsets =
                Add<HConstant>(cache_field_offsets_ref);
            HValue* index = AddUncasted<HAdd>(hash, Add<HConstant>(probe));
            index->ClearFlag(HValue::kCanOverflow);
            HValue* property_index =
                Add<HLoadKeyed>(cache_field_offsets, index, nullptr, cache_keys,
                                INT32_ELEMENTS, NEVER_RETURN_HOLE, 0);
            Push(property_index);
          }
          lookup_if->Else();
        }
        for (int i = 0; i < KeyedLookupCache::kEntriesPerBucket; ++i) {
          lookup_ifs[i].JoinContinuation(&inline_or_runtime_continuation);
        }
      }

      IfBuilder inline_or_runtime(this, &inline_or_runtime_continuation);
      inline_or_runtime.Then();
      {
        // Found a cached index, load property inline.
        Push(Add<HLoadFieldByIndex>(receiver, Pop()));
      }
      inline_or_runtime.Else();
      {
        // KeyedLookupCache miss; call runtime.
        Add<HPushArguments>(receiver, key);
        Push(Add<HCallRuntime>(
            Runtime::FunctionForId(Runtime::kKeyedGetProperty), 2));
      }
      inline_or_runtime.End();
    }
    if_dict_properties.End();
  }
  index_name_split.End();

  return Pop();
}


Handle<Code> KeyedLoadGenericStub::GenerateCode() {
  return DoGenerateCode(this);
}

}  // namespace internal
}  // namespace v8
