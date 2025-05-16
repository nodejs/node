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

#define RUN_MAYBE_ABORT(phase, ...) \
  if (V8_UNLIKELY(!Run<phase>(__VA_ARGS__))) return {};

void SimplificationAndNormalizationPhase::Run(PipelineData* data,
                                              Zone* temp_zone) {
  CopyingPhase<LoadStoreSimplificationReducer,
               InstructionSelectionNormalizationReducer>::Run(data, temp_zone);
}

bool Pipeline::AllocateRegisters(const RegisterConfiguration* config,
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

  RUN_MAYBE_ABORT(MeetRegisterConstraintsPhase);
  RUN_MAYBE_ABORT(ResolvePhisPhase);
  RUN_MAYBE_ABORT(BuildLiveRangesPhase);
  RUN_MAYBE_ABORT(BuildLiveRangeBundlesPhase);

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

  RUN_MAYBE_ABORT(AllocateGeneralRegistersPhase<LinearScanAllocator>);

  if (data_->sequence()->HasFPVirtualRegisters()) {
    RUN_MAYBE_ABORT(AllocateFPRegistersPhase<LinearScanAllocator>);
  }

  if (data_->sequence()->HasSimd128VirtualRegisters() &&
      (kFPAliasing == AliasingKind::kIndependent)) {
    RUN_MAYBE_ABORT(AllocateSimd128RegistersPhase<LinearScanAllocator>);
  }

  RUN_MAYBE_ABORT(DecideSpillingModePhase);
  RUN_MAYBE_ABORT(AssignSpillSlotsPhase);
  RUN_MAYBE_ABORT(CommitAssignmentPhase);

  // TODO(chromium:725559): remove this check once
  // we understand the cause of the bug. We keep just the
  // check at the end of the allocation.
  if (verifier != nullptr) {
    verifier->VerifyAssignment("Immediately after CommitAssignmentPhase.");
  }

  RUN_MAYBE_ABORT(ConnectRangesPhase);

  RUN_MAYBE_ABORT(ResolveControlFlowPhase);

  RUN_MAYBE_ABORT(PopulateReferenceMapsPhase);

  if (v8_flags.turbo_move_optimization) {
    RUN_MAYBE_ABORT(OptimizeMovesPhase);
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
  return true;
}

[[nodiscard]] bool Pipeline::GenerateCode(
    Linkage* linkage, std::shared_ptr<OsrHelper> osr_helper,
    JumpOptimizationInfo* jump_optimization_info,
    const ProfileDataFromFile* profile, int initial_graph_hash) {
  // Run code generation. If we optimize jumps, we repeat this a second time.
  data()->InitializeCodegenComponent(osr_helper, jump_optimization_info);

  // Perform instruction selection and register allocation.
  if (!PrepareForInstructionSelection(profile)) return false;
  if (!SelectInstructions(linkage)) return false;
  if (!AllocateRegisters(linkage->GetIncomingDescriptor())) return false;
  if (!AssembleCode(linkage)) return false;

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
    if (!AllocateRegisters(linkage->GetIncomingDescriptor())) return false;
    // Generate the final machine code.
    if (!AssembleCode(linkage)) return false;
  }
  return true;
}

void BuiltinPipeline::OptimizeBuiltin() {
  Tracing::Scope tracing_scope(data()->info());

  CHECK(Run<CsaEarlyMachineOptimizationPhase>());
  CHECK(Run<CsaLoadEliminationPhase>());
  CHECK(Run<CsaLateEscapeAnalysisPhase>());
  CHECK(Run<CsaBranchEliminationPhase>());
  CHECK(Run<CsaOptimizePhase>());

  if (v8_flags.turboshaft_enable_debug_features) {
    CHECK(Run<DebugFeatureLoweringPhase>());
  }

  CHECK(Run<CodeEliminationAndSimplificationPhase>());
}

}  // namespace v8::internal::compiler::turboshaft
