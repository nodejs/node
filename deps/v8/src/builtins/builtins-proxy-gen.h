// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROXY_GEN_H_
#define V8_BUILTINS_BUILTINS_PROXY_GEN_H_

#include "src/code-stub-assembler.h"
#include "src/objects/js-proxy.h"
#include "torque-generated/builtins-proxy-from-dsl-gen.h"

namespace v8 {
namespace internal {
using compiler::Node;

class ProxiesCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit ProxiesCodeStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  // ES6 section 9.5.8 [[Get]] ( P, Receiver )
  // name should not be an index.
  Node* ProxyGetProperty(Node* context, Node* proxy, Node* name,
                         Node* receiver);

  // ES6 section 9.5.9 [[Set]] ( P, V, Receiver )
  // name should not be an index.
  Node* ProxySetProperty(Node* context, Node* proxy, Node* name, Node* value,
                         Node* receiver);

  Node* AllocateProxy(Node* target, Node* handler, Node* context);
  Node* AllocateProxyRevokeFunction(Node* proxy, Node* context);

  // Get JSNewTarget parameter for ProxyConstructor builtin (Torque).
  // TODO(v8:9120): Remove this once torque support exists
  Node* GetProxyConstructorJSNewTarget();

 protected:
  enum ProxyRevokeFunctionContextSlot {
    kProxySlot = Context::MIN_CONTEXT_SLOTS,
    kProxyContextLength,
  };

  Node* AllocateJSArrayForCodeStubArguments(Node* context,
                                            CodeStubArguments& args, Node* argc,
                                            ParameterMode mode);
  void CheckHasTrapResult(Node* context, Node* target, Node* proxy, Node* name,
                          Label* check_passed, Label* if_bailout);

  void CheckGetSetTrapResult(Node* context, Node* target, Node* proxy,
                             Node* name, Node* trap_result, Label* if_not_found,
                             JSProxy::AccessKind access_kind);

 private:
  Node* CreateProxyRevokeFunctionContext(Node* proxy, Node* native_context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROXY_GEN_H_
