// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_STUB_ASSEMBLER_H_
#define V8_COMPILER_CODE_STUB_ASSEMBLER_H_

#include <map>

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/allocation.h"
#include "src/builtins.h"
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
class Graph;
class Node;
class Operator;
class RawMachineAssembler;
class RawMachineLabel;
class Schedule;

#define CODE_STUB_ASSEMBLER_COMPARE_BINARY_OP_LIST(V) \
  V(Float32Equal)                                     \
  V(Float32LessThan)                                  \
  V(Float32LessThanOrEqual)                           \
  V(Float32GreaterThan)                               \
  V(Float32GreaterThanOrEqual)                        \
  V(Float64Equal)                                     \
  V(Float64LessThan)                                  \
  V(Float64LessThanOrEqual)                           \
  V(Float64GreaterThan)                               \
  V(Float64GreaterThanOrEqual)                        \
  V(Int32GreaterThan)                                 \
  V(Int32GreaterThanOrEqual)                          \
  V(Int32LessThan)                                    \
  V(Int32LessThanOrEqual)                             \
  V(IntPtrLessThan)                                   \
  V(IntPtrLessThanOrEqual)                            \
  V(Uint32LessThan)                                   \
  V(UintPtrGreaterThanOrEqual)                        \
  V(WordEqual)                                        \
  V(WordNotEqual)                                     \
  V(Word32Equal)                                      \
  V(Word32NotEqual)                                   \
  V(Word64Equal)                                      \
  V(Word64NotEqual)

#define CODE_STUB_ASSEMBLER_BINARY_OP_LIST(V)   \
  CODE_STUB_ASSEMBLER_COMPARE_BINARY_OP_LIST(V) \
  V(Float64Add)                                 \
  V(Float64Sub)                                 \
  V(Float64InsertLowWord32)                     \
  V(Float64InsertHighWord32)                    \
  V(IntPtrAdd)                                  \
  V(IntPtrAddWithOverflow)                      \
  V(IntPtrSub)                                  \
  V(IntPtrSubWithOverflow)                      \
  V(Int32Add)                                   \
  V(Int32AddWithOverflow)                       \
  V(Int32Sub)                                   \
  V(Int32Mul)                                   \
  V(WordOr)                                     \
  V(WordAnd)                                    \
  V(WordXor)                                    \
  V(WordShl)                                    \
  V(WordShr)                                    \
  V(WordSar)                                    \
  V(WordRor)                                    \
  V(Word32Or)                                   \
  V(Word32And)                                  \
  V(Word32Xor)                                  \
  V(Word32Shl)                                  \
  V(Word32Shr)                                  \
  V(Word32Sar)                                  \
  V(Word32Ror)                                  \
  V(Word64Or)                                   \
  V(Word64And)                                  \
  V(Word64Xor)                                  \
  V(Word64Shr)                                  \
  V(Word64Sar)                                  \
  V(Word64Ror)

#define CODE_STUB_ASSEMBLER_UNARY_OP_LIST(V) \
  V(Float64Neg)                              \
  V(Float64Sqrt)                             \
  V(ChangeFloat64ToUint32)                   \
  V(ChangeInt32ToFloat64)                    \
  V(ChangeInt32ToInt64)                      \
  V(ChangeUint32ToFloat64)                   \
  V(ChangeUint32ToUint64)                    \
  V(Word32Clz)

class CodeStubAssembler {
 public:
  // Create with CallStub linkage.
  // |result_size| specifies the number of results returned by the stub.
  // TODO(rmcilroy): move result_size to the CallInterfaceDescriptor.
  CodeStubAssembler(Isolate* isolate, Zone* zone,
                    const CallInterfaceDescriptor& descriptor,
                    Code::Flags flags, const char* name,
                    size_t result_size = 1);

  // Create with JSCall linkage.
  CodeStubAssembler(Isolate* isolate, Zone* zone, int parameter_count,
                    Code::Flags flags, const char* name);

  virtual ~CodeStubAssembler();

  Handle<Code> GenerateCode();

  class Label;
  class Variable {
   public:
    explicit Variable(CodeStubAssembler* assembler, MachineRepresentation rep);
    void Bind(Node* value);
    Node* value() const;
    MachineRepresentation rep() const;
    bool IsBound() const;

   private:
    friend class CodeStubAssembler;
    class Impl;
    Impl* impl_;
  };

  enum AllocationFlag : uint8_t {
    kNone = 0,
    kDoubleAlignment = 1,
    kPretenured = 1 << 1
  };

  typedef base::Flags<AllocationFlag> AllocationFlags;

  // ===========================================================================
  // Base Assembler
  // ===========================================================================

  // Constants.
  Node* Int32Constant(int value);
  Node* IntPtrConstant(intptr_t value);
  Node* NumberConstant(double value);
  Node* SmiConstant(Smi* value);
  Node* HeapConstant(Handle<HeapObject> object);
  Node* BooleanConstant(bool value);
  Node* ExternalConstant(ExternalReference address);
  Node* Float64Constant(double value);
  Node* BooleanMapConstant();
  Node* HeapNumberMapConstant();
  Node* NullConstant();
  Node* UndefinedConstant();

  Node* Parameter(int value);
  void Return(Node* value);

  void Bind(Label* label);
  void Goto(Label* label);
  void GotoIf(Node* condition, Label* true_label);
  void GotoUnless(Node* condition, Label* false_label);
  void Branch(Node* condition, Label* true_label, Label* false_label);

  void Switch(Node* index, Label* default_label, int32_t* case_values,
              Label** case_labels, size_t case_count);

  // Access to the frame pointer
  Node* LoadFramePointer();
  Node* LoadParentFramePointer();

  // Access to the stack pointer
  Node* LoadStackPointer();

  // Load raw memory location.
  Node* Load(MachineType rep, Node* base);
  Node* Load(MachineType rep, Node* base, Node* index);

  // Store value to raw memory location.
  Node* Store(MachineRepresentation rep, Node* base, Node* value);
  Node* Store(MachineRepresentation rep, Node* base, Node* index, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* value);
  Node* StoreNoWriteBarrier(MachineRepresentation rep, Node* base, Node* index,
                            Node* value);

// Basic arithmetic operations.
#define DECLARE_CODE_STUB_ASSEMBER_BINARY_OP(name) Node* name(Node* a, Node* b);
  CODE_STUB_ASSEMBLER_BINARY_OP_LIST(DECLARE_CODE_STUB_ASSEMBER_BINARY_OP)
#undef DECLARE_CODE_STUB_ASSEMBER_BINARY_OP

  Node* WordShl(Node* value, int shift);

// Unary
#define DECLARE_CODE_STUB_ASSEMBER_UNARY_OP(name) Node* name(Node* a);
  CODE_STUB_ASSEMBLER_UNARY_OP_LIST(DECLARE_CODE_STUB_ASSEMBER_UNARY_OP)
#undef DECLARE_CODE_STUB_ASSEMBER_UNARY_OP

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

  Node* CallStub(Callable const& callable, Node* context, Node* arg1,
                 size_t result_size = 1);

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

  Node* TailCallStub(Callable const& callable, Node* context, Node* arg1,
                     Node* arg2, size_t result_size = 1);

  Node* TailCallStub(const CallInterfaceDescriptor& descriptor, Node* target,
                     Node* context, Node* arg1, Node* arg2,
                     size_t result_size = 1);

  Node* TailCall(const CallInterfaceDescriptor& descriptor, Node* target,
                 Node** args, size_t result_size = 1);

  // ===========================================================================
  // Macros
  // ===========================================================================

  // Float64 operations.
  Node* Float64Ceil(Node* x);
  Node* Float64Floor(Node* x);
  Node* Float64Round(Node* x);
  Node* Float64Trunc(Node* x);

  // Tag a Word as a Smi value.
  Node* SmiTag(Node* value);
  // Untag a Smi value as a Word.
  Node* SmiUntag(Node* value);

  // Smi conversions.
  Node* SmiToFloat64(Node* value);
  Node* SmiToWord32(Node* value);

  // Smi operations.
  Node* SmiAdd(Node* a, Node* b);
  Node* SmiAddWithOverflow(Node* a, Node* b);
  Node* SmiSub(Node* a, Node* b);
  Node* SmiSubWithOverflow(Node* a, Node* b);
  Node* SmiEqual(Node* a, Node* b);
  Node* SmiLessThan(Node* a, Node* b);
  Node* SmiLessThanOrEqual(Node* a, Node* b);
  Node* SmiMin(Node* a, Node* b);

  // Load a value from the root array.
  Node* LoadRoot(Heap::RootListIndex root_index);

  // Check a value for smi-ness
  Node* WordIsSmi(Node* a);

  // Check that the value is a positive smi.
  Node* WordIsPositiveSmi(Node* a);

  // Load an object pointer from a buffer that isn't in the heap.
  Node* LoadBufferObject(Node* buffer, int offset,
                         MachineType rep = MachineType::AnyTagged());
  // Load a field from an object on the heap.
  Node* LoadObjectField(Node* object, int offset,
                        MachineType rep = MachineType::AnyTagged());
  // Load the floating point value of a HeapNumber.
  Node* LoadHeapNumberValue(Node* object);
  // Store the floating point value of a HeapNumber.
  Node* StoreHeapNumberValue(Node* object, Node* value);
  // Truncate the floating point value of a HeapNumber to an Int32.
  Node* TruncateHeapNumberValueToWord32(Node* object);
  // Load the bit field of a Map.
  Node* LoadMapBitField(Node* map);
  // Load bit field 2 of a map.
  Node* LoadMapBitField2(Node* map);
  // Load bit field 3 of a map.
  Node* LoadMapBitField3(Node* map);
  // Load the instance type of a map.
  Node* LoadMapInstanceType(Node* map);
  // Load the instance descriptors of a map.
  Node* LoadMapDescriptors(Node* map);

  // Load the hash field of a name.
  Node* LoadNameHash(Node* name);

  // Load an array element from a FixedArray.
  Node* LoadFixedArrayElementInt32Index(Node* object, Node* int32_index,
                                        int additional_offset = 0);
  Node* LoadFixedArrayElementSmiIndex(Node* object, Node* smi_index,
                                      int additional_offset = 0);
  Node* LoadFixedArrayElementConstantIndex(Node* object, int index);

  // Allocate an object of the given size.
  Node* Allocate(int size, AllocationFlags flags = kNone);
  // Allocate a HeapNumber without initializing its value.
  Node* AllocateHeapNumber();
  // Allocate a HeapNumber with a specific value.
  Node* AllocateHeapNumberWithValue(Node* value);

  // Store an array element to a FixedArray.
  Node* StoreFixedArrayElementNoWriteBarrier(Node* object, Node* index,
                                             Node* value);
  // Load the Map of an HeapObject.
  Node* LoadMap(Node* object);
  // Store the Map of an HeapObject.
  Node* StoreMapNoWriteBarrier(Node* object, Node* map);
  // Load the instance type of an HeapObject.
  Node* LoadInstanceType(Node* object);

  // Load the elements backing store of a JSObject.
  Node* LoadElements(Node* object);
  // Load the length of a fixed array base instance.
  Node* LoadFixedArrayBaseLength(Node* array);

  // Returns a node that is true if the given bit is set in |word32|.
  template <typename T>
  Node* BitFieldDecode(Node* word32) {
    return BitFieldDecode(word32, T::kShift, T::kMask);
  }

  Node* BitFieldDecode(Node* word32, uint32_t shift, uint32_t mask);

  // Conversions.
  Node* ChangeFloat64ToTagged(Node* value);
  Node* ChangeInt32ToTagged(Node* value);
  Node* TruncateTaggedToFloat64(Node* context, Node* value);
  Node* TruncateTaggedToWord32(Node* context, Node* value);

  // Branching helpers.
  // TODO(danno): Can we be more cleverish wrt. edge-split?
  void BranchIf(Node* condition, Label* if_true, Label* if_false);

#define BRANCH_HELPER(name)                                                \
  void BranchIf##name(Node* a, Node* b, Label* if_true, Label* if_false) { \
    BranchIf(name(a, b), if_true, if_false);                               \
  }
  CODE_STUB_ASSEMBLER_COMPARE_BINARY_OP_LIST(BRANCH_HELPER)
#undef BRANCH_HELPER

  void BranchIfSmiLessThan(Node* a, Node* b, Label* if_true, Label* if_false) {
    BranchIf(SmiLessThan(a, b), if_true, if_false);
  }

  void BranchIfSmiLessThanOrEqual(Node* a, Node* b, Label* if_true,
                                  Label* if_false) {
    BranchIf(SmiLessThanOrEqual(a, b), if_true, if_false);
  }

  void BranchIfFloat64IsNaN(Node* value, Label* if_true, Label* if_false) {
    BranchIfFloat64Equal(value, value, if_false, if_true);
  }

  // Helpers which delegate to RawMachineAssembler.
  Factory* factory() const;
  Isolate* isolate() const;
  Zone* zone() const;

 protected:
  // Protected helpers which delegate to RawMachineAssembler.
  Graph* graph() const;

  // Enables subclasses to perform operations before and after a call.
  virtual void CallPrologue();
  virtual void CallEpilogue();

 private:
  friend class CodeStubAssemblerTester;

  CodeStubAssembler(Isolate* isolate, Zone* zone,
                    CallDescriptor* call_descriptor, Code::Flags flags,
                    const char* name);

  Node* CallN(CallDescriptor* descriptor, Node* code_target, Node** args);
  Node* TailCallN(CallDescriptor* descriptor, Node* code_target, Node** args);

  Node* SmiShiftBitsConstant();

  Node* AllocateRawAligned(Node* size_in_bytes, AllocationFlags flags,
                           Node* top_address, Node* limit_address);
  Node* AllocateRawUnaligned(Node* size_in_bytes, AllocationFlags flags,
                             Node* top_adddress, Node* limit_address);

  base::SmartPointer<RawMachineAssembler> raw_assembler_;
  Code::Flags flags_;
  const char* name_;
  bool code_generated_;
  ZoneVector<Variable::Impl*> variables_;

  DISALLOW_COPY_AND_ASSIGN(CodeStubAssembler);
};

DEFINE_OPERATORS_FOR_FLAGS(CodeStubAssembler::AllocationFlags);

class CodeStubAssembler::Label {
 public:
  enum Type { kDeferred, kNonDeferred };

  explicit Label(CodeStubAssembler* assembler,
                 CodeStubAssembler::Label::Type type =
                     CodeStubAssembler::Label::kNonDeferred)
      : CodeStubAssembler::Label(assembler, 0, nullptr, type) {}
  Label(CodeStubAssembler* assembler,
        CodeStubAssembler::Variable* merged_variable,
        CodeStubAssembler::Label::Type type =
            CodeStubAssembler::Label::kNonDeferred)
      : CodeStubAssembler::Label(assembler, 1, &merged_variable, type) {}
  Label(CodeStubAssembler* assembler, int merged_variable_count,
        CodeStubAssembler::Variable** merged_variables,
        CodeStubAssembler::Label::Type type =
            CodeStubAssembler::Label::kNonDeferred);
  ~Label() {}

 private:
  friend class CodeStubAssembler;

  void Bind();
  void MergeVariables();

  bool bound_;
  size_t merge_count_;
  CodeStubAssembler* assembler_;
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

#endif  // V8_COMPILER_CODE_STUB_ASSEMBLER_H_
