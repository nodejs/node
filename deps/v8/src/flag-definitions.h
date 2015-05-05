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

#define DEFINE_IMPLICATION(whenflag, thenflag)              \
  DEFINE_VALUE_IMPLICATION(whenflag, thenflag, true)

#define DEFINE_NEG_IMPLICATION(whenflag, thenflag)          \
  DEFINE_VALUE_IMPLICATION(whenflag, thenflag, false)

#define DEFINE_NEG_NEG_IMPLICATION(whenflag, thenflag) \
  DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, false)

// We want to declare the names of the variables for the header file.  Normally
// this will just be an extern declaration, but for a readonly flag we let the
// compiler make better optimizations by giving it the value.
#if defined(FLAG_MODE_DECLARE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) extern ctype FLAG_##nam;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt) \
  static ctype const FLAG_##nam = def;

// We want to supply the actual storage and value for the flag variable in the
// .cc file.  We only do this for writable flags.
#elif defined(FLAG_MODE_DEFINE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) ctype FLAG_##nam = def;

// We need to define all of our default values so that the Flag structure can
// access them by pointer.  These are just used internally inside of one .cc,
// for MODE_META, so there is no impact on the flags interface.
#elif defined(FLAG_MODE_DEFINE_DEFAULTS)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  static ctype const FLAGDEFAULT_##nam = def;

// We want to write entries into our meta data table, for internal parsing and
// printing / etc in the flag parser code.  We only do this for writable flags.
#elif defined(FLAG_MODE_META)
#define FLAG_FULL(ftype, ctype, nam, def, cmt)                              \
  { Flag::TYPE_##ftype, #nam, &FLAG_##nam, &FLAGDEFAULT_##nam, cmt, false } \
  ,
#define FLAG_ALIAS(ftype, ctype, alias, nam)                     \
  {                                                              \
    Flag::TYPE_##ftype, #alias, &FLAG_##nam, &FLAGDEFAULT_##nam, \
        "alias for --" #nam, false                               \
  }                                                              \
  ,

// We produce the code to set flags when it is implied by another flag.
#elif defined(FLAG_MODE_DEFINE_IMPLICATIONS)
#define DEFINE_VALUE_IMPLICATION(whenflag, thenflag, value) \
  if (FLAG_##whenflag) FLAG_##thenflag = value;

#define DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, value) \
  if (!FLAG_##whenflag) FLAG_##thenflag = value;

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

#ifndef DEFINE_NEG_VALUE_IMPLICATION
#define DEFINE_NEG_VALUE_IMPLICATION(whenflag, thenflag, value)
#endif

#define COMMA ,

#ifdef FLAG_MODE_DECLARE
// Structure used to hold a collection of arguments to the JavaScript code.
struct JSArguments {
 public:
  inline const char*& operator[](int idx) const { return argv[idx]; }
  static JSArguments Create(int argc, const char** argv) {
    JSArguments args;
    args.argc = argc;
    args.argv = argv;
    return args;
  }
  int argc;
  const char** argv;
};

struct MaybeBoolFlag {
  static MaybeBoolFlag Create(bool has_value, bool value) {
    MaybeBoolFlag flag;
    flag.has_value = has_value;
    flag.value = value;
    return flag;
  }
  bool has_value;
  bool value;
};
#endif

#ifdef DEBUG
#define DEBUG_BOOL true
#else
#define DEBUG_BOOL false
#endif
#if (defined CAN_USE_VFP3_INSTRUCTIONS) || !(defined ARM_TEST_NO_FEATURE_PROBE)
#define ENABLE_VFP3_DEFAULT true
#else
#define ENABLE_VFP3_DEFAULT false
#endif
#if (defined CAN_USE_ARMV7_INSTRUCTIONS) || !(defined ARM_TEST_NO_FEATURE_PROBE)
#define ENABLE_ARMV7_DEFAULT true
#else
#define ENABLE_ARMV7_DEFAULT false
#endif
#if (defined CAN_USE_ARMV8_INSTRUCTIONS) || !(defined ARM_TEST_NO_FEATURE_PROBE)
#define ENABLE_ARMV8_DEFAULT true
#else
#define ENABLE_ARMV8_DEFAULT false
#endif
#if (defined CAN_USE_VFP32DREGS) || !(defined ARM_TEST_NO_FEATURE_PROBE)
#define ENABLE_32DREGS_DEFAULT true
#else
#define ENABLE_32DREGS_DEFAULT false
#endif
#if (defined CAN_USE_NEON) || !(defined ARM_TEST_NO_FEATURE_PROBE)
# define ENABLE_NEON_DEFAULT true
#else
# define ENABLE_NEON_DEFAULT false
#endif

#define DEFINE_BOOL(nam, def, cmt) FLAG(BOOL, bool, nam, def, cmt)
#define DEFINE_BOOL_READONLY(nam, def, cmt) \
  FLAG_READONLY(BOOL, bool, nam, def, cmt)
#define DEFINE_MAYBE_BOOL(nam, cmt) \
  FLAG(MAYBE_BOOL, MaybeBoolFlag, nam, {false COMMA false}, cmt)
#define DEFINE_INT(nam, def, cmt) FLAG(INT, int, nam, def, cmt)
#define DEFINE_FLOAT(nam, def, cmt) FLAG(FLOAT, double, nam, def, cmt)
#define DEFINE_STRING(nam, def, cmt) FLAG(STRING, const char*, nam, def, cmt)
#define DEFINE_ARGS(nam, cmt) FLAG(ARGS, JSArguments, nam, {0 COMMA NULL}, cmt)

#define DEFINE_ALIAS_BOOL(alias, nam) FLAG_ALIAS(BOOL, bool, alias, nam)
#define DEFINE_ALIAS_INT(alias, nam) FLAG_ALIAS(INT, int, alias, nam)
#define DEFINE_ALIAS_FLOAT(alias, nam) FLAG_ALIAS(FLOAT, double, alias, nam)
#define DEFINE_ALIAS_STRING(alias, nam) \
  FLAG_ALIAS(STRING, const char*, alias, nam)
#define DEFINE_ALIAS_ARGS(alias, nam) FLAG_ALIAS(ARGS, JSArguments, alias, nam)

//
// Flags in all modes.
//
#define FLAG FLAG_FULL

// Flags for language modes and experimental language features.
DEFINE_BOOL(use_strict, false, "enforce strict mode")
DEFINE_BOOL(use_strong, false, "enforce strong mode")
DEFINE_IMPLICATION(use_strong, use_strict)

DEFINE_BOOL(strong_mode, false, "experimental strong language mode")
DEFINE_IMPLICATION(use_strong, strong_mode)

DEFINE_BOOL(es_staging, false, "enable all completed harmony features")
DEFINE_BOOL(harmony, false, "enable all completed harmony features")
DEFINE_BOOL(harmony_shipping, true, "enable all shipped harmony fetaures")
DEFINE_IMPLICATION(harmony, es_staging)
DEFINE_IMPLICATION(es_staging, harmony)

// Features that are still work in progress (behind individual flags).
#define HARMONY_INPROGRESS(V)                                   \
  V(harmony_modules, "harmony modules")                         \
  V(harmony_arrays, "harmony array methods")                    \
  V(harmony_array_includes, "harmony Array.prototype.includes") \
  V(harmony_regexps, "harmony regular expression extensions")   \
  V(harmony_arrow_functions, "harmony arrow functions")         \
  V(harmony_proxies, "harmony proxies")                         \
  V(harmony_sloppy, "harmony features in sloppy mode")          \
  V(harmony_unicode, "harmony unicode escapes")                 \
  V(harmony_unicode_regexps, "harmony unicode regexps")         \
  V(harmony_rest_parameters, "harmony rest parameters")         \
  V(harmony_reflect, "harmony Reflect API")

// Features that are complete (but still behind --harmony/es-staging flag).
#define HARMONY_STAGED(V)                                               \
  V(harmony_computed_property_names, "harmony computed property names") \
  V(harmony_tostring, "harmony toString")

// Features that are shipping (turned on by default, but internal flag remains).
#define HARMONY_SHIPPING(V)                                                \
  V(harmony_numeric_literals, "harmony numeric literals")                  \
  V(harmony_classes, "harmony classes (implies object literal extension)") \
  V(harmony_object_literals, "harmony object literal extensions")

// Once a shipping feature has proved stable in the wild, it will be dropped
// from HARMONY_SHIPPING, all occurrences of the FLAG_ variable are removed,
// and associated tests are moved from the harmony directory to the appropriate
// esN directory.


#define FLAG_INPROGRESS_FEATURES(id, description) \
  DEFINE_BOOL(id, false, "enable " #description " (in progress)")
HARMONY_INPROGRESS(FLAG_INPROGRESS_FEATURES)
#undef FLAG_INPROGRESS_FEATURES

#define FLAG_STAGED_FEATURES(id, description) \
  DEFINE_BOOL(id, false, "enable " #description) \
  DEFINE_IMPLICATION(es_staging, id)
HARMONY_STAGED(FLAG_STAGED_FEATURES)
#undef FLAG_STAGED_FEATURES

#define FLAG_SHIPPING_FEATURES(id, description) \
  DEFINE_BOOL(id, true, "enable " #description) \
  DEFINE_NEG_NEG_IMPLICATION(harmony_shipping, id)
HARMONY_SHIPPING(FLAG_SHIPPING_FEATURES)
#undef FLAG_SHIPPING_FEATURES


// Feature dependencies.
DEFINE_IMPLICATION(harmony_classes, harmony_object_literals)
DEFINE_IMPLICATION(harmony_unicode_regexps, harmony_unicode)


// Flags for experimental implementation features.
DEFINE_BOOL(compiled_keyed_generic_loads, false,
            "use optimizing compiler to generate keyed generic load stubs")
// TODO(hpayer): We will remove this flag as soon as we have pretenuring
// support for specific allocation sites.
DEFINE_BOOL(pretenuring_call_new, false, "pretenure call new")
DEFINE_BOOL(allocation_site_pretenuring, true,
            "pretenure with allocation sites")
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
DEFINE_BOOL(smi_binop, true, "support smi representation in binary operations")
DEFINE_BOOL(vector_ics, false, "support vector-based ics")

// Flags for optimization types.
DEFINE_BOOL(optimize_for_size, false,
            "Enables optimizations which favor memory size over execution "
            "speed.")

DEFINE_VALUE_IMPLICATION(optimize_for_size, max_semi_space_size, 1)

// Flags for data representation optimizations
DEFINE_BOOL(unbox_double_arrays, true, "automatically unbox arrays of doubles")
DEFINE_BOOL(string_slices, true, "use string slices")

// Flags for Crankshaft.
DEFINE_BOOL(crankshaft, true, "use crankshaft")
DEFINE_STRING(hydrogen_filter, "*", "optimization filter")
DEFINE_BOOL(use_gvn, true, "use hydrogen global value numbering")
DEFINE_INT(gvn_iterations, 3, "maximum number of GVN fix-point iterations")
DEFINE_BOOL(use_canonicalizing, true, "use hydrogen instruction canonicalizing")
DEFINE_BOOL(use_inlining, true, "use function inlining")
DEFINE_BOOL(use_escape_analysis, true, "use hydrogen escape analysis")
DEFINE_BOOL(use_allocation_folding, true, "use allocation folding")
DEFINE_BOOL(use_local_allocation_folding, false, "only fold in basic blocks")
DEFINE_BOOL(use_write_barrier_elimination, true,
            "eliminate write barriers targeting allocations in optimized code")
DEFINE_INT(max_inlining_levels, 5, "maximum number of inlining levels")
DEFINE_INT(max_inlined_source_size, 600,
           "maximum source size in bytes considered for a single inlining")
DEFINE_INT(max_inlined_nodes, 196,
           "maximum number of AST nodes considered for a single inlining")
DEFINE_INT(max_inlined_nodes_cumulative, 400,
           "maximum cumulative number of AST nodes considered for inlining")
DEFINE_BOOL(loop_invariant_code_motion, true, "loop invariant code motion")
DEFINE_BOOL(fast_math, true, "faster (but maybe less accurate) math functions")
DEFINE_BOOL(collect_megamorphic_maps_from_stub_cache, true,
            "crankshaft harvests type feedback from stub cache")
DEFINE_BOOL(hydrogen_stats, false, "print statistics for hydrogen")
DEFINE_BOOL(trace_check_elimination, false, "trace check elimination phase")
DEFINE_BOOL(trace_environment_liveness, false,
            "trace liveness of local variable slots")
DEFINE_BOOL(trace_hydrogen, false, "trace generated hydrogen to file")
DEFINE_STRING(trace_hydrogen_filter, "*", "hydrogen tracing filter")
DEFINE_BOOL(trace_hydrogen_stubs, false, "trace generated hydrogen for stubs")
DEFINE_STRING(trace_hydrogen_file, NULL, "trace hydrogen to given file name")
DEFINE_STRING(trace_phase, "HLZ", "trace generated IR for specified phases")
DEFINE_BOOL(trace_inlining, false, "trace inlining decisions")
DEFINE_BOOL(trace_load_elimination, false, "trace load elimination")
DEFINE_BOOL(trace_store_elimination, false, "trace store elimination")
DEFINE_BOOL(trace_alloc, false, "trace register allocator")
DEFINE_BOOL(trace_all_uses, false, "trace all use positions")
DEFINE_BOOL(trace_range, false, "trace range analysis")
DEFINE_BOOL(trace_gvn, false, "trace global value numbering")
DEFINE_BOOL(trace_representation, false, "trace representation types")
DEFINE_BOOL(trace_removable_simulates, false, "trace removable simulates")
DEFINE_BOOL(trace_escape_analysis, false, "trace hydrogen escape analysis")
DEFINE_BOOL(trace_allocation_folding, false, "trace allocation folding")
DEFINE_BOOL(trace_track_allocation_sites, false,
            "trace the tracking of allocation sites")
DEFINE_BOOL(trace_migration, false, "trace object migration")
DEFINE_BOOL(trace_generalization, false, "trace map generalization")
DEFINE_BOOL(stress_pointer_maps, false, "pointer map for every instruction")
DEFINE_BOOL(stress_environments, false, "environment for every instruction")
DEFINE_INT(deopt_every_n_times, 0,
           "deoptimize every n times a deopt point is passed")
DEFINE_INT(deopt_every_n_garbage_collections, 0,
           "deoptimize every n garbage collections")
DEFINE_BOOL(print_deopt_stress, false, "print number of possible deopt points")
DEFINE_BOOL(trap_on_deopt, false, "put a break point before deoptimizing")
DEFINE_BOOL(trap_on_stub_deopt, false,
            "put a break point before deoptimizing a stub")
DEFINE_BOOL(deoptimize_uncommon_cases, true, "deoptimize uncommon cases")
DEFINE_BOOL(polymorphic_inlining, true, "polymorphic inlining")
DEFINE_BOOL(use_osr, true, "use on-stack replacement")
DEFINE_BOOL(array_bounds_checks_elimination, true,
            "perform array bounds checks elimination")
DEFINE_BOOL(trace_bce, false, "trace array bounds check elimination")
DEFINE_BOOL(array_bounds_checks_hoisting, false,
            "perform array bounds checks hoisting")
DEFINE_BOOL(array_index_dehoisting, true, "perform array index dehoisting")
DEFINE_BOOL(analyze_environment_liveness, true,
            "analyze liveness of environment slots and zap dead values")
DEFINE_BOOL(load_elimination, true, "use load elimination")
DEFINE_BOOL(check_elimination, true, "use check elimination")
DEFINE_BOOL(store_elimination, false, "use store elimination")
DEFINE_BOOL(dead_code_elimination, true, "use dead code elimination")
DEFINE_BOOL(fold_constants, true, "use constant folding")
DEFINE_BOOL(trace_dead_code_elimination, false, "trace dead code elimination")
DEFINE_BOOL(unreachable_code_elimination, true, "eliminate unreachable code")
DEFINE_BOOL(trace_osr, false, "trace on-stack replacement")
DEFINE_INT(stress_runs, 0, "number of stress runs")
DEFINE_BOOL(lookup_sample_by_shared, true,
            "when picking a function to optimize, watch for shared function "
            "info, not JSFunction itself")
DEFINE_BOOL(cache_optimized_code, true, "cache optimized code for closures")
DEFINE_BOOL(flush_optimized_code_cache, true,
            "flushes the cache of optimized code for closures on every GC")
DEFINE_BOOL(inline_construct, true, "inline constructor calls")
DEFINE_BOOL(inline_arguments, true, "inline functions with arguments object")
DEFINE_BOOL(inline_accessors, true, "inline JavaScript accessors")
DEFINE_INT(escape_analysis_iterations, 2,
           "maximum number of escape analysis fix-point iterations")

DEFINE_BOOL(optimize_for_in, true, "optimize functions containing for-in loops")

DEFINE_BOOL(concurrent_recompilation, true,
            "optimizing hot functions asynchronously on a separate thread")
DEFINE_BOOL(job_based_recompilation, true,
            "post tasks to v8::Platform instead of using a thread for "
            "concurrent recompilation")
DEFINE_BOOL(trace_concurrent_recompilation, false,
            "track concurrent recompilation")
DEFINE_INT(concurrent_recompilation_queue_length, 8,
           "the length of the concurrent compilation queue")
DEFINE_INT(concurrent_recompilation_delay, 0,
           "artificial compilation delay in ms")
DEFINE_BOOL(block_concurrent_recompilation, false,
            "block queued jobs until released")
DEFINE_BOOL(concurrent_osr, true, "concurrent on-stack replacement")
DEFINE_IMPLICATION(concurrent_osr, concurrent_recompilation)

DEFINE_BOOL(omit_map_checks_for_leaf_maps, true,
            "do not emit check maps for constant values that have a leaf map, "
            "deoptimize the optimized code if the layout of the maps changes.")

// Flags for TurboFan.
DEFINE_STRING(turbo_filter, "~", "optimization filter for TurboFan compiler")
DEFINE_BOOL(trace_turbo, false, "trace generated TurboFan IR")
DEFINE_BOOL(trace_turbo_graph, false, "trace generated TurboFan graphs")
DEFINE_IMPLICATION(trace_turbo_graph, trace_turbo)
DEFINE_STRING(trace_turbo_cfg_file, NULL,
              "trace turbo cfg graph (for C1 visualizer) to a given file name")
DEFINE_BOOL(trace_turbo_types, true, "trace TurboFan's types")
DEFINE_BOOL(trace_turbo_scheduler, false, "trace TurboFan's scheduler")
DEFINE_BOOL(trace_turbo_reduction, false, "trace TurboFan's various reducers")
DEFINE_BOOL(trace_turbo_jt, false, "trace TurboFan's jump threading")
DEFINE_BOOL(turbo_asm, true, "enable TurboFan for asm.js code")
DEFINE_BOOL(turbo_verify, DEBUG_BOOL, "verify TurboFan graphs at each phase")
DEFINE_BOOL(turbo_stats, false, "print TurboFan statistics")
DEFINE_BOOL(turbo_splitting, true, "split nodes during scheduling in TurboFan")
DEFINE_BOOL(turbo_types, true, "use typed lowering in TurboFan")
DEFINE_BOOL(turbo_type_feedback, false, "use type feedback in TurboFan")
DEFINE_BOOL(turbo_source_positions, false,
            "track source code positions when building TurboFan IR")
DEFINE_IMPLICATION(trace_turbo, turbo_source_positions)
DEFINE_BOOL(context_specialization, false,
            "enable context specialization in TurboFan")
DEFINE_BOOL(turbo_deoptimization, false, "enable deoptimization in TurboFan")
DEFINE_BOOL(turbo_inlining, false, "enable inlining in TurboFan")
DEFINE_BOOL(turbo_builtin_inlining, true, "enable builtin inlining in TurboFan")
DEFINE_BOOL(trace_turbo_inlining, false, "trace TurboFan inlining")
DEFINE_BOOL(loop_assignment_analysis, true, "perform loop assignment analysis")
DEFINE_BOOL(turbo_profiling, false, "enable profiling in TurboFan")
// TODO(dcarney): this is just for experimentation, remove when default.
DEFINE_BOOL(turbo_delay_ssa_decon, false,
            "delay ssa deconstruction in TurboFan register allocator")
DEFINE_BOOL(turbo_verify_allocation, DEBUG_BOOL,
            "verify register allocation in TurboFan")
DEFINE_BOOL(turbo_move_optimization, true, "optimize gap moves in TurboFan")
DEFINE_BOOL(turbo_jt, true, "enable jump threading in TurboFan")
DEFINE_BOOL(turbo_osr, true, "enable OSR in TurboFan")
DEFINE_BOOL(turbo_exceptions, false, "enable exception handling in TurboFan")
DEFINE_BOOL(turbo_stress_loop_peeling, false,
            "stress loop peeling optimization")
DEFINE_BOOL(turbo_cf_optimization, true, "optimize control flow in TurboFan")

DEFINE_INT(typed_array_max_size_in_heap, 64,
           "threshold for in-heap typed array")

// Profiler flags.
DEFINE_INT(frame_count, 1, "number of stack frames inspected by the profiler")
// 0x1800 fits in the immediate field of an ARM instruction.
DEFINE_INT(interrupt_budget, 0x1800,
           "execution budget before interrupt is triggered")
DEFINE_INT(type_info_threshold, 25,
           "percentage of ICs that must have type info to allow optimization")
DEFINE_INT(generic_ic_threshold, 30,
           "max percentage of megamorphic/generic ICs to allow optimization")
DEFINE_INT(self_opt_count, 130, "call count before self-optimization")

DEFINE_BOOL(trace_opt_verbose, false, "extra verbose compilation tracing")
DEFINE_IMPLICATION(trace_opt_verbose, trace_opt)

// assembler-ia32.cc / assembler-arm.cc / assembler-x64.cc
DEFINE_BOOL(debug_code, false, "generate extra code (assertions) for debugging")
DEFINE_BOOL(code_comments, false, "emit comments in code disassembly")
DEFINE_BOOL(enable_sse3, true, "enable use of SSE3 instructions if available")
DEFINE_BOOL(enable_sse4_1, true,
            "enable use of SSE4.1 instructions if available")
DEFINE_BOOL(enable_sahf, true,
            "enable use of SAHF instruction if available (X64 only)")
DEFINE_BOOL(enable_avx, true, "enable use of AVX instructions if available")
DEFINE_BOOL(enable_fma3, true, "enable use of FMA3 instructions if available")
DEFINE_BOOL(enable_vfp3, ENABLE_VFP3_DEFAULT,
            "enable use of VFP3 instructions if available")
DEFINE_BOOL(enable_armv7, ENABLE_ARMV7_DEFAULT,
            "enable use of ARMv7 instructions if available (ARM only)")
DEFINE_BOOL(enable_armv8, ENABLE_ARMV8_DEFAULT,
            "enable use of ARMv8 instructions if available (ARM 32-bit only)")
DEFINE_BOOL(enable_neon, ENABLE_NEON_DEFAULT,
            "enable use of NEON instructions if available (ARM only)")
DEFINE_BOOL(enable_sudiv, true,
            "enable use of SDIV and UDIV instructions if available (ARM only)")
DEFINE_BOOL(enable_mls, true,
            "enable use of MLS instructions if available (ARM only)")
DEFINE_BOOL(enable_movw_movt, false,
            "enable loading 32-bit constant by means of movw/movt "
            "instruction pairs (ARM only)")
DEFINE_BOOL(enable_unaligned_accesses, true,
            "enable unaligned accesses for ARMv7 (ARM only)")
DEFINE_BOOL(enable_32dregs, ENABLE_32DREGS_DEFAULT,
            "enable use of d16-d31 registers on ARM - this requires VFP3")
DEFINE_BOOL(enable_vldr_imm, false,
            "enable use of constant pools for double immediate (ARM only)")
DEFINE_BOOL(force_long_branches, false,
            "force all emitted branches to be in long mode (MIPS/PPC only)")
DEFINE_STRING(mcpu, "auto", "enable optimization for specific cpu")

// bootstrapper.cc
DEFINE_STRING(expose_natives_as, NULL, "expose natives in global object")
DEFINE_STRING(expose_debug_as, NULL, "expose debug in global object")
DEFINE_BOOL(expose_free_buffer, false, "expose freeBuffer extension")
DEFINE_BOOL(expose_gc, false, "expose gc extension")
DEFINE_STRING(expose_gc_as, NULL,
              "expose gc extension under the specified name")
DEFINE_IMPLICATION(expose_gc_as, expose_gc)
DEFINE_BOOL(expose_externalize_string, false,
            "expose externalize string extension")
DEFINE_BOOL(expose_trigger_failure, false, "expose trigger-failure extension")
DEFINE_INT(stack_trace_limit, 10, "number of stack frames to capture")
DEFINE_BOOL(builtins_in_stack_traces, false,
            "show built-in functions in stack traces")
DEFINE_BOOL(disable_native_files, false, "disable builtin natives files")

// builtins-ia32.cc
DEFINE_BOOL(inline_new, true, "use fast inline allocation")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_BOOL(trace_codegen, false,
            "print name of functions for which code is generated")
DEFINE_BOOL(trace, false, "trace function calls")
DEFINE_BOOL(mask_constants_with_cookie, true,
            "use random jit cookie to mask large constants")

// codegen.cc
DEFINE_BOOL(lazy, true, "use lazy compilation")
DEFINE_BOOL(trace_opt, false, "trace lazy optimization")
DEFINE_BOOL(trace_opt_stats, false, "trace lazy optimization statistics")
DEFINE_BOOL(opt, true, "use adaptive optimizations")
DEFINE_BOOL(always_opt, false, "always try to optimize functions")
DEFINE_BOOL(always_osr, false, "always try to OSR functions")
DEFINE_BOOL(prepare_always_opt, false, "prepare for turning on always opt")
DEFINE_BOOL(trace_deopt, false, "trace optimize function deoptimization")
DEFINE_BOOL(trace_stub_failures, false,
            "trace deoptimization of generated code stubs")

DEFINE_BOOL(serialize_toplevel, true, "enable caching of toplevel scripts")
DEFINE_BOOL(serialize_inner, true, "enable caching of inner functions")
DEFINE_BOOL(trace_serializer, false, "print code serializer trace")

// compiler.cc
DEFINE_INT(min_preparse_length, 1024,
           "minimum length for automatic enable preparsing")
DEFINE_INT(max_opt_count, 10,
           "maximum number of optimization attempts before giving up.")

// compilation-cache.cc
DEFINE_BOOL(compilation_cache, true, "enable compilation cache")

DEFINE_BOOL(cache_prototype_transitions, true, "cache prototype transitions")

// cpu-profiler.cc
DEFINE_INT(cpu_profiler_sampling_interval, 1000,
           "CPU profiler sampling interval in microseconds")

// debug.cc
DEFINE_BOOL(trace_debug_json, false, "trace debugging JSON request/response")
DEFINE_BOOL(trace_js_array_abuse, false,
            "trace out-of-bounds accesses to JS arrays")
DEFINE_BOOL(trace_external_array_abuse, false,
            "trace out-of-bounds-accesses to external arrays")
DEFINE_BOOL(trace_array_abuse, false,
            "trace out-of-bounds accesses to all arrays")
DEFINE_IMPLICATION(trace_array_abuse, trace_js_array_abuse)
DEFINE_IMPLICATION(trace_array_abuse, trace_external_array_abuse)
DEFINE_BOOL(enable_liveedit, true, "enable liveedit experimental feature")
DEFINE_BOOL(hard_abort, true, "abort by crashing")

// execution.cc
DEFINE_INT(stack_size, V8_DEFAULT_STACK_SIZE_KB,
           "default size of stack region v8 is allowed to use (in kBytes)")

// frames.cc
DEFINE_INT(max_stack_trace_source_length, 300,
           "maximum length of function source code printed in a stack trace.")

// full-codegen.cc
DEFINE_BOOL(always_inline_smi_code, false,
            "always inline smi code in non-opt code")

// heap.cc
DEFINE_INT(min_semi_space_size, 0,
           "min size of a semi-space (in MBytes), the new space consists of two"
           "semi-spaces")
DEFINE_INT(target_semi_space_size, 0,
           "target size of a semi-space (in MBytes) before triggering a GC")
DEFINE_INT(max_semi_space_size, 0,
           "max size of a semi-space (in MBytes), the new space consists of two"
           "semi-spaces")
DEFINE_INT(semi_space_growth_factor, 2, "factor by which to grow the new space")
DEFINE_BOOL(experimental_new_space_growth_heuristic, false,
            "Grow the new space based on the percentage of survivors instead "
            "of their absolute value.")
DEFINE_INT(max_old_space_size, 0, "max size of the old space (in Mbytes)")
DEFINE_INT(initial_old_space_size, 0, "initial old space size (in Mbytes)")
DEFINE_INT(max_executable_size, 0, "max size of executable memory (in Mbytes)")
DEFINE_BOOL(gc_global, false, "always perform global GCs")
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
DEFINE_BOOL(print_cumulative_gc_stat, false,
            "print cumulative GC statistics in name=value format on exit")
DEFINE_BOOL(print_max_heap_committed, false,
            "print statistics of the maximum memory committed for the heap "
            "in name=value format on exit")
DEFINE_BOOL(trace_gc_verbose, false,
            "print more details following each garbage collection")
DEFINE_BOOL(trace_fragmentation, false, "report fragmentation for old space")
DEFINE_BOOL(trace_fragmentation_verbose, false,
            "report fragmentation for old space (detailed)")
DEFINE_BOOL(collect_maps, true,
            "garbage collect maps from which no objects can be reached")
DEFINE_BOOL(weak_embedded_maps_in_optimized_code, true,
            "make maps embedded in optimized code weak")
DEFINE_BOOL(weak_embedded_objects_in_optimized_code, true,
            "make objects embedded in optimized code weak")
DEFINE_BOOL(flush_code, true,
            "flush code that we expect not to use again (during full gc)")
DEFINE_BOOL(flush_code_incrementally, true,
            "flush code that we expect not to use again (incrementally)")
DEFINE_BOOL(trace_code_flushing, false, "trace code flushing progress")
DEFINE_BOOL(age_code, true,
            "track un-executed functions to age code and flush only "
            "old code (required for code flushing)")
DEFINE_BOOL(incremental_marking, true, "use incremental marking")
DEFINE_BOOL(incremental_marking_steps, true, "do incremental marking steps")
DEFINE_BOOL(overapproximate_weak_closure, true,
            "overapproximate weak closer to reduce atomic pause time")
DEFINE_INT(min_progress_during_object_groups_marking, 128,
           "keep overapproximating the weak closure as long as we discover at "
           "least this many unmarked objects")
DEFINE_INT(max_object_groups_marking_rounds, 3,
           "at most try this many times to over approximate the weak closure")
DEFINE_BOOL(concurrent_sweeping, true, "use concurrent sweeping")
DEFINE_BOOL(trace_incremental_marking, false,
            "trace progress of the incremental marking")
DEFINE_BOOL(track_gc_object_stats, false,
            "track object counts and memory usage")
DEFINE_BOOL(track_detached_contexts, true,
            "track native contexts that are expected to be garbage collected")
DEFINE_BOOL(trace_detached_contexts, false,
            "trace native contexts that are expected to be garbage collected")
DEFINE_IMPLICATION(trace_detached_contexts, track_detached_contexts)
#ifdef VERIFY_HEAP
DEFINE_BOOL(verify_heap, false, "verify heap pointers before and after GC")
#endif


// heap-snapshot-generator.cc
DEFINE_BOOL(heap_profiler_trace_objects, false,
            "Dump heap object allocations/movements/size_updates")


// v8.cc
DEFINE_BOOL(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")
// ic.cc
DEFINE_BOOL(use_ic, true, "use inline caching")
DEFINE_BOOL(trace_ic, false, "trace inline cache state transitions")

// macro-assembler-ia32.cc
DEFINE_BOOL(native_code_counters, false,
            "generate extra code for manipulating stats counters")

// mark-compact.cc
DEFINE_BOOL(always_compact, false, "Perform compaction on every full GC")
DEFINE_BOOL(never_compact, false,
            "Never perform compaction on full GC - testing only")
DEFINE_BOOL(compact_code_space, true,
            "Compact code space on full non-incremental collections")
DEFINE_BOOL(incremental_code_compaction, true,
            "Compact code space on full incremental collections")
DEFINE_BOOL(cleanup_code_caches_at_gc, true,
            "Flush inline caches prior to mark compact collection and "
            "flush code caches in maps during mark compact cycle.")
DEFINE_BOOL(use_marking_progress_bar, true,
            "Use a progress bar to scan large objects in increments when "
            "incremental marking is active.")
DEFINE_BOOL(zap_code_space, true,
            "Zap free memory in code space with 0xCC while sweeping.")
DEFINE_INT(random_seed, 0,
           "Default seed for initializing random generator "
           "(0, the default, means to use system random).")

// objects.cc
DEFINE_BOOL(trace_weak_arrays, false, "trace WeakFixedArray usage")
DEFINE_BOOL(track_prototype_users, false,
            "keep track of which maps refer to a given prototype object")
DEFINE_BOOL(use_verbose_printer, true, "allows verbose printing")
#if TRACE_MAPS
DEFINE_BOOL(trace_maps, false, "trace map creation")
#endif

// parser.cc
DEFINE_BOOL(allow_natives_syntax, false, "allow natives syntax")
DEFINE_BOOL(trace_parse, false, "trace parsing and preparsing")

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
           "Stack size of the ARM64 and MIPS64 simulator "
           "in kBytes (default is 2 MB)")
DEFINE_BOOL(log_regs_modified, true,
            "When logging register values, only print modified registers.")
DEFINE_BOOL(log_colour, true, "When logging, try to use coloured output.")
DEFINE_BOOL(ignore_asm_unimplemented_break, false,
            "Don't break for ASM_UNIMPLEMENTED_BREAK macros.")
DEFINE_BOOL(trace_sim_messages, false,
            "Trace simulator debug messages. Implied by --trace-sim.")

// isolate.cc
DEFINE_BOOL(stack_trace_on_illegal, false,
            "print stack trace when an illegal exception is thrown")
DEFINE_BOOL(abort_on_uncaught_exception, false,
            "abort program (dump core) when an uncaught exception is thrown")
DEFINE_BOOL(randomize_hashes, true,
            "randomize hashes to avoid predictable hash collisions "
            "(with snapshots this option cannot override the baked-in seed)")
DEFINE_INT(hash_seed, 0,
           "Fixed seed to use to hash property keys (0 means random)"
           "(with snapshots this option cannot override the baked-in seed)")

// snapshot-common.cc
DEFINE_BOOL(profile_deserialization, false,
            "Print the time it takes to deserialize the snapshot.")

// Regexp
DEFINE_BOOL(regexp_optimization, true, "generate optimized regexp code")

// Testing flags test/cctest/test-{flags,api,serialization}.cc
DEFINE_BOOL(testing_bool_flag, true, "testing_bool_flag")
DEFINE_MAYBE_BOOL(testing_maybe_bool_flag, "testing_maybe_bool_flag")
DEFINE_INT(testing_int_flag, 13, "testing_int_flag")
DEFINE_FLOAT(testing_float_flag, 2.5, "float-flag")
DEFINE_STRING(testing_string_flag, "Hello, world!", "string-flag")
DEFINE_INT(testing_prng_seed, 42, "Seed used for threading test randomness")
#ifdef _WIN32
DEFINE_STRING(testing_serialization_file, "C:\\Windows\\Temp\\serdes",
              "file in which to testing_serialize heap")
#else
DEFINE_STRING(testing_serialization_file, "/tmp/serdes",
              "file in which to serialize heap")
#endif

// mksnapshot.cc
DEFINE_STRING(startup_blob, NULL,
              "Write V8 startup blob file. (mksnapshot only)")

// code-stubs-hydrogen.cc
DEFINE_BOOL(profile_hydrogen_code_stub_compilation, false,
            "Print the time it takes to lazily compile hydrogen code stubs.")

DEFINE_BOOL(predictable, false, "enable predictable mode")
DEFINE_NEG_IMPLICATION(predictable, concurrent_recompilation)
DEFINE_NEG_IMPLICATION(predictable, concurrent_osr)
DEFINE_NEG_IMPLICATION(predictable, concurrent_sweeping)

// mark-compact.cc
DEFINE_BOOL(force_marking_deque_overflows, false,
            "force overflows of marking deque by reducing it's size "
            "to 64 words")

DEFINE_BOOL(stress_compaction, false,
            "stress the GC compactor to flush out bugs (implies "
            "--force_marking_deque_overflows)")

DEFINE_BOOL(manual_evacuation_candidates_selection, false,
            "Test mode only flag. It allows an unit test to select evacuation "
            "candidates pages (requires --stress_compaction).")


//
// Dev shell flags
//

DEFINE_BOOL(help, false, "Print usage message, including flags, on console")
DEFINE_BOOL(dump_counters, false, "Dump counters on exit")

DEFINE_BOOL(debugger, false, "Enable JavaScript debugger")

DEFINE_STRING(map_counters, "", "Map counters to a file")
DEFINE_ARGS(js_arguments,
            "Pass all remaining arguments to the script. Alias for \"--\".")

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
DEFINE_BOOL(enable_slow_asserts, false,
            "enable asserts that are slow to execute")
#endif

// codegen-ia32.cc / codegen-arm.cc / macro-assembler-*.cc
DEFINE_BOOL(print_source, false, "pretty print source code")
DEFINE_BOOL(print_builtin_source, false,
            "pretty print source code for builtins")
DEFINE_BOOL(print_ast, false, "print source AST")
DEFINE_BOOL(print_builtin_ast, false, "print source AST for builtins")
DEFINE_STRING(stop_at, "", "function name where to insert a breakpoint")
DEFINE_BOOL(trap_on_abort, false, "replace aborts by breakpoints")

// compiler.cc
DEFINE_BOOL(print_builtin_scopes, false, "print scopes for builtins")
DEFINE_BOOL(print_scopes, false, "print scopes")

// contexts.cc
DEFINE_BOOL(trace_contexts, false, "trace contexts operations")

// heap.cc
DEFINE_BOOL(gc_verbose, false, "print stuff during garbage collection")
DEFINE_BOOL(heap_stats, false, "report heap statistics before and after GC")
DEFINE_BOOL(code_stats, false, "report code statistics after GC")
DEFINE_BOOL(print_handles, false, "report handles after GC")
DEFINE_BOOL(print_global_handles, false, "report global handles after GC")

// TurboFan debug-only flags.
DEFINE_BOOL(print_turbo_replay, false,
            "print C++ code to recreate TurboFan graphs")

// objects.cc
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
DEFINE_BOOL(trace_regexp_bytecodes, false, "trace regexp bytecode execution")
DEFINE_BOOL(trace_regexp_assembler, false,
            "trace regexp macro assembler calls.")

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
DEFINE_BOOL(log_gc, false,
            "Log heap samples on garbage collection for the hp2ps tool.")
DEFINE_BOOL(log_handles, false, "Log global handle events.")
DEFINE_BOOL(log_snapshot_positions, false,
            "log positions of (de)serialized objects in the snapshot.")
DEFINE_BOOL(log_suspect, false, "Log suspect operations.")
DEFINE_BOOL(prof, false,
            "Log statistical profiling information (implies --log-code).")
DEFINE_BOOL(prof_cpp, false, "Like --prof, but ignore generated code.")
DEFINE_IMPLICATION(prof, prof_cpp)
DEFINE_BOOL(prof_browser_mode, true,
            "Used with --prof, turns on browser-compatible mode for profiling.")
DEFINE_BOOL(log_regexp, false, "Log regular expression execution.")
DEFINE_STRING(logfile, "v8.log", "Specify the name of the log file.")
DEFINE_BOOL(logfile_per_isolate, true, "Separate log files for each isolate.")
DEFINE_BOOL(ll_prof, false, "Enable low-level linux profiler.")
DEFINE_BOOL(perf_basic_prof, false,
            "Enable perf linux profiler (basic support).")
DEFINE_NEG_IMPLICATION(perf_basic_prof, compact_code_space)
DEFINE_BOOL(perf_jit_prof, false,
            "Enable perf linux profiler (experimental annotate support).")
DEFINE_NEG_IMPLICATION(perf_jit_prof, compact_code_space)
DEFINE_STRING(gc_fake_mmap, "/tmp/__v8_gc__",
              "Specify the name of the file for fake gc mmap used in ll_prof")
DEFINE_BOOL(log_internal_timer_events, false, "Time internal events.")
DEFINE_BOOL(log_timer_events, false,
            "Time events including external callbacks.")
DEFINE_IMPLICATION(log_timer_events, log_internal_timer_events)
DEFINE_IMPLICATION(log_internal_timer_events, prof)
DEFINE_BOOL(log_instruction_stats, false, "Log AArch64 instruction statistics.")
DEFINE_STRING(log_instruction_file, "arm64_inst.csv",
              "AArch64 instruction statistics log file.")
DEFINE_INT(log_instruction_period, 1 << 22,
           "AArch64 instruction statistics logging period.")

DEFINE_BOOL(redirect_code_traces, false,
            "output deopt information and disassembly into file "
            "code-<pid>-<isolate id>.asm")
DEFINE_STRING(redirect_code_traces_to, NULL,
              "output deopt information and disassembly into the given file")

DEFINE_BOOL(hydrogen_track_positions, false,
            "track source code positions when building IR")

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

// code-stubs.cc
DEFINE_BOOL(print_code_stubs, false, "print code stubs")
DEFINE_BOOL(test_secondary_stub_cache, false,
            "test secondary stub cache by disabling the primary one")

DEFINE_BOOL(test_primary_stub_cache, false,
            "test primary stub cache by disabling the secondary one")


// codegen-ia32.cc / codegen-arm.cc
DEFINE_BOOL(print_code, false, "print generated code")
DEFINE_BOOL(print_opt_code, false, "print optimized code")
DEFINE_BOOL(print_unopt_code, false,
            "print unoptimized code before "
            "printing optimized code based on it")
DEFINE_BOOL(print_code_verbose, false, "print more information for code")
DEFINE_BOOL(print_builtin_code, false, "print generated code for builtins")

#ifdef ENABLE_DISASSEMBLER
DEFINE_BOOL(sodium, false,
            "print generated code output suitable for use with "
            "the Sodium code viewer")

DEFINE_IMPLICATION(sodium, print_code_stubs)
DEFINE_IMPLICATION(sodium, print_code)
DEFINE_IMPLICATION(sodium, print_opt_code)
DEFINE_IMPLICATION(sodium, hydrogen_track_positions)
DEFINE_IMPLICATION(sodium, code_comments)

DEFINE_BOOL(print_all_code, false, "enable all flags related to printing code")
DEFINE_IMPLICATION(print_all_code, print_code)
DEFINE_IMPLICATION(print_all_code, print_opt_code)
DEFINE_IMPLICATION(print_all_code, print_unopt_code)
DEFINE_IMPLICATION(print_all_code, print_code_verbose)
DEFINE_IMPLICATION(print_all_code, print_builtin_code)
DEFINE_IMPLICATION(print_all_code, print_code_stubs)
DEFINE_IMPLICATION(print_all_code, code_comments)
#ifdef DEBUG
DEFINE_IMPLICATION(print_all_code, trace_codegen)
#endif
#endif


//
// VERIFY_PREDICTABLE related flags
//
#undef FLAG

#ifdef VERIFY_PREDICTABLE
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

DEFINE_BOOL(verify_predictable, false,
            "this mode is used for checking that V8 behaves predictably")
DEFINE_INT(dump_allocations_digest_at_alloc, 0,
           "dump allocations digest each n-th allocation")


//
// Read-only flags
//
#undef FLAG
#define FLAG FLAG_READONLY

// assembler.h
DEFINE_BOOL(enable_ool_constant_pool, V8_OOL_CONSTANT_POOL,
            "enable use of out-of-line constant pools (ARM only)")

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
#undef DEFINE_INT
#undef DEFINE_STRING
#undef DEFINE_FLOAT
#undef DEFINE_ARGS
#undef DEFINE_IMPLICATION
#undef DEFINE_NEG_IMPLICATION
#undef DEFINE_NEG_VALUE_IMPLICATION
#undef DEFINE_VALUE_IMPLICATION
#undef DEFINE_ALIAS_BOOL
#undef DEFINE_ALIAS_INT
#undef DEFINE_ALIAS_STRING
#undef DEFINE_ALIAS_FLOAT
#undef DEFINE_ALIAS_ARGS

#undef FLAG_MODE_DECLARE
#undef FLAG_MODE_DEFINE
#undef FLAG_MODE_DEFINE_DEFAULTS
#undef FLAG_MODE_META
#undef FLAG_MODE_DEFINE_IMPLICATIONS

#undef COMMA
