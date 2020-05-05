// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all of the flags.  It is separated into different section,
// for Debug, Release, Logging and Profiling, etc.  To add a new flag, find the
// correct section, and use one of the DEFINE_ macros, without a trailing ';'.
//
// This include does not have a guard, because it is a template-style include,
// which can be included multiple times in different modes.  It expects to have
// a mode defined before it's included.  The modes are FLAG_MODE_... below:
//
// PRESUBMIT_INTENTIONALLY_MISSING_INCLUDE_GUARD

#define DEFINE_IMPLICATION(whenflag, thenflag) \
  DEFINE_VALUE_IMPLICATION(whenflag, thenflag, true)

#define DEFINE_NEG_IMPLICATION(whenflag, thenflag) \
  DEFINE_VALUE_IMPLICATION(whenflag, thenflag, false)

#define DEFINE_NEG_NEG_IMPLICATION(whenflag, thenflag) \
  DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, false)

// We want to declare the names of the variables for the header file.  Normally
// this will just be an extern declaration, but for a readonly flag we let the
// compiler make better optimizations by giving it the value.
#if defined(FLAG_MODE_DECLARE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  V8_EXPORT_PRIVATE extern ctype FLAG_##nam;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt) \
  static constexpr ctype FLAG_##nam = def;

// We want to supply the actual storage and value for the flag variable in the
// .cc file.  We only do this for writable flags.
#elif defined(FLAG_MODE_DEFINE)
#ifdef USING_V8_SHARED
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  V8_EXPORT_PRIVATE extern ctype FLAG_##nam;
#else
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  V8_EXPORT_PRIVATE ctype FLAG_##nam = def;
#endif

// We need to define all of our default values so that the Flag structure can
// access them by pointer.  These are just used internally inside of one .cc,
// for MODE_META, so there is no impact on the flags interface.
#elif defined(FLAG_MODE_DEFINE_DEFAULTS)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  static constexpr ctype FLAGDEFAULT_##nam = def;

// We want to write entries into our meta data table, for internal parsing and
// printing / etc in the flag parser code.  We only do this for writable flags.
#elif defined(FLAG_MODE_META)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  {Flag::TYPE_##ftype, #nam, &FLAG_##nam, &FLAGDEFAULT_##nam, cmt, false},
#define FLAG_ALIAS(ftype, ctype, alias, nam)                     \
  {Flag::TYPE_##ftype,  #alias, &FLAG_##nam, &FLAGDEFAULT_##nam, \
    "alias for --" #nam, false},

// We produce the code to set flags when it is implied by another flag.
#elif defined(FLAG_MODE_DEFINE_IMPLICATIONS)
#define DEFINE_VALUE_IMPLICATION(whenflag, thenflag, value) \
  if (FLAG_##whenflag) FLAG_##thenflag = value;

#define DEFINE_GENERIC_IMPLICATION(whenflag, statement) \
  if (FLAG_##whenflag) statement;

#define DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, value) \
  if (!FLAG_##whenflag) FLAG_##thenflag = value;

// We apply a generic macro to the flags.
#elif defined(FLAG_MODE_APPLY)

#define FLAG_FULL FLAG_MODE_APPLY

#else
#error No mode supplied when including flags.defs
#endif

// Dummy defines for modes where it is not relevant.
#ifndef FLAG_FULL
#define FLAG_FULL(ftype, ctype, nam, def, cmt)
#endif

#ifndef FLAG_READONLY
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)
#endif

#ifndef FLAG_ALIAS
#define FLAG_ALIAS(ftype, ctype, alias, nam)
#endif

#ifndef DEFINE_VALUE_IMPLICATION
#define DEFINE_VALUE_IMPLICATION(whenflag, thenflag, value)
#endif

#ifndef DEFINE_GENERIC_IMPLICATION
#define DEFINE_GENERIC_IMPLICATION(whenflag, statement)
#endif

#ifndef DEFINE_NEG_VALUE_IMPLICATION
#define DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, value)
#endif

#define COMMA ,

#ifdef FLAG_MODE_DECLARE

struct MaybeBoolFlag {
  static MaybeBoolFlag Create(bool has_value, bool value) {
    MaybeBoolFlag flag;
    flag.has_value = has_value;
    flag.value = value;
    return flag;
  }
  bool has_value;
  bool value;

  bool operator!=(const MaybeBoolFlag& other) const {
    return has_value != other.has_value || value != other.value;
  }
};
#endif

#ifdef DEBUG
#define DEBUG_BOOL true
#else
#define DEBUG_BOOL false
#endif

#ifdef V8_COMPRESS_POINTERS
#define COMPRESS_POINTERS_BOOL true
#else
#define COMPRESS_POINTERS_BOOL false
#endif

#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
#define ENABLE_CONTROL_FLOW_INTEGRITY_BOOL true
#else
#define ENABLE_CONTROL_FLOW_INTEGRITY_BOOL false
#endif

// Supported ARM configurations are:
//  "armv6":       ARMv6 + VFPv2
//  "armv7":       ARMv7 + VFPv3-D32 + NEON
//  "armv7+sudiv": ARMv7 + VFPv4-D32 + NEON + SUDIV
//  "armv8":       ARMv8 (including all of the above)
#if !defined(ARM_TEST_NO_FEATURE_PROBE) ||                            \
    (defined(CAN_USE_ARMV8_INSTRUCTIONS) &&                           \
     defined(CAN_USE_ARMV7_INSTRUCTIONS) && defined(CAN_USE_SUDIV) && \
     defined(CAN_USE_NEON) && defined(CAN_USE_VFP3_INSTRUCTIONS))
#define ARM_ARCH_DEFAULT "armv8"
#elif defined(CAN_USE_ARMV7_INSTRUCTIONS) && defined(CAN_USE_SUDIV) && \
    defined(CAN_USE_NEON) && defined(CAN_USE_VFP3_INSTRUCTIONS)
#define ARM_ARCH_DEFAULT "armv7+sudiv"
#elif defined(CAN_USE_ARMV7_INSTRUCTIONS) && defined(CAN_USE_NEON) && \
    defined(CAN_USE_VFP3_INSTRUCTIONS)
#define ARM_ARCH_DEFAULT "armv7"
#else
#define ARM_ARCH_DEFAULT "armv6"
#endif

#ifdef V8_OS_WIN
#define ENABLE_LOG_COLOUR false
#else
#define ENABLE_LOG_COLOUR true
#endif

#define DEFINE_BOOL(nam, def, cmt) FLAG(BOOL, bool, nam, def, cmt)
#define DEFINE_BOOL_READONLY(nam, def, cmt) \
  FLAG_READONLY(BOOL, bool, nam, def, cmt)
#define DEFINE_MAYBE_BOOL(nam, cmt) \
  FLAG(MAYBE_BOOL, MaybeBoolFlag, nam, {false COMMA false}, cmt)
#define DEFINE_INT(nam, def, cmt) FLAG(INT, int, nam, def, cmt)
#define DEFINE_UINT(nam, def, cmt) FLAG(UINT, unsigned int, nam, def, cmt)
#define DEFINE_UINT_READONLY(nam, def, cmt) \
  FLAG_READONLY(UINT, unsigned int, nam, def, cmt)
#define DEFINE_UINT64(nam, def, cmt) FLAG(UINT64, uint64_t, nam, def, cmt)
#define DEFINE_FLOAT(nam, def, cmt) FLAG(FLOAT, double, nam, def, cmt)
#define DEFINE_SIZE_T(nam, def, cmt) FLAG(SIZE_T, size_t, nam, def, cmt)
#define DEFINE_STRING(nam, def, cmt) FLAG(STRING, const char*, nam, def, cmt)
#define DEFINE_ALIAS_BOOL(alias, nam) FLAG_ALIAS(BOOL, bool, alias, nam)
#define DEFINE_ALIAS_INT(alias, nam) FLAG_ALIAS(INT, int, alias, nam)
#define DEFINE_ALIAS_FLOAT(alias, nam) FLAG_ALIAS(FLOAT, double, alias, nam)
#define DEFINE_ALIAS_SIZE_T(alias, nam) FLAG_ALIAS(SIZE_T, size_t, alias, nam)
#define DEFINE_ALIAS_STRING(alias, nam) \
  FLAG_ALIAS(STRING, const char*, alias, nam)

#ifdef DEBUG
#define DEFINE_DEBUG_BOOL DEFINE_BOOL
#else
#define DEFINE_DEBUG_BOOL DEFINE_BOOL_READONLY
#endif

//
// Flags in all modes.
//
#define FLAG FLAG_FULL

// Flags for language modes and experimental language features.
DEFINE_BOOL(use_strict, false, "enforce strict mode")

DEFINE_BOOL(es_staging, false,
            "enable test-worthy harmony features (for internal use only)")
DEFINE_BOOL(harmony, false, "enable all completed harmony features")
DEFINE_BOOL(harmony_shipping, true, "enable all shipped harmony features")
DEFINE_IMPLICATION(es_staging, harmony)
// Enabling import.meta requires to also enable import()
DEFINE_IMPLICATION(harmony_import_meta, harmony_dynamic_import)

// Update bootstrapper.cc whenever adding a new feature flag.

// Features that are still work in progress (behind individual flags).
#define HARMONY_INPROGRESS_BASE(V)                                    \
  V(harmony_string_replaceall, "harmony String.prototype.replaceAll") \
  V(harmony_regexp_sequence, "RegExp Unicode sequence properties")    \
  V(harmony_weak_refs, "harmony weak references")                     \
  V(harmony_regexp_match_indices, "harmony regexp match indices")     \
  V(harmony_top_level_await, "harmony top level await")

#ifdef V8_INTL_SUPPORT
#define HARMONY_INPROGRESS(V)                       \
  HARMONY_INPROGRESS_BASE(V)                        \
  V(harmony_intl_displaynames_date_types, "Intl.DisplayNames date types")
#else
#define HARMONY_INPROGRESS(V) HARMONY_INPROGRESS_BASE(V)
#endif

// Features that are complete (but still behind --harmony/es-staging flag).
#define HARMONY_STAGED_BASE(V)                                     \
  V(harmony_private_methods, "harmony private methods in class literals")

#ifdef V8_INTL_SUPPORT
#define HARMONY_STAGED(V)                                  \
  HARMONY_STAGED_BASE(V)                                   \
  V(harmony_intl_dateformat_day_period,                    \
    "Add dayPeriod option to DateTimeFormat")              \
  V(harmony_intl_dateformat_fractional_second_digits,      \
    "Add fractionalSecondDigits option to DateTimeFormat") \
  V(harmony_intl_segmenter, "Intl.Segmenter")
#else
#define HARMONY_STAGED(V) HARMONY_STAGED_BASE(V)
#endif

// Features that are shipping (turned on by default, but internal flag remains).
#define HARMONY_SHIPPING_BASE(V)                               \
  V(harmony_namespace_exports,                                 \
    "harmony namespace exports (export * as foo from 'bar')")  \
  V(harmony_sharedarraybuffer, "harmony sharedarraybuffer")    \
  V(harmony_import_meta, "harmony import.meta property")       \
  V(harmony_dynamic_import, "harmony dynamic import")          \
  V(harmony_promise_all_settled, "harmony Promise.allSettled") \
  V(harmony_nullish, "harmony nullish operator")               \
  V(harmony_optional_chaining, "harmony optional chaining syntax")

#ifdef V8_INTL_SUPPORT
#define HARMONY_SHIPPING(V)                               \
  HARMONY_SHIPPING_BASE(V)                                \
  V(harmony_intl_add_calendar_numbering_system,           \
    "Add calendar and numberingSystem to DateTimeFormat") \
  V(harmony_intl_displaynames, "Intl.DisplayNames")       \
  V(harmony_intl_other_calendars, "DateTimeFormat other calendars")
#else
#define HARMONY_SHIPPING(V) HARMONY_SHIPPING_BASE(V)
#endif

// Once a shipping feature has proved stable in the wild, it will be dropped
// from HARMONY_SHIPPING, all occurrences of the FLAG_ variable are removed,
// and associated tests are moved from the harmony directory to the appropriate
// esN directory.

#define FLAG_INPROGRESS_FEATURES(id, description) \
  DEFINE_BOOL(id, false, "enable " #description " (in progress)")
HARMONY_INPROGRESS(FLAG_INPROGRESS_FEATURES)
#undef FLAG_INPROGRESS_FEATURES

#define FLAG_STAGED_FEATURES(id, description)    \
  DEFINE_BOOL(id, false, "enable " #description) \
  DEFINE_IMPLICATION(harmony, id)
HARMONY_STAGED(FLAG_STAGED_FEATURES)
#undef FLAG_STAGED_FEATURES

#define FLAG_SHIPPING_FEATURES(id, description) \
  DEFINE_BOOL(id, true, "enable " #description) \
  DEFINE_NEG_NEG_IMPLICATION(harmony_shipping, id)
HARMONY_SHIPPING(FLAG_SHIPPING_FEATURES)
#undef FLAG_SHIPPING_FEATURES

#ifdef V8_INTL_SUPPORT
DEFINE_BOOL(icu_timezone_data, true, "get information about timezones from ICU")
#endif

#ifdef V8_ENABLE_DOUBLE_CONST_STORE_CHECK
#define V8_ENABLE_DOUBLE_CONST_STORE_CHECK_BOOL true
#else
#define V8_ENABLE_DOUBLE_CONST_STORE_CHECK_BOOL false
#endif

#ifdef V8_LITE_MODE
#define V8_LITE_BOOL true
#else
#define V8_LITE_BOOL false
#endif

#ifdef V8_ENABLE_LAZY_SOURCE_POSITIONS
#define V8_LAZY_SOURCE_POSITIONS_BOOL true
#else
#define V8_LAZY_SOURCE_POSITIONS_BOOL false
#endif

#ifdef V8_SHARED_RO_HEAP
#define V8_SHARED_RO_HEAP_BOOL true
#else
#define V8_SHARED_RO_HEAP_BOOL false
#endif

DEFINE_BOOL(lite_mode, V8_LITE_BOOL,
            "enables trade-off of performance for memory savings")

// Lite mode implies other flags to trade-off performance for memory.
DEFINE_IMPLICATION(lite_mode, jitless)
DEFINE_IMPLICATION(lite_mode, lazy_feedback_allocation)
DEFINE_IMPLICATION(lite_mode, optimize_for_size)

#ifdef V8_ENABLE_THIRD_PARTY_HEAP
#define V8_ENABLE_THIRD_PARTY_HEAP_BOOL true
#else
#define V8_ENABLE_THIRD_PARTY_HEAP_BOOL false
#endif

DEFINE_BOOL_READONLY(enable_third_party_heap, V8_ENABLE_THIRD_PARTY_HEAP_BOOL,
                     "Use third-party heap")

#ifdef V8_DISABLE_WRITE_BARRIERS
#define V8_DISABLE_WRITE_BARRIERS_BOOL true
#else
#define V8_DISABLE_WRITE_BARRIERS_BOOL false
#endif

DEFINE_BOOL_READONLY(disable_write_barriers, V8_DISABLE_WRITE_BARRIERS_BOOL,
                     "disable write barriers when GC is non-incremental "
                     "and heap contains single generation.")

// Disable incremental marking barriers
DEFINE_NEG_IMPLICATION(disable_write_barriers, incremental_marking)

#ifdef V8_ENABLE_SINGLE_GENERATION
#define V8_GENERATION_BOOL true
#else
#define V8_GENERATION_BOOL false
#endif

DEFINE_BOOL_READONLY(
    single_generation, V8_GENERATION_BOOL,
    "allocate all objects from young generation to old generation")

// Prevent inline allocation into new space
DEFINE_NEG_IMPLICATION(single_generation, inline_new)
DEFINE_NEG_IMPLICATION(single_generation, turbo_allocation_folding)

#ifdef V8_ENABLE_FUTURE
#define FUTURE_BOOL true
#else
#define FUTURE_BOOL false
#endif
DEFINE_BOOL(future, FUTURE_BOOL,
            "Implies all staged features that we want to ship in the "
            "not-too-far future")

DEFINE_IMPLICATION(future, write_protect_code_memory)

DEFINE_BOOL(assert_types, false,
            "generate runtime type assertions to test the typer")

// Flags for experimental implementation features.
DEFINE_BOOL(allocation_site_pretenuring, true,
            "pretenure with allocation sites")
DEFINE_BOOL(page_promotion, true, "promote pages based on utilization")
DEFINE_BOOL(always_promote_young_mc, true,
            "always promote young objects during mark-compact")
DEFINE_INT(page_promotion_threshold, 70,
           "min percentage of live bytes on a page to enable fast evacuation")
DEFINE_BOOL(trace_pretenuring, false,
            "trace pretenuring decisions of HAllocate instructions")
DEFINE_BOOL(trace_pretenuring_statistics, false,
            "trace allocation site pretenuring statistics")
DEFINE_BOOL(track_fields, true, "track fields with only smi values")
DEFINE_BOOL(track_double_fields, true, "track fields with double values")
DEFINE_BOOL(track_heap_object_fields, true, "track fields with heap values")
DEFINE_BOOL(track_computed_fields, true, "track computed boilerplate fields")
DEFINE_IMPLICATION(track_double_fields, track_fields)
DEFINE_IMPLICATION(track_heap_object_fields, track_fields)
DEFINE_IMPLICATION(track_computed_fields, track_fields)
DEFINE_BOOL(track_field_types, true, "track field types")
DEFINE_IMPLICATION(track_field_types, track_fields)
DEFINE_IMPLICATION(track_field_types, track_heap_object_fields)
DEFINE_BOOL(trace_block_coverage, false,
            "trace collected block coverage information")
DEFINE_BOOL(trace_protector_invalidation, false,
            "trace protector cell invalidations")
DEFINE_BOOL(feedback_normalization, false,
            "feed back normalization to constructors")
// TODO(jkummerow): This currently adds too much load on the stub cache.
DEFINE_BOOL_READONLY(internalize_on_the_fly, true,
                     "internalize string keys for generic keyed ICs on the fly")

// Flag for one shot optimiztions.
DEFINE_BOOL(enable_one_shot_optimization, false,
            "Enable size optimizations for the code that will "
            "only be executed once")

// Flag for sealed, frozen elements kind instead of dictionary elements kind
DEFINE_BOOL_READONLY(enable_sealed_frozen_elements_kind, true,
                     "Enable sealed, frozen elements kind")

// Flags for data representation optimizations
DEFINE_BOOL(unbox_double_arrays, true, "automatically unbox arrays of doubles")
DEFINE_BOOL_READONLY(string_slices, true, "use string slices")

DEFINE_INT(interrupt_budget, 144 * KB,
           "interrupt budget which should be used for the profiler counter")

// Flags for jitless
DEFINE_BOOL(jitless, V8_LITE_BOOL,
            "Disable runtime allocation of executable memory.")

// Jitless V8 has a few implications:
DEFINE_NEG_IMPLICATION(jitless, opt)
// Field representation tracking is only used by TurboFan.
DEFINE_NEG_IMPLICATION(jitless, track_field_types)
DEFINE_NEG_IMPLICATION(jitless, track_heap_object_fields)
// Regexps are interpreted.
DEFINE_IMPLICATION(jitless, regexp_interpret_all)
// asm.js validation is disabled since it triggers wasm code generation.
DEFINE_NEG_IMPLICATION(jitless, validate_asm)
// Wasm is put into interpreter-only mode. We repeat flag implications down
// here to ensure they're applied correctly by setting the --jitless flag.
DEFINE_IMPLICATION(jitless, wasm_interpret_all)
DEFINE_NEG_IMPLICATION(jitless, asm_wasm_lazy_compilation)
DEFINE_NEG_IMPLICATION(jitless, wasm_lazy_compilation)
// --jitless also implies --no-expose-wasm, see InitializeOncePerProcessImpl.

// Flags for inline caching and feedback vectors.
DEFINE_BOOL(use_ic, true, "use inline caching")
DEFINE_INT(budget_for_feedback_vector_allocation, 1 * KB,
           "The budget in amount of bytecode executed by a function before we "
           "decide to allocate feedback vectors")
DEFINE_BOOL(lazy_feedback_allocation, true, "Allocate feedback vectors lazily")

// Flags for Ignition.
DEFINE_BOOL(ignition_elide_noneffectful_bytecodes, true,
            "elide bytecodes which won't have any external effect")
DEFINE_BOOL(ignition_reo, true, "use ignition register equivalence optimizer")
DEFINE_BOOL(ignition_filter_expression_positions, true,
            "filter expression positions before the bytecode pipeline")
DEFINE_BOOL(ignition_share_named_property_feedback, true,
            "share feedback slots when loading the same named property from "
            "the same object")
DEFINE_BOOL(print_bytecode, false,
            "print bytecode generated by ignition interpreter")
DEFINE_BOOL(enable_lazy_source_positions, V8_LAZY_SOURCE_POSITIONS_BOOL,
            "skip generating source positions during initial compile but "
            "regenerate when actually required")
DEFINE_BOOL(stress_lazy_source_positions, false,
            "collect lazy source positions immediately after lazy compile")
DEFINE_STRING(print_bytecode_filter, "*",
              "filter for selecting which functions to print bytecode")
#ifdef V8_TRACE_IGNITION
DEFINE_BOOL(trace_ignition, false,
            "trace the bytecodes executed by the ignition interpreter")
#endif
#ifdef V8_TRACE_FEEDBACK_UPDATES
DEFINE_BOOL(
    trace_feedback_updates, false,
    "trace updates to feedback vectors during ignition interpreter execution.")
#endif
DEFINE_BOOL(trace_ignition_codegen, false,
            "trace the codegen of ignition interpreter bytecode handlers")
DEFINE_BOOL(trace_ignition_dispatches, false,
            "traces the dispatches to bytecode handlers by the ignition "
            "interpreter")
DEFINE_STRING(trace_ignition_dispatches_output_file, nullptr,
              "the file to which the bytecode handler dispatch table is "
              "written (by default, the table is not written to a file)")

DEFINE_BOOL(fast_math, true, "faster (but maybe less accurate) math functions")
DEFINE_BOOL(trace_track_allocation_sites, false,
            "trace the tracking of allocation sites")
DEFINE_BOOL(trace_migration, false, "trace object migration")
DEFINE_BOOL(trace_generalization, false, "trace map generalization")

// Flags for TurboProp.
DEFINE_BOOL(turboprop, false,
            "enable experimental turboprop mid-tier compiler.")
DEFINE_NEG_IMPLICATION(turboprop, turbo_inlining)
DEFINE_IMPLICATION(turboprop, concurrent_inlining)
DEFINE_VALUE_IMPLICATION(turboprop, interrupt_budget, 15 * KB)

// Flags for concurrent recompilation.
DEFINE_BOOL(concurrent_recompilation, true,
            "optimizing hot functions asynchronously on a separate thread")
DEFINE_BOOL(trace_concurrent_recompilation, false,
            "track concurrent recompilation")
DEFINE_INT(concurrent_recompilation_queue_length, 8,
           "the length of the concurrent compilation queue")
DEFINE_INT(concurrent_recompilation_delay, 0,
           "artificial compilation delay in ms")
DEFINE_BOOL(block_concurrent_recompilation, false,
            "block queued jobs until released")
DEFINE_BOOL(concurrent_inlining, false,
            "run optimizing compiler's inlining phase on a separate thread")
DEFINE_INT(max_serializer_nesting, 25,
           "maximum levels for nesting child serializers")
DEFINE_IMPLICATION(future, concurrent_inlining)
DEFINE_BOOL(trace_heap_broker_verbose, false,
            "trace the heap broker verbosely (all reports)")
DEFINE_BOOL(trace_heap_broker_memory, false,
            "trace the heap broker memory (refs analysis and zone numbers)")
DEFINE_BOOL(trace_heap_broker, false,
            "trace the heap broker (reports on missing data only)")
DEFINE_IMPLICATION(trace_heap_broker_verbose, trace_heap_broker)
DEFINE_IMPLICATION(trace_heap_broker_memory, trace_heap_broker)

// Flags for stress-testing the compiler.
DEFINE_INT(stress_runs, 0, "number of stress runs")
DEFINE_INT(deopt_every_n_times, 0,
           "deoptimize every n times a deopt point is passed")
DEFINE_BOOL(print_deopt_stress, false, "print number of possible deopt points")

// Flags for TurboFan.
DEFINE_BOOL(opt, true, "use adaptive optimizations")
DEFINE_BOOL(turbo_sp_frame_access, false,
            "use stack pointer-relative access to frame wherever possible")
DEFINE_BOOL(turbo_control_flow_aware_allocation, true,
            "consider control flow while allocating registers")

DEFINE_STRING(turbo_filter, "*", "optimization filter for TurboFan compiler")
DEFINE_BOOL(trace_turbo, false, "trace generated TurboFan IR")
DEFINE_STRING(trace_turbo_path, nullptr,
              "directory to dump generated TurboFan IR to")
DEFINE_STRING(trace_turbo_filter, "*",
              "filter for tracing turbofan compilation")
DEFINE_BOOL(trace_turbo_graph, false, "trace generated TurboFan graphs")
DEFINE_BOOL(trace_turbo_scheduled, false, "trace TurboFan IR with schedule")
DEFINE_IMPLICATION(trace_turbo_scheduled, trace_turbo_graph)
DEFINE_STRING(trace_turbo_cfg_file, nullptr,
              "trace turbo cfg graph (for C1 visualizer) to a given file name")
DEFINE_BOOL(trace_turbo_types, true, "trace TurboFan's types")
DEFINE_BOOL(trace_turbo_scheduler, false, "trace TurboFan's scheduler")
DEFINE_BOOL(trace_turbo_reduction, false, "trace TurboFan's various reducers")
DEFINE_BOOL(trace_turbo_trimming, false, "trace TurboFan's graph trimmer")
DEFINE_BOOL(trace_turbo_jt, false, "trace TurboFan's jump threading")
DEFINE_BOOL(trace_turbo_ceq, false, "trace TurboFan's control equivalence")
DEFINE_BOOL(trace_turbo_loop, false, "trace TurboFan's loop optimizations")
DEFINE_BOOL(trace_turbo_alloc, false, "trace TurboFan's register allocator")
DEFINE_BOOL(trace_all_uses, false, "trace all use positions")
DEFINE_BOOL(trace_representation, false, "trace representation types")
DEFINE_BOOL(turbo_verify, DEBUG_BOOL, "verify TurboFan graphs at each phase")
DEFINE_STRING(turbo_verify_machine_graph, nullptr,
              "verify TurboFan machine graph before instruction selection")
#ifdef ENABLE_VERIFY_CSA
DEFINE_BOOL(verify_csa, DEBUG_BOOL,
            "verify TurboFan machine graph of code stubs")
#else
// Define the flag as read-only-false so that code still compiles even in the
// non-ENABLE_VERIFY_CSA configuration.
DEFINE_BOOL_READONLY(verify_csa, false,
                     "verify TurboFan machine graph of code stubs")
#endif
DEFINE_BOOL(trace_verify_csa, false, "trace code stubs verification")
DEFINE_STRING(csa_trap_on_node, nullptr,
              "trigger break point when a node with given id is created in "
              "given stub. The format is: StubName,NodeId")
DEFINE_BOOL_READONLY(fixed_array_bounds_checks, true,
                     "enable FixedArray bounds checks")
DEFINE_BOOL(turbo_stats, false, "print TurboFan statistics")
DEFINE_BOOL(turbo_stats_nvp, false,
            "print TurboFan statistics in machine-readable format")
DEFINE_BOOL(turbo_stats_wasm, false,
            "print TurboFan statistics of wasm compilations")
DEFINE_BOOL(turbo_splitting, true, "split nodes during scheduling in TurboFan")
DEFINE_BOOL(function_context_specialization, false,
            "enable function context specialization in TurboFan")
DEFINE_BOOL(turbo_inlining, true, "enable inlining in TurboFan")
DEFINE_INT(max_inlined_bytecode_size, 500,
           "maximum size of bytecode for a single inlining")
DEFINE_INT(max_inlined_bytecode_size_cumulative, 1000,
           "maximum cumulative size of bytecode considered for inlining")
DEFINE_INT(max_inlined_bytecode_size_absolute, 5000,
           "maximum cumulative size of bytecode considered for inlining")
DEFINE_FLOAT(reserve_inline_budget_scale_factor, 1.2,
             "maximum cumulative size of bytecode considered for inlining")
DEFINE_INT(max_inlined_bytecode_size_small, 30,
           "maximum size of bytecode considered for small function inlining")
DEFINE_INT(max_optimized_bytecode_size, 60 * KB,
           "maximum bytecode size to "
           "be considered for optimization; too high values may cause "
           "the compiler to hit (release) assertions")
DEFINE_FLOAT(min_inlining_frequency, 0.15, "minimum frequency for inlining")
DEFINE_BOOL(polymorphic_inlining, true, "polymorphic inlining")
DEFINE_BOOL(stress_inline, false,
            "set high thresholds for inlining to inline as much as possible")
DEFINE_VALUE_IMPLICATION(stress_inline, max_inlined_bytecode_size, 999999)
DEFINE_VALUE_IMPLICATION(stress_inline, max_inlined_bytecode_size_cumulative,
                         999999)
DEFINE_VALUE_IMPLICATION(stress_inline, max_inlined_bytecode_size_absolute,
                         999999)
DEFINE_VALUE_IMPLICATION(stress_inline, min_inlining_frequency, 0)
DEFINE_IMPLICATION(stress_inline, polymorphic_inlining)
DEFINE_BOOL(trace_turbo_inlining, false, "trace TurboFan inlining")
DEFINE_BOOL(turbo_inline_array_builtins, true,
            "inline array builtins in TurboFan code")
DEFINE_BOOL(use_osr, true, "use on-stack replacement")
DEFINE_BOOL(trace_osr, false, "trace on-stack replacement")
DEFINE_BOOL(analyze_environment_liveness, true,
            "analyze liveness of environment slots and zap dead values")
DEFINE_BOOL(trace_environment_liveness, false,
            "trace liveness of local variable slots")
DEFINE_BOOL(turbo_load_elimination, true, "enable load elimination in TurboFan")
DEFINE_BOOL(trace_turbo_load_elimination, false,
            "trace TurboFan load elimination")
DEFINE_BOOL(turbo_profiling, false, "enable profiling in TurboFan")
DEFINE_BOOL(turbo_verify_allocation, DEBUG_BOOL,
            "verify register allocation in TurboFan")
DEFINE_BOOL(turbo_move_optimization, true, "optimize gap moves in TurboFan")
DEFINE_BOOL(turbo_jt, true, "enable jump threading in TurboFan")
DEFINE_BOOL(turbo_loop_peeling, true, "Turbofan loop peeling")
DEFINE_BOOL(turbo_loop_variable, true, "Turbofan loop variable optimization")
DEFINE_BOOL(turbo_loop_rotation, true, "Turbofan loop rotation")
DEFINE_BOOL(turbo_cf_optimization, true, "optimize control flow in TurboFan")
DEFINE_BOOL(turbo_escape, true, "enable escape analysis")
DEFINE_BOOL(turbo_allocation_folding, true, "Turbofan allocation folding")
DEFINE_BOOL(turbo_instruction_scheduling, false,
            "enable instruction scheduling in TurboFan")
DEFINE_BOOL(turbo_stress_instruction_scheduling, false,
            "randomly schedule instructions to stress dependency tracking")
DEFINE_IMPLICATION(turbo_stress_instruction_scheduling,
                   turbo_instruction_scheduling)
DEFINE_BOOL(turbo_store_elimination, true,
            "enable store-store elimination in TurboFan")
DEFINE_BOOL(trace_store_elimination, false, "trace store elimination")
DEFINE_BOOL(turbo_rewrite_far_jumps, true,
            "rewrite far to near jumps (ia32,x64)")
DEFINE_BOOL(
    stress_gc_during_compilation, false,
    "simulate GC/compiler thread race related to https://crbug.com/v8/8520")
DEFINE_BOOL(turbo_fast_api_calls, false, "enable fast API calls from TurboFan")

// Favor memory over execution speed.
DEFINE_BOOL(optimize_for_size, false,
            "Enables optimizations which favor memory size over execution "
            "speed")
DEFINE_VALUE_IMPLICATION(optimize_for_size, max_semi_space_size, 1)

#ifdef DISABLE_UNTRUSTED_CODE_MITIGATIONS
#define V8_DEFAULT_UNTRUSTED_CODE_MITIGATIONS false
#else
#define V8_DEFAULT_UNTRUSTED_CODE_MITIGATIONS true
#endif
DEFINE_BOOL(untrusted_code_mitigations, V8_DEFAULT_UNTRUSTED_CODE_MITIGATIONS,
            "Enable mitigations for executing untrusted code")
#undef V8_DEFAULT_UNTRUSTED_CODE_MITIGATIONS

// Flags for native WebAssembly.
DEFINE_BOOL(expose_wasm, true, "expose wasm interface to JavaScript")
DEFINE_BOOL(assume_asmjs_origin, false,
            "force wasm decoder to assume input is internal asm-wasm format")
DEFINE_INT(wasm_num_compilation_tasks, 128,
           "maximum number of parallel compilation tasks for wasm")
DEFINE_DEBUG_BOOL(trace_wasm_native_heap, false,
                  "trace wasm native heap events")
DEFINE_BOOL(wasm_write_protect_code_memory, false,
            "write protect code memory on the wasm native heap")
DEFINE_DEBUG_BOOL(trace_wasm_serialization, false,
                  "trace serialization/deserialization")
DEFINE_BOOL(wasm_async_compilation, true,
            "enable actual asynchronous compilation for WebAssembly.compile")
DEFINE_BOOL(wasm_test_streaming, false,
            "use streaming compilation instead of async compilation for tests")
DEFINE_UINT(wasm_max_mem_pages,
            v8::internal::wasm::kSpecMaxWasmInitialMemoryPages,
            "maximum initial number of 64KiB memory pages of a wasm instance")
DEFINE_UINT(wasm_max_mem_pages_growth,
            v8::internal::wasm::kSpecMaxWasmMaximumMemoryPages,
            "maximum number of 64KiB pages a Wasm memory can grow to")
DEFINE_UINT(wasm_max_table_size, v8::internal::wasm::kV8MaxWasmTableSize,
            "maximum table size of a wasm instance")
DEFINE_UINT(wasm_max_code_space, v8::internal::kMaxWasmCodeMB,
            "maximum committed code space for wasm (in MB)")
DEFINE_BOOL(wasm_tier_up, true,
            "enable tier up to the optimizing compiler (requires --liftoff to "
            "have an effect)")
DEFINE_DEBUG_BOOL(trace_wasm_decoder, false, "trace decoding of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_compiler, false, "trace compiling of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_interpreter, false,
                  "trace interpretation of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_streaming, false,
                  "trace streaming compilation of wasm code")
DEFINE_INT(trace_wasm_ast_start, 0,
           "start function for wasm AST trace (inclusive)")
DEFINE_INT(trace_wasm_ast_end, 0, "end function for wasm AST trace (exclusive)")
// Enable Liftoff by default on ia32 and x64. More architectures will follow
// once they are implemented and sufficiently tested.
#if V8_TARGET_ARCH_IA32 || V8_TARGET_ARCH_X64
DEFINE_BOOL(liftoff, true,
            "enable Liftoff, the baseline compiler for WebAssembly")
#else
DEFINE_BOOL(liftoff, false,
            "enable Liftoff, the baseline compiler for WebAssembly")
DEFINE_IMPLICATION(future, liftoff)
#endif
DEFINE_DEBUG_BOOL(trace_liftoff, false,
                  "trace Liftoff, the baseline compiler for WebAssembly")
DEFINE_BOOL(trace_wasm_memory, false,
            "print all memory updates performed in wasm code")
// Fuzzers use {wasm_tier_mask_for_testing} together with {liftoff} and
// {no_wasm_tier_up} to force some functions to be compiled with Turbofan.
DEFINE_INT(wasm_tier_mask_for_testing, 0,
           "bitmask of functions to compile with TurboFan instead of Liftoff")

DEFINE_BOOL(debug_in_liftoff, false,
            "use Liftoff instead of the C++ interpreter for debugging "
            "WebAssembly (experimental)")
DEFINE_IMPLICATION(future, debug_in_liftoff)

DEFINE_BOOL(validate_asm, true, "validate asm.js modules before compiling")
DEFINE_BOOL(suppress_asm_messages, false,
            "don't emit asm.js related messages (for golden file testing)")
DEFINE_BOOL(trace_asm_time, false, "log asm.js timing info to the console")
DEFINE_BOOL(trace_asm_scanner, false,
            "log tokens encountered by asm.js scanner")
DEFINE_BOOL(trace_asm_parser, false, "verbose logging of asm.js parse failures")
DEFINE_BOOL(stress_validate_asm, false, "try to validate everything as asm.js")

DEFINE_DEBUG_BOOL(dump_wasm_module, false, "dump wasm module bytes")
DEFINE_STRING(dump_wasm_module_path, nullptr,
              "directory to dump wasm modules to")

// Declare command-line flags for Wasm features. Warning: avoid using these
// flags directly in the implementation. Instead accept wasm::WasmFeatures
// for configurability.
#include "src/wasm/wasm-feature-flags.h"

#define DECL_WASM_FLAG(feat, desc, val)      \
  DEFINE_BOOL(experimental_wasm_##feat, val, \
              "enable prototype " desc " for wasm")
FOREACH_WASM_FEATURE_FLAG(DECL_WASM_FLAG)
#undef DECL_WASM_FLAG

DEFINE_BOOL(wasm_staging, false, "enable staged wasm features")

#define WASM_STAGING_IMPLICATION(feat, desc, val) \
  DEFINE_IMPLICATION(wasm_staging, experimental_wasm_##feat)
FOREACH_WASM_STAGING_FEATURE_FLAG(WASM_STAGING_IMPLICATION)
#undef WASM_STAGING_IMPLICATION

DEFINE_BOOL(wasm_opt, false, "enable wasm optimization")
DEFINE_BOOL(
    wasm_bounds_checks, true,
    "enable bounds checks (disable for performance testing only)")
DEFINE_BOOL(wasm_stack_checks, true,
            "enable stack checks (disable for performance testing only)")
DEFINE_BOOL(wasm_math_intrinsics, true,
            "intrinsify some Math imports into wasm")

DEFINE_BOOL(wasm_trap_handler, true,
            "use signal handlers to catch out of bounds memory access in wasm"
            " (currently Linux x86_64 only)")
DEFINE_BOOL(wasm_fuzzer_gen_test, false,
            "generate a test case when running a wasm fuzzer")
DEFINE_IMPLICATION(wasm_fuzzer_gen_test, single_threaded)
DEFINE_BOOL(print_wasm_code, false, "Print WebAssembly code")
DEFINE_BOOL(print_wasm_stub_code, false, "Print WebAssembly stub code")
DEFINE_BOOL(wasm_interpret_all, false,
            "execute all wasm code in the wasm interpreter")
DEFINE_BOOL(asm_wasm_lazy_compilation, false,
            "enable lazy compilation for asm-wasm modules")
DEFINE_IMPLICATION(validate_asm, asm_wasm_lazy_compilation)
DEFINE_BOOL(wasm_lazy_compilation, false,
            "enable lazy compilation for all wasm modules")
DEFINE_DEBUG_BOOL(trace_wasm_lazy_compilation, false,
                  "trace lazy compilation of wasm functions")
DEFINE_BOOL(wasm_lazy_validation, false,
            "enable lazy validation for lazily compiled wasm functions")

// Flags for wasm prototyping that are not strictly features i.e., part of
// an existing proposal that may be conditionally enabled.
DEFINE_BOOL(wasm_atomics_on_non_shared_memory, false,
            "allow atomic operations on non-shared WebAssembly memory")
DEFINE_BOOL(wasm_grow_shared_memory, true,
            "allow growing shared WebAssembly memory objects")
DEFINE_BOOL(wasm_simd_post_mvp, false,
            "allow experimental SIMD operations for prototyping that are not "
            "included in the current proposal")
DEFINE_IMPLICATION(wasm_simd_post_mvp, experimental_wasm_simd)

// wasm-interpret-all resets {asm-,}wasm-lazy-compilation.
DEFINE_NEG_IMPLICATION(wasm_interpret_all, asm_wasm_lazy_compilation)
DEFINE_NEG_IMPLICATION(wasm_interpret_all, wasm_lazy_compilation)
DEFINE_NEG_IMPLICATION(wasm_interpret_all, wasm_tier_up)
DEFINE_BOOL(wasm_code_gc, true, "enable garbage collection of wasm code")
DEFINE_BOOL(trace_wasm_code_gc, false, "trace garbage collection of wasm code")
DEFINE_BOOL(stress_wasm_code_gc, false,
            "stress test garbage collection of wasm code")
DEFINE_INT(wasm_max_initial_code_space_reservation, 0,
           "maximum size of the initial wasm code space reservation (in MB)")

// Profiler flags.
DEFINE_INT(frame_count, 1, "number of stack frames inspected by the profiler")

DEFINE_INT(stress_sampling_allocation_profiler, 0,
           "Enables sampling allocation profiler with X as a sample interval")

// Garbage collections flags.
DEFINE_SIZE_T(min_semi_space_size, 0,
              "min size of a semi-space (in MBytes), the new space consists of "
              "two semi-spaces")
DEFINE_SIZE_T(max_semi_space_size, 0,
              "max size of a semi-space (in MBytes), the new space consists of "
              "two semi-spaces")
DEFINE_INT(semi_space_growth_factor, 2, "factor by which to grow the new space")
DEFINE_SIZE_T(max_old_space_size, 0, "max size of the old space (in Mbytes)")
DEFINE_SIZE_T(
    max_heap_size, 0,
    "max size of the heap (in Mbytes) "
    "both max_semi_space_size and max_old_space_size take precedence. "
    "All three flags cannot be specified at the same time.")
DEFINE_SIZE_T(initial_heap_size, 0, "initial size of the heap (in Mbytes)")
DEFINE_BOOL(huge_max_old_generation_size, true,
            "Increase max size of the old space to 4 GB for x64 systems with"
            "the physical memory bigger than 16 GB")
DEFINE_SIZE_T(initial_old_space_size, 0, "initial old space size (in Mbytes)")
DEFINE_BOOL(global_gc_scheduling, true,
            "enable GC scheduling based on global memory")
DEFINE_BOOL(gc_global, false, "always perform global GCs")
DEFINE_INT(random_gc_interval, 0,
           "Collect garbage after random(0, X) allocations. It overrides "
           "gc_interval.")
DEFINE_INT(gc_interval, -1, "garbage collect after <n> allocations")
DEFINE_INT(retain_maps_for_n_gc, 2,
           "keeps maps alive for <n> old space garbage collections")
DEFINE_BOOL(trace_gc, false,
            "print one trace line following each garbage collection")
DEFINE_BOOL(trace_gc_nvp, false,
            "print one detailed trace line in name=value format "
            "after each garbage collection")
DEFINE_BOOL(trace_gc_ignore_scavenger, false,
            "do not print trace line after scavenger collection")
DEFINE_BOOL(trace_idle_notification, false,
            "print one trace line following each idle notification")
DEFINE_BOOL(trace_idle_notification_verbose, false,
            "prints the heap state used by the idle notification")
DEFINE_BOOL(trace_gc_verbose, false,
            "print more details following each garbage collection")
DEFINE_IMPLICATION(trace_gc_verbose, trace_gc)
DEFINE_BOOL(trace_gc_freelists, false,
            "prints details of each freelist before and after "
            "each major garbage collection")
DEFINE_BOOL(trace_gc_freelists_verbose, false,
            "prints details of freelists of each page before and after "
            "each major garbage collection")
DEFINE_IMPLICATION(trace_gc_freelists_verbose, trace_gc_freelists)
DEFINE_BOOL(trace_evacuation_candidates, false,
            "Show statistics about the pages evacuation by the compaction")
DEFINE_BOOL(
    trace_allocations_origins, false,
    "Show statistics about the origins of allocations. "
    "Combine with --no-inline-new to track allocations from generated code")
DEFINE_INT(gc_freelist_strategy, 5,
           "Freelist strategy to use: "
           "0:FreeListLegacy. "
           "1:FreeListFastAlloc. "
           "2:FreeListMany. "
           "3:FreeListManyCached. "
           "4:FreeListManyCachedFastPath. "
           "5:FreeListManyCachedOrigin. ")

DEFINE_INT(trace_allocation_stack_interval, -1,
           "print stack trace after <n> free-list allocations")
DEFINE_INT(trace_duplicate_threshold_kb, 0,
           "print duplicate objects in the heap if their size is more than "
           "given threshold")
DEFINE_BOOL(trace_fragmentation, false, "report fragmentation for old space")
DEFINE_BOOL(trace_fragmentation_verbose, false,
            "report fragmentation for old space (detailed)")
DEFINE_BOOL(trace_evacuation, false, "report evacuation statistics")
DEFINE_BOOL(trace_mutator_utilization, false,
            "print mutator utilization, allocation speed, gc speed")
DEFINE_BOOL(incremental_marking, true, "use incremental marking")
DEFINE_BOOL(incremental_marking_wrappers, true,
            "use incremental marking for marking wrappers")
DEFINE_BOOL(trace_unmapper, false, "Trace the unmapping")
DEFINE_BOOL(parallel_scavenge, true, "parallel scavenge")
DEFINE_BOOL(trace_parallel_scavenge, false, "trace parallel scavenge")
DEFINE_BOOL(write_protect_code_memory, true, "write protect code memory")
#ifdef V8_CONCURRENT_MARKING
#define V8_CONCURRENT_MARKING_BOOL true
#else
#define V8_CONCURRENT_MARKING_BOOL false
#endif
DEFINE_BOOL(concurrent_marking, V8_CONCURRENT_MARKING_BOOL,
            "use concurrent marking")
#ifdef V8_ARRAY_BUFFER_EXTENSION
#define V8_ARRAY_BUFFER_EXTENSION_BOOL true
#else
#define V8_ARRAY_BUFFER_EXTENSION_BOOL false
#endif
DEFINE_BOOL_READONLY(array_buffer_extension, V8_ARRAY_BUFFER_EXTENSION_BOOL,
                     "enable array buffer tracking using extension objects")
DEFINE_IMPLICATION(array_buffer_extension, always_promote_young_mc)
DEFINE_BOOL(concurrent_array_buffer_sweeping, true,
            "concurrently sweep array buffers")
DEFINE_BOOL(local_heaps, false, "allow heap access from background tasks")
DEFINE_BOOL(parallel_marking, true, "use parallel marking in atomic pause")
DEFINE_INT(ephemeron_fixpoint_iterations, 10,
           "number of fixpoint iterations it takes to switch to linear "
           "ephemeron algorithm")
DEFINE_BOOL(trace_concurrent_marking, false, "trace concurrent marking")
DEFINE_BOOL(concurrent_store_buffer, true,
            "use concurrent store buffer processing")
DEFINE_BOOL(concurrent_sweeping, true, "use concurrent sweeping")
DEFINE_BOOL(parallel_compaction, true, "use parallel compaction")
DEFINE_BOOL(parallel_pointer_update, true,
            "use parallel pointer update during compaction")
DEFINE_BOOL(detect_ineffective_gcs_near_heap_limit, true,
            "trigger out-of-memory failure to avoid GC storm near heap limit")
DEFINE_BOOL(trace_incremental_marking, false,
            "trace progress of the incremental marking")
DEFINE_BOOL(trace_stress_marking, false, "trace stress marking progress")
DEFINE_BOOL(trace_stress_scavenge, false, "trace stress scavenge progress")
DEFINE_BOOL(track_gc_object_stats, false,
            "track object counts and memory usage")
DEFINE_BOOL(trace_gc_object_stats, false,
            "trace object counts and memory usage")
DEFINE_BOOL(trace_zone_stats, false, "trace zone memory usage")
DEFINE_BOOL(track_retaining_path, false,
            "enable support for tracking retaining path")
DEFINE_DEBUG_BOOL(trace_backing_store, false, "trace backing store events")
DEFINE_BOOL(concurrent_array_buffer_freeing, true,
            "free array buffer allocations on a background thread")
DEFINE_INT(gc_stats, 0, "Used by tracing internally to enable gc statistics")
DEFINE_IMPLICATION(trace_gc_object_stats, track_gc_object_stats)
DEFINE_GENERIC_IMPLICATION(
    track_gc_object_stats,
    TracingFlags::gc_stats.store(
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_GENERIC_IMPLICATION(
    trace_gc_object_stats,
    TracingFlags::gc_stats.store(
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_NEG_IMPLICATION(trace_gc_object_stats, incremental_marking)
DEFINE_NEG_IMPLICATION(track_retaining_path, incremental_marking)
DEFINE_NEG_IMPLICATION(track_retaining_path, parallel_marking)
DEFINE_NEG_IMPLICATION(track_retaining_path, concurrent_marking)
DEFINE_BOOL(track_detached_contexts, true,
            "track native contexts that are expected to be garbage collected")
DEFINE_BOOL(trace_detached_contexts, false,
            "trace native contexts that are expected to be garbage collected")
DEFINE_IMPLICATION(trace_detached_contexts, track_detached_contexts)
#ifdef VERIFY_HEAP
DEFINE_BOOL(verify_heap, false, "verify heap pointers before and after GC")
DEFINE_BOOL(verify_heap_skip_remembered_set, false,
            "disable remembered set verification")
#endif
DEFINE_BOOL(move_object_start, true, "enable moving of object starts")
DEFINE_BOOL(memory_reducer, true, "use memory reducer")
DEFINE_BOOL(memory_reducer_for_small_heaps, true,
            "use memory reducer for small heaps")
DEFINE_INT(heap_growing_percent, 0,
           "specifies heap growing factor as (1 + heap_growing_percent/100)")
DEFINE_INT(v8_os_page_size, 0, "override OS page size (in KBytes)")
DEFINE_BOOL(always_compact, false, "Perform compaction on every full GC")
DEFINE_BOOL(never_compact, false,
            "Never perform compaction on full GC - testing only")
DEFINE_BOOL(compact_code_space, true, "Compact code space on full collections")
DEFINE_BOOL(flush_bytecode, true,
            "flush of bytecode when it has not been executed recently")
DEFINE_BOOL(stress_flush_bytecode, false, "stress bytecode flushing")
DEFINE_IMPLICATION(stress_flush_bytecode, flush_bytecode)
DEFINE_BOOL(use_marking_progress_bar, true,
            "Use a progress bar to scan large objects in increments when "
            "incremental marking is active.")
DEFINE_BOOL(stress_per_context_marking_worklist, false,
            "Use per-context worklist for marking")
DEFINE_BOOL(force_marking_deque_overflows, false,
            "force overflows of marking deque by reducing it's size "
            "to 64 words")
DEFINE_BOOL(stress_compaction, false,
            "stress the GC compactor to flush out bugs (implies "
            "--force_marking_deque_overflows)")
DEFINE_BOOL(stress_compaction_random, false,
            "Stress GC compaction by selecting random percent of pages as "
            "evacuation candidates. It overrides stress_compaction.")
DEFINE_BOOL(stress_incremental_marking, false,
            "force incremental marking for small heaps and run it more often")

DEFINE_BOOL(fuzzer_gc_analysis, false,
            "prints number of allocations and enables analysis mode for gc "
            "fuzz testing, e.g. --stress-marking, --stress-scavenge")
DEFINE_INT(stress_marking, 0,
           "force marking at random points between 0 and X (inclusive) percent "
           "of the regular marking start limit")
DEFINE_INT(stress_scavenge, 0,
           "force scavenge at random points between 0 and X (inclusive) "
           "percent of the new space capacity")
DEFINE_IMPLICATION(fuzzer_gc_analysis, stress_marking)
DEFINE_IMPLICATION(fuzzer_gc_analysis, stress_scavenge)

// These flags will be removed after experiments. Do not rely on them.
DEFINE_BOOL(gc_experiment_background_schedule, false,
            "new background GC schedule heuristics")
DEFINE_BOOL(gc_experiment_less_compaction, false,
            "less compaction in non-memory reducing mode")

DEFINE_BOOL(disable_abortjs, false, "disables AbortJS runtime function")

DEFINE_BOOL(randomize_all_allocations, false,
            "randomize virtual memory reservations by ignoring any hints "
            "passed when allocating pages")

DEFINE_BOOL(manual_evacuation_candidates_selection, false,
            "Test mode only flag. It allows an unit test to select evacuation "
            "candidates pages (requires --stress_compaction).")
DEFINE_BOOL(fast_promotion_new_space, false,
            "fast promote new space on high survival rates")

DEFINE_BOOL(clear_free_memory, false, "initialize free memory with 0")

DEFINE_BOOL(young_generation_large_objects, true,
            "allocates large objects by default in the young generation large "
            "object space")

// assembler-ia32.cc / assembler-arm.cc / assembler-x64.cc
DEFINE_BOOL(debug_code, DEBUG_BOOL,
            "generate extra code (assertions) for debugging")
DEFINE_BOOL(code_comments, false,
            "emit comments in code disassembly; for more readable source "
            "positions you should add --no-concurrent_recompilation")
DEFINE_BOOL(enable_sse3, true, "enable use of SSE3 instructions if available")
DEFINE_BOOL(enable_ssse3, true, "enable use of SSSE3 instructions if available")
DEFINE_BOOL(enable_sse4_1, true,
            "enable use of SSE4.1 instructions if available")
DEFINE_BOOL(enable_sse4_2, true,
            "enable use of SSE4.2 instructions if available")
DEFINE_BOOL(enable_sahf, true,
            "enable use of SAHF instruction if available (X64 only)")
DEFINE_BOOL(enable_avx, true, "enable use of AVX instructions if available")
DEFINE_BOOL(enable_fma3, true, "enable use of FMA3 instructions if available")
DEFINE_BOOL(enable_bmi1, true, "enable use of BMI1 instructions if available")
DEFINE_BOOL(enable_bmi2, true, "enable use of BMI2 instructions if available")
DEFINE_BOOL(enable_lzcnt, true, "enable use of LZCNT instruction if available")
DEFINE_BOOL(enable_popcnt, true,
            "enable use of POPCNT instruction if available")
DEFINE_STRING(arm_arch, ARM_ARCH_DEFAULT,
              "generate instructions for the selected ARM architecture if "
              "available: armv6, armv7, armv7+sudiv or armv8")
DEFINE_BOOL(force_long_branches, false,
            "force all emitted branches to be in long mode (MIPS/PPC only)")
DEFINE_STRING(mcpu, "auto", "enable optimization for specific cpu")
DEFINE_BOOL(partial_constant_pool, true,
            "enable use of partial constant pools (X64 only)")

// Controlling source positions for Torque/CSA code.
DEFINE_BOOL(enable_source_at_csa_bind, false,
            "Include source information in the binary at CSA bind locations.")

// Deprecated ARM flags (replaced by arm_arch).
DEFINE_MAYBE_BOOL(enable_armv7, "deprecated (use --arm_arch instead)")
DEFINE_MAYBE_BOOL(enable_vfp3, "deprecated (use --arm_arch instead)")
DEFINE_MAYBE_BOOL(enable_32dregs, "deprecated (use --arm_arch instead)")
DEFINE_MAYBE_BOOL(enable_neon, "deprecated (use --arm_arch instead)")
DEFINE_MAYBE_BOOL(enable_sudiv, "deprecated (use --arm_arch instead)")
DEFINE_MAYBE_BOOL(enable_armv8, "deprecated (use --arm_arch instead)")

// regexp-macro-assembler-*.cc
DEFINE_BOOL(enable_regexp_unaligned_accesses, true,
            "enable unaligned accesses for the regexp engine")

// api.cc
DEFINE_BOOL(script_streaming, true, "enable parsing on background")
DEFINE_BOOL(
    finalize_streaming_on_background, false,
    "perform the script streaming finalization on the background thread")
DEFINE_BOOL(disable_old_api_accessors, false,
            "Disable old-style API accessors whose setters trigger through the "
            "prototype chain")

// bootstrapper.cc
DEFINE_BOOL(expose_gc, false, "expose gc extension")
DEFINE_STRING(expose_gc_as, nullptr,
              "expose gc extension under the specified name")
DEFINE_IMPLICATION(expose_gc_as, expose_gc)
DEFINE_BOOL(expose_externalize_string, false,
            "expose externalize string extension")
DEFINE_BOOL(expose_trigger_failure, false, "expose trigger-failure extension")
DEFINE_INT(stack_trace_limit, 10, "number of stack frames to capture")
DEFINE_BOOL(builtins_in_stack_traces, false,
            "show built-in functions in stack traces")
DEFINE_BOOL(experimental_stack_trace_frames, false,
            "enable experimental frames (API/Builtins) and stack trace layout")
DEFINE_BOOL(disallow_code_generation_from_strings, false,
            "disallow eval and friends")
DEFINE_BOOL(expose_async_hooks, false, "expose async_hooks object")
DEFINE_STRING(expose_cputracemark_as, nullptr,
              "expose cputracemark extension under the specified name")
#ifdef ENABLE_VTUNE_TRACEMARK
DEFINE_BOOL(enable_vtune_domain_support, true, "enable vtune domain support")
#endif  // ENABLE_VTUNE_TRACEMARK

// builtins.cc
DEFINE_BOOL(allow_unsafe_function_constructor, false,
            "allow invoking the function constructor without security checks")
DEFINE_BOOL(force_slow_path, false, "always take the slow path for builtins")
DEFINE_BOOL(test_small_max_function_context_stub_size, false,
            "enable testing the function context size overflow path "
            "by making the maximum size smaller")

DEFINE_BOOL(inline_new, true, "use fast inline allocation")
DEFINE_NEG_NEG_IMPLICATION(inline_new, turbo_allocation_folding)

// codegen-ia32.cc / codegen-arm.cc
DEFINE_BOOL(trace, false, "trace function calls")

// codegen.cc
DEFINE_BOOL(lazy, true, "use lazy compilation")
DEFINE_BOOL(max_lazy, false, "ignore eager compilation hints")
DEFINE_IMPLICATION(max_lazy, lazy)
DEFINE_BOOL(trace_opt, false, "trace lazy optimization")
DEFINE_BOOL(trace_opt_verbose, false, "extra verbose compilation tracing")
DEFINE_IMPLICATION(trace_opt_verbose, trace_opt)
DEFINE_BOOL(trace_opt_stats, false, "trace lazy optimization statistics")
DEFINE_BOOL(trace_deopt, false, "trace optimize function deoptimization")
DEFINE_BOOL(trace_file_names, false,
            "include file names in trace-opt/trace-deopt output")
DEFINE_BOOL(always_opt, false, "always try to optimize functions")
DEFINE_BOOL(always_osr, false, "always try to OSR functions")
DEFINE_BOOL(prepare_always_opt, false, "prepare for turning on always opt")

DEFINE_BOOL(trace_serializer, false, "print code serializer trace")
#ifdef DEBUG
DEFINE_BOOL(external_reference_stats, false,
            "print statistics on external references used during serialization")
#endif  // DEBUG

// compilation-cache.cc
DEFINE_BOOL(compilation_cache, true, "enable compilation cache")

DEFINE_BOOL(cache_prototype_transitions, true, "cache prototype transitions")

// compiler-dispatcher.cc
DEFINE_BOOL(parallel_compile_tasks, false, "enable parallel compile tasks")
DEFINE_BOOL(compiler_dispatcher, false, "enable compiler dispatcher")
DEFINE_IMPLICATION(parallel_compile_tasks, compiler_dispatcher)
DEFINE_BOOL(trace_compiler_dispatcher, false,
            "trace compiler dispatcher activity")

// cpu-profiler.cc
DEFINE_INT(cpu_profiler_sampling_interval, 1000,
           "CPU profiler sampling interval in microseconds")

// debugger
DEFINE_BOOL(
    trace_side_effect_free_debug_evaluate, false,
    "print debug messages for side-effect-free debug-evaluate for testing")
DEFINE_BOOL(hard_abort, true, "abort by crashing")

// inspector
DEFINE_BOOL(expose_inspector_scripts, false,
            "expose injected-script-source.js for debugging")

// execution.cc
DEFINE_INT(stack_size, V8_DEFAULT_STACK_SIZE_KB,
           "default size of stack region v8 is allowed to use (in kBytes)")

// frames.cc
DEFINE_INT(max_stack_trace_source_length, 300,
           "maximum length of function source code printed in a stack trace.")

// execution.cc, messages.cc
DEFINE_BOOL(clear_exceptions_on_js_entry, false,
            "clear pending exceptions when entering JavaScript")

// counters.cc
DEFINE_INT(histogram_interval, 600000,
           "time interval in ms for aggregating memory histograms")

// heap-snapshot-generator.cc
DEFINE_BOOL(heap_profiler_trace_objects, false,
            "Dump heap object allocations/movements/size_updates")
DEFINE_BOOL(heap_profiler_use_embedder_graph, true,
            "Use the new EmbedderGraph API to get embedder nodes")
DEFINE_INT(heap_snapshot_string_limit, 1024,
           "truncate strings to this length in the heap snapshot")

// sampling-heap-profiler.cc
DEFINE_BOOL(sampling_heap_profiler_suppress_randomness, false,
            "Use constant sample intervals to eliminate test flakiness")

// v8.cc
DEFINE_BOOL(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")
// ic.cc
DEFINE_BOOL(trace_ic, false,
            "trace inline cache state transitions for tools/ic-processor")
DEFINE_IMPLICATION(trace_ic, log_code)
DEFINE_GENERIC_IMPLICATION(
    trace_ic, TracingFlags::ic_stats.store(
                  v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_BOOL_READONLY(fast_map_update, false,
                     "enable fast map update by caching the migration target")
DEFINE_BOOL(modify_field_representation_inplace, true,
            "enable in-place field representation updates")
DEFINE_INT(max_polymorphic_map_count, 4,
           "maximum number of maps to track in POLYMORPHIC state")

DEFINE_BOOL(native_code_counters, DEBUG_BOOL,
            "generate extra code for manipulating stats counters")

// objects.cc
DEFINE_BOOL(thin_strings, true, "Enable ThinString support")
DEFINE_BOOL(trace_prototype_users, false,
            "Trace updates to prototype user tracking")
DEFINE_BOOL(use_verbose_printer, true, "allows verbose printing")
DEFINE_BOOL(trace_for_in_enumerate, false, "Trace for-in enumerate slow-paths")
DEFINE_BOOL(trace_maps, false, "trace map creation")
DEFINE_BOOL(trace_maps_details, true, "also log map details")
DEFINE_IMPLICATION(trace_maps, log_code)

// parser.cc
DEFINE_BOOL(allow_natives_syntax, false, "allow natives syntax")
DEFINE_BOOL(allow_natives_for_fuzzing, false,
            "allow only natives explicitly whitelisted for fuzzers")
DEFINE_BOOL(allow_natives_for_differential_fuzzing, false,
            "allow only natives explicitly whitelisted for differential "
            "fuzzers")
DEFINE_IMPLICATION(allow_natives_for_differential_fuzzing, allow_natives_syntax)
DEFINE_IMPLICATION(allow_natives_for_fuzzing, allow_natives_syntax)
DEFINE_IMPLICATION(allow_natives_for_differential_fuzzing,
                   allow_natives_for_fuzzing)
DEFINE_BOOL(parse_only, false, "only parse the sources")

// simulator-arm.cc, simulator-arm64.cc and simulator-mips.cc
DEFINE_BOOL(trace_sim, false, "Trace simulator execution")
DEFINE_BOOL(debug_sim, false, "Enable debugging the simulator")
DEFINE_BOOL(check_icache, false,
            "Check icache flushes in ARM and MIPS simulator")
DEFINE_INT(stop_sim_at, 0, "Simulator stop after x number of instructions")
#if defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_MIPS64) || \
    defined(V8_TARGET_ARCH_PPC64)
DEFINE_INT(sim_stack_alignment, 16,
           "Stack alignment in bytes in simulator. This must be a power of two "
           "and it must be at least 16. 16 is default.")
#else
DEFINE_INT(sim_stack_alignment, 8,
           "Stack alingment in bytes in simulator (4 or 8, 8 is default)")
#endif
DEFINE_INT(sim_stack_size, 2 * MB / KB,
           "Stack size of the ARM64, MIPS64 and PPC64 simulator "
           "in kBytes (default is 2 MB)")
DEFINE_BOOL(log_colour, ENABLE_LOG_COLOUR,
            "When logging, try to use coloured output.")
DEFINE_BOOL(trace_sim_messages, false,
            "Trace simulator debug messages. Implied by --trace-sim.")

// isolate.cc
DEFINE_BOOL(async_stack_traces, true,
            "include async stack traces in Error.stack")
DEFINE_BOOL(stack_trace_on_illegal, false,
            "print stack trace when an illegal exception is thrown")
DEFINE_BOOL(abort_on_uncaught_exception, false,
            "abort program (dump core) when an uncaught exception is thrown")
DEFINE_BOOL(correctness_fuzzer_suppressions, false,
            "Suppress certain unspecified behaviors to ease correctness "
            "fuzzing: Abort program when the stack overflows or a string "
            "exceeds maximum length (as opposed to throwing RangeError). "
            "Use a fixed suppression string for error messages.")
DEFINE_BOOL(randomize_hashes, true,
            "randomize hashes to avoid predictable hash collisions "
            "(with snapshots this option cannot override the baked-in seed)")
DEFINE_BOOL(rehash_snapshot, true,
            "rehash strings from the snapshot to override the baked-in seed")
DEFINE_UINT64(hash_seed, 0,
              "Fixed seed to use to hash property keys (0 means random)"
              "(with snapshots this option cannot override the baked-in seed)")
DEFINE_INT(random_seed, 0,
           "Default seed for initializing random generator "
           "(0, the default, means to use system random).")
DEFINE_INT(fuzzer_random_seed, 0,
           "Default seed for initializing fuzzer random generator "
           "(0, the default, means to use v8's random number generator seed).")
DEFINE_BOOL(trace_rail, false, "trace RAIL mode")
DEFINE_BOOL(print_all_exceptions, false,
            "print exception object and stack trace on each thrown exception")
DEFINE_BOOL(
    detailed_error_stack_trace, false,
    "includes arguments for each function call in the error stack frames array")
DEFINE_BOOL(adjust_os_scheduling_parameters, true,
            "adjust OS specific scheduling params for the isolate")

// runtime.cc
DEFINE_BOOL(runtime_call_stats, false, "report runtime call counts and times")
DEFINE_GENERIC_IMPLICATION(
    runtime_call_stats,
    TracingFlags::runtime_stats.store(
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_BOOL(rcs, false, "report runtime call counts and times")
DEFINE_IMPLICATION(rcs, runtime_call_stats)

DEFINE_BOOL(rcs_cpu_time, false,
            "report runtime times in cpu time (the default is wall time)")
DEFINE_IMPLICATION(rcs_cpu_time, rcs)

// snapshot-common.cc
DEFINE_BOOL(profile_deserialization, false,
            "Print the time it takes to deserialize the snapshot.")
DEFINE_BOOL(serialization_statistics, false,
            "Collect statistics on serialized objects.")
#ifdef V8_ENABLE_THIRD_PARTY_HEAP
DEFINE_UINT_READONLY(serialization_chunk_size, 1,
                     "Custom size for serialization chunks")
#else
DEFINE_UINT(serialization_chunk_size, 4096,
            "Custom size for serialization chunks")
#endif
// Regexp
DEFINE_BOOL(regexp_optimization, true, "generate optimized regexp code")
DEFINE_BOOL(regexp_mode_modifiers, false, "enable inline flags in regexp.")
DEFINE_BOOL(regexp_interpret_all, false, "interpret all regexp code")
#ifdef V8_TARGET_BIG_ENDIAN
#define REGEXP_PEEPHOLE_OPTIMIZATION_BOOL false
#else
#define REGEXP_PEEPHOLE_OPTIMIZATION_BOOL true
#endif
DEFINE_BOOL(regexp_tier_up, true,
            "enable regexp interpreter and tier up to the compiler after the "
            "number of executions set by the tier up ticks flag")
DEFINE_INT(regexp_tier_up_ticks, 1,
           "set the number of executions for the regexp interpreter before "
           "tiering-up to the compiler")
DEFINE_BOOL(regexp_peephole_optimization, REGEXP_PEEPHOLE_OPTIMIZATION_BOOL,
            "enable peephole optimization for regexp bytecode")
DEFINE_BOOL(trace_regexp_peephole_optimization, false,
            "trace regexp bytecode peephole optimization")
DEFINE_BOOL(trace_regexp_bytecodes, false, "trace regexp bytecode execution")
DEFINE_BOOL(trace_regexp_assembler, false,
            "trace regexp macro assembler calls.")
DEFINE_BOOL(trace_regexp_parser, false, "trace regexp parsing")
DEFINE_BOOL(trace_regexp_tier_up, false, "trace regexp tiering up execution")

// Testing flags test/cctest/test-{flags,api,serialization}.cc
DEFINE_BOOL(testing_bool_flag, true, "testing_bool_flag")
DEFINE_MAYBE_BOOL(testing_maybe_bool_flag, "testing_maybe_bool_flag")
DEFINE_INT(testing_int_flag, 13, "testing_int_flag")
DEFINE_FLOAT(testing_float_flag, 2.5, "float-flag")
DEFINE_STRING(testing_string_flag, "Hello, world!", "string-flag")
DEFINE_INT(testing_prng_seed, 42, "Seed used for threading test randomness")

// Test flag for a check in %OptimizeFunctionOnNextCall
DEFINE_BOOL(
    testing_d8_test_runner, false,
    "test runner turns on this flag to enable a check that the funciton was "
    "prepared for optimization before marking it for optimization")

// mksnapshot.cc
DEFINE_STRING(embedded_src, nullptr,
              "Path for the generated embedded data file. (mksnapshot only)")
DEFINE_STRING(
    embedded_variant, nullptr,
    "Label to disambiguate symbols in embedded data file. (mksnapshot only)")
DEFINE_STRING(startup_src, nullptr,
              "Write V8 startup as C++ src. (mksnapshot only)")
DEFINE_STRING(startup_blob, nullptr,
              "Write V8 startup blob file. (mksnapshot only)")
DEFINE_STRING(target_arch, nullptr,
              "The mksnapshot target arch. (mksnapshot only)")
DEFINE_STRING(target_os, nullptr, "The mksnapshot target os. (mksnapshot only)")
DEFINE_BOOL(target_is_simulator, false,
            "Instruct mksnapshot that the target is meant to run in the "
            "simulator and it can generate simulator-specific instructions. "
            "(mksnapshot only)")

//
// Minor mark compact collector flags.
//
#ifdef ENABLE_MINOR_MC
DEFINE_BOOL(minor_mc_parallel_marking, true,
            "use parallel marking for the young generation")
DEFINE_BOOL(trace_minor_mc_parallel_marking, false,
            "trace parallel marking for the young generation")
DEFINE_BOOL(minor_mc, false, "perform young generation mark compact GCs")
#else
DEFINE_BOOL_READONLY(minor_mc, false,
                     "perform young generation mark compact GCs")
#endif  // ENABLE_MINOR_MC

//
// Dev shell flags
//

DEFINE_BOOL(help, false, "Print usage message, including flags, on console")
DEFINE_BOOL(dump_counters, false, "Dump counters on exit")
DEFINE_BOOL(dump_counters_nvp, false,
            "Dump counters as name-value pairs on exit")
DEFINE_BOOL(use_external_strings, false, "Use external strings for source code")

DEFINE_STRING(map_counters, "", "Map counters to a file")
DEFINE_BOOL(mock_arraybuffer_allocator, false,
            "Use a mock ArrayBuffer allocator for testing.")
DEFINE_SIZE_T(mock_arraybuffer_allocator_limit, 0,
              "Memory limit for mock ArrayBuffer allocator used to simulate "
              "OOM for testing.")
#if V8_OS_LINUX
DEFINE_BOOL(multi_mapped_mock_allocator, false,
            "Use a multi-mapped mock ArrayBuffer allocator for testing.")
#endif

// Flags for Wasm GDB remote debugging.
#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
#define DEFAULT_WASM_GDB_REMOTE_PORT 8765
DEFINE_BOOL(wasm_gdb_remote, false,
            "enable GDB-remote for WebAssembly debugging")
DEFINE_INT(wasm_gdb_remote_port, DEFAULT_WASM_GDB_REMOTE_PORT,
           "default port for WebAssembly debugging with LLDB.")
DEFINE_BOOL(wasm_pause_waiting_for_debugger, false,
            "pause at the first Webassembly instruction waiting for a debugger "
            "to attach")
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

//
// GDB JIT integration flags.
//
#undef FLAG
#ifdef ENABLE_GDB_JIT_INTERFACE
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

DEFINE_BOOL(gdbjit, false, "enable GDBJIT interface")
DEFINE_BOOL(gdbjit_full, false, "enable GDBJIT interface for all code objects")
DEFINE_BOOL(gdbjit_dump, false, "dump elf objects with debug info to disk")
DEFINE_STRING(gdbjit_dump_filter, "",
              "dump only objects containing this substring")

#ifdef ENABLE_GDB_JIT_INTERFACE
DEFINE_IMPLICATION(gdbjit_full, gdbjit)
DEFINE_IMPLICATION(gdbjit_dump, gdbjit)
#endif
DEFINE_NEG_IMPLICATION(gdbjit, compact_code_space)

//
// Debug only flags
//
#undef FLAG
#ifdef DEBUG
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

// checks.cc
#ifdef ENABLE_SLOW_DCHECKS
DEFINE_BOOL(enable_slow_asserts, true,
            "enable asserts that are slow to execute")
#endif

// codegen-ia32.cc / codegen-arm.cc / macro-assembler-*.cc
DEFINE_BOOL(print_ast, false, "print source AST")
DEFINE_BOOL(trap_on_abort, false, "replace aborts by breakpoints")

// compiler.cc
DEFINE_BOOL(print_scopes, false, "print scopes")

// contexts.cc
DEFINE_BOOL(trace_contexts, false, "trace contexts operations")

// heap.cc
DEFINE_BOOL(gc_verbose, false, "print stuff during garbage collection")
DEFINE_BOOL(code_stats, false, "report code statistics after GC")
DEFINE_BOOL(print_handles, false, "report handles after GC")
DEFINE_BOOL(check_handle_count, false,
            "Check that there are not too many handles at GC")
DEFINE_BOOL(print_global_handles, false, "report global handles after GC")

// TurboFan debug-only flags.
DEFINE_BOOL(trace_turbo_escape, false, "enable tracing in escape analysis")

// objects.cc
DEFINE_BOOL(trace_module_status, false,
            "Trace status transitions of ECMAScript modules")
DEFINE_BOOL(trace_normalization, false,
            "prints when objects are turned into dictionaries.")

// runtime.cc
DEFINE_BOOL(trace_lazy, false, "trace lazy compilation")

// spaces.cc
DEFINE_BOOL(collect_heap_spill_statistics, false,
            "report heap spill statistics along with heap_stats "
            "(requires heap_stats)")
DEFINE_BOOL(trace_isolates, false, "trace isolate state changes")

// Regexp
DEFINE_BOOL(regexp_possessive_quantifier, false,
            "enable possessive quantifier syntax for testing")

// Debugger
DEFINE_BOOL(print_break_location, false, "print source location on debug break")

// wasm instance management
DEFINE_DEBUG_BOOL(trace_wasm_instances, false,
                  "trace creation and collection of wasm instances")

#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
DEFINE_BOOL(trace_wasm_gdb_remote, false, "trace Webassembly GDB-remote server")
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

//
// Logging and profiling flags
//
#undef FLAG
#define FLAG FLAG_FULL

// log.cc
DEFINE_BOOL(log, false,
            "Minimal logging (no API, code, GC, suspect, or handles samples).")
DEFINE_BOOL(log_all, false, "Log all events to the log file.")
DEFINE_BOOL(log_api, false, "Log API events to the log file.")
DEFINE_BOOL(log_code, false,
            "Log code events to the log file without profiling.")
DEFINE_BOOL(log_handles, false, "Log global handle events.")
DEFINE_BOOL(log_suspect, false, "Log suspect operations.")
DEFINE_BOOL(log_source_code, false, "Log source code.")
DEFINE_BOOL(log_function_events, false,
            "Log function events "
            "(parse, compile, execute) separately.")
DEFINE_BOOL(prof, false,
            "Log statistical profiling information (implies --log-code).")

DEFINE_BOOL(detailed_line_info, false,
            "Always generate detailed line information for CPU profiling.")

#if defined(ANDROID)
// Phones and tablets have processors that are much slower than desktop
// and laptop computers for which current heuristics are tuned.
#define DEFAULT_PROF_SAMPLING_INTERVAL 5000
#else
#define DEFAULT_PROF_SAMPLING_INTERVAL 1000
#endif
DEFINE_INT(prof_sampling_interval, DEFAULT_PROF_SAMPLING_INTERVAL,
           "Interval for --prof samples (in microseconds).")
#undef DEFAULT_PROF_SAMPLING_INTERVAL

DEFINE_BOOL(prof_cpp, false, "Like --prof, but ignore generated code.")
DEFINE_IMPLICATION(prof, prof_cpp)
DEFINE_BOOL(prof_browser_mode, true,
            "Used with --prof, turns on browser-compatible mode for profiling.")
DEFINE_STRING(logfile, "v8.log", "Specify the name of the log file.")
DEFINE_BOOL(logfile_per_isolate, true, "Separate log files for each isolate.")
DEFINE_BOOL(ll_prof, false, "Enable low-level linux profiler.")

#if V8_OS_LINUX
#define DEFINE_PERF_PROF_BOOL(nam, cmt) DEFINE_BOOL(nam, false, cmt)
#define DEFINE_PERF_PROF_IMPLICATION DEFINE_IMPLICATION
#else
#define DEFINE_PERF_PROF_BOOL(nam, cmt) DEFINE_BOOL_READONLY(nam, false, cmt)
#define DEFINE_PERF_PROF_IMPLICATION(...)
#endif

DEFINE_PERF_PROF_BOOL(perf_basic_prof,
                      "Enable perf linux profiler (basic support).")
DEFINE_NEG_IMPLICATION(perf_basic_prof, compact_code_space)
DEFINE_PERF_PROF_BOOL(
    perf_basic_prof_only_functions,
    "Only report function code ranges to perf (i.e. no stubs).")
DEFINE_PERF_PROF_IMPLICATION(perf_basic_prof_only_functions, perf_basic_prof)
DEFINE_PERF_PROF_BOOL(
    perf_prof, "Enable perf linux profiler (experimental annotate support).")
DEFINE_PERF_PROF_BOOL(
    perf_prof_annotate_wasm,
    "Used with --perf-prof, load wasm source map and provide annotate "
    "support (experimental).")
DEFINE_PERF_PROF_BOOL(
    perf_prof_delete_file,
    "Remove the perf file right after creating it (for testing only).")
DEFINE_NEG_IMPLICATION(perf_prof, compact_code_space)
// TODO(v8:8462) Remove implication once perf supports remapping.
DEFINE_NEG_IMPLICATION(perf_prof, write_protect_code_memory)
DEFINE_NEG_IMPLICATION(perf_prof, wasm_write_protect_code_memory)

// --perf-prof-unwinding-info is available only on selected architectures.
#if !V8_TARGET_ARCH_ARM && !V8_TARGET_ARCH_ARM64 && !V8_TARGET_ARCH_X64 && \
    !V8_TARGET_ARCH_S390X && !V8_TARGET_ARCH_PPC64
#undef DEFINE_PERF_PROF_BOOL
#define DEFINE_PERF_PROF_BOOL(nam, cmt) DEFINE_BOOL_READONLY(nam, false, cmt)
#undef DEFINE_PERF_PROF_IMPLICATION
#define DEFINE_PERF_PROF_IMPLICATION(...)
#endif

DEFINE_PERF_PROF_BOOL(
    perf_prof_unwinding_info,
    "Enable unwinding info for perf linux profiler (experimental).")
DEFINE_PERF_PROF_IMPLICATION(perf_prof, perf_prof_unwinding_info)

#undef DEFINE_PERF_PROF_BOOL
#undef DEFINE_PERF_PROF_IMPLICATION

DEFINE_STRING(gc_fake_mmap, "/tmp/__v8_gc__",
              "Specify the name of the file for fake gc mmap used in ll_prof")
DEFINE_BOOL(log_internal_timer_events, false, "Time internal events.")
DEFINE_IMPLICATION(log_internal_timer_events, prof)

DEFINE_BOOL(redirect_code_traces, false,
            "output deopt information and disassembly into file "
            "code-<pid>-<isolate id>.asm")
DEFINE_STRING(redirect_code_traces_to, nullptr,
              "output deopt information and disassembly into the given file")

DEFINE_BOOL(print_opt_source, false,
            "print source code of optimized and inlined functions")

DEFINE_BOOL(vtune_prof_annotate_wasm, false,
            "Used when v8_enable_vtunejit is enabled, load wasm source map and "
            "provide annotate support (experimental).")

DEFINE_BOOL(win64_unwinding_info, true, "Enable unwinding info for Windows/x64")

#if defined(V8_TARGET_ARCH_ARM) || defined(V8_TARGET_ARCH_S390X)
// Unsupported on above architectures. See https://crbug.com/v8/8713.
DEFINE_BOOL_READONLY(
    interpreted_frames_native_stack, false,
    "Show interpreted frames on the native stack (useful for external "
    "profilers).")
#else
DEFINE_BOOL(interpreted_frames_native_stack, false,
            "Show interpreted frames on the native stack (useful for external "
            "profilers).")
#endif

//
// Disassembler only flags
//
#undef FLAG
#ifdef ENABLE_DISASSEMBLER
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

// elements.cc
DEFINE_BOOL(trace_elements_transitions, false, "trace elements transitions")

DEFINE_BOOL(trace_creation_allocation_sites, false,
            "trace the creation of allocation sites")

DEFINE_BOOL(print_code, false, "print generated code")
DEFINE_BOOL(print_opt_code, false, "print optimized code")
DEFINE_STRING(print_opt_code_filter, "*", "filter for printing optimized code")
DEFINE_BOOL(print_code_verbose, false, "print more information for code")
DEFINE_BOOL(print_builtin_code, false, "print generated code for builtins")
DEFINE_STRING(print_builtin_code_filter, "*",
              "filter for printing builtin code")
DEFINE_BOOL(print_regexp_code, false, "print generated regexp code")
DEFINE_BOOL(print_regexp_bytecode, false, "print generated regexp bytecode")
DEFINE_BOOL(print_builtin_size, false, "print code size for builtins")

#ifdef ENABLE_DISASSEMBLER
DEFINE_BOOL(sodium, false,
            "print generated code output suitable for use with "
            "the Sodium code viewer")

DEFINE_IMPLICATION(sodium, print_code)
DEFINE_IMPLICATION(sodium, print_opt_code)
DEFINE_IMPLICATION(sodium, code_comments)

DEFINE_BOOL(print_all_code, false, "enable all flags related to printing code")
DEFINE_IMPLICATION(print_all_code, print_code)
DEFINE_IMPLICATION(print_all_code, print_opt_code)
DEFINE_IMPLICATION(print_all_code, print_code_verbose)
DEFINE_IMPLICATION(print_all_code, print_builtin_code)
DEFINE_IMPLICATION(print_all_code, print_regexp_code)
DEFINE_IMPLICATION(print_all_code, code_comments)
#endif

#undef FLAG
#define FLAG FLAG_FULL

//
// Predictable mode related flags.
//

DEFINE_BOOL(predictable, false, "enable predictable mode")
DEFINE_IMPLICATION(predictable, single_threaded)
DEFINE_NEG_IMPLICATION(predictable, memory_reducer)
DEFINE_VALUE_IMPLICATION(single_threaded, wasm_num_compilation_tasks, 0)
DEFINE_NEG_IMPLICATION(single_threaded, wasm_async_compilation)

DEFINE_BOOL(predictable_gc_schedule, false,
            "Predictable garbage collection schedule. Fixes heap growing, "
            "idle, and memory reducing behavior.")
DEFINE_VALUE_IMPLICATION(predictable_gc_schedule, min_semi_space_size, 4)
DEFINE_VALUE_IMPLICATION(predictable_gc_schedule, max_semi_space_size, 4)
DEFINE_VALUE_IMPLICATION(predictable_gc_schedule, heap_growing_percent, 30)
DEFINE_NEG_IMPLICATION(predictable_gc_schedule, memory_reducer)

//
// Threading related flags.
//

DEFINE_BOOL(single_threaded, false, "disable the use of background tasks")
DEFINE_IMPLICATION(single_threaded, single_threaded_gc)
DEFINE_NEG_IMPLICATION(single_threaded, concurrent_recompilation)
DEFINE_NEG_IMPLICATION(single_threaded, compiler_dispatcher)

//
// Parallel and concurrent GC (Orinoco) related flags.
//
DEFINE_BOOL(single_threaded_gc, false, "disable the use of background gc tasks")
DEFINE_NEG_IMPLICATION(single_threaded_gc, concurrent_marking)
DEFINE_NEG_IMPLICATION(single_threaded_gc, concurrent_sweeping)
DEFINE_NEG_IMPLICATION(single_threaded_gc, parallel_compaction)
DEFINE_NEG_IMPLICATION(single_threaded_gc, parallel_marking)
DEFINE_NEG_IMPLICATION(single_threaded_gc, parallel_pointer_update)
DEFINE_NEG_IMPLICATION(single_threaded_gc, parallel_scavenge)
DEFINE_NEG_IMPLICATION(single_threaded_gc, concurrent_store_buffer)
#ifdef ENABLE_MINOR_MC
DEFINE_NEG_IMPLICATION(single_threaded_gc, minor_mc_parallel_marking)
#endif  // ENABLE_MINOR_MC
DEFINE_NEG_IMPLICATION(single_threaded_gc, concurrent_array_buffer_freeing)
DEFINE_NEG_IMPLICATION(single_threaded_gc, concurrent_array_buffer_sweeping)

#undef FLAG

#ifdef VERIFY_PREDICTABLE
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

DEFINE_BOOL(verify_predictable, false,
            "this mode is used for checking that V8 behaves predictably")
DEFINE_INT(dump_allocations_digest_at_alloc, -1,
           "dump allocations digest each n-th allocation")

//
// Read-only flags
//
#undef FLAG
#define FLAG FLAG_READONLY

// assembler.h
DEFINE_BOOL(enable_embedded_constant_pool, V8_EMBEDDED_CONSTANT_POOL,
            "enable use of embedded constant pools (PPC only)")

DEFINE_BOOL(unbox_double_fields, V8_DOUBLE_FIELDS_UNBOXING,
            "enable in-object double fields unboxing (64-bit only)")
DEFINE_IMPLICATION(unbox_double_fields, track_double_fields)

// Cleanup...
#undef FLAG_FULL
#undef FLAG_READONLY
#undef FLAG
#undef FLAG_ALIAS

#undef DEFINE_BOOL
#undef DEFINE_MAYBE_BOOL
#undef DEFINE_DEBUG_BOOL
#undef DEFINE_INT
#undef DEFINE_STRING
#undef DEFINE_FLOAT
#undef DEFINE_IMPLICATION
#undef DEFINE_NEG_IMPLICATION
#undef DEFINE_NEG_VALUE_IMPLICATION
#undef DEFINE_VALUE_IMPLICATION
#undef DEFINE_GENERIC_IMPLICATION
#undef DEFINE_ALIAS_BOOL
#undef DEFINE_ALIAS_INT
#undef DEFINE_ALIAS_STRING
#undef DEFINE_ALIAS_FLOAT

#undef FLAG_MODE_DECLARE
#undef FLAG_MODE_DEFINE
#undef FLAG_MODE_DEFINE_DEFAULTS
#undef FLAG_MODE_META
#undef FLAG_MODE_DEFINE_IMPLICATIONS
#undef FLAG_MODE_APPLY

#undef COMMA
