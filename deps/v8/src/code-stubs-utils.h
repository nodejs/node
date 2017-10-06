// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODE_STUBS_UTILS_H_
#define V8_CODE_STUBS_UTILS_H_

namespace v8 {
namespace internal {

namespace compiler {
class CodeAssemblerState;
}  // namespace compiler

// ----------------------------------------------------------------------------
// Support macro for defining code stubs with Turbofan.
// ----------------------------------------------------------------------------
//
// A code stub generator is defined by writing:
//
//   TF_STUB(name, code_assember_base_class) {
//     ...
//   }
//
// In the body of the generator function the arguments can be accessed
// as "Parameter(n)".
#define TF_STUB(StubName, AssemblerBase)                                       \
  class StubName##Assembler : public AssemblerBase {                           \
   public:                                                                     \
    typedef StubName::Descriptor Descriptor;                                   \
                                                                               \
    explicit StubName##Assembler(compiler::CodeAssemblerState* state)          \
        : AssemblerBase(state) {}                                              \
    void Generate##StubName##Impl(const StubName* stub);                       \
                                                                               \
    Node* Parameter(Descriptor::ParameterIndices index) {                      \
      return CodeAssembler::Parameter(static_cast<int>(index));                \
    }                                                                          \
  };                                                                           \
  void StubName::GenerateAssembly(compiler::CodeAssemblerState* state) const { \
    StubName##Assembler assembler(state);                                      \
    assembler.Generate##StubName##Impl(this);                                  \
  }                                                                            \
  void StubName##Assembler::Generate##StubName##Impl(const StubName* stub)

}  // namespace internal
}  // namespace v8

#endif  // V8_CODE_STUBS_UTILS_H_
