// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "test/cctest/cctest.h"

#include "src/base/utils/random-number-generator.h"
#include "src/compiler/graph-inl.h"
#include "src/compiler/machine-operator-reducer.h"
#include "test/cctest/compiler/value-helper.h"

using namespace v8::internal;
using namespace v8::internal::compiler;

template <typename T>
Operator* NewConstantOperator(CommonOperatorBuilder* common, volatile T value);

template <>
Operator* NewConstantOperator<int32_t>(CommonOperatorBuilder* common,
                                       volatile int32_t value) {
  return common->Int32Constant(value);
}

template <>
Operator* NewConstantOperator<double>(CommonOperatorBuilder* common,
                                      volatile double value) {
  return common->Float64Constant(value);
}


class ReducerTester : public HandleAndZoneScope {
 public:
  explicit ReducerTester(int num_parameters = 0)
      : isolate(main_isolate()),
        binop(NULL),
        unop(NULL),
        machine(main_zone()),
        common(main_zone()),
        graph(main_zone()),
        maxuint32(Constant<int32_t>(kMaxUInt32)) {
    Node* s = graph.NewNode(common.Start(num_parameters));
    graph.SetStart(s);
  }

  Isolate* isolate;
  Operator* binop;
  Operator* unop;
  MachineOperatorBuilder machine;
  CommonOperatorBuilder common;
  Graph graph;
  Node* maxuint32;

  template <typename T>
  Node* Constant(volatile T value) {
    return graph.NewNode(NewConstantOperator<T>(&common, value));
  }

  // Check that the reduction of this binop applied to constants {a} and {b}
  // yields the {expect} value.
  template <typename T>
  void CheckFoldBinop(volatile T expect, volatile T a, volatile T b) {
    CheckFoldBinop<T>(expect, Constant<T>(a), Constant<T>(b));
  }

  // Check that the reduction of this binop applied to {a} and {b} yields
  // the {expect} value.
  template <typename T>
  void CheckFoldBinop(volatile T expect, Node* a, Node* b) {
    CHECK_NE(NULL, binop);
    Node* n = graph.NewNode(binop, a, b);
    MachineOperatorReducer reducer(&graph);
    Reduction reduction = reducer.Reduce(n);
    CHECK(reduction.Changed());
    CHECK_NE(n, reduction.replacement());
    CHECK_EQ(expect, ValueOf<T>(reduction.replacement()->op()));
  }

  // Check that the reduction of this binop applied to {a} and {b} yields
  // the {expect} node.
  void CheckBinop(Node* expect, Node* a, Node* b) {
    CHECK_NE(NULL, binop);
    Node* n = graph.NewNode(binop, a, b);
    MachineOperatorReducer reducer(&graph);
    Reduction reduction = reducer.Reduce(n);
    CHECK(reduction.Changed());
    CHECK_EQ(expect, reduction.replacement());
  }

  // Check that the reduction of this binop applied to {left} and {right} yields
  // this binop applied to {left_expect} and {right_expect}.
  void CheckFoldBinop(Node* left_expect, Node* right_expect, Node* left,
                      Node* right) {
    CHECK_NE(NULL, binop);
    Node* n = graph.NewNode(binop, left, right);
    MachineOperatorReducer reducer(&graph);
    Reduction reduction = reducer.Reduce(n);
    CHECK(reduction.Changed());
    CHECK_EQ(binop, reduction.replacement()->op());
    CHECK_EQ(left_expect, reduction.replacement()->InputAt(0));
    CHECK_EQ(right_expect, reduction.replacement()->InputAt(1));
  }

  // Check that the reduction of this binop applied to {left} and {right} yields
  // the {op_expect} applied to {left_expect} and {right_expect}.
  template <typename T>
  void CheckFoldBinop(volatile T left_expect, Operator* op_expect,
                      Node* right_expect, Node* left, Node* right) {
    CHECK_NE(NULL, binop);
    Node* n = graph.NewNode(binop, left, right);
    MachineOperatorReducer reducer(&graph);
    Reduction r = reducer.Reduce(n);
    CHECK(r.Changed());
    CHECK_EQ(op_expect->opcode(), r.replacement()->op()->opcode());
    CHECK_EQ(left_expect, ValueOf<T>(r.replacement()->InputAt(0)->op()));
    CHECK_EQ(right_expect, r.replacement()->InputAt(1));
  }

  // Check that the reduction of this binop applied to {left} and {right} yields
  // the {op_expect} applied to {left_expect} and {right_expect}.
  template <typename T>
  void CheckFoldBinop(Node* left_expect, Operator* op_expect,
                      volatile T right_expect, Node* left, Node* right) {
    CHECK_NE(NULL, binop);
    Node* n = graph.NewNode(binop, left, right);
    MachineOperatorReducer reducer(&graph);
    Reduction r = reducer.Reduce(n);
    CHECK(r.Changed());
    CHECK_EQ(op_expect->opcode(), r.replacement()->op()->opcode());
    CHECK_EQ(left_expect, r.replacement()->InputAt(0));
    CHECK_EQ(right_expect, ValueOf<T>(r.replacement()->InputAt(1)->op()));
  }

  // Check that if the given constant appears on the left, the reducer will
  // swap it to be on the right.
  template <typename T>
  void CheckPutConstantOnRight(volatile T constant) {
    // TODO(titzer): CHECK(binop->HasProperty(Operator::kCommutative));
    Node* p = Parameter();
    Node* k = Constant<T>(constant);
    {
      Node* n = graph.NewNode(binop, k, p);
      MachineOperatorReducer reducer(&graph);
      Reduction reduction = reducer.Reduce(n);
      CHECK(!reduction.Changed() || reduction.replacement() == n);
      CHECK_EQ(p, n->InputAt(0));
      CHECK_EQ(k, n->InputAt(1));
    }
    {
      Node* n = graph.NewNode(binop, p, k);
      MachineOperatorReducer reducer(&graph);
      Reduction reduction = reducer.Reduce(n);
      CHECK(!reduction.Changed());
      CHECK_EQ(p, n->InputAt(0));
      CHECK_EQ(k, n->InputAt(1));
    }
  }

  // Check that if the given constant appears on the left, the reducer will
  // *NOT* swap it to be on the right.
  template <typename T>
  void CheckDontPutConstantOnRight(volatile T constant) {
    CHECK(!binop->HasProperty(Operator::kCommutative));
    Node* p = Parameter();
    Node* k = Constant<T>(constant);
    Node* n = graph.NewNode(binop, k, p);
    MachineOperatorReducer reducer(&graph);
    Reduction reduction = reducer.Reduce(n);
    CHECK(!reduction.Changed());
    CHECK_EQ(k, n->InputAt(0));
    CHECK_EQ(p, n->InputAt(1));
  }

  Node* Parameter(int32_t index = 0) {
    return graph.NewNode(common.Parameter(index), graph.start());
  }
};


TEST(ReduceWord32And) {
  ReducerTester R;
  R.binop = R.machine.Word32And();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x & y, x, y);
    }
  }

  R.CheckPutConstantOnRight(33);
  R.CheckPutConstantOnRight(44000);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);
  Node* minus_1 = R.Constant<int32_t>(-1);

  R.CheckBinop(zero, x, zero);  // x  & 0  => 0
  R.CheckBinop(zero, zero, x);  // 0  & x  => 0
  R.CheckBinop(x, x, minus_1);  // x  & -1 => 0
  R.CheckBinop(x, minus_1, x);  // -1 & x  => 0
  R.CheckBinop(x, x, x);        // x  & x  => x
}


TEST(ReduceWord32Or) {
  ReducerTester R;
  R.binop = R.machine.Word32Or();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x | y, x, y);
    }
  }

  R.CheckPutConstantOnRight(36);
  R.CheckPutConstantOnRight(44001);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);
  Node* minus_1 = R.Constant<int32_t>(-1);

  R.CheckBinop(x, x, zero);           // x  & 0  => x
  R.CheckBinop(x, zero, x);           // 0  & x  => x
  R.CheckBinop(minus_1, x, minus_1);  // x  & -1 => -1
  R.CheckBinop(minus_1, minus_1, x);  // -1 & x  => -1
  R.CheckBinop(x, x, x);              // x  & x  => x
}


TEST(ReduceWord32Xor) {
  ReducerTester R;
  R.binop = R.machine.Word32Xor();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x ^ y, x, y);
    }
  }

  R.CheckPutConstantOnRight(39);
  R.CheckPutConstantOnRight(4403);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);

  R.CheckBinop(x, x, zero);            // x ^ 0  => x
  R.CheckBinop(x, zero, x);            // 0 ^ x  => x
  R.CheckFoldBinop<int32_t>(0, x, x);  // x ^ x  => 0
}


TEST(ReduceWord32Shl) {
  ReducerTester R;
  R.binop = R.machine.Word32Shl();

  // TODO(titzer): out of range shifts
  FOR_INT32_INPUTS(i) {
    for (int y = 0; y < 32; y++) {
      int32_t x = *i;
      R.CheckFoldBinop<int32_t>(x << y, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(44);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);

  R.CheckBinop(x, x, zero);  // x << 0  => x
}


TEST(ReduceWord32Shr) {
  ReducerTester R;
  R.binop = R.machine.Word32Shr();

  // TODO(titzer): test out of range shifts
  FOR_UINT32_INPUTS(i) {
    for (uint32_t y = 0; y < 32; y++) {
      uint32_t x = *i;
      R.CheckFoldBinop<int32_t>(x >> y, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(44);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);

  R.CheckBinop(x, x, zero);  // x >>> 0  => x
}


TEST(ReduceWord32Sar) {
  ReducerTester R;
  R.binop = R.machine.Word32Sar();

  // TODO(titzer): test out of range shifts
  FOR_INT32_INPUTS(i) {
    for (int32_t y = 0; y < 32; y++) {
      int32_t x = *i;
      R.CheckFoldBinop<int32_t>(x >> y, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(44);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);

  R.CheckBinop(x, x, zero);  // x >> 0  => x
}


TEST(ReduceWord32Equal) {
  ReducerTester R;
  R.binop = R.machine.Word32Equal();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x == y ? 1 : 0, x, y);
    }
  }

  R.CheckPutConstantOnRight(48);
  R.CheckPutConstantOnRight(-48);

  Node* x = R.Parameter(0);
  Node* y = R.Parameter(1);
  Node* zero = R.Constant<int32_t>(0);
  Node* sub = R.graph.NewNode(R.machine.Int32Sub(), x, y);

  R.CheckFoldBinop<int32_t>(1, x, x);  // x == x  => 1
  R.CheckFoldBinop(x, y, sub, zero);   // x - y == 0  => x == y
  R.CheckFoldBinop(x, y, zero, sub);   // 0 == x - y  => x == y
}


TEST(ReduceInt32Add) {
  ReducerTester R;
  R.binop = R.machine.Int32Add();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x + y, x, y);  // TODO(titzer): signed overflow
    }
  }

  R.CheckPutConstantOnRight(41);
  R.CheckPutConstantOnRight(4407);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);

  R.CheckBinop(x, x, zero);  // x + 0  => x
  R.CheckBinop(x, zero, x);  // 0 + x  => x
}


TEST(ReduceInt32Sub) {
  ReducerTester R;
  R.binop = R.machine.Int32Sub();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x - y, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(412);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);

  R.CheckBinop(x, x, zero);  // x - 0  => x
}


TEST(ReduceInt32Mul) {
  ReducerTester R;
  R.binop = R.machine.Int32Mul();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x * y, x, y);  // TODO(titzer): signed overflow
    }
  }

  R.CheckPutConstantOnRight(4111);
  R.CheckPutConstantOnRight(-4407);

  Node* x = R.Parameter();
  Node* zero = R.Constant<int32_t>(0);
  Node* one = R.Constant<int32_t>(1);
  Node* minus_one = R.Constant<int32_t>(-1);

  R.CheckBinop(zero, x, zero);  // x * 0  => 0
  R.CheckBinop(zero, zero, x);  // 0 * x  => 0
  R.CheckBinop(x, x, one);      // x * 1  => x
  R.CheckBinop(x, one, x);      // 1 * x  => x
  R.CheckFoldBinop<int32_t>(0, R.machine.Int32Sub(), x, minus_one,
                            x);  // -1 * x  => 0 - x
  R.CheckFoldBinop<int32_t>(0, R.machine.Int32Sub(), x, x,
                            minus_one);  // x * -1  => 0 - x

  for (int32_t n = 1; n < 31; ++n) {
    Node* multiplier = R.Constant<int32_t>(1 << n);
    R.CheckFoldBinop<int32_t>(x, R.machine.Word32Shl(), n, x,
                              multiplier);  // x * 2^n => x << n
    R.CheckFoldBinop<int32_t>(x, R.machine.Word32Shl(), n, multiplier,
                              x);  // 2^n * x => x << n
  }
}


TEST(ReduceInt32Div) {
  ReducerTester R;
  R.binop = R.machine.Int32Div();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      if (y == 0) continue;              // TODO(titzer): test / 0
      int32_t r = y == -1 ? -x : x / y;  // INT_MIN / -1 may explode in C
      R.CheckFoldBinop<int32_t>(r, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(41111);
  R.CheckDontPutConstantOnRight(-44071);

  Node* x = R.Parameter();
  Node* one = R.Constant<int32_t>(1);
  Node* minus_one = R.Constant<int32_t>(-1);

  R.CheckBinop(x, x, one);  // x / 1  => x
  // TODO(titzer):                          // 0 / x  => 0 if x != 0
  // TODO(titzer):                          // x / 2^n => x >> n and round
  R.CheckFoldBinop<int32_t>(0, R.machine.Int32Sub(), x, x,
                            minus_one);  // x / -1  => 0 - x
}


TEST(ReduceInt32UDiv) {
  ReducerTester R;
  R.binop = R.machine.Int32UDiv();

  FOR_UINT32_INPUTS(pl) {
    FOR_UINT32_INPUTS(pr) {
      uint32_t x = *pl, y = *pr;
      if (y == 0) continue;  // TODO(titzer): test / 0
      R.CheckFoldBinop<int32_t>(x / y, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(41311);
  R.CheckDontPutConstantOnRight(-44371);

  Node* x = R.Parameter();
  Node* one = R.Constant<int32_t>(1);

  R.CheckBinop(x, x, one);  // x / 1  => x
  // TODO(titzer):                            // 0 / x  => 0 if x != 0

  for (uint32_t n = 1; n < 32; ++n) {
    Node* divisor = R.Constant<int32_t>(1u << n);
    R.CheckFoldBinop<int32_t>(x, R.machine.Word32Shr(), n, x,
                              divisor);  // x / 2^n => x >> n
  }
}


TEST(ReduceInt32Mod) {
  ReducerTester R;
  R.binop = R.machine.Int32Mod();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      if (y == 0) continue;             // TODO(titzer): test % 0
      int32_t r = y == -1 ? 0 : x % y;  // INT_MIN % -1 may explode in C
      R.CheckFoldBinop<int32_t>(r, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(413);
  R.CheckDontPutConstantOnRight(-4401);

  Node* x = R.Parameter();
  Node* one = R.Constant<int32_t>(1);

  R.CheckFoldBinop<int32_t>(0, x, one);  // x % 1  => 0
  // TODO(titzer):                       // x % 2^n => x & 2^n-1 and round
}


TEST(ReduceInt32UMod) {
  ReducerTester R;
  R.binop = R.machine.Int32UMod();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      uint32_t x = *pl, y = *pr;
      if (y == 0) continue;  // TODO(titzer): test x % 0
      R.CheckFoldBinop<int32_t>(x % y, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(417);
  R.CheckDontPutConstantOnRight(-4371);

  Node* x = R.Parameter();
  Node* one = R.Constant<int32_t>(1);

  R.CheckFoldBinop<int32_t>(0, x, one);  // x % 1  => 0

  for (uint32_t n = 1; n < 32; ++n) {
    Node* divisor = R.Constant<int32_t>(1u << n);
    R.CheckFoldBinop<int32_t>(x, R.machine.Word32And(), (1u << n) - 1, x,
                              divisor);  // x % 2^n => x & 2^n-1
  }
}


TEST(ReduceInt32LessThan) {
  ReducerTester R;
  R.binop = R.machine.Int32LessThan();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x < y ? 1 : 0, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(41399);
  R.CheckDontPutConstantOnRight(-440197);

  Node* x = R.Parameter(0);
  Node* y = R.Parameter(1);
  Node* zero = R.Constant<int32_t>(0);
  Node* sub = R.graph.NewNode(R.machine.Int32Sub(), x, y);

  R.CheckFoldBinop<int32_t>(0, x, x);  // x < x  => 0
  R.CheckFoldBinop(x, y, sub, zero);   // x - y < 0 => x < y
  R.CheckFoldBinop(y, x, zero, sub);   // 0 < x - y => y < x
}


TEST(ReduceInt32LessThanOrEqual) {
  ReducerTester R;
  R.binop = R.machine.Int32LessThanOrEqual();

  FOR_INT32_INPUTS(pl) {
    FOR_INT32_INPUTS(pr) {
      int32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x <= y ? 1 : 0, x, y);
    }
  }

  FOR_INT32_INPUTS(i) { R.CheckDontPutConstantOnRight<int32_t>(*i); }

  Node* x = R.Parameter(0);
  Node* y = R.Parameter(1);
  Node* zero = R.Constant<int32_t>(0);
  Node* sub = R.graph.NewNode(R.machine.Int32Sub(), x, y);

  R.CheckFoldBinop<int32_t>(1, x, x);  // x <= x => 1
  R.CheckFoldBinop(x, y, sub, zero);   // x - y <= 0 => x <= y
  R.CheckFoldBinop(y, x, zero, sub);   // 0 <= x - y => y <= x
}


TEST(ReduceUint32LessThan) {
  ReducerTester R;
  R.binop = R.machine.Uint32LessThan();

  FOR_UINT32_INPUTS(pl) {
    FOR_UINT32_INPUTS(pr) {
      uint32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x < y ? 1 : 0, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(41399);
  R.CheckDontPutConstantOnRight(-440197);

  Node* x = R.Parameter();
  Node* max = R.maxuint32;
  Node* zero = R.Constant<int32_t>(0);

  R.CheckFoldBinop<int32_t>(0, max, x);   // M < x  => 0
  R.CheckFoldBinop<int32_t>(0, x, zero);  // x < 0  => 0
  R.CheckFoldBinop<int32_t>(0, x, x);     // x < x  => 0
}


TEST(ReduceUint32LessThanOrEqual) {
  ReducerTester R;
  R.binop = R.machine.Uint32LessThanOrEqual();

  FOR_UINT32_INPUTS(pl) {
    FOR_UINT32_INPUTS(pr) {
      uint32_t x = *pl, y = *pr;
      R.CheckFoldBinop<int32_t>(x <= y ? 1 : 0, x, y);
    }
  }

  R.CheckDontPutConstantOnRight(41399);
  R.CheckDontPutConstantOnRight(-440197);

  Node* x = R.Parameter();
  Node* max = R.maxuint32;
  Node* zero = R.Constant<int32_t>(0);

  R.CheckFoldBinop<int32_t>(1, x, max);   // x <= M  => 1
  R.CheckFoldBinop<int32_t>(1, zero, x);  // 0 <= x  => 1
  R.CheckFoldBinop<int32_t>(1, x, x);     // x <= x  => 1
}


TEST(ReduceLoadStore) {
  ReducerTester R;

  Node* base = R.Constant<int32_t>(11);
  Node* index = R.Constant<int32_t>(4);
  Node* load = R.graph.NewNode(R.machine.Load(kMachineWord32), base, index);

  {
    MachineOperatorReducer reducer(&R.graph);
    Reduction reduction = reducer.Reduce(load);
    CHECK(!reduction.Changed());  // loads should not be reduced.
  }

  {
    Node* store = R.graph.NewNode(
        R.machine.Store(kMachineWord32, kNoWriteBarrier), base, index, load);
    MachineOperatorReducer reducer(&R.graph);
    Reduction reduction = reducer.Reduce(store);
    CHECK(!reduction.Changed());  // stores should not be reduced.
  }
}


static void CheckNans(ReducerTester* R) {
  Node* x = R->Parameter();
  std::vector<double> nans = ValueHelper::nan_vector();
  for (std::vector<double>::const_iterator pl = nans.begin(); pl != nans.end();
       ++pl) {
    for (std::vector<double>::const_iterator pr = nans.begin();
         pr != nans.end(); ++pr) {
      Node* nan1 = R->Constant<double>(*pl);
      Node* nan2 = R->Constant<double>(*pr);
      R->CheckBinop(nan1, x, nan1);     // x % NaN => NaN
      R->CheckBinop(nan1, nan1, x);     // NaN % x => NaN
      R->CheckBinop(nan1, nan2, nan1);  // NaN % NaN => NaN
    }
  }
}


TEST(ReduceFloat64Add) {
  ReducerTester R;
  R.binop = R.machine.Float64Add();

  FOR_FLOAT64_INPUTS(pl) {
    FOR_FLOAT64_INPUTS(pr) {
      double x = *pl, y = *pr;
      R.CheckFoldBinop<double>(x + y, x, y);
    }
  }

  FOR_FLOAT64_INPUTS(i) { R.CheckPutConstantOnRight(*i); }
  // TODO(titzer): CheckNans(&R);
}


TEST(ReduceFloat64Sub) {
  ReducerTester R;
  R.binop = R.machine.Float64Sub();

  FOR_FLOAT64_INPUTS(pl) {
    FOR_FLOAT64_INPUTS(pr) {
      double x = *pl, y = *pr;
      R.CheckFoldBinop<double>(x - y, x, y);
    }
  }
  // TODO(titzer): CheckNans(&R);
}


TEST(ReduceFloat64Mul) {
  ReducerTester R;
  R.binop = R.machine.Float64Mul();

  FOR_FLOAT64_INPUTS(pl) {
    FOR_FLOAT64_INPUTS(pr) {
      double x = *pl, y = *pr;
      R.CheckFoldBinop<double>(x * y, x, y);
    }
  }

  double inf = V8_INFINITY;
  R.CheckPutConstantOnRight(-inf);
  R.CheckPutConstantOnRight(-0.1);
  R.CheckPutConstantOnRight(0.1);
  R.CheckPutConstantOnRight(inf);

  Node* x = R.Parameter();
  Node* one = R.Constant<double>(1.0);

  R.CheckBinop(x, x, one);  // x * 1.0 => x
  R.CheckBinop(x, one, x);  // 1.0 * x => x

  CheckNans(&R);
}


TEST(ReduceFloat64Div) {
  ReducerTester R;
  R.binop = R.machine.Float64Div();

  FOR_FLOAT64_INPUTS(pl) {
    FOR_FLOAT64_INPUTS(pr) {
      double x = *pl, y = *pr;
      R.CheckFoldBinop<double>(x / y, x, y);
    }
  }

  Node* x = R.Parameter();
  Node* one = R.Constant<double>(1.0);

  R.CheckBinop(x, x, one);  // x / 1.0 => x

  CheckNans(&R);
}


TEST(ReduceFloat64Mod) {
  ReducerTester R;
  R.binop = R.machine.Float64Mod();

  FOR_FLOAT64_INPUTS(pl) {
    FOR_FLOAT64_INPUTS(pr) {
      double x = *pl, y = *pr;
      R.CheckFoldBinop<double>(modulo(x, y), x, y);
    }
  }

  CheckNans(&R);
}


// TODO(titzer): test MachineOperatorReducer for Word64And
// TODO(titzer): test MachineOperatorReducer for Word64Or
// TODO(titzer): test MachineOperatorReducer for Word64Xor
// TODO(titzer): test MachineOperatorReducer for Word64Shl
// TODO(titzer): test MachineOperatorReducer for Word64Shr
// TODO(titzer): test MachineOperatorReducer for Word64Sar
// TODO(titzer): test MachineOperatorReducer for Word64Equal
// TODO(titzer): test MachineOperatorReducer for Word64Not
// TODO(titzer): test MachineOperatorReducer for Int64Add
// TODO(titzer): test MachineOperatorReducer for Int64Sub
// TODO(titzer): test MachineOperatorReducer for Int64Mul
// TODO(titzer): test MachineOperatorReducer for Int64UMul
// TODO(titzer): test MachineOperatorReducer for Int64Div
// TODO(titzer): test MachineOperatorReducer for Int64UDiv
// TODO(titzer): test MachineOperatorReducer for Int64Mod
// TODO(titzer): test MachineOperatorReducer for Int64UMod
// TODO(titzer): test MachineOperatorReducer for Int64Neg
// TODO(titzer): test MachineOperatorReducer for ChangeInt32ToFloat64
// TODO(titzer): test MachineOperatorReducer for ChangeFloat64ToInt32
// TODO(titzer): test MachineOperatorReducer for Float64Compare
