// Copyright 2026 the V8 project authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include <cmath>
#include <functional>
#include <limits>

#include "src/base/bits.h"
#include "src/base/ieee754.h"
#include "src/base/numerics/safe_conversions.h"
#include "src/base/overflowing-math.h"
#include "src/base/utils/random-number-generator.h"
#include "src/builtins/builtins.h"
#include "src/common/ptr-compr-inl.h"
#include "src/objects/objects-inl.h"
#include "src/utils/boxed-float.h"
#include "src/utils/utils.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"
#include "test/common/flag-utils.h"
#include "test/common/value-helper.h"

namespace v8 {
namespace internal {
namespace compiler {

#define WORD_COMPARE(Cond, Type, TYPE, SIGN, v_true, v_false)          \
  {                                                                    \
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Type(),  \
                                                 MachineType::Type()); \
    if (!m.machine()->Word64Select().IsSupported()) {                  \
      return;                                                          \
    }                                                                  \
    Node* cmp = m.Cond(m.Parameter(0), m.Parameter(1));                \
    m.Return(m.Word64Select(cmp, m.Int64Constant(v_true),              \
                            m.Int64Constant(v_false)));                \
    FOR_##TYPE##_INPUTS(i) {                                           \
      FOR_##TYPE##_INPUTS(j) {                                         \
        CHECK_EQ(m.Call(i, j), (i SIGN j) ? v_true : v_false);         \
      }                                                                \
    }                                                                  \
  }

TEST(RunSelectWord32Compare) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    WORD_COMPARE(Word32Equal, Int32, INT32, ==, v_true, v_false)
    WORD_COMPARE(Int32LessThan, Int32, INT32, <, v_true, v_false)
    WORD_COMPARE(Int32LessThanOrEqual, Int32, INT32, <=, v_true, v_false)
    WORD_COMPARE(Uint32LessThan, Uint32, UINT32, <, v_true, v_false)
    WORD_COMPARE(Uint32LessThanOrEqual, Uint32, UINT32, <=, v_true, v_false)
  }
}

TEST(RunSelectWord64Compare) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    WORD_COMPARE(Word64Equal, Int64, INT64, ==, v_true, v_false)
    WORD_COMPARE(Int64LessThan, Int64, INT64, <, v_true, v_false)
    WORD_COMPARE(Int64LessThanOrEqual, Int64, INT64, <=, v_true, v_false)
    WORD_COMPARE(Uint64LessThan, Uint64, UINT64, <, v_true, v_false)
    WORD_COMPARE(Uint64LessThanOrEqual, Uint64, UINT64, <=, v_true, v_false)
  }
}

TEST(RunSelectFloat32Compare) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    WORD_COMPARE(Float32Equal, Float32, FLOAT32, ==, v_true, v_false)
    WORD_COMPARE(Float32LessThan, Float32, FLOAT32, <, v_true, v_false)
    WORD_COMPARE(Float32LessThanOrEqual, Float32, FLOAT32, <=, v_true, v_false)
  }
}

TEST(RunSelectFloat64Compare) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    WORD_COMPARE(Float64Equal, Float64, FLOAT64, ==, v_true, v_false)
    WORD_COMPARE(Float64LessThan, Float64, FLOAT64, <, v_true, v_false)
    WORD_COMPARE(Float64LessThanOrEqual, Float64, FLOAT64, <=, v_true, v_false)
  }
}

#undef WORD_COMPARE

TEST(RunSelectStackPointerGreaterThan) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Uint64());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    v8::RegisterState state;
#if defined(USE_SIMULATOR)
    SimulatorHelper simulator_helper;
    if (!simulator_helper.Init(CcTest::isolate())) return;
    simulator_helper.FillRegisters(&state);
    uint64_t sp = reinterpret_cast<uint64_t>(state.sp);
#else
    uint64_t sp = reinterpret_cast<uint64_t>(&state);
#endif
    Node* cmp = m.StackPointerGreaterThan(m.Parameter(0));
    m.Return(
        m.Word64Select(cmp, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    FOR_UINT64_INPUTS(i) { CHECK_EQ(m.Call(i), sp > i ? v_true : v_false); }
  }
}

TEST(RunSelectWord32AddOvf) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Int32(),
                                                 MachineType::Int32());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    Node* cal = m.Int32AddWithOverflow(m.Parameter(0), m.Parameter(1));
    Node* ovf = m.Projection(1, cal);
    m.Return(
        m.Word64Select(ovf, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    int result;
    FOR_INT32_INPUTS(i) {
      FOR_INT32_INPUTS(j) {
        CHECK_EQ(m.Call(i, j), base::bits::SignedAddOverflow32(i, j, &result)
                                   ? v_true
                                   : v_false);
      }
    }
  }
}

TEST(RunSelectWord32SubOvf) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Int32(),
                                                 MachineType::Int32());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    Node* cal = m.Int32SubWithOverflow(m.Parameter(0), m.Parameter(1));
    Node* ovf = m.Projection(1, cal);
    m.Return(
        m.Word64Select(ovf, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    int result;
    FOR_INT32_INPUTS(i) {
      FOR_INT32_INPUTS(j) {
        CHECK_EQ(m.Call(i, j), base::bits::SignedSubOverflow32(i, j, &result)
                                   ? v_true
                                   : v_false);
      }
    }
  }
}

TEST(RunSelectWord32MulOvf) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Int32(),
                                                 MachineType::Int32());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    Node* cal = m.Int32MulWithOverflow(m.Parameter(0), m.Parameter(1));
    Node* ovf = m.Projection(1, cal);
    m.Return(
        m.Word64Select(ovf, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    int result;
    FOR_INT32_INPUTS(i) {
      FOR_INT32_INPUTS(j) {
        CHECK_EQ(m.Call(i, j), base::bits::SignedMulOverflow32(i, j, &result)
                                   ? v_true
                                   : v_false);
      }
    }
  }
}

TEST(RunSelectWord64AddOvf) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Int64(),
                                                 MachineType::Int64());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    Node* cal = m.Int64AddWithOverflow(m.Parameter(0), m.Parameter(1));
    Node* ovf = m.Projection(1, cal);
    m.Return(
        m.Word64Select(ovf, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    int64_t result;
    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) {
        CHECK_EQ(m.Call(i, j), base::bits::SignedAddOverflow64(i, j, &result)
                                   ? v_true
                                   : v_false);
      }
    }
  }
}

TEST(RunSelectWord64SubOvf) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Int64(),
                                                 MachineType::Int64());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    Node* cal = m.Int64SubWithOverflow(m.Parameter(0), m.Parameter(1));
    Node* ovf = m.Projection(1, cal);
    m.Return(
        m.Word64Select(ovf, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    int64_t result;
    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) {
        CHECK_EQ(m.Call(i, j), base::bits::SignedSubOverflow64(i, j, &result)
                                   ? v_true
                                   : v_false);
      }
    }
  }
}

TEST(RunSelectWord64MulOvf) {
  for (int64_t k = -50; k < 50; k++) {
    int64_t v_true = k % 5, v_false = k % 11;
    BufferedRawMachineAssemblerTester<int64_t> m(MachineType::Int64(),
                                                 MachineType::Int64());
    if (!m.machine()->Word64Select().IsSupported()) {
      return;
    }
    Node* cal = m.Int64MulWithOverflow(m.Parameter(0), m.Parameter(1));
    Node* ovf = m.Projection(1, cal);
    m.Return(
        m.Word64Select(ovf, m.Int64Constant(v_true), m.Int64Constant(v_false)));
    int64_t result;
    FOR_INT64_INPUTS(i) {
      FOR_INT64_INPUTS(j) {
        CHECK_EQ(m.Call(i, j), base::bits::SignedMulOverflow64(i, j, &result)
                                   ? v_true
                                   : v_false);
      }
    }
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
