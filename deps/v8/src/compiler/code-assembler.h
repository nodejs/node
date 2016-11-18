// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_ASSEMBLER_H_
#define V8_COMPILER_CODE_ASSEMBLER_H_

#include <map>
#include <memory>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/allocation.h"
#include "src/builtins/builtins.h"
#include "src/heap/heap.h"
#include "src/machine-type.h"
#include "src/runtime/runtime.h"
#include "src/zone-containers.h"

namespace v8 {
namespace internal {

class Callable;
class CallInterfaceDescriptor;
class Isolate;
class Factory;
class Zone;

namespace compiler {

class CallDescriptor;
class Node;
class RawMachineAssembler;
class RawMachineLabel;

#define CODE_ASSEMBLER_COMPARE_BINARY_OP_LIST(V) \
  V(Float32Equal)                                \
  V(Float32LessThan)                             \
  V(Float32LessThanOrEqual)                      \
  V(Float32GreaterThan)                          \
  V(Float32GreaterThanOrEqual)                   \
  V(Float64Equal)                                \
  V(Float64LessThan)                             \
  V(Float64LessThanOrEqual)                      \
  V(Float64GreaterThan)                          \
  V(Float64GreaterThanOrEqual)                   \
  V(Int32GreaterThan)                            \
  V(Int32GreaterThanOrEqual)                     \
  V(Int32LessThan)                               \
  V(Int32LessThanOrEqual)                        \
  V(IntPtrLessThan)                              \
  V(IntPtrLessThanOrEqual)                       \
  V(IntPtrGreaterThan)                           \
  V(IntPtrGreaterThanOrEqual)                    \
  V(IntPtrEqual)                                 \
  V(Uint32LessThan)                              \
  V(Uint32GreaterThanOrEqual)                    \
  V(UintPtrLessThan)                             \
  V(UintPtrGreaterThanOrEqual)                   \
  V(WordEqual)                                   \
  V(WordNotEqual)                                \
  V(Word32Equal)                                 \
  V(Word32NotEqual)                              \
  V(Word64Equal)                                 \
  V(Word64NotEqual)

#define CODE_ASSEMBLER_BINARY_OP_LIST(V)   \
  CODE_ASSEMBLER_COMPARE_BINARY_OP_LIST(V) \
  V(Float64Add)                            \
  V(Float64Sub)                            \
  V(Float64Mul)                            \
  V(Float64Div)                            \
  V(Float64Mod)                            \
  V(Float64Atan2)                          \
  V(Float64Pow)                            \
  V(Float64InsertLowWord32)                \
  V(Float64InsertHighWord32)               \
  V(IntPtrAdd)                             \
  V(IntPtrAddWithOverflow)                 \
  V(IntPtrSub)                             \
  V(IntPtrSubWithOverflow)                 \
  V(IntPtrMul)                             \
  V(Int32Add)                              \
  V(Int32AddWithOverflow)                  \
  V(Int32Sub)                              \
  V(Int32Mul)                              \
  V(Int32MulWithOverflow)                  \
  V(Int32Div)                              \
  V(Int32Mod)                              \
  V(WordOr)                                \
  V(WordAnd)                               \
  V(WordXor)                               \
  V(WordShl)                               \
  V(WordShr)                               \
  V(WordSar)                               \
  V(WordRor)                               \
  V(Word32Or)                              \
  V(Word32And)                             \
  V(Word32Xor)                             \
  V(Word32Shl)                             \
  V(Word32Shr)                             \
  V(Word32Sar)                             \
  V(Word32Ror)                             \
  V(Word64Or)                              \
  V(Word64And)                             \
  V(Word64Xor)                             \
  V(Word64Shr)                             \
  V(Word64Sar)                             \
  V(Word64Ror)

#define CODE_ASSEMBLER_UNARY_OP_LIST(V) \
  V(Float64Abs)                         \
  V(Float64Acos)                        \
  V(Float64Acosh)                       \
  V(Float64Asin)                        \
  V(Float64Asinh)                       \
  V(Float64Atan)                        \
  V(Float64Atanh)                       \
  V(Float64Cos)                         \
  V(Float64Cosh)                        \
  V(Float64Exp)                         \
  V(Float64Expm1)                       \
  V(Float64Log)                         \
  V(Float64Log1p)                       \
  V(Float64Log2)                        \
  V(Float64Log10)                       \
  V(Float64Cbrt)                        \
  V(Float64Neg)                         \
  V(Float64Sin)                         \
  V(Float64Sinh)                        \
  V(Float64Sqrt)                        \
  V(Float64Tan)                         \
  V(Float64Tanh)                        \
  V(Float64ExtractLowWord32)            \
  V(Float64ExtractHighWord32)           \
  V(BitcastWordToTagged)                \
  V(TruncateFloat64ToFloat32)           \
  V(TruncateFloat64ToWord32)            \
  V(TruncateInt64ToInt32)               \
  V(ChangeFloat32ToFloat64)             \
  V(ChangeFloat64ToUint32)              \
  V(ChangeInt32ToFloat64)               \
  V(ChangeInt32ToInt64)                 \
  V(ChangeUint32ToFloat64)              \
  V(ChangeUint32ToUint64)               \
  V(RoundFloat64ToInt32)                \
  V(Float64RoundDown)                   \
  V(Float64RoundUp)                     \
  V(Float64RoundTruncate)               \
  V(Word32Clz)

// A "public" interface used by components outside of compiler directory to
// create code objects with TurboFan's backend. This class is mostly a thin shim
// around the RawMachineAssembler, and its primary job is to ensure that the
// innards of the RawMachineAssembler and other compiler implementation details
// don't leak outside of the the compiler directory..
//
// V8 components that need to generate low-level code using this interface
// should include this header--and this header only--from the compiler directory
// (this is actually enforced). Since all interesting data structures are
// forward declared, it's not possible for clients to peek inside the compiler
// internals.
//
// In addition to providing isolation between TurboFan and code generation
// clients, CodeAssembler also provides an abstraction for creating variables
// and enhanced Label functionality to merge variable values along paths where
// they have differing values, including loops.
class CodeAssembler {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeAssembler(Isolate* isolate, Zone* zone,
                const CallInterfaceDescriptor& descriptor, Code::Flags flags,
                const char* name, size_t result_size = 1);

  // Create with JSCall linkage.
  CodeAssembler(Isolate* isolate, Zone* zone, int parameter_count,
                Code::Flags flags, const char* name);

  virtual ~CodeAssembler();

  Handle<Code> GenerateCode();

  bool Is64() const;
  bool IsFloat64RoundUpSupported() const;
  bool IsFloat64RoundDownSupported() const;
  bool IsFloat64RoundTruncateSupported() const;

  class Label;
  class Variable {
   public:
    explicit Variable(CodeAssembler* assembler, MachineRepresentation rep);
    ~Variable();
    void Bind(Node* value);
    Node* value() const;
    MachineRepresentation rep() const;
    bool IsBound() const;

   private:
    friend class CodeAssembler;
    class Impl;
    Impl* impl_;
    CodeAssembler* assembler_;
  };

  // ===========================================================================
  // Base Assembler
  // ===========================================================================

  // Constants.
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* IntPtrConstant(intptr_t value);
  Node* NumberConstant(double value);
  Node* SmiConstant(Smi* value);
  Node* HeapConstant(Handle<HeapObject> object);
  Node* BooleanConstant(bool value);
  Node* ExternalConstant(ExternalReference address);
  Node* Float64Constant(double value);
  Node* NaNConstant();

  bool ToInt32Constant(Node* node, int32_t& out_value);
  bool ToInt64Constant(Node* node, int64_t& out_value);
  bool ToIntPtrConstant(Node* node, intptr_t& out_value);

  Node* Parameter(int value);
  void Return(Node* value);

  void DebugBreak();
  void Comment(const char* format, ...);

  void Bind(Label* label);
  void Goto(Label* label);
  void GotoIf(Node* condition, Label* true_label);
  void GotoUnless(Node* condition, Label* false_label);
  void Branch(Node* condition, Label* true_label, Label* false_label);

  void Switch(Node* index, Label* default_label, const int32_t* case_values,
              Label** case_labels, size_t case_count);

  Node* Select(Node* condition, Node* true_value, Node* false_value,
               MachineRepresentation rep = MachineRepresentation::kTagged);

  // Access to the frame pointer
  Node* LoadFramePointer();
  Node* LoadParentFramePointer();

  // Access to the stack pointer
  Node* LoadStackPointer();

  // Load raw memory location.
  Node* Load(MachineType rep, Node* base);
  Node* Load(MachineType rep, Node* base, Node* index);
  Node* AtomicLoad(MachineType rep, Node* base, Node* index);

  // Load a value from the root array.
  Node* LoadRoot(Heap::RootListIndex root_index);

  // Store value to raw memory location.
  Node* Store(MachineRepresentation rep, Node* base, Node* value);
  Node* Store(MachineRepresentation rep, Node* base, Node* index, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* index,
                            Node* value);
  Node* AtomicStore(MachineRepresentation rep, Node* base, Node* index,
                    Node* value);

  // Store a value to the root array.
  Node* StoreRoot(Heap::RootListIndex root_index, Node* value);

// Basic arithmetic operations.
#define DECLARE_CODE_ASSEMBLER_BINARY_OP(name) Node* name(Node* a, Node* b);
  CODE_ASSEMBLER_BINARY_OP_LIST(DECLARE_CODE_ASSEMBLER_BINARY_OP)
#undef DECLARE_CODE_ASSEMBLER_BINARY_OP

  Node* WordShl(Node* value, int shift);
  Node* WordShr(Node* value, int shift);
  Node* Word32Shr(Node* value, int shift);

// Unary
#define DECLARE_CODE_ASSEMBLER_UNARY_OP(name) Node* name(Node* a);
  CODE_ASSEMBLER_UNARY_OP_LIST(DECLARE_CODE_ASSEMBLER_UNARY_OP)
#undef DECLARE_CODE_ASSEMBLER_UNARY_OP

  // No-op on 32-bit, otherwise zero extend.
  Node* ChangeUint32ToWord(Node* value);
  // No-op on 32-bit, otherwise sign extend.
  Node* ChangeInt32ToIntPtr(Node* value);

  // Projections
  Node* Projection(int index, Node* value);

  // Calls
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1,
                    Node* arg2);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1,
                    Node* arg2, Node* arg3);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1,
                    Node* arg2, Node* arg3, Node* arg4);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1,
                    Node* arg2, Node* arg3, Node* arg4, Node* arg5);

  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context);
  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1);
  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1, Node* arg2);
  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1, Node* arg2, Node* arg3);
  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1, Node* arg2, Node* arg3, Node* arg4);
  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1, Node* arg2, Node* arg3, Node* arg4,
                        Node* arg5);

  // A pair of a zero-based argument index and a value.
  // It helps writing arguments order independent code.
  struct Arg {
    Arg(int index, Node* value) : index(index), value(value) {}

    int const index;
    Node* const value;
  };

  Node* CallStub(Callable const& callable, Node* context, Node* arg1,
                 size_t result_size = 1);
  Node* CallStub(Callable const& callable, Node* context, Node* arg1,
                 Node* arg2, size_t result_size = 1);
  Node* CallStub(Callable const& callable, Node* context, Node* arg1,
                 Node* arg2, Node* arg3, size_t result_size = 1);
  Node* CallStubN(Callable const& callable, Node** args,
                  size_t result_size = 1);

  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, Node* arg1, size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, Node* arg1, Node* arg2, size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, Node* arg1, Node* arg2, Node* arg3,
                 size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, Node* arg1, Node* arg2, Node* arg3, Node* arg4,
                 size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, Node* arg1, Node* arg2, Node* arg3, Node* arg4,
                 Node* arg5, size_t result_size = 1);

  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, const Arg& arg1, const Arg& arg2,
                 size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, const Arg& arg1, const Arg& arg2,
                 const Arg& arg3, size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, const Arg& arg1, const Arg& arg2,
                 const Arg& arg3, const Arg& arg4, size_t result_size = 1);
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, const Arg& arg1, const Arg& arg2,
                 const Arg& arg3, const Arg& arg4, const Arg& arg5,
                 size_t result_size = 1);

  Node* CallStubN(const CallInterfaceDescriptor& descriptor, Node* target,
                  Node** args, size_t result_size = 1);

  Node* TailCallStub(Callable const& callable, Node* context, Node* arg1,
                     size_t result_size = 1);
  Node* TailCallStub(Callable const& callable, Node* context, Node* arg1,
                     Node* arg2, size_t result_size = 1);
  Node* TailCallStub(Callable const& callable, Node* context, Node* arg1,
                     Node* arg2, Node* arg3, size_t result_size = 1);
  Node* TailCallStub(Callable const& callable, Node* context, Node* arg1,
                     Node* arg2, Node* arg3, Node* arg4,
                     size_t result_size = 1);
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, Node* arg1, size_t result_size = 1);
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, Node* arg1, Node* arg2,
                     size_t result_size = 1);
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, Node* arg1, Node* arg2, Node* arg3,
                     size_t result_size = 1);
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, Node* arg1, Node* arg2, Node* arg3,
                     Node* arg4, size_t result_size = 1);

  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, const Arg& arg1, const Arg& arg2,
                     const Arg& arg3, const Arg& arg4, size_t result_size = 1);
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, const Arg& arg1, const Arg& arg2,
                     const Arg& arg3, const Arg& arg4, const Arg& arg5,
                     size_t result_size = 1);

  Node* TailCallBytecodeDispatch(const CallInterfaceDescriptor& descriptor,
                                 Node* code_target_address, Node** args);

  Node* CallJS(Callable const& callable, Node* context, Node* function,
               Node* receiver, size_t result_size = 1);
  Node* CallJS(Callable const& callable, Node* context, Node* function,
               Node* receiver, Node* arg1, size_t result_size = 1);
  Node* CallJS(Callable const& callable, Node* context, Node* function,
               Node* receiver, Node* arg1, Node* arg2, size_t result_size = 1);

  // Exception handling support.
  void GotoIfException(Node* node, Label* if_exception,
                       Variable* exception_var = nullptr);

  // Branching helpers.
  void BranchIf(Node* condition, Label* if_true, Label* if_false);

#define BRANCH_HELPER(name)                                                \
  void BranchIf##name(Node* a, Node* b, Label* if_true, Label* if_false) { \
    BranchIf(name(a, b), if_true, if_false);                               \
  }
  CODE_ASSEMBLER_COMPARE_BINARY_OP_LIST(BRANCH_HELPER)
#undef BRANCH_HELPER

  // Helpers which delegate to RawMachineAssembler.
  Factory* factory() const;
  Isolate* isolate() const;
  Zone* zone() const;

 protected:
  // Enables subclasses to perform operations before and after a call.
  virtual void CallPrologue();
  virtual void CallEpilogue();

 private:
  CodeAssembler(Isolate* isolate, Zone* zone, CallDescriptor* call_descriptor,
                Code::Flags flags, const char* name);

  Node* CallN(CallDescriptor* descriptor, Node* code_target, Node** args);
  Node* TailCallN(CallDescriptor* descriptor, Node* code_target, Node** args);

  std::unique_ptr<RawMachineAssembler> raw_assembler_;
  Code::Flags flags_;
  const char* name_;
  bool code_generated_;
  ZoneSet<Variable::Impl*> variables_;

  DISALLOW_COPY_AND_ASSIGN(CodeAssembler);
};

class CodeAssembler::Label {
 public:
  enum Type { kDeferred, kNonDeferred };

  explicit Label(
      CodeAssembler* assembler,
      CodeAssembler::Label::Type type = CodeAssembler::Label::kNonDeferred)
      : CodeAssembler::Label(assembler, 0, nullptr, type) {}
  Label(CodeAssembler* assembler, CodeAssembler::Variable* merged_variable,
        CodeAssembler::Label::Type type = CodeAssembler::Label::kNonDeferred)
      : CodeAssembler::Label(assembler, 1, &merged_variable, type) {}
  Label(CodeAssembler* assembler, int merged_variable_count,
        CodeAssembler::Variable** merged_variables,
        CodeAssembler::Label::Type type = CodeAssembler::Label::kNonDeferred);
  ~Label() {}

 private:
  friend class CodeAssembler;

  void Bind();
  void MergeVariables();

  bool bound_;
  size_t merge_count_;
  CodeAssembler* assembler_;
  RawMachineLabel* label_;
  // Map of variables that need to be merged to their phi nodes (or placeholders
  // for those phis).
  std::map<Variable::Impl*, Node*> variable_phis_;
  // Map of variables to the list of value nodes that have been added from each
  // merge path in their order of merging.
  std::map<Variable::Impl*, std::vector<Node*>> variable_merges_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_ASSEMBLER_H_
