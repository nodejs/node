// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/cpu-profiler.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-compiler.h"
#include "src/ic/ic.h"
#include "src/ic/ic-inl.h"

namespace v8 {
namespace internal {


Handle<Code> PropertyHandlerCompiler::Find(Handle<Name> name,
                                           Handle<Map> stub_holder,
                                           Code::Kind kind,
                                           CacheHolderFlag cache_holder,
                                           Code::StubType type) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, type, cache_holder);
  Object* probe = stub_holder->FindInCodeCache(*name, flags);
  if (probe->IsCode()) return handle(Code::cast(probe));
  return Handle<Code>::null();
}


Handle<Code> NamedLoadHandlerCompiler::ComputeLoadNonexistent(
    Handle<Name> name, Handle<Map> receiver_map) {
  Isolate* isolate = name->GetIsolate();
  if (receiver_map->prototype()->IsNull()) {
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
    if (current_map->prototype()->IsNull()) break;
    last = handle(JSObject::cast(current_map->prototype()));
    current_map = handle(last->map());
  }
  // Compile the stub that is either shared for all names or
  // name specific if there are global objects involved.
  Handle<Code> handler = PropertyHandlerCompiler::Find(
      cache_name, stub_holder_map, Code::LOAD_IC, flag, Code::FAST);
  if (!handler.is_null()) return handler;

  NamedLoadHandlerCompiler compiler(isolate, receiver_map, last, flag);
  handler = compiler.CompileLoadNonexistent(cache_name);
  Map::UpdateCodeCache(stub_holder_map, cache_name, handler);
  return handler;
}


Handle<Code> PropertyHandlerCompiler::GetCode(Code::Kind kind,
                                              Code::StubType type,
                                              Handle<Name> name) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, type, cache_holder());
  Handle<Code> code = GetCodeWithFlags(flags, name);
  PROFILE(isolate(), CodeCreateEvent(Logger::HANDLER_TAG, *code, *name));
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
  PrototypeCheckType check_type = CHECK_ALL_MAPS;
  int function_index = -1;
  if (map()->instance_type() < FIRST_NONSTRING_TYPE) {
    function_index = Context::STRING_FUNCTION_INDEX;
  } else if (map()->instance_type() == SYMBOL_TYPE) {
    function_index = Context::SYMBOL_FUNCTION_INDEX;
  } else if (map()->instance_type() == HEAP_NUMBER_TYPE) {
    function_index = Context::NUMBER_FUNCTION_INDEX;
  } else if (*map() == isolate()->heap()->boolean_map()) {
    function_index = Context::BOOLEAN_FUNCTION_INDEX;
  } else {
    check_type = SKIP_RECEIVER;
  }

  if (check_type == CHECK_ALL_MAPS) {
    GenerateDirectLoadGlobalFunctionPrototype(masm(), function_index,
                                              scratch1(), miss);
    Object* function = isolate()->native_context()->get(function_index);
    Object* prototype = JSFunction::cast(function)->instance_prototype();
    Handle<Map> map(JSObject::cast(prototype)->map());
    set_map(map);
    object_reg = scratch1();
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
  if (IC::ICUseVector(kind())) {
    PushVectorAndSlot();
  }
  Register reg = FrontendHeader(receiver(), name, &miss, RETURN_HOLDER);
  FrontendFooter(name, &miss);
  // The footer consumes the vector and slot from the stack if miss occurs.
  if (IC::ICUseVector(kind())) {
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
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadConstant(Handle<Name> name,
                                                           int constant_index) {
  Register reg = Frontend(name);
  __ Move(receiver(), reg);
  LoadConstantStub stub(isolate(), constant_index);
  GenerateTailCall(masm(), stub.GetCode());
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadNonexistent(
    Handle<Name> name) {
  Label miss;
  if (IC::ICUseVector(kind())) {
    DCHECK(kind() == Code::LOAD_IC);
    PushVectorAndSlot();
  }
  NonexistentFrontendHeader(name, &miss, scratch2(), scratch3());
  if (IC::ICUseVector(kind())) {
    DiscardVectorAndSlot();
  }
  GenerateLoadConstant(isolate()->factory()->undefined_value());
  FrontendFooter(name, &miss);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, Handle<ExecutableAccessorInfo> callback) {
  Register reg = Frontend(name);
  GenerateLoadCallback(reg, callback);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, const CallOptimization& call_optimization,
    int accessor_index) {
  DCHECK(call_optimization.is_simple_api_call());
  Register holder = Frontend(name);
  GenerateApiAccessorCall(masm(), call_optimization, map(), receiver(),
                          scratch2(), false, no_reg, holder, accessor_index);
  return GetCode(kind(), Code::FAST, name);
}


void NamedLoadHandlerCompiler::InterceptorVectorSlotPush(Register holder_reg) {
  if (IC::ICUseVector(kind())) {
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
  if (IC::ICUseVector(kind())) {
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
  // ExecutableAccessorInfo, so inline only them. Other cases may be added
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
      if (accessors->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(accessors);
        inline_followup = info->getter() != NULL &&
                          ExecutableAccessorInfo::IsCompatibleReceiverMap(
                              isolate(), info, map());
      } else if (accessors->IsAccessorPair()) {
        Handle<JSObject> property_holder(it->GetHolder<JSObject>());
        Handle<Object> getter(Handle<AccessorPair>::cast(accessors)->getter(),
                              isolate());
        if (!getter->IsJSFunction()) break;
        if (!property_holder->HasFastProperties()) break;
        auto function = Handle<JSFunction>::cast(getter);
        CallOptimization call_optimization(function);
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
      last = JSObject::cast(iter.GetCurrent());
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
  return GetCode(kind(), Code::FAST, it->name());
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
      if (it->GetAccessors()->IsExecutableAccessorInfo()) {
        Handle<ExecutableAccessorInfo> info =
            Handle<ExecutableAccessorInfo>::cast(it->GetAccessors());
        DCHECK_NOT_NULL(info->getter());
        GenerateLoadCallback(reg, info);
      } else {
        auto function = handle(JSFunction::cast(
            AccessorPair::cast(*it->GetAccessors())->getter()));
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
  return GetCode(kind(), Code::FAST, name);
}


// TODO(verwaest): Cleanup. holder() is actually the receiver.
Handle<Code> NamedStoreHandlerCompiler::CompileStoreTransition(
    Handle<Map> transition, Handle<Name> name) {
  Label miss;

  // Check that we are allowed to write this.
  bool is_nonexistent = holder()->map() == transition->GetBackPointer();
  if (is_nonexistent) {
    // Find the top object.
    Handle<JSObject> last;
    PrototypeIterator iter(isolate(), holder());
    while (!iter.IsAtEnd()) {
      last = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
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
  if (details.type() == DATA_CONSTANT) {
    GenerateRestoreMap(transition, scratch2(), &miss);
    DCHECK(descriptors->GetValue(descriptor)->IsJSFunction());
    Register map_reg = StoreTransitionDescriptor::MapRegister();
    GenerateConstantCheck(map_reg, descriptor, value(), scratch2(), &miss);
    GenerateRestoreName(name);
    StoreTransitionStub stub(isolate());
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

    GenerateRestoreMap(transition, scratch2(), &miss);
    GenerateRestoreName(name);
    StoreTransitionStub stub(isolate(),
                             FieldIndex::ForDescriptor(*transition, descriptor),
                             representation, store_mode);
    GenerateTailCall(masm(), stub.GetCode());
  }

  GenerateRestoreName(&miss, name);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreField(LookupIterator* it) {
  Label miss;
  DCHECK(it->representation().IsHeapObject());

  GenerateFieldTypeChecks(*it->GetFieldType(), value(), &miss);
  StoreFieldStub stub(isolate(), it->GetFieldIndex(), it->representation());
  GenerateTailCall(masm(), stub.GetCode());

  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));
  return GetCode(kind(), Code::FAST, it->name());
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreViaSetter(
    Handle<JSObject> object, Handle<Name> name, int accessor_index,
    int expected_arguments) {
  Register holder = Frontend(name);
  GenerateStoreViaSetter(masm(), map(), receiver(), holder, accessor_index,
                         expected_arguments, scratch2());

  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    const CallOptimization& call_optimization, int accessor_index) {
  Register holder = Frontend(name);
  GenerateApiAccessorCall(masm(), call_optimization, handle(object->map()),
                          receiver(), scratch2(), true, value(), holder,
                          accessor_index);
  return GetCode(kind(), Code::FAST, name);
}


#undef __


void ElementHandlerCompiler::CompileElementHandlers(
    MapHandleList* receiver_maps, CodeHandleList* handlers) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<Map> receiver_map = receiver_maps->at(i);
    Handle<Code> cached_stub;

    if (receiver_map->IsStringMap()) {
      cached_stub = LoadIndexedStringStub(isolate()).GetCode();
    } else if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE) {
      cached_stub = isolate()->builtins()->KeyedLoadIC_Slow();
    } else {
      bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
      ElementsKind elements_kind = receiver_map->elements_kind();

      // No need to check for an elements-free prototype chain here, the
      // generated stub code needs to check that dynamically anyway.
      bool convert_hole_to_undefined =
          is_js_array && elements_kind == FAST_HOLEY_ELEMENTS &&
          *receiver_map == isolate()->get_initial_js_array_map(elements_kind);

      if (receiver_map->has_indexed_interceptor()) {
        cached_stub = LoadIndexedInterceptorStub(isolate()).GetCode();
      } else if (IsSloppyArgumentsElements(elements_kind)) {
        cached_stub = KeyedLoadSloppyArgumentsStub(isolate()).GetCode();
      } else if (IsFastElementsKind(elements_kind) ||
                 IsExternalArrayElementsKind(elements_kind) ||
                 IsFixedTypedArrayElementsKind(elements_kind)) {
        cached_stub = LoadFastElementStub(isolate(), is_js_array, elements_kind,
                                          convert_hole_to_undefined).GetCode();
      } else {
        DCHECK(elements_kind == DICTIONARY_ELEMENTS);
        cached_stub = LoadDictionaryElementStub(isolate()).GetCode();
      }
    }

    handlers->Add(cached_stub);
  }
}
}
}  // namespace v8::internal
