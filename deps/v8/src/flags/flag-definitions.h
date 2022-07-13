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

// A weak implication will be overwritten by a normal implication or by an
// explicit flag.
#define DEFINE_WEAK_IMPLICATION(whenflag, thenflag) \
  DEFINE_WEAK_VALUE_IMPLICATION(whenflag, thenflag, true)

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
#define DEFINE_VALUE_IMPLICATION(whenflag, thenflag, value)                   \
  changed |= TriggerImplication(FLAG_##whenflag, #whenflag, &FLAG_##thenflag, \
                                value, false);

// A weak implication will be overwritten by a normal implication or by an
// explicit flag.
#define DEFINE_WEAK_VALUE_IMPLICATION(whenflag, thenflag, value)              \
  changed |= TriggerImplication(FLAG_##whenflag, #whenflag, &FLAG_##thenflag, \
                                value, true);

#define DEFINE_GENERIC_IMPLICATION(whenflag, statement) \
  if (FLAG_##whenflag) statement;

#define DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, value)                \
  changed |= TriggerImplication(!FLAG_##whenflag, #whenflag, &FLAG_##thenflag, \
                                value, false);

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

#ifndef DEFINE_WEAK_VALUE_IMPLICATION
#define DEFINE_WEAK_VALUE_IMPLICATION(whenflag, thenflag, value)
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

#ifdef V8_MAP_PACKING
#define V8_MAP_PACKING_BOOL true
#else
#define V8_MAP_PACKING_BOOL false
#endif

#ifdef V8_COMPRESS_POINTERS_IN_ISOLATE_CAGE
#define COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL true
#else
#define COMPRESS_POINTERS_IN_ISOLATE_CAGE_BOOL false
#endif

#ifdef V8_COMPRESS_POINTERS_IN_SHARED_CAGE
#define COMPRESS_POINTERS_IN_SHARED_CAGE_BOOL true
#else
#define COMPRESS_POINTERS_IN_SHARED_CAGE_BOOL false
#endif

#ifdef V8_SANDBOXED_EXTERNAL_POINTERS
#define V8_SANDBOXED_EXTERNAL_POINTERS_BOOL true
#else
#define V8_SANDBOXED_EXTERNAL_POINTERS_BOOL false
#endif

#ifdef V8_SANDBOX
#define V8_SANDBOX_BOOL true
#else
#define V8_SANDBOX_BOOL false
#endif

// D8's MultiMappedAllocator is only available on Linux, and only if the sandbox
// is not enabled.
#if V8_OS_LINUX && !V8_SANDBOX_BOOL
#define MULTI_MAPPED_ALLOCATOR_AVAILABLE true
#else
#define MULTI_MAPPED_ALLOCATOR_AVAILABLE false
#endif

#ifdef V8_ENABLE_CONTROL_FLOW_INTEGRITY
#define ENABLE_CONTROL_FLOW_INTEGRITY_BOOL true
#else
#define ENABLE_CONTROL_FLOW_INTEGRITY_BOOL false
#endif

#if V8_TARGET_ARCH_PPC || V8_TARGET_ARCH_PPC64 || \
    (V8_TARGET_ARCH_S390X && COMPRESS_POINTERS_BOOL)
// TODO(v8:11421): Enable Sparkplug for these architectures.
#define ENABLE_SPARKPLUG false
#else
#define ENABLE_SPARKPLUG true
#endif

#if ENABLE_SPARKPLUG && !defined(ANDROID)
// Enable Sparkplug by default on desktop-only.
#define ENABLE_SPARKPLUG_BY_DEFAULT true
#else
#define ENABLE_SPARKPLUG_BY_DEFAULT false
#endif

#if defined(V8_OS_DARWIN) && defined(V8_HOST_ARCH_ARM64)
// Must be enabled on M1.
#define MUST_WRITE_PROTECT_CODE_MEMORY true
#else
#define MUST_WRITE_PROTECT_CODE_MEMORY false
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

// ATTENTION: This is set to true by default in d8. But for API compatibility,
// it generally defaults to false.
DEFINE_BOOL(abort_on_contradictory_flags, false,
            "Disallow flags or implications overriding each other.")
// This implication is also hard-coded into the flags processing to make sure it
// becomes active before we even process subsequent flags.
DEFINE_NEG_IMPLICATION(fuzzing, abort_on_contradictory_flags)
// This is not really a flag, it affects the interpretation of the next flag but
// doesn't become permanently true when specified. This only works for flags
// defined in this file, but not for d8 flags defined in src/d8/d8.cc.
DEFINE_BOOL(allow_overwriting_for_next_flag, false,
            "temporary disable flag contradiction to allow overwriting just "
            "the next flag")

// Flags for language modes and experimental language features.
DEFINE_BOOL(use_strict, false, "enforce strict mode")

DEFINE_BOOL(trace_temporal, false, "trace temporal code")

DEFINE_BOOL(harmony, false, "enable all completed harmony features")
DEFINE_BOOL(harmony_shipping, true, "enable all shipped harmony features")

// Update bootstrapper.cc whenever adding a new feature flag.

// Features that are still work in progress (behind individual flags).
#define HARMONY_INPROGRESS_BASE(V)                                             \
  V(harmony_weak_refs_with_cleanup_some,                                       \
    "harmony weak references with FinalizationRegistry.prototype.cleanupSome") \
  V(harmony_import_assertions, "harmony import assertions")                    \
  V(harmony_rab_gsab,                                                          \
    "harmony ResizableArrayBuffer / GrowableSharedArrayBuffer")                \
  V(harmony_temporal, "Temporal")                                              \
  V(harmony_shadow_realm, "harmony ShadowRealm")                               \
  V(harmony_struct, "harmony structs and shared structs")

#ifdef V8_INTL_SUPPORT
#define HARMONY_INPROGRESS(V) \
  HARMONY_INPROGRESS_BASE(V)  \
  V(harmony_intl_number_format_v3, "Intl.NumberFormat v3")
#else
#define HARMONY_INPROGRESS(V) HARMONY_INPROGRESS_BASE(V)
#endif

// Features that are complete (but still behind the --harmony flag).
#define HARMONY_STAGED_BASE(V) \
  V(harmony_array_grouping, "harmony array grouping")

#ifdef V8_INTL_SUPPORT
#define HARMONY_STAGED(V) \
  HARMONY_STAGED_BASE(V)  \
  V(harmony_intl_best_fit_matcher, "Intl BestFitMatcher")
#else
#define HARMONY_STAGED(V) HARMONY_STAGED_BASE(V)
#endif

// Features that are shipping (turned on by default, but internal flag remains).
#define HARMONY_SHIPPING_BASE(V)                                            \
  V(harmony_sharedarraybuffer, "harmony sharedarraybuffer")                 \
  V(harmony_atomics, "harmony atomics")                                     \
  V(harmony_private_brand_checks, "harmony private brand checks")           \
  V(harmony_relative_indexing_methods, "harmony relative indexing methods") \
  V(harmony_error_cause, "harmony error cause property")                    \
  V(harmony_object_has_own, "harmony Object.hasOwn")                        \
  V(harmony_class_static_blocks, "harmony static initializer blocks")       \
  V(harmony_array_find_last, "harmony array find last helpers")

#ifdef V8_INTL_SUPPORT
#define HARMONY_SHIPPING(V) HARMONY_SHIPPING_BASE(V)
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

DEFINE_BOOL(builtin_subclassing, true,
            "subclassing support in built-in methods")

// If the following flag is set to `true`, the SharedArrayBuffer constructor is
// enabled per context depending on the callback set via
// `SetSharedArrayBufferConstructorEnabledCallback`. If no callback is set, the
// SharedArrayBuffer constructor is disabled.
DEFINE_BOOL(enable_sharedarraybuffer_per_context, false,
            "enable the SharedArrayBuffer constructor per context")

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

DEFINE_BOOL(stress_snapshot, false,
            "disables sharing of the read-only heap for testing")
// Incremental marking is incompatible with the stress_snapshot mode;
// specifically, serialization may clear bytecode arrays from shared function
// infos which the MarkCompactCollector (running concurrently) may still need.
// See also https://crbug.com/v8/10882.
//
// Note: This is not an issue in production because we don't clear SFI's
// there (that only happens in mksnapshot and in --stress-snapshot mode).
DEFINE_NEG_IMPLICATION(stress_snapshot, incremental_marking)

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

DEFINE_NEG_IMPLICATION(enable_third_party_heap, inline_new)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, allocation_site_pretenuring)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, turbo_allocation_folding)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, concurrent_recompilation)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, script_streaming)
DEFINE_NEG_IMPLICATION(enable_third_party_heap,
                       parallel_compile_tasks_for_eager_toplevel)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, use_marking_progress_bar)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, move_object_start)
DEFINE_NEG_IMPLICATION(enable_third_party_heap, concurrent_marking)

DEFINE_BOOL_READONLY(enable_third_party_heap, V8_ENABLE_THIRD_PARTY_HEAP_BOOL,
                     "Use third-party heap")

#ifdef V8_ALLOCATION_FOLDING
#define V8_ALLOCATION_FOLDING_BOOL true
#else
#define V8_ALLOCATION_FOLDING_BOOL false
#endif

DEFINE_BOOL_READONLY(enable_allocation_folding, V8_ALLOCATION_FOLDING_BOOL,
                     "Use allocation folding globally")
DEFINE_NEG_NEG_IMPLICATION(enable_allocation_folding, turbo_allocation_folding)

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

#ifdef V8_ENABLE_UNCONDITIONAL_WRITE_BARRIERS
#define V8_ENABLE_UNCONDITIONAL_WRITE_BARRIERS_BOOL true
#else
#define V8_ENABLE_UNCONDITIONAL_WRITE_BARRIERS_BOOL false
#endif

DEFINE_BOOL_READONLY(enable_unconditional_write_barriers,
                     V8_ENABLE_UNCONDITIONAL_WRITE_BARRIERS_BOOL,
                     "always use full write barriers")

DEFINE_BOOL(use_full_record_write_builtin, true,
            "Force use of full version of RecordWrite builtin.")

#ifdef V8_ENABLE_SINGLE_GENERATION
#define V8_SINGLE_GENERATION_BOOL true
#else
#define V8_SINGLE_GENERATION_BOOL false
#endif

DEFINE_BOOL_READONLY(
    single_generation, V8_SINGLE_GENERATION_BOOL,
    "allocate all objects from young generation to old generation")

#ifdef V8_ENABLE_CONSERVATIVE_STACK_SCANNING
#define V8_ENABLE_CONSERVATIVE_STACK_SCANNING_BOOL true
#else
#define V8_ENABLE_CONSERVATIVE_STACK_SCANNING_BOOL false
#endif
DEFINE_BOOL_READONLY(conservative_stack_scanning,
                     V8_ENABLE_CONSERVATIVE_STACK_SCANNING_BOOL,
                     "use conservative stack scanning")

#ifdef V8_ENABLE_FUTURE
#define FUTURE_BOOL true
#else
#define FUTURE_BOOL false
#endif
DEFINE_BOOL(future, FUTURE_BOOL,
            "Implies all staged features that we want to ship in the "
            "not-too-far future")

#ifdef V8_ENABLE_MAGLEV
#define V8_ENABLE_MAGLEV_BOOL true
DEFINE_BOOL(maglev, false, "enable the maglev optimizing compiler")
#else
#define V8_ENABLE_MAGLEV_BOOL false
DEFINE_BOOL_READONLY(maglev, false, "enable the maglev optimizing compiler")
#endif  // V8_ENABLE_MAGLEV

DEFINE_STRING(maglev_filter, "*", "optimization filter for the maglev compiler")
DEFINE_BOOL(maglev_break_on_entry, false, "insert an int3 on maglev entries")
DEFINE_BOOL(print_maglev_graph, false, "print maglev graph")
DEFINE_BOOL(print_maglev_code, false, "print maglev code")
DEFINE_BOOL(trace_maglev_regalloc, false, "trace maglev register allocation")

#if ENABLE_SPARKPLUG
DEFINE_WEAK_IMPLICATION(future, sparkplug)
DEFINE_WEAK_IMPLICATION(future, flush_baseline_code)
#endif
#if V8_SHORT_BUILTIN_CALLS
DEFINE_WEAK_IMPLICATION(future, short_builtin_calls)
#endif
#if !MUST_WRITE_PROTECT_CODE_MEMORY
DEFINE_WEAK_VALUE_IMPLICATION(future, write_protect_code_memory, false)
#endif
DEFINE_WEAK_IMPLICATION(future, compact_maps)

DEFINE_BOOL_READONLY(dict_property_const_tracking,
                     V8_DICT_PROPERTY_CONST_TRACKING_BOOL,
                     "Use const tracking on dictionary properties")

// Flags for jitless
DEFINE_BOOL(jitless, V8_LITE_BOOL,
            "Disable runtime allocation of executable memory.")

// Jitless V8 has a few implications:
DEFINE_NEG_IMPLICATION(jitless, opt)
// Field type tracking is only used by TurboFan.
DEFINE_NEG_IMPLICATION(jitless, track_field_types)
// Regexps are interpreted.
DEFINE_IMPLICATION(jitless, regexp_interpret_all)
#if ENABLE_SPARKPLUG
// No Sparkplug compilation.
DEFINE_NEG_IMPLICATION(jitless, sparkplug)
DEFINE_NEG_IMPLICATION(jitless, always_sparkplug)
#endif  // ENABLE_SPARKPLUG
#ifdef V8_ENABLE_MAGLEV
// No Maglev compilation.
DEFINE_NEG_IMPLICATION(jitless, maglev)
#endif  // V8_ENABLE_MAGLEV

#ifndef V8_TARGET_ARCH_ARM
// Unsupported on arm. See https://crbug.com/v8/8713.
DEFINE_NEG_IMPLICATION(jitless, interpreted_frames_native_stack)
#endif

DEFINE_BOOL(assert_types, false,
            "generate runtime type assertions to test the typer")
// TODO(tebbi): Support allocating types from background thread.
DEFINE_NEG_IMPLICATION(assert_types, concurrent_recompilation)

// Enable verification of SimplifiedLowering in debug builds.
DEFINE_BOOL(verify_simplified_lowering, DEBUG_BOOL,
            "verify graph generated by simplified lowering")

DEFINE_BOOL(trace_compilation_dependencies, false, "trace code dependencies")
// Depend on --trace-deopt-verbose for reporting dependency invalidations.
DEFINE_IMPLICATION(trace_compilation_dependencies, trace_deopt_verbose)

#ifdef V8_ALLOCATION_SITE_TRACKING
#define V8_ALLOCATION_SITE_TRACKING_BOOL true
#else
#define V8_ALLOCATION_SITE_TRACKING_BOOL false
#endif

DEFINE_BOOL_READONLY(allocation_site_tracking, V8_ALLOCATION_SITE_TRACKING_BOOL,
                     "Enable allocation site tracking")
DEFINE_NEG_NEG_IMPLICATION(allocation_site_tracking,
                           allocation_site_pretenuring)

// Flags for experimental implementation features.
DEFINE_BOOL(allocation_site_pretenuring, true,
            "pretenure with allocation sites")
DEFINE_BOOL(page_promotion, true, "promote pages based on utilization")
DEFINE_INT(page_promotion_threshold, 70,
           "min percentage of live bytes on a page to enable fast evacuation")
DEFINE_BOOL(trace_pretenuring, false,
            "trace pretenuring decisions of HAllocate instructions")
DEFINE_BOOL(trace_pretenuring_statistics, false,
            "trace allocation site pretenuring statistics")
DEFINE_BOOL(track_field_types, true, "track field types")
DEFINE_BOOL(trace_block_coverage, false,
            "trace collected block coverage information")
DEFINE_BOOL(trace_protector_invalidation, false,
            "trace protector cell invalidations")
DEFINE_BOOL(trace_web_snapshot, false, "trace web snapshot deserialization")

DEFINE_BOOL(feedback_normalization, false,
            "feed back normalization to constructors")
// TODO(jkummerow): This currently adds too much load on the stub cache.
DEFINE_BOOL_READONLY(internalize_on_the_fly, true,
                     "internalize string keys for generic keyed ICs on the fly")

// Flag for sealed, frozen elements kind instead of dictionary elements kind
DEFINE_BOOL_READONLY(enable_sealed_frozen_elements_kind, true,
                     "Enable sealed, frozen elements kind")

// Flags for data representation optimizations
DEFINE_BOOL(unbox_double_arrays, true, "automatically unbox arrays of doubles")
DEFINE_BOOL_READONLY(string_slices, true, "use string slices")

// Tiering: Sparkplug / feedback vector allocation.
DEFINE_INT(interrupt_budget_for_feedback_allocation, 940,
           "The fixed interrupt budget (in bytecode size) for allocating "
           "feedback vectors")
DEFINE_INT(interrupt_budget_factor_for_feedback_allocation, 8,
           "The interrupt budget factor (applied to bytecode size) for "
           "allocating feedback vectors, used when bytecode size is known")

// Tiering: Maglev.
// The Maglev interrupt budget is chosen to be roughly 1/10th of Turbofan's
// overall budget (including the multiple required ticks).
DEFINE_INT(interrupt_budget_for_maglev, 40 * KB,
           "interrupt budget which should be used for the profiler counter")

// Tiering: Turbofan.
DEFINE_INT(interrupt_budget, 132 * KB,
           "interrupt budget which should be used for the profiler counter")
DEFINE_INT(ticks_before_optimization, 3,
           "the number of times we have to go through the interrupt budget "
           "before considering this function for optimization")
DEFINE_INT(bytecode_size_allowance_per_tick, 1100,
           "increases the number of ticks required for optimization by "
           "bytecode.length/X")
DEFINE_INT(
    max_bytecode_size_for_early_opt, 81,
    "Maximum bytecode length for a function to be optimized on the first tick")

// Flags for inline caching and feedback vectors.
DEFINE_BOOL(use_ic, true, "use inline caching")
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
#ifdef V8_TRACE_UNOPTIMIZED
DEFINE_BOOL(trace_unoptimized, false,
            "trace the bytecodes executed by all unoptimized execution")
DEFINE_BOOL(trace_ignition, false,
            "trace the bytecodes executed by the ignition interpreter")
DEFINE_BOOL(trace_baseline_exec, false,
            "trace the bytecodes executed by the baseline code")
DEFINE_WEAK_IMPLICATION(trace_unoptimized, trace_ignition)
DEFINE_WEAK_IMPLICATION(trace_unoptimized, trace_baseline_exec)
#endif
#ifdef V8_TRACE_FEEDBACK_UPDATES
DEFINE_BOOL(
    trace_feedback_updates, false,
    "trace updates to feedback vectors during ignition interpreter execution.")
#endif
DEFINE_BOOL(trace_ignition_codegen, false,
            "trace the codegen of ignition interpreter bytecode handlers")
DEFINE_STRING(
    trace_ignition_dispatches_output_file, nullptr,
    "write the bytecode handler dispatch table to the specified file (d8 only) "
    "(requires building with v8_enable_ignition_dispatch_counting)")

DEFINE_BOOL(trace_track_allocation_sites, false,
            "trace the tracking of allocation sites")
DEFINE_BOOL(trace_migration, false, "trace object migration")
DEFINE_BOOL(trace_generalization, false, "trace map generalization")

// Flags for Sparkplug
#undef FLAG
#if ENABLE_SPARKPLUG
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif
DEFINE_BOOL(sparkplug, ENABLE_SPARKPLUG_BY_DEFAULT,
            "enable Sparkplug baseline compiler")
DEFINE_BOOL(always_sparkplug, false, "directly tier up to Sparkplug code")
#if ENABLE_SPARKPLUG
DEFINE_IMPLICATION(always_sparkplug, sparkplug)
DEFINE_BOOL(baseline_batch_compilation, true, "batch compile Sparkplug code")
#if defined(V8_OS_DARWIN) && defined(V8_HOST_ARCH_ARM64)
// M1 requires W^X.
DEFINE_BOOL_READONLY(concurrent_sparkplug, false,
                     "compile Sparkplug code in a background thread")
#else
DEFINE_BOOL(concurrent_sparkplug, false,
            "compile Sparkplug code in a background thread")
DEFINE_WEAK_IMPLICATION(future, concurrent_sparkplug)
DEFINE_NEG_IMPLICATION(predictable, concurrent_sparkplug)
DEFINE_NEG_IMPLICATION(single_threaded, concurrent_sparkplug)
DEFINE_NEG_IMPLICATION(jitless, concurrent_sparkplug)
#endif
DEFINE_UINT(
    concurrent_sparkplug_max_threads, 0,
    "max number of threads that concurrent Sparkplug can use (0 for unbounded)")
#else
DEFINE_BOOL(baseline_batch_compilation, false, "batch compile Sparkplug code")
DEFINE_BOOL_READONLY(concurrent_sparkplug, false,
                     "compile Sparkplug code in a background thread")
#endif
DEFINE_STRING(sparkplug_filter, "*", "filter for Sparkplug baseline compiler")
DEFINE_BOOL(sparkplug_needs_short_builtins, false,
            "only enable Sparkplug baseline compiler when "
            "--short-builtin-calls are also enabled")
DEFINE_INT(baseline_batch_compilation_threshold, 4 * KB,
           "the estimated instruction size of a batch to trigger compilation")
DEFINE_BOOL(trace_baseline, false, "trace baseline compilation")
DEFINE_BOOL(trace_baseline_batch_compilation, false,
            "trace baseline batch compilation")
DEFINE_BOOL(trace_baseline_concurrent_compilation, false,
            "trace baseline concurrent compilation")
#undef FLAG
#define FLAG FLAG_FULL

// Internalize into a shared string table in the shared isolate
DEFINE_BOOL(shared_string_table, false, "internalize strings into shared table")
DEFINE_IMPLICATION(harmony_struct, shared_string_table)

#if !defined(V8_OS_DARWIN) || !defined(V8_HOST_ARCH_ARM64)
DEFINE_BOOL(write_code_using_rwx, true,
            "flip permissions to rwx to write page instead of rw")
DEFINE_NEG_IMPLICATION(jitless, write_code_using_rwx)
#else
DEFINE_BOOL_READONLY(write_code_using_rwx, false,
                     "flip permissions to rwx to write page instead of rw")
#endif

// Flags for concurrent recompilation.
DEFINE_BOOL(concurrent_recompilation, true,
            "optimizing hot functions asynchronously on a separate thread")
DEFINE_BOOL(trace_concurrent_recompilation, false,
            "track concurrent recompilation")
DEFINE_INT(concurrent_recompilation_queue_length, 8,
           "the length of the concurrent compilation queue")
DEFINE_INT(concurrent_recompilation_delay, 0,
           "artificial compilation delay in ms")
DEFINE_BOOL(
    stress_concurrent_inlining, false,
    "create additional concurrent optimization jobs but throw away result")
DEFINE_IMPLICATION(stress_concurrent_inlining, concurrent_recompilation)
DEFINE_NEG_IMPLICATION(stress_concurrent_inlining, lazy_feedback_allocation)
DEFINE_WEAK_VALUE_IMPLICATION(stress_concurrent_inlining, interrupt_budget,
                              15 * KB)
DEFINE_BOOL(stress_concurrent_inlining_attach_code, false,
            "create additional concurrent optimization jobs")
DEFINE_IMPLICATION(stress_concurrent_inlining_attach_code,
                   stress_concurrent_inlining)
DEFINE_INT(max_serializer_nesting, 25,
           "maximum levels for nesting child serializers")
DEFINE_BOOL(trace_heap_broker_verbose, false,
            "trace the heap broker verbosely (all reports)")
DEFINE_BOOL(trace_heap_broker_memory, false,
            "trace the heap broker memory (refs analysis and zone numbers)")
DEFINE_BOOL(trace_heap_broker, false,
            "trace the heap broker (reports on missing data only)")
DEFINE_IMPLICATION(trace_heap_broker_verbose, trace_heap_broker)
DEFINE_IMPLICATION(trace_heap_broker_memory, trace_heap_broker)
DEFINE_IMPLICATION(trace_heap_broker, trace_pending_allocations)

// Flags for stress-testing the compiler.
DEFINE_INT(stress_runs, 0, "number of stress runs")
DEFINE_INT(deopt_every_n_times, 0,
           "deoptimize every n times a deopt point is passed")
DEFINE_BOOL(print_deopt_stress, false, "print number of possible deopt points")

// Flags for TurboFan.
DEFINE_BOOL(opt, true, "use adaptive optimizations")
DEFINE_BOOL(turbo_sp_frame_access, false,
            "use stack pointer-relative access to frame wherever possible")
DEFINE_BOOL(
    stress_turbo_late_spilling, false,
    "optimize placement of all spill instructions, not just loop-top phis")

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
DEFINE_BOOL(
    trace_turbo_stack_accesses, false,
    "trace stack load/store counters for optimized code in run-time (x64 only)")
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
DEFINE_BOOL(turbo_inlining, true, "enable inlining in TurboFan")
DEFINE_INT(max_inlined_bytecode_size, 460,
           "maximum size of bytecode for a single inlining")
DEFINE_INT(max_inlined_bytecode_size_cumulative, 920,
           "maximum cumulative size of bytecode considered for inlining")
DEFINE_INT(max_inlined_bytecode_size_absolute, 4600,
           "maximum absolute size of bytecode considered for inlining")
DEFINE_FLOAT(
    reserve_inline_budget_scale_factor, 1.2,
    "scale factor of bytecode size used to calculate the inlining budget")
DEFINE_INT(max_inlined_bytecode_size_small, 27,
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
DEFINE_BOOL(concurrent_osr, false, "enable concurrent OSR")
DEFINE_WEAK_IMPLICATION(future, concurrent_osr)
DEFINE_BOOL(trace_osr, false, "trace on-stack replacement")
DEFINE_BOOL(analyze_environment_liveness, true,
            "analyze liveness of environment slots and zap dead values")
DEFINE_BOOL(trace_environment_liveness, false,
            "trace liveness of local variable slots")
DEFINE_BOOL(turbo_load_elimination, true, "enable load elimination in TurboFan")
DEFINE_BOOL(trace_turbo_load_elimination, false,
            "trace TurboFan load elimination")
DEFINE_BOOL(turbo_profiling, false, "enable basic block profiling in TurboFan")
DEFINE_BOOL(turbo_profiling_verbose, false,
            "enable basic block profiling in TurboFan, and include each "
            "function's schedule and disassembly in the output")
DEFINE_IMPLICATION(turbo_profiling_verbose, turbo_profiling)
DEFINE_BOOL(turbo_profiling_log_builtins, false,
            "emit data about basic block usage in builtins to v8.log (requires "
            "that V8 was built with v8_enable_builtins_profiling=true)")
DEFINE_BOOL(turbo_verify_allocation, DEBUG_BOOL,
            "verify register allocation in TurboFan")
DEFINE_BOOL(turbo_move_optimization, true, "optimize gap moves in TurboFan")
DEFINE_BOOL(turbo_jt, true, "enable jump threading in TurboFan")
DEFINE_BOOL(turbo_loop_peeling, true, "TurboFan loop peeling")
DEFINE_BOOL(turbo_loop_variable, true, "TurboFan loop variable optimization")
DEFINE_BOOL(turbo_loop_rotation, true, "TurboFan loop rotation")
DEFINE_BOOL(turbo_cf_optimization, true, "optimize control flow in TurboFan")
DEFINE_BOOL(turbo_escape, true, "enable escape analysis")
DEFINE_BOOL(turbo_allocation_folding, true, "TurboFan allocation folding")
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
DEFINE_BOOL(turbo_compress_translation_arrays, false,
            "compress translation arrays (experimental)")
DEFINE_WEAK_IMPLICATION(future, turbo_inline_js_wasm_calls)
DEFINE_BOOL(turbo_inline_js_wasm_calls, false, "inline JS->Wasm calls")
DEFINE_BOOL(turbo_use_mid_tier_regalloc_for_huge_functions, true,
            "fall back to the mid-tier register allocator for huge functions")
DEFINE_BOOL(turbo_force_mid_tier_regalloc, false,
            "always use the mid-tier register allocator (for testing)")

DEFINE_BOOL(turbo_optimize_apply, true, "optimize Function.prototype.apply")

DEFINE_BOOL(turbo_collect_feedback_in_generic_lowering, true,
            "enable experimental feedback collection in generic lowering.")
DEFINE_BOOL(isolate_script_cache_ageing, true,
            "enable ageing of the isolate script cache.")

DEFINE_FLOAT(script_delay, 0, "busy wait [ms] on every Script::Run")
DEFINE_FLOAT(script_delay_once, 0, "busy wait [ms] on the first Script::Run")
DEFINE_FLOAT(script_delay_fraction, 0.0,
             "busy wait after each Script::Run by the given fraction of the "
             "run's duration")

// Favor memory over execution speed.
DEFINE_BOOL(optimize_for_size, false,
            "Enables optimizations which favor memory size over execution "
            "speed")
DEFINE_VALUE_IMPLICATION(optimize_for_size, max_semi_space_size, 1)

// Flags for WebAssembly.
#if V8_ENABLE_WEBASSEMBLY

DEFINE_BOOL(wasm_generic_wrapper, true,
            "allow use of the generic js-to-wasm wrapper instead of "
            "per-signature wrappers")
DEFINE_BOOL(expose_wasm, true, "expose wasm interface to JavaScript")
DEFINE_INT(wasm_num_compilation_tasks, 128,
           "maximum number of parallel compilation tasks for wasm")
DEFINE_VALUE_IMPLICATION(single_threaded, wasm_num_compilation_tasks, 0)
DEFINE_DEBUG_BOOL(trace_wasm_native_heap, false,
                  "trace wasm native heap events")
DEFINE_BOOL(wasm_write_protect_code_memory, true,
            "write protect code memory on the wasm native heap with mprotect")
DEFINE_BOOL(wasm_memory_protection_keys, true,
            "protect wasm code memory with PKU if available (takes precedence "
            "over --wasm-write-protect-code-memory)")
DEFINE_DEBUG_BOOL(trace_wasm_serialization, false,
                  "trace serialization/deserialization")
DEFINE_BOOL(wasm_async_compilation, true,
            "enable actual asynchronous compilation for WebAssembly.compile")
DEFINE_NEG_IMPLICATION(single_threaded, wasm_async_compilation)
DEFINE_BOOL(wasm_test_streaming, false,
            "use streaming compilation instead of async compilation for tests")
DEFINE_UINT(wasm_max_mem_pages, v8::internal::wasm::kV8MaxWasmMemoryPages,
            "maximum number of 64KiB memory pages per wasm memory")
DEFINE_UINT(wasm_max_table_size, v8::internal::wasm::kV8MaxWasmTableSize,
            "maximum table size of a wasm instance")
DEFINE_UINT(wasm_max_code_space, v8::internal::kMaxWasmCodeMB,
            "maximum committed code space for wasm (in MB)")
DEFINE_BOOL(wasm_tier_up, true,
            "enable tier up to the optimizing compiler (requires --liftoff to "
            "have an effect)")
DEFINE_BOOL(wasm_dynamic_tiering, true,
            "enable dynamic tier up to the optimizing compiler")
DEFINE_NEG_NEG_IMPLICATION(liftoff, wasm_dynamic_tiering)
DEFINE_INT(wasm_tiering_budget, 1800000,
           "budget for dynamic tiering (rough approximation of bytes executed")
DEFINE_INT(
    wasm_caching_threshold, 1000000,
    "the amount of wasm top tier code that triggers the next caching event")
DEFINE_BOOL(trace_wasm_compilation_times, false,
            "print how long it took to compile each wasm function")
DEFINE_INT(wasm_tier_up_filter, -1, "only tier-up function with this index")
DEFINE_DEBUG_BOOL(trace_wasm_decoder, false, "trace decoding of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_compiler, false, "trace compiling of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_interpreter, false,
                  "trace interpretation of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_streaming, false,
                  "trace streaming compilation of wasm code")
DEFINE_DEBUG_BOOL(trace_wasm_stack_switching, false,
                  "trace wasm stack switching")
DEFINE_BOOL(liftoff, true,
            "enable Liftoff, the baseline compiler for WebAssembly")
DEFINE_BOOL(liftoff_only, false,
            "disallow TurboFan compilation for WebAssembly (for testing)")
DEFINE_IMPLICATION(liftoff_only, liftoff)
DEFINE_NEG_IMPLICATION(liftoff_only, wasm_tier_up)
DEFINE_NEG_IMPLICATION(liftoff_only, wasm_dynamic_tiering)
DEFINE_NEG_IMPLICATION(fuzzing, liftoff_only)
DEFINE_DEBUG_BOOL(
    enable_testing_opcode_in_wasm, false,
    "enables a testing opcode in wasm that is only implemented in TurboFan")
// We can't tier up (from Liftoff to TurboFan) in single-threaded mode, hence
// disable tier up in that configuration for now.
DEFINE_NEG_IMPLICATION(single_threaded, wasm_tier_up)
DEFINE_DEBUG_BOOL(trace_liftoff, false,
                  "trace Liftoff, the baseline compiler for WebAssembly")
DEFINE_BOOL(trace_wasm_memory, false,
            "print all memory updates performed in wasm code")
// Fuzzers use {wasm_tier_mask_for_testing} and {wasm_debug_mask_for_testing}
// together with {liftoff} and {no_wasm_tier_up} to force some functions to be
// compiled with TurboFan or for debug.
DEFINE_INT(wasm_tier_mask_for_testing, 0,
           "bitmask of functions to compile with TurboFan instead of Liftoff")
DEFINE_INT(wasm_debug_mask_for_testing, 0,
           "bitmask of functions to compile for debugging, only applies if the "
           "tier is Liftoff")

DEFINE_BOOL(validate_asm, true, "validate asm.js modules before compiling")
// asm.js validation is disabled since it triggers wasm code generation.
// --jitless also implies --no-expose-wasm, see InitializeOncePerProcessImpl.
DEFINE_NEG_IMPLICATION(jitless, validate_asm)
DEFINE_BOOL(suppress_asm_messages, false,
            "don't emit asm.js related messages (for golden file testing)")
DEFINE_BOOL(trace_asm_time, false, "print asm.js timing info to the console")
DEFINE_BOOL(trace_asm_scanner, false,
            "print tokens encountered by asm.js scanner")
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

DEFINE_IMPLICATION(experimental_wasm_gc, experimental_wasm_typed_funcref)

DEFINE_BOOL(wasm_gc_js_interop, false, "experimental WasmGC-JS interop")

DEFINE_BOOL(wasm_staging, false, "enable staged wasm features")

#define WASM_STAGING_IMPLICATION(feat, desc, val) \
  DEFINE_IMPLICATION(wasm_staging, experimental_wasm_##feat)
FOREACH_WASM_STAGING_FEATURE_FLAG(WASM_STAGING_IMPLICATION)
#undef WASM_STAGING_IMPLICATION

DEFINE_BOOL(wasm_opt, true, "enable wasm optimization")
DEFINE_BOOL(
    wasm_bounds_checks, true,
    "enable bounds checks (disable for performance testing only)")
DEFINE_BOOL(wasm_stack_checks, true,
            "enable stack checks (disable for performance testing only)")
DEFINE_BOOL(
    wasm_enforce_bounds_checks, false,
    "enforce explicit bounds check even if the trap handler is available")
// "no bounds checks" implies "no enforced bounds checks".
DEFINE_NEG_NEG_IMPLICATION(wasm_bounds_checks, wasm_enforce_bounds_checks)
DEFINE_BOOL(wasm_math_intrinsics, true,
            "intrinsify some Math imports into wasm")

DEFINE_BOOL(
    wasm_inlining, false,
    "enable inlining of wasm functions into wasm functions (experimental)")
DEFINE_SIZE_T(
    wasm_inlining_budget_factor, 75000,
    "maximum allowed size to inline a function is given by {n / caller size}")
DEFINE_SIZE_T(wasm_inlining_max_size, 1000,
              "maximum size of a function that can be inlined, in TF nodes")
DEFINE_BOOL(wasm_speculative_inlining, false,
            "enable speculative inlining of call_ref targets (experimental)")
DEFINE_BOOL(trace_wasm_inlining, false, "trace wasm inlining")
DEFINE_BOOL(trace_wasm_speculative_inlining, false,
            "trace wasm speculative inlining")
DEFINE_BOOL(wasm_type_canonicalization, false,
            "apply isorecursive canonicalization on wasm types")
DEFINE_IMPLICATION(wasm_speculative_inlining, wasm_dynamic_tiering)
DEFINE_IMPLICATION(wasm_speculative_inlining, wasm_inlining)
DEFINE_WEAK_IMPLICATION(experimental_wasm_gc, wasm_speculative_inlining)
DEFINE_WEAK_IMPLICATION(experimental_wasm_typed_funcref,
                        wasm_type_canonicalization)
// Speculative inlining needs type feedback from Liftoff and compilation in
// Turbofan.
DEFINE_NEG_NEG_IMPLICATION(liftoff, wasm_speculative_inlining)
DEFINE_NEG_IMPLICATION(liftoff_only, wasm_speculative_inlining)

DEFINE_BOOL(wasm_loop_unrolling, true,
            "enable loop unrolling for wasm functions")
DEFINE_BOOL(wasm_loop_peeling, false, "enable loop peeling for wasm functions")
DEFINE_BOOL(wasm_fuzzer_gen_test, false,
            "generate a test case when running a wasm fuzzer")
DEFINE_IMPLICATION(wasm_fuzzer_gen_test, single_threaded)
DEFINE_BOOL(print_wasm_code, false, "print WebAssembly code")
DEFINE_INT(print_wasm_code_function_index, -1,
           "print WebAssembly code for function at index")
DEFINE_BOOL(print_wasm_stub_code, false, "print WebAssembly stub code")
DEFINE_BOOL(asm_wasm_lazy_compilation, false,
            "enable lazy compilation for asm-wasm modules")
DEFINE_IMPLICATION(validate_asm, asm_wasm_lazy_compilation)
DEFINE_BOOL(wasm_lazy_compilation, false,
            "enable lazy compilation for all wasm modules")
DEFINE_DEBUG_BOOL(trace_wasm_lazy_compilation, false,
                  "trace lazy compilation of wasm functions")
DEFINE_BOOL(wasm_lazy_validation, false,
            "enable lazy validation for lazily compiled wasm functions")
DEFINE_BOOL(wasm_simd_ssse3_codegen, false, "allow wasm SIMD SSSE3 codegen")

DEFINE_BOOL(wasm_code_gc, true, "enable garbage collection of wasm code")
DEFINE_BOOL(trace_wasm_code_gc, false, "trace garbage collection of wasm code")
DEFINE_BOOL(stress_wasm_code_gc, false,
            "stress test garbage collection of wasm code")
DEFINE_INT(wasm_max_initial_code_space_reservation, 0,
           "maximum size of the initial wasm code space reservation (in MB)")

DEFINE_BOOL(experimental_wasm_allow_huge_modules, false,
            "allow wasm modules bigger than 1GB, but below ~2GB")

DEFINE_BOOL(trace_wasm, false, "trace wasm function calls")

// Flags for Wasm GDB remote debugging.
#ifdef V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING
#define DEFAULT_WASM_GDB_REMOTE_PORT 8765
DEFINE_BOOL(wasm_gdb_remote, false,
            "enable GDB-remote for WebAssembly debugging")
DEFINE_NEG_IMPLICATION(wasm_gdb_remote, wasm_tier_up)
DEFINE_INT(wasm_gdb_remote_port, DEFAULT_WASM_GDB_REMOTE_PORT,
           "default port for WebAssembly debugging with LLDB.")
DEFINE_BOOL(wasm_pause_waiting_for_debugger, false,
            "pause at the first Webassembly instruction waiting for a debugger "
            "to attach")
DEFINE_BOOL(trace_wasm_gdb_remote, false, "trace Webassembly GDB-remote server")
#endif  // V8_ENABLE_WASM_GDB_REMOTE_DEBUGGING

// wasm instance management
DEFINE_DEBUG_BOOL(trace_wasm_instances, false,
                  "trace creation and collection of wasm instances")

#endif  // V8_ENABLE_WEBASSEMBLY

DEFINE_INT(stress_sampling_allocation_profiler, 0,
           "Enables sampling allocation profiler with X as a sample interval")

// Garbage collections flags.
DEFINE_BOOL(lazy_new_space_shrinking, false,
            "Enables the lazy new space shrinking strategy")
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
DEFINE_BOOL(separate_gc_phases, false,
            "yound and full garbage collection phases are not overlapping")
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
DEFINE_BOOL(trace_gc_heap_layout, false,
            "print layout of pages in heap before and after gc")
DEFINE_BOOL(trace_gc_heap_layout_ignore_minor_gc, true,
            "do not print trace line before and after minor-gc")
DEFINE_BOOL(trace_evacuation_candidates, false,
            "Show statistics about the pages evacuation by the compaction")
DEFINE_BOOL(
    trace_allocations_origins, false,
    "Show statistics about the origins of allocations. "
    "Combine with --no-inline-new to track allocations from generated code")
DEFINE_BOOL(trace_pending_allocations, false,
            "trace calls to Heap::IsAllocationPending that return true")

DEFINE_INT(trace_allocation_stack_interval, -1,
           "print stack trace after <n> free-list allocations")
DEFINE_INT(trace_duplicate_threshold_kb, 0,
           "print duplicate objects in the heap if their size is more than "
           "given threshold")
DEFINE_BOOL(trace_fragmentation, false, "report fragmentation for old space")
DEFINE_BOOL(trace_fragmentation_verbose, false,
            "report fragmentation for old space (detailed)")
DEFINE_BOOL(minor_mc_trace_fragmentation, false,
            "trace fragmentation after marking")
DEFINE_BOOL(trace_evacuation, false, "report evacuation statistics")
DEFINE_BOOL(trace_mutator_utilization, false,
            "print mutator utilization, allocation speed, gc speed")
DEFINE_BOOL(incremental_marking, true, "use incremental marking")
DEFINE_BOOL(incremental_marking_wrappers, true,
            "use incremental marking for marking wrappers")
DEFINE_BOOL(incremental_marking_task, true, "use tasks for incremental marking")
DEFINE_INT(incremental_marking_soft_trigger, 0,
           "threshold for starting incremental marking via a task in percent "
           "of available space: limit - size")
DEFINE_INT(incremental_marking_hard_trigger, 0,
           "threshold for starting incremental marking immediately in percent "
           "of available space: limit - size")
DEFINE_BOOL(trace_unmapper, false, "Trace the unmapping")
DEFINE_BOOL(parallel_scavenge, true, "parallel scavenge")
DEFINE_BOOL(scavenge_task, true, "schedule scavenge tasks")
DEFINE_INT(scavenge_task_trigger, 80,
           "scavenge task trigger in percent of the current heap limit")
DEFINE_BOOL(scavenge_separate_stack_scanning, false,
            "use a separate phase for stack scanning in scavenge")
DEFINE_BOOL(trace_parallel_scavenge, false, "trace parallel scavenge")
#if MUST_WRITE_PROTECT_CODE_MEMORY
DEFINE_BOOL_READONLY(write_protect_code_memory, true,
                     "write protect code memory")
#else
DEFINE_BOOL(write_protect_code_memory, true, "write protect code memory")
#endif
#if defined(V8_ATOMIC_OBJECT_FIELD_WRITES)
#define V8_CONCURRENT_MARKING_BOOL true
#else
#define V8_CONCURRENT_MARKING_BOOL false
#endif
DEFINE_BOOL(concurrent_marking, V8_CONCURRENT_MARKING_BOOL,
            "use concurrent marking")
DEFINE_BOOL(concurrent_array_buffer_sweeping, true,
            "concurrently sweep array buffers")
DEFINE_BOOL(stress_concurrent_allocation, false,
            "start background threads that allocate memory")
DEFINE_BOOL(parallel_marking, V8_CONCURRENT_MARKING_BOOL,
            "use parallel marking in atomic pause")
DEFINE_INT(ephemeron_fixpoint_iterations, 10,
           "number of fixpoint iterations it takes to switch to linear "
           "ephemeron algorithm")
DEFINE_BOOL(trace_concurrent_marking, false, "trace concurrent marking")
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
DEFINE_GENERIC_IMPLICATION(
    trace_zone_stats,
    TracingFlags::zone_stats.store(
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_SIZE_T(
    zone_stats_tolerance, 1 * MB,
    "report a tick only when allocated zone memory changes by this amount")
DEFINE_BOOL(trace_zone_type_stats, false, "trace per-type zone memory usage")
DEFINE_GENERIC_IMPLICATION(
    trace_zone_type_stats,
    TracingFlags::zone_stats.store(
        v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_BOOL(track_retaining_path, false,
            "enable support for tracking retaining path")
DEFINE_DEBUG_BOOL(trace_backing_store, false, "trace backing store events")
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
DEFINE_BOOL(allocation_buffer_parking, true, "allocation buffer parking")
DEFINE_BOOL(compact, true,
            "Perform compaction on full GCs based on V8's default heuristics")
DEFINE_BOOL(compact_code_space, true,
            "Perform code space compaction on full collections.")
DEFINE_BOOL(compact_maps, false,
            "Perform compaction on maps on full collections.")
DEFINE_BOOL(use_map_space, true, "Use separate space for maps.")
// Without a map space we have to compact maps.
DEFINE_NEG_VALUE_IMPLICATION(use_map_space, compact_maps, true)
DEFINE_BOOL(compact_on_every_full_gc, false,
            "Perform compaction on every full GC")
DEFINE_BOOL(compact_with_stack, true,
            "Perform compaction when finalizing a full GC with stack")
DEFINE_BOOL(
    compact_code_space_with_stack, true,
    "Perform code space compaction when finalizing a full GC with stack")
DEFINE_BOOL(stress_compaction, false,
            "Stress GC compaction to flush out bugs (implies "
            "--force_marking_deque_overflows)")
DEFINE_BOOL(stress_compaction_random, false,
            "Stress GC compaction by selecting random percent of pages as "
            "evacuation candidates. Overrides stress_compaction.")
DEFINE_BOOL(flush_baseline_code, false,
            "flush of baseline code when it has not been executed recently")
DEFINE_BOOL(flush_bytecode, true,
            "flush of bytecode when it has not been executed recently")
DEFINE_BOOL(stress_flush_code, false, "stress code flushing")
DEFINE_BOOL(trace_flush_bytecode, false, "trace bytecode flushing")
DEFINE_BOOL(use_marking_progress_bar, true,
            "Use a progress bar to scan large objects in increments when "
            "incremental marking is active.")
DEFINE_BOOL(stress_per_context_marking_worklist, false,
            "Use per-context worklist for marking")
DEFINE_BOOL(force_marking_deque_overflows, false,
            "force overflows of marking deque by reducing it's size "
            "to 64 words")
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
DEFINE_VALUE_IMPLICATION(fuzzer_gc_analysis, stress_marking, 99)
DEFINE_VALUE_IMPLICATION(fuzzer_gc_analysis, stress_scavenge, 99)
DEFINE_BOOL(
    reclaim_unmodified_wrappers, true,
    "reclaim otherwise unreachable unmodified wrapper objects when possible")

// These flags will be removed after experiments. Do not rely on them.
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

DEFINE_BOOL(crash_on_aborted_evacuation, false,
            "crash when evacuation of page fails")

// assembler-ia32.cc / assembler-arm.cc / assembler-arm64.cc / assembler-x64.cc
#ifdef V8_ENABLE_DEBUG_CODE
DEFINE_BOOL(debug_code, DEBUG_BOOL,
            "generate extra code (assertions) for debugging")
#else
DEFINE_BOOL_READONLY(debug_code, false, "")
#endif
#ifdef V8_CODE_COMMENTS
DEFINE_BOOL(code_comments, false,
            "emit comments in code disassembly; for more readable source "
            "positions you should add --no-concurrent_recompilation")
#else
DEFINE_BOOL_READONLY(code_comments, false, "")
#endif
DEFINE_BOOL(enable_sse3, true, "enable use of SSE3 instructions if available")
DEFINE_BOOL(enable_ssse3, true, "enable use of SSSE3 instructions if available")
DEFINE_BOOL(enable_sse4_1, true,
            "enable use of SSE4.1 instructions if available")
DEFINE_BOOL(enable_sse4_2, true,
            "enable use of SSE4.2 instructions if available")
DEFINE_BOOL(enable_sahf, true,
            "enable use of SAHF instruction if available (X64 only)")
DEFINE_BOOL(enable_avx, true, "enable use of AVX instructions if available")
DEFINE_BOOL(enable_avx2, true, "enable use of AVX2 instructions if available")
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
DEFINE_STRING(sim_arm64_optional_features, "none",
              "enable optional features on the simulator for testing: none or "
              "all")

#if defined(V8_TARGET_ARCH_RISCV64)
DEFINE_BOOL(riscv_trap_to_simulator_debugger, false,
            "enable simulator trap to debugger")
DEFINE_BOOL(riscv_debug, false, "enable debug prints")

DEFINE_BOOL(riscv_constant_pool, true,
            "enable constant pool (RISCV only)")

DEFINE_BOOL(riscv_c_extension, false,
            "enable compressed extension isa variant (RISCV only)")
#endif

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
DEFINE_BOOL(stress_background_compile, false,
            "stress test parsing on background")
DEFINE_BOOL(concurrent_cache_deserialization, true,
            "enable deserializing code caches on background")
DEFINE_BOOL(disable_old_api_accessors, false,
            "Disable old-style API accessors whose setters trigger through the "
            "prototype chain")
DEFINE_BOOL(
    embedder_instance_types, false,
    "enable type checks based on instance types provided by the embedder")

// bootstrapper.cc
DEFINE_BOOL(expose_gc, false, "expose gc extension")
DEFINE_STRING(expose_gc_as, nullptr,
              "expose gc extension under the specified name")
DEFINE_IMPLICATION(expose_gc_as, expose_gc)
DEFINE_BOOL(expose_externalize_string, false,
            "expose externalize string extension")
DEFINE_BOOL(expose_statistics, false, "expose statistics extension")
DEFINE_BOOL(expose_trigger_failure, false, "expose trigger-failure extension")
DEFINE_BOOL(expose_ignition_statistics, false,
            "expose ignition-statistics extension (requires building with "
            "v8_enable_ignition_dispatch_counting)")
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

// bytecode-generator.cc
DEFINE_INT(switch_table_spread_threshold, 3,
           "allow the jump table used for switch statements to span a range "
           "of integers roughly equal to this number times the number of "
           "clauses in the switch")
DEFINE_INT(switch_table_min_cases, 6,
           "the number of Smi integer cases present in the switch statement "
           "before using the jump table optimization")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_BOOL(trace, false, "trace javascript function calls")

// codegen.cc
DEFINE_BOOL(lazy, true, "use lazy compilation")
DEFINE_BOOL(lazy_eval, true, "use lazy compilation during eval")
DEFINE_BOOL(lazy_streaming, true,
            "use lazy compilation during streaming compilation")
DEFINE_BOOL(max_lazy, false, "ignore eager compilation hints")
DEFINE_IMPLICATION(max_lazy, lazy)
DEFINE_BOOL(trace_opt, false, "trace optimized compilation")
DEFINE_BOOL(trace_opt_verbose, false,
            "extra verbose optimized compilation tracing")
DEFINE_IMPLICATION(trace_opt_verbose, trace_opt)
DEFINE_BOOL(trace_opt_stats, false, "trace optimized compilation statistics")
DEFINE_BOOL(trace_deopt, false, "trace deoptimization")
DEFINE_BOOL(log_deopt, false, "log deoptimization")
DEFINE_BOOL(trace_deopt_verbose, false, "extra verbose deoptimization tracing")
DEFINE_IMPLICATION(trace_deopt_verbose, trace_deopt)
DEFINE_BOOL(trace_file_names, false,
            "include file names in trace-opt/trace-deopt output")
DEFINE_BOOL(always_opt, false, "always try to optimize functions")
DEFINE_IMPLICATION(always_opt, opt)
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

// lazy-compile-dispatcher.cc
DEFINE_BOOL(lazy_compile_dispatcher, false, "enable compiler dispatcher")
DEFINE_UINT(lazy_compile_dispatcher_max_threads, 0,
            "max threads for compiler dispatcher (0 for unbounded)")
DEFINE_BOOL(trace_compiler_dispatcher, false,
            "trace compiler dispatcher activity")
DEFINE_BOOL(
    parallel_compile_tasks_for_eager_toplevel, false,
    "spawn parallel compile tasks for eagerly compiled, top-level functions")
DEFINE_IMPLICATION(parallel_compile_tasks_for_eager_toplevel,
                   lazy_compile_dispatcher)
DEFINE_BOOL(parallel_compile_tasks_for_lazy, false,
            "spawn parallel compile tasks for all lazily compiled functions")
DEFINE_IMPLICATION(parallel_compile_tasks_for_lazy, lazy_compile_dispatcher)

// cpu-profiler.cc
DEFINE_INT(cpu_profiler_sampling_interval, 1000,
           "CPU profiler sampling interval in microseconds")

// debugger
DEFINE_BOOL(
    trace_side_effect_free_debug_evaluate, false,
    "print debug messages for side-effect-free debug-evaluate for testing")
DEFINE_BOOL(hard_abort, true, "abort by crashing")

DEFINE_BOOL(experimental_async_stack_tagging_api, false,
            "enable experimental async stacks tagging API")

// disassembler
DEFINE_BOOL(log_colour, ENABLE_LOG_COLOUR,
            "When logging, try to use coloured output.")

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
DEFINE_BOOL(heap_profiler_show_hidden_objects, false,
            "use 'native' rather than 'hidden' node type in snapshot")
#ifdef V8_ENABLE_HEAP_SNAPSHOT_VERIFY
DEFINE_BOOL(heap_snapshot_verify, false,
            "verify that heap snapshot matches marking visitor behavior")
DEFINE_IMPLICATION(enable_slow_asserts, heap_snapshot_verify)
#endif

// sampling-heap-profiler.cc
DEFINE_BOOL(sampling_heap_profiler_suppress_randomness, false,
            "Use constant sample intervals to eliminate test flakiness")

// v8.cc
DEFINE_BOOL(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")
// ic.cc
DEFINE_BOOL(log_ic, false,
            "Log inline cache state transitions for tools/ic-processor")
DEFINE_IMPLICATION(log_ic, log_code)
DEFINE_GENERIC_IMPLICATION(
    log_ic, TracingFlags::ic_stats.store(
                v8::tracing::TracingCategoryObserver::ENABLED_BY_NATIVE))
DEFINE_BOOL_READONLY(fast_map_update, false,
                     "enable fast map update by caching the migration target")
DEFINE_INT(max_valid_polymorphic_map_count, 4,
           "maximum number of valid maps to track in POLYMORPHIC state")

DEFINE_BOOL(native_code_counters, DEBUG_BOOL,
            "generate extra code for manipulating stats counters")

DEFINE_BOOL(super_ic, true, "use an IC for super property loads")

DEFINE_BOOL(enable_mega_dom_ic, false, "use MegaDOM IC state for API objects")

// objects.cc
DEFINE_BOOL(trace_prototype_users, false,
            "Trace updates to prototype user tracking")
DEFINE_BOOL(trace_for_in_enumerate, false, "Trace for-in enumerate slow-paths")
DEFINE_BOOL(log_maps, false, "Log map creation")
DEFINE_BOOL(log_maps_details, true, "Also log map details")
DEFINE_IMPLICATION(log_maps, log_code)

// parser.cc
DEFINE_BOOL(allow_natives_syntax, false, "allow natives syntax")
DEFINE_BOOL(allow_natives_for_differential_fuzzing, false,
            "allow only natives explicitly allowlisted for differential "
            "fuzzers")
DEFINE_IMPLICATION(allow_natives_for_differential_fuzzing, allow_natives_syntax)
DEFINE_IMPLICATION(allow_natives_for_differential_fuzzing, fuzzing)
DEFINE_BOOL(parse_only, false, "only parse the sources")

// simulator-arm.cc, simulator-arm64.cc and simulator-mips.cc
#ifdef USE_SIMULATOR
DEFINE_BOOL(trace_sim, false, "Trace simulator execution")
DEFINE_BOOL(debug_sim, false, "Enable debugging the simulator")
DEFINE_BOOL(check_icache, false,
            "Check icache flushes in ARM and MIPS simulator")
DEFINE_INT(stop_sim_at, 0, "Simulator stop after x number of instructions")
#if defined(V8_TARGET_ARCH_ARM64) || defined(V8_TARGET_ARCH_MIPS64) ||  \
    defined(V8_TARGET_ARCH_PPC64) || defined(V8_TARGET_ARCH_RISCV64) || \
    defined(V8_TARGET_ARCH_LOONG64)
DEFINE_INT(sim_stack_alignment, 16,
           "Stack alignment in bytes in simulator. This must be a power of two "
           "and it must be at least 16. 16 is default.")
#else
DEFINE_INT(sim_stack_alignment, 8,
           "Stack alingment in bytes in simulator (4 or 8, 8 is default)")
#endif
DEFINE_INT(sim_stack_size, 2 * MB / KB,
           "Stack size of the ARM64, MIPS, MIPS64 and PPC64 simulator "
           "in kBytes (default is 2 MB)")
DEFINE_BOOL(trace_sim_messages, false,
            "Trace simulator debug messages. Implied by --trace-sim.")
#endif  // USE_SIMULATOR

#if defined V8_TARGET_ARCH_ARM64
// pointer-auth-arm64.cc
DEFINE_BOOL(sim_abort_on_bad_auth, true,
            "Stop execution when a pointer authentication fails in the "
            "ARM64 simulator.")
#endif

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
DEFINE_BOOL(experimental_flush_embedded_blob_icache, true,
            "Used in an experiment to evaluate icache flushing on certain CPUs")

// Flags for short builtin calls feature
#if V8_SHORT_BUILTIN_CALLS
#define V8_SHORT_BUILTIN_CALLS_BOOL true
#else
#define V8_SHORT_BUILTIN_CALLS_BOOL false
#endif

DEFINE_BOOL(short_builtin_calls, V8_SHORT_BUILTIN_CALLS_BOOL,
            "Put embedded builtins code into the code range for shorter "
            "builtin calls/jumps if system has >=4GB memory")

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
DEFINE_BOOL(verify_snapshot_checksum, true,
            "Verify snapshot checksums when deserializing snapshots. Enable "
            "checksum creation and verification for code caches.")
DEFINE_BOOL(profile_deserialization, false,
            "Print the time it takes to deserialize the snapshot.")
DEFINE_BOOL(serialization_statistics, false,
            "Collect statistics on serialized objects.")
// Regexp
DEFINE_BOOL(regexp_optimization, true, "generate optimized regexp code")
DEFINE_BOOL(regexp_interpret_all, false, "interpret all regexp code")
#ifdef V8_TARGET_BIG_ENDIAN
#define REGEXP_PEEPHOLE_OPTIMIZATION_BOOL false
#else
#define REGEXP_PEEPHOLE_OPTIMIZATION_BOOL true
#endif
DEFINE_BOOL(regexp_tier_up, true,
            "enable regexp interpreter and tier up to the compiler after the "
            "number of executions set by the tier up ticks flag")
DEFINE_NEG_IMPLICATION(regexp_interpret_all, regexp_tier_up)
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
DEFINE_BOOL(trace_regexp_graph, false, "trace the regexp graph")

DEFINE_BOOL(enable_experimental_regexp_engine, false,
            "recognize regexps with 'l' flag, run them on experimental engine")
DEFINE_BOOL(default_to_experimental_regexp_engine, false,
            "run regexps with the experimental engine where possible")
DEFINE_IMPLICATION(default_to_experimental_regexp_engine,
                   enable_experimental_regexp_engine)
DEFINE_BOOL(trace_experimental_regexp_engine, false,
            "trace execution of experimental regexp engine")

DEFINE_BOOL(enable_experimental_regexp_engine_on_excessive_backtracks, false,
            "fall back to a breadth-first regexp engine on excessive "
            "backtracking")
DEFINE_UINT(regexp_backtracks_before_fallback, 50000,
            "number of backtracks during regexp execution before fall back "
            "to experimental engine if "
            "enable_experimental_regexp_engine_on_excessive_backtracks is set")

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
    "test runner turns on this flag to enable a check that the function was "
    "prepared for optimization before marking it for optimization")

DEFINE_BOOL(
    fuzzing, false,
    "Fuzzers use this flag to signal that they are ... fuzzing. This causes "
    "intrinsics to fail silently (e.g. return undefined) on invalid usage.")

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
DEFINE_STRING(turbo_profiling_log_file, nullptr,
              "Path of the input file containing basic block counters for "
              "builtins. (mksnapshot only)")

// On some platforms, the .text section only has execute permissions.
DEFINE_BOOL(text_is_readable, true,
            "Whether the .text section of binary can be read")
DEFINE_NEG_NEG_IMPLICATION(text_is_readable, partial_constant_pool)

//
// Minor mark compact collector flags.
//
DEFINE_BOOL(trace_minor_mc_parallel_marking, false,
            "trace parallel marking for the young generation")
DEFINE_BOOL(minor_mc, false, "perform young generation mark compact GCs")
DEFINE_BOOL(minor_mc_sweeping, false,
            "perform sweeping in young generation mark compact GCs")

//
// Dev shell flags
//

DEFINE_BOOL(help, false, "Print usage message, including flags, on console")
DEFINE_BOOL(print_flag_values, false, "Print all flag values of V8")

// Slow histograms are also enabled via --dump-counters in d8.
DEFINE_BOOL(slow_histograms, false,
            "Enable slow histograms with more overhead.")

DEFINE_BOOL(use_external_strings, false, "Use external strings for source code")
DEFINE_STRING(map_counters, "", "Map counters to a file")
DEFINE_BOOL(mock_arraybuffer_allocator, false,
            "Use a mock ArrayBuffer allocator for testing.")
DEFINE_SIZE_T(mock_arraybuffer_allocator_limit, 0,
              "Memory limit for mock ArrayBuffer allocator used to simulate "
              "OOM for testing.")
#if MULTI_MAPPED_ALLOCATOR_AVAILABLE
DEFINE_BOOL(multi_mapped_mock_allocator, false,
            "Use a multi-mapped mock ArrayBuffer allocator for testing.")
#endif

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
DEFINE_BOOL(trace_isolates, false, "trace isolate state changes")

// Regexp
DEFINE_BOOL(regexp_possessive_quantifier, false,
            "enable possessive quantifier syntax for testing")

// Debugger
DEFINE_BOOL(print_break_location, false, "print source location on debug break")

//
// Logging and profiling flags
//
// Logging flag dependencies are are also set separately in
// V8::InitializeOncePerProcessImpl. Please add your flag to the log_all_flags
// list in v8.cc to properly set FLAG_log and automatically enable it with
// --log-all.
#undef FLAG
#define FLAG FLAG_FULL

// log.cc
DEFINE_STRING(logfile, "v8.log",
              "Specify the name of the log file, use '-' for console, '+' for "
              "a temporary file.")
DEFINE_BOOL(logfile_per_isolate, true, "Separate log files for each isolate.")

DEFINE_BOOL(log, false,
            "Minimal logging (no API, code, GC, suspect, or handles samples).")
DEFINE_BOOL(log_all, false, "Log all events to the log file.")

DEFINE_BOOL(log_code, false,
            "Log code events to the log file without profiling.")
DEFINE_BOOL(log_code_disassemble, false,
            "Log all disassembled code to the log file.")
DEFINE_IMPLICATION(log_code_disassemble, log_code)
DEFINE_BOOL(log_source_code, false, "Log source code.")
DEFINE_BOOL(log_function_events, false,
            "Log function events "
            "(parse, compile, execute) separately.")

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
DEFINE_BOOL(prof_browser_mode, true,
            "Used with --prof, turns on browser-compatible mode for profiling.")

DEFINE_BOOL(prof, false,
            "Log statistical profiling information (implies --log-code).")
DEFINE_IMPLICATION(prof, prof_cpp)
DEFINE_IMPLICATION(prof, log_code)

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
#if !MUST_WRITE_PROTECT_CODE_MEMORY
DEFINE_NEG_IMPLICATION(perf_prof, write_protect_code_memory)
#endif
#if V8_ENABLE_WEBASSEMBLY
DEFINE_NEG_IMPLICATION(perf_prof, wasm_write_protect_code_memory)
#endif  // V8_ENABLE_WEBASSEMBLY

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

#ifdef V8_TARGET_ARCH_ARM
// Unsupported on arm. See https://crbug.com/v8/8713.
DEFINE_BOOL_READONLY(
    interpreted_frames_native_stack, false,
    "Show interpreted frames on the native stack (useful for external "
    "profilers).")
#else
DEFINE_BOOL(interpreted_frames_native_stack, false,
            "Show interpreted frames on the native stack (useful for external "
            "profilers).")
#endif

DEFINE_BOOL(enable_system_instrumentation, false,
            "Enable platform-specific profiling.")
// Don't move code objects.
DEFINE_NEG_IMPLICATION(enable_system_instrumentation, compact_code_space)
#ifndef V8_TARGET_ARCH_ARM
DEFINE_IMPLICATION(enable_system_instrumentation,
                   interpreted_frames_native_stack)
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
DEFINE_BOOL(print_all_code, false, "enable all flags related to printing code")
DEFINE_IMPLICATION(print_all_code, print_code)
DEFINE_IMPLICATION(print_all_code, print_opt_code)
DEFINE_IMPLICATION(print_all_code, print_code_verbose)
DEFINE_IMPLICATION(print_all_code, print_builtin_code)
DEFINE_IMPLICATION(print_all_code, print_regexp_code)
#endif

#undef FLAG
#define FLAG FLAG_FULL

//
// Predictable mode related flags.
//

DEFINE_BOOL(predictable, false, "enable predictable mode")
DEFINE_NEG_IMPLICATION(predictable, memory_reducer)
// TODO(v8:11848): These flags were recursively implied via --single-threaded
// before. Audit them, and remove any unneeded implications.
DEFINE_IMPLICATION(predictable, single_threaded_gc)
DEFINE_NEG_IMPLICATION(predictable, concurrent_recompilation)
DEFINE_NEG_IMPLICATION(predictable, stress_concurrent_inlining)
DEFINE_NEG_IMPLICATION(predictable, lazy_compile_dispatcher)
DEFINE_NEG_IMPLICATION(predictable, parallel_compile_tasks_for_eager_toplevel)
DEFINE_NEG_IMPLICATION(predictable, parallel_compile_tasks_for_lazy)

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
DEFINE_NEG_IMPLICATION(single_threaded, stress_concurrent_inlining)
DEFINE_NEG_IMPLICATION(single_threaded, lazy_compile_dispatcher)
DEFINE_NEG_IMPLICATION(single_threaded,
                       parallel_compile_tasks_for_eager_toplevel)
DEFINE_NEG_IMPLICATION(single_threaded, parallel_compile_tasks_for_lazy)

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
DEFINE_NEG_IMPLICATION(single_threaded_gc, concurrent_array_buffer_sweeping)
DEFINE_NEG_IMPLICATION(single_threaded_gc, stress_concurrent_allocation)

// Web snapshots
// TODO(v8:11525): Remove this flag once proper embedder integration is done.
DEFINE_BOOL(
    experimental_web_snapshots, false,
    "interpret scripts as web snapshots if they start with a magic number")
DEFINE_NEG_IMPLICATION(experimental_web_snapshots, script_streaming)

#undef FLAG

#ifdef VERIFY_PREDICTABLE
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

DEFINE_BOOL(verify_predictable, false,
            "this mode is used for checking that V8 behaves predictably")
DEFINE_IMPLICATION(verify_predictable, predictable)
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
#undef DEFINE_WEAK_IMPLICATION
#undef DEFINE_NEG_IMPLICATION
#undef DEFINE_NEG_VALUE_IMPLICATION
#undef DEFINE_VALUE_IMPLICATION
#undef DEFINE_WEAK_VALUE_IMPLICATION
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
