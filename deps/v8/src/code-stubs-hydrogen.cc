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
        HParameter::REGISTER_PARAMETER);
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

  HValue* return_value = BuildCodeStub();
  HReturn* hreturn_instruction = new(zone) HReturn(return_value,
                                                   context_,
                                                   stack_parameter_count);
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


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowObjectStub>::BuildCodeStub() {
  Zone* zone = this->zone();
  Factory* factory = isolate()->factory();

  HInstruction* boilerplate =
      AddInstruction(new(zone) HLoadKeyed(GetParameter(0),
                                          GetParameter(1),
                                          NULL,
                                          FAST_ELEMENTS));

  CheckBuilder builder(this, BailoutId::StubEntry());
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
    AddSimulate(BailoutId::StubEntry());
  }

  builder.End();
  return object;
}


Handle<Code> FastCloneShallowObjectStub::GenerateCode() {
  CodeStubGraphBuilder<FastCloneShallowObjectStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


template <>
HValue* CodeStubGraphBuilder<KeyedLoadFastElementStub>::BuildCodeStub() {
  HInstruction* load = BuildUncheckedMonomorphicElementAccess(
      GetParameter(0), GetParameter(1), NULL, NULL,
      casted_stub()->is_js_array(), casted_stub()->elements_kind(),
      false, Representation::Tagged());
  AddInstruction(load);
  return load;
}


Handle<Code> KeyedLoadFastElementStub::GenerateCode() {
  CodeStubGraphBuilder<KeyedLoadFastElementStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


template <>
HValue* CodeStubGraphBuilder<TransitionElementsKindStub>::BuildCodeStub() {
  Zone* zone = this->zone();

  HValue* js_array = GetParameter(0);
  HValue* map = GetParameter(1);

  info()->MarkAsSavesCallerDoubles();

  AddInstruction(new(zone) HTrapAllocationMemento(js_array));

  HInstruction* array_length =
      AddInstruction(new(zone) HJSArrayLength(js_array,
                                              js_array,
                                              HType::Smi()));

  Heap* heap = isolate()->heap();
  const int kMinFreeNewSpaceAfterGC =
      ((heap->InitialSemiSpaceSize() - sizeof(FixedArrayBase)) / 2) /
      kDoubleSize;

  HConstant* max_alloc_size =
      new(zone) HConstant(kMinFreeNewSpaceAfterGC, Representation::Integer32());
  AddInstruction(max_alloc_size);
  // Since we're forcing Integer32 representation for this HBoundsCheck,
  // there's no need to Smi-check the index.
  AddInstruction(
      new(zone) HBoundsCheck(array_length, max_alloc_size,
                             DONT_ALLOW_SMI_KEY, Representation::Integer32()));

  IfBuilder if_builder(this, BailoutId::StubEntry());

  if_builder.BeginTrue(array_length, graph()->GetConstant0(), Token::EQ);

  // Nothing to do, just change the map.

  if_builder.BeginFalse();

  HInstruction* elements =
      AddInstruction(new(zone) HLoadElements(js_array, js_array));

  HInstruction* elements_length =
      AddInstruction(new(zone) HFixedArrayBaseLength(elements));

  ElementsKind to_kind = casted_stub()->to_kind();
  HValue* new_elements =
      BuildAllocateElements(context(), to_kind, elements_length);

  // Fast elements kinds need to be initialized in case statements below cause a
  // garbage collection.
  Factory* factory = isolate()->factory();

  ASSERT(!IsFastSmiElementsKind(to_kind));
  double nan_double = FixedDoubleArray::hole_nan_as_double();
  HValue* hole = IsFastObjectElementsKind(to_kind)
      ? AddInstruction(new(zone) HConstant(factory->the_hole_value(),
                                           Representation::Tagged()))
      : AddInstruction(new(zone) HConstant(nan_double,
                                           Representation::Double()));

  LoopBuilder builder(this, context(), LoopBuilder::kPostIncrement,
                      BailoutId::StubEntry());

  HValue* zero = graph()->GetConstant0();
  HValue* start = IsFastElementsKind(to_kind) ? zero : array_length;
  HValue* key = builder.BeginBody(start, elements_length, Token::LT);

  AddInstruction(new(zone) HStoreKeyed(new_elements, key, hole, to_kind));
  AddSimulate(BailoutId::StubEntry(), REMOVABLE_SIMULATE);

  builder.EndBody();

  BuildCopyElements(context(), elements,
                    casted_stub()->from_kind(), new_elements,
                    to_kind, array_length);

  AddInstruction(new(zone) HStoreNamedField(js_array,
                                            factory->elements_field_string(),
                                            new_elements, true,
                                            JSArray::kElementsOffset));
  AddSimulate(BailoutId::StubEntry());

  if_builder.End();

  AddInstruction(new(zone) HStoreNamedField(js_array, factory->length_string(),
                                            map, true, JSArray::kMapOffset));
  AddSimulate(BailoutId::StubEntry());
  return js_array;
}


template <>
HValue* CodeStubGraphBuilder<ArrayNoArgumentConstructorStub>::BuildCodeStub() {
  HInstruction* deopt = new(zone()) HSoftDeoptimize();
  AddInstruction(deopt);
  current_block()->MarkAsDeoptimizing();
  return GetParameter(0);
}


Handle<Code> ArrayNoArgumentConstructorStub::GenerateCode() {
  CodeStubGraphBuilder<ArrayNoArgumentConstructorStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


template <>
HValue* CodeStubGraphBuilder<ArraySingleArgumentConstructorStub>::
    BuildCodeStub() {
  HInstruction* deopt = new(zone()) HSoftDeoptimize();
  AddInstruction(deopt);
  current_block()->MarkAsDeoptimizing();
  return GetParameter(0);
}


Handle<Code> TransitionElementsKindStub::GenerateCode() {
  CodeStubGraphBuilder<TransitionElementsKindStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


Handle<Code> ArraySingleArgumentConstructorStub::GenerateCode() {
  CodeStubGraphBuilder<ArraySingleArgumentConstructorStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}


template <>
HValue* CodeStubGraphBuilder<ArrayNArgumentsConstructorStub>::BuildCodeStub() {
  HInstruction* deopt = new(zone()) HSoftDeoptimize();
  AddInstruction(deopt);
  current_block()->MarkAsDeoptimizing();
  return GetParameter(0);
}


Handle<Code> ArrayNArgumentsConstructorStub::GenerateCode() {
  CodeStubGraphBuilder<ArrayNArgumentsConstructorStub> builder(this);
  LChunk* chunk = OptimizeGraph(builder.CreateGraph());
  return chunk->Codegen(Code::COMPILED_STUB);
}

} }  // namespace v8::internal
