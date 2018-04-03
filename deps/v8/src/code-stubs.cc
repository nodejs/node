// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/code-stubs.h"

#include <sstream>

#include "src/arguments.h"
#include "src/assembler-inl.h"
#include "src/ast/ast.h"
#include "src/bootstrapper.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/code-stubs-utils.h"
#include "src/counters.h"
#include "src/factory.h"
#include "src/gdb-jit.h"
#include "src/heap/heap-inl.h"
#include "src/ic/ic-stats.h"
#include "src/ic/ic.h"
#include "src/macro-assembler.h"
#include "src/objects-inl.h"
#include "src/tracing/tracing-category-observer.h"

namespace v8 {
namespace internal {

using compiler::CodeAssemblerState;

CodeStubDescriptor::CodeStubDescriptor(CodeStub* stub)
    : isolate_(stub->isolate()),
      call_descriptor_(stub->GetCallInterfaceDescriptor()),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(nullptr),
      miss_handler_(),
      has_miss_handler_(false) {
  stub->InitializeDescriptor(this);
}

CodeStubDescriptor::CodeStubDescriptor(Isolate* isolate, uint32_t stub_key)
    : isolate_(isolate),
      stack_parameter_count_(no_reg),
      hint_stack_parameter_count_(-1),
      function_mode_(NOT_JS_FUNCTION_STUB_MODE),
      deoptimization_handler_(nullptr),
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
  NumberDictionary* stubs = isolate()->heap()->code_stubs();
  int index = stubs->FindEntry(isolate(), GetKey());
  if (index != NumberDictionary::kNotFound) {
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


void CodeStub::DeleteStubFromCacheForTesting() {
  Heap* heap = isolate_->heap();
  Handle<NumberDictionary> dict(heap->code_stubs());
  int entry = dict->FindEntry(GetKey());
  DCHECK_NE(NumberDictionary::kNotFound, entry);
  dict = NumberDictionary::DeleteEntry(dict, entry);
  heap->SetRootCodeStubs(*dict);
}

Handle<Code> PlatformCodeStub::GenerateCode() {
  Factory* factory = isolate()->factory();

  // Generate the new code.
  MacroAssembler masm(isolate(), nullptr, 256, CodeObjectRequired::kYes);

  {
    // Update the static counter each time a new code stub is generated.
    isolate()->counters()->code_stubs()->Increment();

    // Generate the code for the stub.
    // TODO(yangguo): remove this once we can serialize IC stubs.
    masm.enable_serializer();
    NoCurrentFrameScope scope(&masm);
    Generate(&masm);
  }

  // Allocate the handler table.
  Handle<HandlerTable> table = GenerateHandlerTable();

  // Create the code object.
  CodeDesc desc;
  masm.GetCode(isolate(), &desc);
  // Copy the generated code into a heap object.
  Handle<Code> new_object = factory->NewCode(
      desc, Code::STUB, masm.CodeObject(), Builtins::kNoBuiltinId, table,
      MaybeHandle<ByteArray>(), DeoptimizationData::Empty(isolate()),
      NeedsImmovableCode(), GetKey());
  return new_object;
}


Handle<Code> CodeStub::GetCode() {
  Heap* heap = isolate()->heap();
  Code* code;
  if (FindCodeInCache(&code)) {
    DCHECK(code->is_stub());
    return handle(code);
  }

  {
    HandleScope scope(isolate());
    // Canonicalize handles, so that we can share constant pool entries pointing
    // to code targets without dereferencing their handles.
    CanonicalHandleScope canonical(isolate());

    Handle<Code> new_object = GenerateCode();
    DCHECK_EQ(GetKey(), new_object->stub_key());
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

    // Update the dictionary and the root in Heap.
    Handle<NumberDictionary> dict =
        NumberDictionary::Set(handle(heap->code_stubs()), GetKey(), new_object);
    heap->SetRootCodeStubs(*dict);
    code = *new_object;
  }

  Activate(code);
  DCHECK(!NeedsImmovableCode() || Heap::IsImmovable(code));
  return Handle<Code>(code, isolate());
}

CodeStub::Major CodeStub::GetMajorKey(Code* code_stub) {
  return MajorKeyFromKey(code_stub->stub_key());
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
  }
  return nullptr;
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

Handle<HandlerTable> PlatformCodeStub::GenerateHandlerTable() {
  return HandlerTable::Empty(isolate());
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
  *code_out = stub->GetCode();
}


MaybeHandle<Code> CodeStub::GetCode(Isolate* isolate, uint32_t key) {
  HandleScope scope(isolate);
  Handle<Code> code;
  void** value_out = reinterpret_cast<void**>(&code);
  Dispatch(isolate, key, value_out, &GetCodeDispatchCall);
  return scope.CloseAndEscape(code);
}


void StringAddStub::PrintBaseName(std::ostream& os) const {  // NOLINT
  os << "StringAddStub_" << flags() << "_" << pretenure_flag();
}

TF_STUB(StringAddStub, CodeStubAssembler) {
  StringAddFlags flags = stub->flags();
  PretenureFlag pretenure_flag = stub->pretenure_flag();

  Node* left = Parameter(Descriptor::kLeft);
  Node* right = Parameter(Descriptor::kRight);
  Node* context = Parameter(Descriptor::kContext);

  if ((flags & STRING_ADD_CHECK_LEFT) != 0) {
    DCHECK_NE(flags & STRING_ADD_CONVERT, 0);
    // TODO(danno): The ToString and JSReceiverToPrimitive below could be
    // combined to avoid duplicate smi and instance type checks.
    left = ToString(context, JSReceiverToPrimitive(context, left));
  }
  if ((flags & STRING_ADD_CHECK_RIGHT) != 0) {
    DCHECK_NE(flags & STRING_ADD_CONVERT, 0);
    // TODO(danno): The ToString and JSReceiverToPrimitive below could be
    // combined to avoid duplicate smi and instance type checks.
    right = ToString(context, JSReceiverToPrimitive(context, right));
  }

  if ((flags & STRING_ADD_CHECK_BOTH) == 0) {
    CodeStubAssembler::AllocationFlag allocation_flags =
        (pretenure_flag == TENURED) ? CodeStubAssembler::kPretenured
                                    : CodeStubAssembler::kNone;
    Return(StringAdd(context, left, right, allocation_flags));
  } else {
    Callable callable = CodeFactory::StringAdd(isolate(), STRING_ADD_CHECK_NONE,
                                               pretenure_flag);
    TailCallStub(callable, context, left, right);
  }
}

Handle<Code> TurboFanCodeStub::GenerateCode() {
  const char* name = CodeStub::MajorName(MajorKey());
  Zone zone(isolate()->allocator(), ZONE_NAME);
  CallInterfaceDescriptor descriptor(GetCallInterfaceDescriptor());
  compiler::CodeAssemblerState state(isolate(), &zone, descriptor, Code::STUB,
                                     name, 1, GetKey());
  GenerateAssembly(&state);
  return compiler::CodeAssembler::GenerateCode(&state);
}

TF_STUB(ElementsTransitionAndStoreStub, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* map = Parameter(Descriptor::kMap);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Comment(
      "ElementsTransitionAndStoreStub: from_kind=%s, to_kind=%s,"
      " is_jsarray=%d, store_mode=%d",
      ElementsKindToString(stub->from_kind()),
      ElementsKindToString(stub->to_kind()), stub->is_jsarray(),
      stub->store_mode());

  Label miss(this);

  if (FLAG_trace_elements_transitions) {
    // Tracing elements transitions is the job of the runtime.
    Goto(&miss);
  } else {
    TransitionElementsKind(receiver, map, stub->from_kind(), stub->to_kind(),
                           stub->is_jsarray(), &miss);
    EmitElementStore(receiver, key, value, stub->is_jsarray(), stub->to_kind(),
                     stub->store_mode(), &miss);
    Return(value);
  }

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kElementsTransitionAndStoreIC_Miss, context,
                    receiver, key, value, map, slot, vector);
  }
}

TF_STUB(TransitionElementsKindStub, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* object = Parameter(Descriptor::kObject);
  Node* new_map = Parameter(Descriptor::kMap);

  Label bailout(this);
  TransitionElementsKind(object, new_map, stub->from_kind(), stub->to_kind(),
                         stub->is_jsarray(), &bailout);
  Return(object);

  BIND(&bailout);
  {
    Comment("Call runtime");
    TailCallRuntime(Runtime::kTransitionElementsKind, context, object, new_map);
  }
}

// TODO(ishell): move to builtins-handler-gen.
TF_STUB(KeyedLoadSloppyArgumentsStub, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  Node* result = LoadKeyedSloppyArguments(receiver, key, &miss);
  Return(result);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key, slot,
                    vector);
  }
}

// TODO(ishell): move to builtins-handler-gen.
TF_STUB(KeyedStoreSloppyArgumentsStub, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  StoreKeyedSloppyArguments(receiver, key, value, &miss);
  Return(value);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value, slot, vector,
                    receiver, key);
  }
}

// TODO(ishell): move to builtins-handler-gen.
TF_STUB(StoreInterceptorStub, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);
  TailCallRuntime(Runtime::kStorePropertyWithInterceptor, context, value, slot,
                  vector, receiver, name);
}

// TODO(ishell): move to builtins-handler-gen.
TF_STUB(LoadIndexedInterceptorStub, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label if_keyispositivesmi(this), if_keyisinvalid(this);
  Branch(TaggedIsPositiveSmi(key), &if_keyispositivesmi, &if_keyisinvalid);
  BIND(&if_keyispositivesmi);
  TailCallRuntime(Runtime::kLoadElementWithInterceptor, context, receiver, key);

  BIND(&if_keyisinvalid);
  TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, key, slot,
                  vector);
}

Handle<HandlerTable> JSEntryStub::GenerateHandlerTable() {
  Handle<FixedArray> handler_table =
      isolate()->factory()->NewFixedArray(1, TENURED);
  handler_table->set(0, Smi::FromInt(handler_offset_));
  return Handle<HandlerTable>::cast(handler_table);
}


// TODO(ishell): move to builtins.
TF_STUB(GetPropertyStub, CodeStubAssembler) {
  Label call_runtime(this, Label::kDeferred), return_undefined(this), end(this);

  Node* object = Parameter(Descriptor::kObject);
  Node* key = Parameter(Descriptor::kKey);
  Node* context = Parameter(Descriptor::kContext);
  VARIABLE(var_result, MachineRepresentation::kTagged);

  CodeStubAssembler::LookupInHolder lookup_property_in_holder =
      [=, &var_result, &end](Node* receiver, Node* holder, Node* holder_map,
                             Node* holder_instance_type, Node* unique_name,
                             Label* next_holder, Label* if_bailout) {
        VARIABLE(var_value, MachineRepresentation::kTagged);
        Label if_found(this);
        TryGetOwnProperty(context, receiver, holder, holder_map,
                          holder_instance_type, unique_name, &if_found,
                          &var_value, next_holder, if_bailout);
        BIND(&if_found);
        {
          var_result.Bind(var_value.value());
          Goto(&end);
        }
      };

  CodeStubAssembler::LookupInHolder lookup_element_in_holder =
      [=](Node* receiver, Node* holder, Node* holder_map,
          Node* holder_instance_type, Node* index, Label* next_holder,
          Label* if_bailout) {
        // Not supported yet.
        Use(next_holder);
        Goto(if_bailout);
      };

  TryPrototypeChainLookup(object, key, lookup_property_in_holder,
                          lookup_element_in_holder, &return_undefined,
                          &call_runtime);

  BIND(&return_undefined);
  {
    var_result.Bind(UndefinedConstant());
    Goto(&end);
  }

  BIND(&call_runtime);
  {
    var_result.Bind(CallRuntime(Runtime::kGetProperty, context, object, key));
    Goto(&end);
  }

  BIND(&end);
  Return(var_result.value());
}

// TODO(ishell): move to builtins-handler-gen.
TF_STUB(StoreSlowElementStub, CodeStubAssembler) {
  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  TailCallRuntime(Runtime::kKeyedStoreIC_Slow, context, value, slot, vector,
                  receiver, name);
}

TF_STUB(StoreFastElementStub, CodeStubAssembler) {
  Comment("StoreFastElementStub: js_array=%d, elements_kind=%s, store_mode=%d",
          stub->is_js_array(), ElementsKindToString(stub->elements_kind()),
          stub->store_mode());

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* key = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  Label miss(this);

  EmitElementStore(receiver, key, value, stub->is_js_array(),
                   stub->elements_kind(), stub->store_mode(), &miss);
  Return(value);

  BIND(&miss);
  {
    Comment("Miss");
    TailCallRuntime(Runtime::kKeyedStoreIC_Miss, context, value, slot, vector,
                    receiver, key);
  }
}

// static
void StoreFastElementStub::GenerateAheadOfTime(Isolate* isolate) {
  if (FLAG_minimal) return;
  StoreFastElementStub(isolate, false, HOLEY_ELEMENTS, STANDARD_STORE)
      .GetCode();
  StoreFastElementStub(isolate, false, HOLEY_ELEMENTS,
                       STORE_AND_GROW_NO_TRANSITION)
      .GetCode();
  for (int i = FIRST_FAST_ELEMENTS_KIND; i <= LAST_FAST_ELEMENTS_KIND; i++) {
    ElementsKind kind = static_cast<ElementsKind>(i);
    StoreFastElementStub(isolate, true, kind, STANDARD_STORE).GetCode();
    StoreFastElementStub(isolate, true, kind, STORE_AND_GROW_NO_TRANSITION)
        .GetCode();
  }
}


void ProfileEntryHookStub::EntryHookTrampoline(intptr_t function,
                                               intptr_t stack_pointer,
                                               Isolate* isolate) {
  FunctionEntryHook entry_hook = isolate->function_entry_hook();
  DCHECK_NOT_NULL(entry_hook);
  entry_hook(function, stack_pointer);
}

TF_STUB(ArrayNoArgumentConstructorStub, CodeStubAssembler) {
  ElementsKind elements_kind = stub->elements_kind();
  Node* native_context = LoadObjectField(Parameter(Descriptor::kFunction),
                                         JSFunction::kContextOffset);
  bool track_allocation_site =
      AllocationSite::ShouldTrack(elements_kind) &&
      stub->override_mode() != DISABLE_ALLOCATION_SITES;
  Node* allocation_site =
      track_allocation_site ? Parameter(Descriptor::kAllocationSite) : nullptr;
  Node* array_map = LoadJSArrayElementsMap(elements_kind, native_context);
  Node* array =
      AllocateJSArray(elements_kind, array_map,
                      IntPtrConstant(JSArray::kPreallocatedArrayElements),
                      SmiConstant(0), allocation_site);
  Return(array);
}

TF_STUB(InternalArrayNoArgumentConstructorStub, CodeStubAssembler) {
  Node* array_map = LoadObjectField(Parameter(Descriptor::kFunction),
                                    JSFunction::kPrototypeOrInitialMapOffset);
  Node* array = AllocateJSArray(
      stub->elements_kind(), array_map,
      IntPtrConstant(JSArray::kPreallocatedArrayElements), SmiConstant(0));
  Return(array);
}

class ArrayConstructorAssembler : public CodeStubAssembler {
 public:
  typedef compiler::Node Node;

  explicit ArrayConstructorAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  void GenerateConstructor(Node* context, Node* array_function, Node* array_map,
                           Node* array_size, Node* allocation_site,
                           ElementsKind elements_kind, AllocationSiteMode mode);
};

void ArrayConstructorAssembler::GenerateConstructor(
    Node* context, Node* array_function, Node* array_map, Node* array_size,
    Node* allocation_site, ElementsKind elements_kind,
    AllocationSiteMode mode) {
  Label ok(this);
  Label smi_size(this);
  Label small_smi_size(this);
  Label call_runtime(this, Label::kDeferred);

  Branch(TaggedIsSmi(array_size), &smi_size, &call_runtime);

  BIND(&smi_size);

  if (IsFastPackedElementsKind(elements_kind)) {
    Label abort(this, Label::kDeferred);
    Branch(SmiEqual(array_size, SmiConstant(0)), &small_smi_size, &abort);

    BIND(&abort);
    Node* reason = SmiConstant(AbortReason::kAllocatingNonEmptyPackedArray);
    TailCallRuntime(Runtime::kAbort, context, reason);
  } else {
    int element_size =
        IsDoubleElementsKind(elements_kind) ? kDoubleSize : kPointerSize;
    int max_fast_elements =
        (kMaxRegularHeapObjectSize - FixedArray::kHeaderSize - JSArray::kSize -
         AllocationMemento::kSize) /
        element_size;
    Branch(SmiAboveOrEqual(array_size, SmiConstant(max_fast_elements)),
           &call_runtime, &small_smi_size);
  }

  BIND(&small_smi_size);
  {
    Node* array = AllocateJSArray(
        elements_kind, array_map, array_size, array_size,
        mode == DONT_TRACK_ALLOCATION_SITE ? nullptr : allocation_site,
        CodeStubAssembler::SMI_PARAMETERS);
    Return(array);
  }

  BIND(&call_runtime);
  {
    TailCallRuntime(Runtime::kNewArray, context, array_function, array_size,
                    array_function, allocation_site);
  }
}

TF_STUB(ArraySingleArgumentConstructorStub, ArrayConstructorAssembler) {
  ElementsKind elements_kind = stub->elements_kind();
  Node* context = Parameter(Descriptor::kContext);
  Node* function = Parameter(Descriptor::kFunction);
  Node* native_context = LoadObjectField(function, JSFunction::kContextOffset);
  Node* array_map = LoadJSArrayElementsMap(elements_kind, native_context);
  AllocationSiteMode mode = DONT_TRACK_ALLOCATION_SITE;
  if (stub->override_mode() == DONT_OVERRIDE) {
    mode = AllocationSite::ShouldTrack(elements_kind)
               ? TRACK_ALLOCATION_SITE
               : DONT_TRACK_ALLOCATION_SITE;
  }

  Node* array_size = Parameter(Descriptor::kArraySizeSmiParameter);
  Node* allocation_site = Parameter(Descriptor::kAllocationSite);

  GenerateConstructor(context, function, array_map, array_size, allocation_site,
                      elements_kind, mode);
}

TF_STUB(InternalArraySingleArgumentConstructorStub, ArrayConstructorAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* function = Parameter(Descriptor::kFunction);
  Node* array_map =
      LoadObjectField(function, JSFunction::kPrototypeOrInitialMapOffset);
  Node* array_size = Parameter(Descriptor::kArraySizeSmiParameter);
  Node* allocation_site = UndefinedConstant();

  GenerateConstructor(context, function, array_map, array_size, allocation_site,
                      stub->elements_kind(), DONT_TRACK_ALLOCATION_SITE);
}

ArrayConstructorStub::ArrayConstructorStub(Isolate* isolate)
    : PlatformCodeStub(isolate) {}

InternalArrayConstructorStub::InternalArrayConstructorStub(Isolate* isolate)
    : PlatformCodeStub(isolate) {}

CommonArrayConstructorStub::CommonArrayConstructorStub(
    Isolate* isolate, ElementsKind kind,
    AllocationSiteOverrideMode override_mode)
    : TurboFanCodeStub(isolate) {
  // It only makes sense to override local allocation site behavior
  // if there is a difference between the global allocation site policy
  // for an ElementsKind and the desired usage of the stub.
  DCHECK(override_mode != DISABLE_ALLOCATION_SITES ||
         AllocationSite::ShouldTrack(kind));
  set_sub_minor_key(ElementsKindBits::encode(kind) |
                    AllocationSiteOverrideModeBits::encode(override_mode));
}

}  // namespace internal
}  // namespace v8
