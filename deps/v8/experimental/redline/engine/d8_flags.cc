// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/d8_flags.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <numeric>
#include <string>
#include <string_view>

#include "experimental/redline/base/json_parser.h"
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
                0.5, 0, 1, "probability of --private-field-bytecodes");
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
                "flags.turbolev_disable_builtin_reducers", 0.1, 0, 1,
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

// The flags below come from the V8 profile of Fuzzilli (v8ProcessArgs in
// Sources/Fuzzilli/Profiles/V8CommonProfile.swift) and from the fuzz
// configurations of V8 (redline-generators skill, section 5). The default is
// the probability of Fuzzilli. Fuzzilli draws some flags only after a flag
// that a preset always sets, for example --no-turbofan. For these flags, the
// default is the probability after that flag, and Redline draws them only
// after that flag too. If Fuzzilli does not draw a
// flag, the default is the probability of the V8 configurations. The value
// ranges are the ranges of the same source.

// Flags of both profiles: tier-up, deoptimization and code generation.
REDLINE_TUNABLE(kFlagsFuture, "flags.future", 0.8, 0, 1,
                "probability of --future: the staged V8 features");
REDLINE_TUNABLE(kFlagsJitFuzzing, "flags.jit_fuzzing", 0.8, 0, 1,
                "probability of --jit-fuzzing: lower tier-up thresholds");
REDLINE_TUNABLE(kFlagsNoSparkplug, "flags.no_sparkplug", 0.1, 0, 1,
                "probability of --no-sparkplug");
// Fuzzilli picks `--shared-string-table` or `--shared-strings` with 0.25. V8
// deprecated `--shared-string-table`, so `--shared-strings` keeps 0.125.
REDLINE_TUNABLE(kFlagsSharedStrings, "flags.shared_strings", 0.125, 0, 1,
                "probability of --shared-strings: shared strings and shared "
                "heap");
REDLINE_TUNABLE(kFlagsProtoAssignSeqOptCountOne,
                "flags.proto_assign_seq_opt_count_one", 0.25, 0, 1,
                "probability of --proto-assign-seq-opt-count=1");
// Fuzzilli picks `--verify-bytecode-light` or `--verify-bytecode-full` with
// 0.67. Thus each flag has the probability 0.335, and a draw has at most one.
REDLINE_TUNABLE(kFlagsVerifyBytecodeFull, "flags.verify_bytecode_full", 0.335,
                0, 1, "probability of --verify-bytecode-full");
REDLINE_TUNABLE(kFlagsVerifyBytecodeLight, "flags.verify_bytecode_light", 0.335,
                0, 1, "probability of --verify-bytecode-light");
// Only the TypeAssertionsPhase of Turbofan reads --assert-types. The flag
// also turns off concurrent recompilation, also for Maglev and Turbolev.
// Thus the default probability is 0.
REDLINE_TUNABLE(kFlagsAssertTypes, "flags.assert_types", 0, 0, 1,
                "probability of --assert-types");
REDLINE_TUNABLE(kFlagsStressLazySourcePositions,
                "flags.stress_lazy_source_positions", 0.5, 0, 1,
                "probability of --stress-lazy-source-positions");
REDLINE_TUNABLE(kFlagsStressLazy, "flags.stress_lazy", 0.9, 0, 1,
                "probability of --stress-lazy");
REDLINE_TUNABLE(kFlagsNoLazy, "flags.no_lazy", 0.025, 0, 1,
                "probability of --no-lazy");
REDLINE_TUNABLE(kFlagsNoLazyEval, "flags.no_lazy_eval", 0.025, 0, 1,
                "probability of --no-lazy-eval");
REDLINE_TUNABLE(kFlagsAlwaysOsr, "flags.always_osr", 0.025, 0, 1,
                "probability of --always-osr");
REDLINE_TUNABLE(kFlagsAlwaysSparkplug, "flags.always_sparkplug", 0.025, 0, 1,
                "probability of --always-sparkplug");
REDLINE_TUNABLE(kFlagsForceSlowPath, "flags.force_slow_path", 0.025, 0, 1,
                "probability of --force-slow-path");
REDLINE_TUNABLE(kFlagsStressGcDuringCompilation,
                "flags.stress_gc_during_compilation", 0.025, 0, 1,
                "probability of --stress-gc-during-compilation");
REDLINE_TUNABLE(kFlagsFeedbackAllocationCount,
                "flags.invocation_count_for_feedback_allocation", 0.1, 0, 1,
                "probability of --invocation-count-for-feedback-allocation=N");
REDLINE_TUNABLE(kFlagsFeedbackAllocationCountMin,
                "flags.invocation_count_for_feedback_allocation_min", 0, 0,
                1000,
                "smallest N of --invocation-count-for-feedback-allocation=N");
REDLINE_TUNABLE(kFlagsFeedbackAllocationCountMax,
                "flags.invocation_count_for_feedback_allocation_max", 8, 0,
                1000,
                "largest N of --invocation-count-for-feedback-allocation=N");
REDLINE_TUNABLE(kFlagsDeoptEveryNTimes, "flags.deopt_every_n_times", 0.1, 0, 1,
                "probability of --deopt-every-n-times=N, with N from the "
                "list of Fuzzilli");
REDLINE_TUNABLE(kFlagsNoUseIc, "flags.no_use_ic", 0.25, 0, 1,
                "probability of --no-use-ic: the inline caches collect no "
                "feedback");
REDLINE_TUNABLE(kFlagsDisableLoopStackChecks, "flags.disable_loop_stack_checks",
                0.1, 0, 1, "probability of --disable-loop-stack-checks");
REDLINE_TUNABLE(kFlagsStackSize, "flags.stack_size", 0.1, 0, 1,
                "probability of --stack-size=N");
REDLINE_TUNABLE(kFlagsStackSizeMin, "flags.stack_size_min", 54, 54, 984,
                "smallest N of --stack-size=N, in KB");
REDLINE_TUNABLE(kFlagsStackSizeMax, "flags.stack_size_max", 863, 54, 984,
                "largest N of --stack-size=N, in KB");
// Fuzzilli adds one of its six CPU feature flags with the probability 0.05.
// Thus each of these flags has the probability 0.05 / 6, about 0.0083.
REDLINE_TUNABLE(kFlagsNoEnableSse3, "flags.no_enable_sse3", 0.0083, 0, 1,
                "probability of --no-enable-sse3");
REDLINE_TUNABLE(kFlagsNoEnableSsse3, "flags.no_enable_ssse3", 0.0083, 0, 1,
                "probability of --no-enable-ssse3");
REDLINE_TUNABLE(kFlagsNoEnableSse41, "flags.no_enable_sse4_1", 0.0083, 0, 1,
                "probability of --no-enable-sse4-1");
REDLINE_TUNABLE(kFlagsNoEnableSse42, "flags.no_enable_sse4_2", 0.0083, 0, 1,
                "probability of --no-enable-sse4-2");
REDLINE_TUNABLE(kFlagsNoEnableAvx, "flags.no_enable_avx", 0.0083, 0, 1,
                "probability of --no-enable-avx");
REDLINE_TUNABLE(kFlagsNoEnableAvx2, "flags.no_enable_avx2", 0.0083, 0, 1,
                "probability of --no-enable-avx2");
REDLINE_TUNABLE(kFlagsNoEnableSahf, "flags.no_enable_sahf", 0.05, 0, 1,
                "probability of --no-enable-sahf");
REDLINE_TUNABLE(kFlagsNoEnableFma3, "flags.no_enable_fma3", 0.05, 0, 1,
                "probability of --no-enable-fma3");
REDLINE_TUNABLE(kFlagsNoEnableBmi1, "flags.no_enable_bmi1", 0.05, 0, 1,
                "probability of --no-enable-bmi1");
REDLINE_TUNABLE(kFlagsNoEnableBmi2, "flags.no_enable_bmi2", 0.05, 0, 1,
                "probability of --no-enable-bmi2");
REDLINE_TUNABLE(kFlagsNoEnableLzcnt, "flags.no_enable_lzcnt", 0.05, 0, 1,
                "probability of --no-enable-lzcnt");
REDLINE_TUNABLE(kFlagsNoEnablePopcnt, "flags.no_enable_popcnt", 0.05, 0, 1,
                "probability of --no-enable-popcnt");

// Flags of both profiles: features of the Maglev graph builder. Turbolev
// builds its graph with the Maglev graph builder, so these flags also change
// the code of Turbolev.
REDLINE_TUNABLE(kFlagsNoMaglevInlining, "flags.no_maglev_inlining", 0.025, 0, 1,
                "probability of --no-maglev-inlining");
REDLINE_TUNABLE(kFlagsNoMaglevPolyCalls, "flags.no_maglev_poly_calls", 0.025, 0,
                1, "probability of --no-maglev-poly-calls");
REDLINE_TUNABLE(kFlagsNoMaglevEscapeAnalysis, "flags.no_maglev_escape_analysis",
                0.025, 0, 1, "probability of --no-maglev-escape-analysis");
REDLINE_TUNABLE(kFlagsNoMaglevOptimisticPeeledLoops,
                "flags.no_maglev_optimistic_peeled_loops", 0.025, 0, 1,
                "probability of --no-maglev-optimistic-peeled-loops");
REDLINE_TUNABLE(kFlagsNoMaglevTruncation, "flags.no_maglev_truncation", 0.025,
                0, 1, "probability of --no-maglev-truncation");
REDLINE_TUNABLE(kFlagsNoMaglevCse, "flags.no_maglev_cse", 0.025, 0, 1,
                "probability of --no-maglev-cse");

// Flags of both profiles: garbage collection at more points, so that
// optimized code sees moved objects, trimmed descriptor arrays and flushed
// code.
REDLINE_TUNABLE(kFlagsStressCompaction, "flags.stress_compaction", 0.03, 0, 1,
                "probability of --stress-compaction");
REDLINE_TUNABLE(kFlagsStressCompactionRandom, "flags.stress_compaction_random",
                0.03, 0, 1, "probability of --stress-compaction-random");
REDLINE_TUNABLE(kFlagsStressMarking, "flags.stress_marking", 0.04, 0, 1,
                "probability of --stress-marking=N");
REDLINE_TUNABLE(kFlagsStressMarkingMin, "flags.stress_marking_min", 1, 0, 100,
                "smallest N of --stress-marking=N");
REDLINE_TUNABLE(kFlagsStressMarkingMax, "flags.stress_marking_max", 100, 0, 100,
                "largest N of --stress-marking=N");
REDLINE_TUNABLE(kFlagsStressScavenge, "flags.stress_scavenge", 0.04, 0, 1,
                "probability of --stress-scavenge=N");
REDLINE_TUNABLE(kFlagsStressScavengeMin, "flags.stress_scavenge_min", 1, 0, 100,
                "smallest N of --stress-scavenge=N");
REDLINE_TUNABLE(kFlagsStressScavengeMax, "flags.stress_scavenge_max", 100, 0,
                100, "largest N of --stress-scavenge=N");
REDLINE_TUNABLE(kFlagsGcInterval, "flags.gc_interval", 0.02, 0, 1,
                "probability of --gc-interval=N");
REDLINE_TUNABLE(kFlagsGcIntervalMin, "flags.gc_interval_min", 100, 1, 1000000,
                "smallest N of --gc-interval=N");
REDLINE_TUNABLE(kFlagsGcIntervalMax, "flags.gc_interval_max", 10000, 1, 1000000,
                "largest N of --gc-interval=N");
REDLINE_TUNABLE(kFlagsRandomGcInterval, "flags.random_gc_interval", 0.02, 0, 1,
                "probability of --random-gc-interval=N");
REDLINE_TUNABLE(kFlagsRandomGcIntervalMin, "flags.random_gc_interval_min", 1000,
                1, 1000000, "smallest N of --random-gc-interval=N");
REDLINE_TUNABLE(kFlagsRandomGcIntervalMax, "flags.random_gc_interval_max",
                10000, 1, 1000000, "largest N of --random-gc-interval=N");
REDLINE_TUNABLE(kFlagsStressFlushCode, "flags.stress_flush_code", 0.05, 0, 1,
                "probability of --stress-flush-code");
REDLINE_TUNABLE(kFlagsNoFlushBytecode, "flags.no_flush_bytecode", 0.1, 0, 1,
                "probability of --no-flush-bytecode");
REDLINE_TUNABLE(kFlagsMinorMs, "flags.minor_ms", 0.1, 0, 1,
                "probability of --minor-ms");
REDLINE_TUNABLE(kFlagsScavengerChaosMode, "flags.scavenger_chaos_mode", 0.1, 0,
                1,
                "probability of --scavenger-chaos-mode, together with "
                "--scavenger-chaos-mode-threshold=N");
REDLINE_TUNABLE(kFlagsScavengerChaosThresholdMin,
                "flags.scavenger_chaos_mode_threshold_min", 0, 0, 100,
                "smallest N of --scavenger-chaos-mode-threshold=N");
REDLINE_TUNABLE(kFlagsScavengerChaosThresholdMax,
                "flags.scavenger_chaos_mode_threshold_max", 100, 0, 100,
                "largest N of --scavenger-chaos-mode-threshold=N");
REDLINE_TUNABLE(kFlagsStressDescriptorArrayTrimming,
                "flags.stress_descriptor_array_trimming", 0.1, 0, 1,
                "probability of --stress-descriptor-array-trimming");
REDLINE_TUNABLE(kFlagsStressObjectPinning,
                "flags.stress_scavenger_conservative_object_pinning_random",
                0.1, 0, 1,
                "probability of "
                "--stress-scavenger-conservative-object-pinning-random");
REDLINE_TUNABLE(kFlagsPreciseObjectPinning, "flags.precise_object_pinning", 0.1,
                0, 1, "probability of --precise-object-pinning");

// Flags of all three profiles: heap heuristics, builtin calls and background
// compilation.
REDLINE_TUNABLE(kFlagsEfficiencyMode, "flags.efficiency_mode", 0.1, 0, 1,
                "probability of --efficiency-mode");
REDLINE_TUNABLE(kFlagsBatterySaverMode, "flags.battery_saver_mode", 0.1, 0, 1,
                "probability of --battery-saver-mode");
REDLINE_TUNABLE(kFlagsNoShortBuiltinCalls, "flags.no_short_builtin_calls", 0.1,
                0, 1, "probability of --no-short-builtin-calls");
REDLINE_TUNABLE(kFlagsStressBackgroundCompile,
                "flags.stress_background_compile", 0.025, 0, 1,
                "probability of --stress-background-compile");
REDLINE_TUNABLE(kFlagsLazyNewSpaceShrinking, "flags.lazy_new_space_shrinking",
                0.025, 0, 1, "probability of --lazy-new-space-shrinking");
REDLINE_TUNABLE(kFlagsMaglevAssertStackSize, "flags.maglev_assert_stack_size",
                0.05, 0, 1,
                "Maglev and Turbolev profiles: probability of "
                "--maglev-assert-stack-size");

// Flags of the Maglev profile only.
REDLINE_TUNABLE(kFlagsMaglevAsTopTier, "flags.maglev_as_top_tier", 0.5, 0, 1,
                "Maglev profile: probability of --maglev-as-top-tier, when "
                "the d8 flags of the configuration have --no-turbofan");
REDLINE_TUNABLE(kFlagsMaglevInlineApiCalls, "flags.maglev_inline_api_calls",
                0.025, 0, 1,
                "Maglev profile: probability of --maglev-inline-api-calls");

// Flags of the Turbolev profile only.
REDLINE_TUNABLE(kFlagsOptimizeMaglevToTurbofan,
                "flags.optimize_maglev_to_turbofan", 0.2, 0, 1,
                "Turbolev profile: probability of "
                "--optimize-maglev-optimizes-to-turbofan");
REDLINE_TUNABLE(kFlagsStressConcurrentInlining,
                "flags.stress_concurrent_inlining", 0.25, 0, 1,
                "Turbolev profile: probability of "
                "--stress-concurrent-inlining");
REDLINE_TUNABLE(kFlagsStressConcurrentInliningAttachCode,
                "flags.stress_concurrent_inlining_attach_code", 0.05, 0, 1,
                "Turbolev profile: probability of "
                "--stress-concurrent-inlining-attach-code");
// Only the compile dispatcher of Turbofan and Turbolev reads the two flags
// below. Maglev has a different dispatcher.
REDLINE_TUNABLE(kFlagsRecompilationQueueLength,
                "flags.concurrent_recompilation_queue_length", 0.04, 0, 1,
                "Turbolev profile: probability of "
                "--concurrent-recompilation-queue-length=N");
REDLINE_TUNABLE(kFlagsRecompilationQueueLengthMin,
                "flags.concurrent_recompilation_queue_length_min", 4, 1, 1000,
                "smallest N of --concurrent-recompilation-queue-length=N");
REDLINE_TUNABLE(kFlagsRecompilationQueueLengthMax,
                "flags.concurrent_recompilation_queue_length_max", 64, 1, 1000,
                "largest N of --concurrent-recompilation-queue-length=N");
REDLINE_TUNABLE(kFlagsRecompilationDelay,
                "flags.concurrent_recompilation_delay", 0.04, 0, 1,
                "Turbolev profile: probability of "
                "--concurrent-recompilation-delay=N: a delay of N ms for each "
                "concurrent compilation job");
REDLINE_TUNABLE(kFlagsRecompilationDelayMin,
                "flags.concurrent_recompilation_delay_min", 1, 0, 10000,
                "smallest N of --concurrent-recompilation-delay=N");
REDLINE_TUNABLE(kFlagsRecompilationDelayMax,
                "flags.concurrent_recompilation_delay_max", 500, 0, 10000,
                "largest N of --concurrent-recompilation-delay=N");
REDLINE_TUNABLE(kFlagsTurbolevInlinedSize, "flags.turbolev_inlined_size", 0.1,
                0, 1,
                "Turbolev profile: probability of "
                "--max-inlined-bytecode-size=N");
REDLINE_TUNABLE(kFlagsTurbolevInlinedSizeMin, "flags.turbolev_inlined_size_min",
                0, 0, 100000, "smallest N of --max-inlined-bytecode-size=N");
REDLINE_TUNABLE(kFlagsTurbolevInlinedSizeMax, "flags.turbolev_inlined_size_max",
                920, 0, 100000, "largest N of --max-inlined-bytecode-size=N");
REDLINE_TUNABLE(kFlagsTurbolevInlinedSizeCumulative,
                "flags.turbolev_inlined_size_cumulative", 1, 0, 1,
                "Turbolev profile: probability of "
                "--max-inlined-bytecode-size-cumulative=N+M, when "
                "--max-inlined-bytecode-size=N is drawn");
REDLINE_TUNABLE(kFlagsTurbolevInlinedSizeCumulativeMax,
                "flags.turbolev_inlined_size_cumulative_extra_max", 920, 0,
                100000,
                "largest M of --max-inlined-bytecode-size-cumulative=N+M");
REDLINE_TUNABLE(kFlagsTurbolevSmallInlinedSize,
                "flags.turbolev_small_inlined_size", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--max-inlined-bytecode-size-small=N, when the size 0 is not "
                "drawn");
REDLINE_TUNABLE(kFlagsTurbolevSmallInlinedSizeMin,
                "flags.turbolev_small_inlined_size_min", 0, 0, 100000,
                "smallest N of --max-inlined-bytecode-size-small=N");
REDLINE_TUNABLE(kFlagsTurbolevSmallInlinedSizeMax,
                "flags.turbolev_small_inlined_size_max", 54, 0, 100000,
                "largest N of --max-inlined-bytecode-size-small=N");
REDLINE_TUNABLE(kFlagsTurbolevHeapNumInlinedSize,
                "flags.turbolev_heapnum_inlined_size", 0.1, 0, 1,
                "Turbolev profile: probability of "
                "--max-inlined-bytecode-size-small-with-heapnum-in-out=N");
REDLINE_TUNABLE(kFlagsTurbolevHeapNumInlinedSizeMin,
                "flags.turbolev_heapnum_inlined_size_min", 0, 0, 100000,
                "smallest N of "
                "--max-inlined-bytecode-size-small-with-heapnum-in-out=N");
REDLINE_TUNABLE(kFlagsTurbolevHeapNumInlinedSizeMax,
                "flags.turbolev_heapnum_inlined_size_max", 150, 0, 100000,
                "largest N of "
                "--max-inlined-bytecode-size-small-with-heapnum-in-out=N");
// Only Turbolev reads --maglev-range-analysis.
REDLINE_TUNABLE(kFlagsNoMaglevRangeAnalysis, "flags.no_maglev_range_analysis",
                0.025, 0, 1,
                "Turbolev profile: probability of --no-maglev-range-analysis");
REDLINE_TUNABLE(kFlagsNoStringConcatEscapeAnalysis,
                "flags.no_turboshaft_string_concat_escape_analysis", 0.025, 0,
                1,
                "Turbolev profile: probability of "
                "--no-turboshaft-string-concat-escape-analysis");
// Fuzzilli draws each instruction scheduling flag with the probability 0.05
// when Turbofan is on, and again with the probability 0.025.
REDLINE_TUNABLE(kFlagsInstructionScheduling,
                "flags.turbo_instruction_scheduling", 0.075, 0, 1,
                "Turbolev profile: probability of "
                "--turbo-instruction-scheduling");
REDLINE_TUNABLE(kFlagsStressInstructionScheduling,
                "flags.turbo_stress_instruction_scheduling", 0.075, 0, 1,
                "Turbolev profile: probability of "
                "--turbo-stress-instruction-scheduling");
REDLINE_TUNABLE(
    kFlagsNoTurboMoveOptimization, "flags.no_turbo_move_optimization", 0.025, 0,
    1, "Turbolev profile: probability of --no-turbo-move-optimization");
REDLINE_TUNABLE(kFlagsNoTurboJt, "flags.no_turbo_jt", 0.025, 0, 1,
                "Turbolev profile: probability of --no-turbo-jt");
REDLINE_TUNABLE(
    kFlagsNoTurboStoreElimination, "flags.no_turbo_store_elimination", 0.025, 0,
    1, "Turbolev profile: probability of --no-turbo-store-elimination");
REDLINE_TUNABLE(kFlagsNoTurboLoopRotation, "flags.no_turbo_loop_rotation",
                0.025, 0, 1,
                "Turbolev profile: probability of --no-turbo-loop-rotation");
REDLINE_TUNABLE(kFlagsNoTurboAllocationFolding,
                "flags.no_turbo_allocation_folding", 0.025, 0, 1,
                "Turbolev profile: probability of "
                "--no-turbo-allocation-folding");
REDLINE_TUNABLE(kFlagsTurboshaftVerifyLoadElimination,
                "flags.turboshaft_verify_load_elimination", 0.2, 0, 1,
                "Turbolev profile: probability of "
                "--turboshaft-verify-load-elimination");
REDLINE_TUNABLE(kFlagsTurboshaftVerifyTaggedness,
                "flags.turboshaft_verify_load_store_taggedness", 0.2, 0, 1,
                "Turbolev profile: probability of "
                "--turboshaft-verify-load-store-taggedness");
REDLINE_TUNABLE(kFlagsTurboVerify, "flags.turbo_verify", 0.1, 0, 1,
                "Turbolev profile: probability of --turbo-verify");
REDLINE_TUNABLE(kFlagsTurboVerifyAllocation, "flags.turbo_verify_allocation",
                0.1, 0, 1,
                "Turbolev profile: probability of --turbo-verify-allocation");

// The values of --deopt-every-n-times=N in the V8 profile of Fuzzilli.
constexpr int kDeoptEveryNTimesValues[] = {100,  250,  500,  1000,
                                           2500, 5000, 10000};

enum class FuzzilliCond {
  kTop,
  kIfNoTurbofan,
  kElseNoTurbofan,
  kIfNotMaglevTopTier,
  kIfTurbofan,
  kIfMaglev,
  kIfMaglevNonEager,
  kIfMaglevTurbofan,
  kIfTurbolev,
  kIfTurbolevFuture,
  kIfTurbolevNoSmallZero,
  kIfTurbolevNotSandbox,
  kIfTurbofanNoAssertTypes,
  kIfProtoAssignSeq,
  kIfNotSandbox,
  kIfMaglevOrTurbolev,
  kIfNoLiftoff,
  kGcStress,
  kExotic,
};

// kFlagElse draws only if the kFlag row before it did not draw its flag.
// kFlagWithPrev and kRangeWithPrev draw only if the row before them drew its
// flag. kRangeCumulative adds the value of the kRange row before it to its
// own value.
enum class FuzzilliDrawKind {
  kGroup,
  kFlag,
  kFlagElse,
  kFlagWithPrev,
  kBoolFlag,
  kCoinFirst,
  kCoinSecond,
  kRange,
  kRangeWithPrev,
  kRangeCumulative,
  kRangeCoinFirst,
  kRangeCoinSecond,
  kDeopt,
  kCpuUniform,
};

struct FuzzilliFlagEntry {
  FuzzilliCond cond;
  FuzzilliDrawKind kind;
  double prob;
  const char* flag;
  int min_val;
  int max_val;
  // Only the `fuzzilli-wasm` profile draws the rows of the Wasm flags.
  bool wasm = false;
};

// The `fuzzilli` and `fuzzilli-wasm` profile table from `v8ProcessArgs` in
// `Sources/Fuzzilli/Profiles/V8CommonProfile.swift` and `V8Profile.swift` at
// commit `7eb96565`. `tools/d8_flag_sync.py` checks this table.
//
// Deferred flags (Fuzzilli draws them, this table does not draw them yet):
// --fuzzilli-enable-optimized-code-coverage.
constexpr FuzzilliFlagEntry kFuzzilliFlagTable[] = {
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.8, "--future", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--no-turbofan", 0, 0},
    {FuzzilliCond::kIfNoTurbofan, FuzzilliDrawKind::kFlag, 0.5,
     "--maglev-as-top-tier", 0, 0},
    {FuzzilliCond::kElseNoTurbofan, FuzzilliDrawKind::kGroup, 0.1, "", 0, 0},
    {FuzzilliCond::kElseNoTurbofan, FuzzilliDrawKind::kCoinFirst, 1.0,
     "--turbo-instruction-scheduling", 0, 0},
    {FuzzilliCond::kElseNoTurbofan, FuzzilliDrawKind::kCoinSecond, 1.0,
     "--turbo-stress-instruction-scheduling", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--no-flush-bytecode", 0,
     0},
    {FuzzilliCond::kIfNotMaglevTopTier, FuzzilliDrawKind::kFlag, 0.1,
     "--no-maglev", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--no-sparkplug", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--no-short-builtin-calls", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.8, "--jit-fuzzing", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.5, "--no-liftoff", 0, 0,
     true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlagElse, 0.2,
     "--wasm-sync-tier-up", 0, 0, true},
    {FuzzilliCond::kIfNoLiftoff, FuzzilliDrawKind::kFlag, 0.3,
     "--wasm-assert-types", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.5,
     "--wasm-inlining-ignore-call-counts", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.3,
     "--no-wasm-generic-wrapper", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.2,
     "--no-wasm-native-module-cache", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kRange, 0.2,
     "--wasm-caching-threshold", 1, 1000, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.2,
     "--wasm-random-rescheduling", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.4,
     "--wasm-partial-oob-writes-are-noops", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.8,
     "--no-baseline-batch-compilation", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.8,
     "--no-lazy-feedback-allocation", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--minor-ms", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.25, "--shared-strings", 0,
     0},
    {FuzzilliCond::kIfMaglev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-maglev-untagged-phis", 0, 0},
    {FuzzilliCond::kIfMaglev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-maglev-loop-peeling", 0, 0},
    {FuzzilliCond::kIfMaglev, FuzzilliDrawKind::kFlag, 0.25, "--maglev-future",
     0, 0},
    {FuzzilliCond::kIfMaglev, FuzzilliDrawKind::kFlag, 0.1, "--maglev-assert",
     0, 0},
    {FuzzilliCond::kIfMaglev, FuzzilliDrawKind::kFlag, 0.05,
     "--maglev-assert-stack-size", 0, 0},
    {FuzzilliCond::kIfMaglev, FuzzilliDrawKind::kFlag, 0.2,
     "--maglev-non-eager-inlining", 0, 0},
    {FuzzilliCond::kIfMaglevNonEager, FuzzilliDrawKind::kFlag, 0.35,
     "--max-maglev-eager-inlined-bytecode-size=0", 0, 0},
    {FuzzilliCond::kIfMaglevNonEager, FuzzilliDrawKind::kFlag, 0.4,
     "--max-maglev-inlined-bytecode-size-small=0", 0, 0},
    {FuzzilliCond::kIfMaglevTurbofan, FuzzilliDrawKind::kFlag, 0.1,
     "--always-osr-from-maglev", 0, 0},
    {FuzzilliCond::kIfTurbofan, FuzzilliDrawKind::kFlag, 0.7, "--turbolev", 0,
     0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.82,
     "--turbolev-future", 0, 0},
    {FuzzilliCond::kIfTurbolevFuture, FuzzilliDrawKind::kFlag, 0.3,
     "--max-inlined-bytecode-size-small=0", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--maglev-disable-builtin-reducers", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-turbolev-untagged-phis", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-turbolev-truncated-int32-phis", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-turbolev-non-eager-inlining", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-turbolev-non-eager-loop-peeling", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--no-turbolev-eager-loop-peeling-osr", 0, 0},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kRange, 0.1,
     "--max-inlined-bytecode-size", 0, 920},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kRangeCumulative, 1.0,
     "--max-inlined-bytecode-size-cumulative", 0, 920},
    {FuzzilliCond::kIfTurbolevNoSmallZero, FuzzilliDrawKind::kRange, 0.1,
     "--max-inlined-bytecode-size-small", 0, 54},
    {FuzzilliCond::kIfTurbolev, FuzzilliDrawKind::kRange, 0.1,
     "--max-inlined-bytecode-size-small-with-heapnum-in-out", 0, 150},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--efficiency-mode", 0,
     0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--battery-saver-mode",
     0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--stress-scavenger-conservative-object-pinning-random", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--precise-object-pinning", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--scavenger-chaos-mode",
     0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kRangeWithPrev, 1.0,
     "--scavenger-chaos-mode-threshold", 0, 100},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--stress-descriptor-array-trimming", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kRange, 0.1, "--stack-size", 54,
     863},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kRange, 0.5,
     "--wasm-stack-switching-stack-size", 1, 300, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.5, "--wasm-growable-stacks",
     0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-wasm-stack-switching", 0, 0, true},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.5,
     "--proto-assign-seq-lazy-func-opt", 0, 0},
    {FuzzilliCond::kIfProtoAssignSeq, FuzzilliDrawKind::kFlag, 0.5,
     "--proto-assign-seq-opt-count=1", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.5,
     "--private-field-bytecodes", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kCoinFirst, 0.67,
     "--verify-bytecode-light", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kCoinSecond, 0.67,
     "--verify-bytecode-full", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--disable-loop-stack-checks", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.1, "--verify-heap",
     0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.1,
     "--turbo-verify", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.1,
     "--turbo-verify-allocation", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.1,
     "--assert-types", 0, 0},
    {FuzzilliCond::kIfMaglevOrTurbolev, FuzzilliDrawKind::kFlag, 0.1,
     "--maglev-assert-types", 0, 0},
    {FuzzilliCond::kIfTurbolevNotSandbox, FuzzilliDrawKind::kFlag, 0.2,
     "--maglev-range-verification", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.2,
     "--verify-turboshaft", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.2,
     "--turboshaft-verify-load-elimination", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.1,
     "--turboshaft-verify-reductions", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.2,
     "--turboshaft-verify-load-store-taggedness", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-lazy-source-positions", 0, 0},
    {FuzzilliCond::kIfNotSandbox, FuzzilliDrawKind::kFlag, 0.9, "--stress-lazy",
     0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kDeopt, 0.1, "--deopt-every-n-times",
     0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1, "--stress-ic", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlagElse, 0.1, "--no-use-ic", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kRange, 0.1,
     "--invocation-count-for-feedback-allocation", 0, 8},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--no-additive-safe-int-feedback", 0, 0},
    {FuzzilliCond::kTop, FuzzilliDrawKind::kFlag, 0.1,
     "--optimize-on-next-call-optimizes-to-maglev", 0, 0},
    {FuzzilliCond::kIfTurbofan, FuzzilliDrawKind::kFlag, 0.2,
     "--optimize-maglev-optimizes-to-turbofan", 0, 0},
    {FuzzilliCond::kIfTurbofanNoAssertTypes, FuzzilliDrawKind::kFlag, 0.25,
     "--stress-concurrent-inlining", 0, 0},
    {FuzzilliCond::kIfTurbofanNoAssertTypes, FuzzilliDrawKind::kFlagWithPrev,
     0.2, "--stress-concurrent-inlining-attach-code", 0, 0},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kGroup, 0.1, "", 0, 0},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kRange, 0.4, "--stress-marking",
     1, 100},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kRange, 0.4,
     "--stress-scavenge", 1, 100},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-flush-code", 0, 0},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kFlag, 0.5, "--wasm-code-gc", 0,
     0, true},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kFlagWithPrev, 1.0,
     "--stress-wasm-code-gc", 0, 0, true},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-wasm-memory-moving", 0, 0, true},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kRangeCoinFirst, 0.4,
     "--gc-interval", 100, 10000},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kRangeCoinSecond, 0.4,
     "--random-gc-interval", 1000, 10000},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kRange, 0.4,
     "--concurrent-recompilation-queue-length", 4, 64},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kRangeWithPrev, 1.0,
     "--concurrent-recompilation-delay", 1, 500},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kCoinFirst, 0.6,
     "--stress-compaction", 0, 0},
    {FuzzilliCond::kGcStress, FuzzilliDrawKind::kCoinSecond, 0.6,
     "--stress-compaction-random", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kGroup, 0.05, "", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-gc-during-compilation", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kFlag, 0.5,
     "--lazy-new-space-shrinking", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-wasm-memory-moving", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kFlag, 0.5,
     "--stress-background-compile", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--always-sparkplug", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--always-osr", 0,
     0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--concurrent-osr", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--force-slow-path", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--lazy", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--lazy-eval", 0,
     0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-inline-api-calls", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-inlining", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-optimistic-peeled-loops", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-poly-calls", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-truncation", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--maglev-cse", 0,
     0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-range-analysis", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--maglev-escape-analysis", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-move-optimization", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--turbo-jt", 0,
     0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-loop-peeling", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-loop-variable", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-loop-rotation", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-cf-optimization", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--turbo-escape",
     0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-allocation-folding", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-instruction-scheduling", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-stress-instruction-scheduling", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-store-elimination", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-rewrite-far-jumps", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-optimize-apply", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-load-elimination", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turbo-inlining", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-sse3", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-ssse3", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-sse4-1", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-sse4-2", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-avx", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-avx2", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-sahf", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-fma3", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-bmi1", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-bmi2", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-lzcnt", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kCpuUniform, 1.0,
     "--no-enable-popcnt", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turboshaft-loop-unrolling", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turboshaft-load-elimination", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turboshaft-string-concat-escape-analysis", 0, 0},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--wasm-loop-peeling", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--turboshaft-wasm-load-elimination", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--wasm-loop-unrolling", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--wasm-inlining",
     0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--wasm-opt", 0,
     0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5, "--wasm-deopt", 0,
     0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--wasm-enforce-bounds-checks", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--wasm-math-intrinsics", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--wasm-bulkmem-inlining", 0, 0, true},
    {FuzzilliCond::kExotic, FuzzilliDrawKind::kBoolFlag, 0.5,
     "--wasm-lazy-compilation", 0, 0, true},
};

constexpr size_t CountFuzzilliCpuFlags() {
  size_t count = 0;
  for (const FuzzilliFlagEntry& e : kFuzzilliFlagTable) {
    if (e.kind == FuzzilliDrawKind::kCpuUniform) {
      count++;
    }
  }
  return count;
}

constexpr size_t kFuzzilliCpuFlagCount = CountFuzzilliCpuFlags();
static_assert(kFuzzilliCpuFlagCount > 0);

constexpr bool ValidateFuzzilliFlagTable() {
  const size_t n = std::size(kFuzzilliFlagTable);
  size_t cpu_seen = 0;
  bool in_cpu = false;
  for (size_t i = 0; i < n; i++) {
    const FuzzilliFlagEntry& e = kFuzzilliFlagTable[i];
    if (e.kind == FuzzilliDrawKind::kCpuUniform) {
      if (!in_cpu && cpu_seen > 0) return false;
      in_cpu = true;
      cpu_seen++;
    } else {
      in_cpu = false;
    }
    if (e.kind == FuzzilliDrawKind::kCoinFirst) {
      if (i + 1 >= n ||
          kFuzzilliFlagTable[i + 1].kind != FuzzilliDrawKind::kCoinSecond ||
          kFuzzilliFlagTable[i + 1].cond != e.cond ||
          kFuzzilliFlagTable[i + 1].prob != e.prob) {
        return false;
      }
    } else if (e.kind == FuzzilliDrawKind::kRangeCoinFirst) {
      if (i + 1 >= n ||
          kFuzzilliFlagTable[i + 1].kind !=
              FuzzilliDrawKind::kRangeCoinSecond ||
          kFuzzilliFlagTable[i + 1].cond != e.cond ||
          kFuzzilliFlagTable[i + 1].prob != e.prob) {
        return false;
      }
    } else if (e.kind == FuzzilliDrawKind::kBoolFlag) {
      const std::string_view sv(e.flag);
      if (sv.size() < 3 || sv.substr(0, 2) != "--" ||
          sv.substr(0, 5) == "--no-") {
        return false;
      }
    } else if (e.kind == FuzzilliDrawKind::kRangeWithPrev ||
               e.kind == FuzzilliDrawKind::kRangeCumulative) {
      if (e.prob != 1.0) return false;
    }
    // Some rows read the draw of the row before them. That row must have
    // the expected kind, the same condition and the same Wasm gate.
    const bool after_flag = e.kind == FuzzilliDrawKind::kFlagElse ||
                            e.kind == FuzzilliDrawKind::kFlagWithPrev;
    const bool after_range = e.kind == FuzzilliDrawKind::kRangeCumulative;
    if (after_flag || after_range ||
        e.kind == FuzzilliDrawKind::kRangeWithPrev) {
      if (i == 0) return false;
      const FuzzilliFlagEntry& prev = kFuzzilliFlagTable[i - 1];
      if (prev.cond != e.cond || prev.wasm != e.wasm ||
          (after_flag && prev.kind != FuzzilliDrawKind::kFlag) ||
          (e.kind == FuzzilliDrawKind::kRangeWithPrev &&
           prev.kind != FuzzilliDrawKind::kFlag &&
           prev.kind != FuzzilliDrawKind::kRange) ||
          (after_range && prev.kind != FuzzilliDrawKind::kRange)) {
        return false;
      }
    }
  }
  return cpu_seen == kFuzzilliCpuFlagCount;
}
static_assert(ValidateFuzzilliFlagTable());

REDLINE_TUNABLE(kFlagsTargetGuidedChance, "flags.target_guided_chance", 0.75, 0,
                1,
                "change-based fuzzing: probability that a new target process "
                "picks flags that reached target edges");
REDLINE_TUNABLE(kFlagsTargetGuidedWarmup, "flags.target_guided_warmup", 4, 0,
                1000,
                "change-based fuzzing: completed children before "
                "flags.target_guided_chance scales with the miss rate");
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
REDLINE_TUNABLE(kFlagsImportedChance, "flags.imported_chance", 0.25, 0, 1,
                "normal mode: chance that a new target process starts from "
                "the `// Flags:` set of an imported test");
REDLINE_TUNABLE(kFlagsAffinityChance, "flags.affinity_chance", 0.15, 0, 1,
                "normal mode: chance that a new target process starts from "
                "a random flag set that found many new edges");
REDLINE_TUNABLE(kFlagsSeedAffinityTries, "flags.seed_affinity_tries", 2, 1, 16,
                "seed candidates that a worker examines to find a program "
                "that shares a flag with its target process (1: off)");

// D8FlagBlacklist: a failed start blocks its only new flag after this number
// of started target processes.
constexpr uint64_t kBlacklistMinStarts = 4;
constexpr uint32_t kBlacklistRepeatedFailures = 2;

// The flag pairs that CONTRADICTORY_FLAGS of
// tools/clusterfuzz/js_fuzzer/exceptions.js lists.
constexpr std::pair<std::string_view, std::string_view> kContradictoryFlags[] =
    {
        {"--assert-types", "--stress-concurrent-inlining"},
        {"--assert-types", "--stress-concurrent-inlining-attach-code"},
        {"--jitless", "--maglev"},
        {"--jitless", "--maglev-future"},
        {"--jitless", "--stress-maglev"},
        {"--jitless", "--stress-concurrent-inlining"},
        {"--jitless", "--stress-concurrent-inlining-attach-code"},
};

// The flag names that a test cannot give. DISALLOWED_FLAGS and
// DISALLOWED_FLAGS_WITH_DISCOURAGED_FILES of exceptions.js give most of them.
// The other names write logs or traces, stop d8, change the d8 shell or give
// false crashes.
constexpr std::string_view kDeniedTestFlags[] = {
    // exceptions.js.
    "--correctness-fuzzer-suppressions",
    "--expose-trigger-failure",
    "--sandbox-testing",
    "--sandbox-fuzzing",
    "--allow-natives-syntax",
    "--debug-code",
    "--disable-abortjs",
    "--enable-slow-asserts",
    "--expose-gc",
    "--fuzzing",
    "--omit-quit",
    "--disable-in-process-stack-traces",
    "--invoke-weak-callbacks",
    "--verify-heap",
    "--stack-size",
    "--check-handle-count",
    "--expose-debug-as",
    "--expose-natives-as",
    "--mock-arraybuffer-allocator",
    // Redline.
    "--help",
    "--abort-on-contradictory-flags",
    "--single-threaded",
    "--trace",
    "--simulate-errors",
    "--enable-os-system",
    "--isolate",
    "--module",
    "--throws",
    "--stress-runs",
    "--expose-memory-corruption-api",
};

constexpr std::string_view kDeniedTestFlagPrefixes[] = {
    // exceptions.js.
    "--experimental-",
    "--icu-data-file",
    "--random-seed",
    // Redline: logs, traces and profiles.
    "--print-",
    "--trace-",
    "--perf-",
    "--prof",
    "--log",
    "--gdbjit",
    "--heap-snapshot",
    "--wasm-pgo",
};

std::string_view FlagPrefix(std::string_view flag) {
  const size_t eq = flag.find('=');
  return eq == std::string_view::npos ? flag : flag.substr(0, eq);
}

// The flag name of `flag` with `-` for `_`.
std::string NormalizedName(std::string_view flag) {
  std::string name(FlagPrefix(flag));
  std::replace(name.begin(), name.end(), '_', '-');
  return name;
}

// `flag` with `-` for `_` in its name. The value stays.
std::string NormalizedFlag(std::string_view flag) {
  const std::string_view name = FlagPrefix(flag);
  return NormalizedName(name) + std::string(flag.substr(name.size()));
}

// The normalized name of `flag` without `no-`: `--foo` and `--no-foo` have
// the same key.
std::string FlagKey(std::string_view flag) {
  std::string key = NormalizedName(flag);
  if (key.starts_with("--no-")) {
    key.erase(2, 3);
  }
  return key;
}

bool IsDeniedTestFlagName(std::string_view name) {
  if (name.starts_with("--harmony-") && name != "--harmony-shipping") {
    return true;
  }
  if (name.starts_with("--js-") && name != "--js-staging" &&
      name != "--js-shipping") {
    return true;
  }
  for (std::string_view denied : kDeniedTestFlags) {
    if (name == denied) {
      return true;
    }
  }
  for (std::string_view prefix : kDeniedTestFlagPrefixes) {
    if (name.starts_with(prefix)) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> SplitWhitespace(std::string_view text) {
  std::vector<std::string> words;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    const size_t start = i;
    while (i < text.size() && !isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    if (i > start) {
      words.emplace_back(text.substr(start, i - start));
    }
  }
  return words;
}

bool HasFlagName(const std::vector<std::string>& flags, std::string_view name) {
  return std::any_of(flags.begin(), flags.end(), [name](const std::string& f) {
    return NormalizedName(f) == name;
  });
}

bool HasFlagKey(const std::vector<std::string>& flags, std::string_view key) {
  return std::any_of(flags.begin(), flags.end(),
                     [key](const std::string& f) { return FlagKey(f) == key; });
}

int64_t FlagIntValue(const std::string& flag) {
  const size_t eq = flag.find('=');
  return eq == std::string::npos
             ? -1
             : std::strtoll(flag.c_str() + eq + 1, nullptr, 10);
}

bool ContainsFlag(const std::vector<std::string>& list, std::string_view name) {
  return std::any_of(list.begin(), list.end(),
                     [name](const std::string& f) { return f == name; });
}

// Draws the rows of kFuzzilliFlagTable. The Wasm rows need `with_wasm`.
void DrawFuzzilliD8Flags(Random& rng,
                         const std::vector<std::string>& fixed_args,
                         bool with_wasm, std::vector<std::string>* flags) {
  bool drew_no_turbofan = false;
  bool else_no_turbofan = false;
  bool has_no_turbofan = ContainsFlag(fixed_args, "--no-turbofan");
  bool has_top_tier = ContainsFlag(fixed_args, "--maglev-as-top-tier");
  bool has_no_maglev = ContainsFlag(fixed_args, "--no-maglev");
  bool drew_maglev_non_eager = false;
  bool has_turbolev = ContainsFlag(fixed_args, "--turbolev");
  bool drew_turbolev = false;
  bool drew_turbolev_future = false;
  bool drew_small_zero = false;
  bool has_assert_types = ContainsFlag(fixed_args, "--assert-types");
  bool drew_proto_assign_seq = false;
  bool drew_no_liftoff = false;
  const bool for_sandbox = ContainsFlag(fixed_args, "--sandbox-fuzzing") ||
                           ContainsFlag(fixed_args, "--sandbox-testing");
  bool in_gc_stress = false;
  bool in_exotic = false;
  bool prev_drawn = false;
  int64_t prev_value = 0;

  const size_t n = std::size(kFuzzilliFlagTable);
  for (size_t i = 0; i < n; i++) {
    const FuzzilliFlagEntry& e = kFuzzilliFlagTable[i];
    if (e.kind == FuzzilliDrawKind::kGroup) {
      if (e.cond == FuzzilliCond::kElseNoTurbofan) {
        else_no_turbofan = !drew_no_turbofan && rng.Chance(e.prob);
      } else if (e.cond == FuzzilliCond::kGcStress) {
        in_gc_stress = rng.Chance(e.prob);
      } else if (e.cond == FuzzilliCond::kExotic) {
        in_exotic = rng.Chance(e.prob);
      }
      prev_drawn = false;
      continue;
    }

    bool active = false;
    switch (e.cond) {
      case FuzzilliCond::kTop:
        active = true;
        break;
      case FuzzilliCond::kIfNoTurbofan:
        active = drew_no_turbofan;
        break;
      case FuzzilliCond::kElseNoTurbofan:
        active = else_no_turbofan;
        break;
      case FuzzilliCond::kIfNotMaglevTopTier:
        active = !has_top_tier;
        break;
      case FuzzilliCond::kIfTurbofan:
        active = !has_no_turbofan;
        break;
      case FuzzilliCond::kIfMaglev:
        active = !has_no_maglev;
        break;
      case FuzzilliCond::kIfMaglevNonEager:
        active = !has_no_maglev && drew_maglev_non_eager;
        break;
      case FuzzilliCond::kIfMaglevTurbofan:
        active = !has_no_maglev && !has_no_turbofan;
        break;
      case FuzzilliCond::kIfTurbolev:
        active = drew_turbolev;
        break;
      case FuzzilliCond::kIfTurbolevFuture:
        active = drew_turbolev && drew_turbolev_future;
        break;
      case FuzzilliCond::kIfTurbolevNoSmallZero:
        active = drew_turbolev && !drew_small_zero;
        break;
      case FuzzilliCond::kIfTurbolevNotSandbox:
        active = has_turbolev && !for_sandbox;
        break;
      case FuzzilliCond::kIfTurbofanNoAssertTypes:
        active = !has_no_turbofan && !has_assert_types;
        break;
      case FuzzilliCond::kIfProtoAssignSeq:
        active = drew_proto_assign_seq;
        break;
      case FuzzilliCond::kIfNotSandbox:
        active = !for_sandbox;
        break;
      case FuzzilliCond::kIfMaglevOrTurbolev:
        active = !for_sandbox && (!has_no_maglev || has_turbolev);
        break;
      case FuzzilliCond::kIfNoLiftoff:
        active = drew_no_liftoff && !for_sandbox;
        break;
      case FuzzilliCond::kGcStress:
        active = in_gc_stress;
        break;
      case FuzzilliCond::kExotic:
        active = in_exotic;
        break;
    }
    if (!active || (e.wasm && !with_wasm)) {
      if (e.kind == FuzzilliDrawKind::kCoinFirst ||
          e.kind == FuzzilliDrawKind::kRangeCoinFirst) {
        i++;
      } else if (e.kind == FuzzilliDrawKind::kCpuUniform) {
        i += kFuzzilliCpuFlagCount - 1;
      }
      prev_drawn = false;
      continue;
    }

    switch (e.kind) {
      case FuzzilliDrawKind::kFlag:
      case FuzzilliDrawKind::kFlagElse:
      case FuzzilliDrawKind::kFlagWithPrev: {
        bool drawn = false;
        if (e.kind == FuzzilliDrawKind::kFlag) {
          drawn = rng.Chance(e.prob);
        } else if (e.kind == FuzzilliDrawKind::kFlagElse) {
          drawn = !prev_drawn && rng.Chance(e.prob);
        } else {
          drawn = prev_drawn && rng.Chance(e.prob);
        }
        if (e.kind != FuzzilliDrawKind::kFlagWithPrev) {
          prev_drawn = drawn;
        }
        if (drawn) {
          flags->emplace_back(e.flag);
          const std::string_view name(e.flag);
          if (name == "--no-turbofan") {
            drew_no_turbofan = true;
            has_no_turbofan = true;
          } else if (name == "--maglev-as-top-tier") {
            has_top_tier = true;
          } else if (name == "--no-maglev") {
            has_no_maglev = true;
          } else if (name == "--maglev-non-eager-inlining") {
            drew_maglev_non_eager = true;
          } else if (name == "--turbolev") {
            drew_turbolev = true;
            has_turbolev = true;
          } else if (name == "--turbolev-future") {
            drew_turbolev_future = true;
          } else if (name == "--max-inlined-bytecode-size-small=0") {
            drew_small_zero = true;
          } else if (name == "--assert-types") {
            has_assert_types = true;
          } else if (name == "--proto-assign-seq-lazy-func-opt") {
            drew_proto_assign_seq = true;
          } else if (name == "--no-liftoff") {
            drew_no_liftoff = true;
          }
        }
        break;
      }
      case FuzzilliDrawKind::kBoolFlag: {
        if (rng.Chance(e.prob)) {
          flags->emplace_back(e.flag);
        } else {
          flags->push_back("--no-" + std::string(e.flag + 2));
        }
        prev_drawn = false;
        break;
      }
      case FuzzilliDrawKind::kCoinFirst: {
        const FuzzilliFlagEntry& second = kFuzzilliFlagTable[++i];
        if (rng.Chance(e.prob)) {
          flags->emplace_back(rng.Chance(0.5) ? e.flag : second.flag);
        }
        prev_drawn = false;
        break;
      }
      case FuzzilliDrawKind::kRange: {
        const bool drawn = rng.Chance(e.prob);
        prev_drawn = drawn;
        if (drawn) {
          prev_value = rng.Range(e.min_val, e.max_val);
          flags->push_back(std::string(e.flag) + "=" +
                           std::to_string(prev_value));
        }
        break;
      }
      case FuzzilliDrawKind::kRangeWithPrev:
      case FuzzilliDrawKind::kRangeCumulative: {
        if (prev_drawn) {
          int64_t value = rng.Range(e.min_val, e.max_val);
          if (e.kind == FuzzilliDrawKind::kRangeCumulative) {
            value += prev_value;
          }
          flags->push_back(std::string(e.flag) + "=" + std::to_string(value));
        }
        break;
      }
      case FuzzilliDrawKind::kRangeCoinFirst: {
        const FuzzilliFlagEntry& second = kFuzzilliFlagTable[++i];
        if (rng.Chance(e.prob)) {
          const FuzzilliFlagEntry& pick = rng.Chance(0.5) ? e : second;
          flags->push_back(
              std::string(pick.flag) + "=" +
              std::to_string(rng.Range(pick.min_val, pick.max_val)));
        }
        prev_drawn = false;
        break;
      }
      case FuzzilliDrawKind::kDeopt: {
        if (rng.Chance(e.prob)) {
          const int val = kDeoptEveryNTimesValues[rng.Index(
              std::size(kDeoptEveryNTimesValues))];
          flags->push_back(std::string(e.flag) + "=" + std::to_string(val));
        }
        prev_drawn = false;
        break;
      }
      case FuzzilliDrawKind::kCpuUniform: {
        const size_t pick = rng.Index(kFuzzilliCpuFlagCount);
        flags->emplace_back(kFuzzilliFlagTable[i + pick].flag);
        i += kFuzzilliCpuFlagCount - 1;
        prev_drawn = false;
        break;
      }
      case FuzzilliDrawKind::kGroup:
      case FuzzilliDrawKind::kCoinSecond:
      case FuzzilliDrawKind::kRangeCoinSecond:
        break;
    }
  }
}

// Removes drawn flags that clash with `fixed_args` in the `fuzzilli` profile.
// Keeps pairs of drawn flags unchanged, because `d8 --fuzzing` accepts the
// flag sets of Fuzzilli. Fixed flags such as `--jitless`, `--lite-mode`,
// `--disable-optimizing-compilers`, `--max-lazy` or `--empty-shared-heap`
// clash with some drawn flags.
void SanitizeFuzzilliFixedClashes(std::vector<std::string>* flags,
                                  const std::vector<std::string>& fixed_args) {
  if (fixed_args.empty()) {
    return;
  }
  auto has_fixed = [&](std::string_view name) {
    return ContainsFlag(fixed_args, name);
  };
  auto remove_exact = [&](std::string_view name) {
    flags->erase(std::remove_if(
                     flags->begin(), flags->end(),
                     [name](const std::string& flag) { return flag == name; }),
                 flags->end());
  };

  if (has_fixed("--jitless") || has_fixed("--lite-mode") ||
      has_fixed("--disable-optimizing-compilers")) {
    remove_exact("--maglev-as-top-tier");
    remove_exact("--maglev-future");
    remove_exact("--turbolev-future");
    remove_exact("--jit-fuzzing");
    remove_exact("--verify-turboshaft");
    remove_exact("--always-osr-from-maglev");
    remove_exact("--optimize-maglev-optimizes-to-turbofan");
  }
  // V8 resets a fixed flag that clashes with --stress-concurrent-inlining.
  if (has_fixed("--maglev-as-top-tier") || has_fixed("--single-threaded") ||
      has_fixed("--turboshaft-assert-types") || has_fixed("--jitless") ||
      has_fixed("--lite-mode") || has_fixed("--disable-optimizing-compilers")) {
    remove_exact("--stress-concurrent-inlining");
    remove_exact("--stress-concurrent-inlining-attach-code");
  }
  if (has_fixed("--turbofan") || has_fixed("--turbolev") ||
      has_fixed("--turbolev-future") || has_fixed("--no-maglev")) {
    remove_exact("--maglev-as-top-tier");
  }
  if (has_fixed("--maglev-as-top-tier")) {
    remove_exact("--turbolev-future");
  }
  if (has_fixed("--stress-concurrent-inlining") ||
      has_fixed("--stress-concurrent-inlining-attach-code")) {
    remove_exact("--assert-types");
    remove_exact("--maglev-as-top-tier");
  }
  if (has_fixed("--predictable-gc-schedule")) {
    remove_exact("--stress-compaction");
    remove_exact("--stress-scavenger-conservative-object-pinning-random");
  }
  if (has_fixed("--no-turbofan") && has_fixed("--no-maglev")) {
    remove_exact("--jit-fuzzing");
  }
  if (has_fixed("--max-lazy")) {
    remove_exact("--jit-fuzzing");
    remove_exact("--no-lazy");
  }
  if (has_fixed("--empty-shared-heap")) {
    remove_exact("--shared-strings");
  }
}

}  // namespace

void SanitizeD8Flags(std::vector<std::string>* flags,
                     const std::vector<std::string>& fixed_args) {
  std::vector<std::string> deduped;
  std::vector<std::string> keys;
  deduped.reserve(flags->size());
  for (const std::string& flag : *flags) {
    std::string key = FlagKey(flag);
    if (std::find(keys.begin(), keys.end(), key) == keys.end()) {
      keys.push_back(std::move(key));
      deduped.push_back(flag);
    }
  }
  *flags = std::move(deduped);

  auto has_fixed = [&](std::string_view name) {
    return ContainsFlag(fixed_args, name);
  };
  auto has_any = [&](std::string_view name) {
    return ContainsFlag(*flags, name) || ContainsFlag(fixed_args, name);
  };
  auto remove_exact = [&](std::string_view name) {
    flags->erase(std::remove_if(
                     flags->begin(), flags->end(),
                     [name](const std::string& flag) { return flag == name; }),
                 flags->end());
  };
  auto remove_prefix = [&](std::string_view prefix) {
    flags->erase(std::remove_if(flags->begin(), flags->end(),
                                [prefix](const std::string& flag) {
                                  return FlagPrefix(flag) == prefix;
                                }),
                 flags->end());
  };

  // --maglev-as-top-tier turns off Turbofan and Turbolev, and turns on Maglev.
  // Thus it clashes with these flags.
  if (has_any("--turbofan") || has_any("--turbolev") ||
      has_any("--turbolev-future") || has_any("--no-maglev") ||
      has_fixed("--jitless") || has_fixed("--lite-mode") ||
      has_fixed("--disable-optimizing-compilers")) {
    remove_exact("--maglev-as-top-tier");
  }
  if (has_fixed("--stress-concurrent-inlining") ||
      has_fixed("--stress-concurrent-inlining-attach-code")) {
    remove_exact("--assert-types");
    remove_exact("--single-threaded");
    remove_exact("--maglev-as-top-tier");
  }
  if (has_any("--assert-types") || has_any("--single-threaded") ||
      has_any("--no-concurrent-recompilation") || has_any("--no-turbofan") ||
      has_any("--jitless") || has_any("--lite-mode") ||
      has_any("--disable-optimizing-compilers") ||
      has_any("--maglev-as-top-tier") || has_any("--turboshaft-assert-types")) {
    remove_exact("--stress-concurrent-inlining");
    remove_exact("--stress-concurrent-inlining-attach-code");
  }

  // --always-sparkplug turns --sparkplug back on, so keep at most one.
  if (has_fixed("--always-sparkplug") || has_fixed("--sparkplug")) {
    remove_exact("--no-sparkplug");
  } else if (has_any("--no-sparkplug") || has_any("--jitless")) {
    remove_exact("--always-sparkplug");
  }

  if (has_fixed("--verify-bytecode-light") ||
      has_fixed("--no-verify-bytecode-full")) {
    remove_exact("--verify-bytecode-full");
  }
  if (has_any("--verify-bytecode-full") ||
      has_fixed("--no-verify-bytecode-light")) {
    remove_exact("--verify-bytecode-light");
  }

  if (has_fixed("--no-turbofan") || has_fixed("--jitless") ||
      has_fixed("--lite-mode") || has_fixed("--disable-optimizing-compilers")) {
    remove_exact("--always-osr-from-maglev");
    remove_exact("--optimize-maglev-optimizes-to-turbofan");
  }
  if (has_fixed("--jitless") || has_fixed("--lite-mode") ||
      has_fixed("--disable-optimizing-compilers")) {
    remove_exact("--maglev-future");
    remove_exact("--turbolev-future");
    remove_exact("--verify-turboshaft");
    remove_exact("--jit-fuzzing");
  }
  if (has_fixed("--no-turbofan") && has_fixed("--no-maglev")) {
    remove_exact("--jit-fuzzing");
  }
  if (has_fixed("--max-lazy")) {
    remove_exact("--jit-fuzzing");
    remove_exact("--no-lazy");
  }
  if (has_fixed("--sandbox-fuzzing") || has_fixed("--sandbox-testing")) {
    remove_exact("--stress-lazy");
    remove_exact("--stress-lazy-source-positions");
  }
  if (has_fixed("--empty-shared-heap")) {
    remove_exact("--shared-strings");
    remove_exact("--shared-string-table");
  }

  if (!has_any("--maglev-non-eager-inlining") ||
      has_fixed("--no-maglev-non-eager-inlining")) {
    remove_exact("--max-maglev-eager-inlined-bytecode-size=0");
    remove_exact("--max-maglev-inlined-bytecode-size-small=0");
  }
  if (!has_any("--scavenger-chaos-mode") ||
      has_fixed("--no-scavenger-chaos-mode")) {
    remove_prefix("--scavenger-chaos-mode-threshold");
  }

  auto size_it =
      std::find_if(flags->begin(), flags->end(), [](const std::string& flag) {
        return FlagPrefix(flag) == "--max-inlined-bytecode-size";
      });
  auto cum_it =
      std::find_if(flags->begin(), flags->end(), [](const std::string& flag) {
        return FlagPrefix(flag) == "--max-inlined-bytecode-size-cumulative";
      });
  if (cum_it != flags->end()) {
    if (size_it == flags->end() ||
        FlagIntValue(*cum_it) < FlagIntValue(*size_it)) {
      flags->erase(cum_it);
    } else if (cum_it < size_it) {
      std::iter_swap(size_it, cum_it);
    }
  }

  // The contradictory pairs: the flag of `fixed_args` stays, else the first
  // flag stays.
  for (const auto& [a, b] : kContradictoryFlags) {
    auto it_a = std::find(flags->begin(), flags->end(), a);
    auto it_b = std::find(flags->begin(), flags->end(), b);
    const bool in_a = it_a != flags->end();
    const bool in_b = it_b != flags->end();
    if (in_a && has_fixed(b)) {
      flags->erase(it_a);
    } else if (in_b && has_fixed(a)) {
      flags->erase(it_b);
    } else if (in_a && in_b) {
      flags->erase(it_a < it_b ? it_b : it_a);
    }
  }
}

bool ParseD8FlagProfile(const std::string& name, D8FlagProfile* profile) {
  if (name == "maglev") {
    *profile = D8FlagProfile::kMaglev;
    return true;
  }
  if (name == "turbolev") {
    *profile = D8FlagProfile::kTurbolev;
    return true;
  }
  if (name == "fuzzilli") {
    *profile = D8FlagProfile::kFuzzilli;
    return true;
  }
  if (name == "fuzzilli-wasm") {
    *profile = D8FlagProfile::kFuzzilliWasm;
    return true;
  }
  if (name == "runtime") {
    *profile = D8FlagProfile::kRuntime;
    return true;
  }
  return false;
}

const char* D8FlagProfileName(D8FlagProfile profile) {
  switch (profile) {
    case D8FlagProfile::kMaglev:
      return "maglev";
    case D8FlagProfile::kTurbolev:
      return "turbolev";
    case D8FlagProfile::kFuzzilli:
      return "fuzzilli";
    case D8FlagProfile::kFuzzilliWasm:
      return "fuzzilli-wasm";
    case D8FlagProfile::kRuntime:
      return "runtime";
  }
  return "maglev";
}

std::vector<std::string> RandomD8Flags(
    Random& rng, D8FlagProfile profile,
    const std::vector<std::string>& fixed_args) {
  std::vector<std::string> flags;
  if (profile == D8FlagProfile::kFuzzilli ||
      profile == D8FlagProfile::kFuzzilliWasm) {
    DrawFuzzilliD8Flags(rng, fixed_args,
                        profile == D8FlagProfile::kFuzzilliWasm, &flags);
    SanitizeFuzzilliFixedClashes(&flags, fixed_args);
    return flags;
  }
  auto maybe = [&](const Tunable& probability, const char* flag) {
    if (rng.Chance(probability())) {
      flags.emplace_back(flag);
      return true;
    }
    return false;
  };
  // Adds FLAG=N, with N from min to max (both included).
  auto add_value = [&](const char* flag, int64_t min, int64_t max) {
    flags.push_back(std::string(flag) + "=" +
                    std::to_string(rng.Range(min, max)));
  };
  auto maybe_value = [&](const Tunable& probability, const char* flag,
                         const Tunable& min, const Tunable& max) {
    if (!rng.Chance(probability())) {
      return false;
    }
    add_value(flag, min.AsInt(), max.AsInt());
    return true;
  };
  // kRuntime draws no flag that only acts on Sparkplug, Maglev, Turbofan or
  // Turbolev, and no CPU feature flag.
  const bool jit = profile != D8FlagProfile::kRuntime;
  const bool single_threaded =
      maybe(kFlagsSingleThreaded, "--single-threaded") ||
      ContainsFlag(fixed_args, "--single-threaded");
  const bool assert_types =
      (jit && maybe(kFlagsAssertTypes, "--assert-types")) ||
      ContainsFlag(fixed_args, "--assert-types");
  maybe(kFlagsNoLazyFeedbackAllocation, "--no-lazy-feedback-allocation");
  if (jit) {
    maybe(kFlagsNoBaselineBatchCompilation, "--no-baseline-batch-compilation");
  }
  maybe(kFlagsPrivateFieldBytecodes, "--private-field-bytecodes");
  if (profile == D8FlagProfile::kTurbolev) {
    maybe(kFlagsTurboVerify, "--turbo-verify");
    maybe(kFlagsTurboVerifyAllocation, "--turbo-verify-allocation");
    // Turbolev builds its graph with the Maglev graph builder, so the builtin
    // reducers of Maglev apply to it too.
    maybe(kFlagsTurbolevDisableBuiltinReducers,
          "--maglev-disable-builtin-reducers");
    const bool small_size_zero = maybe(kFlagsTurbolevSmallInlinedSizeZero,
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
    maybe(kFlagsOptimizeMaglevToTurbofan,
          "--optimize-maglev-optimizes-to-turbofan");
    // --single-threaded and --assert-types turn off concurrent compilation. V8
    // also resets flags with a warning when concurrent inlining is on.
    if (!single_threaded && !assert_types) {
      maybe(kFlagsStressConcurrentInlining, "--stress-concurrent-inlining");
      maybe(kFlagsStressConcurrentInliningAttachCode,
            "--stress-concurrent-inlining-attach-code");
    }
    maybe_value(kFlagsRecompilationQueueLength,
                "--concurrent-recompilation-queue-length",
                kFlagsRecompilationQueueLengthMin,
                kFlagsRecompilationQueueLengthMax);
    maybe_value(kFlagsRecompilationDelay, "--concurrent-recompilation-delay",
                kFlagsRecompilationDelayMin, kFlagsRecompilationDelayMax);
    // The inlining limits of the bytecode budget fuzzer of V8. The cumulative
    // limit is N+M, where N is the limit for one function.
    if (rng.Chance(kFlagsTurbolevInlinedSize())) {
      const int64_t size = rng.Range(kFlagsTurbolevInlinedSizeMin.AsInt(),
                                     kFlagsTurbolevInlinedSizeMax.AsInt());
      flags.push_back("--max-inlined-bytecode-size=" + std::to_string(size));
      if (rng.Chance(kFlagsTurbolevInlinedSizeCumulative())) {
        add_value("--max-inlined-bytecode-size-cumulative", size,
                  size + kFlagsTurbolevInlinedSizeCumulativeMax.AsInt());
      }
    }
    if (!small_size_zero) {
      maybe_value(
          kFlagsTurbolevSmallInlinedSize, "--max-inlined-bytecode-size-small",
          kFlagsTurbolevSmallInlinedSizeMin, kFlagsTurbolevSmallInlinedSizeMax);
    }
    maybe_value(kFlagsTurbolevHeapNumInlinedSize,
                "--max-inlined-bytecode-size-small-with-heapnum-in-out",
                kFlagsTurbolevHeapNumInlinedSizeMin,
                kFlagsTurbolevHeapNumInlinedSizeMax);
    maybe(kFlagsNoMaglevRangeAnalysis, "--no-maglev-range-analysis");
    maybe(kFlagsNoStringConcatEscapeAnalysis,
          "--no-turboshaft-string-concat-escape-analysis");
    maybe(kFlagsInstructionScheduling, "--turbo-instruction-scheduling");
    maybe(kFlagsStressInstructionScheduling,
          "--turbo-stress-instruction-scheduling");
    maybe(kFlagsNoTurboMoveOptimization, "--no-turbo-move-optimization");
    maybe(kFlagsNoTurboJt, "--no-turbo-jt");
    maybe(kFlagsNoTurboStoreElimination, "--no-turbo-store-elimination");
    maybe(kFlagsNoTurboLoopRotation, "--no-turbo-loop-rotation");
    maybe(kFlagsNoTurboAllocationFolding, "--no-turbo-allocation-folding");
    maybe(kFlagsTurboshaftVerifyLoadElimination,
          "--turboshaft-verify-load-elimination");
    maybe(kFlagsTurboshaftVerifyTaggedness,
          "--turboshaft-verify-load-store-taggedness");
  } else if (profile == D8FlagProfile::kMaglev) {
    maybe(kFlagsMaglevFuture, "--maglev-future");
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
    maybe(kFlagsOptimizeToMaglev,
          "--optimize-on-next-call-optimizes-to-maglev");
    // Like Fuzzilli, draw --maglev-as-top-tier only after --no-turbofan.
    if (ContainsFlag(fixed_args, "--no-turbofan")) {
      maybe(kFlagsMaglevAsTopTier, "--maglev-as-top-tier");
    }
    maybe(kFlagsMaglevInlineApiCalls, "--maglev-inline-api-calls");
  }

  // The flags of all profiles. Turbolev also reads the Maglev assertions in
  // its graph builder and in Turboshaft.
  if (jit) {
    maybe(kFlagsMaglevAssert, "--maglev-assert");
    maybe(kFlagsMaglevAssertTypes, "--maglev-assert-types");
    maybe(kFlagsMaglevAssertStackSize, "--maglev-assert-stack-size");
  }
  maybe(kFlagsStressIc, "--stress-ic");
  maybe(kFlagsVerifyHeap, "--verify-heap");
  maybe(kFlagsFuture, "--future");
  bool no_sparkplug = ContainsFlag(fixed_args, "--no-sparkplug");
  if (jit) {
    maybe(kFlagsJitFuzzing, "--jit-fuzzing");
    no_sparkplug |= maybe(kFlagsNoSparkplug, "--no-sparkplug");
  }
  maybe(kFlagsSharedStrings, "--shared-strings");
  maybe(kFlagsProtoAssignSeqOptCountOne, "--proto-assign-seq-opt-count=1");
  const double verify_roll = rng.Double();
  if (verify_roll < kFlagsVerifyBytecodeFull()) {
    flags.emplace_back("--verify-bytecode-full");
  } else if (verify_roll <
             kFlagsVerifyBytecodeFull() + kFlagsVerifyBytecodeLight()) {
    flags.emplace_back("--verify-bytecode-light");
  }
  maybe(kFlagsStressLazySourcePositions, "--stress-lazy-source-positions");
  maybe(kFlagsStressLazy, "--stress-lazy");
  maybe(kFlagsNoLazy, "--no-lazy");
  maybe(kFlagsNoLazyEval, "--no-lazy-eval");
  if (jit) {
    maybe(kFlagsAlwaysOsr, "--always-osr");
    if (!no_sparkplug) {
      maybe(kFlagsAlwaysSparkplug, "--always-sparkplug");
    }
  }
  maybe(kFlagsForceSlowPath, "--force-slow-path");
  if (jit) {
    maybe(kFlagsStressGcDuringCompilation, "--stress-gc-during-compilation");
  }
  maybe_value(kFlagsFeedbackAllocationCount,
              "--invocation-count-for-feedback-allocation",
              kFlagsFeedbackAllocationCountMin,
              kFlagsFeedbackAllocationCountMax);
  if (jit && rng.Chance(kFlagsDeoptEveryNTimes())) {
    const int n =
        kDeoptEveryNTimesValues[rng.Index(std::size(kDeoptEveryNTimesValues))];
    flags.push_back("--deopt-every-n-times=" + std::to_string(n));
  }
  maybe(kFlagsNoUseIc, "--no-use-ic");
  // Only the optimizing compilers read --disable-loop-stack-checks.
  if (jit) {
    maybe(kFlagsDisableLoopStackChecks, "--disable-loop-stack-checks");
  }
  maybe_value(kFlagsStackSize, "--stack-size", kFlagsStackSizeMin,
              kFlagsStackSizeMax);
  if (jit) {
    maybe(kFlagsNoEnableSse3, "--no-enable-sse3");
    maybe(kFlagsNoEnableSsse3, "--no-enable-ssse3");
    maybe(kFlagsNoEnableSse41, "--no-enable-sse4-1");
    maybe(kFlagsNoEnableSse42, "--no-enable-sse4-2");
    maybe(kFlagsNoEnableAvx, "--no-enable-avx");
    maybe(kFlagsNoEnableAvx2, "--no-enable-avx2");
    maybe(kFlagsNoEnableSahf, "--no-enable-sahf");
    maybe(kFlagsNoEnableFma3, "--no-enable-fma3");
    maybe(kFlagsNoEnableBmi1, "--no-enable-bmi1");
    maybe(kFlagsNoEnableBmi2, "--no-enable-bmi2");
    maybe(kFlagsNoEnableLzcnt, "--no-enable-lzcnt");
    maybe(kFlagsNoEnablePopcnt, "--no-enable-popcnt");
    maybe(kFlagsNoMaglevInlining, "--no-maglev-inlining");
    maybe(kFlagsNoMaglevPolyCalls, "--no-maglev-poly-calls");
    maybe(kFlagsNoMaglevEscapeAnalysis, "--no-maglev-escape-analysis");
    maybe(kFlagsNoMaglevOptimisticPeeledLoops,
          "--no-maglev-optimistic-peeled-loops");
    maybe(kFlagsNoMaglevTruncation, "--no-maglev-truncation");
    maybe(kFlagsNoMaglevCse, "--no-maglev-cse");
  }
  maybe(kFlagsStressCompaction, "--stress-compaction");
  maybe(kFlagsStressCompactionRandom, "--stress-compaction-random");
  maybe_value(kFlagsStressMarking, "--stress-marking", kFlagsStressMarkingMin,
              kFlagsStressMarkingMax);
  maybe_value(kFlagsStressScavenge, "--stress-scavenge",
              kFlagsStressScavengeMin, kFlagsStressScavengeMax);
  maybe_value(kFlagsGcInterval, "--gc-interval", kFlagsGcIntervalMin,
              kFlagsGcIntervalMax);
  maybe_value(kFlagsRandomGcInterval, "--random-gc-interval",
              kFlagsRandomGcIntervalMin, kFlagsRandomGcIntervalMax);
  maybe(kFlagsStressFlushCode, "--stress-flush-code");
  maybe(kFlagsNoFlushBytecode, "--no-flush-bytecode");
  maybe(kFlagsMinorMs, "--minor-ms");
  if (maybe(kFlagsScavengerChaosMode, "--scavenger-chaos-mode")) {
    add_value("--scavenger-chaos-mode-threshold",
              kFlagsScavengerChaosThresholdMin.AsInt(),
              kFlagsScavengerChaosThresholdMax.AsInt());
  }
  maybe(kFlagsStressDescriptorArrayTrimming,
        "--stress-descriptor-array-trimming");
  maybe(kFlagsStressObjectPinning,
        "--stress-scavenger-conservative-object-pinning-random");
  maybe(kFlagsPreciseObjectPinning, "--precise-object-pinning");
  maybe(kFlagsEfficiencyMode, "--efficiency-mode");
  maybe(kFlagsBatterySaverMode, "--battery-saver-mode");
  maybe(kFlagsNoShortBuiltinCalls, "--no-short-builtin-calls");
  maybe(kFlagsStressBackgroundCompile, "--stress-background-compile");
  maybe(kFlagsLazyNewSpaceShrinking, "--lazy-new-space-shrinking");
  SanitizeD8Flags(&flags, fixed_args);
  return flags;
}

std::vector<std::string> CombineD8Flags(
    const std::vector<std::string>& base, const std::vector<std::string>& extra,
    Random& rng, const std::vector<std::string>& fixed_args) {
  std::vector<std::string> combined;
  for (const std::string& flag : base) {
    if (rng.Chance(kFlagsTargetKeepChance())) {
      combined.push_back(flag);
    }
  }
  for (const std::string& flag : extra) {
    combined.push_back(flag);
  }
  SanitizeD8Flags(&combined, fixed_args);
  return combined;
}

double TargetGuidedChance() { return kFlagsTargetGuidedChance(); }
int TargetGuidedWarmup() { return kFlagsTargetGuidedWarmup.AsInt(); }

double EffectiveTargetGuidedChance(uint64_t completed_children,
                                   uint64_t hit_children) {
  const double base = kFlagsTargetGuidedChance();
  const uint64_t warmup = static_cast<uint64_t>(
      std::max<int64_t>(0, kFlagsTargetGuidedWarmup.AsInt()));
  if (completed_children == 0 || completed_children < warmup) {
    return base;
  }
  const double hit_rate =
      std::clamp(static_cast<double>(hit_children) /
                     static_cast<double>(completed_children),
                 0.0, 1.0);
  return base * (1.0 - hit_rate);
}

double TargetMutateChance() { return kFlagsTargetMutateChance(); }
int TargetTournament() { return kFlagsTargetTournament.AsInt(); }
int TargetProbeExecs() { return kFlagsTargetProbeExecs.AsInt(); }

double ImportedFlagsChance() { return kFlagsImportedChance(); }
double FlagAffinityChance() { return kFlagsAffinityChance(); }
int SeedAffinityTries() { return kFlagsSeedAffinityTries.AsInt(); }

std::string_view D8FlagName(std::string_view flag) { return FlagPrefix(flag); }

bool IsAllowedTestFlag(std::string_view flag) {
  // A bare `--` stops the flag parser of V8.
  if (flag.size() <= 2 || !flag.starts_with("--") || flag.ends_with(".mjs")) {
    return false;
  }
  const std::string name = NormalizedName(flag);
  if (IsDeniedTestFlagName(name)) {
    return false;
  }
  // V8 also reads `--no-foo` and `--nofoo` as the negation of `--foo`.
  if (name.starts_with("--no-")) {
    return !IsDeniedTestFlagName("--" + name.substr(5));
  }
  if (name.starts_with("--no")) {
    return !IsDeniedTestFlagName("--" + name.substr(4));
  }
  return true;
}

std::vector<std::string> SanitizeTestFlags(const std::vector<std::string>& raw,
                                           std::vector<std::string>* rejected) {
  std::vector<std::string> flags;
  for (const std::string& flag : raw) {
    if (!IsAllowedTestFlag(flag)) {
      if (rejected != nullptr) {
        rejected->push_back(flag);
      }
      continue;
    }
    flags.push_back(NormalizedFlag(flag));
  }
  SanitizeD8Flags(&flags);
  return flags;
}

void RemoveFixedD8Flags(std::vector<std::string>* flags,
                        const std::vector<std::string>& fixed_args) {
  if (fixed_args.empty()) {
    return;
  }
  flags->erase(std::remove_if(flags->begin(), flags->end(),
                              [&fixed_args](const std::string& flag) {
                                return HasFlagKey(fixed_args, FlagKey(flag));
                              }),
               flags->end());
}

std::vector<std::string> MergeProgramFlags(
    const std::vector<std::string>& first,
    const std::vector<std::string>& second, size_t max_flags) {
  std::vector<std::string> merged;
  for (const std::vector<std::string>* list : {&first, &second}) {
    for (const std::string& flag : *list) {
      if (merged.size() >= max_flags) {
        return merged;
      }
      if (!HasFlagKey(merged, FlagKey(flag))) {
        merged.push_back(flag);
      }
    }
  }
  return merged;
}

bool SharesD8Flag(const std::vector<std::string>& a,
                  const std::vector<std::string>& b) {
  for (const std::string& x : a) {
    const std::string_view name = FlagPrefix(x);
    for (const std::string& y : b) {
      if (FlagPrefix(y) == name) {
        return true;
      }
    }
  }
  return false;
}

bool ParseD8Trials(std::string_view json, std::vector<D8Trial>* trials,
                   std::string* error, size_t* dropped) {
  JsonValue root;
  if (!JsonValue::Parse(json, &root, error)) {
    return false;
  }
  if (!root.is_array()) {
    *error = "the trials file is not a JSON list";
    return false;
  }
  for (const JsonValue& entry : root.as_array()) {
    if (!entry.is_object()) {
      *error = "a trial is not a JSON object";
      return false;
    }
    if (entry.GetString("app_name", "") != "d8") {
      continue;
    }
    D8Trial trial;
    trial.probability =
        std::clamp(entry.GetNumber("probability", 0.0), 0.0, 1.0);
    const std::vector<std::string> args =
        SplitWhitespace(entry.GetString("app_args", ""));
    if (args.empty() || trial.probability <= 0) {
      continue;
    }
    if (!std::all_of(args.begin(), args.end(), [](const std::string& arg) {
          return IsAllowedTestFlag(arg);
        })) {
      if (dropped != nullptr) {
        (*dropped)++;
      }
      continue;
    }
    for (const std::string& arg : args) {
      trial.args.push_back(NormalizedFlag(arg));
    }
    for (const JsonValue& flag : entry.Get("contradicts").as_array()) {
      if (flag.is_string()) {
        trial.contradicts.push_back(NormalizedName(flag.as_string()));
      }
    }
    trials->push_back(std::move(trial));
  }
  return true;
}

void AddD8Trials(Random& rng, const std::vector<D8Trial>& trials,
                 const std::vector<std::string>& fixed_args,
                 std::vector<std::string>* flags) {
  std::vector<size_t> order(trials.size());
  std::iota(order.begin(), order.end(), 0);
  for (size_t i = order.size(); i > 1; i--) {
    std::swap(order[i - 1], order[rng.Index(i)]);
  }
  // The flag names that the drawn trials contradict.
  std::vector<std::string> blocked;
  for (size_t index : order) {
    const D8Trial& trial = trials[index];
    if (!rng.Chance(trial.probability)) {
      continue;
    }
    const bool contradicts = std::any_of(
        trial.contradicts.begin(), trial.contradicts.end(),
        [&](const std::string& name) {
          return HasFlagName(*flags, name) || HasFlagName(fixed_args, name);
        });
    const bool clashes = std::any_of(
        trial.args.begin(), trial.args.end(), [&](const std::string& arg) {
          const std::string key = FlagKey(arg);
          return HasFlagKey(*flags, key) || HasFlagKey(fixed_args, key) ||
                 std::find(blocked.begin(), blocked.end(),
                           NormalizedName(arg)) != blocked.end();
        });
    if (contradicts || clashes) {
      continue;
    }
    flags->insert(flags->end(), trial.args.begin(), trial.args.end());
    blocked.insert(blocked.end(), trial.contradicts.begin(),
                   trial.contradicts.end());
  }
  SanitizeD8Flags(flags, fixed_args);
}

void FlagSetPool::Record(const std::vector<std::string>& flags, double score) {
  if (flags.empty() || score <= 0) {
    return;
  }
  std::scoped_lock<std::mutex> lock(mutex_);
  for (Entry& entry : entries_) {
    if (entry.flags == flags) {
      entry.score = std::max(entry.score, score);
      return;
    }
  }
  if (entries_.size() < capacity_) {
    entries_.push_back(Entry{flags, score});
    size_.store(entries_.size(), std::memory_order_release);
    return;
  }
  auto worst = std::min_element(
      entries_.begin(), entries_.end(),
      [](const Entry& a, const Entry& b) { return a.score < b.score; });
  if (worst != entries_.end() && score > worst->score) {
    *worst = Entry{flags, score};
  }
}

void FlagSetPool::Decay(double factor) {
  std::scoped_lock<std::mutex> lock(mutex_);
  for (Entry& entry : entries_) {
    entry.score *= factor;
  }
}

bool FlagSetPool::Pick(Random& rng, int tournament,
                       std::vector<std::string>* flags) const {
  std::scoped_lock<std::mutex> lock(mutex_);
  if (entries_.empty()) {
    return false;
  }
  size_t best = rng.Index(entries_.size());
  for (int i = 1; i < tournament; i++) {
    const size_t candidate = rng.Index(entries_.size());
    if (entries_[candidate].score > entries_[best].score) {
      best = candidate;
    }
  }
  *flags = entries_[best].flags;
  return true;
}

bool D8FlagBlacklist::RecordSpawn(const std::vector<std::string>& flags,
                                  bool started) {
  std::vector<std::string> names;
  for (const std::string& flag : flags) {
    std::string name = NormalizedName(flag);
    if (std::find(names.begin(), names.end(), name) == names.end()) {
      names.push_back(std::move(name));
    }
  }
  if (names.empty()) {
    return false;
  }
  std::scoped_lock<std::mutex> lock(mutex_);
  if (started) {
    total_started_++;
    for (const std::string& name : names) {
      counts_[name].started++;
    }
    return false;
  }
  failed_spawns_.fetch_add(1, std::memory_order_relaxed);
  std::vector<std::string> never_started;
  for (const std::string& name : names) {
    Counts& counts = counts_[name];
    counts.failed++;
    if (counts.started == 0) {
      never_started.push_back(name);
    }
  }
  std::vector<std::string> block;
  if (names.size() == 1 ||
      (never_started.size() == 1 && total_started_ >= kBlacklistMinStarts)) {
    block = never_started;
  } else {
    // The flag that is in the most failed starts. A tie blocks no flag,
    // because a flag that is often drawn with the bad flag can tie.
    uint32_t most = 0;
    uint32_t second = 0;
    const std::string* top = nullptr;
    for (const std::string& name : never_started) {
      const uint32_t failed = counts_[name].failed;
      if (failed > most) {
        second = most;
        most = failed;
        top = &name;
      } else if (failed > second) {
        second = failed;
      }
    }
    if (top != nullptr && most >= kBlacklistRepeatedFailures && most > second) {
      block.push_back(*top);
    }
  }
  bool blocked_new = false;
  for (std::string& name : block) {
    blocked_new |= blocked_.insert(std::move(name)).second;
  }
  if (!blocked_.empty()) {
    any_blocked_.store(true, std::memory_order_release);
  }
  return blocked_new;
}

bool D8FlagBlacklist::IsBlocked(std::string_view flag) const {
  if (!any_blocked_.load(std::memory_order_acquire)) {
    return false;
  }
  std::scoped_lock<std::mutex> lock(mutex_);
  return blocked_.contains(NormalizedName(flag));
}

void D8FlagBlacklist::Filter(std::vector<std::string>* flags) const {
  if (!any_blocked_.load(std::memory_order_acquire)) {
    return;
  }
  std::scoped_lock<std::mutex> lock(mutex_);
  flags->erase(std::remove_if(flags->begin(), flags->end(),
                              [this](const std::string& flag) {
                                return blocked_.contains(NormalizedName(flag));
                              }),
               flags->end());
}

std::vector<std::string> D8FlagBlacklist::Blocked() const {
  std::scoped_lock<std::mutex> lock(mutex_);
  return std::vector<std::string>(blocked_.begin(), blocked_.end());
}

}  // namespace redline
