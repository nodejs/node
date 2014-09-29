// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"
#include "test/cctest/cctest.h"

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/structured-machine-assembler.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/cctest/compiler/value-helper.h"

#if V8_TURBOFAN_TARGET

using namespace v8::internal::compiler;

typedef StructuredMachineAssembler::IfBuilder IfBuilder;
typedef StructuredMachineAssembler::LoopBuilder Loop;

namespace v8 {
namespace internal {
namespace compiler {

class StructuredMachineAssemblerFriend {
 public:
  static bool VariableAlive(StructuredMachineAssembler* m,
                            const Variable& var) {
    CHECK(m->current_environment_ != NULL);
    int offset = var.offset_;
    return offset < static_cast<int>(m->CurrentVars()->size()) &&
           m->CurrentVars()->at(offset) != NULL;
  }
};
}
}
}  // namespace v8::internal::compiler


TEST(RunVariable) {
  StructuredMachineAssemblerTester<int32_t> m;

  int32_t constant = 0x86c2bb16;

  Variable v1 = m.NewVariable(m.Int32Constant(constant));
  Variable v2 = m.NewVariable(v1.Get());
  m.Return(v2.Get());

  CHECK_EQ(constant, m.Call());
}


TEST(RunSimpleIf) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0xc4a3e3a6;
  {
    IfBuilder cond(&m);
    cond.If(m.Parameter(0)).Then();
    m.Return(m.Int32Constant(constant));
  }
  m.Return(m.Word32Not(m.Int32Constant(constant)));

  CHECK_EQ(~constant, m.Call(0));
  CHECK_EQ(constant, m.Call(1));
}


TEST(RunSimpleIfVariable) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0xdb6f20c2;
  Variable var = m.NewVariable(m.Int32Constant(constant));
  {
    IfBuilder cond(&m);
    cond.If(m.Parameter(0)).Then();
    var.Set(m.Word32Not(var.Get()));
  }
  m.Return(var.Get());

  CHECK_EQ(constant, m.Call(0));
  CHECK_EQ(~constant, m.Call(1));
}


TEST(RunSimpleElse) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0xfc5eadf4;
  {
    IfBuilder cond(&m);
    cond.If(m.Parameter(0)).Else();
    m.Return(m.Int32Constant(constant));
  }
  m.Return(m.Word32Not(m.Int32Constant(constant)));

  CHECK_EQ(constant, m.Call(0));
  CHECK_EQ(~constant, m.Call(1));
}


TEST(RunSimpleIfElse) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0xaa9c8cd3;
  {
    IfBuilder cond(&m);
    cond.If(m.Parameter(0)).Then();
    m.Return(m.Int32Constant(constant));
    cond.Else();
    m.Return(m.Word32Not(m.Int32Constant(constant)));
  }

  CHECK_EQ(~constant, m.Call(0));
  CHECK_EQ(constant, m.Call(1));
}


TEST(RunSimpleIfElseVariable) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0x67b6f39c;
  Variable var = m.NewVariable(m.Int32Constant(constant));
  {
    IfBuilder cond(&m);
    cond.If(m.Parameter(0)).Then();
    var.Set(m.Word32Not(m.Word32Not(var.Get())));
    cond.Else();
    var.Set(m.Word32Not(var.Get()));
  }
  m.Return(var.Get());

  CHECK_EQ(~constant, m.Call(0));
  CHECK_EQ(constant, m.Call(1));
}


TEST(RunSimpleIfNoThenElse) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0xd5e550ed;
  {
    IfBuilder cond(&m);
    cond.If(m.Parameter(0));
  }
  m.Return(m.Int32Constant(constant));

  CHECK_EQ(constant, m.Call(0));
  CHECK_EQ(constant, m.Call(1));
}


TEST(RunSimpleConjunctionVariable) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0xf8fb9ec6;
  Variable var = m.NewVariable(m.Int32Constant(constant));
  {
    IfBuilder cond(&m);
    cond.If(m.Int32Constant(1)).And();
    var.Set(m.Word32Not(var.Get()));
    cond.If(m.Parameter(0)).Then();
    var.Set(m.Word32Not(m.Word32Not(var.Get())));
    cond.Else();
    var.Set(m.Word32Not(var.Get()));
  }
  m.Return(var.Get());

  CHECK_EQ(constant, m.Call(0));
  CHECK_EQ(~constant, m.Call(1));
}


TEST(RunSimpleDisjunctionVariable) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0x118f6ffc;
  Variable var = m.NewVariable(m.Int32Constant(constant));
  {
    IfBuilder cond(&m);
    cond.If(m.Int32Constant(0)).Or();
    var.Set(m.Word32Not(var.Get()));
    cond.If(m.Parameter(0)).Then();
    var.Set(m.Word32Not(m.Word32Not(var.Get())));
    cond.Else();
    var.Set(m.Word32Not(var.Get()));
  }
  m.Return(var.Get());

  CHECK_EQ(constant, m.Call(0));
  CHECK_EQ(~constant, m.Call(1));
}


TEST(RunIfElse) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  {
    IfBuilder cond(&m);
    bool first = true;
    FOR_INT32_INPUTS(i) {
      Node* c = m.Int32Constant(*i);
      if (first) {
        cond.If(m.Word32Equal(m.Parameter(0), c)).Then();
        m.Return(c);
        first = false;
      } else {
        cond.Else();
        cond.If(m.Word32Equal(m.Parameter(0), c)).Then();
        m.Return(c);
      }
    }
  }
  m.Return(m.Int32Constant(333));

  FOR_INT32_INPUTS(i) { CHECK_EQ(*i, m.Call(*i)); }
}


enum IfBuilderBranchType { kSkipBranch, kBranchFallsThrough, kBranchReturns };


static IfBuilderBranchType all_branch_types[] = {
    kSkipBranch, kBranchFallsThrough, kBranchReturns};


static void RunIfBuilderDisjunction(size_t max, IfBuilderBranchType then_type,
                                    IfBuilderBranchType else_type) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  std::vector<int32_t> inputs = ValueHelper::int32_vector();
  std::vector<int32_t>::const_iterator i = inputs.begin();
  int32_t hit = 0x8c723c9a;
  int32_t miss = 0x88a6b9f3;
  {
    Node* p0 = m.Parameter(0);
    IfBuilder cond(&m);
    for (size_t j = 0; j < max; j++, ++i) {
      CHECK(i != inputs.end());  // Thank you STL.
      if (j > 0) cond.Or();
      cond.If(m.Word32Equal(p0, m.Int32Constant(*i)));
    }
    switch (then_type) {
      case kSkipBranch:
        break;
      case kBranchFallsThrough:
        cond.Then();
        break;
      case kBranchReturns:
        cond.Then();
        m.Return(m.Int32Constant(hit));
        break;
    }
    switch (else_type) {
      case kSkipBranch:
        break;
      case kBranchFallsThrough:
        cond.Else();
        break;
      case kBranchReturns:
        cond.Else();
        m.Return(m.Int32Constant(miss));
        break;
    }
  }
  if (then_type != kBranchReturns || else_type != kBranchReturns) {
    m.Return(m.Int32Constant(miss));
  }

  if (then_type != kBranchReturns) hit = miss;

  i = inputs.begin();
  for (size_t j = 0; i != inputs.end(); j++, ++i) {
    int32_t result = m.Call(*i);
    CHECK_EQ(j < max ? hit : miss, result);
  }
}


TEST(RunIfBuilderDisjunction) {
  size_t len = ValueHelper::int32_vector().size() - 1;
  size_t max = len > 10 ? 10 : len - 1;
  for (size_t i = 0; i < ARRAY_SIZE(all_branch_types); i++) {
    for (size_t j = 0; j < ARRAY_SIZE(all_branch_types); j++) {
      for (size_t size = 1; size < max; size++) {
        RunIfBuilderDisjunction(size, all_branch_types[i], all_branch_types[j]);
      }
      RunIfBuilderDisjunction(len, all_branch_types[i], all_branch_types[j]);
    }
  }
}


static void RunIfBuilderConjunction(size_t max, IfBuilderBranchType then_type,
                                    IfBuilderBranchType else_type) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  std::vector<int32_t> inputs = ValueHelper::int32_vector();
  std::vector<int32_t>::const_iterator i = inputs.begin();
  int32_t hit = 0xa0ceb9ca;
  int32_t miss = 0x226cafaa;
  {
    IfBuilder cond(&m);
    Node* p0 = m.Parameter(0);
    for (size_t j = 0; j < max; j++, ++i) {
      if (j > 0) cond.And();
      cond.If(m.Word32NotEqual(p0, m.Int32Constant(*i)));
    }
    switch (then_type) {
      case kSkipBranch:
        break;
      case kBranchFallsThrough:
        cond.Then();
        break;
      case kBranchReturns:
        cond.Then();
        m.Return(m.Int32Constant(hit));
        break;
    }
    switch (else_type) {
      case kSkipBranch:
        break;
      case kBranchFallsThrough:
        cond.Else();
        break;
      case kBranchReturns:
        cond.Else();
        m.Return(m.Int32Constant(miss));
        break;
    }
  }
  if (then_type != kBranchReturns || else_type != kBranchReturns) {
    m.Return(m.Int32Constant(miss));
  }

  if (then_type != kBranchReturns) hit = miss;

  i = inputs.begin();
  for (size_t j = 0; i != inputs.end(); j++, ++i) {
    int32_t result = m.Call(*i);
    CHECK_EQ(j >= max ? hit : miss, result);
  }
}


TEST(RunIfBuilderConjunction) {
  size_t len = ValueHelper::int32_vector().size() - 1;
  size_t max = len > 10 ? 10 : len - 1;
  for (size_t i = 0; i < ARRAY_SIZE(all_branch_types); i++) {
    for (size_t j = 0; j < ARRAY_SIZE(all_branch_types); j++) {
      for (size_t size = 1; size < max; size++) {
        RunIfBuilderConjunction(size, all_branch_types[i], all_branch_types[j]);
      }
      RunIfBuilderConjunction(len, all_branch_types[i], all_branch_types[j]);
    }
  }
}


static void RunDisjunctionVariables(int disjunctions, bool explicit_then,
                                    bool explicit_else) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0x65a09535;

  Node* cmp_val = m.Int32Constant(constant);
  Node* one = m.Int32Constant(1);
  Variable var = m.NewVariable(m.Parameter(0));
  {
    IfBuilder cond(&m);
    cond.If(m.Word32Equal(var.Get(), cmp_val));
    for (int i = 0; i < disjunctions; i++) {
      cond.Or();
      var.Set(m.Int32Add(var.Get(), one));
      cond.If(m.Word32Equal(var.Get(), cmp_val));
    }
    if (explicit_then) {
      cond.Then();
    }
    if (explicit_else) {
      cond.Else();
      var.Set(m.Int32Add(var.Get(), one));
    }
  }
  m.Return(var.Get());

  int adds = disjunctions + (explicit_else ? 1 : 0);
  int32_t input = constant - 2 * adds;
  for (int i = 0; i < adds; i++) {
    CHECK_EQ(input + adds, m.Call(input));
    input++;
  }
  for (int i = 0; i < adds + 1; i++) {
    CHECK_EQ(constant, m.Call(input));
    input++;
  }
  for (int i = 0; i < adds; i++) {
    CHECK_EQ(input + adds, m.Call(input));
    input++;
  }
}


TEST(RunDisjunctionVariables) {
  for (int disjunctions = 0; disjunctions < 10; disjunctions++) {
    RunDisjunctionVariables(disjunctions, false, false);
    RunDisjunctionVariables(disjunctions, false, true);
    RunDisjunctionVariables(disjunctions, true, false);
    RunDisjunctionVariables(disjunctions, true, true);
  }
}


static void RunConjunctionVariables(int conjunctions, bool explicit_then,
                                    bool explicit_else) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  int32_t constant = 0x2c7f4b45;
  Node* cmp_val = m.Int32Constant(constant);
  Node* one = m.Int32Constant(1);
  Variable var = m.NewVariable(m.Parameter(0));
  {
    IfBuilder cond(&m);
    cond.If(m.Word32NotEqual(var.Get(), cmp_val));
    for (int i = 0; i < conjunctions; i++) {
      cond.And();
      var.Set(m.Int32Add(var.Get(), one));
      cond.If(m.Word32NotEqual(var.Get(), cmp_val));
    }
    if (explicit_then) {
      cond.Then();
      var.Set(m.Int32Add(var.Get(), one));
    }
    if (explicit_else) {
      cond.Else();
    }
  }
  m.Return(var.Get());

  int adds = conjunctions + (explicit_then ? 1 : 0);
  int32_t input = constant - 2 * adds;
  for (int i = 0; i < adds; i++) {
    CHECK_EQ(input + adds, m.Call(input));
    input++;
  }
  for (int i = 0; i < adds + 1; i++) {
    CHECK_EQ(constant, m.Call(input));
    input++;
  }
  for (int i = 0; i < adds; i++) {
    CHECK_EQ(input + adds, m.Call(input));
    input++;
  }
}


TEST(RunConjunctionVariables) {
  for (int conjunctions = 0; conjunctions < 10; conjunctions++) {
    RunConjunctionVariables(conjunctions, false, false);
    RunConjunctionVariables(conjunctions, false, true);
    RunConjunctionVariables(conjunctions, true, false);
    RunConjunctionVariables(conjunctions, true, true);
  }
}


TEST(RunSimpleNestedIf) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32, kMachineWord32);
  const size_t NUM_VALUES = 7;
  std::vector<int32_t> inputs = ValueHelper::int32_vector();
  CHECK(inputs.size() >= NUM_VALUES);
  Node* values[NUM_VALUES];
  for (size_t j = 0; j < NUM_VALUES; j++) {
    values[j] = m.Int32Constant(inputs[j]);
  }
  {
    IfBuilder if_0(&m);
    if_0.If(m.Word32Equal(m.Parameter(0), values[0])).Then();
    {
      IfBuilder if_1(&m);
      if_1.If(m.Word32Equal(m.Parameter(1), values[1])).Then();
      { m.Return(values[3]); }
      if_1.Else();
      { m.Return(values[4]); }
    }
    if_0.Else();
    {
      IfBuilder if_1(&m);
      if_1.If(m.Word32Equal(m.Parameter(1), values[2])).Then();
      { m.Return(values[5]); }
      if_1.Else();
      { m.Return(values[6]); }
    }
  }

  int32_t result = m.Call(inputs[0], inputs[1]);
  CHECK_EQ(inputs[3], result);

  result = m.Call(inputs[0], inputs[1] + 1);
  CHECK_EQ(inputs[4], result);

  result = m.Call(inputs[0] + 1, inputs[2]);
  CHECK_EQ(inputs[5], result);

  result = m.Call(inputs[0] + 1, inputs[2] + 1);
  CHECK_EQ(inputs[6], result);
}


TEST(RunUnreachableBlockAfterIf) {
  StructuredMachineAssemblerTester<int32_t> m;
  {
    IfBuilder cond(&m);
    cond.If(m.Int32Constant(0)).Then();
    m.Return(m.Int32Constant(1));
    cond.Else();
    m.Return(m.Int32Constant(2));
  }
  // This is unreachable.
  m.Return(m.Int32Constant(3));
  CHECK_EQ(2, m.Call());
}


TEST(RunUnreachableBlockAfterLoop) {
  StructuredMachineAssemblerTester<int32_t> m;
  {
    Loop loop(&m);
    m.Return(m.Int32Constant(1));
  }
  // This is unreachable.
  m.Return(m.Int32Constant(3));
  CHECK_EQ(1, m.Call());
}


TEST(RunSimpleLoop) {
  StructuredMachineAssemblerTester<int32_t> m;
  int32_t constant = 0x120c1f85;
  {
    Loop loop(&m);
    m.Return(m.Int32Constant(constant));
  }
  CHECK_EQ(constant, m.Call());
}


TEST(RunSimpleLoopBreak) {
  StructuredMachineAssemblerTester<int32_t> m;
  int32_t constant = 0x10ddb0a6;
  {
    Loop loop(&m);
    loop.Break();
  }
  m.Return(m.Int32Constant(constant));
  CHECK_EQ(constant, m.Call());
}


TEST(RunCountToTen) {
  StructuredMachineAssemblerTester<int32_t> m;
  Variable i = m.NewVariable(m.Int32Constant(0));
  Node* ten = m.Int32Constant(10);
  Node* one = m.Int32Constant(1);
  {
    Loop loop(&m);
    {
      IfBuilder cond(&m);
      cond.If(m.Word32Equal(i.Get(), ten)).Then();
      loop.Break();
    }
    i.Set(m.Int32Add(i.Get(), one));
  }
  m.Return(i.Get());
  CHECK_EQ(10, m.Call());
}


TEST(RunCountToTenAcc) {
  StructuredMachineAssemblerTester<int32_t> m;
  int32_t constant = 0xf27aed64;
  Variable i = m.NewVariable(m.Int32Constant(0));
  Variable var = m.NewVariable(m.Int32Constant(constant));
  Node* ten = m.Int32Constant(10);
  Node* one = m.Int32Constant(1);
  {
    Loop loop(&m);
    {
      IfBuilder cond(&m);
      cond.If(m.Word32Equal(i.Get(), ten)).Then();
      loop.Break();
    }
    i.Set(m.Int32Add(i.Get(), one));
    var.Set(m.Int32Add(var.Get(), i.Get()));
  }
  m.Return(var.Get());

  CHECK_EQ(constant + 10 + 9 * 5, m.Call());
}


TEST(RunSimpleNestedLoop) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  Node* zero = m.Int32Constant(0);
  Node* one = m.Int32Constant(1);
  Node* two = m.Int32Constant(2);
  Node* three = m.Int32Constant(3);
  {
    Loop l1(&m);
    {
      Loop l2(&m);
      {
        IfBuilder cond(&m);
        cond.If(m.Word32Equal(m.Parameter(0), one)).Then();
        l1.Break();
      }
      {
        Loop l3(&m);
        {
          IfBuilder cond(&m);
          cond.If(m.Word32Equal(m.Parameter(0), two)).Then();
          l2.Break();
          cond.Else();
          cond.If(m.Word32Equal(m.Parameter(0), three)).Then();
          l3.Break();
        }
        m.Return(three);
      }
      m.Return(two);
    }
    m.Return(one);
  }
  m.Return(zero);

  CHECK_EQ(0, m.Call(1));
  CHECK_EQ(1, m.Call(2));
  CHECK_EQ(2, m.Call(3));
  CHECK_EQ(3, m.Call(4));
}


TEST(RunFib) {
  StructuredMachineAssemblerTester<int32_t> m(kMachineWord32);

  // Constants.
  Node* zero = m.Int32Constant(0);
  Node* one = m.Int32Constant(1);
  Node* two = m.Int32Constant(2);
  // Variables.
  // cnt = input
  Variable cnt = m.NewVariable(m.Parameter(0));
  // if (cnt < 2) return i
  {
    IfBuilder lt2(&m);
    lt2.If(m.Int32LessThan(cnt.Get(), two)).Then();
    m.Return(cnt.Get());
  }
  // cnt -= 2
  cnt.Set(m.Int32Sub(cnt.Get(), two));
  // res = 1
  Variable res = m.NewVariable(one);
  {
    // prv_0 = 1
    // prv_1 = 1
    Variable prv_0 = m.NewVariable(one);
    Variable prv_1 = m.NewVariable(one);
    // while (cnt != 0) {
    Loop main(&m);
    {
      IfBuilder nz(&m);
      nz.If(m.Word32Equal(cnt.Get(), zero)).Then();
      main.Break();
    }
    // res = prv_0 + prv_1
    // prv_0 = prv_1
    // prv_1 = res
    res.Set(m.Int32Add(prv_0.Get(), prv_1.Get()));
    prv_0.Set(prv_1.Get());
    prv_1.Set(res.Get());
    // cnt--
    cnt.Set(m.Int32Sub(cnt.Get(), one));
  }
  m.Return(res.Get());

  int32_t values[] = {0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144};
  for (size_t i = 0; i < ARRAY_SIZE(values); i++) {
    CHECK_EQ(values[i], m.Call(static_cast<int32_t>(i)));
  }
}


static int VariableIntroduction() {
  while (true) {
    int ret = 0;
    for (int i = 0; i < 10; i++) {
      for (int j = i; j < 10; j++) {
        for (int k = j; k < 10; k++) {
          ret++;
        }
        ret++;
      }
      ret++;
    }
    return ret;
  }
}


TEST(RunVariableIntroduction) {
  StructuredMachineAssemblerTester<int32_t> m;
  Node* zero = m.Int32Constant(0);
  Node* one = m.Int32Constant(1);
  // Use an IfBuilder to get out of start block.
  {
    IfBuilder i0(&m);
    i0.If(zero).Then();
    m.Return(one);
  }
  Node* ten = m.Int32Constant(10);
  Variable v0 =
      m.NewVariable(zero);  // Introduce variable outside of start block.
  {
    Loop l0(&m);
    Variable ret = m.NewVariable(zero);  // Introduce loop variable.
    {
      Loop l1(&m);
      {
        IfBuilder i1(&m);
        i1.If(m.Word32Equal(v0.Get(), ten)).Then();
        l1.Break();
      }
      Variable v1 = m.NewVariable(v0.Get());  // Introduce loop variable.
      {
        Loop l2(&m);
        {
          IfBuilder i2(&m);
          i2.If(m.Word32Equal(v1.Get(), ten)).Then();
          l2.Break();
        }
        Variable v2 = m.NewVariable(v1.Get());  // Introduce loop variable.
        {
          Loop l3(&m);
          {
            IfBuilder i3(&m);
            i3.If(m.Word32Equal(v2.Get(), ten)).Then();
            l3.Break();
          }
          ret.Set(m.Int32Add(ret.Get(), one));
          v2.Set(m.Int32Add(v2.Get(), one));
        }
        ret.Set(m.Int32Add(ret.Get(), one));
        v1.Set(m.Int32Add(v1.Get(), one));
      }
      ret.Set(m.Int32Add(ret.Get(), one));
      v0.Set(m.Int32Add(v0.Get(), one));
    }
    m.Return(ret.Get());  // Return loop variable.
  }
  CHECK_EQ(VariableIntroduction(), m.Call());
}


TEST(RunIfBuilderVariableLiveness) {
  StructuredMachineAssemblerTester<int32_t> m;
  typedef i::compiler::StructuredMachineAssemblerFriend F;
  Node* zero = m.Int32Constant(0);
  Variable v_outer = m.NewVariable(zero);
  IfBuilder cond(&m);
  cond.If(zero).Then();
  Variable v_then = m.NewVariable(zero);
  CHECK(F::VariableAlive(&m, v_outer));
  CHECK(F::VariableAlive(&m, v_then));
  cond.Else();
  Variable v_else = m.NewVariable(zero);
  CHECK(F::VariableAlive(&m, v_outer));
  CHECK(F::VariableAlive(&m, v_else));
  CHECK(!F::VariableAlive(&m, v_then));
  cond.End();
  CHECK(F::VariableAlive(&m, v_outer));
  CHECK(!F::VariableAlive(&m, v_then));
  CHECK(!F::VariableAlive(&m, v_else));
}


TEST(RunSimpleExpression1) {
  StructuredMachineAssemblerTester<int32_t> m;

  int32_t constant = 0x0c2974ef;
  Node* zero = m.Int32Constant(0);
  Node* one = m.Int32Constant(1);
  {
    // if (((1 && 1) && 1) && 1) return constant; return 0;
    IfBuilder cond(&m);
    cond.OpenParen();
    cond.OpenParen().If(one).And();
    cond.If(one).CloseParen().And();
    cond.If(one).CloseParen().And();
    cond.If(one).Then();
    m.Return(m.Int32Constant(constant));
  }
  m.Return(zero);

  CHECK_EQ(constant, m.Call());
}


TEST(RunSimpleExpression2) {
  StructuredMachineAssemblerTester<int32_t> m;

  int32_t constant = 0x2eddc11b;
  Node* zero = m.Int32Constant(0);
  Node* one = m.Int32Constant(1);
  {
    // if (((0 || 1) && 1) && 1) return constant; return 0;
    IfBuilder cond(&m);
    cond.OpenParen();
    cond.OpenParen().If(zero).Or();
    cond.If(one).CloseParen().And();
    cond.If(one).CloseParen().And();
    cond.If(one).Then();
    m.Return(m.Int32Constant(constant));
  }
  m.Return(zero);

  CHECK_EQ(constant, m.Call());
}


TEST(RunSimpleExpression3) {
  StructuredMachineAssemblerTester<int32_t> m;

  int32_t constant = 0x9ed5e9ef;
  Node* zero = m.Int32Constant(0);
  Node* one = m.Int32Constant(1);
  {
    // if (1 && ((0 || 1) && 1) && 1) return constant; return 0;
    IfBuilder cond(&m);
    cond.If(one).And();
    cond.OpenParen();
    cond.OpenParen().If(zero).Or();
    cond.If(one).CloseParen().And();
    cond.If(one).CloseParen().And();
    cond.If(one).Then();
    m.Return(m.Int32Constant(constant));
  }
  m.Return(zero);

  CHECK_EQ(constant, m.Call());
}


TEST(RunSimpleExpressionVariable1) {
  StructuredMachineAssemblerTester<int32_t> m;

  int32_t constant = 0x4b40a986;
  Node* one = m.Int32Constant(1);
  Variable var = m.NewVariable(m.Int32Constant(constant));
  {
    // if (var.Get() && ((!var || var) && var) && var) {} return var;
    // incrementing var in each environment.
    IfBuilder cond(&m);
    cond.If(var.Get()).And();
    var.Set(m.Int32Add(var.Get(), one));
    cond.OpenParen().OpenParen().If(m.Word32BinaryNot(var.Get())).Or();
    var.Set(m.Int32Add(var.Get(), one));
    cond.If(var.Get()).CloseParen().And();
    var.Set(m.Int32Add(var.Get(), one));
    cond.If(var.Get()).CloseParen().And();
    var.Set(m.Int32Add(var.Get(), one));
    cond.If(var.Get());
  }
  m.Return(var.Get());

  CHECK_EQ(constant + 4, m.Call());
}


class QuicksortHelper : public StructuredMachineAssemblerTester<int32_t> {
 public:
  QuicksortHelper()
      : StructuredMachineAssemblerTester<int32_t>(
            MachineOperatorBuilder::pointer_rep(), kMachineWord32,
            MachineOperatorBuilder::pointer_rep(), kMachineWord32),
        input_(NULL),
        stack_limit_(NULL),
        one_(Int32Constant(1)),
        stack_frame_size_(Int32Constant(kFrameVariables * 4)),
        left_offset_(Int32Constant(0 * 4)),
        right_offset_(Int32Constant(1 * 4)) {
    Build();
  }

  int32_t DoCall(int32_t* input, int32_t input_length) {
    int32_t stack_space[20];
    // Do call.
    int32_t return_val = Call(input, input_length, stack_space,
                              static_cast<int32_t>(ARRAY_SIZE(stack_space)));
    // Ran out of stack space.
    if (return_val != 0) return return_val;
    // Check sorted.
    int32_t last = input[0];
    for (int32_t i = 0; i < input_length; i++) {
      CHECK(last <= input[i]);
      last = input[i];
    }
    return return_val;
  }

 private:
  void Inc32(const Variable& var) { var.Set(Int32Add(var.Get(), one_)); }
  Node* Index(Node* index) { return Word32Shl(index, Int32Constant(2)); }
  Node* ArrayLoad(Node* index) {
    return Load(kMachineWord32, input_, Index(index));
  }
  void Swap(Node* a_index, Node* b_index) {
    Node* a = ArrayLoad(a_index);
    Node* b = ArrayLoad(b_index);
    Store(kMachineWord32, input_, Index(a_index), b);
    Store(kMachineWord32, input_, Index(b_index), a);
  }
  void AddToCallStack(const Variable& fp, Node* left, Node* right) {
    {
      // Stack limit check.
      IfBuilder cond(this);
      cond.If(IntPtrLessThanOrEqual(fp.Get(), stack_limit_)).Then();
      Return(Int32Constant(-1));
    }
    Store(kMachineWord32, fp.Get(), left_offset_, left);
    Store(kMachineWord32, fp.Get(), right_offset_, right);
    fp.Set(IntPtrAdd(fp.Get(), ConvertInt32ToIntPtr(stack_frame_size_)));
  }
  void Build() {
    Variable left = NewVariable(Int32Constant(0));
    Variable right =
        NewVariable(Int32Sub(Parameter(kInputLengthParameter), one_));
    input_ = Parameter(kInputParameter);
    Node* top_of_stack = Parameter(kStackParameter);
    stack_limit_ = IntPtrSub(
        top_of_stack, ConvertInt32ToIntPtr(Parameter(kStackLengthParameter)));
    Variable fp = NewVariable(top_of_stack);
    {
      Loop outermost(this);
      // Edge case - 2 element array.
      {
        IfBuilder cond(this);
        cond.If(Word32Equal(left.Get(), Int32Sub(right.Get(), one_))).And();
        cond.If(Int32LessThanOrEqual(ArrayLoad(right.Get()),
                                     ArrayLoad(left.Get()))).Then();
        Swap(left.Get(), right.Get());
      }
      {
        IfBuilder cond(this);
        // Algorithm complete condition.
        cond.If(WordEqual(top_of_stack, fp.Get())).And();
        cond.If(Int32LessThanOrEqual(Int32Sub(right.Get(), one_), left.Get()))
            .Then();
        outermost.Break();
        // 'Recursion' exit condition. Pop frame and continue.
        cond.Else();
        cond.If(Int32LessThanOrEqual(Int32Sub(right.Get(), one_), left.Get()))
            .Then();
        fp.Set(IntPtrSub(fp.Get(), ConvertInt32ToIntPtr(stack_frame_size_)));
        left.Set(Load(kMachineWord32, fp.Get(), left_offset_));
        right.Set(Load(kMachineWord32, fp.Get(), right_offset_));
        outermost.Continue();
      }
      // Partition.
      Variable store_index = NewVariable(left.Get());
      {
        Node* pivot_index =
            Int32Div(Int32Add(left.Get(), right.Get()), Int32Constant(2));
        Node* pivot = ArrayLoad(pivot_index);
        Swap(pivot_index, right.Get());
        Variable i = NewVariable(left.Get());
        {
          Loop partition(this);
          {
            IfBuilder cond(this);
            // Parition complete.
            cond.If(Word32Equal(i.Get(), right.Get())).Then();
            partition.Break();
            // Need swap.
            cond.Else();
            cond.If(Int32LessThanOrEqual(ArrayLoad(i.Get()), pivot)).Then();
            Swap(i.Get(), store_index.Get());
            Inc32(store_index);
          }
          Inc32(i);
        }  // End partition loop.
        Swap(store_index.Get(), right.Get());
      }
      // 'Recurse' left and right halves of partition.
      // Tail recurse second one.
      AddToCallStack(fp, left.Get(), Int32Sub(store_index.Get(), one_));
      left.Set(Int32Add(store_index.Get(), one_));
    }  // End outermost loop.
    Return(Int32Constant(0));
  }

  static const int kFrameVariables = 2;  // left, right
  // Parameter offsets.
  static const int kInputParameter = 0;
  static const int kInputLengthParameter = 1;
  static const int kStackParameter = 2;
  static const int kStackLengthParameter = 3;
  // Function inputs.
  Node* input_;
  Node* stack_limit_;
  // Constants.
  Node* const one_;
  // Frame constants.
  Node* const stack_frame_size_;
  Node* const left_offset_;
  Node* const right_offset_;
};


TEST(RunSimpleQuicksort) {
  QuicksortHelper m;
  int32_t inputs[] = {9, 7, 1, 8, 11};
  CHECK_EQ(0, m.DoCall(inputs, ARRAY_SIZE(inputs)));
}


TEST(RunRandomQuicksort) {
  QuicksortHelper m;

  v8::base::RandomNumberGenerator rng;
  static const int kMaxLength = 40;
  int32_t inputs[kMaxLength];

  for (int length = 1; length < kMaxLength; length++) {
    for (int i = 0; i < 70; i++) {
      // Randomize inputs.
      for (int j = 0; j < length; j++) {
        inputs[j] = rng.NextInt(10) - 5;
      }
      CHECK_EQ(0, m.DoCall(inputs, length));
    }
  }
}


TEST(MultipleScopes) {
  StructuredMachineAssemblerTester<int32_t> m;
  for (int i = 0; i < 10; i++) {
    IfBuilder b(&m);
    b.If(m.Int32Constant(0)).Then();
    m.NewVariable(m.Int32Constant(0));
  }
  m.Return(m.Int32Constant(0));
  CHECK_EQ(0, m.Call());
}

#endif
