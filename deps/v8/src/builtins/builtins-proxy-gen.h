// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BUILTINS_PROXY_GEN_H_
#define V8_BUILTINS_BUILTINS_PROXY_GEN_H_

#include "src/codegen/code-stub-assembler.h"
#include "src/objects/js-proxy.h"

namespace v8 {
namespace internal {
using compiler::Node;

class ProxiesCodeStubAssembler : public CodeStubAssembler {
 public:
  explicit ProxiesCodeStubAssembler(compiler::CodeAssemblerState* state)
      : CodeStubAssembler(state) {}

  Node* AllocateProxy(Node* target, Node* handler, Node* context);
  Node* AllocateProxyRevokeFunction(Node* proxy, Node* context);

  // Get JSNewTarget parameter for ProxyConstructor builtin (Torque).
  // TODO(v8:9120): Remove this once torque support exists
  Node* GetProxyConstructorJSNewTarget();

  Node* CheckGetSetTrapResult(Node* context, Node* target, Node* proxy,
                              Node* name, Node* trap_result,
                              JSProxy::AccessKind access_kind);

  Node* CheckHasTrapResult(Node* context, Node* target, Node* proxy,
                           Node* name);

 protected:
  enum ProxyRevokeFunctionContextSlot {
    kProxySlot = Context::MIN_CONTEXT_SLOTS,
    kProxyContextLength,
  };

  Node* AllocateJSArrayForCodeStubArguments(Node* context,
                                            CodeStubArguments& args, Node* argc,
                                            ParameterMode mode);

 private:
  Node* CreateProxyRevokeFunctionContext(Node* proxy, Node* native_context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROXY_GEN_H_
