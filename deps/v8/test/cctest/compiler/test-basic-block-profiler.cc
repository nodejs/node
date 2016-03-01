// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/basic-block-profiler.h"
#include "test/cctest/cctest.h"
#include "test/cctest/compiler/codegen-tester.h"

namespace v8 {
namespace internal {
namespace compiler {

class BasicBlockProfilerTest : public RawMachineAssemblerTester<int32_t> {
 public:
  BasicBlockProfilerTest()
      : RawMachineAssemblerTester<int32_t>(MachineType::Int32()) {
    FLAG_turbo_profiling = true;
  }

  void ResetCounts() { isolate()->basic_block_profiler()->ResetCounts(); }

  void Expect(size_t size, uint32_t* expected) {
    CHECK(isolate()->basic_block_profiler());
    const BasicBlockProfiler::DataList* l =
        isolate()->basic_block_profiler()->data_list();
    CHECK_NE(0, static_cast<int>(l->size()));
    const BasicBlockProfiler::Data* data = l->back();
    CHECK_EQ(static_cast<int>(size), static_cast<int>(data->n_blocks()));
    const uint32_t* counts = data->counts();
    for (size_t i = 0; i < size; ++i) {
      CHECK_EQ(static_cast<int>(expected[i]), static_cast<int>(counts[i]));
    }
  }
};


TEST(ProfileDiamond) {
  BasicBlockProfilerTest m;

  RawMachineLabel blocka, blockb, end;
  m.Branch(m.Parameter(0), &blocka, &blockb);
  m.Bind(&blocka);
  m.Goto(&end);
  m.Bind(&blockb);
  m.Goto(&end);
  m.Bind(&end);
  m.Return(m.Int32Constant(0));

  m.GenerateCode();
  {
    uint32_t expected[] = {0, 0, 0, 0};
    m.Expect(arraysize(expected), expected);
  }

  m.Call(0);
  {
    uint32_t expected[] = {1, 1, 0, 1};
    m.Expect(arraysize(expected), expected);
  }

  m.ResetCounts();

  m.Call(1);
  {
    uint32_t expected[] = {1, 0, 1, 1};
    m.Expect(arraysize(expected), expected);
  }

  m.Call(0);
  {
    uint32_t expected[] = {2, 1, 1, 2};
    m.Expect(arraysize(expected), expected);
  }
}


TEST(ProfileLoop) {
  BasicBlockProfilerTest m;

  RawMachineLabel header, body, end;
  Node* one = m.Int32Constant(1);
  m.Goto(&header);

  m.Bind(&header);
  Node* count = m.Phi(MachineRepresentation::kWord32, m.Parameter(0), one);
  m.Branch(count, &body, &end);

  m.Bind(&body);
  count->ReplaceInput(1, m.Int32Sub(count, one));
  m.Goto(&header);

  m.Bind(&end);
  m.Return(one);

  m.GenerateCode();
  {
    uint32_t expected[] = {0, 0, 0, 0};
    m.Expect(arraysize(expected), expected);
  }

  uint32_t runs[] = {0, 1, 500, 10000};
  for (size_t i = 0; i < arraysize(runs); i++) {
    m.ResetCounts();
    CHECK_EQ(1, m.Call(static_cast<int>(runs[i])));
    uint32_t expected[] = {1, runs[i] + 1, runs[i], 1};
    m.Expect(arraysize(expected), expected);
  }
}

}  // namespace compiler
}  // namespace internal
}  // namespace v8
