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
#include "src/code-factory.h"
#include "src/globals.h"
#include "src/heap/heap.h"
#include "src/machine-type.h"
#include "src/runtime/runtime.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Callable;
class CallInterfaceDescriptor;
class Isolate;
class Factory;
class Zone;

namespace compiler {

class CallDescriptor;
class CodeAssemblerLabel;
class CodeAssemblerVariable;
class CodeAssemblerState;
class Node;
class RawMachineAssembler;
class RawMachineLabel;

typedef ZoneList<CodeAssemblerVariable*> CodeAssemblerVariableList;

typedef std::function<void()> CodeAssemblerCallback;

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
  V(Uint32LessThanOrEqual)                       \
  V(Uint32GreaterThanOrEqual)                    \
  V(UintPtrLessThan)                             \
  V(UintPtrLessThanOrEqual)                      \
  V(UintPtrGreaterThan)                          \
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
  V(IntPtrAddWithOverflow)                 \
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
  V(BitcastTaggedToWord)                \
  V(BitcastWordToTagged)                \
  V(BitcastWordToTaggedSigned)          \
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
  V(RoundInt32ToFloat32)                \
  V(Float64SilenceNaN)                  \
  V(Float64RoundDown)                   \
  V(Float64RoundUp)                     \
  V(Float64RoundTiesEven)               \
  V(Float64RoundTruncate)               \
  V(Word32Clz)                          \
  V(Word32Not)                          \
  V(Word32BinaryNot)

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
//
// The CodeAssembler itself is stateless (and instances are expected to be
// temporary-scoped and short-lived); all its state is encapsulated into
// a CodeAssemblerState instance.
class V8_EXPORT_PRIVATE CodeAssembler {
 public:
  explicit CodeAssembler(CodeAssemblerState* state) : state_(state) {}
  ~CodeAssembler();

  static Handle<Code> GenerateCode(CodeAssemblerState* state);

  bool Is64() const;
  bool IsFloat64RoundUpSupported() const;
  bool IsFloat64RoundDownSupported() const;
  bool IsFloat64RoundTiesEvenSupported() const;
  bool IsFloat64RoundTruncateSupported() const;

  // Shortened aliases for use in CodeAssembler subclasses.
  typedef CodeAssemblerLabel Label;
  typedef CodeAssemblerVariable Variable;
  typedef CodeAssemblerVariableList VariableList;

  // ===========================================================================
  // Base Assembler
  // ===========================================================================

  // Constants.
  Node* Int32Constant(int32_t value);
  Node* Int64Constant(int64_t value);
  Node* IntPtrConstant(intptr_t value);
  Node* NumberConstant(double value);
  Node* SmiConstant(Smi* value);
  Node* SmiConstant(int value);
  Node* HeapConstant(Handle<HeapObject> object);
  Node* BooleanConstant(bool value);
  Node* ExternalConstant(ExternalReference address);
  Node* Float64Constant(double value);
  Node* NaNConstant();

  bool ToInt32Constant(Node* node, int32_t& out_value);
  bool ToInt64Constant(Node* node, int64_t& out_value);
  bool ToSmiConstant(Node* node, Smi*& out_value);
  bool ToIntPtrConstant(Node* node, intptr_t& out_value);

  Node* Parameter(int value);
  void Return(Node* value);
  void PopAndReturn(Node* pop, Node* value);

  void DebugBreak();
  void Comment(const char* format, ...);

  void Bind(Label* label);
  void Goto(Label* label);
  void GotoIf(Node* condition, Label* true_label);
  void GotoUnless(Node* condition, Label* false_label);
  void Branch(Node* condition, Label* true_label, Label* false_label);

  void Switch(Node* index, Label* default_label, const int32_t* case_values,
              Label** case_labels, size_t case_count);

  // Access to the frame pointer
  Node* LoadFramePointer();
  Node* LoadParentFramePointer();

  // Access to the stack pointer
  Node* LoadStackPointer();

  // Load raw memory location.
  Node* Load(MachineType rep, Node* base);
  Node* Load(MachineType rep, Node* base, Node* offset);
  Node* AtomicLoad(MachineType rep, Node* base, Node* offset);

  // Load a value from the root array.
  Node* LoadRoot(Heap::RootListIndex root_index);

  // Store value to raw memory location.
  Node* Store(Node* base, Node* value);
  Node* Store(Node* base, Node* offset, Node* value);
  Node* StoreWithMapWriteBarrier(Node* base, Node* offset, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* offset,
                            Node* value);
  Node* AtomicStore(MachineRepresentation rep, Node* base, Node* offset,
                    Node* value);

  // Store a value to the root array.
  Node* StoreRoot(Heap::RootListIndex root_index, Node* value);

// Basic arithmetic operations.
#define DECLARE_CODE_ASSEMBLER_BINARY_OP(name) Node* name(Node* a, Node* b);
  CODE_ASSEMBLER_BINARY_OP_LIST(DECLARE_CODE_ASSEMBLER_BINARY_OP)
#undef DECLARE_CODE_ASSEMBLER_BINARY_OP

  Node* IntPtrAdd(Node* left, Node* right);
  Node* IntPtrSub(Node* left, Node* right);

  Node* WordShl(Node* value, int shift);
  Node* WordShr(Node* value, int shift);
  Node* Word32Shr(Node* value, int shift);

// Unary
#define DECLARE_CODE_ASSEMBLER_UNARY_OP(name) Node* name(Node* a);
  CODE_ASSEMBLER_UNARY_OP_LIST(DECLARE_CODE_ASSEMBLER_UNARY_OP)
#undef DECLARE_CODE_ASSEMBLER_UNARY_OP

  // Changes an intptr_t to a double, e.g. for storing an element index
  // outside Smi range in a HeapNumber. Lossless on 32-bit,
  // rounds on 64-bit (which doesn't affect valid element indices).
  Node* RoundIntPtrToFloat64(Node* value);
  // No-op on 32-bit, otherwise zero extend.
  Node* ChangeUint32ToWord(Node* value);
  // No-op on 32-bit, otherwise sign extend.
  Node* ChangeInt32ToIntPtr(Node* value);

  // No-op that guarantees that the value is kept alive till this point even
  // if GC happens.
  Node* Retain(Node* value);

  // Projections
  Node* Projection(int index, Node* value);

  // Calls
  template <class... TArgs>
  Node* CallRuntime(Runtime::FunctionId function, Node* context, TArgs... args);

  template <class... TArgs>
  Node* TailCallRuntime(Runtime::FunctionId function, Node* context,
                        TArgs... args);

  template <class... TArgs>
  Node* CallStub(Callable const& callable, Node* context, TArgs... args) {
    Node* target = HeapConstant(callable.code());
    return CallStub(callable.descriptor(), target, context, args...);
  }

  template <class... TArgs>
  Node* CallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node* context, TArgs... args) {
    return CallStubR(descriptor, 1, target, context, args...);
  }

  template <class... TArgs>
  Node* CallStubR(const CallInterfaceDescriptor& descriptor, size_t result_size,
                  Node* target, Node* context, TArgs... args);

  Node* CallStubN(const CallInterfaceDescriptor& descriptor, size_t result_size,
                  int input_count, Node* const* inputs);

  template <class... TArgs>
  Node* TailCallStub(Callable const& callable, Node* context, TArgs... args) {
    Node* target = HeapConstant(callable.code());
    return TailCallStub(callable.descriptor(), target, context, args...);
  }

  template <class... TArgs>
  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, TArgs... args);

  template <class... TArgs>
  Node* TailCallBytecodeDispatch(const CallInterfaceDescriptor& descriptor,
                                 Node* target, TArgs... args);

  template <class... TArgs>
  Node* CallJS(Callable const& callable, Node* context, Node* function,
               Node* receiver, TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    Node* arity = Int32Constant(argc);
    return CallStub(callable, context, function, arity, receiver, args...);
  }

  template <class... TArgs>
  Node* ConstructJS(Callable const& callable, Node* context, Node* new_target,
                    TArgs... args) {
    int argc = static_cast<int>(sizeof...(args));
    Node* arity = Int32Constant(argc);
    Node* receiver = LoadRoot(Heap::kUndefinedValueRootIndex);

    // Construct(target, new_target, arity, receiver, arguments...)
    return CallStub(callable, context, new_target, new_target, arity, receiver,
                    args...);
  }

  // Call to a C function with two arguments.
  Node* CallCFunction2(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, Node* function, Node* arg0,
                       Node* arg1);

  // Call to a C function with three arguments.
  Node* CallCFunction3(MachineType return_type, MachineType arg0_type,
                       MachineType arg1_type, MachineType arg2_type,
                       Node* function, Node* arg0, Node* arg1, Node* arg2);

  // Exception handling support.
  void GotoIfException(Node* node, Label* if_exception,
                       Variable* exception_var = nullptr);

  // Helpers which delegate to RawMachineAssembler.
  Factory* factory() const;
  Isolate* isolate() const;
  Zone* zone() const;

  CodeAssemblerState* state() { return state_; }

  void BreakOnNode(int node_id);

 protected:
  void RegisterCallGenerationCallbacks(
      const CodeAssemblerCallback& call_prologue,
      const CodeAssemblerCallback& call_epilogue);
  void UnregisterCallGenerationCallbacks();

 private:
  RawMachineAssembler* raw_assembler() const;

  // Calls respective callback registered in the state.
  void CallPrologue();
  void CallEpilogue();

  CodeAssemblerState* state_;

  DISALLOW_COPY_AND_ASSIGN(CodeAssembler);
};

class CodeAssemblerVariable {
 public:
  explicit CodeAssemblerVariable(CodeAssembler* assembler,
                                 MachineRepresentation rep);
  ~CodeAssemblerVariable();
  void Bind(Node* value);
  Node* value() const;
  MachineRepresentation rep() const;
  bool IsBound() const;

 private:
  friend class CodeAssemblerLabel;
  friend class CodeAssemblerState;
  class Impl;
  Impl* impl_;
  CodeAssemblerState* state_;
};

class CodeAssemblerLabel {
 public:
  enum Type { kDeferred, kNonDeferred };

  explicit CodeAssemblerLabel(
      CodeAssembler* assembler,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, 0, nullptr, type) {}
  CodeAssemblerLabel(
      CodeAssembler* assembler,
      const CodeAssemblerVariableList& merged_variables,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, merged_variables.length(),
                           &(merged_variables[0]), type) {}
  CodeAssemblerLabel(
      CodeAssembler* assembler, size_t count, CodeAssemblerVariable** vars,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred);
  CodeAssemblerLabel(
      CodeAssembler* assembler, CodeAssemblerVariable* merged_variable,
      CodeAssemblerLabel::Type type = CodeAssemblerLabel::kNonDeferred)
      : CodeAssemblerLabel(assembler, 1, &merged_variable, type) {}
  ~CodeAssemblerLabel() {}

 private:
  friend class CodeAssembler;

  void Bind();
  void MergeVariables();

  bool bound_;
  size_t merge_count_;
  CodeAssemblerState* state_;
  RawMachineLabel* label_;
  // Map of variables that need to be merged to their phi nodes (or placeholders
  // for those phis).
  std::map<CodeAssemblerVariable::Impl*, Node*> variable_phis_;
  // Map of variables to the list of value nodes that have been added from each
  // merge path in their order of merging.
  std::map<CodeAssemblerVariable::Impl*, std::vector<Node*>> variable_merges_;
};

class V8_EXPORT_PRIVATE CodeAssemblerState {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeAssemblerState(Isolate* isolate, Zone* zone,
                     const CallInterfaceDescriptor& descriptor,
                     Code::Flags flags, const char* name,
                     size_t result_size = 1);

  // Create with JSCall linkage.
  CodeAssemblerState(Isolate* isolate, Zone* zone, int parameter_count,
                     Code::Flags flags, const char* name);

  ~CodeAssemblerState();

  const char* name() const { return name_; }
  int parameter_count() const;

 private:
  friend class CodeAssembler;
  friend class CodeAssemblerLabel;
  friend class CodeAssemblerVariable;

  CodeAssemblerState(Isolate* isolate, Zone* zone,
                     CallDescriptor* call_descriptor, Code::Flags flags,
                     const char* name);

  std::unique_ptr<RawMachineAssembler> raw_assembler_;
  Code::Flags flags_;
  const char* name_;
  bool code_generated_;
  ZoneSet<CodeAssemblerVariable::Impl*> variables_;
  CodeAssemblerCallback call_prologue_;
  CodeAssemblerCallback call_epilogue_;

  DISALLOW_COPY_AND_ASSIGN(CodeAssemblerState);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_ASSEMBLER_H_
