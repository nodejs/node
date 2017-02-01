// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/handler-compiler.h"

#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-configuration.h"
#include "src/ic/ic-inl.h"
#include "src/ic/ic.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

Handle<Code> PropertyHandlerCompiler::Find(Handle<Name> name,
                                           Handle<Map> stub_holder,
                                           Code::Kind kind,
                                           CacheHolderFlag cache_holder) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, cache_holder);
  Code* code = stub_holder->LookupInCodeCache(*name, flags);
  if (code == nullptr) return Handle<Code>();
  return handle(code);
}


Handle<Code> NamedLoadHandlerCompiler::ComputeLoadNonexistent(
    Handle<Name> name, Handle<Map> receiver_map) {
  Isolate* isolate = name->GetIsolate();
  if (receiver_map->prototype()->IsNull(isolate)) {
    // TODO(jkummerow/verwaest): If there is no prototype and the property
    // is nonexistent, introduce a builtin to handle this (fast properties
    // -> return undefined, dictionary properties -> do negative lookup).
    return Handle<Code>();
  }
  CacheHolderFlag flag;
  Handle<Map> stub_holder_map =
      IC::GetHandlerCacheHolder(receiver_map, false, isolate, &flag);

  // If no dictionary mode objects are present in the prototype chain, the load
  // nonexistent IC stub can be shared for all names for a given map and we use
  // the empty string for the map cache in that case. If there are dictionary
  // mode objects involved, we need to do negative lookups in the stub and
  // therefore the stub will be specific to the name.
  Handle<Name> cache_name =
      receiver_map->is_dictionary_map()
          ? name
          : Handle<Name>::cast(isolate->factory()->nonexistent_symbol());
  Handle<Map> current_map = stub_holder_map;
  Handle<JSObject> last(JSObject::cast(receiver_map->prototype()));
  while (true) {
    if (current_map->is_dictionary_map()) cache_name = name;
    if (current_map->prototype()->IsNull(isolate)) break;
    if (name->IsPrivate()) {
      // TODO(verwaest): Use nonexistent_private_symbol.
      cache_name = name;
      if (!current_map->has_hidden_prototype()) break;
    }

    last = handle(JSObject::cast(current_map->prototype()));
    current_map = handle(last->map());
  }
  // Compile the stub that is either shared for all names or
  // name specific if there are global objects involved.
  Handle<Code> handler = PropertyHandlerCompiler::Find(
      cache_name, stub_holder_map, Code::LOAD_IC, flag);
  if (!handler.is_null()) return handler;

  TRACE_HANDLER_STATS(isolate, LoadIC_LoadNonexistent);
  NamedLoadHandlerCompiler compiler(isolate, receiver_map, last, flag);
  handler = compiler.CompileLoadNonexistent(cache_name);
  Map::UpdateCodeCache(stub_holder_map, cache_name, handler);
  return handler;
}


Handle<Code> PropertyHandlerCompiler::GetCode(Code::Kind kind,
                                              Handle<Name> name) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, cache_holder());
  Handle<Code> code = GetCodeWithFlags(flags, name);
  PROFILE(isolate(), CodeCreateEvent(CodeEventListener::HANDLER_TAG,
                                     AbstractCode::cast(*code), *name));
#ifdef DEBUG
  code->VerifyEmbeddedObjects();
#endif
  return code;
}


#define __ ACCESS_MASM(masm())


Register NamedLoadHandlerCompiler::FrontendHeader(Register object_reg,
                                                  Handle<Name> name,
                                                  Label* miss,
                                                  ReturnHolder return_what) {
  PrototypeCheckType check_type = SKIP_RECEIVER;
  int function_index = map()->IsPrimitiveMap()
                           ? map()->GetConstructorFunctionIndex()
                           : Map::kNoConstructorFunctionIndex;
  if (function_index != Map::kNoConstructorFunctionIndex) {
    GenerateDirectLoadGlobalFunctionPrototype(masm(), function_index,
                                              scratch1(), miss);
    Object* function = isolate()->native_context()->get(function_index);
    Object* prototype = JSFunction::cast(function)->instance_prototype();
    Handle<Map> map(JSObject::cast(prototype)->map());
    set_map(map);
    object_reg = scratch1();
    check_type = CHECK_ALL_MAPS;
  }

  // Check that the maps starting from the prototype haven't changed.
  return CheckPrototypes(object_reg, scratch1(), scratch2(), scratch3(), name,
                         miss, check_type, return_what);
}


// Frontend for store uses the name register. It has to be restored before a
// miss.
Register NamedStoreHandlerCompiler::FrontendHeader(Register object_reg,
                                                   Handle<Name> name,
                                                   Label* miss,
                                                   ReturnHolder return_what) {
  return CheckPrototypes(object_reg, this->name(), scratch1(), scratch2(), name,
                         miss, SKIP_RECEIVER, return_what);
}


Register PropertyHandlerCompiler::Frontend(Handle<Name> name) {
  Label miss;
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    PushVectorAndSlot();
  }
  Register reg = FrontendHeader(receiver(), name, &miss, RETURN_HOLDER);
  FrontendFooter(name, &miss);
  // The footer consumes the vector and slot from the stack if miss occurs.
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    DiscardVectorAndSlot();
  }
  return reg;
}


void PropertyHandlerCompiler::NonexistentFrontendHeader(Handle<Name> name,
                                                        Label* miss,
                                                        Register scratch1,
                                                        Register scratch2) {
  Register holder_reg;
  Handle<Map> last_map;
  if (holder().is_null()) {
    holder_reg = receiver();
    last_map = map();
    // If |type| has null as its prototype, |holder()| is
    // Handle<JSObject>::null().
    DCHECK(last_map->prototype() == isolate()->heap()->null_value());
  } else {
    last_map = handle(holder()->map());
    // This condition matches the branches below.
    bool need_holder =
        last_map->is_dictionary_map() && !last_map->IsJSGlobalObjectMap();
    holder_reg =
        FrontendHeader(receiver(), name, miss,
                       need_holder ? RETURN_HOLDER : DONT_RETURN_ANYTHING);
  }

  if (last_map->is_dictionary_map()) {
    if (last_map->IsJSGlobalObjectMap()) {
      Handle<JSGlobalObject> global =
          holder().is_null()
              ? Handle<JSGlobalObject>::cast(isolate()->global_object())
              : Handle<JSGlobalObject>::cast(holder());
      GenerateCheckPropertyCell(masm(), global, name, scratch1, miss);
    } else {
      if (!name->IsUniqueName()) {
        DCHECK(name->IsString());
        name = factory()->InternalizeString(Handle<String>::cast(name));
      }
      DCHECK(holder().is_null() ||
             holder()->property_dictionary()->FindEntry(name) ==
                 NameDictionary::kNotFound);
      GenerateDictionaryNegativeLookup(masm(), miss, holder_reg, name, scratch1,
                                       scratch2);
    }
  }
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadField(Handle<Name> name,
                                                        FieldIndex field) {
  Register reg = Frontend(name);
  __ Move(receiver(), reg);
  LoadFieldStub stub(isolate(), field);
  GenerateTailCall(masm(), stub.GetCode());
  return GetCode(kind(), name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadConstant(Handle<Name> name,
                                                           int constant_index) {
  Register reg = Frontend(name);
  __ Move(receiver(), reg);
  LoadConstantStub stub(isolate(), constant_index);
  GenerateTailCall(masm(), stub.GetCode());
  return GetCode(kind(), name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadNonexistent(
    Handle<Name> name) {
  Label miss;
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    DCHECK(kind() == Code::LOAD_IC);
    PushVectorAndSlot();
  }
  NonexistentFrontendHeader(name, &miss, scratch2(), scratch3());
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    DiscardVectorAndSlot();
  }
  GenerateLoadConstant(isolate()->factory()->undefined_value());
  FrontendFooter(name, &miss);
  return GetCode(kind(), name);
}

Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, Handle<AccessorInfo> callback, Handle<Code> slow_stub) {
  if (FLAG_runtime_call_stats) {
    GenerateTailCall(masm(), slow_stub);
  }
  Register reg = Frontend(name);
  GenerateLoadCallback(reg, callback);
  return GetCode(kind(), name);
}

Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, const CallOptimization& call_optimization,
    int accessor_index, Handle<Code> slow_stub) {
  DCHECK(call_optimization.is_simple_api_call());
  if (FLAG_runtime_call_stats) {
    GenerateTailCall(masm(), slow_stub);
  }
  Register holder = Frontend(name);
  GenerateApiAccessorCall(masm(), call_optimization, map(), receiver(),
                          scratch2(), false, no_reg, holder, accessor_index);
  return GetCode(kind(), name);
}


void NamedLoadHandlerCompiler::InterceptorVectorSlotPush(Register holder_reg) {
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    if (holder_reg.is(receiver())) {
      PushVectorAndSlot();
    } else {
      DCHECK(holder_reg.is(scratch1()));
      PushVectorAndSlot(scratch2(), scratch3());
    }
  }
}


void NamedLoadHandlerCompiler::InterceptorVectorSlotPop(Register holder_reg,
                                                        PopMode mode) {
  if (IC::ShouldPushPopSlotAndVector(kind())) {
    if (mode == DISCARD) {
      DiscardVectorAndSlot();
    } else {
      if (holder_reg.is(receiver())) {
        PopVectorAndSlot();
      } else {
        DCHECK(holder_reg.is(scratch1()));
        PopVectorAndSlot(scratch2(), scratch3());
      }
    }
  }
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadInterceptor(
    LookupIterator* it) {
  // So far the most popular follow ups for interceptor loads are DATA and
  // AccessorInfo, so inline only them. Other cases may be added
  // later.
  bool inline_followup = false;
  switch (it->state()) {
    case LookupIterator::TRANSITION:
      UNREACHABLE();
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
      break;
    case LookupIterator::DATA:
      inline_followup =
          it->property_details().type() == DATA && !it->is_dictionary_holder();
      break;
    case LookupIterator::ACCESSOR: {
      Handle<Object> accessors = it->GetAccessors();
      if (accessors->IsAccessorInfo()) {
        Handle<AccessorInfo> info = Handle<AccessorInfo>::cast(accessors);
        inline_followup =
            info->getter() != NULL &&
            AccessorInfo::IsCompatibleReceiverMap(isolate(), info, map());
      } else if (accessors->IsAccessorPair()) {
        Handle<JSObject> property_holder(it->GetHolder<JSObject>());
        Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                              isolate());
        if (!(getter->IsJSFunction() || getter->IsFunctionTemplateInfo())) {
          break;
        }
        if (!property_holder->HasFastProperties()) break;
        CallOptimization call_optimization(getter);
        Handle<Map> receiver_map = map();
        inline_followup = call_optimization.is_simple_api_call() &&
                          call_optimization.IsCompatibleReceiverMap(
                              receiver_map, property_holder);
      }
    }
  }

  Label miss;
  InterceptorVectorSlotPush(receiver());
  bool lost_holder_register = false;
  auto holder_orig = holder();
  // non masking interceptors must check the entire chain, so temporarily reset
  // the holder to be that last element for the FrontendHeader call.
  if (holder()->GetNamedInterceptor()->non_masking()) {
    DCHECK(!inline_followup);
    JSObject* last = *holder();
    PrototypeIterator iter(isolate(), last);
    while (!iter.IsAtEnd()) {
      lost_holder_register = true;
      // Casting to JSObject is fine here. The LookupIterator makes sure to
      // look behind non-masking interceptors during the original lookup, and
      // we wouldn't try to compile a handler if there was a Proxy anywhere.
      last = iter.GetCurrent<JSObject>();
      iter.Advance();
    }
    auto last_handle = handle(last);
    set_holder(last_handle);
  }
  Register reg = FrontendHeader(receiver(), it->name(), &miss, RETURN_HOLDER);
  // Reset the holder so further calculations are correct.
  set_holder(holder_orig);
  if (lost_holder_register) {
    if (*it->GetReceiver() == *holder()) {
      reg = receiver();
    } else {
      // Reload lost holder register.
      auto cell = isolate()->factory()->NewWeakCell(holder());
      __ LoadWeakValue(reg, cell, &miss);
    }
  }
  FrontendFooter(it->name(), &miss);
  InterceptorVectorSlotPop(reg);
  if (inline_followup) {
    // TODO(368): Compile in the whole chain: all the interceptors in
    // prototypes and ultimate answer.
    GenerateLoadInterceptorWithFollowup(it, reg);
  } else {
    GenerateLoadInterceptor(reg);
  }
  return GetCode(kind(), it->name());
}

void NamedLoadHandlerCompiler::GenerateLoadCallback(
    Register reg, Handle<AccessorInfo> callback) {
  DCHECK(receiver().is(ApiGetterDescriptor::ReceiverRegister()));
  __ Move(ApiGetterDescriptor::HolderRegister(), reg);
  // The callback is alive if this instruction is executed,
  // so the weak cell is not cleared and points to data.
  Handle<WeakCell> cell = isolate()->factory()->NewWeakCell(callback);
  __ GetWeakValue(ApiGetterDescriptor::CallbackRegister(), cell);

  CallApiGetterStub stub(isolate());
  __ TailCallStub(&stub);
}

void NamedLoadHandlerCompiler::GenerateLoadPostInterceptor(
    LookupIterator* it, Register interceptor_reg) {
  Handle<JSObject> real_named_property_holder(it->GetHolder<JSObject>());

  Handle<Map> holder_map(holder()->map());
  set_map(holder_map);
  set_holder(real_named_property_holder);

  Label miss;
  InterceptorVectorSlotPush(interceptor_reg);
  Register reg =
      FrontendHeader(interceptor_reg, it->name(), &miss, RETURN_HOLDER);
  FrontendFooter(it->name(), &miss);
  // We discard the vector and slot now because we don't miss below this point.
  InterceptorVectorSlotPop(reg, DISCARD);

  switch (it->state()) {
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::INTEGER_INDEXED_EXOTIC:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
    case LookupIterator::DATA: {
      DCHECK_EQ(DATA, it->property_details().type());
      __ Move(receiver(), reg);
      LoadFieldStub stub(isolate(), it->GetFieldIndex());
      GenerateTailCall(masm(), stub.GetCode());
      break;
    }
    case LookupIterator::ACCESSOR:
      if (it->GetAccessors()->IsAccessorInfo()) {
        Handle<AccessorInfo> info =
            Handle<AccessorInfo>::cast(it->GetAccessors());
        DCHECK_NOT_NULL(info->getter());
        GenerateLoadCallback(reg, info);
      } else {
        Handle<Object> function = handle(
            AccessorPair::cast(*it->GetAccessors())->getter(), isolate());
        CallOptimization call_optimization(function);
        GenerateApiAccessorCall(masm(), call_optimization, holder_map,
                                receiver(), scratch2(), false, no_reg, reg,
                                it->GetAccessorIndex());
      }
  }
}

Handle<Code> NamedLoadHandlerCompiler::CompileLoadViaGetter(
    Handle<Name> name, int accessor_index, int expected_arguments) {
  Register holder = Frontend(name);
  GenerateLoadViaGetter(masm(), map(), receiver(), holder, accessor_index,
                        expected_arguments, scratch2());
  return GetCode(kind(), name);
}


// TODO(verwaest): Cleanup. holder() is actually the receiver.
Handle<Code> NamedStoreHandlerCompiler::CompileStoreTransition(
    Handle<Map> transition, Handle<Name> name) {
  Label miss;

  // Ensure that the StoreTransitionStub we are going to call has the same
  // number of stack arguments. This means that we don't have to adapt them
  // if we decide to call the transition or miss stub.
  STATIC_ASSERT(Descriptor::kStackArgumentsCount ==
                StoreTransitionDescriptor::kStackArgumentsCount);
  STATIC_ASSERT(Descriptor::kStackArgumentsCount == 0 ||
                Descriptor::kStackArgumentsCount == 3);
  STATIC_ASSERT(Descriptor::kParameterCount - Descriptor::kValue ==
                StoreTransitionDescriptor::kParameterCount -
                    StoreTransitionDescriptor::kValue);
  STATIC_ASSERT(Descriptor::kParameterCount - Descriptor::kSlot ==
                StoreTransitionDescriptor::kParameterCount -
                    StoreTransitionDescriptor::kSlot);
  STATIC_ASSERT(Descriptor::kParameterCount - Descriptor::kVector ==
                StoreTransitionDescriptor::kParameterCount -
                    StoreTransitionDescriptor::kVector);

  if (Descriptor::kPassLastArgsOnStack) {
    __ LoadParameterFromStack<Descriptor>(value(), Descriptor::kValue);
  }

  bool need_save_restore = IC::ShouldPushPopSlotAndVector(kind());
  if (need_save_restore) {
    PushVectorAndSlot();
  }

  // Check that we are allowed to write this.
  bool is_nonexistent = holder()->map() == transition->GetBackPointer();
  if (is_nonexistent) {
    // Find the top object.
    Handle<JSObject> last;
    PrototypeIterator::WhereToEnd end =
        name->IsPrivate() ? PrototypeIterator::END_AT_NON_HIDDEN
                          : PrototypeIterator::END_AT_NULL;
    PrototypeIterator iter(isolate(), holder(), kStartAtPrototype, end);
    while (!iter.IsAtEnd()) {
      last = PrototypeIterator::GetCurrent<JSObject>(iter);
      iter.Advance();
    }
    if (!last.is_null()) set_holder(last);
    NonexistentFrontendHeader(name, &miss, scratch1(), scratch2());
  } else {
    FrontendHeader(receiver(), name, &miss, DONT_RETURN_ANYTHING);
    DCHECK(holder()->HasFastProperties());
  }

  int descriptor = transition->LastAdded();
  Handle<DescriptorArray> descriptors(transition->instance_descriptors());
  PropertyDetails details = descriptors->GetDetails(descriptor);
  Representation representation = details.representation();
  DCHECK(!representation.IsNone());

  // Stub is never generated for objects that require access checks.
  DCHECK(!transition->is_access_check_needed());

  // Call to respective StoreTransitionStub.
  Register map_reg = StoreTransitionDescriptor::MapRegister();

  if (details.type() == DATA_CONSTANT) {
    DCHECK(descriptors->GetValue(descriptor)->IsJSFunction());
    GenerateRestoreMap(transition, map_reg, scratch1(), &miss);
    GenerateConstantCheck(map_reg, descriptor, value(), scratch1(), &miss);
    if (need_save_restore) {
      PopVectorAndSlot();
    }
    GenerateRestoreName(name);
    StoreMapStub stub(isolate());
    GenerateTailCall(masm(), stub.GetCode());

  } else {
    if (representation.IsHeapObject()) {
      GenerateFieldTypeChecks(descriptors->GetFieldType(descriptor), value(),
                              &miss);
    }
    StoreTransitionStub::StoreMode store_mode =
        Map::cast(transition->GetBackPointer())->unused_property_fields() == 0
            ? StoreTransitionStub::ExtendStorageAndStoreMapAndValue
            : StoreTransitionStub::StoreMapAndValue;
    GenerateRestoreMap(transition, map_reg, scratch1(), &miss);
    if (need_save_restore) {
      PopVectorAndSlot();
    }
    // We need to pass name on the stack.
    PopReturnAddress(this->name());
    __ Push(name);
    PushReturnAddress(this->name());

    FieldIndex index = FieldIndex::ForDescriptor(*transition, descriptor);
    __ Move(StoreNamedTransitionDescriptor::FieldOffsetRegister(),
            Smi::FromInt(index.index() << kPointerSizeLog2));

    StoreTransitionStub stub(isolate(), index.is_inobject(), representation,
                             store_mode);
    GenerateTailCall(masm(), stub.GetCode());
  }

  __ bind(&miss);
  if (need_save_restore) {
    PopVectorAndSlot();
  }
  GenerateRestoreName(name);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  return GetCode(kind(), name);
}

bool NamedStoreHandlerCompiler::RequiresFieldTypeChecks(
    FieldType* field_type) const {
  return field_type->IsClass();
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreField(LookupIterator* it) {
  Label miss;
  DCHECK(it->representation().IsHeapObject());

  FieldType* field_type = *it->GetFieldType();
  bool need_save_restore = false;
  if (RequiresFieldTypeChecks(field_type)) {
    need_save_restore = IC::ShouldPushPopSlotAndVector(kind());
    if (Descriptor::kPassLastArgsOnStack) {
      __ LoadParameterFromStack<Descriptor>(value(), Descriptor::kValue);
    }
    if (need_save_restore) PushVectorAndSlot();
    GenerateFieldTypeChecks(field_type, value(), &miss);
    if (need_save_restore) PopVectorAndSlot();
  }

  StoreFieldStub stub(isolate(), it->GetFieldIndex(), it->representation());
  GenerateTailCall(masm(), stub.GetCode());

  __ bind(&miss);
  if (need_save_restore) PopVectorAndSlot();
  TailCallBuiltin(masm(), MissBuiltin(kind()));
  return GetCode(kind(), it->name());
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreViaSetter(
    Handle<JSObject> object, Handle<Name> name, int accessor_index,
    int expected_arguments) {
  Register holder = Frontend(name);
  GenerateStoreViaSetter(masm(), map(), receiver(), holder, accessor_index,
                         expected_arguments, scratch2());

  return GetCode(kind(), name);
}

Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    const CallOptimization& call_optimization, int accessor_index,
    Handle<Code> slow_stub) {
  if (FLAG_runtime_call_stats) {
    GenerateTailCall(masm(), slow_stub);
  }
  Register holder = Frontend(name);
  if (Descriptor::kPassLastArgsOnStack) {
    __ LoadParameterFromStack<Descriptor>(value(), Descriptor::kValue);
  }
  GenerateApiAccessorCall(masm(), call_optimization, handle(object->map()),
                          receiver(), scratch2(), true, value(), holder,
                          accessor_index);
  return GetCode(kind(), name);
}


#undef __

// static
Handle<Object> ElementHandlerCompiler::GetKeyedLoadHandler(
    Handle<Map> receiver_map, Isolate* isolate) {
  if (receiver_map->has_indexed_interceptor() &&
      !receiver_map->GetIndexedInterceptor()->getter()->IsUndefined(isolate) &&
      !receiver_map->GetIndexedInterceptor()->non_masking()) {
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_LoadIndexedInterceptorStub);
    return LoadIndexedInterceptorStub(isolate).GetCode();
  }
  if (receiver_map->IsStringMap()) {
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_LoadIndexedStringStub);
    return LoadIndexedStringStub(isolate).GetCode();
  }
  InstanceType instance_type = receiver_map->instance_type();
  if (instance_type < FIRST_JS_RECEIVER_TYPE) {
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_SlowStub);
    return isolate->builtins()->KeyedLoadIC_Slow();
  }

  ElementsKind elements_kind = receiver_map->elements_kind();
  if (IsSloppyArgumentsElements(elements_kind)) {
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_KeyedLoadSloppyArgumentsStub);
    return KeyedLoadSloppyArgumentsStub(isolate).GetCode();
  }
  bool is_js_array = instance_type == JS_ARRAY_TYPE;
  if (elements_kind == DICTIONARY_ELEMENTS) {
    if (FLAG_tf_load_ic_stub) {
      int config = KeyedLoadElementsKind::encode(elements_kind) |
                   KeyedLoadConvertHole::encode(false) |
                   KeyedLoadIsJsArray::encode(is_js_array) |
                   LoadHandlerTypeBit::encode(kLoadICHandlerForElements);
      return handle(Smi::FromInt(config), isolate);
    }
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_LoadDictionaryElementStub);
    return LoadDictionaryElementStub(isolate).GetCode();
  }
  DCHECK(IsFastElementsKind(elements_kind) ||
         IsFixedTypedArrayElementsKind(elements_kind));
  // TODO(jkummerow): Use IsHoleyElementsKind(elements_kind).
  bool convert_hole_to_undefined =
      is_js_array && elements_kind == FAST_HOLEY_ELEMENTS &&
      *receiver_map == isolate->get_initial_js_array_map(elements_kind);
  if (FLAG_tf_load_ic_stub) {
    int config = KeyedLoadElementsKind::encode(elements_kind) |
                 KeyedLoadConvertHole::encode(convert_hole_to_undefined) |
                 KeyedLoadIsJsArray::encode(is_js_array) |
                 LoadHandlerTypeBit::encode(kLoadICHandlerForElements);
    return handle(Smi::FromInt(config), isolate);
  } else {
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_LoadFastElementStub);
    return LoadFastElementStub(isolate, is_js_array, elements_kind,
                               convert_hole_to_undefined)
        .GetCode();
  }
}

void ElementHandlerCompiler::CompileElementHandlers(
    MapHandleList* receiver_maps, List<Handle<Object>>* handlers) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    handlers->Add(GetKeyedLoadHandler(receiver_maps->at(i), isolate()));
  }
}
}  // namespace internal
}  // namespace v8
