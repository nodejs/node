// Copyright 2024 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/compiler/turboshaft/pipelines.h"

#include "src/compiler/pipeline-data-inl.h"
#include "src/compiler/turboshaft/csa-optimize-phase.h"
#include "src/compiler/turboshaft/debug-feature-lowering-phase.h"
#include "src/compiler/turboshaft/instruction-selection-normalization-reducer.h"
#include "src/compiler/turboshaft/load-store-simplification-reducer.h"
#include "src/compiler/turboshaft/stack-check-lowering-reducer.h"

namespace v8::internal::compiler::turboshaft {

void SimplificationAndNormalizationPhase::Run(PipelineData* data,
                                              Zone* temp_zone) {
  CopyingPhase<LoadStoreSimplificationReducer,
               InstructionSelectionNormalizationReducer>::Run(data, temp_zone);
}

void Pipeline::AllocateRegisters(const RegisterConfiguration* config,
                                 CallDescriptor* call_descriptor,
                                 bool run_verifier) {
  // Don't track usage for this zone in compiler stats.
  std::unique_ptr<Zone> verifier_zone;
  RegisterAllocatorVerifier* verifier = nullptr;
  if (run_verifier) {
    AccountingAllocator* allocator = data()->allocator();
    DCHECK_NOT_NULL(allocator);
    verifier_zone.reset(
        new Zone(allocator, kRegisterAllocatorVerifierZoneName));
    verifier = verifier_zone->New<RegisterAllocatorVerifier>(
        verifier_zone.get(), config, data()->sequence(), data()->frame());
  }

#ifdef DEBUG
  data_->sequence()->ValidateEdgeSplitForm();
  data_->sequence()->ValidateDeferredBlockEntryPaths();
  data_->sequence()->ValidateDeferredBlockExitPaths();
#endif

  data_->InitializeRegisterComponent(config, call_descriptor);

  Run<MeetRegisterConstraintsPhase>();
  Run<ResolvePhisPhase>();
  Run<BuildLiveRangesPhase>();
  Run<BuildLiveRangeBundlesPhase>();

  TraceSequence("before register allocation");
  if (verifier != nullptr) {
    CHECK(!data_->register_allocation_data()->ExistsUseWithoutDefinition());
    CHECK(data_->register_allocation_data()
              ->RangesDefinedInDeferredStayInDeferred());
  }

  if (data_->info()->trace_turbo_json() && !MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(data_->isolate());
    tcf << AsC1VRegisterAllocationData("PreAllocation",
                                       data_->register_allocation_data());
  }

  Run<AllocateGeneralRegistersPhase<LinearScanAllocator>>();

  if (data_->sequence()->HasFPVirtualRegisters()) {
    Run<AllocateFPRegistersPhase<LinearScanAllocator>>();
  }

  if (data_->sequence()->HasSimd128VirtualRegisters() &&
      (kFPAliasing == AliasingKind::kIndependent)) {
    Run<AllocateSimd128RegistersPhase<LinearScanAllocator>>();
  }

  Run<DecideSpillingModePhase>();
  Run<AssignSpillSlotsPhase>();
  Run<CommitAssignmentPhase>();

  // TODO(chromium:725559): remove this check once
  // we understand the cause of the bug. We keep just the
  // check at the end of the allocation.
  if (verifier != nullptr) {
    verifier->VerifyAssignment("Immediately after CommitAssignmentPhase.");
  }

  Run<ConnectRangesPhase>();

  Run<ResolveControlFlowPhase>();

  Run<PopulateReferenceMapsPhase>();

  if (v8_flags.turbo_move_optimization) {
    Run<OptimizeMovesPhase>();
  }

  TraceSequence("after register allocation");

  if (verifier != nullptr) {
    verifier->VerifyAssignment("End of regalloc pipeline.");
    verifier->VerifyGapMoves();
  }

  if (data_->info()->trace_turbo_json() && !MayHaveUnverifiableGraph()) {
    TurboCfgFile tcf(data_->isolate());
    tcf << AsC1VRegisterAllocationData("CodeGen",
                                       data_->register_allocation_data());
  }

  data()->ClearRegisterComponent();
}

[[nodiscard]] bool Pipeline::GenerateCode(
    Linkage* linkage, std::shared_ptr<OsrHelper> osr_helper,
    JumpOptimizationInfo* jump_optimization_info,
    const ProfileDataFromFile* profile, int initial_graph_hash) {
  // Run code generation. If we optimize jumps, we repeat this a second time.
  data()->InitializeCodegenComponent(osr_helper, jump_optimization_info);

  // Perform instruction selection and register allocation.
  PrepareForInstructionSelection(profile);
  CHECK(SelectInstructions(linkage));
  CHECK(AllocateRegisters(linkage->GetIncomingDescriptor()));
  AssembleCode(linkage);

  if (v8_flags.turbo_profiling) {
    info()->profiler_data()->SetHash(initial_graph_hash);
  }

  if (jump_optimization_info && jump_optimization_info->is_optimizable()) {
    // Reset data for a second run of instruction selection.
    data()->ClearCodegenComponent();
    jump_optimization_info->set_optimizing();

    // Perform instruction selection and register allocation.
    data()->InitializeCodegenComponent(osr_helper, jump_optimization_info);
    if (!SelectInstructions(linkage)) {
      return false;
    }
    AllocateRegisters(linkage->GetIncomingDescriptor());
    // Generate the final machine code.
    AssembleCode(linkage);
  }
  return true;
}

void BuiltinPipeline::OptimizeBuiltin() {
  Tracing::Scope tracing_scope(data()->info());

  Run<CsaEarlyMachineOptimizationPhase>();
  Run<CsaLoadEliminationPhase>();
  Run<CsaLateEscapeAnalysisPhase>();
  Run<CsaBranchEliminationPhase>();
  Run<CsaOptimizePhase>();

  if (v8_flags.turboshaft_enable_debug_features) {
    Run<DebugFeatureLoweringPhase>();
  }

  Run<CodeEliminationAndSimplificationPhase>();
}

}  // namespace v8::internal::compiler::turboshaft
