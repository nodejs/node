// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_TORQUE_ASSEMBLER_H_
#define V8_TORQUE_ASSEMBLER_H_

#include <deque>
#include <vector>

#include "src/code-stub-assembler.h"

#include "src/base/optional.h"

namespace v8 {
namespace internal {

class TorqueAssembler : public CodeStubAssembler {
 public:
  using CodeStubAssembler::CodeStubAssembler;

 protected:
  template <class... Ts>
  using PLabel = compiler::CodeAssemblerParameterizedLabel<Ts...>;

  template <class T>
  TNode<T> Uninitialized() {
    return {};
  }

  template <class... T, class... Args>
  void Goto(PLabel<T...>* label, Args... args) {
    label->AddInputs(args...);
    CodeStubAssembler::Goto(label->plain_label());
  }
  using CodeStubAssembler::Goto;
  template <class... T>
  void Bind(PLabel<T...>* label, TNode<T>*... phis) {
    Bind(label->plain_label());
    label->CreatePhis(phis...);
  }
  void Bind(Label* label) { CodeAssembler::Bind(label); }
  using CodeStubAssembler::Bind;
  template <class... T, class... Args>
  void Branch(TNode<BoolT> condition, PLabel<T...>* if_true,
              PLabel<T...>* if_false, Args... args) {
    if_true->AddInputs(args...);
    if_false->AddInputs(args...);
    CodeStubAssembler::Branch(condition, if_true->plain_label(),
                              if_false->plain_label());
  }
  using CodeStubAssembler::Branch;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_TORQUE_ASSEMBLER_H_
