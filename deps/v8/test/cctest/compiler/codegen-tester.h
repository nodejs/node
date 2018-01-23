// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CCTEST_COMPILER_CODEGEN_TESTER_H_
#define V8_CCTEST_COMPILER_CODEGEN_TESTER_H_

#include "src/compilation-info.h"
#include "src/compiler/instruction-selector.h"
#include "src/compiler/pipeline.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/simulator.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/call-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

template <typename ReturnType>
class RawMachineAssemblerTester : public HandleAndZoneScope,
                                  public CallHelper<ReturnType>,
                                  public RawMachineAssembler {
 public:
  template <typename... ParamMachTypes>
  explicit RawMachineAssemblerTester(ParamMachTypes... p)
      : HandleAndZoneScope(),
        CallHelper<ReturnType>(
            main_isolate(),
            CSignature::New(main_zone(), MachineTypeForC<ReturnType>(), p...)),
        RawMachineAssembler(
            main_isolate(), new (main_zone()) Graph(main_zone()),
            Linkage::GetSimplifiedCDescriptor(
                main_zone(),
                CSignature::New(main_zone(), MachineTypeForC<ReturnType>(),
                                p...),
                true),
            MachineType::PointerRepresentation(),
            InstructionSelector::SupportedMachineOperatorFlags(),
            InstructionSelector::AlignmentRequirements()) {}

  virtual ~RawMachineAssemblerTester() {}

  void CheckNumber(double expected, Object* number) {
    CHECK(this->isolate()->factory()->NewNumber(expected)->SameValue(number));
  }

  void CheckString(const char* expected, Object* string) {
    CHECK(
        this->isolate()->factory()->InternalizeUtf8String(expected)->SameValue(
            string));
  }

  void GenerateCode() { Generate(); }

  Handle<Code> GetCode() {
    Generate();
    return code_.ToHandleChecked();
  }

 protected:
  virtual byte* Generate() {
    if (code_.is_null()) {
      Schedule* schedule = this->Export();
      CallDescriptor* call_descriptor = this->call_descriptor();
      Graph* graph = this->graph();
      CompilationInfo info(ArrayVector("testing"), main_zone(), Code::STUB);
      code_ = Pipeline::GenerateCodeForTesting(
          &info, main_isolate(), call_descriptor, graph, schedule);
    }
    return this->code_.ToHandleChecked()->entry();
  }

 private:
  MaybeHandle<Code> code_;
};

template <typename ReturnType>
class BufferedRawMachineAssemblerTester
    : public RawMachineAssemblerTester<int32_t> {
 public:
  template <typename... ParamMachTypes>
  explicit BufferedRawMachineAssemblerTester(ParamMachTypes... p)
      : RawMachineAssemblerTester<int32_t>(
            MachineType::Pointer(), ((void)p, MachineType::Pointer())...),
        test_graph_signature_(
            CSignature::New(this->main_zone(), MachineType::Int32(), p...)),
        return_parameter_index_(sizeof...(p)) {
    static_assert(sizeof...(p) <= arraysize(parameter_nodes_),
                  "increase parameter_nodes_ array");
    std::array<MachineType, sizeof...(p)> p_arr{{p...}};
    for (size_t i = 0; i < p_arr.size(); ++i) {
      parameter_nodes_[i] = Load(p_arr[i], RawMachineAssembler::Parameter(i));
    }
  }

  virtual byte* Generate() { return RawMachineAssemblerTester::Generate(); }

  // The BufferedRawMachineAssemblerTester does not pass parameters directly
  // to the constructed IR graph. Instead it passes a pointer to the parameter
  // to the IR graph, and adds Load nodes to the IR graph to load the
  // parameters from memory. Thereby it is possible to pass 64 bit parameters
  // to the IR graph.
  Node* Parameter(size_t index) {
    CHECK_GT(arraysize(parameter_nodes_), index);
    return parameter_nodes_[index];
  }

  // The BufferedRawMachineAssemblerTester adds a Store node to the IR graph
  // to store the graph's return value in memory. The memory address for the
  // Store node is provided as a parameter. By storing the return value in
  // memory it is possible to return 64 bit values.
  void Return(Node* input) {
    Store(MachineTypeForC<ReturnType>().representation(),
          RawMachineAssembler::Parameter(return_parameter_index_), input,
          kNoWriteBarrier);
    RawMachineAssembler::Return(Int32Constant(1234));
  }

  template <typename... Params>
  ReturnType Call(Params... p) {
    ReturnType return_value;
    CSignature::VerifyParams<Params...>(test_graph_signature_);
    CallHelper<int32_t>::Call(reinterpret_cast<void*>(&p)...,
                              reinterpret_cast<void*>(&return_value));
    return return_value;
  }

 private:
  CSignature* test_graph_signature_;
  Node* parameter_nodes_[4];
  uint32_t return_parameter_index_;
};

template <>
class BufferedRawMachineAssemblerTester<void>
    : public RawMachineAssemblerTester<void> {
 public:
  template <typename... ParamMachTypes>
  explicit BufferedRawMachineAssemblerTester(ParamMachTypes... p)
      : RawMachineAssemblerTester<void>(((void)p, MachineType::Pointer())...),
        test_graph_signature_(
            CSignature::New(RawMachineAssemblerTester<void>::main_zone(),
                            MachineType::None(), p...)) {
    static_assert(sizeof...(p) <= arraysize(parameter_nodes_),
                  "increase parameter_nodes_ array");
    std::array<MachineType, sizeof...(p)> p_arr{{p...}};
    for (size_t i = 0; i < p_arr.size(); ++i) {
      parameter_nodes_[i] = Load(p_arr[i], RawMachineAssembler::Parameter(i));
    }
  }

  virtual byte* Generate() { return RawMachineAssemblerTester::Generate(); }

  // The BufferedRawMachineAssemblerTester does not pass parameters directly
  // to the constructed IR graph. Instead it passes a pointer to the parameter
  // to the IR graph, and adds Load nodes to the IR graph to load the
  // parameters from memory. Thereby it is possible to pass 64 bit parameters
  // to the IR graph.
  Node* Parameter(size_t index) {
    CHECK_GT(arraysize(parameter_nodes_), index);
    return parameter_nodes_[index];
  }

  template <typename... Params>
  void Call(Params... p) {
    CSignature::VerifyParams<Params...>(test_graph_signature_);
    CallHelper<void>::Call(reinterpret_cast<void*>(&p)...);
  }

 private:
  CSignature* test_graph_signature_;
  Node* parameter_nodes_[4];
};

static const bool USE_RESULT_BUFFER = true;
static const bool USE_RETURN_REGISTER = false;
static const int32_t CHECK_VALUE = 0x99BEEDCE;


// TODO(titzer): use the C-style calling convention, or any register-based
// calling convention for binop tests.
template <typename CType, bool use_result_buffer>
class BinopTester {
 public:
  explicit BinopTester(RawMachineAssemblerTester<int32_t>* tester,
                       MachineType rep)
      : T(tester),
        param0(T->LoadFromPointer(&p0, rep)),
        param1(T->LoadFromPointer(&p1, rep)),
        rep(rep),
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
      return static_cast<CType>(T->Call());
    }
  }

  void AddReturn(Node* val) {
    if (use_result_buffer) {
      T->Store(rep.representation(), T->PointerConstant(&result),
               T->Int32Constant(0), val, kNoWriteBarrier);
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
  MachineType rep;
  CType p0;
  CType p1;
  CType result;
};


// A helper class for testing code sequences that take two int parameters and
// return an int value.
class Int32BinopTester : public BinopTester<int32_t, USE_RETURN_REGISTER> {
 public:
  explicit Int32BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<int32_t, USE_RETURN_REGISTER>(tester,
                                                  MachineType::Int32()) {}
};


// A helper class for testing code sequences that take two int parameters and
// return an int value.
class Int64BinopTester : public BinopTester<int64_t, USE_RETURN_REGISTER> {
 public:
  explicit Int64BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<int64_t, USE_RETURN_REGISTER>(tester,
                                                  MachineType::Int64()) {}
};


// A helper class for testing code sequences that take two uint parameters and
// return an uint value.
class Uint32BinopTester : public BinopTester<uint32_t, USE_RETURN_REGISTER> {
 public:
  explicit Uint32BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<uint32_t, USE_RETURN_REGISTER>(tester,
                                                   MachineType::Uint32()) {}

  uint32_t call(uint32_t a0, uint32_t a1) {
    p0 = a0;
    p1 = a1;
    return static_cast<uint32_t>(T->Call());
  }
};


// A helper class for testing code sequences that take two float parameters and
// return a float value.
class Float32BinopTester : public BinopTester<float, USE_RESULT_BUFFER> {
 public:
  explicit Float32BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<float, USE_RESULT_BUFFER>(tester, MachineType::Float32()) {}
};


// A helper class for testing code sequences that take two double parameters and
// return a double value.
class Float64BinopTester : public BinopTester<double, USE_RESULT_BUFFER> {
 public:
  explicit Float64BinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<double, USE_RESULT_BUFFER>(tester, MachineType::Float64()) {
  }
};


// A helper class for testing code sequences that take two pointer parameters
// and return a pointer value.
// TODO(titzer): pick word size of pointers based on V8_TARGET.
template <typename Type>
class PointerBinopTester : public BinopTester<Type*, USE_RETURN_REGISTER> {
 public:
  explicit PointerBinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<Type*, USE_RETURN_REGISTER>(tester,
                                                MachineType::Pointer()) {}
};


// A helper class for testing code sequences that take two tagged parameters and
// return a tagged value.
template <typename Type>
class TaggedBinopTester : public BinopTester<Type*, USE_RETURN_REGISTER> {
 public:
  explicit TaggedBinopTester(RawMachineAssemblerTester<int32_t>* tester)
      : BinopTester<Type*, USE_RETURN_REGISTER>(tester,
                                                MachineType::AnyTagged()) {}
};

// A helper class for testing compares. Wraps a machine opcode and provides
// evaluation routines and the operators.
class CompareWrapper {
 public:
  explicit CompareWrapper(IrOpcode::Value op) : opcode(op) {}

  Node* MakeNode(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) {
    return m->AddNode(op(m->machine()), a, b);
  }

  const Operator* op(MachineOperatorBuilder* machine) {
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
    return nullptr;
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
  explicit Int32BinopInputShapeTester(BinopGen<int32_t>* g)
      : gen(g), input_a(0), input_b(0) {}

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
