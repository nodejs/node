// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_CODEGEN_TESTER_H_
#define V8_CCTEST_COMPILER_CODEGEN_TESTER_H_

#include "src/v8.h"

#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/structured-machine-assembler.h"
#include "src/simulator.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename MachineAssembler>
class MachineAssemblerTester : public HandleAndZoneScope,
                               public CallHelper,
                               public MachineAssembler {
 public:
  MachineAssemblerTester(MachineType return_type, MachineType p0,
                         MachineType p1, MachineType p2, MachineType p3,
                         MachineType p4)
      : HandleAndZoneScope(),
        CallHelper(main_isolate()),
        MachineAssembler(new (main_zone()) Graph(main_zone()),
                         ToCallDescriptorBuilder(main_zone(), return_type, p0,
                                                 p1, p2, p3, p4),
                         MachineOperatorBuilder::pointer_rep()) {}

  Node* LoadFromPointer(void* address, MachineType rep, int32_t offset = 0) {
    return this->Load(rep, this->PointerConstant(address),
                      this->Int32Constant(offset));
  }

  void StoreToPointer(void* address, MachineType rep, Node* node) {
    this->Store(rep, this->PointerConstant(address), node);
  }

  Node* StringConstant(const char* string) {
    return this->HeapConstant(
        this->isolate()->factory()->InternalizeUtf8String(string));
  }

  void CheckNumber(double expected, Object* number) {
    CHECK(this->isolate()->factory()->NewNumber(expected)->SameValue(number));
  }

  void CheckString(const char* expected, Object* string) {
    CHECK(
        this->isolate()->factory()->InternalizeUtf8String(expected)->SameValue(
            string));
  }

  void GenerateCode() { Generate(); }

 protected:
  virtual void VerifyParameters(int parameter_count,
                                MachineType* parameter_types) {
    CHECK_EQ(this->parameter_count(), parameter_count);
    const MachineType* expected_types = this->parameter_types();
    for (int i = 0; i < parameter_count; i++) {
      CHECK_EQ(expected_types[i], parameter_types[i]);
    }
  }

  virtual byte* Generate() {
    if (code_.is_null()) {
      Schedule* schedule = this->Export();
      CallDescriptor* call_descriptor = this->call_descriptor();
      Graph* graph = this->graph();
      CompilationInfo info(graph->zone()->isolate(), graph->zone());
      Linkage linkage(&info, call_descriptor);
      Pipeline pipeline(&info);
      code_ = pipeline.GenerateCodeForMachineGraph(&linkage, graph, schedule);
    }
    return this->code_.ToHandleChecked()->entry();
  }

 private:
  MaybeHandle<Code> code_;
};


template <typename ReturnType>
class RawMachineAssemblerTester
    : public MachineAssemblerTester<RawMachineAssembler>,
      public CallHelper2<ReturnType, RawMachineAssemblerTester<ReturnType> > {
 public:
  RawMachineAssemblerTester(MachineType p0 = kMachineLast,
                            MachineType p1 = kMachineLast,
                            MachineType p2 = kMachineLast,
                            MachineType p3 = kMachineLast,
                            MachineType p4 = kMachineLast)
      : MachineAssemblerTester<RawMachineAssembler>(
            ReturnValueTraits<ReturnType>::Representation(), p0, p1, p2, p3,
            p4) {}

  template <typename Ci, typename Fn>
  void Run(const Ci& ci, const Fn& fn) {
    typename Ci::const_iterator i;
    for (i = ci.begin(); i != ci.end(); ++i) {
      CHECK_EQ(fn(*i), this->Call(*i));
    }
  }

  template <typename Ci, typename Cj, typename Fn>
  void Run(const Ci& ci, const Cj& cj, const Fn& fn) {
    typename Ci::const_iterator i;
    typename Cj::const_iterator j;
    for (i = ci.begin(); i != ci.end(); ++i) {
      for (j = cj.begin(); j != cj.end(); ++j) {
        CHECK_EQ(fn(*i, *j), this->Call(*i, *j));
      }
    }
  }
};


template <typename ReturnType>
class StructuredMachineAssemblerTester
    : public MachineAssemblerTester<StructuredMachineAssembler>,
      public CallHelper2<ReturnType,
                         StructuredMachineAssemblerTester<ReturnType> > {
 public:
  StructuredMachineAssemblerTester(MachineType p0 = kMachineLast,
                                   MachineType p1 = kMachineLast,
                                   MachineType p2 = kMachineLast,
                                   MachineType p3 = kMachineLast,
                                   MachineType p4 = kMachineLast)
      : MachineAssemblerTester<StructuredMachineAssembler>(
            ReturnValueTraits<ReturnType>::Representation(), p0, p1, p2, p3,
            p4) {}
};


static const bool USE_RESULT_BUFFER = true;
static const bool USE_RETURN_REGISTER = false;
static const int32_t CHECK_VALUE = 0x99BEEDCE;


// TODO(titzer): use the C-style calling convention, or any register-based
// calling convention for binop tests.
template <typename CType, MachineType rep, bool use_result_buffer>
class BinopTester {
 public:
  explicit BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : T(tester),
        param0(T->LoadFromPointer(&p0, rep)),
        param1(T->LoadFromPointer(&p1, rep)),
        p0(static_cast<CType>(0)),
        p1(static_cast<CType>(0)),
        result(static_cast<CType>(0)) {}

  RawMachineAssemblerTester<int32_t>* T;
  Node* param0;
  Node* param1;

  CType call(CType a0, CType a1) {
    p0 = a0;
    p1 = a1;
    if (use_result_buffer) {
      CHECK_EQ(CHECK_VALUE, T->Call());
      return result;
    } else {
      return T->Call();
    }
  }

  void AddReturn(Node* val) {
    if (use_result_buffer) {
      T->Store(rep, T->PointerConstant(&result), T->Int32Constant(0), val);
      T->Return(T->Int32Constant(CHECK_VALUE));
    } else {
      T->Return(val);
    }
  }

  template <typename Ci, typename Cj, typename Fn>
  void Run(const Ci& ci, const Cj& cj, const Fn& fn) {
    typename Ci::const_iterator i;
    typename Cj::const_iterator j;
    for (i = ci.begin(); i != ci.end(); ++i) {
      for (j = cj.begin(); j != cj.end(); ++j) {
        CHECK_EQ(fn(*i, *j), this->call(*i, *j));
      }
    }
  }

 protected:
  CType p0;
  CType p1;
  CType result;
};


// A helper class for testing code sequences that take two int parameters and
// return an int value.
class Int32BinopTester
    : public BinopTester<int32_t, kMachineWord32, USE_RETURN_REGISTER> {
 public:
  explicit Int32BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<int32_t, kMachineWord32, USE_RETURN_REGISTER>(tester) {}

  int32_t call(uint32_t a0, uint32_t a1) {
    p0 = static_cast<int32_t>(a0);
    p1 = static_cast<int32_t>(a1);
    return T->Call();
  }
};


// A helper class for testing code sequences that take two double parameters and
// return a double value.
// TODO(titzer): figure out how to return doubles correctly on ia32.
class Float64BinopTester
    : public BinopTester<double, kMachineFloat64, USE_RESULT_BUFFER> {
 public:
  explicit Float64BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<double, kMachineFloat64, USE_RESULT_BUFFER>(tester) {}
};


// A helper class for testing code sequences that take two pointer parameters
// and return a pointer value.
// TODO(titzer): pick word size of pointers based on V8_TARGET.
template <typename Type>
class PointerBinopTester
    : public BinopTester<Type*, kMachineWord32, USE_RETURN_REGISTER> {
 public:
  explicit PointerBinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<Type*, kMachineWord32, USE_RETURN_REGISTER>(tester) {}
};


// A helper class for testing code sequences that take two tagged parameters and
// return a tagged value.
template <typename Type>
class TaggedBinopTester
    : public BinopTester<Type*, kMachineTagged, USE_RETURN_REGISTER> {
 public:
  explicit TaggedBinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<Type*, kMachineTagged, USE_RETURN_REGISTER>(tester) {}
};

// A helper class for testing compares. Wraps a machine opcode and provides
// evaluation routines and the operators.
class CompareWrapper {
 public:
  explicit CompareWrapper(IrOpcode::Value op) : opcode(op) {}

  Node* MakeNode(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) {
    return m->NewNode(op(m->machine()), a, b);
  }

  Operator* op(MachineOperatorBuilder* machine) {
    switch (opcode) {
      case IrOpcode::kWord32Equal:
        return machine->Word32Equal();
      case IrOpcode::kInt32LessThan:
        return machine->Int32LessThan();
      case IrOpcode::kInt32LessThanOrEqual:
        return machine->Int32LessThanOrEqual();
      case IrOpcode::kUint32LessThan:
        return machine->Uint32LessThan();
      case IrOpcode::kUint32LessThanOrEqual:
        return machine->Uint32LessThanOrEqual();
      case IrOpcode::kFloat64Equal:
        return machine->Float64Equal();
      case IrOpcode::kFloat64LessThan:
        return machine->Float64LessThan();
      case IrOpcode::kFloat64LessThanOrEqual:
        return machine->Float64LessThanOrEqual();
      default:
        UNREACHABLE();
    }
    return NULL;
  }

  bool Int32Compare(int32_t a, int32_t b) {
    switch (opcode) {
      case IrOpcode::kWord32Equal:
        return a == b;
      case IrOpcode::kInt32LessThan:
        return a < b;
      case IrOpcode::kInt32LessThanOrEqual:
        return a <= b;
      case IrOpcode::kUint32LessThan:
        return static_cast<uint32_t>(a) < static_cast<uint32_t>(b);
      case IrOpcode::kUint32LessThanOrEqual:
        return static_cast<uint32_t>(a) <= static_cast<uint32_t>(b);
      default:
        UNREACHABLE();
    }
    return false;
  }

  bool Float64Compare(double a, double b) {
    switch (opcode) {
      case IrOpcode::kFloat64Equal:
        return a == b;
      case IrOpcode::kFloat64LessThan:
        return a < b;
      case IrOpcode::kFloat64LessThanOrEqual:
        return a <= b;
      default:
        UNREACHABLE();
    }
    return false;
  }

  IrOpcode::Value opcode;
};


// A small closure class to generate code for a function of two inputs that
// produces a single output so that it can be used in many different contexts.
// The {expected()} method should compute the expected output for a given
// pair of inputs.
template <typename T>
class BinopGen {
 public:
  virtual void gen(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) = 0;
  virtual T expected(T a, T b) = 0;
  virtual ~BinopGen() {}
};

// A helper class to generate various combination of input shape combinations
// and run the generated code to ensure it produces the correct results.
class Int32BinopInputShapeTester {
 public:
  explicit Int32BinopInputShapeTester(BinopGen<int32_t>* g) : gen(g) {}

  void TestAllInputShapes();

 private:
  BinopGen<int32_t>* gen;
  int32_t input_a;
  int32_t input_b;

  void Run(RawMachineAssemblerTester<int32_t>* m);
  void RunLeft(RawMachineAssemblerTester<int32_t>* m);
  void RunRight(RawMachineAssemblerTester<int32_t>* m);
};
}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_CCTEST_COMPILER_CODEGEN_TESTER_H_
