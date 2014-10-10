// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

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
    Handle<Name> name, Handle<HeapType> type) {
  Isolate* isolate = name->GetIsolate();
  Handle<Map> receiver_map = IC::TypeToMap(*type, isolate);
  if (receiver_map->prototype()->IsNull()) {
    // TODO(jkummerow/verwaest): If there is no prototype and the property
    // is nonexistent, introduce a builtin to handle this (fast properties
    // -> return undefined, dictionary properties -> do negative lookup).
    return Handle<Code>();
  }
  CacheHolderFlag flag;
  Handle<Map> stub_holder_map =
      IC::GetHandlerCacheHolder(*type, false, isolate, &flag);

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

  NamedLoadHandlerCompiler compiler(isolate, type, last, flag);
  handler = compiler.CompileLoadNonexistent(cache_name);
  Map::UpdateCodeCache(stub_holder_map, cache_name, handler);
  return handler;
}


Handle<Code> PropertyHandlerCompiler::GetCode(Code::Kind kind,
                                              Code::StubType type,
                                              Handle<Name> name) {
  Code::Flags flags = Code::ComputeHandlerFlags(kind, type, cache_holder());
  Handle<Code> code = GetCodeWithFlags(flags, name);
  PROFILE(isolate(), CodeCreateEvent(Logger::STUB_TAG, *code, *name));
  return code;
}


void PropertyHandlerCompiler::set_type_for_object(Handle<Object> object) {
  type_ = IC::CurrentTypeOf(object, isolate());
}


#define __ ACCESS_MASM(masm())


Register NamedLoadHandlerCompiler::FrontendHeader(Register object_reg,
                                                  Handle<Name> name,
                                                  Label* miss) {
  PrototypeCheckType check_type = CHECK_ALL_MAPS;
  int function_index = -1;
  if (type()->Is(HeapType::String())) {
    function_index = Context::STRING_FUNCTION_INDEX;
  } else if (type()->Is(HeapType::Symbol())) {
    function_index = Context::SYMBOL_FUNCTION_INDEX;
  } else if (type()->Is(HeapType::Number())) {
    function_index = Context::NUMBER_FUNCTION_INDEX;
  } else if (type()->Is(HeapType::Boolean())) {
    function_index = Context::BOOLEAN_FUNCTION_INDEX;
  } else {
    check_type = SKIP_RECEIVER;
  }

  if (check_type == CHECK_ALL_MAPS) {
    GenerateDirectLoadGlobalFunctionPrototype(masm(), function_index,
                                              scratch1(), miss);
    Object* function = isolate()->native_context()->get(function_index);
    Object* prototype = JSFunction::cast(function)->instance_prototype();
    set_type_for_object(handle(prototype, isolate()));
    object_reg = scratch1();
  }

  // Check that the maps starting from the prototype haven't changed.
  return CheckPrototypes(object_reg, scratch1(), scratch2(), scratch3(), name,
                         miss, check_type);
}


// Frontend for store uses the name register. It has to be restored before a
// miss.
Register NamedStoreHandlerCompiler::FrontendHeader(Register object_reg,
                                                   Handle<Name> name,
                                                   Label* miss) {
  return CheckPrototypes(object_reg, this->name(), scratch1(), scratch2(), name,
                         miss, SKIP_RECEIVER);
}


Register PropertyHandlerCompiler::Frontend(Register object_reg,
                                           Handle<Name> name) {
  Label miss;
  Register reg = FrontendHeader(object_reg, name, &miss);
  FrontendFooter(name, &miss);
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
    last_map = IC::TypeToMap(*type(), isolate());
    // If |type| has null as its prototype, |holder()| is
    // Handle<JSObject>::null().
    DCHECK(last_map->prototype() == isolate()->heap()->null_value());
  } else {
    holder_reg = FrontendHeader(receiver(), name, miss);
    last_map = handle(holder()->map());
  }

  if (last_map->is_dictionary_map()) {
    if (last_map->IsJSGlobalObjectMap()) {
      Handle<JSGlobalObject> global =
          holder().is_null()
              ? Handle<JSGlobalObject>::cast(type()->AsConstant()->Value())
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
  Register reg = Frontend(receiver(), name);
  __ Move(receiver(), reg);
  LoadFieldStub stub(isolate(), field);
  GenerateTailCall(masm(), stub.GetCode());
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadConstant(Handle<Name> name,
                                                           int constant_index) {
  Register reg = Frontend(receiver(), name);
  __ Move(receiver(), reg);
  LoadConstantStub stub(isolate(), constant_index);
  GenerateTailCall(masm(), stub.GetCode());
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadNonexistent(
    Handle<Name> name) {
  Label miss;
  NonexistentFrontendHeader(name, &miss, scratch2(), scratch3());
  GenerateLoadConstant(isolate()->factory()->undefined_value());
  FrontendFooter(name, &miss);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, Handle<ExecutableAccessorInfo> callback) {
  Register reg = Frontend(receiver(), name);
  GenerateLoadCallback(reg, callback);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadCallback(
    Handle<Name> name, const CallOptimization& call_optimization) {
  DCHECK(call_optimization.is_simple_api_call());
  Frontend(receiver(), name);
  Handle<Map> receiver_map = IC::TypeToMap(*type(), isolate());
  GenerateFastApiCall(masm(), call_optimization, receiver_map, receiver(),
                      scratch1(), false, 0, NULL);
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadInterceptor(
    LookupIterator* it) {
  // So far the most popular follow ups for interceptor loads are FIELD and
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
      break;
    case LookupIterator::DATA:
      inline_followup = it->property_details().type() == FIELD;
      break;
    case LookupIterator::ACCESSOR: {
      Handle<Object> accessors = it->GetAccessors();
      inline_followup = accessors->IsExecutableAccessorInfo();
      if (!inline_followup) break;
      Handle<ExecutableAccessorInfo> info =
          Handle<ExecutableAccessorInfo>::cast(accessors);
      inline_followup = info->getter() != NULL &&
                        ExecutableAccessorInfo::IsCompatibleReceiverType(
                            isolate(), info, type());
    }
  }

  Register reg = Frontend(receiver(), it->name());
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

  set_type_for_object(holder());
  set_holder(real_named_property_holder);
  Register reg = Frontend(interceptor_reg, it->name());

  switch (it->state()) {
    case LookupIterator::ACCESS_CHECK:
    case LookupIterator::INTERCEPTOR:
    case LookupIterator::JSPROXY:
    case LookupIterator::NOT_FOUND:
    case LookupIterator::TRANSITION:
      UNREACHABLE();
    case LookupIterator::DATA: {
      DCHECK_EQ(FIELD, it->property_details().type());
      __ Move(receiver(), reg);
      LoadFieldStub stub(isolate(), it->GetFieldIndex());
      GenerateTailCall(masm(), stub.GetCode());
      break;
    }
    case LookupIterator::ACCESSOR:
      Handle<ExecutableAccessorInfo> info =
          Handle<ExecutableAccessorInfo>::cast(it->GetAccessors());
      DCHECK_NE(NULL, info->getter());
      GenerateLoadCallback(reg, info);
  }
}


Handle<Code> NamedLoadHandlerCompiler::CompileLoadViaGetter(
    Handle<Name> name, Handle<JSFunction> getter) {
  Frontend(receiver(), name);
  GenerateLoadViaGetter(masm(), type(), receiver(), getter);
  return GetCode(kind(), Code::FAST, name);
}


// TODO(verwaest): Cleanup. holder() is actually the receiver.
Handle<Code> NamedStoreHandlerCompiler::CompileStoreTransition(
    Handle<Map> transition, Handle<Name> name) {
  Label miss, slow;

  // Ensure no transitions to deprecated maps are followed.
  __ CheckMapDeprecated(transition, scratch1(), &miss);

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
    FrontendHeader(receiver(), name, &miss);
    DCHECK(holder()->HasFastProperties());
  }

  GenerateStoreTransition(transition, name, receiver(), this->name(), value(),
                          scratch1(), scratch2(), scratch3(), &miss, &slow);

  GenerateRestoreName(&miss, name);
  TailCallBuiltin(masm(), MissBuiltin(kind()));

  GenerateRestoreName(&slow, name);
  TailCallBuiltin(masm(), SlowBuiltin(kind()));
  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreField(LookupIterator* it) {
  Label miss;
  GenerateStoreField(it, value(), &miss);
  __ bind(&miss);
  TailCallBuiltin(masm(), MissBuiltin(kind()));
  return GetCode(kind(), Code::FAST, it->name());
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreViaSetter(
    Handle<JSObject> object, Handle<Name> name, Handle<JSFunction> setter) {
  Frontend(receiver(), name);
  GenerateStoreViaSetter(masm(), type(), receiver(), setter);

  return GetCode(kind(), Code::FAST, name);
}


Handle<Code> NamedStoreHandlerCompiler::CompileStoreCallback(
    Handle<JSObject> object, Handle<Name> name,
    const CallOptimization& call_optimization) {
  Frontend(receiver(), name);
  Register values[] = {value()};
  GenerateFastApiCall(masm(), call_optimization, handle(object->map()),
                      receiver(), scratch1(), true, 1, values);
  return GetCode(kind(), Code::FAST, name);
}


#undef __


void ElementHandlerCompiler::CompileElementHandlers(
    MapHandleList* receiver_maps, CodeHandleList* handlers) {
  for (int i = 0; i < receiver_maps->length(); ++i) {
    Handle<Map> receiver_map = receiver_maps->at(i);
    Handle<Code> cached_stub;

    if ((receiver_map->instance_type() & kNotStringTag) == 0) {
      cached_stub = isolate()->builtins()->KeyedLoadIC_String();
    } else if (receiver_map->instance_type() < FIRST_JS_RECEIVER_TYPE) {
      cached_stub = isolate()->builtins()->KeyedLoadIC_Slow();
    } else {
      bool is_js_array = receiver_map->instance_type() == JS_ARRAY_TYPE;
      ElementsKind elements_kind = receiver_map->elements_kind();
      if (receiver_map->has_indexed_interceptor()) {
        cached_stub = LoadIndexedInterceptorStub(isolate()).GetCode();
      } else if (IsSloppyArgumentsElements(elements_kind)) {
        cached_stub = KeyedLoadSloppyArgumentsStub(isolate()).GetCode();
      } else if (IsFastElementsKind(elements_kind) ||
                 IsExternalArrayElementsKind(elements_kind) ||
                 IsFixedTypedArrayElementsKind(elements_kind)) {
        cached_stub = LoadFastElementStub(isolate(), is_js_array, elements_kind)
                          .GetCode();
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
