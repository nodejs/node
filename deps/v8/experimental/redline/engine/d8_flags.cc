// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/d8_flags.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

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

// The values of --deopt-every-n-times=N in the V8 profile of Fuzzilli.
constexpr int kDeoptEveryNTimesValues[] = {100,  250,  500,  1000,
                                           2500, 5000, 10000};

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

std::string_view FlagPrefix(std::string_view flag) {
  const size_t eq = flag.find('=');
  return eq == std::string_view::npos ? flag : flag.substr(0, eq);
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

}  // namespace

void SanitizeD8Flags(std::vector<std::string>* flags,
                     const std::vector<std::string>& fixed_args) {
  std::vector<std::string> deduped;
  deduped.reserve(flags->size());
  for (const std::string& flag : *flags) {
    const std::string_view prefix = FlagPrefix(flag);
    const bool seen = std::any_of(deduped.begin(), deduped.end(),
                                  [prefix](const std::string& existing) {
                                    return FlagPrefix(existing) == prefix;
                                  });
    if (!seen) {
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
  return false;
}

const char* D8FlagProfileName(D8FlagProfile profile) {
  return profile == D8FlagProfile::kTurbolev ? "turbolev" : "maglev";
}

std::vector<std::string> RandomD8Flags(
    Random& rng, D8FlagProfile profile,
    const std::vector<std::string>& fixed_args) {
  std::vector<std::string> flags;
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
  const bool single_threaded =
      maybe(kFlagsSingleThreaded, "--single-threaded") ||
      ContainsFlag(fixed_args, "--single-threaded");
  const bool assert_types = maybe(kFlagsAssertTypes, "--assert-types") ||
                            ContainsFlag(fixed_args, "--assert-types");
  maybe(kFlagsNoLazyFeedbackAllocation, "--no-lazy-feedback-allocation");
  maybe(kFlagsNoBaselineBatchCompilation, "--no-baseline-batch-compilation");
  maybe(kFlagsPrivateFieldBytecodes, "--private-field-bytecodes");
  if (profile == D8FlagProfile::kTurbolev) {
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
  } else {
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

  // The flags of both profiles. Turbolev also reads the Maglev assertions in
  // its graph builder and in Turboshaft.
  maybe(kFlagsMaglevAssert, "--maglev-assert");
  maybe(kFlagsMaglevAssertTypes, "--maglev-assert-types");
  maybe(kFlagsStressIc, "--stress-ic");
  maybe(kFlagsVerifyHeap, "--verify-heap");
  maybe(kFlagsFuture, "--future");
  maybe(kFlagsJitFuzzing, "--jit-fuzzing");
  const bool no_sparkplug = maybe(kFlagsNoSparkplug, "--no-sparkplug") ||
                            ContainsFlag(fixed_args, "--no-sparkplug");
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
  maybe(kFlagsAlwaysOsr, "--always-osr");
  if (!no_sparkplug) {
    maybe(kFlagsAlwaysSparkplug, "--always-sparkplug");
  }
  maybe(kFlagsForceSlowPath, "--force-slow-path");
  maybe(kFlagsStressGcDuringCompilation, "--stress-gc-during-compilation");
  maybe_value(kFlagsFeedbackAllocationCount,
              "--invocation-count-for-feedback-allocation",
              kFlagsFeedbackAllocationCountMin,
              kFlagsFeedbackAllocationCountMax);
  if (rng.Chance(kFlagsDeoptEveryNTimes())) {
    const int n =
        kDeoptEveryNTimesValues[rng.Index(std::size(kDeoptEveryNTimesValues))];
    flags.push_back("--deopt-every-n-times=" + std::to_string(n));
  }
  maybe(kFlagsNoUseIc, "--no-use-ic");
  maybe(kFlagsDisableLoopStackChecks, "--disable-loop-stack-checks");
  maybe_value(kFlagsStackSize, "--stack-size", kFlagsStackSizeMin,
              kFlagsStackSizeMax);
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

}  // namespace redline
