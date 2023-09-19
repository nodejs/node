// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/base/overflowing-math.h"
#include "src/objects/objects-inl.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/common/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

static IrOpcode::Value int32cmp_opcodes[] = {
    IrOpcode::kWord32Equal, IrOpcode::kInt32LessThan,
    IrOpcode::kInt32LessThanOrEqual, IrOpcode::kUint32LessThan,
    IrOpcode::kUint32LessThanOrEqual};


TEST(BranchCombineWord32EqualZero_1) {
  // Test combining a branch with x == 0
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
  int32_t eq_constant = -1033;
  int32_t ne_constant = 825118;
  Node* p0 = m.Parameter(0);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Word32Equal(p0, m.Int32Constant(0)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(a) {
    int32_t expect = a == 0 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineWord32EqualZero_chain) {
  // Test combining a branch with a chain of x == 0 == 0 == 0 ...
  int32_t eq_constant = -1133;
  int32_t ne_constant = 815118;

  for (int k = 0; k < 6; k++) {
    RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
    Node* p0 = m.Parameter(0);
    RawMachineLabel blocka, blockb;
    Node* cond = p0;
    for (int j = 0; j < k; j++) {
      cond = m.Word32Equal(cond, m.Int32Constant(0));
    }
    m.Branch(cond, &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(eq_constant));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(ne_constant));

    FOR_INT32_INPUTS(a) {
      int32_t expect = (k & 1) == 1 ? (a == 0 ? eq_constant : ne_constant)
                                    : (a == 0 ? ne_constant : eq_constant);
      CHECK_EQ(expect, m.Call(a));
    }
  }
}


TEST(BranchCombineInt32LessThanZero_1) {
  // Test combining a branch with x < 0
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
  int32_t eq_constant = -1433;
  int32_t ne_constant = 845118;
  Node* p0 = m.Parameter(0);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Int32LessThan(p0, m.Int32Constant(0)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(a) {
    int32_t expect = a < 0 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineUint32LessThan100_1) {
  // Test combining a branch with x < 100
  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32());
  int32_t eq_constant = 1471;
  int32_t ne_constant = 88845718;
  Node* p0 = m.Parameter(0);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Uint32LessThan(p0, m.Int32Constant(100)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_UINT32_INPUTS(a) {
    int32_t expect = a < 100 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineUint32LessThanOrEqual100_1) {
  // Test combining a branch with x <= 100
  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32());
  int32_t eq_constant = 1479;
  int32_t ne_constant = 77845719;
  Node* p0 = m.Parameter(0);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Uint32LessThanOrEqual(p0, m.Int32Constant(100)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_UINT32_INPUTS(a) {
    int32_t expect = a <= 100 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineZeroLessThanInt32_1) {
  // Test combining a branch with 0 < x
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
  int32_t eq_constant = -2033;
  int32_t ne_constant = 225118;
  Node* p0 = m.Parameter(0);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Int32LessThan(m.Int32Constant(0), p0), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(a) {
    int32_t expect = 0 < a ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineInt32GreaterThanZero_1) {
  // Test combining a branch with x > 0
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
  int32_t eq_constant = -1073;
  int32_t ne_constant = 825178;
  Node* p0 = m.Parameter(0);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Int32GreaterThan(p0, m.Int32Constant(0)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(a) {
    int32_t expect = a > 0 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineWord32EqualP) {
  // Test combining a branch with an Word32Equal.
  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  int32_t eq_constant = -1035;
  int32_t ne_constant = 825018;
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);

  RawMachineLabel blocka, blockb;
  m.Branch(m.Word32Equal(p0, p1), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect = a == b ? eq_constant : ne_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}


TEST(BranchCombineWord32EqualI) {
  int32_t eq_constant = -1135;
  int32_t ne_constant = 925718;

  for (int left = 0; left < 2; left++) {
    FOR_INT32_INPUTS(a) {
      RawMachineAssemblerTester<int32_t> m(MachineType::Int32());

      Node* p0 = m.Int32Constant(a);
      Node* p1 = m.Parameter(0);

      RawMachineLabel blocka, blockb;
      if (left == 1) m.Branch(m.Word32Equal(p0, p1), &blocka, &blockb);
      if (left == 0) m.Branch(m.Word32Equal(p1, p0), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(eq_constant));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(ne_constant));

      FOR_INT32_INPUTS(b) {
        int32_t expect = a == b ? eq_constant : ne_constant;
        CHECK_EQ(expect, m.Call(b));
      }
    }
  }
}


TEST(BranchCombineInt32CmpP) {
  int32_t eq_constant = -1235;
  int32_t ne_constant = 725018;

  for (int op = 0; op < 2; op++) {
    RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                         MachineType::Int32());
    Node* p0 = m.Parameter(0);
    Node* p1 = m.Parameter(1);

    RawMachineLabel blocka, blockb;
    if (op == 0) m.Branch(m.Int32LessThan(p0, p1), &blocka, &blockb);
    if (op == 1) m.Branch(m.Int32LessThanOrEqual(p0, p1), &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(eq_constant));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(ne_constant));

    FOR_INT32_INPUTS(a) {
      FOR_INT32_INPUTS(b) {
        int32_t expect = 0;
        if (op == 0) expect = a < b ? eq_constant : ne_constant;
        if (op == 1) expect = a <= b ? eq_constant : ne_constant;
        CHECK_EQ(expect, m.Call(a, b));
      }
    }
  }
}


TEST(BranchCombineInt32CmpI) {
  int32_t eq_constant = -1175;
  int32_t ne_constant = 927711;

  for (int op = 0; op < 2; op++) {
    FOR_INT32_INPUTS(a) {
      RawMachineAssemblerTester<int32_t> m(MachineType::Int32());
      Node* p0 = m.Int32Constant(a);
      Node* p1 = m.Parameter(0);

      RawMachineLabel blocka, blockb;
      if (op == 0) m.Branch(m.Int32LessThan(p0, p1), &blocka, &blockb);
      if (op == 1) m.Branch(m.Int32LessThanOrEqual(p0, p1), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(eq_constant));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(ne_constant));

      FOR_INT32_INPUTS(b) {
        int32_t expect = 0;
        if (op == 0) expect = a < b ? eq_constant : ne_constant;
        if (op == 1) expect = a <= b ? eq_constant : ne_constant;
        CHECK_EQ(expect, m.Call(b));
      }
    }
  }
}


// Now come the sophisticated tests for many input shape combinations.

// Materializes a boolean (1 or 0) from a comparison.
class CmpMaterializeBoolGen : public BinopGen<int32_t> {
 public:
  CompareWrapper w;
  bool invert;

  CmpMaterializeBoolGen(IrOpcode::Value opcode, bool i)
      : w(opcode), invert(i) {}

  void gen(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) override {
    Node* cond = w.MakeNode(m, a, b);
    if (invert) cond = m->Word32Equal(cond, m->Int32Constant(0));
    m->Return(cond);
  }
  int32_t expected(int32_t a, int32_t b) override {
    if (invert) return !w.Int32Compare(a, b) ? 1 : 0;
    return w.Int32Compare(a, b) ? 1 : 0;
  }
};


// Generates a branch and return one of two values from a comparison.
class CmpBranchGen : public BinopGen<int32_t> {
 public:
  CompareWrapper w;
  bool invert;
  bool true_first;
  int32_t eq_constant;
  int32_t ne_constant;

  CmpBranchGen(IrOpcode::Value opcode, bool i, bool t, int32_t eq, int32_t ne)
      : w(opcode), invert(i), true_first(t), eq_constant(eq), ne_constant(ne) {}

  void gen(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) override {
    RawMachineLabel blocka, blockb;
    Node* cond = w.MakeNode(m, a, b);
    if (invert) cond = m->Word32Equal(cond, m->Int32Constant(0));
    m->Branch(cond, &blocka, &blockb);
    if (true_first) {
      m->Bind(&blocka);
      m->Return(m->Int32Constant(eq_constant));
      m->Bind(&blockb);
      m->Return(m->Int32Constant(ne_constant));
    } else {
      m->Bind(&blockb);
      m->Return(m->Int32Constant(ne_constant));
      m->Bind(&blocka);
      m->Return(m->Int32Constant(eq_constant));
    }
  }
  int32_t expected(int32_t a, int32_t b) override {
    if (invert) return !w.Int32Compare(a, b) ? eq_constant : ne_constant;
    return w.Int32Compare(a, b) ? eq_constant : ne_constant;
  }
};


TEST(BranchCombineInt32CmpAllInputShapes_materialized) {
  for (size_t i = 0; i < arraysize(int32cmp_opcodes); i++) {
    CmpMaterializeBoolGen gen(int32cmp_opcodes[i], false);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_inverted_materialized) {
  for (size_t i = 0; i < arraysize(int32cmp_opcodes); i++) {
    CmpMaterializeBoolGen gen(int32cmp_opcodes[i], true);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_branch_true) {
  for (int i = 0; i < static_cast<int>(arraysize(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], false, false, 995 + i, -1011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_branch_false) {
  for (int i = 0; i < static_cast<int>(arraysize(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], false, true, 795 + i, -2011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_inverse_branch_true) {
  for (int i = 0; i < static_cast<int>(arraysize(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], true, false, 695 + i, -3011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_inverse_branch_false) {
  for (int i = 0; i < static_cast<int>(arraysize(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], true, true, 595 + i, -4011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineFloat64Compares) {
  double inf = V8_INFINITY;
  double nan = std::numeric_limits<double>::quiet_NaN();
  double inputs[] = {0.0, 1.0, -1.0, -inf, inf, nan};

  int32_t eq_constant = -1733;
  int32_t ne_constant = 915118;

  double input_a = 0.0;
  double input_b = 0.0;

  CompareWrapper cmps[] = {CompareWrapper(IrOpcode::kFloat64Equal),
                           CompareWrapper(IrOpcode::kFloat64LessThan),
                           CompareWrapper(IrOpcode::kFloat64LessThanOrEqual)};

  for (size_t c = 0; c < arraysize(cmps); c++) {
    CompareWrapper cmp = cmps[c];
    for (int invert = 0; invert < 2; invert++) {
      RawMachineAssemblerTester<int32_t> m;
      Node* a = m.LoadFromPointer(&input_a, MachineType::Float64());
      Node* b = m.LoadFromPointer(&input_b, MachineType::Float64());

      RawMachineLabel blocka, blockb;
      Node* cond = cmp.MakeNode(&m, a, b);
      if (invert) cond = m.Word32Equal(cond, m.Int32Constant(0));
      m.Branch(cond, &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(eq_constant));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(ne_constant));

      for (size_t i = 0; i < arraysize(inputs); ++i) {
        for (size_t j = 0; j < arraysize(inputs); ++j) {
          input_a = inputs[i];
          input_b = inputs[j];
          int32_t expected =
              invert ? (cmp.Float64Compare(input_a, input_b) ? ne_constant
                                                             : eq_constant)
                     : (cmp.Float64Compare(input_a, input_b) ? eq_constant
                                                             : ne_constant);
          CHECK_EQ(expected, m.Call());
        }
      }
    }
  }
}

TEST(BranchCombineEffectLevel) {
  // Test that the load doesn't get folded into the branch, as there's a store
  // between them. See http://crbug.com/611976.
  int32_t input = 0;

  RawMachineAssemblerTester<int32_t> m;
  Node* a = m.LoadFromPointer(&input, MachineType::Int32());
  Node* compare = m.Word32And(a, m.Int32Constant(1));
  Node* equal = m.Word32Equal(compare, m.Int32Constant(0));
  m.StoreToPointer(&input, MachineRepresentation::kWord32, m.Int32Constant(1));

  RawMachineLabel blocka, blockb;
  m.Branch(equal, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(42));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(0));

  CHECK_EQ(42, m.Call());
}

TEST(BranchCombineInt32AddLessThanZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Int32LessThan(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect =
          (base::AddWithWraparound(a, b) < 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineInt32AddGreaterThanOrEqualZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Int32GreaterThanOrEqual(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect =
          (base::AddWithWraparound(a, b) >= 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineInt32ZeroGreaterThanAdd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Int32GreaterThan(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect =
          (0 > base::AddWithWraparound(a, b)) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineInt32ZeroLessThanOrEqualAdd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Int32LessThanOrEqual(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect =
          (0 <= base::AddWithWraparound(a, b)) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32AddLessThanOrEqualZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Uint32LessThanOrEqual(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = (a + b <= 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32AddGreaterThanZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Uint32GreaterThan(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = (a + b > 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32ZeroGreaterThanOrEqualAdd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Uint32GreaterThanOrEqual(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = (0 >= a + b) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32ZeroLessThanAdd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Int32Add(p0, p1);
  Node* compare = m.Uint32LessThan(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = (0 < a + b) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineWord32AndLessThanZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Int32LessThan(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect = ((a & b) < 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineWord32AndGreaterThanOrEqualZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Int32GreaterThanOrEqual(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect = ((a & b) >= 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineInt32ZeroGreaterThanAnd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Int32GreaterThan(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect = (0 > (a & b)) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineInt32ZeroLessThanOrEqualAnd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Int32(),
                                       MachineType::Int32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Int32LessThanOrEqual(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_INT32_INPUTS(a) {
    FOR_INT32_INPUTS(b) {
      int32_t expect = (0 <= (a & b)) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32AndLessThanOrEqualZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Uint32LessThanOrEqual(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = ((a & b) <= 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32AndGreaterThanZero) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Uint32GreaterThan(add, m.Int32Constant(0));

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = ((a & b) > 0) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32ZeroGreaterThanOrEqualAnd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Uint32GreaterThanOrEqual(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = (0 >= (a & b)) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

TEST(BranchCombineUint32ZeroLessThanAnd) {
  int32_t t_constant = -1033;
  int32_t f_constant = 825118;

  RawMachineAssemblerTester<int32_t> m(MachineType::Uint32(),
                                       MachineType::Uint32());
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);
  Node* add = m.Word32And(p0, p1);
  Node* compare = m.Uint32LessThan(m.Int32Constant(0), add);

  RawMachineLabel blocka, blockb;
  m.Branch(compare, &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(t_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(f_constant));

  FOR_UINT32_INPUTS(a) {
    FOR_UINT32_INPUTS(b) {
      int32_t expect = (0 < (a & b)) ? t_constant : f_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
