// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/d8_flags.h"

#include <algorithm>

#include "experimental/redline/core/tunable.h"

namespace redline {

namespace {

// --single-threaded stops the background compiler and GC tasks, so it can hide
// bugs in concurrent code. Thus the default probability is 0.
REDLINE_TUNABLE(kFlagsSingleThreaded, "flags.single_threaded", 0, 0, 1,
                "probability of --single-threaded: synchronous tier-up "
                "compilation without background threads");
REDLINE_TUNABLE(kFlagsNoLazyFeedbackAllocation,
                "flags.no_lazy_feedback_allocation", 0.8, 0, 1,
                "probability of --no-lazy-feedback-allocation: a function "
                "gets its feedback vector at its first call");
REDLINE_TUNABLE(kFlagsNoBaselineBatchCompilation,
                "flags.no_baseline_batch_compilation", 0.8, 0, 1,
                "probability of --no-baseline-batch-compilation");
REDLINE_TUNABLE(kFlagsMaglevFuture, "flags.maglev_future", 0.25, 0, 1,
                "probability of --maglev-future");
REDLINE_TUNABLE(kFlagsMaglevObjectTracking, "flags.maglev_object_tracking", 0.2,
                0, 1, "probability of --maglev-object-tracking");
REDLINE_TUNABLE(kFlagsMaglevNonEagerInlining, "flags.maglev_non_eager_inlining",
                0.25, 0, 1, "probability of --maglev-non-eager-inlining");
REDLINE_TUNABLE(kFlagsMaglevEagerInlinedSizeZero,
                "flags.maglev_eager_inlined_size_zero", 0.35, 0, 1,
                "probability of --max-maglev-eager-inlined-bytecode-size=0, "
                "when --maglev-non-eager-inlining is on");
REDLINE_TUNABLE(kFlagsMaglevSmallInlinedSizeZero,
                "flags.maglev_small_inlined_size_zero", 0.25, 0, 1,
                "probability of --max-maglev-inlined-bytecode-size-small=0, "
                "when --maglev-non-eager-inlining is on");
REDLINE_TUNABLE(kFlagsMaglevDisableBuiltinReducers,
                "flags.maglev_disable_builtin_reducers", 0.15, 0, 1,
                "probability of --maglev-disable-builtin-reducers");
REDLINE_TUNABLE(kFlagsNoMaglevUntaggedPhis, "flags.no_maglev_untagged_phis",
                0.1, 0, 1, "probability of --no-maglev-untagged-phis");
REDLINE_TUNABLE(kFlagsNoMaglevLoopPeeling, "flags.no_maglev_loop_peeling", 0.1,
                0, 1, "probability of --no-maglev-loop-peeling");
REDLINE_TUNABLE(kFlagsMaglevAssert, "flags.maglev_assert", 0.2, 0, 1,
                "probability of --maglev-assert");
REDLINE_TUNABLE(kFlagsMaglevAssertTypes, "flags.maglev_assert_types", 0.1, 0, 1,
                "probability of --maglev-assert-types");
REDLINE_TUNABLE(kFlagsOptimizeToMaglev, "flags.optimize_to_maglev", 0.2, 0, 1,
                "probability of --optimize-on-next-call-optimizes-to-maglev");
REDLINE_TUNABLE(kFlagsStressIc, "flags.stress_ic", 0.1, 0, 1,
                "probability of --stress-ic");
REDLINE_TUNABLE(kFlagsVerifyHeap, "flags.verify_heap", 0.1, 0, 1,
                "probability of --verify-heap");
REDLINE_TUNABLE(kFlagsPrivateFieldBytecodes, "flags.private_field_bytecodes",
                0.25, 0, 1, "probability of --private-field-bytecodes");
REDLINE_TUNABLE(kFlagsTurbolevSmallInlinedSizeZero,
                "flags.turbolev_small_inlined_size_zero", 0.3, 0, 1,
                "Turbolev profile: probability of "
                "--max-inlined-bytecode-size-small=0, so that small callees "
                "stay for the later inliner");
REDLINE_TUNABLE(kFlagsVerifyTurboshaft, "flags.verify_turboshaft", 0.2, 0, 1,
                "Turbolev profile: probability of --verify-turboshaft");
REDLINE_TUNABLE(kFlagsTurboshaftVerifyReductions,
                "flags.turboshaft_verify_reductions", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--turboshaft-verify-reductions");
REDLINE_TUNABLE(kFlagsMaglevRangeVerification,
                "flags.maglev_range_verification", 0.2, 0, 1,
                "Turbolev profile: probability of "
                "--maglev-range-verification");
REDLINE_TUNABLE(kFlagsAlwaysOsrFromMaglev, "flags.always_osr_from_maglev", 0.1,
                0, 1,
                "Turbolev profile: probability of --always-osr-from-maglev");
REDLINE_TUNABLE(kFlagsNoAdditiveSafeIntFeedback,
                "flags.no_additive_safe_int_feedback", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-additive-safe-int-feedback");
REDLINE_TUNABLE(kFlagsTurbolevDisableBuiltinReducers,
                "flags.turbolev_disable_builtin_reducers", 0.15, 0, 1,
                "Turbolev profile: probability of "
                "--maglev-disable-builtin-reducers");
REDLINE_TUNABLE(kFlagsNoTurbolevUntaggedPhis, "flags.no_turbolev_untagged_phis",
                0.1, 0, 1,
                "Turbolev profile: probability of --no-turbolev-untagged-phis");
REDLINE_TUNABLE(kFlagsNoTurbolevTruncatedInt32Phis,
                "flags.no_turbolev_truncated_int32_phis", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-turbolev-truncated-int32-phis");
REDLINE_TUNABLE(kFlagsNoTurbolevNonEagerInlining,
                "flags.no_turbolev_non_eager_inlining", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-turbolev-non-eager-inlining");
REDLINE_TUNABLE(kFlagsNoTurbolevLoopPeeling, "flags.no_turbolev_loop_peeling",
                0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-turbolev-non-eager-loop-peeling");
REDLINE_TUNABLE(kFlagsNoTurbolevOsrLoopPeeling,
                "flags.no_turbolev_osr_loop_peeling", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-turbolev-eager-loop-peeling-osr");
REDLINE_TUNABLE(kFlagsNoTurboshaftLoadElimination,
                "flags.no_turboshaft_load_elimination", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-turboshaft-load-elimination");
REDLINE_TUNABLE(kFlagsNoTurboshaftLoopUnrolling,
                "flags.no_turboshaft_loop_unrolling", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--no-turboshaft-loop-unrolling");
REDLINE_TUNABLE(kFlagsTargetGuidedChance, "flags.target_guided_chance", 0.75, 0,
                1,
                "change-based fuzzing: probability that a new target process "
                "picks flags that reached target edges");
REDLINE_TUNABLE(kFlagsTargetMutateChance, "flags.target_mutate_chance", 0.4, 0,
                1,
                "probability that target-guided flags are combined with a "
                "fresh random flag draw");
REDLINE_TUNABLE(kFlagsTargetKeepChance, "flags.target_keep_chance", 0.85, 0, 1,
                "probability that CombineD8Flags keeps each flag of the base "
                "set");
REDLINE_TUNABLE(kFlagsTargetTournament, "flags.target_tournament", 4, 1, 32,
                "tournament size when picking target-hitting flags");
REDLINE_TUNABLE(kFlagsTargetProbeExecs, "flags.target_probe_execs", 64, 0,
                10000,
                "change-based fuzzing: executions without a target hit "
                "before a child respawns");

}  // namespace

bool ParseD8FlagProfile(const std::string& name, D8FlagProfile* profile) {
  if (name == "maglev") {
    *profile = D8FlagProfile::kMaglev;
    return true;
  }
  if (name == "turbolev") {
    *profile = D8FlagProfile::kTurbolev;
    return true;
  }
  return false;
}

const char* D8FlagProfileName(D8FlagProfile profile) {
  return profile == D8FlagProfile::kTurbolev ? "turbolev" : "maglev";
}

std::vector<std::string> RandomD8Flags(Random& rng, D8FlagProfile profile) {
  std::vector<std::string> flags;
  auto maybe = [&](const Tunable& probability, const char* flag) {
    if (rng.Chance(probability())) {
      flags.emplace_back(flag);
      return true;
    }
    return false;
  };
  maybe(kFlagsSingleThreaded, "--single-threaded");
  maybe(kFlagsNoLazyFeedbackAllocation, "--no-lazy-feedback-allocation");
  maybe(kFlagsNoBaselineBatchCompilation, "--no-baseline-batch-compilation");
  maybe(kFlagsPrivateFieldBytecodes, "--private-field-bytecodes");
  if (profile == D8FlagProfile::kTurbolev) {
    // Turbolev builds its graph with the Maglev graph builder, so the object
    // tracking and the builtin reducers of Maglev apply to it too.
    maybe(kFlagsMaglevObjectTracking, "--maglev-object-tracking");
    maybe(kFlagsTurbolevDisableBuiltinReducers,
          "--maglev-disable-builtin-reducers");
    maybe(kFlagsTurbolevSmallInlinedSizeZero,
          "--max-inlined-bytecode-size-small=0");
    maybe(kFlagsVerifyTurboshaft, "--verify-turboshaft");
    maybe(kFlagsTurboshaftVerifyReductions, "--turboshaft-verify-reductions");
    maybe(kFlagsMaglevRangeVerification, "--maglev-range-verification");
    maybe(kFlagsAlwaysOsrFromMaglev, "--always-osr-from-maglev");
    maybe(kFlagsNoAdditiveSafeIntFeedback, "--no-additive-safe-int-feedback");
    maybe(kFlagsNoTurbolevUntaggedPhis, "--no-turbolev-untagged-phis");
    maybe(kFlagsNoTurbolevTruncatedInt32Phis,
          "--no-turbolev-truncated-int32-phis");
    maybe(kFlagsNoTurbolevNonEagerInlining, "--no-turbolev-non-eager-inlining");
    maybe(kFlagsNoTurbolevLoopPeeling, "--no-turbolev-non-eager-loop-peeling");
    maybe(kFlagsNoTurbolevOsrLoopPeeling,
          "--no-turbolev-eager-loop-peeling-osr");
    maybe(kFlagsNoTurboshaftLoadElimination,
          "--no-turboshaft-load-elimination");
    maybe(kFlagsNoTurboshaftLoopUnrolling, "--no-turboshaft-loop-unrolling");
    maybe(kFlagsStressIc, "--stress-ic");
    maybe(kFlagsVerifyHeap, "--verify-heap");
    return flags;
  }
  maybe(kFlagsMaglevFuture, "--maglev-future");
  maybe(kFlagsMaglevObjectTracking, "--maglev-object-tracking");
  if (maybe(kFlagsMaglevNonEagerInlining, "--maglev-non-eager-inlining")) {
    // Without eager inlining, the calls stay for the later inliner.
    maybe(kFlagsMaglevEagerInlinedSizeZero,
          "--max-maglev-eager-inlined-bytecode-size=0");
    maybe(kFlagsMaglevSmallInlinedSizeZero,
          "--max-maglev-inlined-bytecode-size-small=0");
  }
  maybe(kFlagsMaglevDisableBuiltinReducers,
        "--maglev-disable-builtin-reducers");
  maybe(kFlagsNoMaglevUntaggedPhis, "--no-maglev-untagged-phis");
  maybe(kFlagsNoMaglevLoopPeeling, "--no-maglev-loop-peeling");
  maybe(kFlagsMaglevAssert, "--maglev-assert");
  maybe(kFlagsMaglevAssertTypes, "--maglev-assert-types");
  maybe(kFlagsOptimizeToMaglev, "--optimize-on-next-call-optimizes-to-maglev");
  maybe(kFlagsStressIc, "--stress-ic");
  maybe(kFlagsVerifyHeap, "--verify-heap");
  return flags;
}

std::vector<std::string> CombineD8Flags(const std::vector<std::string>& base,
                                        const std::vector<std::string>& extra,
                                        Random& rng) {
  std::vector<std::string> combined;
  for (const std::string& flag : base) {
    if (rng.Chance(kFlagsTargetKeepChance())) {
      combined.push_back(flag);
    }
  }
  for (const std::string& flag : extra) {
    if (std::find(combined.begin(), combined.end(), flag) == combined.end()) {
      combined.push_back(flag);
    }
  }
  const bool has_non_eager =
      std::find(combined.begin(), combined.end(),
                "--maglev-non-eager-inlining") != combined.end();
  if (!has_non_eager) {
    combined.erase(
        std::remove_if(
            combined.begin(), combined.end(),
            [](const std::string& flag) {
              return flag == "--max-maglev-eager-inlined-bytecode-size=0" ||
                     flag == "--max-maglev-inlined-bytecode-size-small=0";
            }),
        combined.end());
  }
  return combined;
}

double TargetGuidedChance() { return kFlagsTargetGuidedChance(); }
double TargetMutateChance() { return kFlagsTargetMutateChance(); }
int TargetTournament() { return kFlagsTargetTournament.AsInt(); }
int TargetProbeExecs() { return kFlagsTargetProbeExecs.AsInt(); }

}  // namespace redline
