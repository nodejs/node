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

  TNode<JSProxy> AllocateProxy(TNode<Context> context, TNode<JSReceiver> target,
                               TNode<JSReceiver> handler);
  TNode<JSFunction> AllocateProxyRevokeFunction(TNode<Context> context,
                                                TNode<JSProxy> proxy);

  void CheckGetSetTrapResult(TNode<Context> context, TNode<JSReceiver> target,
                             TNode<JSProxy> proxy, TNode<Name> name,
                             TNode<Object> trap_result,
                             JSProxy::AccessKind access_kind);

  void CheckHasTrapResult(TNode<Context> context, TNode<JSReceiver> target,
                          TNode<JSProxy> proxy, TNode<Name> name);

  void CheckDeleteTrapResult(TNode<Context> context, TNode<JSReceiver> target,
                             TNode<JSProxy> proxy, TNode<Name> name);

 protected:
  enum ProxyRevokeFunctionContextSlot {
    kProxySlot = Context::MIN_CONTEXT_SLOTS,
    kProxyContextLength,
  };

  TNode<JSArray> AllocateJSArrayForCodeStubArguments(
      TNode<Context> context, const CodeStubArguments& args,
      TNode<IntPtrT> argc);

 private:
  TNode<Context> CreateProxyRevokeFunctionContext(
      TNode<JSProxy> proxy, TNode<NativeContext> native_context);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_BUILTINS_PROXY_GEN_H_
