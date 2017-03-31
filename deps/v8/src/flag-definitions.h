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
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  V8_EXPORT_PRIVATE extern ctype FLAG_##nam;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt) \
  static ctype const FLAG_##nam = def;

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
# define ENABLE_LOG_COLOUR false
#else
# define ENABLE_LOG_COLOUR true
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

DEFINE_BOOL(experimental_extras, false,
            "enable code compiled in via v8_experimental_extra_library_files")

// Flags for language modes and experimental language features.
DEFINE_BOOL(use_strict, false, "enforce strict mode")

DEFINE_BOOL(es_staging, false,
            "enable test-worthy harmony features (for internal use only)")
DEFINE_BOOL(harmony, false, "enable all completed harmony features")
DEFINE_BOOL(harmony_shipping, true, "enable all shipped harmony features")
DEFINE_IMPLICATION(es_staging, harmony)


// Activate on ClusterFuzz.
DEFINE_IMPLICATION(es_staging, harmony_regexp_lookbehind)
DEFINE_IMPLICATION(es_staging, move_object_start)

// Features that are still work in progress (behind individual flags).
#define HARMONY_INPROGRESS(V)                                           \
  V(harmony_array_prototype_values, "harmony Array.prototype.values")   \
  V(harmony_function_sent, "harmony function.sent")                     \
  V(harmony_sharedarraybuffer, "harmony sharedarraybuffer")             \
  V(harmony_simd, "harmony simd")                                       \
  V(harmony_do_expressions, "harmony do-expressions")                   \
  V(harmony_regexp_named_captures, "harmony regexp named captures")     \
  V(harmony_regexp_property, "harmony unicode regexp property classes") \
  V(harmony_class_fields, "harmony public fields in class literals")    \
  V(harmony_object_spread, "harmony object spread")

// Features that are complete (but still behind --harmony/es-staging flag).
#define HARMONY_STAGED_BASE(V)                              \
  V(harmony_regexp_lookbehind, "harmony regexp lookbehind") \
  V(harmony_restrictive_generators,                         \
    "harmony restrictions on generator declarations")       \
  V(harmony_tailcalls, "harmony tail calls")                \
  V(harmony_trailing_commas,                                \
    "harmony trailing commas in function parameter lists")

#ifdef V8_I18N_SUPPORT
#define HARMONY_STAGED(V)                                          \
  HARMONY_STAGED_BASE(V)                                           \
  V(icu_case_mapping, "case mapping with ICU rather than Unibrow")
#else
#define HARMONY_STAGED(V) HARMONY_STAGED_BASE(V)
#endif

// Features that are shipping (turned on by default, but internal flag remains).
#define HARMONY_SHIPPING_BASE(V)

#ifdef V8_I18N_SUPPORT
#define HARMONY_SHIPPING(V) \
  HARMONY_SHIPPING_BASE(V)  \
  V(datetime_format_to_parts, "Intl.DateTimeFormat.formatToParts")
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

#define FLAG_STAGED_FEATURES(id, description) \
  DEFINE_BOOL(id, false, "enable " #description) \
  DEFINE_IMPLICATION(harmony, id)
HARMONY_STAGED(FLAG_STAGED_FEATURES)
#undef FLAG_STAGED_FEATURES

#define FLAG_SHIPPING_FEATURES(id, description) \
  DEFINE_BOOL(id, true, "enable " #description) \
  DEFINE_NEG_NEG_IMPLICATION(harmony_shipping, id)
HARMONY_SHIPPING(FLAG_SHIPPING_FEATURES)
#undef FLAG_SHIPPING_FEATURES

DEFINE_BOOL(future, false,
            "Implies all staged features that we want to ship in the "
            "not-too-far future")
DEFINE_IMPLICATION(future, ignition_staging)

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
DEFINE_BOOL(mark_shared_functions_for_tier_up, false,
            "mark shared functions for tier up")

// Flags for strongly rooting literal arrays in the feedback vector.
DEFINE_BOOL(trace_strong_rooted_literals, false, "trace literal rooting")

// Flags for optimization types.
DEFINE_BOOL(optimize_for_size, false,
            "Enables optimizations which favor memory size over execution "
            "speed")

DEFINE_VALUE_IMPLICATION(optimize_for_size, max_semi_space_size, 1)

// Flags for data representation optimizations
DEFINE_BOOL(unbox_double_arrays, true, "automatically unbox arrays of doubles")
DEFINE_BOOL(string_slices, true, "use string slices")

// Flags for Ignition.
DEFINE_BOOL(ignition, false, "use ignition interpreter")
DEFINE_BOOL(ignition_staging, false, "use ignition with all staged features")
DEFINE_IMPLICATION(ignition_staging, ignition)
DEFINE_IMPLICATION(ignition_staging, compiler_dispatcher)
DEFINE_STRING(ignition_filter, "*", "filter for ignition interpreter")
DEFINE_BOOL(ignition_deadcode, true,
            "use ignition dead code elimination optimizer")
DEFINE_BOOL(ignition_osr, true, "enable support for OSR from ignition code")
DEFINE_BOOL(ignition_peephole, true, "use ignition peephole optimizer")
DEFINE_BOOL(ignition_reo, true, "use ignition register equivalence optimizer")
DEFINE_BOOL(ignition_filter_expression_positions, true,
            "filter expression positions before the bytecode pipeline")
DEFINE_BOOL(print_bytecode, false,
            "print bytecode generated by ignition interpreter")
DEFINE_BOOL(trace_ignition, false,
            "trace the bytecodes executed by the ignition interpreter")
DEFINE_BOOL(trace_ignition_codegen, false,
            "trace the codegen of ignition interpreter bytecode handlers")
DEFINE_BOOL(trace_ignition_dispatches, false,
            "traces the dispatches to bytecode handlers by the ignition "
            "interpreter")
DEFINE_STRING(trace_ignition_dispatches_output_file, nullptr,
              "the file to which the bytecode handler dispatch table is "
              "written (by default, the table is not written to a file)")

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
DEFINE_INT(max_inlined_nodes, 200,
           "maximum number of AST nodes considered for a single inlining")
DEFINE_INT(max_inlined_nodes_cumulative, 400,
           "maximum cumulative number of AST nodes considered for inlining")
DEFINE_BOOL(loop_invariant_code_motion, true, "loop invariant code motion")
DEFINE_BOOL(fast_math, true, "faster (but maybe less accurate) math functions")
DEFINE_BOOL(collect_megamorphic_maps_from_stub_cache, false,
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
DEFINE_BOOL(turbo_verify_store_elimination, false,
            "verify store elimination more rigorously")
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
DEFINE_BOOL(flush_optimized_code_cache, false,
            "flushes the cache of optimized code for closures on every GC")
DEFINE_BOOL(inline_construct, true, "inline constructor calls")
DEFINE_BOOL(inline_arguments, true, "inline functions with arguments object")
DEFINE_BOOL(inline_accessors, true, "inline JavaScript accessors")
DEFINE_BOOL(inline_into_try, true, "inline into try blocks")
DEFINE_INT(escape_analysis_iterations, 1,
           "maximum number of escape analysis fix-point iterations")

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

DEFINE_BOOL(omit_map_checks_for_leaf_maps, true,
            "do not emit check maps for constant values that have a leaf map, "
            "deoptimize the optimized code if the layout of the maps changes.")

// Flags for TurboFan.
DEFINE_BOOL(turbo, false, "enable TurboFan compiler")
DEFINE_BOOL(turbo_sp_frame_access, false,
            "use stack pointer-relative access to frame wherever possible")
DEFINE_BOOL(turbo_preprocess_ranges, true,
            "run pre-register allocation heuristics")
DEFINE_BOOL(turbo_loop_stackcheck, true, "enable stack checks in loops")
DEFINE_STRING(turbo_filter, "~~", "optimization filter for TurboFan compiler")
DEFINE_BOOL(trace_turbo, false, "trace generated TurboFan IR")
DEFINE_BOOL(trace_turbo_graph, false, "trace generated TurboFan graphs")
DEFINE_IMPLICATION(trace_turbo_graph, trace_turbo)
DEFINE_STRING(trace_turbo_cfg_file, NULL,
              "trace turbo cfg graph (for C1 visualizer) to a given file name")
DEFINE_BOOL(trace_turbo_types, true, "trace TurboFan's types")
DEFINE_BOOL(trace_turbo_scheduler, false, "trace TurboFan's scheduler")
DEFINE_BOOL(trace_turbo_reduction, false, "trace TurboFan's various reducers")
DEFINE_BOOL(trace_turbo_trimming, false, "trace TurboFan's graph trimmer")
DEFINE_BOOL(trace_turbo_jt, false, "trace TurboFan's jump threading")
DEFINE_BOOL(trace_turbo_ceq, false, "trace TurboFan's control equivalence")
DEFINE_BOOL(trace_turbo_loop, false, "trace TurboFan's loop optimizations")
DEFINE_BOOL(turbo_asm, true, "enable TurboFan for asm.js code")
DEFINE_BOOL(turbo_verify, DEBUG_BOOL, "verify TurboFan graphs at each phase")
DEFINE_STRING(turbo_verify_machine_graph, nullptr,
              "verify TurboFan machine graph before instruction selection")
DEFINE_BOOL(csa_verify, DEBUG_BOOL,
            "verify TurboFan machine graph of code stubs")
DEFINE_BOOL(trace_csa_verify, false, "trace code stubs verification")
DEFINE_STRING(csa_trap_on_node, nullptr,
              "trigger break point when a node with given id is created in "
              "given stub. The format is: StubName,NodeId")
DEFINE_BOOL(turbo_stats, false, "print TurboFan statistics")
DEFINE_BOOL(turbo_stats_nvp, false,
            "print TurboFan statistics in machine-readable format")
DEFINE_BOOL(turbo_splitting, true, "split nodes during scheduling in TurboFan")
DEFINE_BOOL(function_context_specialization, false,
            "enable function context specialization in TurboFan")
DEFINE_BOOL(turbo_inlining, true, "enable inlining in TurboFan")
DEFINE_BOOL(trace_turbo_inlining, false, "trace TurboFan inlining")
DEFINE_BOOL(turbo_load_elimination, true, "enable load elimination in TurboFan")
DEFINE_BOOL(trace_turbo_load_elimination, false,
            "trace TurboFan load elimination")
DEFINE_BOOL(loop_assignment_analysis, true, "perform loop assignment analysis")
DEFINE_BOOL(turbo_profiling, false, "enable profiling in TurboFan")
DEFINE_BOOL(turbo_verify_allocation, DEBUG_BOOL,
            "verify register allocation in TurboFan")
DEFINE_BOOL(turbo_move_optimization, true, "optimize gap moves in TurboFan")
DEFINE_BOOL(turbo_jt, true, "enable jump threading in TurboFan")
DEFINE_BOOL(turbo_loop_peeling, true, "Turbofan loop peeling")
DEFINE_BOOL(turbo_loop_variable, true, "Turbofan loop variable optimization")
DEFINE_BOOL(turbo_cf_optimization, true, "optimize control flow in TurboFan")
DEFINE_BOOL(turbo_frame_elision, true, "elide frames in TurboFan")
DEFINE_BOOL(turbo_escape, true, "enable escape analysis")
DEFINE_BOOL(turbo_instruction_scheduling, false,
            "enable instruction scheduling in TurboFan")
DEFINE_BOOL(turbo_stress_instruction_scheduling, false,
            "randomly schedule instructions to stress dependency tracking")
DEFINE_BOOL(turbo_store_elimination, true,
            "enable store-store elimination in TurboFan")
DEFINE_BOOL(turbo_lower_create_closure, false,
            "enable inline allocation for closure instantiation")

// Flags to help platform porters
DEFINE_BOOL(minimal, false,
            "simplifies execution model to make porting "
            "easier (e.g. always use Ignition, never use Crankshaft")
DEFINE_IMPLICATION(minimal, ignition)
DEFINE_NEG_IMPLICATION(minimal, crankshaft)
DEFINE_NEG_IMPLICATION(minimal, use_ic)

// Flags for native WebAssembly.
DEFINE_BOOL(expose_wasm, true, "expose WASM interface to JavaScript")
DEFINE_BOOL(wasm_disable_structured_cloning, false,
            "disable WASM structured cloning")
DEFINE_INT(wasm_num_compilation_tasks, 10,
           "number of parallel compilation tasks for wasm")
DEFINE_BOOL(trace_wasm_encoder, false, "trace encoding of wasm code")
DEFINE_BOOL(trace_wasm_decoder, false, "trace decoding of wasm code")
DEFINE_BOOL(trace_wasm_decode_time, false, "trace decoding time of wasm code")
DEFINE_BOOL(trace_wasm_compiler, false, "trace compiling of wasm code")
DEFINE_BOOL(trace_wasm_interpreter, false, "trace interpretation of wasm code")
DEFINE_INT(trace_wasm_ast_start, 0,
           "start function for WASM AST trace (inclusive)")
DEFINE_INT(trace_wasm_ast_end, 0, "end function for WASM AST trace (exclusive)")
DEFINE_INT(trace_wasm_text_start, 0,
           "start function for WASM text generation (inclusive)")
DEFINE_INT(trace_wasm_text_end, 0,
           "end function for WASM text generation (exclusive)")
DEFINE_INT(skip_compiling_wasm_funcs, 0, "start compiling at function N")
DEFINE_BOOL(wasm_break_on_decoder_error, false,
            "debug break when wasm decoder encounters an error")
DEFINE_BOOL(wasm_loop_assignment_analysis, true,
            "perform loop assignment analysis for WASM")

DEFINE_BOOL(validate_asm, false, "validate asm.js modules before compiling")
DEFINE_IMPLICATION(ignition_staging, validate_asm)
DEFINE_BOOL(suppress_asm_messages, false,
            "don't emit asm.js related messages (for golden file testing)")
DEFINE_BOOL(trace_asm_time, false, "log asm.js timing info to the console")

DEFINE_BOOL(dump_wasm_module, false, "dump WASM module bytes")
DEFINE_STRING(dump_wasm_module_path, NULL, "directory to dump wasm modules to")

DEFINE_INT(typed_array_max_size_in_heap, 64,
           "threshold for in-heap typed array")

DEFINE_BOOL(wasm_simd_prototype, false,
            "enable prototype simd opcodes for wasm")
DEFINE_BOOL(wasm_eh_prototype, false,
            "enable prototype exception handling opcodes for wasm")
DEFINE_BOOL(wasm_mv_prototype, false,
            "enable prototype multi-value support for wasm")
DEFINE_BOOL(wasm_atomics_prototype, false,
            "enable prototype atomic opcodes for wasm")

DEFINE_BOOL(wasm_opt, true, "enable wasm optimization")
DEFINE_BOOL(wasm_no_bounds_checks, false,
            "disable bounds checks (performance testing only)")
DEFINE_BOOL(wasm_no_stack_checks, false,
            "disable stack checks (performance testing only)")

DEFINE_BOOL(wasm_trap_handler, false,
            "use signal handlers to catch out of bounds memory access in wasm"
            " (experimental, currently Linux x86_64 only)")
DEFINE_BOOL(wasm_guard_pages, false,
            "add guard pages to the end of WebWassembly memory"
            " (experimental, no effect on 32-bit)")
DEFINE_IMPLICATION(wasm_trap_handler, wasm_guard_pages)
DEFINE_BOOL(wasm_trap_if, false,
            "enable the use of the trap_if operator for traps")
DEFINE_BOOL(wasm_code_fuzzer_gen_test, false,
            "Generate a test case when running the wasm-code fuzzer")
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
DEFINE_BOOL(debug_code, DEBUG_BOOL,
            "generate extra code (assertions) for debugging")
DEFINE_BOOL(code_comments, false, "emit comments in code disassembly")
DEFINE_BOOL(enable_sse3, true, "enable use of SSE3 instructions if available")
DEFINE_BOOL(enable_ssse3, true, "enable use of SSSE3 instructions if available")
DEFINE_BOOL(enable_sse4_1, true,
            "enable use of SSE4.1 instructions if available")
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
DEFINE_BOOL(enable_vldr_imm, false,
            "enable use of constant pools for double immediate (ARM only)")
DEFINE_BOOL(force_long_branches, false,
            "force all emitted branches to be in long mode (MIPS/PPC only)")
DEFINE_STRING(mcpu, "auto", "enable optimization for specific cpu")

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

// builtins.cc
DEFINE_BOOL(allow_unsafe_function_constructor, false,
            "allow invoking the function constructor without security checks")

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
DEFINE_BOOL(trace_file_names, false,
            "include file names in trace-opt/trace-deopt output")
DEFINE_BOOL(opt, true, "use adaptive optimizations")
DEFINE_BOOL(always_opt, false, "always try to optimize functions")
DEFINE_BOOL(always_osr, false, "always try to OSR functions")
DEFINE_BOOL(prepare_always_opt, false, "prepare for turning on always opt")
DEFINE_BOOL(trace_deopt, false, "trace optimize function deoptimization")
DEFINE_BOOL(trace_stub_failures, false,
            "trace deoptimization of generated code stubs")

DEFINE_BOOL(serialize_toplevel, true, "enable caching of toplevel scripts")
DEFINE_BOOL(serialize_eager, false, "compile eagerly when caching scripts")
DEFINE_BOOL(serialize_age_code, false, "pre age code in the code cache")
DEFINE_BOOL(trace_serializer, false, "print code serializer trace")
#ifdef DEBUG
DEFINE_BOOL(external_reference_stats, false,
            "print statistics on external references used during serialization")
#endif  // DEBUG

// compiler.cc
DEFINE_INT(max_opt_count, 10,
           "maximum number of optimization attempts before giving up.")

// compilation-cache.cc
DEFINE_BOOL(compilation_cache, true, "enable compilation cache")

DEFINE_BOOL(cache_prototype_transitions, true, "cache prototype transitions")

// compiler-dispatcher.cc
DEFINE_BOOL(compiler_dispatcher, false, "enable compiler dispatcher")
DEFINE_BOOL(trace_compiler_dispatcher, false,
            "trace compiler dispatcher activity")

// compiler-dispatcher-job.cc
DEFINE_BOOL(
    trace_compiler_dispatcher_jobs, false,
    "trace progress of individual jobs managed by the compiler dispatcher")

// cpu-profiler.cc
DEFINE_INT(cpu_profiler_sampling_interval, 1000,
           "CPU profiler sampling interval in microseconds")

// Array abuse tracing
DEFINE_BOOL(trace_js_array_abuse, false,
            "trace out-of-bounds accesses to JS arrays")
DEFINE_BOOL(trace_external_array_abuse, false,
            "trace out-of-bounds-accesses to external arrays")
DEFINE_BOOL(trace_array_abuse, false,
            "trace out-of-bounds accesses to all arrays")
DEFINE_IMPLICATION(trace_array_abuse, trace_js_array_abuse)
DEFINE_IMPLICATION(trace_array_abuse, trace_external_array_abuse)

// debugger
DEFINE_BOOL(trace_debug_json, false, "trace debugging JSON request/response")
DEFINE_BOOL(enable_liveedit, true, "enable liveedit experimental feature")
DEFINE_BOOL(side_effect_free_debug_evaluate, false,
            "use side-effect-free debug-evaluate for testing")
DEFINE_BOOL(
    trace_side_effect_free_debug_evaluate, false,
    "print debug messages for side-effect-free debug-evaluate for testing")
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
DEFINE_BOOL(verify_operand_stack_depth, false,
            "emit debug code that verifies the static tracking of the operand "
            "stack depth")

// heap.cc
DEFINE_INT(min_semi_space_size, 0,
           "min size of a semi-space (in MBytes), the new space consists of two"
           "semi-spaces")
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
DEFINE_BOOL(print_max_heap_committed, false,
            "print statistics of the maximum memory committed for the heap "
            "in name=value format on exit")
DEFINE_BOOL(trace_gc_verbose, false,
            "print more details following each garbage collection")
DEFINE_INT(trace_allocation_stack_interval, -1,
           "print stack trace after <n> free-list allocations")
DEFINE_BOOL(trace_fragmentation, false, "report fragmentation for old space")
DEFINE_BOOL(trace_fragmentation_verbose, false,
            "report fragmentation for old space (detailed)")
DEFINE_BOOL(trace_evacuation, false, "report evacuation statistics")
DEFINE_BOOL(trace_mutator_utilization, false,
            "print mutator utilization, allocation speed, gc speed")
DEFINE_BOOL(weak_embedded_maps_in_optimized_code, true,
            "make maps embedded in optimized code weak")
DEFINE_BOOL(weak_embedded_objects_in_optimized_code, true,
            "make objects embedded in optimized code weak")
DEFINE_BOOL(flush_code, true, "flush code that we expect not to use again")
DEFINE_BOOL(trace_code_flushing, false, "trace code flushing progress")
DEFINE_BOOL(age_code, true,
            "track un-executed functions to age code and flush only "
            "old code (required for code flushing)")
DEFINE_BOOL(incremental_marking, true, "use incremental marking")
DEFINE_BOOL(incremental_marking_wrappers, true,
            "use incremental marking for marking wrappers")
DEFINE_BOOL(object_grouping_in_incremental_finalization, true,
            "enable object grouping in incremental finalization")
DEFINE_INT(min_progress_during_incremental_marking_finalization, 32,
           "keep finalizing incremental marking as long as we discover at "
           "least this many unmarked objects")
DEFINE_INT(max_incremental_marking_finalization_rounds, 3,
           "at most try this many times to finalize incremental marking")
DEFINE_BOOL(minor_mc, false, "perform young generation mark compact GCs")
DEFINE_NEG_IMPLICATION(minor_mc, incremental_marking)
DEFINE_BOOL(black_allocation, true, "use black allocation")
DEFINE_BOOL(concurrent_sweeping, true, "use concurrent sweeping")
DEFINE_BOOL(parallel_compaction, true, "use parallel compaction")
DEFINE_BOOL(parallel_pointer_update, true,
            "use parallel pointer update during compaction")
DEFINE_BOOL(trace_incremental_marking, false,
            "trace progress of the incremental marking")
DEFINE_BOOL(track_gc_object_stats, false,
            "track object counts and memory usage")
DEFINE_BOOL(trace_gc_object_stats, false,
            "trace object counts and memory usage")
DEFINE_INT(gc_stats, 0, "Used by tracing internally to enable gc statistics")
DEFINE_IMPLICATION(trace_gc_object_stats, track_gc_object_stats)
DEFINE_VALUE_IMPLICATION(track_gc_object_stats, gc_stats, 1)
DEFINE_VALUE_IMPLICATION(trace_gc_object_stats, gc_stats, 1)
DEFINE_NEG_IMPLICATION(trace_gc_object_stats, incremental_marking)
DEFINE_BOOL(track_detached_contexts, true,
            "track native contexts that are expected to be garbage collected")
DEFINE_BOOL(trace_detached_contexts, false,
            "trace native contexts that are expected to be garbage collected")
DEFINE_IMPLICATION(trace_detached_contexts, track_detached_contexts)
#ifdef VERIFY_HEAP
DEFINE_BOOL(verify_heap, false, "verify heap pointers before and after GC")
#endif
DEFINE_BOOL(move_object_start, true, "enable moving of object starts")
DEFINE_BOOL(memory_reducer, true, "use memory reducer")
DEFINE_INT(heap_growing_percent, 0,
           "specifies heap growing factor as (1 + heap_growing_percent/100)")

// spaces.cc
DEFINE_INT(v8_os_page_size, 0, "override OS page size (in KBytes)")

// execution.cc, messages.cc
DEFINE_BOOL(clear_exceptions_on_js_entry, false,
            "clear pending exceptions when entering JavaScript")

// counters.cc
DEFINE_INT(histogram_interval, 600000,
           "time interval in ms for aggregating memory histograms")

// global-handles.cc
DEFINE_BOOL(trace_object_groups, false,
            "print object groups detected during each garbage collection")

// heap-snapshot-generator.cc
DEFINE_BOOL(heap_profiler_trace_objects, false,
            "Dump heap object allocations/movements/size_updates")


// sampling-heap-profiler.cc
DEFINE_BOOL(sampling_heap_profiler_suppress_randomness, false,
            "Use constant sample intervals to eliminate test flakiness")


// v8.cc
DEFINE_BOOL(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")
// ic.cc
DEFINE_BOOL(use_ic, true, "use inline caching")
DEFINE_BOOL(trace_ic, false, "trace inline cache state transitions")
DEFINE_INT(ic_stats, 0, "inline cache state transitions statistics")
DEFINE_VALUE_IMPLICATION(trace_ic, ic_stats, 1)

// macro-assembler-ia32.cc
DEFINE_BOOL(native_code_counters, false,
            "generate extra code for manipulating stats counters")

// mark-compact.cc
DEFINE_BOOL(always_compact, false, "Perform compaction on every full GC")
DEFINE_BOOL(never_compact, false,
            "Never perform compaction on full GC - testing only")
DEFINE_BOOL(compact_code_space, true, "Compact code space on full collections")
DEFINE_BOOL(cleanup_code_caches_at_gc, true,
            "Flush inline caches prior to mark compact collection and "
            "flush code caches in maps during mark compact cycle.")
DEFINE_BOOL(use_marking_progress_bar, true,
            "Use a progress bar to scan large objects in increments when "
            "incremental marking is active.")
DEFINE_BOOL(zap_code_space, DEBUG_BOOL,
            "Zap free memory in code space with 0xCC while sweeping.")
DEFINE_INT(random_seed, 0,
           "Default seed for initializing random generator "
           "(0, the default, means to use system random).")

// objects.cc
DEFINE_BOOL(trace_weak_arrays, false, "Trace WeakFixedArray usage")
DEFINE_BOOL(trace_prototype_users, false,
            "Trace updates to prototype user tracking")
DEFINE_BOOL(use_verbose_printer, true, "allows verbose printing")
DEFINE_BOOL(trace_for_in_enumerate, false, "Trace for-in enumerate slow-paths")
#if TRACE_MAPS
DEFINE_BOOL(trace_maps, false, "trace map creation")
#endif

// parser.cc
DEFINE_BOOL(allow_natives_syntax, false, "allow natives syntax")
DEFINE_BOOL(trace_parse, false, "trace parsing and preparsing")
DEFINE_BOOL(trace_preparse, false, "trace preparsing decisions")
DEFINE_BOOL(lazy_inner_functions, false, "enable lazy parsing inner functions")
DEFINE_BOOL(aggressive_lazy_inner_functions, false,
            "even lazier inner function parsing")
DEFINE_IMPLICATION(aggressive_lazy_inner_functions, lazy_inner_functions)

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
DEFINE_BOOL(log_regs_modified, true,
            "When logging register values, only print modified registers.")
DEFINE_BOOL(log_colour, ENABLE_LOG_COLOUR,
            "When logging, try to use coloured output.")
DEFINE_BOOL(ignore_asm_unimplemented_break, false,
            "Don't break for ASM_UNIMPLEMENTED_BREAK macros.")
DEFINE_BOOL(trace_sim_messages, false,
            "Trace simulator debug messages. Implied by --trace-sim.")

// isolate.cc
DEFINE_BOOL(stack_trace_on_illegal, false,
            "print stack trace when an illegal exception is thrown")
DEFINE_BOOL(abort_on_uncaught_exception, false,
            "abort program (dump core) when an uncaught exception is thrown")
DEFINE_BOOL(abort_on_stack_overflow, false,
            "Abort program when stack overflow (as opposed to throwing "
            "RangeError). This is useful for fuzzing where the spec behaviour "
            "would introduce nondeterminism.")
DEFINE_BOOL(randomize_hashes, true,
            "randomize hashes to avoid predictable hash collisions "
            "(with snapshots this option cannot override the baked-in seed)")
DEFINE_INT(hash_seed, 0,
           "Fixed seed to use to hash property keys (0 means random)"
           "(with snapshots this option cannot override the baked-in seed)")
DEFINE_BOOL(trace_rail, false, "trace RAIL mode")
DEFINE_BOOL(print_all_exceptions, false,
            "print exception object and stack trace on each thrown exception")

// runtime.cc
DEFINE_BOOL(runtime_call_stats, false, "report runtime call counts and times")
DEFINE_INT(runtime_stats, 0,
           "internal usage only for controlling runtime statistics")
DEFINE_VALUE_IMPLICATION(runtime_call_stats, runtime_stats, 1)

// snapshot-common.cc
DEFINE_BOOL(profile_deserialization, false,
            "Print the time it takes to deserialize the snapshot.")
DEFINE_BOOL(serialization_statistics, false,
            "Collect statistics on serialized objects.")

// Regexp
DEFINE_BOOL(regexp_optimization, true, "generate optimized regexp code")

// Testing flags test/cctest/test-{flags,api,serialization}.cc
DEFINE_BOOL(testing_bool_flag, true, "testing_bool_flag")
DEFINE_MAYBE_BOOL(testing_maybe_bool_flag, "testing_maybe_bool_flag")
DEFINE_INT(testing_int_flag, 13, "testing_int_flag")
DEFINE_FLOAT(testing_float_flag, 2.5, "float-flag")
DEFINE_STRING(testing_string_flag, "Hello, world!", "string-flag")
DEFINE_INT(testing_prng_seed, 42, "Seed used for threading test randomness")

// mksnapshot.cc
DEFINE_STRING(startup_src, NULL,
              "Write V8 startup as C++ src. (mksnapshot only)")
DEFINE_STRING(startup_blob, NULL,
              "Write V8 startup blob file. (mksnapshot only)")

// code-stubs-hydrogen.cc
DEFINE_BOOL(profile_hydrogen_code_stub_compilation, false,
            "Print the time it takes to lazily compile hydrogen code stubs.")

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

DEFINE_BOOL(disable_old_api_accessors, false,
            "Disable old-style API accessors whose setters trigger through the "
            "prototype chain")

//
// Dev shell flags
//

DEFINE_BOOL(help, false, "Print usage message, including flags, on console")
DEFINE_BOOL(dump_counters, false, "Dump counters on exit")
DEFINE_BOOL(dump_counters_nvp, false,
            "Dump counters as name-value pairs on exit")

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
DEFINE_BOOL(print_ast, false, "print source AST")
DEFINE_BOOL(print_builtin_ast, false, "print source AST for builtins")
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
DEFINE_BOOL(check_handle_count, false,
            "Check that there are not too many handles at GC")
DEFINE_BOOL(print_global_handles, false, "report global handles after GC")

// TurboFan debug-only flags.
DEFINE_BOOL(print_turbo_replay, false,
            "print C++ code to recreate TurboFan graphs")
DEFINE_BOOL(trace_turbo_escape, false, "enable tracing in escape analysis")

// objects.cc
DEFINE_BOOL(trace_normalization, false,
            "prints when objects are turned into dictionaries.")

// runtime.cc
DEFINE_BOOL(trace_lazy, false, "trace lazy compilation")

// spaces.cc
DEFINE_BOOL(collect_heap_spill_statistics, false,
            "report heap spill statistics along with heap_stats "
            "(requires heap_stats)")
DEFINE_BOOL(trace_live_bytes, false,
            "trace incrementing and resetting of live bytes")
DEFINE_BOOL(trace_isolates, false, "trace isolate state changes")

// Regexp
DEFINE_BOOL(regexp_possessive_quantifier, false,
            "enable possessive quantifier syntax for testing")
DEFINE_BOOL(trace_regexp_bytecodes, false, "trace regexp bytecode execution")
DEFINE_BOOL(trace_regexp_assembler, false,
            "trace regexp macro assembler calls.")
DEFINE_BOOL(trace_regexp_parser, false, "trace regexp parsing")

// Debugger
DEFINE_BOOL(print_break_location, false, "print source location on debug break")

// wasm instance management
DEFINE_BOOL(trace_wasm_instances, false,
            "trace creation and collection of wasm instances")

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
DEFINE_BOOL(log_suspect, false, "Log suspect operations.")
DEFINE_BOOL(prof, false,
            "Log statistical profiling information (implies --log-code).")
DEFINE_BOOL(prof_cpp, false, "Like --prof, but ignore generated code.")
DEFINE_IMPLICATION(prof, prof_cpp)
DEFINE_BOOL(prof_browser_mode, true,
            "Used with --prof, turns on browser-compatible mode for profiling.")
DEFINE_STRING(logfile, "v8.log", "Specify the name of the log file.")
DEFINE_BOOL(logfile_per_isolate, true, "Separate log files for each isolate.")
DEFINE_BOOL(ll_prof, false, "Enable low-level linux profiler.")
DEFINE_BOOL(perf_basic_prof, false,
            "Enable perf linux profiler (basic support).")
DEFINE_NEG_IMPLICATION(perf_basic_prof, compact_code_space)
DEFINE_BOOL(perf_basic_prof_only_functions, false,
            "Only report function code ranges to perf (i.e. no stubs).")
DEFINE_IMPLICATION(perf_basic_prof_only_functions, perf_basic_prof)
DEFINE_BOOL(perf_prof, false,
            "Enable perf linux profiler (experimental annotate support).")
DEFINE_NEG_IMPLICATION(perf_prof, compact_code_space)
DEFINE_BOOL(perf_prof_unwinding_info, false,
            "Enable unwinding info for perf linux profiler (experimental).")
DEFINE_IMPLICATION(perf_prof, perf_prof_unwinding_info)
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

DEFINE_BOOL(print_opt_source, false,
            "print source code of optimized and inlined functions")
DEFINE_IMPLICATION(hydrogen_track_positions, print_opt_source)

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

DEFINE_BOOL(test_small_max_function_context_stub_size, false,
            "enable testing the function context size overflow path "
            "by making the maximum size smaller")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_BOOL(print_code, false, "print generated code")
DEFINE_BOOL(print_opt_code, false, "print optimized code")
DEFINE_STRING(print_opt_code_filter, "*", "filter for printing optimized code")
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

#undef FLAG
#define FLAG FLAG_FULL

//
// Predictable mode related flags.
//

DEFINE_BOOL(predictable, false, "enable predictable mode")
DEFINE_IMPLICATION(predictable, single_threaded)
DEFINE_NEG_IMPLICATION(predictable, memory_reducer)
DEFINE_VALUE_IMPLICATION(single_threaded, wasm_num_compilation_tasks, 0)

//
// Threading related flags.
//

DEFINE_BOOL(single_threaded, false, "disable the use of background tasks")
DEFINE_NEG_IMPLICATION(single_threaded, concurrent_recompilation)
DEFINE_NEG_IMPLICATION(single_threaded, concurrent_sweeping)
DEFINE_NEG_IMPLICATION(single_threaded, parallel_compaction)


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
            "enable use of embedded constant pools (ARM/PPC only)")

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
