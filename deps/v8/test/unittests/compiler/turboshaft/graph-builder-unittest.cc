// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/graph-builder.h"

#include <array>
#include <optional>

#include "src/codegen/assembler.h"
#include "src/codegen/machine-type.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/compiler/linkage.h"
#include "src/compiler/machine-operator.h"
#include "src/compiler/raw-machine-assembler.h"
#include "src/compiler/schedule.h"
#include "src/compiler/turbofan-graph.h"
#include "src/compiler/turboshaft/graph.h"
#include "src/compiler/turboshaft/operations.h"
#include "src/compiler/turboshaft/phase.h"
#include "src/compiler/zone-stats.h"
#include "test/unittests/test-utils.h"

namespace v8::internal::compiler::turboshaft {

class GraphBuilderTest : public TestWithIsolateAndZone {
 protected:
  template <typename BuildFn, typename CheckFn>
  void BuildAndCheck(MachineSignature* sig, BuildFn&& build, CheckFn&& check) {
    CallDescriptor* desc = Linkage::GetSimplifiedCDescriptor(zone(), sig);
    TFGraph tf_graph(zone());
    RawMachineAssembler rasm(i_isolate(), &tf_graph, desc);
    build(rasm);
    Schedule* schedule = rasm.ExportForTest();

    ZoneStats zone_stats(zone()->allocator());
    const testing::TestInfo* test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    OptimizedCompilationInfo info(test_info->name(), zone(),
                                  CodeKind::FOR_TESTING_JS);
    PipelineData data(&zone_stats, TurboshaftPipelineKind::kJS, i_isolate(),
                      &info, AssemblerOptions::Default(i_isolate()));
    data.InitializeGraphComponent(nullptr, Graph::Origin::kCreatedFromTurbofan);
    Linkage linkage(desc);
    Zone phase_zone(zone()->allocator(), "graph-builder-test-phase-zone");
    std::optional<BailoutReason> bailout =
        turboshaft::BuildGraph(&data, schedule, &phase_zone, &linkage);
    ASSERT_FALSE(bailout.has_value());
    check(data.graph());
  }

  void TestAtomicLoad(bool is_64bit, AtomicMemoryOrder order,
                      MemoryAccessKind access_kind) {
    MachineType load_type =
        is_64bit ? MachineType::Uint64() : MachineType::Uint32();
    MachineSignature::Builder sig_builder(zone(), 1, 2);
    sig_builder.AddReturn(load_type);
    sig_builder.AddParam(MachineType::Pointer());
    sig_builder.AddParam(MachineType::Pointer());

    BuildAndCheck(
        sig_builder.Get(),
        [&](RawMachineAssembler& rasm) {
          Node* base = rasm.Parameter(0);
          Node* index = rasm.Parameter(1);
          AtomicLoadParameters params(load_type, order, access_kind);
          Node* load = is_64bit ? rasm.AtomicLoad64(params, base, index)
                                : rasm.AtomicLoad(params, base, index);
          rasm.Return(load);
        },
        [&](const Graph& graph) {
          const LoadOp* found_load = nullptr;
          for (const Operation& op : graph.AllOperations()) {
            if (const LoadOp* load = op.TryCast<LoadOp>()) {
              EXPECT_EQ(found_load, nullptr);
              found_load = load;
            }
          }
          ASSERT_NE(found_load, nullptr);
          EXPECT_EQ(found_load->kind.with_trap_handler,
                    access_kind == MemoryAccessKind::kTrapping);
          EXPECT_EQ(found_load->kind.memory_order(), order);
          EXPECT_TRUE(found_load->kind.is_atomic());
        });
  }

  void TestAtomicStore(bool is_64bit, AtomicMemoryOrder order,
                       MemoryAccessKind access_kind) {
    MachineType value_type =
        is_64bit ? MachineType::Uint64() : MachineType::Uint32();
    MachineSignature::Builder sig_builder(zone(), 1, 3);
    sig_builder.AddReturn(MachineType::Int32());
    sig_builder.AddParam(MachineType::Pointer());
    sig_builder.AddParam(MachineType::Pointer());
    sig_builder.AddParam(value_type);

    BuildAndCheck(
        sig_builder.Get(),
        [&](RawMachineAssembler& rasm) {
          Node* base = rasm.Parameter(0);
          Node* index = rasm.Parameter(1);
          Node* value = rasm.Parameter(2);
          AtomicStoreParameters params(value_type.representation(),
                                       WriteBarrierKind::kNoWriteBarrier, order,
                                       access_kind);
          if (is_64bit) {
            rasm.AtomicStore64(params, base, index, value, nullptr);
          } else {
            rasm.AtomicStore(params, base, index, value);
          }
          rasm.Return(rasm.Int32Constant(0));
        },
        [&](const Graph& graph) {
          const StoreOp* found_store = nullptr;
          for (const Operation& op : graph.AllOperations()) {
            if (const StoreOp* store = op.TryCast<StoreOp>()) {
              EXPECT_EQ(found_store, nullptr);
              found_store = store;
            }
          }
          ASSERT_NE(found_store, nullptr);
          EXPECT_EQ(found_store->kind.with_trap_handler,
                    access_kind == MemoryAccessKind::kTrapping);
          EXPECT_EQ(found_store->kind.memory_order(), order);
          EXPECT_TRUE(found_store->kind.is_atomic());
        });
  }
};

TEST_F(GraphBuilderTest, AtomicLoad) {
  constexpr std::array orders = {AtomicMemoryOrder::kSeqCst,
                                 AtomicMemoryOrder::kAcqRel};
  constexpr std::array access_kinds = {MemoryAccessKind::kNormal,
                                       MemoryAccessKind::kTrapping};
  for (AtomicMemoryOrder order : orders) {
    for (MemoryAccessKind access_kind : access_kinds) {
      TestAtomicLoad(false, order, access_kind);
      if constexpr (Is64()) {
        TestAtomicLoad(true, order, access_kind);
      }
    }
  }
}

TEST_F(GraphBuilderTest, AtomicStore) {
  constexpr std::array orders = {AtomicMemoryOrder::kSeqCst,
                                 AtomicMemoryOrder::kAcqRel};
  constexpr std::array access_kinds = {MemoryAccessKind::kNormal,
                                       MemoryAccessKind::kTrapping};
  for (AtomicMemoryOrder order : orders) {
    for (MemoryAccessKind access_kind : access_kinds) {
      TestAtomicStore(false, order, access_kind);
      if constexpr (Is64()) {
        TestAtomicStore(true, order, access_kind);
      }
    }
  }
}

}  // namespace v8::internal::compiler::turboshaft
