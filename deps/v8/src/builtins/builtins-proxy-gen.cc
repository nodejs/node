// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils-gen.h"
#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-stub-assembler.h"

#include "src/counters.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {
using compiler::Node;
using compiler::CodeAssembler;

// ES6 section 26.2.1.1 Proxy ( target, handler ) for the [[Call]] case.
TF_BUILTIN(ProxyConstructor, CodeStubAssembler) {
  Node* context = Parameter(Descriptor::kContext);
  ThrowTypeError(context, MessageTemplate::kConstructorNotFunction, "Proxy");
}

class ProxiesCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit ProxiesCodeStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* IsProxyRevoked(Node* proxy) {
    CSA_ASSERT(this, IsJSProxy(proxy));

    Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);
    CSA_ASSERT(this, Word32Or(IsJSReceiver(handler), IsNull(handler)));

    return IsNull(handler);
  }

  void GotoIfProxyRevoked(Node* object, Label* if_proxy_revoked) {
    Label continue_checks(this);
    GotoIfNot(IsJSProxy(object), &continue_checks);
    GotoIf(IsProxyRevoked(object), if_proxy_revoked);
    Goto(&continue_checks);
    BIND(&continue_checks);
  }

  Node* AllocateProxy(Node* target, Node* handler, Node* context) {
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
                         Heap::kEmptyPropertiesDictionaryRootIndex);
    StoreObjectFieldNoWriteBarrier(proxy, JSProxy::kTargetOffset, target);
    StoreObjectFieldNoWriteBarrier(proxy, JSProxy::kHandlerOffset, handler);
    StoreObjectFieldNoWriteBarrier(proxy, JSProxy::kHashOffset,
                                   UndefinedConstant());

    return proxy;
  }

  Node* AllocateJSArrayForCodeStubArguments(Node* context,
                                            CodeStubArguments& args, Node* argc,
                                            ParameterMode mode) {
    Node* array = nullptr;
    Node* elements = nullptr;
    Node* native_context = LoadNativeContext(context);
    Node* array_map = LoadJSArrayElementsMap(PACKED_ELEMENTS, native_context);
    Node* argc_smi = ParameterToTagged(argc, mode);
    std::tie(array, elements) = AllocateUninitializedJSArrayWithElements(
        PACKED_ELEMENTS, array_map, argc_smi, nullptr, argc, INTPTR_PARAMETERS);

    StoreMapNoWriteBarrier(elements, Heap::kFixedArrayMapRootIndex);
    StoreObjectFieldNoWriteBarrier(elements, FixedArrayBase::kLengthOffset,
                                   argc_smi);

    VARIABLE(index, MachineType::PointerRepresentation());
    index.Bind(IntPtrConstant(FixedArrayBase::kHeaderSize - kHeapObjectTag));
    VariableList list({&index}, zone());
    args.ForEach(list, [this, elements, &index](Node* arg) {
      StoreNoWriteBarrier(MachineRepresentation::kTagged, elements,
                          index.value(), arg);
      Increment(index, kPointerSize);
    });
    return array;
  }
};

// ES6 section 26.2.1.1 Proxy ( target, handler ) for the [[Construct]] case.
TF_BUILTIN(ProxyConstructor_ConstructStub, ProxiesCodeStubAssembler) {
  int const kTargetArg = 0;
  int const kHandlerArg = 1;

  Node* argc =
      ChangeInt32ToIntPtr(Parameter(BuiltinDescriptor::kArgumentsCount));
  CodeStubArguments args(this, argc);

  Node* target = args.GetOptionalArgumentValue(kTargetArg);
  Node* handler = args.GetOptionalArgumentValue(kHandlerArg);
  Node* context = Parameter(BuiltinDescriptor::kContext);

  Label throw_proxy_non_object(this, Label::kDeferred),
      throw_proxy_handler_or_target_revoked(this, Label::kDeferred),
      return_create_proxy(this);

  GotoIf(TaggedIsSmi(target), &throw_proxy_non_object);
  GotoIfNot(IsJSReceiver(target), &throw_proxy_non_object);
  GotoIfProxyRevoked(target, &throw_proxy_handler_or_target_revoked);

  GotoIf(TaggedIsSmi(handler), &throw_proxy_non_object);
  GotoIfNot(IsJSReceiver(handler), &throw_proxy_non_object);
  GotoIfProxyRevoked(handler, &throw_proxy_handler_or_target_revoked);

  args.PopAndReturn(AllocateProxy(target, handler, context));

  BIND(&throw_proxy_non_object);
  ThrowTypeError(context, MessageTemplate::kProxyNonObject);

  BIND(&throw_proxy_handler_or_target_revoked);
  ThrowTypeError(context, MessageTemplate::kProxyHandlerOrTargetRevoked);
}

TF_BUILTIN(CallProxy, ProxiesCodeStubAssembler) {
  Node* argc = Parameter(Descriptor::kActualArgumentsCount);
  Node* argc_ptr = ChangeInt32ToIntPtr(argc);
  Node* proxy = Parameter(Descriptor::kFunction);
  Node* context = Parameter(Descriptor::kContext);

  CSA_ASSERT(this, IsJSProxy(proxy));
  CSA_ASSERT(this, IsCallable(proxy));

  Label throw_proxy_handler_revoked(this, Label::kDeferred),
      trap_undefined(this), trap_defined(this, Label::kDeferred);

  // 1. Let handler be the value of the [[ProxyHandler]] internal slot of O.
  Node* handler = LoadObjectField(proxy, JSProxy::kHandlerOffset);

  // 2. If handler is null, throw a TypeError exception.
  CSA_ASSERT(this, Word32Or(IsJSReceiver(handler), IsNull(handler)));
  GotoIf(IsNull(handler), &throw_proxy_handler_revoked);

  // 3. Assert: Type(handler) is Object.
  CSA_ASSERT(this, IsJSReceiver(handler));

  // 4. Let target be the value of the [[ProxyTarget]] internal slot of O.
  Node* target = LoadObjectField(proxy, JSProxy::kTargetOffset);

  // 5. Let trap be ? GetMethod(handler, "apply").
  Handle<Name> trap_name = factory()->apply_string();
  Node* trap = GetProperty(context, handler, trap_name);

  // 6. If trap is undefined, then
  GotoIf(IsUndefined(trap), &trap_undefined);
  Branch(IsNull(trap), &trap_undefined, &trap_defined);

  BIND(&trap_defined);
  {
    CodeStubArguments args(this, argc_ptr);
    Node* receiver = args.GetReceiver();

    // 7. Let argArray be CreateArrayFromList(argumentsList).
    Node* array = AllocateJSArrayForCodeStubArguments(context, args, argc_ptr,
                                                      INTPTR_PARAMETERS);

    // 8. Return Call(trap, handler, «target, thisArgument, argArray»).
    Node* result = CallJS(CodeFactory::Call(isolate()), context, trap, handler,
                          target, receiver, array);
    args.PopAndReturn(result);
  }

  BIND(&trap_undefined);
  {
    // 6.a. Return Call(target, thisArgument, argumentsList).
    TailCallStub(CodeFactory::Call(isolate()), context, target, argc);
  }

  BIND(&throw_proxy_handler_revoked);
  {
    CallRuntime(Runtime::kThrowTypeError, context,
                SmiConstant(MessageTemplate::kProxyRevoked),
                StringConstant("apply"));
    Unreachable();
  }
}

}  // namespace internal
}  // namespace v8
