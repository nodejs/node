// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-proxy-helpers-gen.h"
#include "src/builtins/builtins-utils-gen.h"

namespace v8 {
namespace internal {
TF_BUILTIN(ProxyGetProperty, ProxyAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  Node* proxy = Parameter(Descriptor::kProxy);
  Node* name = Parameter(Descriptor::kName);
  Node* receiver = Parameter(Descriptor::kReceiverValue);

  CSA_ASSERT(this, IsJSProxy(proxy));

  // 1. Assert: IsPropertyKey(P) is true.
  CSA_ASSERT(this, IsName(name));

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this), no_target_desc(this, Label::kDeferred),
      trap_not_callable(this, Label::kDeferred);

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

  GotoIf(TaggedIsSmi(trap), &trap_not_callable);
  GotoIfNot(IsCallable(trap), &trap_not_callable);

  // 8. Let trapResult be ? Call(trap, handler, « target, P, Receiver »).
  Node* trap_result = CallJS(CodeFactory::Call(isolate()), context, trap,
                             handler, target, name, receiver);
  // 9. Let targetDesc be ? target.[[GetOwnProperty]](P).
  Label return_result(this);
  CheckGetTrapResult(context, target, proxy, name, trap_result, &return_result,
                     &no_target_desc);

  BIND(&return_result);
  {
    // 11. Return trapResult.
    Return(trap_result);
  }

  BIND(&no_target_desc);
  {
    CSA_ASSERT(this, IsJSReceiver(target));
    CallRuntime(Runtime::kCheckProxyGetTrapResult, context, name, target,
                trap_result);
    Return(trap_result);
  }

  BIND(&trap_undefined);
  {
    // 7.a. Return ? target.[[Get]](P, Receiver).
    Return(CallRuntime(Runtime::kGetPropertyWithReceiver, context, target, name,
                       receiver));
  }

  BIND(&throw_proxy_handler_revoked);
  { ThrowTypeError(context, MessageTemplate::kProxyRevoked, "get"); }

  BIND(&trap_not_callable);
  {
    ThrowTypeError(context, MessageTemplate::kPropertyNotFunction, trap,
                   StringConstant("get"), receiver);
  }
}

void ProxyAssembler::CheckGetTrapResult(Node* context, Node* target,
                                        Node* proxy, Node* name,
                                        Node* trap_result, Label* check_passed,
                                        Label* if_bailout) {
  Node* map = LoadMap(target);
  VARIABLE(var_value, MachineRepresentation::kTagged);
  VARIABLE(var_details, MachineRepresentation::kWord32);
  VARIABLE(var_raw_value, MachineRepresentation::kTagged);

  Label if_found_value(this, Label::kDeferred);

  Node* instance_type = LoadInstanceType(target);
  TryGetOwnProperty(context, proxy, target, map, instance_type, name,
                    &if_found_value, &var_value, &var_details, &var_raw_value,
                    check_passed, if_bailout, kReturnAccessorPair);

  BIND(&if_found_value);
  {
    Label throw_non_configurable_data(this, Label::kDeferred),
        throw_non_configurable_accessor(this, Label::kDeferred),
        check_accessor(this), check_data(this);

    // 10. If targetDesc is not undefined and targetDesc.[[Configurable]] is
    // false, then:
    GotoIfNot(IsSetWord32(var_details.value(),
                          PropertyDetails::kAttributesDontDeleteMask),
              check_passed);

    // 10.a. If IsDataDescriptor(targetDesc) is true and
    // targetDesc.[[Writable]] is false, then:
    BranchIfAccessorPair(var_raw_value.value(), &check_accessor, &check_data);

    BIND(&check_data);
    {
      Node* read_only = IsSetWord32(var_details.value(),
                                    PropertyDetails::kAttributesReadOnlyMask);
      GotoIfNot(read_only, check_passed);

      // 10.a.i. If SameValue(trapResult, targetDesc.[[Value]]) is false,
      // throw a TypeError exception.
      GotoIfNot(SameValue(trap_result, var_value.value()),
                &throw_non_configurable_data);
      Goto(check_passed);
    }

    BIND(&check_accessor);
    {
      // 10.b. If IsAccessorDescriptor(targetDesc) is true and
      // targetDesc.[[Get]] is undefined, then:
      Node* accessor_pair = var_raw_value.value();
      Node* getter =
          LoadObjectField(accessor_pair, AccessorPair::kGetterOffset);

      // Here we check for null as well because if the getter was never
      // defined it's set as null.
      GotoIfNot(Word32Or(IsUndefined(getter), IsNull(getter)), check_passed);

      // 10.b.i. If trapResult is not undefined, throw a TypeError exception.
      GotoIfNot(IsUndefined(trap_result), &throw_non_configurable_accessor);
      Goto(check_passed);
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
}  // namespace internal
}  // namespace v8
