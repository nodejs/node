// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_CODE_STUB_ASSEMBLER_H_
#define V8_COMPILER_CODE_STUB_ASSEMBLER_H_

// Clients of this interface shouldn't depend on lots of compiler internals.
// Do not include anything from src/compiler here!
#include "src/allocation.h"
#include "src/builtins.h"
#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

class CallInterfaceDescriptor;
class Isolate;
class Zone;

namespace compiler {

class CallDescriptor;
class Graph;
class Node;
class Operator;
class RawMachineAssembler;
class Schedule;

class CodeStubAssembler {
 public:
  CodeStubAssembler(Isolate* isolate, Zone* zone,
                    const CallInterfaceDescriptor& descriptor, Code::Kind kind,
                    const char* name);
  virtual ~CodeStubAssembler();

  Handle<Code> GenerateCode();

  // Constants.
  Node* Int32Constant(int value);
  Node* IntPtrConstant(intptr_t value);
  Node* NumberConstant(double value);
  Node* HeapConstant(Handle<HeapObject> object);
  Node* BooleanConstant(bool value);

  Node* Parameter(int value);
  void Return(Node* value);

  // Tag and untag Smi values.
  Node* SmiTag(Node* value);
  Node* SmiUntag(Node* value);

  // Basic arithmetic operations.
  Node* IntPtrAdd(Node* a, Node* b);
  Node* IntPtrSub(Node* a, Node* b);
  Node* WordShl(Node* value, int shift);

  // Load a field from an object on the heap.
  Node* LoadObjectField(Node* object, int offset);

  // Call runtime function.
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1);
  Node* CallRuntime(Runtime::FunctionId function_id, Node* context, Node* arg1,
                    Node* arg2);

  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1);
  Node* TailCallRuntime(Runtime::FunctionId function_id, Node* context,
                        Node* arg1, Node* arg2);

 private:
  friend class CodeStubAssemblerTester;

  Node* CallN(CallDescriptor* descriptor, Node* code_target, Node** args);
  Node* TailCallN(CallDescriptor* descriptor, Node* code_target, Node** args);

  Node* SmiShiftBitsConstant();

  // Private helpers which delegate to RawMachineAssembler.
  Graph* graph();
  Isolate* isolate();
  Zone* zone();

  base::SmartPointer<RawMachineAssembler> raw_assembler_;
  Code::Kind kind_;
  const char* name_;
  bool code_generated_;

  DISALLOW_COPY_AND_ASSIGN(CodeStubAssembler);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_CODE_STUB_ASSEMBLER_H_
