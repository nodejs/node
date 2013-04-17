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
    int major_key = stub->MajorKey();
    descriptor_ = isolate->code_stub_interface_descriptor(major_key);
    if (descriptor_->register_param_count_ < 0) {
      stub->InitializeInterfaceDescriptor(isolate, descriptor_);
    }
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

 private:
  SmartArrayPointer<HParameter*> parameters_;
  HValue* arguments_length_;
  CompilationInfoWithZone info_;
  CodeStubInterfaceDescriptor* descriptor_;
  HContext* context_;
};


bool CodeStubGraphBuilderBase::BuildGraph() {
  if (FLAG_trace_hydrogen) {
    const char* name = CodeStub::MajorName(stub()->MajorKey(), false);
    PrintF("-----------------------------------------------------------\n");
    PrintF("Compiling stub %s using hydrogen\n", name);
    isolate()->GetHTracer()->TraceCompilation(&info_);
  }

  Zone* zone = this->zone();
  int param_count = descriptor_->register_param_count_;
  HEnvironment* start_environment = graph()->start_environment();
  HBasicBlock* next_block =
      CreateBasicBlock(start_environment, BailoutId::StubEntry());
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
    // it's essential to bind this value to the environment in case of deopt
    start_environment->Bind(param_count, stack_parameter_count);
    AddInstruction(stack_parameter_count);
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
    HInstruction* amount = graph()->GetConstant1();
    stack_pop_count = AddInstruction(
        HAdd::New(zone, context_, stack_parameter_count, amount));
    stack_pop_count->ChangeRepresentation(Representation::Integer32());
    stack_pop_count->ClearFlag(HValue::kCanOverflow);
  }

  HReturn* hreturn_instruction = new(zone) HReturn(return_value,
                                                   context_,
                                                   stack_pop_count);
  current_block()->Finish(hreturn_instruction);
  return true;
}


template <class Stub>
class CodeStubGraphBuilder: public CodeStubGraphBuilderBase {
 public:
  explicit CodeStubGraphBuilder(Stub* stub)
      : CodeStubGraphBuilderBase(Isolate::Current(), stub) {}

 protected:
  virtual HValue* BuildCodeStub();
  Stub* casted_stub() { return static_cast<Stub*>(stub()); }
};


template <class Stub>
static Handle<Code> DoGenerateCode(Stub* stub) {
  CodeStubGraphBuilder<Stub> builder(stub);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowArrayStub>::BuildCodeStub() {
  Zone* zone = this->zone();
  Factory* factory = isolate()->factory();
  AllocationSiteMode alloc_site_mode = casted_stub()->allocation_site_mode();
  FastCloneShallowArrayStub::Mode mode = casted_stub()->mode();
  int length = casted_stub()->length();

  HInstruction* boilerplate =
      AddInstruction(new(zone) HLoadKeyed(GetParameter(0),
                                          GetParameter(1),
                                          NULL,
                                          FAST_ELEMENTS));

  CheckBuilder builder(this);
  builder.CheckNotUndefined(boilerplate);

  if (mode == FastCloneShallowArrayStub::CLONE_ANY_ELEMENTS) {
    HValue* elements =
        AddInstruction(new(zone) HLoadElements(boilerplate, NULL));

    IfBuilder if_fixed_cow(this);
    if_fixed_cow.BeginIfMapEquals(elements, factory->fixed_cow_array_map());
    environment()->Push(BuildCloneShallowArray(context(),
                                               boilerplate,
                                               alloc_site_mode,
                                               FAST_ELEMENTS,
                                               0/*copy-on-write*/));
    if_fixed_cow.BeginElse();

    IfBuilder if_fixed(this);
    if_fixed.BeginIfMapEquals(elements, factory->fixed_array_map());
    environment()->Push(BuildCloneShallowArray(context(),
                                               boilerplate,
                                               alloc_site_mode,
                                               FAST_ELEMENTS,
                                               length));
    if_fixed.BeginElse();

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

  return environment()->Pop();
}


Handle<Code> FastCloneShallowArrayStub::GenerateCode() {
  CodeStubGraphBuilder<FastCloneShallowArrayStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowObjectStub>::BuildCodeStub() {
  Zone* zone = this->zone();
  Factory* factory = isolate()->factory();

  HInstruction* boilerplate =
      AddInstruction(new(zone) HLoadKeyed(GetParameter(0),
                                          GetParameter(1),
                                          NULL,
                                          FAST_ELEMENTS));

  CheckBuilder builder(this);
  builder.CheckNotUndefined(boilerplate);

  int size = JSObject::kHeaderSize + casted_stub()->length() * kPointerSize;
  HValue* boilerplate_size =
      AddInstruction(new(zone) HInstanceSize(boilerplate));
  HValue* size_in_words =
      AddInstruction(new(zone) HConstant(size >> kPointerSizeLog2,
                                         Representation::Integer32()));
  builder.CheckIntegerEq(boilerplate_size, size_in_words);

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
        AddInstruction(new(zone) HLoadNamedField(boilerplate, true, i));
    AddInstruction(new(zone) HStoreNamedField(object,
                                              factory->empty_string(),
                                              value,
                                              true, i));
  }

  builder.End();
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

  if_builder.BeginIf(array_length, graph()->GetConstant0(), Token::EQ);

  // Nothing to do, just change the map.

  if_builder.BeginElse();

  HInstruction* elements =
      AddInstruction(new(zone) HLoadElements(js_array, js_array));

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
                                            JSArray::kElementsOffset));

  if_builder.End();

  AddInstruction(new(zone) HStoreNamedField(js_array, factory->length_string(),
                                            map, true, JSArray::kMapOffset));
  return js_array;
}


Handle<Code> TransitionElementsKindStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArrayNoArgumentConstructorStub>::BuildCodeStub() {
  HInstruction* deopt = new(zone()) HSoftDeoptimize();
  AddInstruction(deopt);
  current_block()->MarkAsDeoptimizing();
  return GetParameter(0);
}


Handle<Code> ArrayNoArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArraySingleArgumentConstructorStub>::
    BuildCodeStub() {
  HInstruction* deopt = new(zone()) HSoftDeoptimize();
  AddInstruction(deopt);
  current_block()->MarkAsDeoptimizing();
  return GetParameter(0);
}


Handle<Code> ArraySingleArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArrayNArgumentsConstructorStub>::BuildCodeStub() {
  HInstruction* deopt = new(zone()) HSoftDeoptimize();
  AddInstruction(deopt);
  current_block()->MarkAsDeoptimizing();
  return GetParameter(0);
}


Handle<Code> ArrayNArgumentsConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}

} }  // namespace v8::internal
