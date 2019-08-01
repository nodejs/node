// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-proxy-gen.h"
#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"

#include "src/logging/counters.h"
#include "src/objects/objects-inl.h"

namespace v8 {
namespace internal {

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
                       RootIndex::kEmptyPropertyDictionary);
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

    TVARIABLE(IntPtrT, offset,
              IntPtrConstant(FixedArrayBase::kHeaderSize - kHeapObjectTag));
    VariableList list({&offset}, zone());

    GotoIf(SmiGreaterThan(length, SmiConstant(FixedArray::kMaxRegularLength)),
           &if_large_object);
    args.ForEach(list, [=, &offset](Node* arg) {
      StoreNoWriteBarrier(MachineRepresentation::kTagged, allocated_elements,
                          offset.value(), arg);
      Increment(&offset, kTaggedSize);
    });
    Goto(&allocate_js_array);

    BIND(&if_large_object);
    {
      args.ForEach(list, [=, &offset](Node* arg) {
        Store(allocated_elements, offset.value(), arg);
        Increment(&offset, kTaggedSize);
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
  TNode<Map> array_map =
      LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
  TNode<JSArray> array =
      AllocateJSArray(array_map, CAST(elements.value()), length);

  return array;
}

Node* ProxiesCodeStubAssembler::CreateProxyRevokeFunctionContext(
    Node* proxy, Node* native_context) {
  Node* const context = Allocate(FixedArray::SizeFor(kProxyContextLength));
  StoreMapNoWriteBarrier(context, RootIndex::kFunctionContextMap);
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

Node* ProxiesCodeStubAssembler::GetProxyConstructorJSNewTarget() {
  return CodeAssembler::Parameter(static_cast<int>(
      Builtin_ProxyConstructor_InterfaceDescriptor::kJSNewTarget));
}

TF_BUILTIN(CallProxy, ProxiesCodeStubAssembler) {
  Node* argc = Parameter(Descriptor::kActualArgumentsCount);
  Node* argc_ptr = ChangeInt32ToIntPtr(argc);
  Node* proxy = Parameter(Descriptor::kFunction);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsJSProxy(proxy));
  CSA_ASSERT(this, IsCallable(proxy));

  PerformStackCheck(CAST(context));

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
  Node* proxy = Parameter(Descriptor::kTarget);
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

Node* ProxiesCodeStubAssembler::CheckGetSetTrapResult(
    Node* context, Node* target, Node* proxy, Node* name, Node* trap_result,
    JSProxy::AccessKind access_kind) {
  Node* map = LoadMap(target);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_details, MachineRepresentation::kWord32);
  VARIABLE(var_raw_value, MachineRepresentation::kTagged);

  Label if_found_value(this), check_in_runtime(this, Label::kDeferred),
      check_passed(this);

  GotoIfNot(IsUniqueNameNoIndex(CAST(name)), &check_in_runtime);
  Node* instance_type = LoadInstanceType(target);
  TryGetOwnProperty(context, target, target, map, instance_type, name,
                    &if_found_value, &var_value, &var_details, &var_raw_value,
                    &check_passed, &check_in_runtime, kReturnAccessorPair);

  BIND(&if_found_value);
  {
    Label throw_non_configurable_data(this, Label::kDeferred),
        throw_non_configurable_accessor(this, Label::kDeferred),
        check_accessor(this), check_data(this);

    // If targetDesc is not undefined and targetDesc.[[Configurable]] is
    // false, then:
    GotoIfNot(IsSetWord32(var_details.value(),
                          PropertyDetails::kAttributesDontDeleteMask),
              &check_passed);

    // If IsDataDescriptor(targetDesc) is true and
    // targetDesc.[[Writable]] is false, then:
    BranchIfAccessorPair(var_raw_value.value(), &check_accessor, &check_data);

    BIND(&check_data);
    {
      Node* read_only = IsSetWord32(var_details.value(),
                                    PropertyDetails::kAttributesReadOnlyMask);
      GotoIfNot(read_only, &check_passed);

      // If SameValue(trapResult, targetDesc.[[Value]]) is false,
      // throw a TypeError exception.
      BranchIfSameValue(trap_result, var_value.value(), &check_passed,
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
        Goto(&check_passed);

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
      Goto(&check_passed);
    }

    BIND(&throw_non_configurable_data);
    {
      if (access_kind == JSProxy::kGet) {
        ThrowTypeError(context, MessageTemplate::kProxyGetNonConfigurableData,
                       name, var_value.value(), trap_result);
      } else {
        ThrowTypeError(context, MessageTemplate::kProxySetFrozenData, name);
      }
    }

    BIND(&throw_non_configurable_accessor);
    {
      if (access_kind == JSProxy::kGet) {
        ThrowTypeError(context,
                       MessageTemplate::kProxyGetNonConfigurableAccessor, name,
                       trap_result);
      } else {
        ThrowTypeError(context, MessageTemplate::kProxySetFrozenAccessor, name);
      }
    }

    BIND(&check_in_runtime);
    {
      CallRuntime(Runtime::kCheckProxyGetSetTrapResult, context, name, target,
                  trap_result, SmiConstant(access_kind));
      Goto(&check_passed);
    }

    BIND(&check_passed);
    return trap_result;
  }
}

Node* ProxiesCodeStubAssembler::CheckHasTrapResult(Node* context, Node* target,
                                                   Node* proxy, Node* name) {
  Node* target_map = LoadMap(target);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_details, MachineRepresentation::kWord32);
  VARIABLE(var_raw_value, MachineRepresentation::kTagged);

  Label if_found_value(this, Label::kDeferred),
      throw_non_configurable(this, Label::kDeferred),
      throw_non_extensible(this, Label::kDeferred), check_passed(this),
      check_in_runtime(this, Label::kDeferred);

  // 9.a. Let targetDesc be ? target.[[GetOwnProperty]](P).
  GotoIfNot(IsUniqueNameNoIndex(CAST(name)), &check_in_runtime);
  Node* instance_type = LoadInstanceType(target);
  TryGetOwnProperty(context, target, target, target_map, instance_type, name,
                    &if_found_value, &var_value, &var_details, &var_raw_value,
                    &check_passed, &check_in_runtime, kReturnAccessorPair);

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
    Goto(&check_passed);
  }

  BIND(&throw_non_configurable);
  { ThrowTypeError(context, MessageTemplate::kProxyHasNonConfigurable, name); }

  BIND(&throw_non_extensible);
  { ThrowTypeError(context, MessageTemplate::kProxyHasNonExtensible, name); }

  BIND(&check_in_runtime);
  {
    CallRuntime(Runtime::kCheckProxyHasTrapResult, context, name, target);
    Goto(&check_passed);
  }

  BIND(&check_passed);
  return FalseConstant();
}

}  // namespace internal
}  // namespace v8
