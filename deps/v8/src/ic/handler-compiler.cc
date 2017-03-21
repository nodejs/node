// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/ic/handler-compiler.h"

#include "src/field-type.h"
#include "src/ic/call-optimization.h"
#include "src/ic/handler-configuration-inl.h"
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
  if (map()->IsPrimitiveMap() || map()->IsJSGlobalProxyMap()) {
    // If the receiver is a global proxy and if we get to this point then
    // the compile-time (current) native context has access to global proxy's
    // native context. Since access rights revocation is not supported at all,
    // we can generate a check that an execution-time native context is either
    // the same as compile-time native context or has the same access token.
    Handle<Context> native_context = isolate()->native_context();
    Handle<WeakCell> weak_cell(native_context->self_weak_cell(), isolate());

    bool compare_native_contexts_only = map()->IsPrimitiveMap();
    GenerateAccessCheck(weak_cell, scratch1(), scratch2(), miss,
                        compare_native_contexts_only);
  }

  // Check that the maps starting from the prototype haven't changed.
  return CheckPrototypes(object_reg, scratch1(), scratch2(), scratch3(), name,
                         miss, return_what);
}


// Frontend for store uses the name register. It has to be restored before a
// miss.
Register NamedStoreHandlerCompiler::FrontendHeader(Register object_reg,
                                                   Handle<Name> name,
                                                   Label* miss,
                                                   ReturnHolder return_what) {
  if (map()->IsJSGlobalProxyMap()) {
    Handle<Context> native_context = isolate()->native_context();
    Handle<WeakCell> weak_cell(native_context->self_weak_cell(), isolate());
    GenerateAccessCheck(weak_cell, scratch1(), scratch2(), miss, false);
  }

  return CheckPrototypes(object_reg, this->name(), scratch1(), scratch2(), name,
                         miss, return_what);
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

Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, Handle<AccessorInfo> callback, Handle<Code> slow_stub) {
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
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
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
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
    case LookupIterator::DATA: {
      PropertyDetails details = it->property_details();
      inline_followup = details.kind() == kData &&
                        details.location() == kField &&
                        !it->is_dictionary_holder();
      break;
    }
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
      DCHECK_EQ(kData, it->property_details().kind());
      DCHECK_EQ(kField, it->property_details().location());
      __ Move(LoadFieldDescriptor::ReceiverRegister(), reg);
      Handle<Object> smi_handler =
          LoadIC::SimpleFieldLoad(isolate(), it->GetFieldIndex());
      __ Move(LoadFieldDescriptor::SmiHandlerRegister(), smi_handler);
      LoadFieldStub stub(isolate());
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
  if (V8_UNLIKELY(FLAG_runtime_stats)) {
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
    TRACE_HANDLER_STATS(isolate, KeyedLoadIC_LoadElementDH);
    return LoadHandler::LoadElement(isolate, elements_kind, false, is_js_array);
  }
  DCHECK(IsFastElementsKind(elements_kind) ||
         IsFixedTypedArrayElementsKind(elements_kind));
  // TODO(jkummerow): Use IsHoleyElementsKind(elements_kind).
  bool convert_hole_to_undefined =
      is_js_array && elements_kind == FAST_HOLEY_ELEMENTS &&
      *receiver_map == isolate->get_initial_js_array_map(elements_kind);
  TRACE_HANDLER_STATS(isolate, KeyedLoadIC_LoadElementDH);
  return LoadHandler::LoadElement(isolate, elements_kind,
                                  convert_hole_to_undefined, is_js_array);
}

void ElementHandlerCompiler::CompileElementHandlers(
    MapHandleList* receiver_maps, List<Handle<Object>>* handlers) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    handlers->Add(GetKeyedLoadHandler(receiver_maps->at(i), isolate()));
  }
}
}  // namespace internal
}  // namespace v8
