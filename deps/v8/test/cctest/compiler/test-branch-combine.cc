// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal;
using namespace v8::internal::compiler;

typedef RawMachineAssembler::Label MLabel;

static IrOpcode::Value int32cmp_opcodes[] = {
    IrOpcode::kWord32Equal, IrOpcode::kInt32LessThan,
    IrOpcode::kInt32LessThanOrEqual, IrOpcode::kUint32LessThan,
    IrOpcode::kUint32LessThanOrEqual};


TEST(BranchCombineWord32EqualZero_1) {
  // Test combining a branch with x == 0
  RawMachineAssemblerTester<int32_t> m(kMachineWord32);
  int32_t eq_constant = -1033;
  int32_t ne_constant = 825118;
  Node* p0 = m.Parameter(0);

  MLabel blocka, blockb;
  m.Branch(m.Word32Equal(p0, m.Int32Constant(0)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(i) {
    int32_t a = *i;
    int32_t expect = a == 0 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineWord32EqualZero_chain) {
  // Test combining a branch with a chain of x == 0 == 0 == 0 ...
  int32_t eq_constant = -1133;
  int32_t ne_constant = 815118;

  for (int k = 0; k < 6; k++) {
    RawMachineAssemblerTester<int32_t> m(kMachineWord32);
    Node* p0 = m.Parameter(0);
    MLabel blocka, blockb;
    Node* cond = p0;
    for (int j = 0; j < k; j++) {
      cond = m.Word32Equal(cond, m.Int32Constant(0));
    }
    m.Branch(cond, &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(eq_constant));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(ne_constant));

    FOR_INT32_INPUTS(i) {
      int32_t a = *i;
      int32_t expect = (k & 1) == 1 ? (a == 0 ? eq_constant : ne_constant)
                                    : (a == 0 ? ne_constant : eq_constant);
      CHECK_EQ(expect, m.Call(a));
    }
  }
}


TEST(BranchCombineInt32LessThanZero_1) {
  // Test combining a branch with x < 0
  RawMachineAssemblerTester<int32_t> m(kMachineWord32);
  int32_t eq_constant = -1433;
  int32_t ne_constant = 845118;
  Node* p0 = m.Parameter(0);

  MLabel blocka, blockb;
  m.Branch(m.Int32LessThan(p0, m.Int32Constant(0)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(i) {
    int32_t a = *i;
    int32_t expect = a < 0 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineUint32LessThan100_1) {
  // Test combining a branch with x < 100
  RawMachineAssemblerTester<int32_t> m(kMachineWord32);
  int32_t eq_constant = 1471;
  int32_t ne_constant = 88845718;
  Node* p0 = m.Parameter(0);

  MLabel blocka, blockb;
  m.Branch(m.Uint32LessThan(p0, m.Int32Constant(100)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_UINT32_INPUTS(i) {
    uint32_t a = *i;
    int32_t expect = a < 100 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineUint32LessThanOrEqual100_1) {
  // Test combining a branch with x <= 100
  RawMachineAssemblerTester<int32_t> m(kMachineWord32);
  int32_t eq_constant = 1479;
  int32_t ne_constant = 77845719;
  Node* p0 = m.Parameter(0);

  MLabel blocka, blockb;
  m.Branch(m.Uint32LessThanOrEqual(p0, m.Int32Constant(100)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_UINT32_INPUTS(i) {
    uint32_t a = *i;
    int32_t expect = a <= 100 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineZeroLessThanInt32_1) {
  // Test combining a branch with 0 < x
  RawMachineAssemblerTester<int32_t> m(kMachineWord32);
  int32_t eq_constant = -2033;
  int32_t ne_constant = 225118;
  Node* p0 = m.Parameter(0);

  MLabel blocka, blockb;
  m.Branch(m.Int32LessThan(m.Int32Constant(0), p0), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(i) {
    int32_t a = *i;
    int32_t expect = 0 < a ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineInt32GreaterThanZero_1) {
  // Test combining a branch with x > 0
  RawMachineAssemblerTester<int32_t> m(kMachineWord32);
  int32_t eq_constant = -1073;
  int32_t ne_constant = 825178;
  Node* p0 = m.Parameter(0);

  MLabel blocka, blockb;
  m.Branch(m.Int32GreaterThan(p0, m.Int32Constant(0)), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(i) {
    int32_t a = *i;
    int32_t expect = a > 0 ? eq_constant : ne_constant;
    CHECK_EQ(expect, m.Call(a));
  }
}


TEST(BranchCombineWord32EqualP) {
  // Test combining a branch with an Word32Equal.
  RawMachineAssemblerTester<int32_t> m(kMachineWord32, kMachineWord32);
  int32_t eq_constant = -1035;
  int32_t ne_constant = 825018;
  Node* p0 = m.Parameter(0);
  Node* p1 = m.Parameter(1);

  MLabel blocka, blockb;
  m.Branch(m.Word32Equal(p0, p1), &blocka, &blockb);
  m.Bind(&blocka);
  m.Return(m.Int32Constant(eq_constant));
  m.Bind(&blockb);
  m.Return(m.Int32Constant(ne_constant));

  FOR_INT32_INPUTS(i) {
    FOR_INT32_INPUTS(j) {
      int32_t a = *i;
      int32_t b = *j;
      int32_t expect = a == b ? eq_constant : ne_constant;
      CHECK_EQ(expect, m.Call(a, b));
    }
  }
}


TEST(BranchCombineWord32EqualI) {
  int32_t eq_constant = -1135;
  int32_t ne_constant = 925718;

  for (int left = 0; left < 2; left++) {
    FOR_INT32_INPUTS(i) {
      RawMachineAssemblerTester<int32_t> m(kMachineWord32);
      int32_t a = *i;

      Node* p0 = m.Int32Constant(a);
      Node* p1 = m.Parameter(0);

      MLabel blocka, blockb;
      if (left == 1) m.Branch(m.Word32Equal(p0, p1), &blocka, &blockb);
      if (left == 0) m.Branch(m.Word32Equal(p1, p0), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(eq_constant));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(ne_constant));

      FOR_INT32_INPUTS(j) {
        int32_t b = *j;
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
    RawMachineAssemblerTester<int32_t> m(kMachineWord32, kMachineWord32);
    Node* p0 = m.Parameter(0);
    Node* p1 = m.Parameter(1);

    MLabel blocka, blockb;
    if (op == 0) m.Branch(m.Int32LessThan(p0, p1), &blocka, &blockb);
    if (op == 1) m.Branch(m.Int32LessThanOrEqual(p0, p1), &blocka, &blockb);
    m.Bind(&blocka);
    m.Return(m.Int32Constant(eq_constant));
    m.Bind(&blockb);
    m.Return(m.Int32Constant(ne_constant));

    FOR_INT32_INPUTS(i) {
      FOR_INT32_INPUTS(j) {
        int32_t a = *i;
        int32_t b = *j;
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
    FOR_INT32_INPUTS(i) {
      RawMachineAssemblerTester<int32_t> m(kMachineWord32);
      int32_t a = *i;
      Node* p0 = m.Int32Constant(a);
      Node* p1 = m.Parameter(0);

      MLabel blocka, blockb;
      if (op == 0) m.Branch(m.Int32LessThan(p0, p1), &blocka, &blockb);
      if (op == 1) m.Branch(m.Int32LessThanOrEqual(p0, p1), &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(eq_constant));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(ne_constant));

      FOR_INT32_INPUTS(j) {
        int32_t b = *j;
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

  virtual void gen(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) {
    Node* cond = w.MakeNode(m, a, b);
    if (invert) cond = m->Word32Equal(cond, m->Int32Constant(0));
    m->Return(cond);
  }
  virtual int32_t expected(int32_t a, int32_t b) {
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

  virtual void gen(RawMachineAssemblerTester<int32_t>* m, Node* a, Node* b) {
    MLabel blocka, blockb;
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
  virtual int32_t expected(int32_t a, int32_t b) {
    if (invert) return !w.Int32Compare(a, b) ? eq_constant : ne_constant;
    return w.Int32Compare(a, b) ? eq_constant : ne_constant;
  }
};


TEST(BranchCombineInt32CmpAllInputShapes_materialized) {
  for (size_t i = 0; i < ARRAY_SIZE(int32cmp_opcodes); i++) {
    CmpMaterializeBoolGen gen(int32cmp_opcodes[i], false);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_inverted_materialized) {
  for (size_t i = 0; i < ARRAY_SIZE(int32cmp_opcodes); i++) {
    CmpMaterializeBoolGen gen(int32cmp_opcodes[i], true);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_branch_true) {
  for (int i = 0; i < static_cast<int>(ARRAY_SIZE(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], false, false, 995 + i, -1011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_branch_false) {
  for (int i = 0; i < static_cast<int>(ARRAY_SIZE(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], false, true, 795 + i, -2011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_inverse_branch_true) {
  for (int i = 0; i < static_cast<int>(ARRAY_SIZE(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], true, false, 695 + i, -3011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineInt32CmpAllInputShapes_inverse_branch_false) {
  for (int i = 0; i < static_cast<int>(ARRAY_SIZE(int32cmp_opcodes)); i++) {
    CmpBranchGen gen(int32cmp_opcodes[i], true, true, 595 + i, -4011 - i);
    Int32BinopInputShapeTester tester(&gen);
    tester.TestAllInputShapes();
  }
}


TEST(BranchCombineFloat64Compares) {
  double inf = V8_INFINITY;
  double nan = v8::base::OS::nan_value();
  double inputs[] = {0.0, 1.0, -1.0, -inf, inf, nan};

  int32_t eq_constant = -1733;
  int32_t ne_constant = 915118;

  double input_a = 0.0;
  double input_b = 0.0;

  CompareWrapper cmps[] = {CompareWrapper(IrOpcode::kFloat64Equal),
                           CompareWrapper(IrOpcode::kFloat64LessThan),
                           CompareWrapper(IrOpcode::kFloat64LessThanOrEqual)};

  for (size_t c = 0; c < ARRAY_SIZE(cmps); c++) {
    CompareWrapper cmp = cmps[c];
    for (int invert = 0; invert < 2; invert++) {
      RawMachineAssemblerTester<int32_t> m;
      Node* a = m.LoadFromPointer(&input_a, kMachineFloat64);
      Node* b = m.LoadFromPointer(&input_b, kMachineFloat64);

      MLabel blocka, blockb;
      Node* cond = cmp.MakeNode(&m, a, b);
      if (invert) cond = m.Word32Equal(cond, m.Int32Constant(0));
      m.Branch(cond, &blocka, &blockb);
      m.Bind(&blocka);
      m.Return(m.Int32Constant(eq_constant));
      m.Bind(&blockb);
      m.Return(m.Int32Constant(ne_constant));

      for (size_t i = 0; i < ARRAY_SIZE(inputs); i++) {
        for (size_t j = 0; j < ARRAY_SIZE(inputs); j += 2) {
          input_a = inputs[i];
          input_b = inputs[i];
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
#endif  // V8_TURBOFAN_TARGET
