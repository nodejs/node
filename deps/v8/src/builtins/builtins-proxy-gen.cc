// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-proxy-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"

#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

void ProxiesCodeStubAssembler::GotoIfRevokedProxy(Node* object,
                                                  Label* if_proxy_revoked) {
  Label proxy_not_revoked(this);
  GotoIfNot(IsJSProxy(object), &proxy_not_revoked);
  Branch(IsJSReceiver(LoadObjectField(object, JSProxy::kHandlerOffset)),
         &proxy_not_revoked, if_proxy_revoked);
  BIND(&proxy_not_revoked);
}

Node* ProxiesCodeStubAssembler::AllocateProxy(Node* target, Node* handler,
                                              Node* context) {
  VARIABLE(map, MachineRepresentation::kTagged);

  Label callable_target(this), constructor_target(this), none_target(this),
      create_proxy(this);

  Node* nativeContext = LoadNativeContext(context);

  Branch(IsCallable(target), &callable_target, &none_target);

  BIND(&callable_target);
  {
    // Every object that is a constructor is implicitly callable
    // so it's okay to nest this check here
    GotoIf(IsConstructor(target), &constructor_target);
    map.Bind(
        LoadContextElement(nativeContext, Context::PROXY_CALLABLE_MAP_INDEX));
    Goto(&create_proxy);
  }
  BIND(&constructor_target);
  {
    map.Bind(LoadContextElement(nativeContext,
                                Context::PROXY_CONSTRUCTOR_MAP_INDEX));
    Goto(&create_proxy);
  }
  BIND(&none_target);
  {
    map.Bind(LoadContextElement(nativeContext, Context::PROXY_MAP_INDEX));
    Goto(&create_proxy);
  }

  BIND(&create_proxy);
  Node* proxy = Allocate(JSProxy::kSize);
  StoreMapNoWriteBarrier(proxy, map.value());
  StoreObjectFieldRoot(proxy, JSProxy::kPropertiesOrHashOffset,
                       Heap::kEmptyPropertyDictionaryRootIndex);
  StoreObjectFieldNoWriteBarrier(proxy, JSProxy::kTargetOffset, target);
  StoreObjectFieldNoWriteBarrier(proxy, JSProxy::kHandlerOffset, handler);

  return proxy;
}

Node* ProxiesCodeStubAssembler::AllocateJSArrayForCodeStubArguments(
    Node* context, CodeStubArguments& args, Node* argc, ParameterMode mode) {
  Comment("AllocateJSArrayForCodeStubArguments");

  Label if_empty_array(this), allocate_js_array(this);
  // Do not use AllocateJSArray since {elements} might end up in LOS.
  VARIABLE(elements, MachineRepresentation::kTagged);

  TNode<Smi> length = ParameterToTagged(argc, mode);
  GotoIf(SmiEqual(length, SmiConstant(0)), &if_empty_array);
  {
    Label if_large_object(this, Label::kDeferred);
    Node* allocated_elements = AllocateFixedArray(PACKED_ELEMENTS, argc, mode,
                                                  kAllowLargeObjectAllocation);
    elements.Bind(allocated_elements);

    VARIABLE(index, MachineType::PointerRepresentation(),
             IntPtrConstant(FixedArrayBase::kHeaderSize - kHeapObjectTag));
    VariableList list({&index}, zone());

    GotoIf(SmiGreaterThan(length, SmiConstant(FixedArray::kMaxRegularLength)),
           &if_large_object);
    args.ForEach(list, [=, &index](Node* arg) {
      StoreNoWriteBarrier(MachineRepresentation::kTagged, allocated_elements,
                          index.value(), arg);
      Increment(&index, kPointerSize);
    });
    Goto(&allocate_js_array);

    BIND(&if_large_object);
    {
      args.ForEach(list, [=, &index](Node* arg) {
        Store(allocated_elements, index.value(), arg);
        Increment(&index, kPointerSize);
      });
      Goto(&allocate_js_array);
    }
  }

  BIND(&if_empty_array);
  {
    elements.Bind(EmptyFixedArrayConstant());
    Goto(&allocate_js_array);
  }

  BIND(&allocate_js_array);
  // Allocate the result JSArray.
  Node* native_context = LoadNativeContext(context);
  Node* array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
  Node* array = AllocateUninitializedJSArrayWithoutElements(array_map, length);
  StoreObjectFieldNoWriteBarrier(array, JSObject::kElementsOffset,
                                 elements.value());

  return array;
}

Node* ProxiesCodeStubAssembler::CreateProxyRevokeFunctionContext(
    Node* proxy, Node* native_context) {
  Node* const context = Allocate(FixedArray::SizeFor(kProxyContextLength));
  StoreMapNoWriteBarrier(context, Heap::kFunctionContextMapRootIndex);
  InitializeFunctionContext(native_context, context, kProxyContextLength);
  StoreContextElementNoWriteBarrier(context, kProxySlot, proxy);
  return context;
}

Node* ProxiesCodeStubAssembler::AllocateProxyRevokeFunction(Node* proxy,
                                                            Node* context) {
  Node* const native_context = LoadNativeContext(context);

  Node* const proxy_context =
      CreateProxyRevokeFunctionContext(proxy, native_context);
  Node* const revoke_map = LoadContextElement(
      native_context, Context::STRICT_FUNCTION_WITHOUT_PROTOTYPE_MAP_INDEX);
  Node* const revoke_info =
      LoadContextElement(native_context, Context::PROXY_REVOKE_SHARED_FUN);

  return AllocateFunctionWithMapAndContext(revoke_map, revoke_info,
                                           proxy_context);
}

// ES #sec-proxy-constructor
TF_BUILTIN(ProxyConstructor, ProxiesCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);

  // 1. If NewTarget is undefined, throw a TypeError exception.
  Node* new_target = Parameter(Descriptor::kNewTarget);
  Label throwtypeerror(this, Label::kDeferred), createproxy(this);
  Branch(IsUndefined(new_target), &throwtypeerror, &createproxy);

  BIND(&throwtypeerror);
  {
    ThrowTypeError(context, MessageTemplate::kConstructorNotFunction, "Proxy");
  }

  // 2. Return ? ProxyCreate(target, handler).
  BIND(&createproxy);
  {
    // https://tc39.github.io/ecma262/#sec-proxycreate
    Node* target = Parameter(Descriptor::kTarget);
    Node* handler = Parameter(Descriptor::kHandler);

    // 1. If Type(target) is not Object, throw a TypeError exception.
    // 2. If target is a Proxy exotic object and target.[[ProxyHandler]] is
    //    null, throw a TypeError exception.
    // 3. If Type(handler) is not Object, throw a TypeError exception.
    // 4. If handler is a Proxy exotic object and handler.[[ProxyHandler]]
    //    is null, throw a TypeError exception.
    Label throw_proxy_non_object(this, Label::kDeferred),
        throw_proxy_handler_or_target_revoked(this, Label::kDeferred),
        return_create_proxy(this);

    GotoIf(TaggedIsSmi(target), &throw_proxy_non_object);
    GotoIfNot(IsJSReceiver(target), &throw_proxy_non_object);
    GotoIfRevokedProxy(target, &throw_proxy_handler_or_target_revoked);

    GotoIf(TaggedIsSmi(handler), &throw_proxy_non_object);
    GotoIfNot(IsJSReceiver(handler), &throw_proxy_non_object);
    GotoIfRevokedProxy(handler, &throw_proxy_handler_or_target_revoked);

    // 5. Let P be a newly created object.
    // 6. Set P's essential internal methods (except for [[Call]] and
    //    [[Construct]]) to the definitions specified in 9.5.
    // 7. If IsCallable(target) is true, then
    //    a. Set P.[[Call]] as specified in 9.5.12.
    //    b. If IsConstructor(target) is true, then
    //       1. Set P.[[Construct]] as specified in 9.5.13.
    // 8. Set P.[[ProxyTarget]] to target.
    // 9. Set P.[[ProxyHandler]] to handler.
    // 10. Return P.
    Return(AllocateProxy(target, handler, context));

    BIND(&throw_proxy_non_object);
    ThrowTypeError(context, MessageTemplate::kProxyNonObject);

    BIND(&throw_proxy_handler_or_target_revoked);
    ThrowTypeError(context, MessageTemplate::kProxyHandlerOrTargetRevoked);
  }
}

TF_BUILTIN(ProxyRevocable, ProxiesCodeStubAssembler) {
  Node* const target = Parameter(Descriptor::kTarget);
  Node* const handler = Parameter(Descriptor::kHandler);
  Node* const context = Parameter(Descriptor::kContext);
  Node* const native_context = LoadNativeContext(context);

  Label throw_proxy_non_object(this, Label::kDeferred),
      throw_proxy_handler_or_target_revoked(this, Label::kDeferred),
      return_create_proxy(this);

  GotoIf(TaggedIsSmi(target), &throw_proxy_non_object);
  GotoIfNot(IsJSReceiver(target), &throw_proxy_non_object);
  GotoIfRevokedProxy(target, &throw_proxy_handler_or_target_revoked);

  GotoIf(TaggedIsSmi(handler), &throw_proxy_non_object);
  GotoIfNot(IsJSReceiver(handler), &throw_proxy_non_object);
  GotoIfRevokedProxy(handler, &throw_proxy_handler_or_target_revoked);

  Node* const proxy = AllocateProxy(target, handler, context);
  Node* const revoke = AllocateProxyRevokeFunction(proxy, context);

  Node* const result = Allocate(JSProxyRevocableResult::kSize);
  Node* const result_map = LoadContextElement(
      native_context, Context::PROXY_REVOCABLE_RESULT_MAP_INDEX);
  StoreMapNoWriteBarrier(result, result_map);
  StoreObjectFieldRoot(result, JSProxyRevocableResult::kPropertiesOrHashOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldRoot(result, JSProxyRevocableResult::kElementsOffset,
                       Heap::kEmptyFixedArrayRootIndex);
  StoreObjectFieldNoWriteBarrier(result, JSProxyRevocableResult::kProxyOffset,
                                 proxy);
  StoreObjectFieldNoWriteBarrier(result, JSProxyRevocableResult::kRevokeOffset,
                                 revoke);
  Return(result);

  BIND(&throw_proxy_non_object);
  ThrowTypeError(context, MessageTemplate::kProxyNonObject);

  BIND(&throw_proxy_handler_or_target_revoked);
  ThrowTypeError(context, MessageTemplate::kProxyHandlerOrTargetRevoked);
}

// Proxy Revocation Functions
// https://tc39.github.io/ecma262/#sec-proxy-revocation-functions
TF_BUILTIN(ProxyRevoke, ProxiesCodeStubAssembler) {
  Node* const context = Parameter(Descriptor::kContext);

  // 1. Let p be F.[[RevocableProxy]].
  Node* const proxy_slot = IntPtrConstant(kProxySlot);
  Node* const proxy = LoadContextElement(context, proxy_slot);

  Label revoke_called(this);

  // 2. If p is null, ...
  GotoIf(IsNull(proxy), &revoke_called);

  // 3. Set F.[[RevocableProxy]] to null.
  StoreContextElement(context, proxy_slot, NullConstant());

  // 4. Assert: p is a Proxy object.
  CSA_ASSERT(this, IsJSProxy(proxy));

  // 5. Set p.[[ProxyTarget]] to null.
  StoreObjectField(proxy, JSProxy::kTargetOffset, NullConstant());

  // 6. Set p.[[ProxyHandler]] to null.
  StoreObjectField(proxy, JSProxy::kHandlerOffset, NullConstant());

  // 7. Return undefined.
  Return(UndefinedConstant());

  BIND(&revoke_called);
  // 2. ... return undefined.
  Return(UndefinedConstant());
}

TF_BUILTIN(CallProxy, ProxiesCodeStubAssembler) {
  Node* argc = Parameter(Descriptor::kActualArgumentsCount);
  Node* argc_ptr = ChangeInt32ToIntPtr(argc);
  Node* proxy = Parameter(Descriptor::kFunction);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsJSProxy(proxy));
  CSA_ASSERT(this, IsCallable(proxy));

  PerformStackCheck(context);

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this);

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);

  // 2. If handler is null, throw a TypeError exception.
  CSA_ASSERT(this, IsNullOrJSReceiver(handler));
  GotoIfNot(IsJSReceiver(handler), &throw_proxy_handler_revoked);

  // 3. Assert: Type(handler) is Object.
  CSA_ASSERT(this, IsJSReceiver(handler));

  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Node* target = LoadObjectField(proxy, JSProxy::kTargetOffset);

  // 5. Let trap be ? GetMethod(handler, "apply").
  // 6. If trap is undefined, then
  Handle<Name> trap_name = factory()->apply_string();
  Node* trap = GetMethod(context, handler, trap_name, &trap_undefined);

  CodeStubArguments args(this, argc_ptr);
  Node* receiver = args.GetReceiver();

  // 7. Let argArray be CreateArrayFromList(argumentsList).
  Node* array = AllocateJSArrayForCodeStubArguments(context, args, argc_ptr,
                                                    INTPTR_PARAMETERS);

  // 8. Return Call(trap, handler, «target, thisArgument, argArray»).
  Node* result = CallJS(CodeFactory::Call(isolate()), context, trap, handler,
                        target, receiver, array);
  args.PopAndReturn(result);

  BIND(&trap_undefined);
  {
    // 6.a. Return Call(target, thisArgument, argumentsList).
    TailCallStub(CodeFactory::Call(isolate()), context, target, argc);
  }

  BIND(&throw_proxy_handler_revoked);
  { ThrowTypeError(context, MessageTemplate::kProxyRevoked, "apply"); }
}

TF_BUILTIN(ConstructProxy, ProxiesCodeStubAssembler) {
  Node* argc = Parameter(Descriptor::kActualArgumentsCount);
  Node* argc_ptr = ChangeInt32ToIntPtr(argc);
  Node* proxy = Parameter(Descriptor::kFunction);
  Node* new_target = Parameter(Descriptor::kNewTarget);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsJSProxy(proxy));
  CSA_ASSERT(this, IsCallable(proxy));

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this), not_an_object(this, Label::kDeferred);

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);

  // 2. If handler is null, throw a TypeError exception.
  CSA_ASSERT(this, IsNullOrJSReceiver(handler));
  GotoIfNot(IsJSReceiver(handler), &throw_proxy_handler_revoked);

  // 3. Assert: Type(handler) is Object.
  CSA_ASSERT(this, IsJSReceiver(handler));

  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Node* target = LoadObjectField(proxy, JSProxy::kTargetOffset);

  // 5. Let trap be ? GetMethod(handler, "construct").
  // 6. If trap is undefined, then
  Handle<Name> trap_name = factory()->construct_string();
  Node* trap = GetMethod(context, handler, trap_name, &trap_undefined);

  CodeStubArguments args(this, argc_ptr);

  // 7. Let argArray be CreateArrayFromList(argumentsList).
  Node* array = AllocateJSArrayForCodeStubArguments(context, args, argc_ptr,
                                                    INTPTR_PARAMETERS);

  // 8. Let newObj be ? Call(trap, handler, « target, argArray, newTarget »).
  Node* new_obj = CallJS(CodeFactory::Call(isolate()), context, trap, handler,
                         target, array, new_target);

  // 9. If Type(newObj) is not Object, throw a TypeError exception.
  GotoIf(TaggedIsSmi(new_obj), &not_an_object);
  GotoIfNot(IsJSReceiver(new_obj), &not_an_object);

  // 10. Return newObj.
  args.PopAndReturn(new_obj);

  BIND(&not_an_object);
  {
    ThrowTypeError(context, MessageTemplate::kProxyConstructNonObject, new_obj);
  }

  BIND(&trap_undefined);
  {
    // 6.a. Assert: target has a [[Construct]] internal method.
    CSA_ASSERT(this, IsConstructor(target));

    // 6.b. Return ? Construct(target, argumentsList, newTarget).
    TailCallStub(CodeFactory::Construct(isolate()), context, target, new_target,
                 argc);
  }

  BIND(&throw_proxy_handler_revoked);
  { ThrowTypeError(context, MessageTemplate::kProxyRevoked, "construct"); }
}

TF_BUILTIN(ProxyHasProperty, ProxiesCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* proxy = Parameter(Descriptor::kProxy);
  Node* name = Parameter(Descriptor::kName);

  CSA_ASSERT(this, IsJSProxy(proxy));

  // 1. Assert: IsPropertyKey(P) is true.
  CSA_ASSERT(this, IsName(name));
  CSA_ASSERT(this, Word32Equal(IsPrivateSymbol(name), Int32Constant(0)));

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this),
      if_try_get_own_property_bailout(this, Label::kDeferred),
      trap_not_callable(this, Label::kDeferred), return_true(this),
      return_false(this), check_target_desc(this);

  // 2. Let handler be O.[[ProxyHandler]].
  Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);

  // 3. If handler is null, throw a TypeError exception.
  // 4. Assert: Type(handler) is Object.
  GotoIfNot(IsJSReceiver(handler), &throw_proxy_handler_revoked);

  // 5. Let target be O.[[ProxyTarget]].
  Node* target = LoadObjectField(proxy, JSProxy::kTargetOffset);

  // 6. Let trap be ? GetMethod(handler, "has").
  // 7. If trap is undefined, then (see 7.a below).
  Handle<Name> trap_name = factory()->has_string();
  Node* trap = GetMethod(context, handler, trap_name, &trap_undefined);

  GotoIf(TaggedIsSmi(trap), &trap_not_callable);
  GotoIfNot(IsCallable(trap), &trap_not_callable);

  // 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler, « target, P
  // »)).
  BranchIfToBooleanIsTrue(CallJS(CodeFactory::Call(isolate()), context, trap,
                                 handler, target, name),
                          &return_true, &check_target_desc);

  BIND(&check_target_desc);
  {
    // 9. If booleanTrapResult is false, then (see 9.a. in CheckHasTrapResult).
    CheckHasTrapResult(context, target, proxy, name, &return_false,
                       &if_try_get_own_property_bailout);
  }

  BIND(&if_try_get_own_property_bailout);
  {
    CallRuntime(Runtime::kCheckProxyHasTrap, context, name, target);
    Return(FalseConstant());
  }

  BIND(&trap_undefined);
  {
    // 7.a. Return ? target.[[HasProperty]](P).
    TailCallBuiltin(Builtins::kHasProperty, context, name, target);
  }

  BIND(&return_false);
  Return(FalseConstant());

  BIND(&return_true);
  Return(TrueConstant());

  BIND(&throw_proxy_handler_revoked);
  ThrowTypeError(context, MessageTemplate::kProxyRevoked, "has");

  BIND(&trap_not_callable);
  ThrowTypeError(context, MessageTemplate::kPropertyNotFunction, trap,
                 StringConstant("has"), proxy);
}

TF_BUILTIN(ProxyGetProperty, ProxiesCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* proxy = Parameter(Descriptor::kProxy);
  Node* name = Parameter(Descriptor::kName);
  Node* receiver = Parameter(Descriptor::kReceiverValue);

  CSA_ASSERT(this, IsJSProxy(proxy));

  // 1. Assert: IsPropertyKey(P) is true.
  CSA_ASSERT(this, TaggedIsNotSmi(name));
  CSA_ASSERT(this, IsName(name));
  CSA_ASSERT(this, Word32Equal(IsPrivateSymbol(name), Int32Constant(0)));

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this);

  // 2. Let handler be O.[[ProxyHandler]].
  Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);

  // 3. If handler is null, throw a TypeError exception.
  GotoIf(IsNull(handler), &throw_proxy_handler_revoked);

  // 4. Assert: Type(handler) is Object.
  CSA_ASSERT(this, IsJSReceiver(handler));

  // 5. Let target be O.[[ProxyTarget]].
  Node* target = LoadObjectField(proxy, JSProxy::kTargetOffset);

  // 6. Let trap be ? GetMethod(handler, "get").
  // 7. If trap is undefined, then (see 7.a below).
  Handle<Name> trap_name = factory()->get_string();
  Node* trap = GetMethod(context, handler, trap_name, &trap_undefined);

  // 8. Let trapResult be ? Call(trap, handler, « target, P, Receiver »).
  Node* trap_result = CallJS(
      CodeFactory::Call(isolate(), ConvertReceiverMode::kNotNullOrUndefined),
      context, trap, handler, target, name, receiver);

  // 9. Let targetDesc be ? target.[[GetOwnProperty]](P).
  Label return_result(this);
  CheckGetSetTrapResult(context, target, proxy, name, trap_result,
                        &return_result, JSProxy::kGet);

  BIND(&return_result);
  {
    // 11. Return trapResult.
    Return(trap_result);
  }

  BIND(&trap_undefined);
  {
    // 7.a. Return ? target.[[Get]](P, Receiver).
    // TODO(mslekova): Introduce GetPropertyWithReceiver stub
    Return(CallRuntime(Runtime::kGetPropertyWithReceiver, context, target, name,
                       receiver));
  }

  BIND(&throw_proxy_handler_revoked);
  ThrowTypeError(context, MessageTemplate::kProxyRevoked, "get");
}

TF_BUILTIN(ProxySetProperty, ProxiesCodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* proxy = Parameter(Descriptor::kProxy);
  Node* name = Parameter(Descriptor::kName);
  Node* value = Parameter(Descriptor::kValue);
  Node* receiver = Parameter(Descriptor::kReceiverValue);
  Node* language_mode = Parameter(Descriptor::kLanguageMode);

  CSA_ASSERT(this, IsJSProxy(proxy));

  // 1. Assert: IsPropertyKey(P) is true.
  CSA_ASSERT(this, TaggedIsNotSmi(name));
  CSA_ASSERT(this, IsName(name));

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this), failure(this, Label::kDeferred),
      continue_checks(this), success(this),
      private_symbol(this, Label::kDeferred);

  GotoIf(IsPrivateSymbol(name), &private_symbol);

  // 2. Let handler be O.[[ProxyHandler]].
  Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);

  // 3. If handler is null, throw a TypeError exception.
  GotoIfNot(IsJSReceiver(handler), &throw_proxy_handler_revoked);

  // 4. Assert: Type(handler) is Object.
  CSA_ASSERT(this, IsJSReceiver(handler));

  // 5. Let target be O.[[ProxyTarget]].
  Node* target = LoadObjectField(proxy, JSProxy::kTargetOffset);

  // 6. Let trap be ? GetMethod(handler, "set").
  // 7. If trap is undefined, then (see 7.a below).
  Handle<Name> set_string = factory()->set_string();
  Node* trap = GetMethod(context, handler, set_string, &trap_undefined);

  // 8. Let booleanTrapResult be ToBoolean(? Call(trap, handler,
  // « target, P, V, Receiver »)).
  // 9. If booleanTrapResult is false, return false.
  BranchIfToBooleanIsTrue(
      CallJS(CodeFactory::Call(isolate(),
                               ConvertReceiverMode::kNotNullOrUndefined),
             context, trap, handler, target, name, value, receiver),
      &continue_checks, &failure);

  BIND(&continue_checks);
  {
    // 9. Let targetDesc be ? target.[[GetOwnProperty]](P).
    Label return_result(this);
    CheckGetSetTrapResult(context, target, proxy, name, value, &success,
                          JSProxy::kSet);
  }

  BIND(&failure);
  {
    Label if_throw(this, Label::kDeferred);
    Branch(SmiEqual(language_mode, SmiConstant(LanguageMode::kStrict)),
           &if_throw, &success);

    BIND(&if_throw);
    ThrowTypeError(context, MessageTemplate::kProxyTrapReturnedFalsishFor,
                   HeapConstant(set_string), name);
  }

  // 12. Return true.
  BIND(&success);
  Return(value);

  BIND(&private_symbol);
  {
    Label failure(this), throw_error(this, Label::kDeferred);

    Branch(SmiEqual(language_mode, SmiConstant(LanguageMode::kStrict)),
           &throw_error, &failure);

    BIND(&failure);
    Return(UndefinedConstant());

    BIND(&throw_error);
    ThrowTypeError(context, MessageTemplate::kProxyPrivate);
  }

  BIND(&trap_undefined);
  {
    // 7.a. Return ? target.[[Set]](P, V, Receiver).
    CallRuntime(Runtime::kSetPropertyWithReceiver, context, target, name, value,
                receiver, language_mode);
    Return(value);
  }

  BIND(&throw_proxy_handler_revoked);
  ThrowTypeError(context, MessageTemplate::kProxyRevoked, "set");
}

void ProxiesCodeStubAssembler::CheckGetSetTrapResult(
    Node* context, Node* target, Node* proxy, Node* name, Node* trap_result,
    Label* check_passed, JSProxy::AccessKind access_kind) {
  Node* map = LoadMap(target);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_details, MachineRepresentation::kWord32);
  VARIABLE(var_raw_value, MachineRepresentation::kTagged);

  Label if_found_value(this), check_in_runtime(this, Label::kDeferred);

  Node* instance_type = LoadInstanceType(target);
  TryGetOwnProperty(context, target, target, map, instance_type, name,
                    &if_found_value, &var_value, &var_details, &var_raw_value,
                    check_passed, &check_in_runtime, kReturnAccessorPair);

  BIND(&if_found_value);
  {
    Label throw_non_configurable_data(this, Label::kDeferred),
        throw_non_configurable_accessor(this, Label::kDeferred),
        check_accessor(this), check_data(this);

    // If targetDesc is not undefined and targetDesc.[[Configurable]] is
    // false, then:
    GotoIfNot(IsSetWord32(var_details.value(),
                          PropertyDetails::kAttributesDontDeleteMask),
              check_passed);

    // If IsDataDescriptor(targetDesc) is true and
    // targetDesc.[[Writable]] is false, then:
    BranchIfAccessorPair(var_raw_value.value(), &check_accessor, &check_data);

    BIND(&check_data);
    {
      Node* read_only = IsSetWord32(var_details.value(),
                                    PropertyDetails::kAttributesReadOnlyMask);
      GotoIfNot(read_only, check_passed);

      // If SameValue(trapResult, targetDesc.[[Value]]) is false,
      // throw a TypeError exception.
      BranchIfSameValue(trap_result, var_value.value(), check_passed,
                        &throw_non_configurable_data);
    }

    BIND(&check_accessor);
    {
      Node* accessor_pair = var_raw_value.value();

      if (access_kind == JSProxy::kGet) {
        Label continue_check(this, Label::kDeferred);
        // 10.b. If IsAccessorDescriptor(targetDesc) is true and
        // targetDesc.[[Get]] is undefined, then:
        Node* getter =
            LoadObjectField(accessor_pair, AccessorPair::kGetterOffset);
        // Here we check for null as well because if the getter was never
        // defined it's set as null.
        GotoIf(IsUndefined(getter), &continue_check);
        GotoIf(IsNull(getter), &continue_check);
        Goto(check_passed);

        // 10.b.i. If trapResult is not undefined, throw a TypeError exception.
        BIND(&continue_check);
        GotoIfNot(IsUndefined(trap_result), &throw_non_configurable_accessor);
      } else {
        // 11.b.i. If targetDesc.[[Set]] is undefined, throw a TypeError
        // exception.
        Node* setter =
            LoadObjectField(accessor_pair, AccessorPair::kSetterOffset);
        GotoIf(IsUndefined(setter), &throw_non_configurable_accessor);
        GotoIf(IsNull(setter), &throw_non_configurable_accessor);
      }
      Goto(check_passed);
    }

    BIND(&check_in_runtime);
    {
      CallRuntime(Runtime::kCheckProxyGetSetTrapResult, context, name, target,
                  trap_result, SmiConstant(access_kind));
      Return(trap_result);
    }

    BIND(&throw_non_configurable_data);
    {
      ThrowTypeError(context, MessageTemplate::kProxyGetNonConfigurableData,
                     name, var_value.value(), trap_result);
    }

    BIND(&throw_non_configurable_accessor);
    {
      ThrowTypeError(context, MessageTemplate::kProxyGetNonConfigurableAccessor,
                     name, trap_result);
    }
  }
}

void ProxiesCodeStubAssembler::CheckHasTrapResult(Node* context, Node* target,
                                                  Node* proxy, Node* name,
                                                  Label* check_passed,
                                                  Label* if_bailout) {
  Node* target_map = LoadMap(target);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_details, MachineRepresentation::kWord32);
  VARIABLE(var_raw_value, MachineRepresentation::kTagged);

  Label if_found_value(this, Label::kDeferred),
      throw_non_configurable(this, Label::kDeferred),
      throw_non_extensible(this, Label::kDeferred);

  // 9.a. Let targetDesc be ? target.[[GetOwnProperty]](P).
  Node* instance_type = LoadInstanceType(target);
  TryGetOwnProperty(context, target, target, target_map, instance_type, name,
                    &if_found_value, &var_value, &var_details, &var_raw_value,
                    check_passed, if_bailout, kReturnAccessorPair);

  // 9.b. If targetDesc is not undefined, then (see 9.b.i. below).
  BIND(&if_found_value);
  {
    // 9.b.i. If targetDesc.[[Configurable]] is false, throw a TypeError
    // exception.
    Node* non_configurable = IsSetWord32(
        var_details.value(), PropertyDetails::kAttributesDontDeleteMask);
    GotoIf(non_configurable, &throw_non_configurable);

    // 9.b.ii. Let extensibleTarget be ? IsExtensible(target).
    Node* target_extensible = IsExtensibleMap(target_map);

    // 9.b.iii. If extensibleTarget is false, throw a TypeError exception.
    GotoIfNot(target_extensible, &throw_non_extensible);
    Goto(check_passed);
  }

  BIND(&throw_non_configurable);
  { ThrowTypeError(context, MessageTemplate::kProxyHasNonConfigurable, name); }

  BIND(&throw_non_extensible);
  { ThrowTypeError(context, MessageTemplate::kProxyHasNonExtensible, name); }
}

}  // namespace internal
}  // namespace v8
