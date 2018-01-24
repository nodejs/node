// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/accessor-assembler.h"

#include "src/code-factory.h"
#include "src/code-stubs.h"
#include "src/counters.h"
#include "src/ic/handler-configuration.h"
#include "src/ic/ic.h"
#include "src/ic/stub-cache.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

using compiler::CodeAssemblerState;
using compiler::Node;

//////////////////// Private helpers.

Node* AccessorAssembler::TryMonomorphicCase(Node* slot, Node* vector,
                                            Node* receiver_map,
                                            Label* if_handler,
                                            Variable* var_handler,
                                            Label* if_miss) {
  Comment("TryMonomorphicCase");
  DCHECK_EQ(MachineRepresentation::kTagged, var_handler->rep());

  // TODO(ishell): add helper class that hides offset computations for a series
  // of loads.
  CSA_ASSERT(this, IsFeedbackVector(vector), vector);
  int32_t header_size = FeedbackVector::kFeedbackSlotsOffset - kHeapObjectTag;
  // Adding |header_size| with a separate IntPtrAdd rather than passing it
  // into ElementOffsetFromIndex() allows it to be folded into a single
  // [base, index, offset] indirect memory access on x64.
  Node* offset = ElementOffsetFromIndex(slot, HOLEY_ELEMENTS, SMI_PARAMETERS);
  Node* feedback = Load(MachineType::AnyTagged(), vector,
                        IntPtrAdd(offset, IntPtrConstant(header_size)));

  // Try to quickly handle the monomorphic case without knowing for sure
  // if we have a weak cell in feedback. We do know it's safe to look
  // at WeakCell::kValueOffset.
  GotoIf(WordNotEqual(receiver_map, LoadWeakCellValueUnchecked(feedback)),
         if_miss);

  Node* handler =
      Load(MachineType::AnyTagged(), vector,
           IntPtrAdd(offset, IntPtrConstant(header_size + kPointerSize)));

  var_handler->Bind(handler);
  Goto(if_handler);
  return feedback;
}

void AccessorAssembler::HandlePolymorphicCase(Node* receiver_map,
                                              Node* feedback, Label* if_handler,
                                              Variable* var_handler,
                                              Label* if_miss,
                                              int min_feedback_capacity) {
  Comment("HandlePolymorphicCase");
  DCHECK_EQ(MachineRepresentation::kTagged, var_handler->rep());

  // Deferred so the unrolled case can omit frame construction in bytecode
  // handler.
  Label loop(this, Label::kDeferred);

  // Iterate {feedback} array.
  const int kEntrySize = 2;

  // Loading feedback's length is delayed until we need it when looking past
  // the first {min_feedback_capacity} (map, handler) pairs.
  Node* length = nullptr;
  CSA_ASSERT(this, SmiGreaterThanOrEqual(
                       LoadFixedArrayBaseLength(feedback),
                       SmiConstant(min_feedback_capacity * kEntrySize)));

  const int kUnrolledIterations = IC::kMaxPolymorphicMapCount;
  for (int i = 0; i < kUnrolledIterations; i++) {
    int map_index = i * kEntrySize;
    int handler_index = i * kEntrySize + 1;

    if (i >= min_feedback_capacity) {
      if (length == nullptr) length = LoadFixedArrayBaseLength(feedback);
      GotoIf(SmiGreaterThanOrEqual(SmiConstant(handler_index), length),
             if_miss);
    }

    Label next_entry(this);
    Node* cached_map =
        LoadWeakCellValue(LoadFixedArrayElement(feedback, map_index));
    GotoIf(WordNotEqual(receiver_map, cached_map), &next_entry);

    // Found, now call handler.
    Node* handler = LoadFixedArrayElement(feedback, handler_index);
    var_handler->Bind(handler);
    Goto(if_handler);

    BIND(&next_entry);
  }
  Goto(&loop);

  // Loop from {kUnrolledIterations}*kEntrySize to {length}.
  BIND(&loop);
  Node* start_index = IntPtrConstant(kUnrolledIterations * kEntrySize);
  Node* end_index = LoadAndUntagFixedArrayBaseLength(feedback);
  BuildFastLoop(
      start_index, end_index,
      [this, receiver_map, feedback, if_handler, var_handler](Node* index) {
        Node* cached_map =
            LoadWeakCellValue(LoadFixedArrayElement(feedback, index));

        Label next_entry(this);
        GotoIf(WordNotEqual(receiver_map, cached_map), &next_entry);

        // Found, now call handler.
        Node* handler = LoadFixedArrayElement(feedback, index, kPointerSize);
        var_handler->Bind(handler);
        Goto(if_handler);

        BIND(&next_entry);
      },
      kEntrySize, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);
  // The loop falls through if no handler was found.
  Goto(if_miss);
}

void AccessorAssembler::HandleLoadICHandlerCase(
    const LoadICParameters* p, Node* handler, Label* miss,
    ExitPoint* exit_point, ElementSupport support_elements) {
  Comment("have_handler");

  VARIABLE(var_holder, MachineRepresentation::kTagged, p->receiver);
  VARIABLE(var_smi_handler, MachineRepresentation::kTagged, handler);

  Variable* vars[] = {&var_holder, &var_smi_handler};
  Label if_smi_handler(this, 2, vars);
  Label try_proto_handler(this, Label::kDeferred),
      call_handler(this, Label::kDeferred);

  Branch(TaggedIsSmi(handler), &if_smi_handler, &try_proto_handler);

  // |handler| is a Smi, encoding what to do. See SmiHandler methods
  // for the encoding format.
  BIND(&if_smi_handler);
  {
    HandleLoadICSmiHandlerCase(p, var_holder.value(), var_smi_handler.value(),
                               miss, exit_point, false, support_elements);
  }

  BIND(&try_proto_handler);
  {
    GotoIf(IsCodeMap(LoadMap(handler)), &call_handler);
    HandleLoadICProtoHandlerCase(p, handler, &var_holder, &var_smi_handler,
                                 &if_smi_handler, miss, exit_point, false);
  }

  BIND(&call_handler);
  {
    typedef LoadWithVectorDescriptor Descriptor;
    exit_point->ReturnCallStub(Descriptor(isolate()), handler, p->context,
                               p->receiver, p->name, p->slot, p->vector);
  }
}

void AccessorAssembler::HandleLoadField(Node* holder, Node* handler_word,
                                        Variable* var_double_value,
                                        Label* rebox_double,
                                        ExitPoint* exit_point) {
  Comment("field_load");
  Node* index = DecodeWord<LoadHandler::FieldIndexBits>(handler_word);
  Node* offset = IntPtrMul(index, IntPtrConstant(kPointerSize));

  Label inobject(this), out_of_object(this);
  Branch(IsSetWord<LoadHandler::IsInobjectBits>(handler_word), &inobject,
         &out_of_object);

  BIND(&inobject);
  {
    Label is_double(this);
    GotoIf(IsSetWord<LoadHandler::IsDoubleBits>(handler_word), &is_double);
    exit_point->Return(LoadObjectField(holder, offset));

    BIND(&is_double);
    if (FLAG_unbox_double_fields) {
      var_double_value->Bind(
          LoadObjectField(holder, offset, MachineType::Float64()));
    } else {
      Node* mutable_heap_number = LoadObjectField(holder, offset);
      var_double_value->Bind(LoadHeapNumberValue(mutable_heap_number));
    }
    Goto(rebox_double);
  }

  BIND(&out_of_object);
  {
    Label is_double(this);
    Node* properties = LoadFastProperties(holder);
    Node* value = LoadObjectField(properties, offset);
    GotoIf(IsSetWord<LoadHandler::IsDoubleBits>(handler_word), &is_double);
    exit_point->Return(value);

    BIND(&is_double);
    var_double_value->Bind(LoadHeapNumberValue(value));
    Goto(rebox_double);
  }
}

Node* AccessorAssembler::LoadDescriptorValue(Node* map, Node* descriptor) {
  Node* descriptors = LoadMapDescriptors(map);
  Node* scaled_descriptor =
      IntPtrMul(descriptor, IntPtrConstant(DescriptorArray::kEntrySize));
  Node* value_index = IntPtrAdd(
      scaled_descriptor, IntPtrConstant(DescriptorArray::kFirstIndex +
                                        DescriptorArray::kEntryValueIndex));
  CSA_ASSERT(this, UintPtrLessThan(descriptor, LoadAndUntagFixedArrayBaseLength(
                                                   descriptors)));
  return LoadFixedArrayElement(descriptors, value_index);
}

void AccessorAssembler::HandleLoadICSmiHandlerCase(
    const LoadICParameters* p, Node* holder, Node* smi_handler, Label* miss,
    ExitPoint* exit_point, bool throw_reference_error_if_nonexistent,
    ElementSupport support_elements) {
  VARIABLE(var_double_value, MachineRepresentation::kFloat64);
  Label rebox_double(this, &var_double_value);

  Node* handler_word = SmiUntag(smi_handler);
  Node* handler_kind = DecodeWord<LoadHandler::KindBits>(handler_word);
  if (support_elements == kSupportElements) {
    Label if_element(this), if_indexed_string(this), if_property(this);
    GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kElement)),
           &if_element);
    Branch(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kIndexedString)),
           &if_indexed_string, &if_property);

    BIND(&if_element);
    Comment("element_load");
    Node* intptr_index = TryToIntptr(p->name, miss);
    Node* elements = LoadElements(holder);
    Node* is_jsarray_condition =
        IsSetWord<LoadHandler::IsJsArrayBits>(handler_word);
    Node* elements_kind =
        DecodeWord32FromWord<LoadHandler::ElementsKindBits>(handler_word);
    Label if_hole(this), unimplemented_elements_kind(this),
        if_oob(this, Label::kDeferred);
    EmitElementLoad(holder, elements, elements_kind, intptr_index,
                    is_jsarray_condition, &if_hole, &rebox_double,
                    &var_double_value, &unimplemented_elements_kind, &if_oob,
                    miss, exit_point);

    BIND(&unimplemented_elements_kind);
    {
      // Smi handlers should only be installed for supported elements kinds.
      // Crash if we get here.
      DebugBreak();
      Goto(miss);
    }

    BIND(&if_oob);
    {
      Comment("out of bounds elements access");
      Label return_undefined(this);

      // Negative indices aren't valid array indices (according to
      // the ECMAScript specification), and are stored as properties
      // in V8, not elements. So we cannot handle them here.
      GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), miss);

      // Check if we're allowed to handle OOB accesses.
      Node* allow_out_of_bounds =
          IsSetWord<LoadHandler::AllowOutOfBoundsBits>(handler_word);
      GotoIfNot(allow_out_of_bounds, miss);

      // For typed arrays we never lookup elements in the prototype chain.
      GotoIf(IsJSTypedArray(holder), &return_undefined);

      // For all other receivers we need to check that the prototype chain
      // doesn't contain any elements.
      BranchIfPrototypesHaveNoElements(LoadMap(holder), &return_undefined,
                                       miss);

      BIND(&return_undefined);
      exit_point->Return(UndefinedConstant());
    }

    BIND(&if_hole);
    {
      Comment("convert hole");
      GotoIfNot(IsSetWord<LoadHandler::ConvertHoleBits>(handler_word), miss);
      GotoIf(IsNoElementsProtectorCellInvalid(), miss);
      exit_point->Return(UndefinedConstant());
    }

    BIND(&if_indexed_string);
    {
      Label if_oob(this, Label::kDeferred);

      Comment("indexed string");
      Node* intptr_index = TryToIntptr(p->name, miss);
      Node* length = LoadStringLengthAsWord(holder);
      GotoIf(UintPtrGreaterThanOrEqual(intptr_index, length), &if_oob);
      Node* code = StringCharCodeAt(holder, intptr_index);
      Node* result = StringFromCharCode(code);
      Return(result);

      BIND(&if_oob);
      Node* allow_out_of_bounds =
          IsSetWord<LoadHandler::AllowOutOfBoundsBits>(handler_word);
      GotoIfNot(allow_out_of_bounds, miss);
      GotoIf(IsNoElementsProtectorCellInvalid(), miss);
      Return(UndefinedConstant());
    }

    BIND(&if_property);
    Comment("property_load");
  }

  Label constant(this), field(this), normal(this, Label::kDeferred),
      interceptor(this, Label::kDeferred), nonexistent(this),
      accessor(this, Label::kDeferred), global(this, Label::kDeferred),
      module_export(this, Label::kDeferred), proxy(this, Label::kDeferred),
      native_data_property(this), api_getter(this);
  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kField)), &field);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kConstant)),
         &constant);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kNonExistent)),
         &nonexistent);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kNormal)),
         &normal);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kAccessor)),
         &accessor);

  GotoIf(
      WordEqual(handler_kind, IntPtrConstant(LoadHandler::kNativeDataProperty)),
      &native_data_property);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kApiGetter)),
         &api_getter);

  GotoIf(WordEqual(handler_kind,
                   IntPtrConstant(LoadHandler::kApiGetterHolderIsPrototype)),
         &api_getter);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kGlobal)),
         &global);

  GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kProxy)), &proxy);

  Branch(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kModuleExport)),
         &module_export, &interceptor);

  BIND(&field);
  HandleLoadField(holder, handler_word, &var_double_value, &rebox_double,
                  exit_point);

  BIND(&nonexistent);
  // This is a handler for a load of a non-existent value.
  if (throw_reference_error_if_nonexistent) {
    exit_point->ReturnCallRuntime(Runtime::kThrowReferenceError, p->context,
                                  p->name);
  } else {
    exit_point->Return(UndefinedConstant());
  }

  BIND(&constant);
  {
    Comment("constant_load");
    Node* descriptor = DecodeWord<LoadHandler::DescriptorBits>(handler_word);
    Node* value = LoadDescriptorValue(LoadMap(holder), descriptor);

    exit_point->Return(value);
  }

  BIND(&normal);
  {
    Comment("load_normal");
    Node* properties = LoadSlowProperties(holder);
    VARIABLE(var_name_index, MachineType::PointerRepresentation());
    Label found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, p->name, &found,
                                         &var_name_index, miss);
    BIND(&found);
    {
      VARIABLE(var_details, MachineRepresentation::kWord32);
      VARIABLE(var_value, MachineRepresentation::kTagged);
      LoadPropertyFromNameDictionary(properties, var_name_index.value(),
                                     &var_details, &var_value);
      Node* value = CallGetterIfAccessor(var_value.value(), var_details.value(),
                                         p->context, p->receiver, miss);
      exit_point->Return(value);
    }
  }

  BIND(&accessor);
  {
    Comment("accessor_load");
    Node* descriptor = DecodeWord<LoadHandler::DescriptorBits>(handler_word);
    Node* accessor_pair = LoadDescriptorValue(LoadMap(holder), descriptor);
    CSA_ASSERT(this, IsAccessorPair(accessor_pair));
    Node* getter = LoadObjectField(accessor_pair, AccessorPair::kGetterOffset);
    CSA_ASSERT(this, Word32BinaryNot(IsTheHole(getter)));

    Callable callable = CodeFactory::Call(isolate());
    exit_point->Return(CallJS(callable, p->context, getter, p->receiver));
  }

  BIND(&native_data_property);
  {
    Comment("native_data_property_load");
    Node* descriptor = DecodeWord<LoadHandler::DescriptorBits>(handler_word);
    Node* accessor_info = LoadDescriptorValue(LoadMap(holder), descriptor);

    Callable callable = CodeFactory::ApiGetter(isolate());
    exit_point->ReturnCallStub(callable, p->context, p->receiver, holder,
                               accessor_info);
  }

  BIND(&api_getter);
  {
    Comment("api_getter");
    Node* context = LoadWeakCellValueUnchecked(
        LoadObjectField(holder, Tuple2::kValue1Offset));
    Node* call_handler_info = LoadWeakCellValueUnchecked(
        LoadObjectField(holder, Tuple2::kValue2Offset));

    Node* foreign =
        LoadObjectField(call_handler_info, CallHandlerInfo::kJsCallbackOffset);
    Node* callback = LoadObjectField(foreign, Foreign::kForeignAddressOffset,
                                     MachineType::Pointer());
    Node* data =
        LoadObjectField(call_handler_info, CallHandlerInfo::kDataOffset);

    VARIABLE(api_holder, MachineRepresentation::kTagged, p->receiver);
    Label load(this);
    GotoIf(WordEqual(handler_kind, IntPtrConstant(LoadHandler::kApiGetter)),
           &load);

    CSA_ASSERT(
        this,
        WordEqual(handler_kind,
                  IntPtrConstant(LoadHandler::kApiGetterHolderIsPrototype)));

    api_holder.Bind(LoadMapPrototype(LoadMap(p->receiver)));
    Goto(&load);

    BIND(&load);
    Callable callable = CodeFactory::CallApiCallback(isolate(), 0);
    exit_point->Return(CallStub(callable, nullptr, context, data,
                                api_holder.value(), callback, p->receiver));
  }

  BIND(&proxy);
  {
    VARIABLE(var_index, MachineType::PointerRepresentation());
    VARIABLE(var_unique, MachineRepresentation::kTagged);

    Label if_index(this), if_unique_name(this),
        to_name_failed(this, Label::kDeferred);

    if (support_elements == kSupportElements) {
      TryToName(p->name, &if_index, &var_index, &if_unique_name, &var_unique,
                &to_name_failed);

      BIND(&if_unique_name);
      exit_point->ReturnCallStub(
          Builtins::CallableFor(isolate(), Builtins::kProxyGetProperty),
          p->context, holder, var_unique.value(), p->receiver);

      BIND(&if_index);
      // TODO(mslekova): introduce TryToName that doesn't try to compute
      // the intptr index value
      Goto(&to_name_failed);

      BIND(&to_name_failed);
      exit_point->ReturnCallRuntime(Runtime::kGetPropertyWithReceiver,
                                    p->context, holder, p->name, p->receiver);

    } else {
      exit_point->ReturnCallStub(
          Builtins::CallableFor(isolate(), Builtins::kProxyGetProperty),
          p->context, holder, p->name, p->receiver);
    }
  }

  BIND(&global);
  {
    CSA_ASSERT(this, IsPropertyCell(holder));
    // Ensure the property cell doesn't contain the hole.
    Node* value = LoadObjectField(holder, PropertyCell::kValueOffset);
    Node* details =
        LoadAndUntagToWord32ObjectField(holder, PropertyCell::kDetailsOffset);
    GotoIf(IsTheHole(value), miss);

    exit_point->Return(
        CallGetterIfAccessor(value, details, p->context, p->receiver, miss));
  }

  BIND(&interceptor);
  {
    Comment("load_interceptor");
    exit_point->ReturnCallRuntime(Runtime::kLoadPropertyWithInterceptor,
                                  p->context, p->name, p->receiver, holder,
                                  p->slot, p->vector);
  }

  BIND(&module_export);
  {
    Comment("module export");
    Node* index = DecodeWord<LoadHandler::ExportsIndexBits>(handler_word);
    Node* module =
        LoadObjectField(p->receiver, JSModuleNamespace::kModuleOffset,
                        MachineType::TaggedPointer());
    Node* exports = LoadObjectField(module, Module::kExportsOffset,
                                    MachineType::TaggedPointer());
    Node* cell = LoadFixedArrayElement(exports, index);
    // The handler is only installed for exports that exist.
    CSA_ASSERT(this, IsCell(cell));
    Node* value = LoadCellValue(cell);
    Label is_the_hole(this, Label::kDeferred);
    GotoIf(IsTheHole(value), &is_the_hole);
    exit_point->Return(value);

    BIND(&is_the_hole);
    {
      Node* message = SmiConstant(MessageTemplate::kNotDefined);
      exit_point->ReturnCallRuntime(Runtime::kThrowReferenceError, p->context,
                                    message, p->name);
    }
  }

  BIND(&rebox_double);
  exit_point->Return(AllocateHeapNumberWithValue(var_double_value.value()));
}

void AccessorAssembler::HandleLoadICProtoHandlerCase(
    const LoadICParameters* p, Node* handler, Variable* var_holder,
    Variable* var_smi_handler, Label* if_smi_handler, Label* miss,
    ExitPoint* exit_point, bool throw_reference_error_if_nonexistent) {
  DCHECK_EQ(MachineRepresentation::kTagged, var_holder->rep());
  DCHECK_EQ(MachineRepresentation::kTagged, var_smi_handler->rep());

  // IC dispatchers rely on these assumptions to be held.
  STATIC_ASSERT(FixedArray::kLengthOffset == LoadHandler::kDataOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(LoadHandler::kSmiHandlerIndex),
            LoadHandler::kSmiHandlerOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(LoadHandler::kValidityCellIndex),
            LoadHandler::kValidityCellOffset);

  // Both FixedArray and Tuple3 handlers have validity cell at the same offset.
  Label validity_cell_check_done(this);
  Node* validity_cell =
      LoadObjectField(handler, LoadHandler::kValidityCellOffset);
  GotoIf(WordEqual(validity_cell, IntPtrConstant(0)),
         &validity_cell_check_done);
  Node* cell_value = LoadObjectField(validity_cell, Cell::kValueOffset);
  GotoIf(WordNotEqual(cell_value, SmiConstant(Map::kPrototypeChainValid)),
         miss);
  Goto(&validity_cell_check_done);

  BIND(&validity_cell_check_done);
  Node* smi_handler = LoadObjectField(handler, LoadHandler::kSmiHandlerOffset);
  CSA_ASSERT(this, TaggedIsSmi(smi_handler));
  Node* handler_flags = SmiUntag(smi_handler);

  Label check_prototypes(this);
  GotoIfNot(IsSetWord<LoadHandler::LookupOnReceiverBits>(handler_flags),
            &check_prototypes);
  {
    CSA_ASSERT(this, Word32BinaryNot(
                         HasInstanceType(p->receiver, JS_GLOBAL_OBJECT_TYPE)));
    Node* properties = LoadSlowProperties(p->receiver);
    VARIABLE(var_name_index, MachineType::PointerRepresentation());
    Label found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, p->name, &found,
                                         &var_name_index, &check_prototypes);
    BIND(&found);
    {
      VARIABLE(var_details, MachineRepresentation::kWord32);
      VARIABLE(var_value, MachineRepresentation::kTagged);
      LoadPropertyFromNameDictionary(properties, var_name_index.value(),
                                     &var_details, &var_value);
      Node* value = CallGetterIfAccessor(var_value.value(), var_details.value(),
                                         p->context, p->receiver, miss);
      exit_point->Return(value);
    }
  }

  BIND(&check_prototypes);
  Node* maybe_holder_cell = LoadObjectField(handler, LoadHandler::kDataOffset);
  Label array_handler(this), tuple_handler(this);
  Branch(TaggedIsSmi(maybe_holder_cell), &array_handler, &tuple_handler);

  BIND(&tuple_handler);
  {
    Label load_from_cached_holder(this), done(this);

    Branch(IsNull(maybe_holder_cell), &done, &load_from_cached_holder);

    BIND(&load_from_cached_holder);
    {
      Label unwrap_cell(this), bind_holder(this);
      Branch(IsWeakCell(maybe_holder_cell), &unwrap_cell, &bind_holder);

      BIND(&unwrap_cell);
      {
        // For regular holders, having passed the receiver map check and the
        // validity cell check implies that |holder| is alive. However, for
        // global object receivers, the |maybe_holder_cell| may be cleared.
        Node* holder = LoadWeakCellValue(maybe_holder_cell, miss);

        var_holder->Bind(holder);
        Goto(&done);
      }

      BIND(&bind_holder);
      {
        var_holder->Bind(maybe_holder_cell);
        Goto(&done);
      }
    }

    BIND(&done);
    var_smi_handler->Bind(smi_handler);
    Goto(if_smi_handler);
  }

  BIND(&array_handler);
  {
    exit_point->ReturnCallStub(
        CodeFactory::LoadICProtoArray(isolate(),
                                      throw_reference_error_if_nonexistent),
        p->context, p->receiver, p->name, p->slot, p->vector, handler);
  }
}

void AccessorAssembler::EmitAccessCheck(Node* expected_native_context,
                                        Node* context, Node* receiver,
                                        Label* can_access, Label* miss) {
  CSA_ASSERT(this, IsNativeContext(expected_native_context));

  Node* native_context = LoadNativeContext(context);
  GotoIf(WordEqual(expected_native_context, native_context), can_access);
  // If the receiver is not a JSGlobalProxy then we miss.
  GotoIfNot(IsJSGlobalProxy(receiver), miss);
  // For JSGlobalProxy receiver try to compare security tokens of current
  // and expected native contexts.
  Node* expected_token = LoadContextElement(expected_native_context,
                                            Context::SECURITY_TOKEN_INDEX);
  Node* current_token =
      LoadContextElement(native_context, Context::SECURITY_TOKEN_INDEX);
  Branch(WordEqual(expected_token, current_token), can_access, miss);
}

Node* AccessorAssembler::EmitLoadICProtoArrayCheck(const LoadICParameters* p,
                                                   Node* handler,
                                                   Node* handler_length,
                                                   Node* handler_flags,
                                                   Label* miss) {
  VARIABLE(var_start_index, MachineType::PointerRepresentation(),
           IntPtrConstant(LoadHandler::kFirstPrototypeIndex));

  Label can_access(this);
  GotoIfNot(IsSetWord<LoadHandler::DoAccessCheckOnReceiverBits>(handler_flags),
            &can_access);
  {
    // Skip this entry of a handler.
    var_start_index.Bind(IntPtrConstant(LoadHandler::kFirstPrototypeIndex + 1));

    int offset =
        FixedArray::OffsetOfElementAt(LoadHandler::kFirstPrototypeIndex);
    Node* expected_native_context =
        LoadWeakCellValue(LoadObjectField(handler, offset), miss);

    EmitAccessCheck(expected_native_context, p->context, p->receiver,
                    &can_access, miss);
  }
  BIND(&can_access);

  BuildFastLoop(var_start_index.value(), handler_length,
                [=](Node* current) {
                  Node* prototype_cell =
                      LoadFixedArrayElement(handler, current);
                  CheckPrototype(prototype_cell, p->name, miss);
                },
                1, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);

  Node* maybe_holder_cell =
      LoadFixedArrayElement(handler, LoadHandler::kDataIndex);

  VARIABLE(var_holder, MachineRepresentation::kTagged, p->receiver);
  Label done(this);
  GotoIf(IsNull(maybe_holder_cell), &done);

  {
    Label unwrap_cell(this), bind_holder(this);
    Branch(IsWeakCell(maybe_holder_cell), &unwrap_cell, &bind_holder);

    BIND(&unwrap_cell);
    {
      // For regular holders, having passed the receiver map check and the
      // validity cell check implies that |holder| is alive. However, for
      // global object receivers, the |maybe_holder_cell| may be cleared.
      Node* holder = LoadWeakCellValue(maybe_holder_cell, miss);

      var_holder.Bind(holder);
      Goto(&done);
    }

    BIND(&bind_holder);
    {
      var_holder.Bind(maybe_holder_cell);
      Goto(&done);
    }
  }

  BIND(&done);
  return var_holder.value();
}

void AccessorAssembler::HandleLoadGlobalICHandlerCase(
    const LoadICParameters* pp, Node* handler, Label* miss,
    ExitPoint* exit_point, bool throw_reference_error_if_nonexistent) {
  LoadICParameters p = *pp;
  DCHECK_NULL(p.receiver);
  Node* native_context = LoadNativeContext(p.context);
  p.receiver = LoadContextElement(native_context, Context::GLOBAL_PROXY_INDEX);

  VARIABLE(var_holder, MachineRepresentation::kTagged,
           LoadContextElement(native_context, Context::EXTENSION_INDEX));
  VARIABLE(var_smi_handler, MachineRepresentation::kTagged);
  Label if_smi_handler(this);

  HandleLoadICProtoHandlerCase(&p, handler, &var_holder, &var_smi_handler,
                               &if_smi_handler, miss, exit_point,
                               throw_reference_error_if_nonexistent);
  BIND(&if_smi_handler);
  HandleLoadICSmiHandlerCase(
      &p, var_holder.value(), var_smi_handler.value(), miss, exit_point,
      throw_reference_error_if_nonexistent, kOnlyProperties);
}

void AccessorAssembler::JumpIfDataProperty(Node* details, Label* writable,
                                           Label* readonly) {
  // Accessor properties never have the READ_ONLY attribute set.
  GotoIf(IsSetWord32(details, PropertyDetails::kAttributesReadOnlyMask),
         readonly);
  Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
  GotoIf(Word32Equal(kind, Int32Constant(kData)), writable);
  // Fall through if it's an accessor property.
}

void AccessorAssembler::HandleStoreICNativeDataProperty(
    const StoreICParameters* p, Node* holder, Node* handler_word) {
  Node* descriptor = DecodeWord<StoreHandler::DescriptorBits>(handler_word);
  Node* accessor_info = LoadDescriptorValue(LoadMap(holder), descriptor);
  CSA_CHECK(this, IsAccessorInfo(accessor_info));

  Node* language_mode = GetLanguageMode(p->vector, p->slot);

  TailCallRuntime(Runtime::kStoreCallbackProperty, p->context, p->receiver,
                  holder, accessor_info, p->name, p->value, language_mode);
}

void AccessorAssembler::HandleStoreICHandlerCase(
    const StoreICParameters* p, Node* handler, Label* miss,
    ElementSupport support_elements) {
  Label if_smi_handler(this), if_nonsmi_handler(this);
  Label if_proto_handler(this), if_element_handler(this), call_handler(this),
      store_global(this);

  Branch(TaggedIsSmi(handler), &if_smi_handler, &if_nonsmi_handler);

  // |handler| is a Smi, encoding what to do. See SmiHandler methods
  // for the encoding format.
  BIND(&if_smi_handler);
  {
    Node* holder = p->receiver;
    Node* handler_word = SmiUntag(handler);

    Label if_fast_smi(this), if_proxy(this);

    STATIC_ASSERT(StoreHandler::kGlobalProxy + 1 == StoreHandler::kNormal);
    STATIC_ASSERT(StoreHandler::kNormal + 1 == StoreHandler::kProxy);
    STATIC_ASSERT(StoreHandler::kProxy + 1 == StoreHandler::kKindsNumber);

    Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);
    GotoIf(IntPtrLessThan(handler_kind,
                          IntPtrConstant(StoreHandler::kGlobalProxy)),
           &if_fast_smi);
    GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kProxy)),
           &if_proxy);
    CSA_ASSERT(this,
               WordEqual(handler_kind, IntPtrConstant(StoreHandler::kNormal)));
    Node* properties = LoadSlowProperties(holder);

    VARIABLE(var_name_index, MachineType::PointerRepresentation());
    Label dictionary_found(this, &var_name_index);
    NameDictionaryLookup<NameDictionary>(properties, p->name, &dictionary_found,
                                         &var_name_index, miss);
    BIND(&dictionary_found);
    {
      Node* details = LoadDetailsByKeyIndex<NameDictionary>(
          properties, var_name_index.value());
      // Check that the property is a writable data property (no accessor).
      const int kTypeAndReadOnlyMask = PropertyDetails::KindField::kMask |
                                       PropertyDetails::kAttributesReadOnlyMask;
      STATIC_ASSERT(kData == 0);
      GotoIf(IsSetWord32(details, kTypeAndReadOnlyMask), miss);

      StoreValueByKeyIndex<NameDictionary>(properties, var_name_index.value(),
                                           p->value);
      Return(p->value);
    }

    BIND(&if_fast_smi);
    {
      Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);

      Label data(this), accessor(this), native_data_property(this);
      GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kAccessor)),
             &accessor);
      Branch(WordEqual(handler_kind,
                       IntPtrConstant(StoreHandler::kNativeDataProperty)),
             &native_data_property, &data);

      BIND(&accessor);
      HandleStoreAccessor(p, holder, handler_word);

      BIND(&native_data_property);
      HandleStoreICNativeDataProperty(p, holder, handler_word);

      BIND(&data);
      // Handle non-transitioning field stores.
      HandleStoreICSmiHandlerCase(handler_word, holder, p->value, nullptr,
                                  miss);
    }

    BIND(&if_proxy);
    HandleStoreToProxy(p, holder, miss, support_elements);
  }

  BIND(&if_nonsmi_handler);
  {
    Node* handler_map = LoadMap(handler);
    if (support_elements == kSupportElements) {
      GotoIf(IsTuple2Map(handler_map), &if_element_handler);
    }
    GotoIf(IsWeakCellMap(handler_map), &store_global);
    Branch(IsCodeMap(handler_map), &call_handler, &if_proto_handler);
  }

  if (support_elements == kSupportElements) {
    BIND(&if_element_handler);
    HandleStoreICElementHandlerCase(p, handler, miss);
  }

  BIND(&if_proto_handler);
  HandleStoreICProtoHandler(p, handler, miss, support_elements);

  // |handler| is a heap object. Must be code, call it.
  BIND(&call_handler);
  {
    StoreWithVectorDescriptor descriptor(isolate());
    TailCallStub(descriptor, handler, p->context, p->receiver, p->name,
                 p->value, p->slot, p->vector);
  }

  BIND(&store_global);
  {
    // Load value or miss if the {handler} weak cell is cleared.
    Node* cell = LoadWeakCellValue(handler, miss);

    ExitPoint direct_exit(this);
    StoreGlobalIC_PropertyCellCase(cell, p->value, &direct_exit, miss);
  }
}

void AccessorAssembler::HandleStoreICElementHandlerCase(
    const StoreICParameters* p, Node* handler, Label* miss) {
  Comment("HandleStoreICElementHandlerCase");
  Node* validity_cell = LoadObjectField(handler, Tuple2::kValue1Offset);
  Node* cell_value = LoadObjectField(validity_cell, Cell::kValueOffset);
  GotoIf(WordNotEqual(cell_value, SmiConstant(Map::kPrototypeChainValid)),
         miss);

  Node* code_handler = LoadObjectField(handler, Tuple2::kValue2Offset);
  CSA_ASSERT(this, IsCodeMap(LoadMap(code_handler)));

  StoreWithVectorDescriptor descriptor(isolate());
  TailCallStub(descriptor, code_handler, p->context, p->receiver, p->name,
               p->value, p->slot, p->vector);
}

void AccessorAssembler::HandleStoreAccessor(const StoreICParameters* p,
                                            Node* holder, Node* handler_word) {
  Comment("accessor_store");
  Node* descriptor = DecodeWord<StoreHandler::DescriptorBits>(handler_word);
  Node* accessor_pair = LoadDescriptorValue(LoadMap(holder), descriptor);
  CSA_ASSERT(this, IsAccessorPair(accessor_pair));
  Node* setter = LoadObjectField(accessor_pair, AccessorPair::kSetterOffset);
  CSA_ASSERT(this, Word32BinaryNot(IsTheHole(setter)));

  Callable callable = CodeFactory::Call(isolate());
  Return(CallJS(callable, p->context, setter, p->receiver, p->value));
}

void AccessorAssembler::HandleStoreICProtoHandler(
    const StoreICParameters* p, Node* handler, Label* miss,
    ElementSupport support_elements) {
  Comment("HandleStoreICProtoHandler");

  // IC dispatchers rely on these assumptions to be held.
  STATIC_ASSERT(FixedArray::kLengthOffset == StoreHandler::kDataOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(StoreHandler::kSmiHandlerIndex),
            StoreHandler::kSmiHandlerOffset);
  DCHECK_EQ(FixedArray::OffsetOfElementAt(StoreHandler::kValidityCellIndex),
            StoreHandler::kValidityCellOffset);

  // Both FixedArray and Tuple3 handlers have validity cell at the same offset.
  Label validity_cell_check_done(this);
  Node* validity_cell =
      LoadObjectField(handler, StoreHandler::kValidityCellOffset);
  GotoIf(WordEqual(validity_cell, IntPtrConstant(0)),
         &validity_cell_check_done);
  Node* cell_value = LoadObjectField(validity_cell, Cell::kValueOffset);
  GotoIf(WordNotEqual(cell_value, SmiConstant(Map::kPrototypeChainValid)),
         miss);
  Goto(&validity_cell_check_done);

  BIND(&validity_cell_check_done);
  Node* smi_or_code = LoadObjectField(handler, StoreHandler::kSmiHandlerOffset);

  Node* maybe_transition_cell =
      LoadObjectField(handler, StoreHandler::kDataOffset);
  Label array_handler(this), do_store(this);

  VARIABLE(var_transition_map_or_holder, MachineRepresentation::kTagged,
           maybe_transition_cell);

  Branch(TaggedIsSmi(maybe_transition_cell), &array_handler, &do_store);

  BIND(&array_handler);
  {
    VARIABLE(var_start_index, MachineType::PointerRepresentation(),
             IntPtrConstant(StoreHandler::kFirstPrototypeIndex));

    Comment("array_handler");
    Label can_access(this);
    // Only Tuple3 handlers are allowed to have code handlers.
    CSA_ASSERT(this, TaggedIsSmi(smi_or_code));
    GotoIfNot(
        IsSetSmi(smi_or_code, StoreHandler::DoAccessCheckOnReceiverBits::kMask),
        &can_access);

    {
      // Skip this entry of a handler.
      var_start_index.Bind(
          IntPtrConstant(StoreHandler::kFirstPrototypeIndex + 1));

      int offset =
          FixedArray::OffsetOfElementAt(StoreHandler::kFirstPrototypeIndex);
      Node* expected_native_context =
          LoadWeakCellValue(LoadObjectField(handler, offset), miss);

      EmitAccessCheck(expected_native_context, p->context, p->receiver,
                      &can_access, miss);
    }
    BIND(&can_access);

    Node* length = SmiUntag(maybe_transition_cell);
    BuildFastLoop(var_start_index.value(), length,
                  [=](Node* current) {
                    Node* prototype_cell =
                        LoadFixedArrayElement(handler, current);
                    CheckPrototype(prototype_cell, p->name, miss);
                  },
                  1, INTPTR_PARAMETERS, IndexAdvanceMode::kPost);

    Node* maybe_transition_cell =
        LoadFixedArrayElement(handler, StoreHandler::kDataIndex);
    var_transition_map_or_holder.Bind(maybe_transition_cell);
    Goto(&do_store);
  }

  Label if_transition_map(this), if_holder_object(this);

  BIND(&do_store);
  {
    Node* maybe_transition_cell = var_transition_map_or_holder.value();

    Label unwrap_cell(this);
    Branch(IsWeakCell(maybe_transition_cell), &unwrap_cell, &if_holder_object);

    BIND(&unwrap_cell);
    {
      Node* maybe_transition = LoadWeakCellValue(maybe_transition_cell, miss);
      var_transition_map_or_holder.Bind(maybe_transition);
      Branch(IsMap(maybe_transition), &if_transition_map, &if_holder_object);
    }
  }

  BIND(&if_transition_map);
  {
    Label if_transition_to_constant(this), if_store_normal(this);

    Node* holder = p->receiver;
    Node* transition_map = var_transition_map_or_holder.value();

    GotoIf(IsDeprecatedMap(transition_map), miss);

    if (support_elements == kSupportElements) {
      Label if_smi_handler(this);

      GotoIf(TaggedIsSmi(smi_or_code), &if_smi_handler);
      Node* code_handler = smi_or_code;
      CSA_ASSERT(this, IsCodeMap(LoadMap(code_handler)));

      StoreTransitionDescriptor descriptor(isolate());
      TailCallStub(descriptor, code_handler, p->context, p->receiver, p->name,
                   transition_map, p->value, p->slot, p->vector);

      BIND(&if_smi_handler);
    }

    Node* smi_handler = smi_or_code;
    CSA_ASSERT(this, TaggedIsSmi(smi_handler));
    Node* handler_word = SmiUntag(smi_handler);

    Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);
    GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kNormal)),
           &if_store_normal);
    GotoIf(WordEqual(handler_kind,
                     IntPtrConstant(StoreHandler::kTransitionToConstant)),
           &if_transition_to_constant);

    CSA_ASSERT(this,
               WordEqual(handler_kind,
                         IntPtrConstant(StoreHandler::kTransitionToField)));

    // Handle transitioning field stores.
    HandleStoreICSmiHandlerCase(handler_word, holder, p->value, transition_map,
                                miss);

    BIND(&if_transition_to_constant);
    {
      // Check that constant matches value.
      Node* descriptor = DecodeWord<StoreHandler::DescriptorBits>(handler_word);
      Node* constant = LoadDescriptorValue(transition_map, descriptor);
      GotoIf(WordNotEqual(p->value, constant), miss);

      StoreMap(p->receiver, transition_map);
      Return(p->value);
    }

    BIND(&if_store_normal);
    {
      Node* properties = LoadSlowProperties(p->receiver);

      VARIABLE(var_name_index, MachineType::PointerRepresentation());
      Label found(this, &var_name_index), not_found(this);
      NameDictionaryLookup<NameDictionary>(properties, p->name, &found,
                                           &var_name_index, &not_found);
      BIND(&found);
      {
        Node* details = LoadDetailsByKeyIndex<NameDictionary>(
            properties, var_name_index.value());
        // Check that the property is a writable data property (no accessor).
        const int kTypeAndReadOnlyMask =
            PropertyDetails::KindField::kMask |
            PropertyDetails::kAttributesReadOnlyMask;
        STATIC_ASSERT(kData == 0);
        GotoIf(IsSetWord32(details, kTypeAndReadOnlyMask), miss);

        StoreValueByKeyIndex<NameDictionary>(properties, var_name_index.value(),
                                             p->value);
        Return(p->value);
      }

      BIND(&not_found);
      {
        Label slow(this);
        Add<NameDictionary>(properties, p->name, p->value, &slow);
        Return(p->value);

        BIND(&slow);
        TailCallRuntime(Runtime::kAddDictionaryProperty, p->context,
                        p->receiver, p->name, p->value);
      }
    }
  }
  BIND(&if_holder_object);
  {
    Label if_store_global_proxy(this), if_api_setter(this), if_accessor(this),
        if_native_data_property(this);
    Node* holder = var_transition_map_or_holder.value();

    Node* smi_handler = smi_or_code;
    CSA_ASSERT(this, TaggedIsSmi(smi_handler));
    Node* handler_word = SmiUntag(smi_handler);

    Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);
    GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kGlobalProxy)),
           &if_store_global_proxy);

    GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kAccessor)),
           &if_accessor);

    GotoIf(WordEqual(handler_kind,
                     IntPtrConstant(StoreHandler::kNativeDataProperty)),
           &if_native_data_property);

    GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kApiSetter)),
           &if_api_setter);

    GotoIf(WordEqual(handler_kind,
                     IntPtrConstant(StoreHandler::kApiSetterHolderIsPrototype)),
           &if_api_setter);

    CSA_ASSERT(this,
               WordEqual(handler_kind, IntPtrConstant(StoreHandler::kProxy)));
    HandleStoreToProxy(p, holder, miss, support_elements);

    BIND(&if_accessor);
    HandleStoreAccessor(p, holder, handler_word);

    BIND(&if_native_data_property);
    HandleStoreICNativeDataProperty(p, holder, handler_word);

    BIND(&if_api_setter);
    {
      Comment("api_setter");
      Node* context = LoadWeakCellValueUnchecked(
          LoadObjectField(holder, Tuple2::kValue1Offset));
      Node* call_handler_info = LoadWeakCellValueUnchecked(
          LoadObjectField(holder, Tuple2::kValue2Offset));

      Node* foreign = LoadObjectField(call_handler_info,
                                      CallHandlerInfo::kJsCallbackOffset);
      Node* callback = LoadObjectField(foreign, Foreign::kForeignAddressOffset,
                                       MachineType::Pointer());
      Node* data =
          LoadObjectField(call_handler_info, CallHandlerInfo::kDataOffset);

      VARIABLE(api_holder, MachineRepresentation::kTagged, p->receiver);
      Label store(this);
      GotoIf(WordEqual(handler_kind, IntPtrConstant(StoreHandler::kApiSetter)),
             &store);

      CSA_ASSERT(
          this,
          WordEqual(handler_kind,
                    IntPtrConstant(StoreHandler::kApiSetterHolderIsPrototype)));

      api_holder.Bind(LoadMapPrototype(LoadMap(p->receiver)));
      Goto(&store);

      BIND(&store);
      Callable callable = CodeFactory::CallApiCallback(isolate(), 1);
      Return(CallStub(callable, nullptr, context, data, api_holder.value(),
                      callback, p->receiver, p->value));
    }

    BIND(&if_store_global_proxy);
    {
      ExitPoint direct_exit(this);
      StoreGlobalIC_PropertyCellCase(holder, p->value, &direct_exit, miss);
    }
  }
}

Node* AccessorAssembler::GetLanguageMode(Node* vector, Node* slot) {
  VARIABLE(var_language_mode, MachineRepresentation::kTaggedSigned,
           SmiConstant(LanguageMode::kStrict));
  Label language_mode_determined(this);
  BranchIfStrictMode(vector, slot, &language_mode_determined);
  var_language_mode.Bind(SmiConstant(LanguageMode::kSloppy));
  Goto(&language_mode_determined);
  BIND(&language_mode_determined);
  return var_language_mode.value();
}

void AccessorAssembler::HandleStoreToProxy(const StoreICParameters* p,
                                           Node* proxy, Label* miss,
                                           ElementSupport support_elements) {
  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged);

  Label if_index(this), if_unique_name(this),
      to_name_failed(this, Label::kDeferred);

  Node* language_mode = GetLanguageMode(p->vector, p->slot);

  if (support_elements == kSupportElements) {
    TryToName(p->name, &if_index, &var_index, &if_unique_name, &var_unique,
              &to_name_failed);

    BIND(&if_unique_name);
    CallBuiltin(Builtins::kProxySetProperty, p->context, proxy,
                var_unique.value(), p->value, p->receiver, language_mode);
    Return(p->value);

    // The index case is handled earlier by the runtime.
    BIND(&if_index);
    // TODO(mslekova): introduce TryToName that doesn't try to compute
    // the intptr index value
    Goto(&to_name_failed);

    BIND(&to_name_failed);
    TailCallRuntime(Runtime::kSetPropertyWithReceiver, p->context, proxy,
                    p->name, p->value, p->receiver, language_mode);
  } else {
    Node* name = ToName(p->context, p->name);
    TailCallBuiltin(Builtins::kProxySetProperty, p->context, proxy, name,
                    p->value, p->receiver, language_mode);
  }
}

void AccessorAssembler::HandleStoreICSmiHandlerCase(Node* handler_word,
                                                    Node* holder, Node* value,
                                                    Node* transition,
                                                    Label* miss) {
  Comment(transition ? "transitioning field store" : "field store");
#ifdef DEBUG
  Node* handler_kind = DecodeWord<StoreHandler::KindBits>(handler_word);
  if (transition) {
    CSA_ASSERT(
        this,
        Word32Or(
            WordEqual(handler_kind,
                      IntPtrConstant(StoreHandler::kTransitionToField)),
            WordEqual(handler_kind,
                      IntPtrConstant(StoreHandler::kTransitionToConstant))));
  } else {
    if (FLAG_track_constant_fields) {
      CSA_ASSERT(
          this, Word32Or(WordEqual(handler_kind,
                                   IntPtrConstant(StoreHandler::kField)),
                         WordEqual(handler_kind,
                                   IntPtrConstant(StoreHandler::kConstField))));
    } else {
      CSA_ASSERT(this,
                 WordEqual(handler_kind, IntPtrConstant(StoreHandler::kField)));
    }
  }
#endif

  Node* field_representation =
      DecodeWord<StoreHandler::FieldRepresentationBits>(handler_word);

  Label if_smi_field(this), if_double_field(this), if_heap_object_field(this),
      if_tagged_field(this);

  GotoIf(WordEqual(field_representation, IntPtrConstant(StoreHandler::kTagged)),
         &if_tagged_field);
  GotoIf(WordEqual(field_representation,
                   IntPtrConstant(StoreHandler::kHeapObject)),
         &if_heap_object_field);
  GotoIf(WordEqual(field_representation, IntPtrConstant(StoreHandler::kDouble)),
         &if_double_field);
  CSA_ASSERT(this, WordEqual(field_representation,
                             IntPtrConstant(StoreHandler::kSmi)));
  Goto(&if_smi_field);

  BIND(&if_tagged_field);
  {
    Comment("store tagged field");
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Tagged(),
                              value, transition, miss);
  }

  BIND(&if_double_field);
  {
    Comment("store double field");
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Double(),
                              value, transition, miss);
  }

  BIND(&if_heap_object_field);
  {
    Comment("store heap object field");
    HandleStoreFieldAndReturn(handler_word, holder,
                              Representation::HeapObject(), value, transition,
                              miss);
  }

  BIND(&if_smi_field);
  {
    Comment("store smi field");
    HandleStoreFieldAndReturn(handler_word, holder, Representation::Smi(),
                              value, transition, miss);
  }
}

void AccessorAssembler::HandleStoreFieldAndReturn(Node* handler_word,
                                                  Node* holder,
                                                  Representation representation,
                                                  Node* value, Node* transition,
                                                  Label* miss) {
  bool transition_to_field = transition != nullptr;
  Node* prepared_value = PrepareValueForStore(
      handler_word, holder, representation, transition, value, miss);

  Label if_inobject(this), if_out_of_object(this);
  Branch(IsSetWord<StoreHandler::IsInobjectBits>(handler_word), &if_inobject,
         &if_out_of_object);

  BIND(&if_inobject);
  {
    StoreNamedField(handler_word, holder, true, representation, prepared_value,
                    transition_to_field, miss);
    if (transition_to_field) {
      StoreMap(holder, transition);
    }
    Return(value);
  }

  BIND(&if_out_of_object);
  {
    if (transition_to_field) {
      ExtendPropertiesBackingStore(holder, handler_word);
    }

    StoreNamedField(handler_word, holder, false, representation, prepared_value,
                    transition_to_field, miss);
    if (transition_to_field) {
      StoreMap(holder, transition);
    }
    Return(value);
  }
}

Node* AccessorAssembler::PrepareValueForStore(Node* handler_word, Node* holder,
                                              Representation representation,
                                              Node* transition, Node* value,
                                              Label* bailout) {
  if (representation.IsDouble()) {
    value = TryTaggedToFloat64(value, bailout);

  } else if (representation.IsHeapObject()) {
    GotoIf(TaggedIsSmi(value), bailout);

    Label done(this);
    if (FLAG_track_constant_fields && !transition) {
      // Skip field type check in favor of constant value check when storing
      // to constant field.
      GotoIf(WordEqual(DecodeWord<StoreHandler::KindBits>(handler_word),
                       IntPtrConstant(StoreHandler::kConstField)),
             &done);
    }
    Node* descriptor = DecodeWord<StoreHandler::DescriptorBits>(handler_word);
    Node* maybe_field_type = LoadDescriptorValue(
        transition ? transition : LoadMap(holder), descriptor);

    GotoIf(TaggedIsSmi(maybe_field_type), &done);
    // Check that value type matches the field type.
    {
      Node* field_type = LoadWeakCellValue(maybe_field_type, bailout);
      Branch(WordEqual(LoadMap(value), field_type), &done, bailout);
    }
    BIND(&done);

  } else if (representation.IsSmi()) {
    GotoIfNot(TaggedIsSmi(value), bailout);

  } else {
    DCHECK(representation.IsTagged());
  }
  return value;
}

void AccessorAssembler::ExtendPropertiesBackingStore(Node* object,
                                                     Node* handler_word) {
  Label done(this);
  GotoIfNot(IsSetWord<StoreHandler::ExtendStorageBits>(handler_word), &done);
  Comment("[ Extend storage");

  ParameterMode mode = OptimalParameterMode();

  // TODO(gsathya): Clean up the type conversions by creating smarter
  // helpers that do the correct op based on the mode.
  VARIABLE(var_properties, MachineRepresentation::kTaggedPointer);
  VARIABLE(var_encoded_hash, MachineRepresentation::kWord32);
  VARIABLE(var_length, ParameterRepresentation(mode));

  Node* properties = LoadObjectField(object, JSObject::kPropertiesOrHashOffset);
  var_properties.Bind(properties);

  Label if_smi_hash(this), if_property_array(this), extend_store(this);
  Branch(TaggedIsSmi(properties), &if_smi_hash, &if_property_array);

  BIND(&if_smi_hash);
  {
    Node* hash = SmiToWord32(properties);
    Node* encoded_hash =
        Word32Shl(hash, Int32Constant(PropertyArray::HashField::kShift));
    var_encoded_hash.Bind(encoded_hash);
    var_length.Bind(IntPtrOrSmiConstant(0, mode));
    var_properties.Bind(EmptyFixedArrayConstant());
    Goto(&extend_store);
  }

  BIND(&if_property_array);
  {
    Node* length_and_hash_int32 = LoadAndUntagToWord32ObjectField(
        var_properties.value(), PropertyArray::kLengthAndHashOffset);
    var_encoded_hash.Bind(Word32And(
        length_and_hash_int32, Int32Constant(PropertyArray::HashField::kMask)));
    Node* length_intptr = ChangeInt32ToIntPtr(
        Word32And(length_and_hash_int32,
                  Int32Constant(PropertyArray::LengthField::kMask)));
    Node* length = WordToParameter(length_intptr, mode);
    var_length.Bind(length);
    Goto(&extend_store);
  }

  BIND(&extend_store);
  {
    // Previous property deletion could have left behind unused backing store
    // capacity even for a map that think it doesn't have any unused fields.
    // Perform a bounds check to see if we actually have to grow the array.
    Node* index = DecodeWord<StoreHandler::FieldIndexBits>(handler_word);
    Node* offset = IntPtrMul(index, IntPtrConstant(kPointerSize));
    Node* size = ElementOffsetFromIndex(var_length.value(), PACKED_ELEMENTS,
                                        mode, FixedArray::kHeaderSize);
    GotoIf(UintPtrLessThan(offset, size), &done);

    Node* delta = IntPtrOrSmiConstant(JSObject::kFieldsAdded, mode);
    Node* new_capacity = IntPtrOrSmiAdd(var_length.value(), delta, mode);

    // Grow properties array.
    DCHECK(kMaxNumberOfDescriptors + JSObject::kFieldsAdded <
           FixedArrayBase::GetMaxLengthForNewSpaceAllocation(PACKED_ELEMENTS));
    // The size of a new properties backing store is guaranteed to be small
    // enough that the new backing store will be allocated in new space.
    CSA_ASSERT(this,
               UintPtrOrSmiLessThan(
                   new_capacity,
                   IntPtrOrSmiConstant(
                       kMaxNumberOfDescriptors + JSObject::kFieldsAdded, mode),
                   mode));

    Node* new_properties = AllocatePropertyArray(new_capacity, mode);

    FillPropertyArrayWithUndefined(new_properties, var_length.value(),
                                   new_capacity, mode);

    // |new_properties| is guaranteed to be in new space, so we can skip
    // the write barrier.
    CopyPropertyArrayValues(var_properties.value(), new_properties,
                            var_length.value(), SKIP_WRITE_BARRIER, mode);

    // TODO(gsathya): Clean up the type conversions by creating smarter
    // helpers that do the correct op based on the mode.
    Node* new_capacity_int32 =
        TruncateWordToWord32(ParameterToWord(new_capacity, mode));
    Node* new_length_and_hash_int32 =
        Word32Or(var_encoded_hash.value(), new_capacity_int32);
    StoreObjectField(new_properties, PropertyArray::kLengthAndHashOffset,
                     SmiFromWord32(new_length_and_hash_int32));
    StoreObjectField(object, JSObject::kPropertiesOrHashOffset, new_properties);
    Comment("] Extend storage");
    Goto(&done);
  }
  BIND(&done);
}

void AccessorAssembler::StoreNamedField(Node* handler_word, Node* object,
                                        bool is_inobject,
                                        Representation representation,
                                        Node* value, bool transition_to_field,
                                        Label* bailout) {
  bool store_value_as_double = representation.IsDouble();
  Node* property_storage = object;
  if (!is_inobject) {
    property_storage = LoadFastProperties(object);
  }

  Node* index = DecodeWord<StoreHandler::FieldIndexBits>(handler_word);
  Node* offset = IntPtrMul(index, IntPtrConstant(kPointerSize));
  if (representation.IsDouble()) {
    if (!FLAG_unbox_double_fields || !is_inobject) {
      if (transition_to_field) {
        Node* heap_number = AllocateHeapNumberWithValue(value, MUTABLE);
        // Store the new mutable heap number into the object.
        value = heap_number;
        store_value_as_double = false;
      } else {
        // Load the heap number.
        property_storage = LoadObjectField(property_storage, offset);
        // Store the double value into it.
        offset = IntPtrConstant(HeapNumber::kValueOffset);
      }
    }
  }

  // Do constant value check if necessary.
  if (FLAG_track_constant_fields && !transition_to_field) {
    Label done(this);
    GotoIfNot(WordEqual(DecodeWord<StoreHandler::KindBits>(handler_word),
                        IntPtrConstant(StoreHandler::kConstField)),
              &done);
    {
      if (store_value_as_double) {
        Node* current_value =
            LoadObjectField(property_storage, offset, MachineType::Float64());
        GotoIfNot(Float64Equal(current_value, value), bailout);
      } else {
        Node* current_value = LoadObjectField(property_storage, offset);
        GotoIfNot(WordEqual(current_value, value), bailout);
      }
      Goto(&done);
    }
    BIND(&done);
  }

  // Do the store.
  if (store_value_as_double) {
    StoreObjectFieldNoWriteBarrier(property_storage, offset, value,
                                   MachineRepresentation::kFloat64);
  } else if (representation.IsSmi()) {
    StoreObjectFieldNoWriteBarrier(property_storage, offset, value);
  } else {
    StoreObjectField(property_storage, offset, value);
  }
}

void AccessorAssembler::EmitFastElementsBoundsCheck(Node* object,
                                                    Node* elements,
                                                    Node* intptr_index,
                                                    Node* is_jsarray_condition,
                                                    Label* miss) {
  VARIABLE(var_length, MachineType::PointerRepresentation());
  Comment("Fast elements bounds check");
  Label if_array(this), length_loaded(this, &var_length);
  GotoIf(is_jsarray_condition, &if_array);
  {
    var_length.Bind(SmiUntag(LoadFixedArrayBaseLength(elements)));
    Goto(&length_loaded);
  }
  BIND(&if_array);
  {
    var_length.Bind(SmiUntag(LoadFastJSArrayLength(object)));
    Goto(&length_loaded);
  }
  BIND(&length_loaded);
  GotoIfNot(UintPtrLessThan(intptr_index, var_length.value()), miss);
}

void AccessorAssembler::EmitElementLoad(
    Node* object, Node* elements, Node* elements_kind, Node* intptr_index,
    Node* is_jsarray_condition, Label* if_hole, Label* rebox_double,
    Variable* var_double_value, Label* unimplemented_elements_kind,
    Label* out_of_bounds, Label* miss, ExitPoint* exit_point) {
  Label if_typed_array(this), if_fast_packed(this), if_fast_holey(this),
      if_fast_double(this), if_fast_holey_double(this), if_nonfast(this),
      if_dictionary(this);
  GotoIf(
      Int32GreaterThan(elements_kind, Int32Constant(LAST_FAST_ELEMENTS_KIND)),
      &if_nonfast);

  EmitFastElementsBoundsCheck(object, elements, intptr_index,
                              is_jsarray_condition, out_of_bounds);
  int32_t kinds[] = {// Handled by if_fast_packed.
                     PACKED_SMI_ELEMENTS, PACKED_ELEMENTS,
                     // Handled by if_fast_holey.
                     HOLEY_SMI_ELEMENTS, HOLEY_ELEMENTS,
                     // Handled by if_fast_double.
                     PACKED_DOUBLE_ELEMENTS,
                     // Handled by if_fast_holey_double.
                     HOLEY_DOUBLE_ELEMENTS};
  Label* labels[] = {// FAST_{SMI,}_ELEMENTS
                     &if_fast_packed, &if_fast_packed,
                     // FAST_HOLEY_{SMI,}_ELEMENTS
                     &if_fast_holey, &if_fast_holey,
                     // PACKED_DOUBLE_ELEMENTS
                     &if_fast_double,
                     // HOLEY_DOUBLE_ELEMENTS
                     &if_fast_holey_double};
  Switch(elements_kind, unimplemented_elements_kind, kinds, labels,
         arraysize(kinds));

  BIND(&if_fast_packed);
  {
    Comment("fast packed elements");
    exit_point->Return(LoadFixedArrayElement(elements, intptr_index));
  }

  BIND(&if_fast_holey);
  {
    Comment("fast holey elements");
    Node* element = LoadFixedArrayElement(elements, intptr_index);
    GotoIf(WordEqual(element, TheHoleConstant()), if_hole);
    exit_point->Return(element);
  }

  BIND(&if_fast_double);
  {
    Comment("packed double elements");
    var_double_value->Bind(LoadFixedDoubleArrayElement(elements, intptr_index,
                                                       MachineType::Float64()));
    Goto(rebox_double);
  }

  BIND(&if_fast_holey_double);
  {
    Comment("holey double elements");
    Node* value = LoadFixedDoubleArrayElement(elements, intptr_index,
                                              MachineType::Float64(), 0,
                                              INTPTR_PARAMETERS, if_hole);
    var_double_value->Bind(value);
    Goto(rebox_double);
  }

  BIND(&if_nonfast);
  {
    STATIC_ASSERT(LAST_ELEMENTS_KIND == LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND);
    GotoIf(Int32GreaterThanOrEqual(
               elements_kind,
               Int32Constant(FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND)),
           &if_typed_array);
    GotoIf(Word32Equal(elements_kind, Int32Constant(DICTIONARY_ELEMENTS)),
           &if_dictionary);
    Goto(unimplemented_elements_kind);
  }

  BIND(&if_dictionary);
  {
    Comment("dictionary elements");
    GotoIf(IntPtrLessThan(intptr_index, IntPtrConstant(0)), out_of_bounds);
    VARIABLE(var_entry, MachineType::PointerRepresentation());
    Label if_found(this);
    NumberDictionaryLookup(elements, intptr_index, &if_found, &var_entry,
                           if_hole);
    BIND(&if_found);
    // Check that the value is a data property.
    Node* index = EntryToIndex<NumberDictionary>(var_entry.value());
    Node* details = LoadDetailsByKeyIndex<NumberDictionary>(elements, index);
    Node* kind = DecodeWord32<PropertyDetails::KindField>(details);
    // TODO(jkummerow): Support accessors without missing?
    GotoIfNot(Word32Equal(kind, Int32Constant(kData)), miss);
    // Finally, load the value.
    exit_point->Return(LoadValueByKeyIndex<NumberDictionary>(elements, index));
  }

  BIND(&if_typed_array);
  {
    Comment("typed elements");
    // Check if buffer has been neutered.
    Node* buffer = LoadObjectField(object, JSArrayBufferView::kBufferOffset);
    GotoIf(IsDetachedBuffer(buffer), miss);

    // Bounds check.
    Node* length =
        SmiUntag(CAST(LoadObjectField(object, JSTypedArray::kLengthOffset)));
    GotoIfNot(UintPtrLessThan(intptr_index, length), out_of_bounds);

    // Backing store = external_pointer + base_pointer.
    Node* external_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kExternalPointerOffset,
                        MachineType::Pointer());
    Node* base_pointer =
        LoadObjectField(elements, FixedTypedArrayBase::kBasePointerOffset);
    Node* backing_store =
        IntPtrAdd(external_pointer, BitcastTaggedToWord(base_pointer));

    Label uint8_elements(this), int8_elements(this), uint16_elements(this),
        int16_elements(this), uint32_elements(this), int32_elements(this),
        float32_elements(this), float64_elements(this);
    Label* elements_kind_labels[] = {
        &uint8_elements,  &uint8_elements,   &int8_elements,
        &uint16_elements, &int16_elements,   &uint32_elements,
        &int32_elements,  &float32_elements, &float64_elements};
    int32_t elements_kinds[] = {
        UINT8_ELEMENTS,  UINT8_CLAMPED_ELEMENTS, INT8_ELEMENTS,
        UINT16_ELEMENTS, INT16_ELEMENTS,         UINT32_ELEMENTS,
        INT32_ELEMENTS,  FLOAT32_ELEMENTS,       FLOAT64_ELEMENTS};
    const size_t kTypedElementsKindCount =
        LAST_FIXED_TYPED_ARRAY_ELEMENTS_KIND -
        FIRST_FIXED_TYPED_ARRAY_ELEMENTS_KIND + 1;
    DCHECK_EQ(kTypedElementsKindCount, arraysize(elements_kinds));
    DCHECK_EQ(kTypedElementsKindCount, arraysize(elements_kind_labels));
    Switch(elements_kind, miss, elements_kinds, elements_kind_labels,
           kTypedElementsKindCount);
    BIND(&uint8_elements);
    {
      Comment("UINT8_ELEMENTS");  // Handles UINT8_CLAMPED_ELEMENTS too.
      Node* element = Load(MachineType::Uint8(), backing_store, intptr_index);
      exit_point->Return(SmiFromWord32(element));
    }
    BIND(&int8_elements);
    {
      Comment("INT8_ELEMENTS");
      Node* element = Load(MachineType::Int8(), backing_store, intptr_index);
      exit_point->Return(SmiFromWord32(element));
    }
    BIND(&uint16_elements);
    {
      Comment("UINT16_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(1));
      Node* element = Load(MachineType::Uint16(), backing_store, index);
      exit_point->Return(SmiFromWord32(element));
    }
    BIND(&int16_elements);
    {
      Comment("INT16_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(1));
      Node* element = Load(MachineType::Int16(), backing_store, index);
      exit_point->Return(SmiFromWord32(element));
    }
    BIND(&uint32_elements);
    {
      Comment("UINT32_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(2));
      Node* element = Load(MachineType::Uint32(), backing_store, index);
      exit_point->Return(ChangeUint32ToTagged(element));
    }
    BIND(&int32_elements);
    {
      Comment("INT32_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(2));
      Node* element = Load(MachineType::Int32(), backing_store, index);
      exit_point->Return(ChangeInt32ToTagged(element));
    }
    BIND(&float32_elements);
    {
      Comment("FLOAT32_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(2));
      Node* element = Load(MachineType::Float32(), backing_store, index);
      var_double_value->Bind(ChangeFloat32ToFloat64(element));
      Goto(rebox_double);
    }
    BIND(&float64_elements);
    {
      Comment("FLOAT64_ELEMENTS");
      Node* index = WordShl(intptr_index, IntPtrConstant(3));
      Node* element = Load(MachineType::Float64(), backing_store, index);
      var_double_value->Bind(element);
      Goto(rebox_double);
    }
  }
}

void AccessorAssembler::CheckPrototype(Node* prototype_cell, Node* name,
                                       Label* miss) {
  Node* maybe_prototype = LoadWeakCellValue(prototype_cell, miss);

  Label done(this);
  Label if_property_cell(this), if_dictionary_object(this);

  // |maybe_prototype| is either a PropertyCell or a slow-mode prototype.
  Branch(IsPropertyCell(maybe_prototype), &if_property_cell,
         &if_dictionary_object);

  BIND(&if_dictionary_object);
  {
    CSA_ASSERT(this, IsDictionaryMap(LoadMap(maybe_prototype)));
    NameDictionaryNegativeLookup(maybe_prototype, name, miss);
    Goto(&done);
  }

  BIND(&if_property_cell);
  {
    // Ensure the property cell still contains the hole.
    Node* value = LoadObjectField(maybe_prototype, PropertyCell::kValueOffset);
    GotoIfNot(IsTheHole(value), miss);
    Goto(&done);
  }

  BIND(&done);
}

void AccessorAssembler::NameDictionaryNegativeLookup(Node* object, Node* name,
                                                     Label* miss) {
  CSA_ASSERT(this, IsDictionaryMap(LoadMap(object)));
  Node* properties = LoadSlowProperties(object);
  // Ensure the property does not exist in a dictionary-mode object.
  VARIABLE(var_name_index, MachineType::PointerRepresentation());
  Label done(this);
  NameDictionaryLookup<NameDictionary>(properties, name, miss, &var_name_index,
                                       &done);
  BIND(&done);
}

void AccessorAssembler::BranchIfStrictMode(Node* vector, Node* slot,
                                           Label* if_strict) {
  Node* sfi =
      LoadObjectField(vector, FeedbackVector::kSharedFunctionInfoOffset);
  Node* metadata =
      LoadObjectField(sfi, SharedFunctionInfo::kFeedbackMetadataOffset);
  Node* slot_int = SmiToWord32(slot);

  // See VectorICComputer::index().
  const int kItemsPerWord = FeedbackMetadata::VectorICComputer::kItemsPerWord;
  Node* word_index = Int32Div(slot_int, Int32Constant(kItemsPerWord));
  Node* word_offset = Int32Mod(slot_int, Int32Constant(kItemsPerWord));
  Node* data = SmiToWord32(LoadFixedArrayElement(
      metadata, ChangeInt32ToIntPtr(word_index),
      FeedbackMetadata::kReservedIndexCount * kPointerSize, INTPTR_PARAMETERS));
  // See VectorICComputer::decode().
  const int kBitsPerItem = FeedbackMetadata::kFeedbackSlotKindBits;
  Node* shift = Int32Mul(word_offset, Int32Constant(kBitsPerItem));
  const int kMask = FeedbackMetadata::VectorICComputer::kMask;
  Node* kind = Word32And(Word32Shr(data, shift), Int32Constant(kMask));

  STATIC_ASSERT(FeedbackSlotKind::kStoreGlobalSloppy <=
                FeedbackSlotKind::kLastSloppyKind);
  STATIC_ASSERT(FeedbackSlotKind::kStoreKeyedSloppy <=
                FeedbackSlotKind::kLastSloppyKind);
  STATIC_ASSERT(FeedbackSlotKind::kStoreNamedSloppy <=
                FeedbackSlotKind::kLastSloppyKind);
  GotoIfNot(Int32LessThanOrEqual(kind, Int32Constant(static_cast<int>(
                                           FeedbackSlotKind::kLastSloppyKind))),
            if_strict);
}

void AccessorAssembler::GenericElementLoad(Node* receiver, Node* receiver_map,
                                           Node* instance_type, Node* index,
                                           Label* slow) {
  Comment("integer index");

  ExitPoint direct_exit(this);

  Label if_custom(this), if_element_hole(this), if_oob(this);
  // Receivers requiring non-standard element accesses (interceptors, access
  // checks, strings and string wrappers, proxies) are handled in the runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_CUSTOM_ELEMENTS_RECEIVER)),
         &if_custom);
  Node* elements = LoadElements(receiver);
  Node* elements_kind = LoadMapElementsKind(receiver_map);
  Node* is_jsarray_condition = InstanceTypeEqual(instance_type, JS_ARRAY_TYPE);
  VARIABLE(var_double_value, MachineRepresentation::kFloat64);
  Label rebox_double(this, &var_double_value);

  // Unimplemented elements kinds fall back to a runtime call.
  Label* unimplemented_elements_kind = slow;
  IncrementCounter(isolate()->counters()->ic_keyed_load_generic_smi(), 1);
  EmitElementLoad(receiver, elements, elements_kind, index,
                  is_jsarray_condition, &if_element_hole, &rebox_double,
                  &var_double_value, unimplemented_elements_kind, &if_oob, slow,
                  &direct_exit);

  BIND(&rebox_double);
  Return(AllocateHeapNumberWithValue(var_double_value.value()));

  BIND(&if_oob);
  {
    Comment("out of bounds");
    // Negative keys can't take the fast OOB path.
    GotoIf(IntPtrLessThan(index, IntPtrConstant(0)), slow);
    // Positive OOB indices are effectively the same as hole loads.
    Goto(&if_element_hole);
  }

  BIND(&if_element_hole);
  {
    Comment("found the hole");
    Label return_undefined(this);
    BranchIfPrototypesHaveNoElements(receiver_map, &return_undefined, slow);

    BIND(&return_undefined);
    Return(UndefinedConstant());
  }

  BIND(&if_custom);
  {
    Comment("check if string");
    GotoIfNot(IsStringInstanceType(instance_type), slow);
    Comment("load string character");
    Node* length = LoadAndUntagObjectField(receiver, String::kLengthOffset);
    GotoIfNot(UintPtrLessThan(index, length), slow);
    IncrementCounter(isolate()->counters()->ic_keyed_load_generic_smi(), 1);
    TailCallBuiltin(Builtins::kStringCharAt, NoContextConstant(), receiver,
                    index);
  }
}

void AccessorAssembler::GenericPropertyLoad(Node* receiver, Node* receiver_map,
                                            Node* instance_type,
                                            const LoadICParameters* p,
                                            Label* slow,
                                            UseStubCache use_stub_cache) {
  ExitPoint direct_exit(this);

  Comment("key is unique name");
  Label if_found_on_receiver(this), if_property_dictionary(this),
      lookup_prototype_chain(this), special_receiver(this);
  VARIABLE(var_details, MachineRepresentation::kWord32);
  VARIABLE(var_value, MachineRepresentation::kTagged);

  // Receivers requiring non-standard accesses (interceptors, access
  // checks, strings and string wrappers) are handled in the runtime.
  GotoIf(Int32LessThanOrEqual(instance_type,
                              Int32Constant(LAST_SPECIAL_RECEIVER_TYPE)),
         &special_receiver);

  // Check if the receiver has fast or slow properties.
  Node* bitfield3 = LoadMapBitField3(receiver_map);
  GotoIf(IsSetWord32<Map::DictionaryMap>(bitfield3), &if_property_dictionary);

  // Try looking up the property on the receiver; if unsuccessful, look
  // for a handler in the stub cache.
  Node* descriptors = LoadMapDescriptors(receiver_map);

  Label if_descriptor_found(this), stub_cache(this);
  VARIABLE(var_name_index, MachineType::PointerRepresentation());
  Label* notfound =
      use_stub_cache == kUseStubCache ? &stub_cache : &lookup_prototype_chain;
  DescriptorLookup(p->name, descriptors, bitfield3, &if_descriptor_found,
                   &var_name_index, notfound);

  BIND(&if_descriptor_found);
  {
    LoadPropertyFromFastObject(receiver, receiver_map, descriptors,
                               var_name_index.value(), &var_details,
                               &var_value);
    Goto(&if_found_on_receiver);
  }

  if (use_stub_cache == kUseStubCache) {
    BIND(&stub_cache);
    Comment("stub cache probe for fast property load");
    VARIABLE(var_handler, MachineRepresentation::kTagged);
    Label found_handler(this, &var_handler), stub_cache_miss(this);
    TryProbeStubCache(isolate()->load_stub_cache(), receiver, p->name,
                      &found_handler, &var_handler, &stub_cache_miss);
    BIND(&found_handler);
    {
      HandleLoadICHandlerCase(p, var_handler.value(), &stub_cache_miss,
                              &direct_exit);
    }

    BIND(&stub_cache_miss);
    {
      // TODO(jkummerow): Check if the property exists on the prototype
      // chain. If it doesn't, then there's no point in missing.
      Comment("KeyedLoadGeneric_miss");
      TailCallRuntime(Runtime::kKeyedLoadIC_Miss, p->context, p->receiver,
                      p->name, p->slot, p->vector);
    }
  }

  BIND(&if_property_dictionary);
  {
    Comment("dictionary property load");
    // We checked for LAST_CUSTOM_ELEMENTS_RECEIVER before, which rules out
    // seeing global objects here (which would need special handling).

    VARIABLE(var_name_index, MachineType::PointerRepresentation());
    Label dictionary_found(this, &var_name_index);
    Node* properties = LoadSlowProperties(receiver);
    NameDictionaryLookup<NameDictionary>(properties, p->name, &dictionary_found,
                                         &var_name_index,
                                         &lookup_prototype_chain);
    BIND(&dictionary_found);
    {
      LoadPropertyFromNameDictionary(properties, var_name_index.value(),
                                     &var_details, &var_value);
      Goto(&if_found_on_receiver);
    }
  }

  BIND(&if_found_on_receiver);
  {
    Node* value = CallGetterIfAccessor(var_value.value(), var_details.value(),
                                       p->context, receiver, slow);
    IncrementCounter(isolate()->counters()->ic_keyed_load_generic_symbol(), 1);
    Return(value);
  }

  BIND(&lookup_prototype_chain);
  {
    VARIABLE(var_holder_map, MachineRepresentation::kTagged);
    VARIABLE(var_holder_instance_type, MachineRepresentation::kWord32);
    Label return_undefined(this);
    Variable* merged_variables[] = {&var_holder_map, &var_holder_instance_type};
    Label loop(this, arraysize(merged_variables), merged_variables);

    var_holder_map.Bind(receiver_map);
    var_holder_instance_type.Bind(instance_type);
    // Private symbols must not be looked up on the prototype chain.
    GotoIf(IsPrivateSymbol(p->name), &return_undefined);
    Goto(&loop);
    BIND(&loop);
    {
      // Bailout if it can be an integer indexed exotic case.
      GotoIf(InstanceTypeEqual(var_holder_instance_type.value(),
                               JS_TYPED_ARRAY_TYPE),
             slow);
      Node* proto = LoadMapPrototype(var_holder_map.value());
      GotoIf(WordEqual(proto, NullConstant()), &return_undefined);
      Node* proto_map = LoadMap(proto);
      Node* proto_instance_type = LoadMapInstanceType(proto_map);
      var_holder_map.Bind(proto_map);
      var_holder_instance_type.Bind(proto_instance_type);
      Label next_proto(this), return_value(this, &var_value), goto_slow(this);
      TryGetOwnProperty(p->context, receiver, proto, proto_map,
                        proto_instance_type, p->name, &return_value, &var_value,
                        &next_proto, &goto_slow);

      // This trampoline and the next are required to appease Turbofan's
      // variable merging.
      BIND(&next_proto);
      Goto(&loop);

      BIND(&goto_slow);
      Goto(slow);

      BIND(&return_value);
      Return(var_value.value());
    }

    BIND(&return_undefined);
    Return(UndefinedConstant());
  }

  BIND(&special_receiver);
  {
    // TODO(jkummerow): Consider supporting JSModuleNamespace.
    GotoIfNot(InstanceTypeEqual(instance_type, JS_PROXY_TYPE), slow);

    direct_exit.ReturnCallStub(
        Builtins::CallableFor(isolate(), Builtins::kProxyGetProperty),
        p->context, receiver /*holder is the same as receiver*/, p->name,
        receiver);
  }
}

//////////////////// Stub cache access helpers.

enum AccessorAssembler::StubCacheTable : int {
  kPrimary = static_cast<int>(StubCache::kPrimary),
  kSecondary = static_cast<int>(StubCache::kSecondary)
};

Node* AccessorAssembler::StubCachePrimaryOffset(Node* name, Node* map) {
  // See v8::internal::StubCache::PrimaryOffset().
  STATIC_ASSERT(StubCache::kCacheIndexShift == Name::kHashShift);
  // Compute the hash of the name (use entire hash field).
  Node* hash_field = LoadNameHashField(name);
  CSA_ASSERT(this,
             Word32Equal(Word32And(hash_field,
                                   Int32Constant(Name::kHashNotComputedMask)),
                         Int32Constant(0)));

  // Using only the low bits in 64-bit mode is unlikely to increase the
  // risk of collision even if the heap is spread over an area larger than
  // 4Gb (and not at all if it isn't).
  Node* map32 = TruncateWordToWord32(BitcastTaggedToWord(map));
  Node* hash = Int32Add(hash_field, map32);
  // Base the offset on a simple combination of name and map.
  hash = Word32Xor(hash, Int32Constant(StubCache::kPrimaryMagic));
  uint32_t mask = (StubCache::kPrimaryTableSize - 1)
                  << StubCache::kCacheIndexShift;
  return ChangeUint32ToWord(Word32And(hash, Int32Constant(mask)));
}

Node* AccessorAssembler::StubCacheSecondaryOffset(Node* name, Node* seed) {
  // See v8::internal::StubCache::SecondaryOffset().

  // Use the seed from the primary cache in the secondary cache.
  Node* name32 = TruncateWordToWord32(BitcastTaggedToWord(name));
  Node* hash = Int32Sub(TruncateWordToWord32(seed), name32);
  hash = Int32Add(hash, Int32Constant(StubCache::kSecondaryMagic));
  int32_t mask = (StubCache::kSecondaryTableSize - 1)
                 << StubCache::kCacheIndexShift;
  return ChangeUint32ToWord(Word32And(hash, Int32Constant(mask)));
}

void AccessorAssembler::TryProbeStubCacheTable(StubCache* stub_cache,
                                               StubCacheTable table_id,
                                               Node* entry_offset, Node* name,
                                               Node* map, Label* if_handler,
                                               Variable* var_handler,
                                               Label* if_miss) {
  StubCache::Table table = static_cast<StubCache::Table>(table_id);
#ifdef DEBUG
  if (FLAG_test_secondary_stub_cache && table == StubCache::kPrimary) {
    Goto(if_miss);
    return;
  } else if (FLAG_test_primary_stub_cache && table == StubCache::kSecondary) {
    Goto(if_miss);
    return;
  }
#endif
  // The {table_offset} holds the entry offset times four (due to masking
  // and shifting optimizations).
  const int kMultiplier = sizeof(StubCache::Entry) >> Name::kHashShift;
  entry_offset = IntPtrMul(entry_offset, IntPtrConstant(kMultiplier));

  // Check that the key in the entry matches the name.
  Node* key_base =
      ExternalConstant(ExternalReference(stub_cache->key_reference(table)));
  Node* entry_key = Load(MachineType::Pointer(), key_base, entry_offset);
  GotoIf(WordNotEqual(name, entry_key), if_miss);

  // Get the map entry from the cache.
  DCHECK_EQ(kPointerSize * 2, stub_cache->map_reference(table).address() -
                                  stub_cache->key_reference(table).address());
  Node* entry_map =
      Load(MachineType::Pointer(), key_base,
           IntPtrAdd(entry_offset, IntPtrConstant(kPointerSize * 2)));
  GotoIf(WordNotEqual(map, entry_map), if_miss);

  DCHECK_EQ(kPointerSize, stub_cache->value_reference(table).address() -
                              stub_cache->key_reference(table).address());
  Node* handler = Load(MachineType::TaggedPointer(), key_base,
                       IntPtrAdd(entry_offset, IntPtrConstant(kPointerSize)));

  // We found the handler.
  var_handler->Bind(handler);
  Goto(if_handler);
}

void AccessorAssembler::TryProbeStubCache(StubCache* stub_cache, Node* receiver,
                                          Node* name, Label* if_handler,
                                          Variable* var_handler,
                                          Label* if_miss) {
  Label try_secondary(this), miss(this);

  Counters* counters = isolate()->counters();
  IncrementCounter(counters->megamorphic_stub_cache_probes(), 1);

  // Check that the {receiver} isn't a smi.
  GotoIf(TaggedIsSmi(receiver), &miss);

  Node* receiver_map = LoadMap(receiver);

  // Probe the primary table.
  Node* primary_offset = StubCachePrimaryOffset(name, receiver_map);
  TryProbeStubCacheTable(stub_cache, kPrimary, primary_offset, name,
                         receiver_map, if_handler, var_handler, &try_secondary);

  BIND(&try_secondary);
  {
    // Probe the secondary table.
    Node* secondary_offset = StubCacheSecondaryOffset(name, primary_offset);
    TryProbeStubCacheTable(stub_cache, kSecondary, secondary_offset, name,
                           receiver_map, if_handler, var_handler, &miss);
  }

  BIND(&miss);
  {
    IncrementCounter(counters->megamorphic_stub_cache_misses(), 1);
    Goto(if_miss);
  }
}

//////////////////// Entry points into private implementation (one per stub).

void AccessorAssembler::LoadIC_BytecodeHandler(const LoadICParameters* p,
                                               ExitPoint* exit_point) {
  // Must be kept in sync with LoadIC.

  // This function is hand-tuned to omit frame construction for common cases,
  // e.g.: monomorphic field and constant loads through smi handlers.
  // Polymorphic ICs with a hit in the first two entries also omit frames.
  // TODO(jgruber): Frame omission is fragile and can be affected by minor
  // changes in control flow and logic. We currently have no way of ensuring
  // that no frame is constructed, so it's easy to break this optimization by
  // accident.
  Label stub_call(this, Label::kDeferred), miss(this, Label::kDeferred);

  // Inlined fast path.
  {
    Comment("LoadIC_BytecodeHandler_fast");

    Node* recv_map = LoadReceiverMap(p->receiver);
    GotoIf(IsDeprecatedMap(recv_map), &miss);

    VARIABLE(var_handler, MachineRepresentation::kTagged);
    Label try_polymorphic(this), if_handler(this, &var_handler);

    Node* feedback =
        TryMonomorphicCase(p->slot, p->vector, recv_map, &if_handler,
                           &var_handler, &try_polymorphic);

    BIND(&if_handler);
    HandleLoadICHandlerCase(p, var_handler.value(), &miss, exit_point);

    BIND(&try_polymorphic);
    {
      GotoIfNot(WordEqual(LoadMap(feedback), FixedArrayMapConstant()),
                &stub_call);
      HandlePolymorphicCase(recv_map, feedback, &if_handler, &var_handler,
                            &miss, 2);
    }
  }

  BIND(&stub_call);
  {
    Comment("LoadIC_BytecodeHandler_noninlined");

    // Call into the stub that implements the non-inlined parts of LoadIC.
    Callable ic =
        Builtins::CallableFor(isolate(), Builtins::kLoadIC_Noninlined);
    Node* code_target = HeapConstant(ic.code());
    exit_point->ReturnCallStub(ic.descriptor(), code_target, p->context,
                               p->receiver, p->name, p->slot, p->vector);
  }

  BIND(&miss);
  {
    Comment("LoadIC_BytecodeHandler_miss");

    exit_point->ReturnCallRuntime(Runtime::kLoadIC_Miss, p->context,
                                  p->receiver, p->name, p->slot, p->vector);
  }
}

void AccessorAssembler::LoadIC(const LoadICParameters* p) {
  // Must be kept in sync with LoadIC_BytecodeHandler.

  ExitPoint direct_exit(this);

  VARIABLE(var_handler, MachineRepresentation::kTagged);
  Label if_handler(this, &var_handler), non_inlined(this, Label::kDeferred),
      try_polymorphic(this), miss(this, Label::kDeferred);

  Node* receiver_map = LoadReceiverMap(p->receiver);
  GotoIf(IsDeprecatedMap(receiver_map), &miss);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  BIND(&if_handler);
  HandleLoadICHandlerCase(p, var_handler.value(), &miss, &direct_exit);

  BIND(&try_polymorphic);
  {
    // Check polymorphic case.
    Comment("LoadIC_try_polymorphic");
    GotoIfNot(WordEqual(LoadMap(feedback), FixedArrayMapConstant()),
              &non_inlined);
    HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                          &miss, 2);
  }

  BIND(&non_inlined);
  LoadIC_Noninlined(p, receiver_map, feedback, &var_handler, &if_handler, &miss,
                    &direct_exit);

  BIND(&miss);
  direct_exit.ReturnCallRuntime(Runtime::kLoadIC_Miss, p->context, p->receiver,
                                p->name, p->slot, p->vector);
}

void AccessorAssembler::LoadIC_Noninlined(const LoadICParameters* p,
                                          Node* receiver_map, Node* feedback,
                                          Variable* var_handler,
                                          Label* if_handler, Label* miss,
                                          ExitPoint* exit_point) {
  Label try_uninitialized(this, Label::kDeferred);

  // Neither deprecated map nor monomorphic. These cases are handled in the
  // bytecode handler.
  CSA_ASSERT(this, Word32BinaryNot(IsDeprecatedMap(receiver_map)));
  CSA_ASSERT(this,
             WordNotEqual(receiver_map, LoadWeakCellValueUnchecked(feedback)));
  CSA_ASSERT(this, WordNotEqual(LoadMap(feedback), FixedArrayMapConstant()));
  DCHECK_EQ(MachineRepresentation::kTagged, var_handler->rep());

  {
    // Check megamorphic case.
    GotoIfNot(WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
              &try_uninitialized);

    TryProbeStubCache(isolate()->load_stub_cache(), p->receiver, p->name,
                      if_handler, var_handler, miss);
  }

  BIND(&try_uninitialized);
  {
    // Check uninitialized case.
    GotoIfNot(
        WordEqual(feedback, LoadRoot(Heap::kuninitialized_symbolRootIndex)),
        miss);
    exit_point->ReturnCallStub(
        Builtins::CallableFor(isolate(), Builtins::kLoadIC_Uninitialized),
        p->context, p->receiver, p->name, p->slot, p->vector);
  }
}

void AccessorAssembler::LoadIC_Uninitialized(const LoadICParameters* p) {
  Label miss(this, Label::kDeferred);
  Node* receiver = p->receiver;
  GotoIf(TaggedIsSmi(receiver), &miss);
  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);

  // Optimistically write the state transition to the vector.
  StoreFeedbackVectorSlot(p->vector, p->slot,
                          LoadRoot(Heap::kpremonomorphic_symbolRootIndex),
                          SKIP_WRITE_BARRIER, 0, SMI_PARAMETERS);

  {
    // Special case for Function.prototype load, because it's very common
    // for ICs that are only executed once (MyFunc.prototype.foo = ...).
    Label not_function_prototype(this, Label::kDeferred);
    GotoIfNot(InstanceTypeEqual(instance_type, JS_FUNCTION_TYPE),
              &not_function_prototype);
    GotoIfNot(IsPrototypeString(p->name), &not_function_prototype);

    // if (!(has_prototype_slot() && !has_non_instance_prototype())) use generic
    // property loading mechanism.
    int has_prototype_slot_mask = 1 << Map::kHasPrototypeSlot;
    int has_non_instance_prototype_mask = 1 << Map::kHasNonInstancePrototype;
    GotoIfNot(
        Word32Equal(Word32And(LoadMapBitField(receiver_map),
                              Int32Constant(has_prototype_slot_mask |
                                            has_non_instance_prototype_mask)),
                    Int32Constant(has_prototype_slot_mask)),
        &not_function_prototype);
    Return(LoadJSFunctionPrototype(receiver, &miss));
    BIND(&not_function_prototype);
  }

  GenericPropertyLoad(receiver, receiver_map, instance_type, p, &miss,
                      kDontUseStubCache);

  BIND(&miss);
  {
    // Undo the optimistic state transition.
    StoreFeedbackVectorSlot(p->vector, p->slot,
                            LoadRoot(Heap::kuninitialized_symbolRootIndex),
                            SKIP_WRITE_BARRIER, 0, SMI_PARAMETERS);

    TailCallRuntime(Runtime::kLoadIC_Miss, p->context, p->receiver, p->name,
                    p->slot, p->vector);
  }
}

void AccessorAssembler::LoadICProtoArray(
    const LoadICParameters* p, Node* handler,
    bool throw_reference_error_if_nonexistent) {
  Label miss(this);
  CSA_ASSERT(this, Word32BinaryNot(TaggedIsSmi(handler)));
  CSA_ASSERT(this, IsFixedArrayMap(LoadMap(handler)));

  ExitPoint direct_exit(this);

  Node* smi_handler = LoadObjectField(handler, LoadHandler::kSmiHandlerOffset);
  Node* handler_flags = SmiUntag(smi_handler);

  Node* handler_length = LoadAndUntagFixedArrayBaseLength(handler);

  Node* holder = EmitLoadICProtoArrayCheck(p, handler, handler_length,
                                           handler_flags, &miss);

  HandleLoadICSmiHandlerCase(p, holder, smi_handler, &miss, &direct_exit,
                             throw_reference_error_if_nonexistent,
                             kOnlyProperties);

  BIND(&miss);
  {
    TailCallRuntime(Runtime::kLoadIC_Miss, p->context, p->receiver, p->name,
                    p->slot, p->vector);
  }
}

void AccessorAssembler::LoadGlobalIC_TryPropertyCellCase(
    Node* vector, Node* slot, ExitPoint* exit_point, Label* try_handler,
    Label* miss, ParameterMode slot_mode) {
  Comment("LoadGlobalIC_TryPropertyCellCase");

  Node* weak_cell = LoadFeedbackVectorSlot(vector, slot, 0, slot_mode);
  CSA_ASSERT(this, HasInstanceType(weak_cell, WEAK_CELL_TYPE));

  // Load value or try handler case if the {weak_cell} is cleared.
  Node* property_cell = LoadWeakCellValue(weak_cell, try_handler);
  CSA_ASSERT(this, IsPropertyCell(property_cell));

  Node* value = LoadObjectField(property_cell, PropertyCell::kValueOffset);
  GotoIf(WordEqual(value, TheHoleConstant()), miss);
  exit_point->Return(value);
}

void AccessorAssembler::LoadGlobalIC_TryHandlerCase(const LoadICParameters* pp,
                                                    TypeofMode typeof_mode,
                                                    ExitPoint* exit_point,
                                                    Label* miss) {
  Comment("LoadGlobalIC_TryHandlerCase");

  Label call_handler(this), non_smi(this);

  Node* handler = LoadFeedbackVectorSlot(pp->vector, pp->slot, kPointerSize,
                                         SMI_PARAMETERS);
  GotoIf(WordEqual(handler, LoadRoot(Heap::kuninitialized_symbolRootIndex)),
         miss);

  GotoIfNot(TaggedIsSmi(handler), &non_smi);

  bool throw_reference_error_if_nonexistent = typeof_mode == NOT_INSIDE_TYPEOF;

  {
    LoadICParameters p = *pp;
    DCHECK_NULL(p.receiver);
    Node* native_context = LoadNativeContext(p.context);
    p.receiver =
        LoadContextElement(native_context, Context::GLOBAL_PROXY_INDEX);
    Node* holder = LoadContextElement(native_context, Context::EXTENSION_INDEX);
    HandleLoadICSmiHandlerCase(&p, holder, handler, miss, exit_point,
                               throw_reference_error_if_nonexistent,
                               kOnlyProperties);
  }

  BIND(&non_smi);
  GotoIf(IsCodeMap(LoadMap(handler)), &call_handler);

  HandleLoadGlobalICHandlerCase(pp, handler, miss, exit_point,
                                throw_reference_error_if_nonexistent);

  BIND(&call_handler);
  {
    LoadWithVectorDescriptor descriptor(isolate());
    Node* native_context = LoadNativeContext(pp->context);
    Node* receiver =
        LoadContextElement(native_context, Context::GLOBAL_PROXY_INDEX);
    exit_point->ReturnCallStub(descriptor, handler, pp->context, receiver,
                               pp->name, pp->slot, pp->vector);
  }
}

void AccessorAssembler::LoadGlobalIC_MissCase(const LoadICParameters* p,
                                              ExitPoint* exit_point) {
  Comment("LoadGlobalIC_MissCase");

  exit_point->ReturnCallRuntime(Runtime::kLoadGlobalIC_Miss, p->context,
                                p->name, p->slot, p->vector);
}

void AccessorAssembler::LoadGlobalIC(const LoadICParameters* p,
                                     TypeofMode typeof_mode) {
  // Must be kept in sync with Interpreter::BuildLoadGlobal.

  ExitPoint direct_exit(this);

  Label try_handler(this), miss(this);
  LoadGlobalIC_TryPropertyCellCase(p->vector, p->slot, &direct_exit,
                                   &try_handler, &miss);

  BIND(&try_handler);
  LoadGlobalIC_TryHandlerCase(p, typeof_mode, &direct_exit, &miss);

  BIND(&miss);
  LoadGlobalIC_MissCase(p, &direct_exit);
}

void AccessorAssembler::KeyedLoadIC(const LoadICParameters* p) {
  ExitPoint direct_exit(this);

  VARIABLE(var_handler, MachineRepresentation::kTagged);
  Label if_handler(this, &var_handler), try_polymorphic(this, Label::kDeferred),
      try_megamorphic(this, Label::kDeferred),
      try_polymorphic_name(this, Label::kDeferred),
      miss(this, Label::kDeferred);

  Node* receiver_map = LoadReceiverMap(p->receiver);
  GotoIf(IsDeprecatedMap(receiver_map), &miss);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  BIND(&if_handler);
  {
    HandleLoadICHandlerCase(p, var_handler.value(), &miss, &direct_exit,
                            kSupportElements);
  }

  BIND(&try_polymorphic);
  {
    // Check polymorphic case.
    Comment("KeyedLoadIC_try_polymorphic");
    GotoIfNot(WordEqual(LoadMap(feedback), FixedArrayMapConstant()),
              &try_megamorphic);
    HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                          &miss, 2);
  }

  BIND(&try_megamorphic);
  {
    // Check megamorphic case.
    Comment("KeyedLoadIC_try_megamorphic");
    GotoIfNot(WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
              &try_polymorphic_name);
    // TODO(jkummerow): Inline this? Or some of it?
    TailCallStub(
        Builtins::CallableFor(isolate(), Builtins::kKeyedLoadIC_Megamorphic),
        p->context, p->receiver, p->name, p->slot, p->vector);
  }
  BIND(&try_polymorphic_name);
  {
    // We might have a name in feedback, and a fixed array in the next slot.
    Node* name = p->name;
    Comment("KeyedLoadIC_try_polymorphic_name");
    VARIABLE(var_name, MachineRepresentation::kTagged, name);
    VARIABLE(var_index, MachineType::PointerRepresentation());
    Label if_polymorphic_name(this, &var_name), if_internalized(this),
        if_notinternalized(this, Label::kDeferred);

    // Fast-case: The recorded {feedback} matches the {name}.
    GotoIf(WordEqual(feedback, name), &if_polymorphic_name);

    // Try to internalize the {name} if it isn't already.
    TryToName(name, &miss, &var_index, &if_internalized, &var_name, &miss,
              &if_notinternalized);

    BIND(&if_internalized);
    {
      // The {var_name} now contains a unique name.
      Branch(WordEqual(feedback, var_name.value()), &if_polymorphic_name,
             &miss);
    }

    BIND(&if_notinternalized);
    {
      // Try to internalize the {name}.
      Node* function = ExternalConstant(
          ExternalReference::try_internalize_string_function(isolate()));
      var_name.Bind(CallCFunction1(MachineType::AnyTagged(),
                                   MachineType::AnyTagged(), function, name));
      Goto(&if_internalized);
    }

    BIND(&if_polymorphic_name);
    {
      // If the name comparison succeeded, we know we have a fixed array with
      // at least one map/handler pair.
      Node* name = var_name.value();
      TailCallBuiltin(Builtins::kKeyedLoadIC_PolymorphicName, p->context,
                      p->receiver, name, p->slot, p->vector);
    }
  }

  BIND(&miss);
  {
    Comment("KeyedLoadIC_miss");
    TailCallRuntime(Runtime::kKeyedLoadIC_Miss, p->context, p->receiver,
                    p->name, p->slot, p->vector);
  }
}

void AccessorAssembler::KeyedLoadICGeneric(const LoadICParameters* p) {
  VARIABLE(var_index, MachineType::PointerRepresentation());
  VARIABLE(var_unique, MachineRepresentation::kTagged);
  var_unique.Bind(p->name);  // Dummy initialization.
  Label if_index(this), if_unique_name(this), if_notunique(this), slow(this);

  Node* receiver = p->receiver;
  GotoIf(TaggedIsSmi(receiver), &slow);
  Node* receiver_map = LoadMap(receiver);
  Node* instance_type = LoadMapInstanceType(receiver_map);

  TryToName(p->name, &if_index, &var_index, &if_unique_name, &var_unique, &slow,
            &if_notunique);

  BIND(&if_index);
  {
    GenericElementLoad(receiver, receiver_map, instance_type, var_index.value(),
                       &slow);
  }

  BIND(&if_unique_name);
  {
    LoadICParameters pp = *p;
    pp.name = var_unique.value();
    GenericPropertyLoad(receiver, receiver_map, instance_type, &pp, &slow);
  }

  BIND(&if_notunique);
  {
    if (FLAG_internalize_on_the_fly) {
      // Ideally we could return undefined directly here if the name is not
      // found in the string table, i.e. it was never internalized, but that
      // invariant doesn't hold with named property interceptors (at this
      // point), so we take the {slow} path instead.
      Label if_in_string_table(this);
      TryInternalizeString(p->name, &if_index, &var_index, &if_in_string_table,
                           &var_unique, &slow, &slow);

      BIND(&if_in_string_table);
      {
        // TODO(bmeurer): We currently use a version of GenericPropertyLoad
        // here, where we don't try to probe the megamorphic stub cache after
        // successfully internalizing the incoming string. Past experiments
        // with this have shown that it causes too much traffic on the stub
        // cache. We may want to re-evaluate that in the future.
        LoadICParameters pp = *p;
        pp.name = var_unique.value();
        GenericPropertyLoad(receiver, receiver_map, instance_type, &pp, &slow,
                            kDontUseStubCache);
      }
    } else {
      Goto(&slow);
    }
  }

  BIND(&slow);
  {
    Comment("KeyedLoadGeneric_slow");
    IncrementCounter(isolate()->counters()->ic_keyed_load_generic_slow(), 1);
    // TODO(jkummerow): Should we use the GetProperty TF stub instead?
    TailCallRuntime(Runtime::kKeyedGetProperty, p->context, p->receiver,
                    p->name);
  }
}

void AccessorAssembler::KeyedLoadICPolymorphicName(const LoadICParameters* p) {
  VARIABLE(var_handler, MachineRepresentation::kTagged);
  Label if_handler(this, &var_handler), miss(this, Label::kDeferred);

  Node* receiver = p->receiver;
  Node* receiver_map = LoadReceiverMap(receiver);
  Node* name = p->name;
  Node* vector = p->vector;
  Node* slot = p->slot;
  Node* context = p->context;

  // When we get here, we know that the {name} matches the recorded
  // feedback name in the {vector} and can safely be used for the
  // LoadIC handler logic below.
  CSA_ASSERT(this, IsName(name));
  CSA_ASSERT(this, Word32BinaryNot(IsDeprecatedMap(receiver_map)));
  CSA_ASSERT(this, WordEqual(name, LoadFeedbackVectorSlot(vector, slot, 0,
                                                          SMI_PARAMETERS)));

  // Check if we have a matching handler for the {receiver_map}.
  Node* array =
      LoadFeedbackVectorSlot(vector, slot, kPointerSize, SMI_PARAMETERS);
  HandlePolymorphicCase(receiver_map, array, &if_handler, &var_handler, &miss,
                        1);

  BIND(&if_handler);
  {
    ExitPoint direct_exit(this);
    HandleLoadICHandlerCase(p, var_handler.value(), &miss, &direct_exit,
                            kOnlyProperties);
  }

  BIND(&miss);
  {
    Comment("KeyedLoadIC_miss");
    TailCallRuntime(Runtime::kKeyedLoadIC_Miss, context, receiver, name, slot,
                    vector);
  }
}

void AccessorAssembler::StoreIC(const StoreICParameters* p) {
  VARIABLE(var_handler, MachineRepresentation::kTagged);
  Label if_handler(this, &var_handler), try_polymorphic(this, Label::kDeferred),
      try_megamorphic(this, Label::kDeferred),
      try_uninitialized(this, Label::kDeferred), miss(this, Label::kDeferred);

  Node* receiver_map = LoadReceiverMap(p->receiver);
  GotoIf(IsDeprecatedMap(receiver_map), &miss);

  // Check monomorphic case.
  Node* feedback =
      TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                         &var_handler, &try_polymorphic);
  BIND(&if_handler);
  {
    Comment("StoreIC_if_handler");
    HandleStoreICHandlerCase(p, var_handler.value(), &miss);
  }

  BIND(&try_polymorphic);
  {
    // Check polymorphic case.
    Comment("StoreIC_try_polymorphic");
    GotoIfNot(
        WordEqual(LoadMap(feedback), LoadRoot(Heap::kFixedArrayMapRootIndex)),
        &try_megamorphic);
    HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                          &miss, 2);
  }

  BIND(&try_megamorphic);
  {
    // Check megamorphic case.
    GotoIfNot(WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
              &try_uninitialized);

    TryProbeStubCache(isolate()->store_stub_cache(), p->receiver, p->name,
                      &if_handler, &var_handler, &miss);
  }
  BIND(&try_uninitialized);
  {
    // Check uninitialized case.
    GotoIfNot(
        WordEqual(feedback, LoadRoot(Heap::kuninitialized_symbolRootIndex)),
        &miss);
    Callable stub =
        Builtins::CallableFor(isolate(), Builtins::kStoreIC_Uninitialized);
    TailCallStub(stub, p->context, p->receiver, p->name, p->value, p->slot,
                 p->vector);
  }
  BIND(&miss);
  {
    TailCallRuntime(Runtime::kStoreIC_Miss, p->context, p->value, p->slot,
                    p->vector, p->receiver, p->name);
  }
}

void AccessorAssembler::StoreGlobalIC_PropertyCellCase(Node* property_cell,
                                                       Node* value,
                                                       ExitPoint* exit_point,
                                                       Label* miss) {
  Comment("StoreGlobalIC_TryPropertyCellCase");
  CSA_ASSERT(this, IsPropertyCell(property_cell));

  // Load the payload of the global parameter cell. A hole indicates that
  // the cell has been invalidated and that the store must be handled by the
  // runtime.
  Node* cell_contents =
      LoadObjectField(property_cell, PropertyCell::kValueOffset);
  Node* details = LoadAndUntagToWord32ObjectField(property_cell,
                                                  PropertyCell::kDetailsOffset);
  Node* type = DecodeWord32<PropertyDetails::PropertyCellTypeField>(details);

  Label constant(this), store(this), not_smi(this);

  GotoIf(Word32Equal(type, Int32Constant(
                               static_cast<int>(PropertyCellType::kConstant))),
         &constant);

  GotoIf(IsTheHole(cell_contents), miss);

  GotoIf(Word32Equal(
             type, Int32Constant(static_cast<int>(PropertyCellType::kMutable))),
         &store);
  CSA_ASSERT(this,
             Word32Or(Word32Equal(type, Int32Constant(static_cast<int>(
                                            PropertyCellType::kConstantType))),
                      Word32Equal(type, Int32Constant(static_cast<int>(
                                            PropertyCellType::kUndefined)))));

  GotoIfNot(TaggedIsSmi(cell_contents), &not_smi);
  GotoIfNot(TaggedIsSmi(value), miss);
  Goto(&store);

  BIND(&not_smi);
  {
    GotoIf(TaggedIsSmi(value), miss);
    Node* expected_map = LoadMap(cell_contents);
    Node* map = LoadMap(value);
    GotoIfNot(WordEqual(expected_map, map), miss);
    Goto(&store);
  }

  BIND(&store);
  {
    StoreObjectField(property_cell, PropertyCell::kValueOffset, value);
    exit_point->Return(value);
  }

  BIND(&constant);
  {
    GotoIfNot(WordEqual(cell_contents, value), miss);
    exit_point->Return(value);
  }
}

void AccessorAssembler::KeyedStoreIC(const StoreICParameters* p) {
  Label miss(this, Label::kDeferred);
  {
    VARIABLE(var_handler, MachineRepresentation::kTagged);

    Label if_handler(this, &var_handler),
        try_polymorphic(this, Label::kDeferred),
        try_megamorphic(this, Label::kDeferred),
        try_polymorphic_name(this, Label::kDeferred);

    Node* receiver_map = LoadReceiverMap(p->receiver);
    GotoIf(IsDeprecatedMap(receiver_map), &miss);

    // Check monomorphic case.
    Node* feedback =
        TryMonomorphicCase(p->slot, p->vector, receiver_map, &if_handler,
                           &var_handler, &try_polymorphic);
    BIND(&if_handler);
    {
      Comment("KeyedStoreIC_if_handler");
      HandleStoreICHandlerCase(p, var_handler.value(), &miss, kSupportElements);
    }

    BIND(&try_polymorphic);
    {
      // CheckPolymorphic case.
      Comment("KeyedStoreIC_try_polymorphic");
      GotoIfNot(
          WordEqual(LoadMap(feedback), LoadRoot(Heap::kFixedArrayMapRootIndex)),
          &try_megamorphic);
      HandlePolymorphicCase(receiver_map, feedback, &if_handler, &var_handler,
                            &miss, 2);
    }

    BIND(&try_megamorphic);
    {
      // Check megamorphic case.
      Comment("KeyedStoreIC_try_megamorphic");
      GotoIfNot(
          WordEqual(feedback, LoadRoot(Heap::kmegamorphic_symbolRootIndex)),
          &try_polymorphic_name);
      TailCallStub(
          Builtins::CallableFor(isolate(), Builtins::kKeyedStoreIC_Megamorphic),
          p->context, p->receiver, p->name, p->value, p->slot, p->vector);
    }

    BIND(&try_polymorphic_name);
    {
      // We might have a name in feedback, and a fixed array in the next slot.
      Comment("KeyedStoreIC_try_polymorphic_name");
      GotoIfNot(WordEqual(feedback, p->name), &miss);
      // If the name comparison succeeded, we know we have a feedback vector
      // with at least one map/handler pair.
      Node* array = LoadFeedbackVectorSlot(p->vector, p->slot, kPointerSize,
                                           SMI_PARAMETERS);
      HandlePolymorphicCase(receiver_map, array, &if_handler, &var_handler,
                            &miss, 1);
    }
  }
  BIND(&miss);
  {
    Comment("KeyedStoreIC_miss");
    TailCallRuntime(Runtime::kKeyedStoreIC_Miss, p->context, p->value, p->slot,
                    p->vector, p->receiver, p->name);
  }
}

//////////////////// Public methods.

void AccessorAssembler::GenerateLoadIC() {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, receiver, name, slot, vector);
  LoadIC(&p);
}

void AccessorAssembler::GenerateLoadIC_Noninlined() {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  ExitPoint direct_exit(this);
  VARIABLE(var_handler, MachineRepresentation::kTagged);
  Label if_handler(this, &var_handler), miss(this, Label::kDeferred);

  Node* receiver_map = LoadReceiverMap(receiver);
  Node* feedback = LoadFeedbackVectorSlot(vector, slot, 0, SMI_PARAMETERS);

  LoadICParameters p(context, receiver, name, slot, vector);
  LoadIC_Noninlined(&p, receiver_map, feedback, &var_handler, &if_handler,
                    &miss, &direct_exit);

  BIND(&if_handler);
  HandleLoadICHandlerCase(&p, var_handler.value(), &miss, &direct_exit);

  BIND(&miss);
  direct_exit.ReturnCallRuntime(Runtime::kLoadIC_Miss, context, receiver, name,
                                slot, vector);
}

void AccessorAssembler::GenerateLoadIC_Uninitialized() {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, receiver, name, slot, vector);
  LoadIC_Uninitialized(&p);
}

void AccessorAssembler::GenerateLoadICTrampoline() {
  typedef LoadDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* context = Parameter(Descriptor::kContext);
  Node* vector = LoadFeedbackVectorForStub();

  TailCallBuiltin(Builtins::kLoadIC, context, receiver, name, slot, vector);
}

void AccessorAssembler::GenerateLoadICProtoArray(
    bool throw_reference_error_if_nonexistent) {
  typedef LoadICProtoArrayDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* handler = Parameter(Descriptor::kHandler);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, receiver, name, slot, vector);
  LoadICProtoArray(&p, handler, throw_reference_error_if_nonexistent);
}

void AccessorAssembler::GenerateLoadField() {
  typedef LoadFieldDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = nullptr;
  Node* slot = nullptr;
  Node* vector = nullptr;
  Node* context = Parameter(Descriptor::kContext);
  LoadICParameters p(context, receiver, name, slot, vector);

  ExitPoint direct_exit(this);

  VARIABLE(var_double_value, MachineRepresentation::kFloat64);
  Label rebox_double(this, &var_double_value);

  Node* smi_handler = Parameter(Descriptor::kSmiHandler);
  Node* handler_word = SmiUntag(smi_handler);
  HandleLoadField(receiver, handler_word, &var_double_value, &rebox_double,
                  &direct_exit);

  BIND(&rebox_double);
  Return(AllocateHeapNumberWithValue(var_double_value.value()));
}

void AccessorAssembler::GenerateLoadGlobalIC(TypeofMode typeof_mode) {
  typedef LoadGlobalWithVectorDescriptor Descriptor;

  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, nullptr, name, slot, vector);
  LoadGlobalIC(&p, typeof_mode);
}

void AccessorAssembler::GenerateLoadGlobalICTrampoline(TypeofMode typeof_mode) {
  typedef LoadGlobalDescriptor Descriptor;

  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* context = Parameter(Descriptor::kContext);
  Node* vector = LoadFeedbackVectorForStub();

  Callable callable =
      CodeFactory::LoadGlobalICInOptimizedCode(isolate(), typeof_mode);
  TailCallStub(callable, context, name, slot, vector);
}

void AccessorAssembler::GenerateKeyedLoadIC() {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, receiver, name, slot, vector);
  KeyedLoadIC(&p);
}

void AccessorAssembler::GenerateKeyedLoadICTrampoline() {
  typedef LoadDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* context = Parameter(Descriptor::kContext);
  Node* vector = LoadFeedbackVectorForStub();

  TailCallBuiltin(Builtins::kKeyedLoadIC, context, receiver, name, slot,
                  vector);
}

void AccessorAssembler::GenerateKeyedLoadIC_Megamorphic() {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, receiver, name, slot, vector);
  KeyedLoadICGeneric(&p);
}

void AccessorAssembler::GenerateKeyedLoadIC_PolymorphicName() {
  typedef LoadWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  LoadICParameters p(context, receiver, name, slot, vector);
  KeyedLoadICPolymorphicName(&p);
}

void AccessorAssembler::GenerateStoreIC() {
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  StoreICParameters p(context, receiver, name, value, slot, vector);
  StoreIC(&p);
}

void AccessorAssembler::GenerateStoreICTrampoline() {
  typedef StoreDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* context = Parameter(Descriptor::kContext);
  Node* vector = LoadFeedbackVectorForStub();

  Callable callable = Builtins::CallableFor(isolate(), Builtins::kStoreIC);
  TailCallStub(callable, context, receiver, name, value, slot, vector);
}

void AccessorAssembler::GenerateKeyedStoreIC() {
  typedef StoreWithVectorDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* vector = Parameter(Descriptor::kVector);
  Node* context = Parameter(Descriptor::kContext);

  StoreICParameters p(context, receiver, name, value, slot, vector);
  KeyedStoreIC(&p);
}

void AccessorAssembler::GenerateKeyedStoreICTrampoline() {
  typedef StoreDescriptor Descriptor;

  Node* receiver = Parameter(Descriptor::kReceiver);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* slot = Parameter(Descriptor::kSlot);
  Node* context = Parameter(Descriptor::kContext);
  Node* vector = LoadFeedbackVectorForStub();

  Callable callable = Builtins::CallableFor(isolate(), Builtins::kKeyedStoreIC);
  TailCallStub(callable, context, receiver, name, value, slot, vector);
}

}  // namespace internal
}  // namespace v8
