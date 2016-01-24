// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include "src/bailout-reason.h"
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
  explicit CodeStubGraphBuilderBase(CompilationInfo* info)
      : HGraphBuilder(info),
        arguments_length_(NULL),
        info_(info),
        descriptor_(info->code_stub()),
        context_(NULL) {
    int parameter_count = GetParameterCount();
    parameters_.Reset(new HParameter*[parameter_count]);
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
    return RepresentationFromType(descriptor_.GetParameterType(parameter));
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
  CodeStub* stub() { return info_->code_stub(); }
  HContext* context() { return context_; }
  Isolate* isolate() { return info_->isolate(); }

  HLoadNamedField* BuildLoadNamedField(HValue* object, FieldIndex index);
  void BuildStoreNamedField(HValue* object, HValue* value, FieldIndex index,
                            Representation representation,
                            bool transition_to_field);

  enum ArgumentClass {
    NONE,
    SINGLE,
    MULTIPLE
  };

  HValue* UnmappedCase(HValue* elements, HValue* key, HValue* value);
  HValue* EmitKeyedSloppyArguments(HValue* receiver, HValue* key,
                                   HValue* value);

  HValue* BuildArrayConstructor(ElementsKind kind,
                                AllocationSiteOverrideMode override_mode,
                                ArgumentClass argument_class);
  HValue* BuildInternalArrayConstructor(ElementsKind kind,
                                        ArgumentClass argument_class);

  // BuildCheckAndInstallOptimizedCode emits code to install the optimized
  // function found in the optimized code map at map_index in js_function, if
  // the function at map_index matches the given native_context. Builder is
  // left in the "Then()" state after the install.
  void BuildCheckAndInstallOptimizedCode(HValue* js_function,
                                         HValue* native_context,
                                         IfBuilder* builder,
                                         HValue* optimized_map,
                                         HValue* map_index);
  void BuildInstallOptimizedCode(HValue* js_function, HValue* native_context,
                                 HValue* code_object, HValue* literals);
  void BuildInstallCode(HValue* js_function, HValue* shared_info);

  HInstruction* LoadFromOptimizedCodeMap(HValue* optimized_map,
                                         HValue* iterator,
                                         int field_offset);
  void BuildInstallFromOptimizedCodeMap(HValue* js_function,
                                        HValue* shared_info,
                                        HValue* native_context);

  HValue* BuildToString(HValue* input, bool convert);
  HValue* BuildToPrimitive(HValue* input, HValue* input_map);

 private:
  HValue* BuildArraySingleArgumentConstructor(JSArrayBuilder* builder);
  HValue* BuildArrayNArgumentsConstructor(JSArrayBuilder* builder,
                                          ElementsKind kind);

  base::SmartArrayPointer<HParameter*> parameters_;
  HValue* arguments_length_;
  CompilationInfo* info_;
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
      start_environment->Bind(i, param);
    }
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
  explicit CodeStubGraphBuilder(CompilationInfo* info)
      : CodeStubGraphBuilderBase(info) {}

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
    builder.ElseDeopt(Deoptimizer::kForcedDeoptToRuntime);
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
  Code::Flags flags = Code::ComputeFlags(
      GetCodeKind(),
      GetICState(),
      GetExtraICState(),
      GetStubType());
  Handle<Code> new_object = factory->NewCode(
      desc, flags, masm.CodeObject(), NeedsImmovableCode());
  return new_object;
}


template <class Stub>
static Handle<Code> DoGenerateCode(Stub* stub) {
  Isolate* isolate = stub->isolate();
  CodeStubDescriptor descriptor(stub);

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
  Zone zone;
  CompilationInfo info(stub, isolate, &zone);
  CodeStubGraphBuilder<Stub> builder(&info);
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
  HValue* number = GetParameter(NumberToStringStub::kNumber);
  return BuildNumberToString(number, Type::Number(zone()));
}


Handle<Code> NumberToStringStub::GenerateCode() {
  return DoGenerateCode(this);
}


// Returns the type string of a value; see ECMA-262, 11.4.3 (p 47).
template <>
HValue* CodeStubGraphBuilder<TypeofStub>::BuildCodeStub() {
  Factory* factory = isolate()->factory();
  HConstant* number_string = Add<HConstant>(factory->number_string());
  HValue* object = GetParameter(TypeofStub::kObject);

  IfBuilder is_smi(this);
  HValue* smi_check = is_smi.If<HIsSmiAndBranch>(object);
  is_smi.Then();
  { Push(number_string); }
  is_smi.Else();
  {
    IfBuilder is_number(this);
    is_number.If<HCompareMap>(object, isolate()->factory()->heap_number_map());
    is_number.Then();
    { Push(number_string); }
    is_number.Else();
    {
      HValue* map = AddLoadMap(object, smi_check);
      HValue* instance_type = Add<HLoadNamedField>(
          map, nullptr, HObjectAccess::ForMapInstanceType());
      IfBuilder is_string(this);
      is_string.If<HCompareNumericAndBranch>(
          instance_type, Add<HConstant>(FIRST_NONSTRING_TYPE), Token::LT);
      is_string.Then();
      { Push(Add<HConstant>(factory->string_string())); }
      is_string.Else();
      {
        HConstant* object_string = Add<HConstant>(factory->object_string());
        IfBuilder is_oddball(this);
        is_oddball.If<HCompareNumericAndBranch>(
            instance_type, Add<HConstant>(ODDBALL_TYPE), Token::EQ);
        is_oddball.Then();
        {
          Push(Add<HLoadNamedField>(object, nullptr,
                                    HObjectAccess::ForOddballTypeOf()));
        }
        is_oddball.Else();
        {
          IfBuilder is_symbol(this);
          is_symbol.If<HCompareNumericAndBranch>(
              instance_type, Add<HConstant>(SYMBOL_TYPE), Token::EQ);
          is_symbol.Then();
          { Push(Add<HConstant>(factory->symbol_string())); }
          is_symbol.Else();
          {
            HValue* bit_field = Add<HLoadNamedField>(
                map, nullptr, HObjectAccess::ForMapBitField());
            HValue* bit_field_masked = AddUncasted<HBitwise>(
                Token::BIT_AND, bit_field,
                Add<HConstant>((1 << Map::kIsCallable) |
                               (1 << Map::kIsUndetectable)));
            IfBuilder is_function(this);
            is_function.If<HCompareNumericAndBranch>(
                bit_field_masked, Add<HConstant>(1 << Map::kIsCallable),
                Token::EQ);
            is_function.Then();
            { Push(Add<HConstant>(factory->function_string())); }
            is_function.Else();
            {
#define SIMD128_BUILDER_OPEN(TYPE, Type, type, lane_count, lane_type) \
  IfBuilder is_##type(this);                                          \
  is_##type.If<HCompareObjectEqAndBranch>(                            \
      map, Add<HConstant>(factory->type##_map()));                    \
  is_##type.Then();                                                   \
  { Push(Add<HConstant>(factory->type##_string())); }                 \
  is_##type.Else(); {
              SIMD128_TYPES(SIMD128_BUILDER_OPEN)
#undef SIMD128_BUILDER_OPEN
              // Is it an undetectable object?
              IfBuilder is_undetectable(this);
              is_undetectable.If<HCompareNumericAndBranch>(
                  bit_field_masked, graph()->GetConstant0(), Token::NE);
              is_undetectable.Then();
              {
                // typeof an undetectable object is 'undefined'.
                Push(Add<HConstant>(factory->undefined_string()));
              }
              is_undetectable.Else();
              {
                // For any kind of object not handled above, the spec rule for
                // host objects gives that it is okay to return "object".
                Push(object_string);
              }
#define SIMD128_BUILDER_CLOSE(TYPE, Type, type, lane_count, lane_type) }
              SIMD128_TYPES(SIMD128_BUILDER_CLOSE)
#undef SIMD128_BUILDER_CLOSE
            }
            is_function.End();
          }
          is_symbol.End();
        }
        is_oddball.End();
      }
      is_string.End();
    }
    is_number.End();
  }
  is_smi.End();

  return environment()->Pop();
}


Handle<Code> TypeofStub::GenerateCode() { return DoGenerateCode(this); }


template <>
HValue* CodeStubGraphBuilder<FastCloneRegExpStub>::BuildCodeStub() {
  HValue* closure = GetParameter(0);
  HValue* literal_index = GetParameter(1);

  // This stub is very performance sensitive, the generated code must be tuned
  // so that it doesn't build and eager frame.
  info()->MarkMustNotHaveEagerFrame();

  HValue* literals_array = Add<HLoadNamedField>(
      closure, nullptr, HObjectAccess::ForLiteralsPointer());
  HInstruction* boilerplate = Add<HLoadKeyed>(
      literals_array, literal_index, nullptr, nullptr, FAST_ELEMENTS,
      NEVER_RETURN_HOLE, LiteralsArray::kOffsetToFirstLiteral - kHeapObjectTag);

  IfBuilder if_notundefined(this);
  if_notundefined.IfNot<HCompareObjectEqAndBranch>(
      boilerplate, graph()->GetConstantUndefined());
  if_notundefined.Then();
  {
    int result_size =
        JSRegExp::kSize + JSRegExp::kInObjectFieldCount * kPointerSize;
    HValue* result =
        Add<HAllocate>(Add<HConstant>(result_size), HType::JSObject(),
                       NOT_TENURED, JS_REGEXP_TYPE);
    Add<HStoreNamedField>(
        result, HObjectAccess::ForMap(),
        Add<HLoadNamedField>(boilerplate, nullptr, HObjectAccess::ForMap()));
    Add<HStoreNamedField>(
        result, HObjectAccess::ForPropertiesPointer(),
        Add<HLoadNamedField>(boilerplate, nullptr,
                             HObjectAccess::ForPropertiesPointer()));
    Add<HStoreNamedField>(
        result, HObjectAccess::ForElementsPointer(),
        Add<HLoadNamedField>(boilerplate, nullptr,
                             HObjectAccess::ForElementsPointer()));
    for (int offset = JSObject::kHeaderSize; offset < result_size;
         offset += kPointerSize) {
      HObjectAccess access = HObjectAccess::ForObservableJSObjectOffset(offset);
      Add<HStoreNamedField>(result, access,
                            Add<HLoadNamedField>(boilerplate, nullptr, access));
    }
    Push(result);
  }
  if_notundefined.ElseDeopt(Deoptimizer::kUninitializedBoilerplateInFastClone);
  if_notundefined.End();

  return Pop();
}


Handle<Code> FastCloneRegExpStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowArrayStub>::BuildCodeStub() {
  Factory* factory = isolate()->factory();
  HValue* undefined = graph()->GetConstantUndefined();
  AllocationSiteMode alloc_site_mode = casted_stub()->allocation_site_mode();
  HValue* closure = GetParameter(0);
  HValue* literal_index = GetParameter(1);

  // This stub is very performance sensitive, the generated code must be tuned
  // so that it doesn't build and eager frame.
  info()->MarkMustNotHaveEagerFrame();

  HValue* literals_array = Add<HLoadNamedField>(
      closure, nullptr, HObjectAccess::ForLiteralsPointer());

  HInstruction* allocation_site = Add<HLoadKeyed>(
      literals_array, literal_index, nullptr, nullptr, FAST_ELEMENTS,
      NEVER_RETURN_HOLE, LiteralsArray::kOffsetToFirstLiteral - kHeapObjectTag);
  IfBuilder checker(this);
  checker.IfNot<HCompareObjectEqAndBranch, HValue*>(allocation_site,
                                                    undefined);
  checker.Then();

  HObjectAccess access = HObjectAccess::ForAllocationSiteOffset(
      AllocationSite::kTransitionInfoOffset);
  HInstruction* boilerplate =
      Add<HLoadNamedField>(allocation_site, nullptr, access);
  HValue* elements = AddLoadElements(boilerplate);
  HValue* capacity = AddLoadFixedArrayLength(elements);
  IfBuilder zero_capacity(this);
  zero_capacity.If<HCompareNumericAndBranch>(capacity, graph()->GetConstant0(),
                                           Token::EQ);
  zero_capacity.Then();
  Push(BuildCloneShallowArrayEmpty(boilerplate,
                                   allocation_site,
                                   alloc_site_mode));
  zero_capacity.Else();
  IfBuilder if_fixed_cow(this);
  if_fixed_cow.If<HCompareMap>(elements, factory->fixed_cow_array_map());
  if_fixed_cow.Then();
  Push(BuildCloneShallowArrayCow(boilerplate,
                                 allocation_site,
                                 alloc_site_mode,
                                 FAST_ELEMENTS));
  if_fixed_cow.Else();
  IfBuilder if_fixed(this);
  if_fixed.If<HCompareMap>(elements, factory->fixed_array_map());
  if_fixed.Then();
  Push(BuildCloneShallowArrayNonEmpty(boilerplate,
                                      allocation_site,
                                      alloc_site_mode,
                                      FAST_ELEMENTS));

  if_fixed.Else();
  Push(BuildCloneShallowArrayNonEmpty(boilerplate,
                                      allocation_site,
                                      alloc_site_mode,
                                      FAST_DOUBLE_ELEMENTS));
  if_fixed.End();
  if_fixed_cow.End();
  zero_capacity.End();

  checker.ElseDeopt(Deoptimizer::kUninitializedBoilerplateLiterals);
  checker.End();

  return environment()->Pop();
}


Handle<Code> FastCloneShallowArrayStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<FastCloneShallowObjectStub>::BuildCodeStub() {
  HValue* undefined = graph()->GetConstantUndefined();
  HValue* closure = GetParameter(0);
  HValue* literal_index = GetParameter(1);

  HValue* literals_array = Add<HLoadNamedField>(
      closure, nullptr, HObjectAccess::ForLiteralsPointer());

  HInstruction* allocation_site = Add<HLoadKeyed>(
      literals_array, literal_index, nullptr, nullptr, FAST_ELEMENTS,
      NEVER_RETURN_HOLE, LiteralsArray::kOffsetToFirstLiteral - kHeapObjectTag);

  IfBuilder checker(this);
  checker.IfNot<HCompareObjectEqAndBranch, HValue*>(allocation_site,
                                                    undefined);
  checker.And();

  HObjectAccess access = HObjectAccess::ForAllocationSiteOffset(
      AllocationSite::kTransitionInfoOffset);
  HInstruction* boilerplate =
      Add<HLoadNamedField>(allocation_site, nullptr, access);

  int length = casted_stub()->length();
  if (length == 0) {
    // Empty objects have some slack added to them.
    length = JSObject::kInitialGlobalObjectUnusedPropertiesCount;
  }
  int size = JSObject::kHeaderSize + length * kPointerSize;
  int object_size = size;
  if (FLAG_allocation_site_pretenuring) {
    size += AllocationMemento::kSize;
  }

  HValue* boilerplate_map =
      Add<HLoadNamedField>(boilerplate, nullptr, HObjectAccess::ForMap());
  HValue* boilerplate_size = Add<HLoadNamedField>(
      boilerplate_map, nullptr, HObjectAccess::ForMapInstanceSize());
  HValue* size_in_words = Add<HConstant>(object_size >> kPointerSizeLog2);
  checker.If<HCompareNumericAndBranch>(boilerplate_size,
                                       size_in_words, Token::EQ);
  checker.Then();

  HValue* size_in_bytes = Add<HConstant>(size);

  HInstruction* object = Add<HAllocate>(size_in_bytes, HType::JSObject(),
      NOT_TENURED, JS_OBJECT_TYPE);

  for (int i = 0; i < object_size; i += kPointerSize) {
    HObjectAccess access = HObjectAccess::ForObservableJSObjectOffset(i);
    Add<HStoreNamedField>(object, access,
                          Add<HLoadNamedField>(boilerplate, nullptr, access));
  }

  DCHECK(FLAG_allocation_site_pretenuring || (size == object_size));
  if (FLAG_allocation_site_pretenuring) {
    BuildCreateAllocationMemento(
        object, Add<HConstant>(object_size), allocation_site);
  }

  environment()->Push(object);
  checker.ElseDeopt(Deoptimizer::kUninitializedBoilerplateInFastClone);
  checker.End();

  return environment()->Pop();
}


Handle<Code> FastCloneShallowObjectStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<CreateAllocationSiteStub>::BuildCodeStub() {
  // This stub is performance sensitive, the generated code must be tuned
  // so that it doesn't build an eager frame.
  info()->MarkMustNotHaveEagerFrame();

  HValue* size = Add<HConstant>(AllocationSite::kSize);
  HInstruction* object = Add<HAllocate>(size, HType::JSObject(), TENURED,
      JS_OBJECT_TYPE);

  // Store the map
  Handle<Map> allocation_site_map = isolate()->factory()->allocation_site_map();
  AddStoreMapConstant(object, allocation_site_map);

  // Store the payload (smi elements kind)
  HValue* initial_elements_kind = Add<HConstant>(GetInitialFastElementsKind());
  Add<HStoreNamedField>(object,
                        HObjectAccess::ForAllocationSiteOffset(
                            AllocationSite::kTransitionInfoOffset),
                        initial_elements_kind);

  // Unlike literals, constructed arrays don't have nested sites
  Add<HStoreNamedField>(object,
                        HObjectAccess::ForAllocationSiteOffset(
                            AllocationSite::kNestedSiteOffset),
                        graph()->GetConstant0());

  // Pretenuring calculation field.
  Add<HStoreNamedField>(object,
                        HObjectAccess::ForAllocationSiteOffset(
                            AllocationSite::kPretenureDataOffset),
                        graph()->GetConstant0());

  // Pretenuring memento creation count field.
  Add<HStoreNamedField>(object,
                        HObjectAccess::ForAllocationSiteOffset(
                            AllocationSite::kPretenureCreateCountOffset),
                        graph()->GetConstant0());

  // Store an empty fixed array for the code dependency.
  HConstant* empty_fixed_array =
    Add<HConstant>(isolate()->factory()->empty_fixed_array());
  Add<HStoreNamedField>(
      object,
      HObjectAccess::ForAllocationSiteOffset(
          AllocationSite::kDependentCodeOffset),
      empty_fixed_array);

  // Link the object to the allocation site list
  HValue* site_list = Add<HConstant>(
      ExternalReference::allocation_sites_list_address(isolate()));
  HValue* site = Add<HLoadNamedField>(site_list, nullptr,
                                      HObjectAccess::ForAllocationSiteList());
  // TODO(mvstanton): This is a store to a weak pointer, which we may want to
  // mark as such in order to skip the write barrier, once we have a unified
  // system for weakness. For now we decided to keep it like this because having
  // an initial write barrier backed store makes this pointer strong until the
  // next GC, and allocation sites are designed to survive several GCs anyway.
  Add<HStoreNamedField>(
      object,
      HObjectAccess::ForAllocationSiteOffset(AllocationSite::kWeakNextOffset),
      site);
  Add<HStoreNamedField>(site_list, HObjectAccess::ForAllocationSiteList(),
                        object);

  HInstruction* feedback_vector = GetParameter(0);
  HInstruction* slot = GetParameter(1);
  Add<HStoreKeyed>(feedback_vector, slot, object, nullptr, FAST_ELEMENTS,
                   INITIALIZING_STORE);
  return feedback_vector;
}


Handle<Code> CreateAllocationSiteStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<CreateWeakCellStub>::BuildCodeStub() {
  // This stub is performance sensitive, the generated code must be tuned
  // so that it doesn't build an eager frame.
  info()->MarkMustNotHaveEagerFrame();

  HValue* size = Add<HConstant>(WeakCell::kSize);
  HInstruction* object =
      Add<HAllocate>(size, HType::JSObject(), TENURED, JS_OBJECT_TYPE);

  Handle<Map> weak_cell_map = isolate()->factory()->weak_cell_map();
  AddStoreMapConstant(object, weak_cell_map);

  HInstruction* value = GetParameter(CreateWeakCellDescriptor::kValueIndex);
  Add<HStoreNamedField>(object, HObjectAccess::ForWeakCellValue(), value);
  Add<HStoreNamedField>(object, HObjectAccess::ForWeakCellNext(),
                        graph()->GetConstantHole());

  HInstruction* feedback_vector =
      GetParameter(CreateWeakCellDescriptor::kVectorIndex);
  HInstruction* slot = GetParameter(CreateWeakCellDescriptor::kSlotIndex);
  Add<HStoreKeyed>(feedback_vector, slot, object, nullptr, FAST_ELEMENTS,
                   INITIALIZING_STORE);
  return graph()->GetConstant0();
}


Handle<Code> CreateWeakCellStub::GenerateCode() { return DoGenerateCode(this); }


template <>
HValue* CodeStubGraphBuilder<LoadScriptContextFieldStub>::BuildCodeStub() {
  int context_index = casted_stub()->context_index();
  int slot_index = casted_stub()->slot_index();

  HValue* script_context = BuildGetScriptContext(context_index);
  return Add<HLoadNamedField>(script_context, nullptr,
                              HObjectAccess::ForContextSlot(slot_index));
}


Handle<Code> LoadScriptContextFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<StoreScriptContextFieldStub>::BuildCodeStub() {
  int context_index = casted_stub()->context_index();
  int slot_index = casted_stub()->slot_index();

  HValue* script_context = BuildGetScriptContext(context_index);
  Add<HStoreNamedField>(script_context,
                        HObjectAccess::ForContextSlot(slot_index),
                        GetParameter(2), STORE_TO_INITIALIZED_ENTRY);
  return GetParameter(2);
}


Handle<Code> StoreScriptContextFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<GrowArrayElementsStub>::BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  if (IsFastDoubleElementsKind(kind)) {
    info()->MarkAsSavesCallerDoubles();
  }

  HValue* object = GetParameter(GrowArrayElementsDescriptor::kObjectIndex);
  HValue* key = GetParameter(GrowArrayElementsDescriptor::kKeyIndex);

  HValue* elements = AddLoadElements(object);
  HValue* current_capacity = Add<HLoadNamedField>(
      elements, nullptr, HObjectAccess::ForFixedArrayLength());

  HValue* length =
      casted_stub()->is_js_array()
          ? Add<HLoadNamedField>(object, static_cast<HValue*>(NULL),
                                 HObjectAccess::ForArrayLength(kind))
          : current_capacity;

  return BuildCheckAndGrowElementsCapacity(object, elements, kind, length,
                                           current_capacity, key);
}


Handle<Code> GrowArrayElementsStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<LoadFastElementStub>::BuildCodeStub() {
  LoadKeyedHoleMode hole_mode = casted_stub()->convert_hole_to_undefined()
                                    ? CONVERT_HOLE_TO_UNDEFINED
                                    : NEVER_RETURN_HOLE;

  HInstruction* load = BuildUncheckedMonomorphicElementAccess(
      GetParameter(LoadDescriptor::kReceiverIndex),
      GetParameter(LoadDescriptor::kNameIndex), NULL,
      casted_stub()->is_js_array(), casted_stub()->elements_kind(), LOAD,
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
  return BuildLoadNamedField(GetParameter(0), casted_stub()->index());
}


Handle<Code> LoadFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArrayBufferViewLoadFieldStub>::BuildCodeStub() {
  return BuildArrayBufferViewFieldAccessor(GetParameter(0), nullptr,
                                           casted_stub()->index());
}


Handle<Code> ArrayBufferViewLoadFieldStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<LoadConstantStub>::BuildCodeStub() {
  HValue* map = AddLoadMap(GetParameter(0), NULL);
  HObjectAccess descriptors_access = HObjectAccess::ForObservableJSObjectOffset(
      Map::kDescriptorsOffset, Representation::Tagged());
  HValue* descriptors = Add<HLoadNamedField>(map, nullptr, descriptors_access);
  HObjectAccess value_access = HObjectAccess::ForObservableJSObjectOffset(
      DescriptorArray::GetValueOffset(casted_stub()->constant_index()));
  return Add<HLoadNamedField>(descriptors, nullptr, value_access);
}


Handle<Code> LoadConstantStub::GenerateCode() { return DoGenerateCode(this); }


HValue* CodeStubGraphBuilderBase::UnmappedCase(HValue* elements, HValue* key,
                                               HValue* value) {
  HValue* result = NULL;
  HInstruction* backing_store =
      Add<HLoadKeyed>(elements, graph()->GetConstant1(), nullptr, nullptr,
                      FAST_ELEMENTS, ALLOW_RETURN_HOLE);
  Add<HCheckMaps>(backing_store, isolate()->factory()->fixed_array_map());
  HValue* backing_store_length = Add<HLoadNamedField>(
      backing_store, nullptr, HObjectAccess::ForFixedArrayLength());
  IfBuilder in_unmapped_range(this);
  in_unmapped_range.If<HCompareNumericAndBranch>(key, backing_store_length,
                                                 Token::LT);
  in_unmapped_range.Then();
  {
    if (value == NULL) {
      result = Add<HLoadKeyed>(backing_store, key, nullptr, nullptr,
                               FAST_HOLEY_ELEMENTS, NEVER_RETURN_HOLE);
    } else {
      Add<HStoreKeyed>(backing_store, key, value, nullptr, FAST_HOLEY_ELEMENTS);
    }
  }
  in_unmapped_range.ElseDeopt(Deoptimizer::kOutsideOfRange);
  in_unmapped_range.End();
  return result;
}


HValue* CodeStubGraphBuilderBase::EmitKeyedSloppyArguments(HValue* receiver,
                                                           HValue* key,
                                                           HValue* value) {
  // Mapped arguments are actual arguments. Unmapped arguments are values added
  // to the arguments object after it was created for the call. Mapped arguments
  // are stored in the context at indexes given by elements[key + 2]. Unmapped
  // arguments are stored as regular indexed properties in the arguments array,
  // held at elements[1]. See NewSloppyArguments() in runtime.cc for a detailed
  // look at argument object construction.
  //
  // The sloppy arguments elements array has a special format:
  //
  // 0: context
  // 1: unmapped arguments array
  // 2: mapped_index0,
  // 3: mapped_index1,
  // ...
  //
  // length is 2 + min(number_of_actual_arguments, number_of_formal_arguments).
  // If key + 2 >= elements.length then attempt to look in the unmapped
  // arguments array (given by elements[1]) and return the value at key, missing
  // to the runtime if the unmapped arguments array is not a fixed array or if
  // key >= unmapped_arguments_array.length.
  //
  // Otherwise, t = elements[key + 2]. If t is the hole, then look up the value
  // in the unmapped arguments array, as described above. Otherwise, t is a Smi
  // index into the context array given at elements[0]. Return the value at
  // context[t].

  bool is_load = value == NULL;

  key = AddUncasted<HForceRepresentation>(key, Representation::Smi());
  IfBuilder positive_smi(this);
  positive_smi.If<HCompareNumericAndBranch>(key, graph()->GetConstant0(),
                                            Token::LT);
  positive_smi.ThenDeopt(Deoptimizer::kKeyIsNegative);
  positive_smi.End();

  HValue* constant_two = Add<HConstant>(2);
  HValue* elements = AddLoadElements(receiver, nullptr);
  HValue* elements_length = Add<HLoadNamedField>(
      elements, nullptr, HObjectAccess::ForFixedArrayLength());
  HValue* adjusted_length = AddUncasted<HSub>(elements_length, constant_two);
  IfBuilder in_range(this);
  in_range.If<HCompareNumericAndBranch>(key, adjusted_length, Token::LT);
  in_range.Then();
  {
    HValue* index = AddUncasted<HAdd>(key, constant_two);
    HInstruction* mapped_index =
        Add<HLoadKeyed>(elements, index, nullptr, nullptr, FAST_HOLEY_ELEMENTS,
                        ALLOW_RETURN_HOLE);

    IfBuilder is_valid(this);
    is_valid.IfNot<HCompareObjectEqAndBranch>(mapped_index,
                                              graph()->GetConstantHole());
    is_valid.Then();
    {
      // TODO(mvstanton): I'd like to assert from this point, that if the
      // mapped_index is not the hole that it is indeed, a smi. An unnecessary
      // smi check is being emitted.
      HValue* the_context = Add<HLoadKeyed>(elements, graph()->GetConstant0(),
                                            nullptr, nullptr, FAST_ELEMENTS);
      STATIC_ASSERT(Context::kHeaderSize == FixedArray::kHeaderSize);
      if (is_load) {
        HValue* result =
            Add<HLoadKeyed>(the_context, mapped_index, nullptr, nullptr,
                            FAST_ELEMENTS, ALLOW_RETURN_HOLE);
        environment()->Push(result);
      } else {
        DCHECK(value != NULL);
        Add<HStoreKeyed>(the_context, mapped_index, value, nullptr,
                         FAST_ELEMENTS);
        environment()->Push(value);
      }
    }
    is_valid.Else();
    {
      HValue* result = UnmappedCase(elements, key, value);
      environment()->Push(is_load ? result : value);
    }
    is_valid.End();
  }
  in_range.Else();
  {
    HValue* result = UnmappedCase(elements, key, value);
    environment()->Push(is_load ? result : value);
  }
  in_range.End();

  return environment()->Pop();
}


template <>
HValue* CodeStubGraphBuilder<KeyedLoadSloppyArgumentsStub>::BuildCodeStub() {
  HValue* receiver = GetParameter(LoadDescriptor::kReceiverIndex);
  HValue* key = GetParameter(LoadDescriptor::kNameIndex);

  return EmitKeyedSloppyArguments(receiver, key, NULL);
}


Handle<Code> KeyedLoadSloppyArgumentsStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<KeyedStoreSloppyArgumentsStub>::BuildCodeStub() {
  HValue* receiver = GetParameter(StoreDescriptor::kReceiverIndex);
  HValue* key = GetParameter(StoreDescriptor::kNameIndex);
  HValue* value = GetParameter(StoreDescriptor::kValueIndex);

  return EmitKeyedSloppyArguments(receiver, key, value);
}


Handle<Code> KeyedStoreSloppyArgumentsStub::GenerateCode() {
  return DoGenerateCode(this);
}


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
                           MUTABLE_HEAP_NUMBER_TYPE);
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
HValue* CodeStubGraphBuilder<StoreFieldStub>::BuildCodeStub() {
  BuildStoreNamedField(GetParameter(0), GetParameter(2), casted_stub()->index(),
                       casted_stub()->representation(), false);
  return GetParameter(2);
}


Handle<Code> StoreFieldStub::GenerateCode() { return DoGenerateCode(this); }


template <>
HValue* CodeStubGraphBuilder<StoreTransitionStub>::BuildCodeStub() {
  HValue* object = GetParameter(StoreTransitionHelper::ReceiverIndex());

  switch (casted_stub()->store_mode()) {
    case StoreTransitionStub::ExtendStorageAndStoreMapAndValue: {
      HValue* properties = Add<HLoadNamedField>(
          object, nullptr, HObjectAccess::ForPropertiesPointer());
      HValue* length = AddLoadFixedArrayLength(properties);
      HValue* delta =
          Add<HConstant>(static_cast<int32_t>(JSObject::kFieldsAdded));
      HValue* new_capacity = AddUncasted<HAdd>(length, delta);

      // Grow properties array.
      ElementsKind kind = FAST_ELEMENTS;
      Add<HBoundsCheck>(new_capacity,
                        Add<HConstant>((Page::kMaxRegularHeapObjectSize -
                                        FixedArray::kHeaderSize) >>
                                       ElementsKindToShiftSize(kind)));

      // Reuse this code for properties backing store allocation.
      HValue* new_properties =
          BuildAllocateAndInitializeArray(kind, new_capacity);

      BuildCopyProperties(properties, new_properties, length, new_capacity);

      Add<HStoreNamedField>(object, HObjectAccess::ForPropertiesPointer(),
                            new_properties);
    }
    // Fall through.
    case StoreTransitionStub::StoreMapAndValue:
      // Store the new value into the "extended" object.
      BuildStoreNamedField(
          object, GetParameter(StoreTransitionHelper::ValueIndex()),
          casted_stub()->index(), casted_stub()->representation(), true);
    // Fall through.

    case StoreTransitionStub::StoreMapOnly:
      // And finally update the map.
      Add<HStoreNamedField>(object, HObjectAccess::ForMap(),
                            GetParameter(StoreTransitionHelper::MapIndex()));
      break;
  }
  return GetParameter(StoreTransitionHelper::ValueIndex());
}


Handle<Code> StoreTransitionStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<StoreFastElementStub>::BuildCodeStub() {
  BuildUncheckedMonomorphicElementAccess(
      GetParameter(StoreDescriptor::kReceiverIndex),
      GetParameter(StoreDescriptor::kNameIndex),
      GetParameter(StoreDescriptor::kValueIndex), casted_stub()->is_js_array(),
      casted_stub()->elements_kind(), STORE, NEVER_RETURN_HOLE,
      casted_stub()->store_mode());

  return GetParameter(2);
}


Handle<Code> StoreFastElementStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<TransitionElementsKindStub>::BuildCodeStub() {
  info()->MarkAsSavesCallerDoubles();

  BuildTransitionElementsKind(GetParameter(0),
                              GetParameter(1),
                              casted_stub()->from_kind(),
                              casted_stub()->to_kind(),
                              casted_stub()->is_js_array());

  return GetParameter(0);
}


Handle<Code> TransitionElementsKindStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<AllocateHeapNumberStub>::BuildCodeStub() {
  HValue* result =
      Add<HAllocate>(Add<HConstant>(HeapNumber::kSize), HType::HeapNumber(),
                     NOT_TENURED, HEAP_NUMBER_TYPE);
  AddStoreMapConstant(result, isolate()->factory()->heap_number_map());
  return result;
}


Handle<Code> AllocateHeapNumberStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<AllocateMutableHeapNumberStub>::BuildCodeStub() {
  HValue* result =
      Add<HAllocate>(Add<HConstant>(HeapNumber::kSize), HType::HeapObject(),
                     NOT_TENURED, MUTABLE_HEAP_NUMBER_TYPE);
  AddStoreMapConstant(result, isolate()->factory()->mutable_heap_number_map());
  return result;
}


Handle<Code> AllocateMutableHeapNumberStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<AllocateInNewSpaceStub>::BuildCodeStub() {
  HValue* result = Add<HAllocate>(GetParameter(0), HType::Tagged(), NOT_TENURED,
                                  JS_OBJECT_TYPE);
  return result;
}


Handle<Code> AllocateInNewSpaceStub::GenerateCode() {
  return DoGenerateCode(this);
}


HValue* CodeStubGraphBuilderBase::BuildArrayConstructor(
    ElementsKind kind,
    AllocationSiteOverrideMode override_mode,
    ArgumentClass argument_class) {
  HValue* constructor = GetParameter(ArrayConstructorStubBase::kConstructor);
  HValue* alloc_site = GetParameter(ArrayConstructorStubBase::kAllocationSite);
  JSArrayBuilder array_builder(this, kind, alloc_site, constructor,
                               override_mode);
  HValue* result = NULL;
  switch (argument_class) {
    case NONE:
      // This stub is very performance sensitive, the generated code must be
      // tuned so that it doesn't build and eager frame.
      info()->MarkMustNotHaveEagerFrame();
      result = array_builder.AllocateEmptyArray();
      break;
    case SINGLE:
      result = BuildArraySingleArgumentConstructor(&array_builder);
      break;
    case MULTIPLE:
      result = BuildArrayNArgumentsConstructor(&array_builder, kind);
      break;
  }

  return result;
}


HValue* CodeStubGraphBuilderBase::BuildInternalArrayConstructor(
    ElementsKind kind, ArgumentClass argument_class) {
  HValue* constructor = GetParameter(
      InternalArrayConstructorStubBase::kConstructor);
  JSArrayBuilder array_builder(this, kind, constructor);

  HValue* result = NULL;
  switch (argument_class) {
    case NONE:
      // This stub is very performance sensitive, the generated code must be
      // tuned so that it doesn't build and eager frame.
      info()->MarkMustNotHaveEagerFrame();
      result = array_builder.AllocateEmptyArray();
      break;
    case SINGLE:
      result = BuildArraySingleArgumentConstructor(&array_builder);
      break;
    case MULTIPLE:
      result = BuildArrayNArgumentsConstructor(&array_builder, kind);
      break;
  }
  return result;
}


HValue* CodeStubGraphBuilderBase::BuildArraySingleArgumentConstructor(
    JSArrayBuilder* array_builder) {
  // Smi check and range check on the input arg.
  HValue* constant_one = graph()->GetConstant1();
  HValue* constant_zero = graph()->GetConstant0();

  HInstruction* elements = Add<HArgumentsElements>(false);
  HInstruction* argument = Add<HAccessArgumentsAt>(
      elements, constant_one, constant_zero);

  return BuildAllocateArrayFromLength(array_builder, argument);
}


HValue* CodeStubGraphBuilderBase::BuildArrayNArgumentsConstructor(
    JSArrayBuilder* array_builder, ElementsKind kind) {
  // Insert a bounds check because the number of arguments might exceed
  // the kInitialMaxFastElementArray limit. This cannot happen for code
  // that was parsed, but calling via Array.apply(thisArg, [...]) might
  // trigger it.
  HValue* length = GetArgumentsLength();
  HConstant* max_alloc_length =
      Add<HConstant>(JSArray::kInitialMaxFastElementArray);
  HValue* checked_length = Add<HBoundsCheck>(length, max_alloc_length);

  // We need to fill with the hole if it's a smi array in the multi-argument
  // case because we might have to bail out while copying arguments into
  // the array because they aren't compatible with a smi array.
  // If it's a double array, no problem, and if it's fast then no
  // problem either because doubles are boxed.
  //
  // TODO(mvstanton): consider an instruction to memset fill the array
  // with zero in this case instead.
  JSArrayBuilder::FillMode fill_mode = IsFastSmiElementsKind(kind)
      ? JSArrayBuilder::FILL_WITH_HOLE
      : JSArrayBuilder::DONT_FILL_WITH_HOLE;
  HValue* new_object = array_builder->AllocateArray(checked_length,
                                                    max_alloc_length,
                                                    checked_length,
                                                    fill_mode);
  HValue* elements = array_builder->GetElementsLocation();
  DCHECK(elements != NULL);

  // Now populate the elements correctly.
  LoopBuilder builder(this,
                      context(),
                      LoopBuilder::kPostIncrement);
  HValue* start = graph()->GetConstant0();
  HValue* key = builder.BeginBody(start, checked_length, Token::LT);
  HInstruction* argument_elements = Add<HArgumentsElements>(false);
  HInstruction* argument = Add<HAccessArgumentsAt>(
      argument_elements, checked_length, key);

  Add<HStoreKeyed>(elements, key, argument, nullptr, kind);
  builder.EndBody();
  return new_object;
}


template <>
HValue* CodeStubGraphBuilder<ArrayNoArgumentConstructorStub>::BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  AllocationSiteOverrideMode override_mode = casted_stub()->override_mode();
  return BuildArrayConstructor(kind, override_mode, NONE);
}


Handle<Code> ArrayNoArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArraySingleArgumentConstructorStub>::
    BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  AllocationSiteOverrideMode override_mode = casted_stub()->override_mode();
  return BuildArrayConstructor(kind, override_mode, SINGLE);
}


Handle<Code> ArraySingleArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ArrayNArgumentsConstructorStub>::BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  AllocationSiteOverrideMode override_mode = casted_stub()->override_mode();
  return BuildArrayConstructor(kind, override_mode, MULTIPLE);
}


Handle<Code> ArrayNArgumentsConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<InternalArrayNoArgumentConstructorStub>::
    BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  return BuildInternalArrayConstructor(kind, NONE);
}


Handle<Code> InternalArrayNoArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<InternalArraySingleArgumentConstructorStub>::
    BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  return BuildInternalArrayConstructor(kind, SINGLE);
}


Handle<Code> InternalArraySingleArgumentConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<InternalArrayNArgumentsConstructorStub>::
    BuildCodeStub() {
  ElementsKind kind = casted_stub()->elements_kind();
  return BuildInternalArrayConstructor(kind, MULTIPLE);
}


Handle<Code> InternalArrayNArgumentsConstructorStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<CompareNilICStub>::BuildCodeInitializedStub() {
  Isolate* isolate = graph()->isolate();
  CompareNilICStub* stub = casted_stub();
  HIfContinuation continuation;
  Handle<Map> sentinel_map(isolate->heap()->meta_map());
  Type* type = stub->GetType(zone(), sentinel_map);
  BuildCompareNil(GetParameter(0), type, &continuation, kEmbedMapsViaWeakCells);
  IfBuilder if_nil(this, &continuation);
  if_nil.Then();
  if (continuation.IsFalseReachable()) {
    if_nil.Else();
    if_nil.Return(graph()->GetConstantFalse());
  }
  if_nil.End();
  return continuation.IsTrueReachable() ? graph()->GetConstantTrue()
                                        : graph()->GetConstantUndefined();
}


Handle<Code> CompareNilICStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<BinaryOpICStub>::BuildCodeInitializedStub() {
  BinaryOpICState state = casted_stub()->state();

  HValue* left = GetParameter(BinaryOpICStub::kLeft);
  HValue* right = GetParameter(BinaryOpICStub::kRight);

  Type* left_type = state.GetLeftType();
  Type* right_type = state.GetRightType();
  Type* result_type = state.GetResultType();

  DCHECK(!left_type->Is(Type::None()) && !right_type->Is(Type::None()) &&
         (state.HasSideEffects() || !result_type->Is(Type::None())));

  HValue* result = NULL;
  HAllocationMode allocation_mode(NOT_TENURED);
  if (state.op() == Token::ADD &&
      (left_type->Maybe(Type::String()) || right_type->Maybe(Type::String())) &&
      !left_type->Is(Type::String()) && !right_type->Is(Type::String())) {
    // For the generic add stub a fast case for string addition is performance
    // critical.
    if (left_type->Maybe(Type::String())) {
      IfBuilder if_leftisstring(this);
      if_leftisstring.If<HIsStringAndBranch>(left);
      if_leftisstring.Then();
      {
        Push(BuildBinaryOperation(state.op(), left, right, Type::String(zone()),
                                  right_type, result_type,
                                  state.fixed_right_arg(), allocation_mode,
                                  state.strength()));
      }
      if_leftisstring.Else();
      {
        Push(BuildBinaryOperation(
            state.op(), left, right, left_type, right_type, result_type,
            state.fixed_right_arg(), allocation_mode, state.strength()));
      }
      if_leftisstring.End();
      result = Pop();
    } else {
      IfBuilder if_rightisstring(this);
      if_rightisstring.If<HIsStringAndBranch>(right);
      if_rightisstring.Then();
      {
        Push(BuildBinaryOperation(state.op(), left, right, left_type,
                                  Type::String(zone()), result_type,
                                  state.fixed_right_arg(), allocation_mode,
                                  state.strength()));
      }
      if_rightisstring.Else();
      {
        Push(BuildBinaryOperation(
            state.op(), left, right, left_type, right_type, result_type,
            state.fixed_right_arg(), allocation_mode, state.strength()));
      }
      if_rightisstring.End();
      result = Pop();
    }
  } else {
    result = BuildBinaryOperation(
        state.op(), left, right, left_type, right_type, result_type,
        state.fixed_right_arg(), allocation_mode, state.strength());
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

  HValue* allocation_site = GetParameter(
      BinaryOpWithAllocationSiteStub::kAllocationSite);
  HValue* left = GetParameter(BinaryOpWithAllocationSiteStub::kLeft);
  HValue* right = GetParameter(BinaryOpWithAllocationSiteStub::kRight);

  Type* left_type = state.GetLeftType();
  Type* right_type = state.GetRightType();
  Type* result_type = state.GetResultType();
  HAllocationMode allocation_mode(allocation_site);

  return BuildBinaryOperation(state.op(), left, right, left_type, right_type,
                              result_type, state.fixed_right_arg(),
                              allocation_mode, state.strength());
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
    Push(BuildNumberToString(input, Type::SignedSmall()));
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
      ToStringDescriptor descriptor(isolate());
      ToStringStub stub(isolate());
      HValue* values[] = {context(), Pop()};
      Push(AddUncasted<HCallWithDescriptor>(
          Add<HConstant>(stub.GetCode()), 0, descriptor,
          Vector<HValue*>(values, arraysize(values))));
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

  HValue* left = GetParameter(StringAddStub::kLeft);
  HValue* right = GetParameter(StringAddStub::kRight);

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
HValue* CodeStubGraphBuilder<ToBooleanStub>::BuildCodeInitializedStub() {
  ToBooleanStub* stub = casted_stub();
  IfBuilder if_true(this);
  if_true.If<HBranch>(GetParameter(0), stub->types());
  if_true.Then();
  if_true.Return(graph()->GetConstantTrue());
  if_true.Else();
  if_true.End();
  return graph()->GetConstantFalse();
}


Handle<Code> ToBooleanStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<StoreGlobalStub>::BuildCodeInitializedStub() {
  StoreGlobalStub* stub = casted_stub();
  HParameter* value = GetParameter(StoreDescriptor::kValueIndex);
  if (stub->check_global()) {
    // Check that the map of the global has not changed: use a placeholder map
    // that will be replaced later with the global object's map.
    HParameter* proxy = GetParameter(StoreDescriptor::kReceiverIndex);
    HValue* proxy_map =
        Add<HLoadNamedField>(proxy, nullptr, HObjectAccess::ForMap());
    HValue* global =
        Add<HLoadNamedField>(proxy_map, nullptr, HObjectAccess::ForPrototype());
    HValue* map_cell = Add<HConstant>(isolate()->factory()->NewWeakCell(
        StoreGlobalStub::global_map_placeholder(isolate())));
    HValue* expected_map = Add<HLoadNamedField>(
        map_cell, nullptr, HObjectAccess::ForWeakCellValue());
    HValue* map =
        Add<HLoadNamedField>(global, nullptr, HObjectAccess::ForMap());
    IfBuilder map_check(this);
    map_check.IfNot<HCompareObjectEqAndBranch>(expected_map, map);
    map_check.ThenDeopt(Deoptimizer::kUnknownMap);
    map_check.End();
  }

  HValue* weak_cell = Add<HConstant>(isolate()->factory()->NewWeakCell(
      StoreGlobalStub::property_cell_placeholder(isolate())));
  HValue* cell = Add<HLoadNamedField>(weak_cell, nullptr,
                                      HObjectAccess::ForWeakCellValue());
  Add<HCheckHeapObject>(cell);
  HObjectAccess access = HObjectAccess::ForPropertyCellValue();
  // Load the payload of the global parameter cell. A hole indicates that the
  // cell has been invalidated and that the store must be handled by the
  // runtime.
  HValue* cell_contents = Add<HLoadNamedField>(cell, nullptr, access);

  auto cell_type = stub->cell_type();
  if (cell_type == PropertyCellType::kConstant ||
      cell_type == PropertyCellType::kUndefined) {
    // This is always valid for all states a cell can be in.
    IfBuilder builder(this);
    builder.If<HCompareObjectEqAndBranch>(cell_contents, value);
    builder.Then();
    builder.ElseDeopt(
        Deoptimizer::kUnexpectedCellContentsInConstantGlobalStore);
    builder.End();
  } else {
    IfBuilder builder(this);
    HValue* hole_value = graph()->GetConstantHole();
    builder.If<HCompareObjectEqAndBranch>(cell_contents, hole_value);
    builder.Then();
    builder.Deopt(Deoptimizer::kUnexpectedCellContentsInGlobalStore);
    builder.Else();
    // When dealing with constant types, the type may be allowed to change, as
    // long as optimized code remains valid.
    if (cell_type == PropertyCellType::kConstantType) {
      switch (stub->constant_type()) {
        case PropertyCellConstantType::kSmi:
          access = access.WithRepresentation(Representation::Smi());
          break;
        case PropertyCellConstantType::kStableMap: {
          // It is sufficient here to check that the value and cell contents
          // have identical maps, no matter if they are stable or not or if they
          // are the maps that were originally in the cell or not. If optimized
          // code will deopt when a cell has a unstable map and if it has a
          // dependency on a stable map, it will deopt if the map destabilizes.
          Add<HCheckHeapObject>(value);
          Add<HCheckHeapObject>(cell_contents);
          HValue* expected_map = Add<HLoadNamedField>(cell_contents, nullptr,
                                                      HObjectAccess::ForMap());
          HValue* map =
              Add<HLoadNamedField>(value, nullptr, HObjectAccess::ForMap());
          IfBuilder map_check(this);
          map_check.IfNot<HCompareObjectEqAndBranch>(expected_map, map);
          map_check.ThenDeopt(Deoptimizer::kUnknownMap);
          map_check.End();
          access = access.WithRepresentation(Representation::HeapObject());
          break;
        }
      }
    }
    Add<HStoreNamedField>(cell, access, value);
    builder.End();
  }

  return value;
}


Handle<Code> StoreGlobalStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ElementsTransitionAndStoreStub>::BuildCodeStub() {
  HValue* object = GetParameter(StoreTransitionHelper::ReceiverIndex());
  HValue* key = GetParameter(StoreTransitionHelper::NameIndex());
  HValue* value = GetParameter(StoreTransitionHelper::ValueIndex());
  HValue* map = GetParameter(StoreTransitionHelper::MapIndex());

  if (FLAG_trace_elements_transitions) {
    // Tracing elements transitions is the job of the runtime.
    Add<HDeoptimize>(Deoptimizer::kTracingElementsTransitions,
                     Deoptimizer::EAGER);
  } else {
    info()->MarkAsSavesCallerDoubles();

    BuildTransitionElementsKind(object, map,
                                casted_stub()->from_kind(),
                                casted_stub()->to_kind(),
                                casted_stub()->is_jsarray());

    BuildUncheckedMonomorphicElementAccess(object, key, value,
                                           casted_stub()->is_jsarray(),
                                           casted_stub()->to_kind(),
                                           STORE, ALLOW_RETURN_HOLE,
                                           casted_stub()->store_mode());
  }

  return value;
}


Handle<Code> ElementsTransitionAndStoreStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<ToObjectStub>::BuildCodeStub() {
  HValue* receiver = GetParameter(ToObjectDescriptor::kReceiverIndex);
  return BuildToObject(receiver);
}


Handle<Code> ToObjectStub::GenerateCode() { return DoGenerateCode(this); }


void CodeStubGraphBuilderBase::BuildCheckAndInstallOptimizedCode(
    HValue* js_function,
    HValue* native_context,
    IfBuilder* builder,
    HValue* optimized_map,
    HValue* map_index) {
  HValue* osr_ast_id_none = Add<HConstant>(BailoutId::None().ToInt());
  HValue* context_slot = LoadFromOptimizedCodeMap(
      optimized_map, map_index, SharedFunctionInfo::kContextOffset);
  context_slot = Add<HLoadNamedField>(context_slot, nullptr,
                                      HObjectAccess::ForWeakCellValue());
  HValue* osr_ast_slot = LoadFromOptimizedCodeMap(
      optimized_map, map_index, SharedFunctionInfo::kOsrAstIdOffset);
  HValue* code_object = LoadFromOptimizedCodeMap(
      optimized_map, map_index, SharedFunctionInfo::kCachedCodeOffset);
  code_object = Add<HLoadNamedField>(code_object, nullptr,
                                     HObjectAccess::ForWeakCellValue());
  builder->If<HCompareObjectEqAndBranch>(native_context,
                                         context_slot);
  builder->AndIf<HCompareObjectEqAndBranch>(osr_ast_slot, osr_ast_id_none);
  builder->And();
  builder->IfNot<HCompareObjectEqAndBranch>(code_object,
                                            graph()->GetConstant0());
  builder->Then();
  HValue* literals = LoadFromOptimizedCodeMap(optimized_map,
      map_index, SharedFunctionInfo::kLiteralsOffset);
  literals = Add<HLoadNamedField>(literals, nullptr,
                                  HObjectAccess::ForWeakCellValue());
  IfBuilder maybe_deopt(this);
  maybe_deopt.If<HCompareObjectEqAndBranch>(literals, graph()->GetConstant0());
  maybe_deopt.ThenDeopt(Deoptimizer::kLiteralsWereDisposed);
  maybe_deopt.End();

  BuildInstallOptimizedCode(js_function, native_context, code_object, literals);

  // The builder continues in the "then" after this function.
}


void CodeStubGraphBuilderBase::BuildInstallOptimizedCode(HValue* js_function,
                                                         HValue* native_context,
                                                         HValue* code_object,
                                                         HValue* literals) {
  Counters* counters = isolate()->counters();
  AddIncrementCounter(counters->fast_new_closure_install_optimized());

  // TODO(fschneider): Idea: store proper code pointers in the optimized code
  // map and either unmangle them on marking or do nothing as the whole map is
  // discarded on major GC anyway.
  Add<HStoreCodeEntry>(js_function, code_object);
  Add<HStoreNamedField>(js_function, HObjectAccess::ForLiteralsPointer(),
                        literals);

  // Now link a function into a list of optimized functions.
  HValue* optimized_functions_list = Add<HLoadNamedField>(
      native_context, nullptr,
      HObjectAccess::ForContextSlot(Context::OPTIMIZED_FUNCTIONS_LIST));
  Add<HStoreNamedField>(js_function,
                        HObjectAccess::ForNextFunctionLinkPointer(),
                        optimized_functions_list);

  // This store is the only one that should have a write barrier.
  Add<HStoreNamedField>(native_context,
           HObjectAccess::ForContextSlot(Context::OPTIMIZED_FUNCTIONS_LIST),
           js_function);
}


void CodeStubGraphBuilderBase::BuildInstallCode(HValue* js_function,
                                                HValue* shared_info) {
  Add<HStoreNamedField>(js_function,
                        HObjectAccess::ForNextFunctionLinkPointer(),
                        graph()->GetConstantUndefined());
  HValue* code_object = Add<HLoadNamedField>(shared_info, nullptr,
                                             HObjectAccess::ForCodeOffset());
  Add<HStoreCodeEntry>(js_function, code_object);
}


HInstruction* CodeStubGraphBuilderBase::LoadFromOptimizedCodeMap(
    HValue* optimized_map,
    HValue* iterator,
    int field_offset) {
  // By making sure to express these loads in the form [<hvalue> + constant]
  // the keyed load can be hoisted.
  DCHECK(field_offset >= 0 && field_offset < SharedFunctionInfo::kEntryLength);
  HValue* field_slot = iterator;
  if (field_offset > 0) {
    HValue* field_offset_value = Add<HConstant>(field_offset);
    field_slot = AddUncasted<HAdd>(iterator, field_offset_value);
  }
  HInstruction* field_entry = Add<HLoadKeyed>(optimized_map, field_slot,
                                              nullptr, nullptr, FAST_ELEMENTS);
  return field_entry;
}


void CodeStubGraphBuilderBase::BuildInstallFromOptimizedCodeMap(
    HValue* js_function,
    HValue* shared_info,
    HValue* native_context) {
  Counters* counters = isolate()->counters();
  Factory* factory = isolate()->factory();
  IfBuilder is_optimized(this);
  HInstruction* optimized_map = Add<HLoadNamedField>(
      shared_info, nullptr, HObjectAccess::ForOptimizedCodeMap());
  HValue* null_constant = Add<HConstant>(0);
  is_optimized.If<HCompareObjectEqAndBranch>(optimized_map, null_constant);
  is_optimized.Then();
  {
    BuildInstallCode(js_function, shared_info);
  }
  is_optimized.Else();
  {
    AddIncrementCounter(counters->fast_new_closure_try_optimized());
    // The {optimized_map} points to fixed array of 4-element entries:
    //   (native context, optimized code, literals, ast-id).
    // Iterate through the {optimized_map} backwards. After the loop, if no
    // matching optimized code was found, install unoptimized code.
    //   for(i = map.length() - SharedFunctionInfo::kEntryLength;
    //       i >= SharedFunctionInfo::kEntriesStart;
    //       i -= SharedFunctionInfo::kEntryLength) { ... }
    HValue* first_entry_index =
        Add<HConstant>(SharedFunctionInfo::kEntriesStart);
    HValue* shared_function_entry_length =
        Add<HConstant>(SharedFunctionInfo::kEntryLength);
    LoopBuilder loop_builder(this, context(), LoopBuilder::kPostDecrement,
                             shared_function_entry_length);
    HValue* array_length = Add<HLoadNamedField>(
        optimized_map, nullptr, HObjectAccess::ForFixedArrayLength());
    HValue* start_pos =
        AddUncasted<HSub>(array_length, shared_function_entry_length);
    HValue* slot_iterator =
        loop_builder.BeginBody(start_pos, first_entry_index, Token::GTE);
    {
      IfBuilder done_check(this);
      BuildCheckAndInstallOptimizedCode(js_function, native_context,
                                        &done_check, optimized_map,
                                        slot_iterator);
      // Fall out of the loop
      loop_builder.Break();
    }
    loop_builder.EndBody();

    // If {slot_iterator} is less than the first entry index, then we failed to
    // find a context-dependent code and try context-independent code next.
    IfBuilder no_optimized_code_check(this);
    no_optimized_code_check.If<HCompareNumericAndBranch>(
        slot_iterator, first_entry_index, Token::LT);
    no_optimized_code_check.Then();
    {
      IfBuilder shared_code_check(this);
      HValue* shared_code =
          Add<HLoadNamedField>(optimized_map, nullptr,
                               HObjectAccess::ForOptimizedCodeMapSharedCode());
      shared_code = Add<HLoadNamedField>(shared_code, nullptr,
                                         HObjectAccess::ForWeakCellValue());
      shared_code_check.IfNot<HCompareObjectEqAndBranch>(
          shared_code, graph()->GetConstant0());
      shared_code_check.Then();
      {
        // Store the context-independent optimized code.
        HValue* literals = Add<HConstant>(factory->empty_fixed_array());
        BuildInstallOptimizedCode(js_function, native_context, shared_code,
                                  literals);
      }
      shared_code_check.Else();
      {
        // Store the unoptimized code.
        BuildInstallCode(js_function, shared_info);
      }
    }
  }
}


template<>
HValue* CodeStubGraphBuilder<FastNewClosureStub>::BuildCodeStub() {
  Counters* counters = isolate()->counters();
  Factory* factory = isolate()->factory();
  HInstruction* empty_fixed_array =
      Add<HConstant>(factory->empty_fixed_array());
  HValue* shared_info = GetParameter(0);

  AddIncrementCounter(counters->fast_new_closure_total());

  // Create a new closure from the given function info in new space
  HValue* size = Add<HConstant>(JSFunction::kSize);
  HInstruction* js_function =
      Add<HAllocate>(size, HType::JSObject(), NOT_TENURED, JS_FUNCTION_TYPE);

  int map_index = Context::FunctionMapIndex(casted_stub()->language_mode(),
                                            casted_stub()->kind());

  // Compute the function map in the current native context and set that
  // as the map of the allocated object.
  HInstruction* native_context = BuildGetNativeContext();
  HInstruction* map_slot_value = Add<HLoadNamedField>(
      native_context, nullptr, HObjectAccess::ForContextSlot(map_index));
  Add<HStoreNamedField>(js_function, HObjectAccess::ForMap(), map_slot_value);

  // Initialize the rest of the function.
  Add<HStoreNamedField>(js_function, HObjectAccess::ForPropertiesPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(js_function, HObjectAccess::ForElementsPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(js_function, HObjectAccess::ForLiteralsPointer(),
                        empty_fixed_array);
  Add<HStoreNamedField>(js_function, HObjectAccess::ForPrototypeOrInitialMap(),
                        graph()->GetConstantHole());
  Add<HStoreNamedField>(
      js_function, HObjectAccess::ForSharedFunctionInfoPointer(), shared_info);
  Add<HStoreNamedField>(js_function, HObjectAccess::ForFunctionContextPointer(),
                        context());

  // Initialize the code pointer in the function to be the one found in the
  // shared function info object. But first check if there is an optimized
  // version for our context.
  BuildInstallFromOptimizedCodeMap(js_function, shared_info, native_context);

  return js_function;
}


Handle<Code> FastNewClosureStub::GenerateCode() {
  return DoGenerateCode(this);
}


template<>
HValue* CodeStubGraphBuilder<FastNewContextStub>::BuildCodeStub() {
  int length = casted_stub()->slots() + Context::MIN_CONTEXT_SLOTS;

  // Get the function.
  HParameter* function = GetParameter(FastNewContextStub::kFunction);

  // Allocate the context in new space.
  HAllocate* function_context = Add<HAllocate>(
      Add<HConstant>(length * kPointerSize + FixedArray::kHeaderSize),
      HType::HeapObject(), NOT_TENURED, FIXED_ARRAY_TYPE);

  // Set up the object header.
  AddStoreMapConstant(function_context,
                      isolate()->factory()->function_context_map());
  Add<HStoreNamedField>(function_context,
                        HObjectAccess::ForFixedArrayLength(),
                        Add<HConstant>(length));

  // Set up the fixed slots.
  Add<HStoreNamedField>(function_context,
                        HObjectAccess::ForContextSlot(Context::CLOSURE_INDEX),
                        function);
  Add<HStoreNamedField>(function_context,
                        HObjectAccess::ForContextSlot(Context::PREVIOUS_INDEX),
                        context());
  Add<HStoreNamedField>(function_context,
                        HObjectAccess::ForContextSlot(Context::EXTENSION_INDEX),
                        graph()->GetConstantHole());

  // Copy the native context from the previous context.
  HValue* native_context = Add<HLoadNamedField>(
      context(), nullptr,
      HObjectAccess::ForContextSlot(Context::NATIVE_CONTEXT_INDEX));
  Add<HStoreNamedField>(function_context, HObjectAccess::ForContextSlot(
                                              Context::NATIVE_CONTEXT_INDEX),
                        native_context);

  // Initialize the rest of the slots to undefined.
  for (int i = Context::MIN_CONTEXT_SLOTS; i < length; ++i) {
    Add<HStoreNamedField>(function_context,
                          HObjectAccess::ForContextSlot(i),
                          graph()->GetConstantUndefined());
  }

  return function_context;
}


Handle<Code> FastNewContextStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
HValue* CodeStubGraphBuilder<LoadDictionaryElementStub>::BuildCodeStub() {
  HValue* receiver = GetParameter(LoadDescriptor::kReceiverIndex);
  HValue* key = GetParameter(LoadDescriptor::kNameIndex);

  Add<HCheckSmi>(key);

  HValue* elements = AddLoadElements(receiver);

  HValue* hash = BuildElementIndexHash(key);

  return BuildUncheckedDictionaryElementLoad(receiver, elements, key, hash,
                                             casted_stub()->language_mode());
}


Handle<Code> LoadDictionaryElementStub::GenerateCode() {
  return DoGenerateCode(this);
}


template<>
HValue* CodeStubGraphBuilder<RegExpConstructResultStub>::BuildCodeStub() {
  // Determine the parameters.
  HValue* length = GetParameter(RegExpConstructResultStub::kLength);
  HValue* index = GetParameter(RegExpConstructResultStub::kIndex);
  HValue* input = GetParameter(RegExpConstructResultStub::kInput);

  info()->MarkMustNotHaveEagerFrame();

  return BuildRegExpConstructResult(length, index, input);
}


Handle<Code> RegExpConstructResultStub::GenerateCode() {
  return DoGenerateCode(this);
}


template <>
class CodeStubGraphBuilder<KeyedLoadGenericStub>
    : public CodeStubGraphBuilderBase {
 public:
  explicit CodeStubGraphBuilder(CompilationInfo* info)
      : CodeStubGraphBuilderBase(info) {}

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
  HValue* receiver = GetParameter(LoadDescriptor::kReceiverIndex);
  HValue* key = GetParameter(LoadDescriptor::kNameIndex);
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

      Push(BuildUncheckedDictionaryElementLoad(receiver, elements, key, hash,
                                               casted_stub()->language_mode()));
    }
    kind_if.Else();

    // The SLOW_SLOPPY_ARGUMENTS_ELEMENTS check generates a "kind_if.Then"
    STATIC_ASSERT(FAST_SLOPPY_ARGUMENTS_ELEMENTS <
                  SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    BuildElementsKindLimitCheck(&kind_if, bit_field2,
                                SLOW_SLOPPY_ARGUMENTS_ELEMENTS);
    // Non-strict elements are not handled.
    Add<HDeoptimize>(Deoptimizer::kNonStrictElementsInKeyedLoadGenericStub,
                     Deoptimizer::EAGER);
    Push(graph()->GetConstant0());

    kind_if.ElseDeopt(
        Deoptimizer::kElementsKindUnhandledInKeyedLoadGenericStub);

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

      HValue* value = BuildUncheckedDictionaryElementLoad(
          receiver, properties, key, hash, casted_stub()->language_mode());
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
            Runtime::FunctionForId(is_strong(casted_stub()->language_mode())
                                       ? Runtime::kKeyedGetPropertyStrong
                                       : Runtime::kKeyedGetProperty),
            2));
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
