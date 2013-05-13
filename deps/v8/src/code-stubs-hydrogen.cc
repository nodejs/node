// Copyright 2012 the V8 project authors. All rights reserved.
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

#include "code-stubs.h"
#include "hydrogen.h"
#include "lithium.h"

namespace v8 {
namespace internal {


static LChunk* OptimizeGraph(HGraph* graph) {
  Isolate* isolate =  graph->isolate();
  AssertNoAllocation no_gc;
  NoHandleAllocation no_handles(isolate);
  HandleDereferenceGuard no_deref(isolate, HandleDereferenceGuard::DISALLOW);

  ASSERT(graph != NULL);
  SmartArrayPointer<char> bailout_reason;
  if (!graph->Optimize(&bailout_reason)) {
    FATAL(bailout_reason.is_empty() ? "unknown" : *bailout_reason);
  }
  LChunk* chunk = LChunk::NewChunk(graph);
  if (chunk == NULL) {
    FATAL(graph->info()->bailout_reason());
  }
  return chunk;
}


class CodeStubGraphBuilderBase : public HGraphBuilder {
 public:
  CodeStubGraphBuilderBase(Isolate* isolate, HydrogenCodeStub* stub)
      : HGraphBuilder(&info_),
        arguments_length_(NULL),
        info_(stub, isolate),
        context_(NULL) {
    descriptor_ = stub->GetInterfaceDescriptor(isolate);
    parameters_.Reset(new HParameter*[descriptor_->register_param_count_]);
  }
  virtual bool BuildGraph();

 protected:
  virtual HValue* BuildCodeStub() = 0;
  HParameter* GetParameter(int parameter) {
    ASSERT(parameter < descriptor_->register_param_count_);
    return parameters_[parameter];
  }
  HValue* GetArgumentsLength() {
    // This is initialized in BuildGraph()
    ASSERT(arguments_length_ != NULL);
    return arguments_length_;
  }
  CompilationInfo* info() { return &info_; }
  HydrogenCodeStub* stub() { return info_.code_stub(); }
  HContext* context() { return context_; }
  Isolate* isolate() { return info_.isolate(); }

  class ArrayContextChecker {
   public:
    ArrayContextChecker(HGraphBuilder* builder, HValue* constructor,
                        HValue* array_function)
        : checker_(builder) {
      checker_.If<HCompareObjectEqAndBranch, HValue*>(constructor,
                                                      array_function);
      checker_.Then();
    }

    ~ArrayContextChecker() {
      checker_.ElseDeopt();
      checker_.End();
    }
   private:
    IfBuilder checker_;
  };

 private:
  SmartArrayPointer<HParameter*> parameters_;
  HValue* arguments_length_;
  CompilationInfoWithZone info_;
  CodeStubInterfaceDescriptor* descriptor_;
  HContext* context_;
};


bool CodeStubGraphBuilderBase::BuildGraph() {
  // Update the static counter each time a new code stub is generated.
  isolate()->counters()->code_stubs()->Increment();

  if (FLAG_trace_hydrogen) {
    const char* name = CodeStub::MajorName(stub()->MajorKey(), false);
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling stub %s using hydrogen\n", name);
    isolate()->GetHTracer()->TraceCompilation(&info_);
  }

  Zone* zone = this->zone();
  int param_count = descriptor_->register_param_count_;
  HEnvironment* start_environment = graph()->start_environment();
  HBasicBlock* next_block = CreateBasicBlock(start_environment);
  current_block()->Goto(next_block);
  next_block->SetJoinId(BailoutId::StubEntry());
  set_current_block(next_block);

  HConstant* undefined_constant = new(zone) HConstant(
      isolate()->factory()->undefined_value(), Representation::Tagged());
  AddInstruction(undefined_constant);
  graph()->set_undefined_constant(undefined_constant);

  for (int i = 0; i < param_count; ++i) {
    HParameter* param =
        new(zone) HParameter(i, HParameter::REGISTER_PARAMETER);
    AddInstruction(param);
    start_environment->Bind(i, param);
    parameters_[i] = param;
  }

  HInstruction* stack_parameter_count;
  if (descriptor_->stack_parameter_count_ != NULL) {
    ASSERT(descriptor_->environment_length() == (param_count + 1));
    stack_parameter_count = new(zone) HParameter(param_count,
                                                 HParameter::REGISTER_PARAMETER,
                                                 Representation::Integer32());
    stack_parameter_count->set_type(HType::Smi());
    // it's essential to bind this value to the environment in case of deopt
    AddInstruction(stack_parameter_count);
    start_environment->Bind(param_count, stack_parameter_count);
    arguments_length_ = stack_parameter_count;
  } else {
    ASSERT(descriptor_->environment_length() == param_count);
    stack_parameter_count = graph()->GetConstantMinus1();
    arguments_length_ = graph()->GetConstant0();
  }

  context_ = new(zone) HContext();
  AddInstruction(context_);
  start_environment->BindContext(context_);

  AddSimulate(BailoutId::StubEntry());

  NoObservableSideEffectsScope no_effects(this);

  HValue* return_value = BuildCodeStub();

  // We might have extra expressions to pop from the stack in addition to the
  // arguments above
  HInstruction* stack_pop_count = stack_parameter_count;
  if (descriptor_->function_mode_ == JS_FUNCTION_STUB_MODE) {
    if (!stack_parameter_count->IsConstant() &&
        descriptor_->hint_stack_parameter_count_ < 0) {
      HInstruction* amount = graph()->GetConstant1();
      stack_pop_count = AddInstruction(
          HAdd::New(zone, context_, stack_parameter_count, amount));
      stack_pop_count->ChangeRepresentation(Representation::Integer32());
      stack_pop_count->ClearFlag(HValue::kCanOverflow);
    } else {
      int count = descriptor_->hint_stack_parameter_count_;
      stack_pop_count = AddInstruction(new(zone)
          HConstant(count, Representation::Integer32()));
    }
  }

  if (!current_block()->IsFinished()) {
    HReturn* hreturn_instruction = new(zone) HReturn(return_value,
                                                     context_,
                                                     stack_pop_count);
    current_block()->Finish(hreturn_instruction);
  }
  return true;
}


template <class Stub>
class CodeStubGraphBuilder: public CodeStubGraphBuilderBase {
 public:
  explicit CodeStubGraphBuilder(Stub* stub)
      : CodeStubGraphBuilderBase(Isolate::Current(), stub) {}

 protected:
  virtual HValue* BuildCodeStub() {
    if (casted_stub()->IsMiss()) {
      return BuildCodeInitializedStub();
    } else {
      return BuildCodeUninitializedStub();
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
    builder.ElseDeopt();
    return undefined;
  }

  Stub* casted_stub() { return static_cast<Stub*>(stub()); }
};


Handle<Code> HydrogenCodeStub::GenerateLightweightMissCode(Isolate* isolate) {
  Factory* factory = isolate->factory();

  // Generate the new code.
  MacroAssembler masm(isolate, NULL, 256);

  {
    // Update the static counter each time a new code stub is generated.
    isolate->counters()->code_stubs()->Increment();

    // Nested stubs are not allowed for leaves.
    AllowStubCallsScope allow_scope(&masm, false);

    // Generate the code for the stub.
    masm.set_generating_stub(true);
    NoCurrentFrameScope scope(&masm);
    GenerateLightweightMiss(&masm);
  }

  // Create the code object.
  CodeDesc desc;
  masm.GetCode(&desc);

  // Copy the generated code into a heap object.
  Code::Flags flags = Code::ComputeFlags(
      GetCodeKind(),
      GetICState(),
      GetExtraICState(),
      GetStubType(),
      GetStubFlags());
  Handle<Code> new_object = factory->NewCode(
      desc, flags, masm.CodeObject(), NeedsImmovableCode());
  return new_object;
}


template <class Stub>
static Handle<Code> DoGenerateCode(Stub* stub) {
  Isolate* isolate = Isolate::Current();
  CodeStub::Major  major_key =
      static_cast<HydrogenCodeStub*>(stub)->MajorKey();
  CodeStubInterfaceDescriptor* descriptor =
      isolate->code_stub_interface_descriptor(major_key);
  if (descriptor->register_param_count_ < 0) {
    stub->InitializeInterfaceDescriptor(isolate, descriptor);
  }
  // The miss case without stack parameters can use a light-weight stub to enter
  // the runtime that is significantly faster than using the standard
  // stub-failure deopt mechanism.
  if (stub->IsMiss() && descriptor->stack_parameter_count_ == NULL) {
    return stub->GenerateLightweightMissCode(isolate);
  } else {
    CodeStubGraphBuilder<Stub> builder(stub);
    LChunk* chunk = OptimizeGraph(builder.CreateGraph());
    return chunk->Codegen();
  }
}


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowArrayStub>::BuildCodeStub() {
  Zone* zone = this->zone();
  Factory* factory = isolate()->factory();
  HValue* undefined = graph()->GetConstantUndefined();
  AllocationSiteMode alloc_site_mode = casted_stub()->allocation_site_mode();
  FastCloneShallowArrayStub::Mode mode = casted_stub()->mode();
  int length = casted_stub()->length();

  HInstruction* boilerplate =
      AddInstruction(new(zone) HLoadKeyed(GetParameter(0),
                                          GetParameter(1),
                                          NULL,
                                          FAST_ELEMENTS));

  IfBuilder checker(this);
  checker.IfNot<HCompareObjectEqAndBranch, HValue*>(boilerplate, undefined);
  checker.Then();

  if (mode == FastCloneShallowArrayStub::CLONE_ANY_ELEMENTS) {
    HValue* elements = AddLoadElements(boilerplate);

    IfBuilder if_fixed_cow(this);
    if_fixed_cow.IfCompareMap(elements, factory->fixed_cow_array_map());
    if_fixed_cow.Then();
    environment()->Push(BuildCloneShallowArray(context(),
                                               boilerplate,
                                               alloc_site_mode,
                                               FAST_ELEMENTS,
                                               0/*copy-on-write*/));
    if_fixed_cow.Else();

    IfBuilder if_fixed(this);
    if_fixed.IfCompareMap(elements, factory->fixed_array_map());
    if_fixed.Then();
    environment()->Push(BuildCloneShallowArray(context(),
                                               boilerplate,
                                               alloc_site_mode,
                                               FAST_ELEMENTS,
                                               length));
    if_fixed.Else();
    environment()->Push(BuildCloneShallowArray(context(),
                                               boilerplate,
                                               alloc_site_mode,
                                               FAST_DOUBLE_ELEMENTS,
                                               length));
  } else {
    ElementsKind elements_kind = casted_stub()->ComputeElementsKind();
    environment()->Push(BuildCloneShallowArray(context(),
                                               boilerplate,
                                               alloc_site_mode,
                                               elements_kind,
                                               length));
  }

  HValue* result = environment()->Pop();
  checker.ElseDeopt();
  return result;
}


Handle<Code> FastCloneShallowArrayStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowObjectStub>::BuildCodeStub() {
  Zone* zone = this->zone();
  Factory* factory = isolate()->factory();
  HValue* undefined = graph()->GetConstantUndefined();

  HInstruction* boilerplate =
      AddInstruction(new(zone) HLoadKeyed(GetParameter(0),
                                          GetParameter(1),
                                          NULL,
                                          FAST_ELEMENTS));

  IfBuilder checker(this);
  checker.IfNot<HCompareObjectEqAndBranch, HValue*>(boilerplate, undefined);
  checker.And();

  int size = JSObject::kHeaderSize + casted_stub()->length() * kPointerSize;
  HValue* boilerplate_size =
      AddInstruction(new(zone) HInstanceSize(boilerplate));
  HValue* size_in_words =
      AddInstruction(new(zone) HConstant(size >> kPointerSizeLog2,
                                         Representation::Integer32()));
  checker.IfCompare(boilerplate_size, size_in_words, Token::EQ);
  checker.Then();

  HValue* size_in_bytes =
      AddInstruction(new(zone) HConstant(size, Representation::Integer32()));
  HAllocate::Flags flags = HAllocate::CAN_ALLOCATE_IN_NEW_SPACE;
  if (FLAG_pretenure_literals) {
    flags = static_cast<HAllocate::Flags>(
       flags | HAllocate::CAN_ALLOCATE_IN_OLD_POINTER_SPACE);
  }
  HInstruction* object =
      AddInstruction(new(zone) HAllocate(context(),
                                         size_in_bytes,
                                         HType::JSObject(),
                                         flags));

  for (int i = 0; i < size; i += kPointerSize) {
    HInstruction* value =
        AddInstruction(new(zone) HLoadNamedField(
            boilerplate, true, Representation::Tagged(), i));
    AddInstruction(new(zone) HStoreNamedField(object,
                                              factory->empty_string(),
                                              value, true,
                                              Representation::Tagged(), i));
  }

  checker.ElseDeopt();
  return object;
}


Handle<Code> FastCloneShallowObjectStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<KeyedLoadFastElementStub>::BuildCodeStub() {
  HInstruction* load = BuildUncheckedMonomorphicElementAccess(
      GetParameter(0), GetParameter(1), NULL, NULL,
      casted_stub()->is_js_array(), casted_stub()->elements_kind(),
      false, STANDARD_STORE, Representation::Tagged());
  return load;
}


Handle<Code> KeyedLoadFastElementStub::GenerateCode() {
  return DoGenerateCode(this);
}


template<>
HValue* CodeStubGraphBuilder<LoadFieldStub>::BuildCodeStub() {
  Representation representation = casted_stub()->representation();
  HInstruction* load = AddInstruction(DoBuildLoadNamedField(
      GetParameter(0), casted_stub()->is_inobject(),
      representation, casted_stub()->offset()));
  return load;
}


Handle<Code> LoadFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template<>
HValue* CodeStubGraphBuilder<KeyedLoadFieldStub>::BuildCodeStub() {
  Representation representation = casted_stub()->representation();
  HInstruction* load = AddInstruction(DoBuildLoadNamedField(
      GetParameter(0), casted_stub()->is_inobject(),
      representation, casted_stub()->offset()));
  return load;
}


Handle<Code> KeyedLoadFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<KeyedStoreFastElementStub>::BuildCodeStub() {
  BuildUncheckedMonomorphicElementAccess(
      GetParameter(0), GetParameter(1), GetParameter(2), NULL,
      casted_stub()->is_js_array(), casted_stub()->elements_kind(),
      true, casted_stub()->store_mode(), Representation::Tagged());

  return GetParameter(2);
}


Handle<Code> KeyedStoreFastElementStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<TransitionElementsKindStub>::BuildCodeStub() {
  Zone* zone = this->zone();

  HValue* js_array = GetParameter(0);
  HValue* map = GetParameter(1);

  info()->MarkAsSavesCallerDoubles();

  AddInstruction(new(zone) HTrapAllocationMemento(js_array));

  HInstruction* array_length =
      AddInstruction(HLoadNamedField::NewArrayLength(
            zone, js_array, js_array, HType::Smi()));

  ElementsKind to_kind = casted_stub()->to_kind();
  BuildNewSpaceArrayCheck(array_length, to_kind);

  IfBuilder if_builder(this);

  if_builder.IfCompare(array_length, graph()->GetConstant0(), Token::EQ);
  if_builder.Then();

  // Nothing to do, just change the map.

  if_builder.Else();

  HInstruction* elements = AddLoadElements(js_array);

  HInstruction* elements_length =
      AddInstruction(new(zone) HFixedArrayBaseLength(elements));

  HValue* new_elements =
      BuildAllocateAndInitializeElements(context(), to_kind, elements_length);

  BuildCopyElements(context(), elements,
                    casted_stub()->from_kind(), new_elements,
                    to_kind, array_length, elements_length);

  Factory* factory = isolate()->factory();

  AddInstruction(new(zone) HStoreNamedField(js_array,
                                            factory->elements_field_string(),
                                            new_elements, true,
                                            Representation::Tagged(),
                                            JSArray::kElementsOffset));

  if_builder.End();

  AddInstruction(new(zone) HStoreNamedField(js_array, factory->length_string(),
                                            map, true,
                                            Representation::Tagged(),
                                            JSArray::kMapOffset));
  return js_array;
}


Handle<Code> TransitionElementsKindStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArrayNoArgumentConstructorStub>::BuildCodeStub() {
  // ----------- S t a t e -------------
  //  -- Parameter 1 : type info cell
  //  -- Parameter 0 : constructor
  // -----------------------------------
  HInstruction* array_function = BuildGetArrayFunction(context());
  ArrayContextChecker(this,
                      GetParameter(ArrayConstructorStubBase::kConstructor),
                      array_function);
  // Get the right map
  // Should be a constant
  JSArrayBuilder array_builder(
      this,
      casted_stub()->elements_kind(),
      GetParameter(ArrayConstructorStubBase::kPropertyCell),
      casted_stub()->mode());
  return array_builder.AllocateEmptyArray();
}


Handle<Code> ArrayNoArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArraySingleArgumentConstructorStub>::
    BuildCodeStub() {
  HInstruction* array_function = BuildGetArrayFunction(context());
  ArrayContextChecker(this,
                      GetParameter(ArrayConstructorStubBase::kConstructor),
                      array_function);
  // Smi check and range check on the input arg.
  HValue* constant_one = graph()->GetConstant1();
  HValue* constant_zero = graph()->GetConstant0();

  HInstruction* elements = AddInstruction(
      new(zone()) HArgumentsElements(false));
  HInstruction* argument = AddInstruction(
      new(zone()) HAccessArgumentsAt(elements, constant_one, constant_zero));

  HConstant* max_alloc_length =
      new(zone()) HConstant(JSObject::kInitialMaxFastElementArray,
                            Representation::Tagged());
  AddInstruction(max_alloc_length);
  const int initial_capacity = JSArray::kPreallocatedArrayElements;
  HConstant* initial_capacity_node =
      new(zone()) HConstant(initial_capacity, Representation::Tagged());
  AddInstruction(initial_capacity_node);

  // Since we're forcing Integer32 representation for this HBoundsCheck,
  // there's no need to Smi-check the index.
  HBoundsCheck* checked_arg = AddBoundsCheck(argument, max_alloc_length,
                                             ALLOW_SMI_KEY,
                                             Representation::Tagged());
  IfBuilder if_builder(this);
  if_builder.IfCompare(checked_arg, constant_zero, Token::EQ);
  if_builder.Then();
  Push(initial_capacity_node);  // capacity
  Push(constant_zero);  // length
  if_builder.Else();
  Push(checked_arg);  // capacity
  Push(checked_arg);  // length
  if_builder.End();

  // Figure out total size
  HValue* length = Pop();
  HValue* capacity = Pop();

  JSArrayBuilder array_builder(
      this,
      casted_stub()->elements_kind(),
      GetParameter(ArrayConstructorStubBase::kPropertyCell),
      casted_stub()->mode());
  return array_builder.AllocateArray(capacity, length, true);
}


Handle<Code> ArraySingleArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArrayNArgumentsConstructorStub>::BuildCodeStub() {
  HInstruction* array_function = BuildGetArrayFunction(context());
  ArrayContextChecker(this,
                      GetParameter(ArrayConstructorStubBase::kConstructor),
                      array_function);
  ElementsKind kind = casted_stub()->elements_kind();
  HValue* length = GetArgumentsLength();

  JSArrayBuilder array_builder(
      this,
      kind,
      GetParameter(ArrayConstructorStubBase::kPropertyCell),
      casted_stub()->mode());

  // We need to fill with the hole if it's a smi array in the multi-argument
  // case because we might have to bail out while copying arguments into
  // the array because they aren't compatible with a smi array.
  // If it's a double array, no problem, and if it's fast then no
  // problem either because doubles are boxed.
  bool fill_with_hole = IsFastSmiElementsKind(kind);
  HValue* new_object = array_builder.AllocateArray(length,
                                                   length,
                                                   fill_with_hole);
  HValue* elements = array_builder.GetElementsLocation();
  ASSERT(elements != NULL);

  // Now populate the elements correctly.
  LoopBuilder builder(this,
                      context(),
                      LoopBuilder::kPostIncrement);
  HValue* start = graph()->GetConstant0();
  HValue* key = builder.BeginBody(start, length, Token::LT);
  HInstruction* argument_elements = AddInstruction(
      new(zone()) HArgumentsElements(false));
  HInstruction* argument = AddInstruction(new(zone()) HAccessArgumentsAt(
      argument_elements, length, key));

  // Checks to prevent incompatible stores
  if (IsFastSmiElementsKind(kind)) {
    AddInstruction(new(zone()) HCheckSmi(argument));
  }

  AddInstruction(new(zone()) HStoreKeyed(elements, key, argument, kind));
  builder.EndBody();
  return new_object;
}


Handle<Code> ArrayNArgumentsConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<CompareNilICStub>::BuildCodeUninitializedStub() {
  CompareNilICStub* stub = casted_stub();
  HIfContinuation continuation;
  Handle<Map> sentinel_map(graph()->isolate()->heap()->meta_map());
  BuildCompareNil(GetParameter(0), stub->GetKind(),
                  stub->GetTypes(), sentinel_map,
                  RelocInfo::kNoPosition, &continuation);
  IfBuilder if_nil(this, &continuation);
  if_nil.Then();
  if (continuation.IsFalseReachable()) {
    if_nil.Else();
    if_nil.Return(graph()->GetConstantSmi0());
  }
  if_nil.End();
  return continuation.IsTrueReachable()
      ? graph()->GetConstantSmi1()
      : graph()->GetConstantUndefined();
}


Handle<Code> CompareNilICStub::GenerateCode() {
  return DoGenerateCode(this);
}

} }  // namespace v8::internal
