// Copyright 2012 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file defines all of the flags.  It is separated into different section,
// for Debug, Release, Logging and Profiling, etc.  To add a new flag, find the
// correct section, and use one of the DEFINE_ macros, without a trailing ';'.
//
// This include does not have a guard, because it is a template-style include,
// which can be included multiple times in different modes.  It expects to have
// a mode defined before it's included.  The modes are FLAG_MODE_... below:

// We want to declare the names of the variables for the header file.  Normally
// this will just be an extern declaration, but for a readonly flag we let the
// compiler make better optimizations by giving it the value.
#if defined(FLAG_MODE_DECLARE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  extern ctype FLAG_##nam;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt) \
  static ctype const FLAG_##nam = def;
#define DEFINE_implication(whenflag, thenflag)

// We want to supply the actual storage and value for the flag variable in the
// .cc file.  We only do this for writable flags.
#elif defined(FLAG_MODE_DEFINE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  ctype FLAG_##nam = def;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)
#define DEFINE_implication(whenflag, thenflag)

// We need to define all of our default values so that the Flag structure can
// access them by pointer.  These are just used internally inside of one .cc,
// for MODE_META, so there is no impact on the flags interface.
#elif defined(FLAG_MODE_DEFINE_DEFAULTS)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  static ctype const FLAGDEFAULT_##nam = def;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)
#define DEFINE_implication(whenflag, thenflag)

// We want to write entries into our meta data table, for internal parsing and
// printing / etc in the flag parser code.  We only do this for writable flags.
#elif defined(FLAG_MODE_META)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  { Flag::TYPE_##ftype, #nam, &FLAG_##nam, &FLAGDEFAULT_##nam, cmt, false },
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)
#define DEFINE_implication(whenflag, thenflag)

// We produce the code to set flags when it is implied by another flag.
#elif defined(FLAG_MODE_DEFINE_IMPLICATIONS)
#define FLAG_FULL(ftype, ctype, nam, def, cmt)
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)
#define DEFINE_implication(whenflag, thenflag) \
  if (FLAG_##whenflag) FLAG_##thenflag = true;

#else
#error No mode supplied when including flags.defs
#endif

#ifdef FLAG_MODE_DECLARE
// Structure used to hold a collection of arguments to the JavaScript code.
#define JSARGUMENTS_INIT {{}}
struct JSArguments {
public:
  inline int argc() const {
    return static_cast<int>(storage_[0]);
  }
  inline const char** argv() const {
    return reinterpret_cast<const char**>(storage_[1]);
  }
  inline const char*& operator[] (int idx) const {
    return argv()[idx];
  }
  inline JSArguments& operator=(JSArguments args) {
    set_argc(args.argc());
    set_argv(args.argv());
    return *this;
  }
  static JSArguments Create(int argc, const char** argv) {
    JSArguments args;
    args.set_argc(argc);
    args.set_argv(argv);
    return args;
  }
private:
  void set_argc(int argc) {
    storage_[0] = argc;
  }
  void set_argv(const char** argv) {
    storage_[1] = reinterpret_cast<AtomicWord>(argv);
  }
public:
  // Contains argc and argv. Unfortunately we have to store these two fields
  // into a single one to avoid making the initialization macro (which would be
  // "{ 0, NULL }") contain a coma.
  AtomicWord storage_[2];
};
#endif

#define DEFINE_bool(nam, def, cmt) FLAG(BOOL, bool, nam, def, cmt)
#define DEFINE_int(nam, def, cmt) FLAG(INT, int, nam, def, cmt)
#define DEFINE_float(nam, def, cmt) FLAG(FLOAT, double, nam, def, cmt)
#define DEFINE_string(nam, def, cmt) FLAG(STRING, const char*, nam, def, cmt)
#define DEFINE_args(nam, def, cmt) FLAG(ARGS, JSArguments, nam, def, cmt)

//
// Flags in all modes.
//
#define FLAG FLAG_FULL

// Flags for language modes and experimental language features.
DEFINE_bool(use_strict, false, "enforce strict mode")
DEFINE_bool(es52_globals, false,
            "activate new semantics for global var declarations")

DEFINE_bool(harmony_typeof, false, "enable harmony semantics for typeof")
DEFINE_bool(harmony_scoping, false, "enable harmony block scoping")
DEFINE_bool(harmony_modules, false,
            "enable harmony modules (implies block scoping)")
DEFINE_bool(harmony_proxies, false, "enable harmony proxies")
DEFINE_bool(harmony_collections, false,
            "enable harmony collections (sets, maps, and weak maps)")
DEFINE_bool(harmony, false, "enable all harmony features (except typeof)")
DEFINE_implication(harmony, harmony_scoping)
DEFINE_implication(harmony, harmony_modules)
DEFINE_implication(harmony, harmony_proxies)
DEFINE_implication(harmony, harmony_collections)
DEFINE_implication(harmony_modules, harmony_scoping)

// Flags for experimental implementation features.
DEFINE_bool(smi_only_arrays, true, "tracks arrays with only smi values")
DEFINE_bool(clever_optimizations,
            true,
            "Optimize object size, Array shift, DOM strings and string +")

// Flags for data representation optimizations
DEFINE_bool(unbox_double_arrays, true, "automatically unbox arrays of doubles")
DEFINE_bool(string_slices, true, "use string slices")

// Flags for Crankshaft.
DEFINE_bool(crankshaft, true, "use crankshaft")
DEFINE_string(hydrogen_filter, "", "optimization filter")
DEFINE_bool(use_range, true, "use hydrogen range analysis")
DEFINE_bool(eliminate_dead_phis, true, "eliminate dead phis")
DEFINE_bool(use_gvn, true, "use hydrogen global value numbering")
DEFINE_bool(use_canonicalizing, true, "use hydrogen instruction canonicalizing")
DEFINE_bool(use_inlining, true, "use function inlining")
DEFINE_int(max_inlined_source_size, 600,
           "maximum source size in bytes considered for a single inlining")
DEFINE_int(max_inlined_nodes, 196,
           "maximum number of AST nodes considered for a single inlining")
DEFINE_int(max_inlined_nodes_cumulative, 196,
           "maximum cumulative number of AST nodes considered for inlining")
DEFINE_bool(loop_invariant_code_motion, true, "loop invariant code motion")
DEFINE_bool(collect_megamorphic_maps_from_stub_cache,
            true,
            "crankshaft harvests type feedback from stub cache")
DEFINE_bool(hydrogen_stats, false, "print statistics for hydrogen")
DEFINE_bool(trace_hydrogen, false, "trace generated hydrogen to file")
DEFINE_string(trace_phase, "Z", "trace generated IR for specified phases")
DEFINE_bool(trace_inlining, false, "trace inlining decisions")
DEFINE_bool(trace_alloc, false, "trace register allocator")
DEFINE_bool(trace_all_uses, false, "trace all use positions")
DEFINE_bool(trace_range, false, "trace range analysis")
DEFINE_bool(trace_gvn, false, "trace global value numbering")
DEFINE_bool(trace_representation, false, "trace representation types")
DEFINE_bool(stress_pointer_maps, false, "pointer map for every instruction")
DEFINE_bool(stress_environments, false, "environment for every instruction")
DEFINE_int(deopt_every_n_times,
           0,
           "deoptimize every n times a deopt point is passed")
DEFINE_bool(trap_on_deopt, false, "put a break point before deoptimizing")
DEFINE_bool(deoptimize_uncommon_cases, true, "deoptimize uncommon cases")
DEFINE_bool(polymorphic_inlining, true, "polymorphic inlining")
DEFINE_bool(use_osr, true, "use on-stack replacement")
DEFINE_bool(array_bounds_checks_elimination, true,
            "perform array bounds checks elimination")

DEFINE_bool(trace_osr, false, "trace on-stack replacement")
DEFINE_int(stress_runs, 0, "number of stress runs")
DEFINE_bool(optimize_closures, true, "optimize closures")
DEFINE_bool(inline_construct, true, "inline constructor calls")
DEFINE_bool(inline_arguments, true, "inline functions with arguments object")
DEFINE_int(loop_weight, 1, "loop weight for representation inference")

DEFINE_bool(optimize_for_in, true,
            "optimize functions containing for-in loops")

// Experimental profiler changes.
DEFINE_bool(experimental_profiler, true, "enable all profiler experiments")
DEFINE_bool(watch_ic_patching, false, "profiler considers IC stability")
DEFINE_int(frame_count, 1, "number of stack frames inspected by the profiler")
DEFINE_bool(self_optimization, false,
            "primitive functions trigger their own optimization")
DEFINE_bool(direct_self_opt, false,
            "call recompile stub directly when self-optimizing")
DEFINE_bool(retry_self_opt, false, "re-try self-optimization if it failed")
DEFINE_bool(count_based_interrupts, false,
            "trigger profiler ticks based on counting instead of timing")
DEFINE_bool(interrupt_at_exit, false,
            "insert an interrupt check at function exit")
DEFINE_bool(weighted_back_edges, false,
            "weight back edges by jump distance for interrupt triggering")
DEFINE_int(interrupt_budget, 5900,
           "execution budget before interrupt is triggered")
DEFINE_int(type_info_threshold, 15,
           "percentage of ICs that must have type info to allow optimization")
DEFINE_int(self_opt_count, 130, "call count before self-optimization")

DEFINE_implication(experimental_profiler, watch_ic_patching)
DEFINE_implication(experimental_profiler, self_optimization)
// Not implying direct_self_opt here because it seems to be a bad idea.
DEFINE_implication(experimental_profiler, retry_self_opt)
DEFINE_implication(experimental_profiler, count_based_interrupts)
DEFINE_implication(experimental_profiler, interrupt_at_exit)
DEFINE_implication(experimental_profiler, weighted_back_edges)

DEFINE_bool(trace_opt_verbose, false, "extra verbose compilation tracing")
DEFINE_implication(trace_opt_verbose, trace_opt)

// assembler-ia32.cc / assembler-arm.cc / assembler-x64.cc
DEFINE_bool(debug_code, false,
            "generate extra code (assertions) for debugging")
DEFINE_bool(code_comments, false, "emit comments in code disassembly")
DEFINE_bool(enable_sse2, true,
            "enable use of SSE2 instructions if available")
DEFINE_bool(enable_sse3, true,
            "enable use of SSE3 instructions if available")
DEFINE_bool(enable_sse4_1, true,
            "enable use of SSE4.1 instructions if available")
DEFINE_bool(enable_cmov, true,
            "enable use of CMOV instruction if available")
DEFINE_bool(enable_rdtsc, true,
            "enable use of RDTSC instruction if available")
DEFINE_bool(enable_sahf, true,
            "enable use of SAHF instruction if available (X64 only)")
DEFINE_bool(enable_vfp3, true,
            "enable use of VFP3 instructions if available - this implies "
            "enabling ARMv7 instructions (ARM only)")
DEFINE_bool(enable_armv7, true,
            "enable use of ARMv7 instructions if available (ARM only)")
DEFINE_bool(enable_fpu, true,
            "enable use of MIPS FPU instructions if available (MIPS only)")

// bootstrapper.cc
DEFINE_string(expose_natives_as, NULL, "expose natives in global object")
DEFINE_string(expose_debug_as, NULL, "expose debug in global object")
DEFINE_bool(expose_gc, false, "expose gc extension")
DEFINE_bool(expose_externalize_string, false,
            "expose externalize string extension")
DEFINE_int(stack_trace_limit, 10, "number of stack frames to capture")
DEFINE_bool(builtins_in_stack_traces, false,
            "show built-in functions in stack traces")
DEFINE_bool(disable_native_files, false, "disable builtin natives files")

// builtins-ia32.cc
DEFINE_bool(inline_new, true, "use fast inline allocation")

// checks.cc
DEFINE_bool(stack_trace_on_abort, true,
            "print a stack trace if an assertion failure occurs")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(trace, false, "trace function calls")
DEFINE_bool(mask_constants_with_cookie,
            true,
            "use random jit cookie to mask large constants")

// codegen.cc
DEFINE_bool(lazy, true, "use lazy compilation")
DEFINE_bool(trace_opt, false, "trace lazy optimization")
DEFINE_bool(trace_opt_stats, false, "trace lazy optimization statistics")
DEFINE_bool(opt, true, "use adaptive optimizations")
DEFINE_bool(always_opt, false, "always try to optimize functions")
DEFINE_bool(prepare_always_opt, false, "prepare for turning on always opt")
DEFINE_bool(trace_deopt, false, "trace deoptimization")

// compiler.cc
DEFINE_int(min_preparse_length, 1024,
           "minimum length for automatic enable preparsing")
DEFINE_bool(always_full_compiler, false,
            "try to use the dedicated run-once backend for all code")
DEFINE_bool(trace_bailout, false,
            "print reasons for falling back to using the classic V8 backend")

// compilation-cache.cc
DEFINE_bool(compilation_cache, true, "enable compilation cache")

DEFINE_bool(cache_prototype_transitions, true, "cache prototype transitions")

// debug.cc
DEFINE_bool(trace_debug_json, false, "trace debugging JSON request/response")
DEFINE_bool(debugger_auto_break, true,
            "automatically set the debug break flag when debugger commands are "
            "in the queue")
DEFINE_bool(enable_liveedit, true, "enable liveedit experimental feature")
DEFINE_bool(break_on_abort, true, "always cause a debug break before aborting")

// execution.cc
// Slightly less than 1MB on 64-bit, since Windows' default stack size for
// the main execution thread is 1MB for both 32 and 64-bit.
DEFINE_int(stack_size, kPointerSize * 123,
           "default size of stack region v8 is allowed to use (in kBytes)")

// frames.cc
DEFINE_int(max_stack_trace_source_length, 300,
           "maximum length of function source code printed in a stack trace.")

// full-codegen.cc
DEFINE_bool(always_inline_smi_code, false,
            "always inline smi code in non-opt code")

// heap.cc
DEFINE_int(max_new_space_size, 0, "max size of the new generation (in kBytes)")
DEFINE_int(max_old_space_size, 0, "max size of the old generation (in Mbytes)")
DEFINE_int(max_executable_size, 0, "max size of executable memory (in Mbytes)")
DEFINE_bool(gc_global, false, "always perform global GCs")
DEFINE_int(gc_interval, -1, "garbage collect after <n> allocations")
DEFINE_bool(trace_gc, false,
            "print one trace line following each garbage collection")
DEFINE_bool(trace_gc_nvp, false,
            "print one detailed trace line in name=value format "
            "after each garbage collection")
DEFINE_bool(print_cumulative_gc_stat, false,
            "print cumulative GC statistics in name=value format on exit")
DEFINE_bool(trace_gc_verbose, false,
            "print more details following each garbage collection")
DEFINE_bool(trace_fragmentation, false,
            "report fragmentation for old pointer and data pages")
DEFINE_bool(collect_maps, true,
            "garbage collect maps from which no objects can be reached")
DEFINE_bool(flush_code, true,
            "flush code that we expect not to use again before full gc")
DEFINE_bool(incremental_marking, true, "use incremental marking")
DEFINE_bool(incremental_marking_steps, true, "do incremental marking steps")
DEFINE_bool(trace_incremental_marking, false,
            "trace progress of the incremental marking")

// v8.cc
DEFINE_bool(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")

DEFINE_bool(send_idle_notification, false,
            "Send idle notifcation between stress runs.")
// ic.cc
DEFINE_bool(use_ic, true, "use inline caching")

#ifdef LIVE_OBJECT_LIST
// liveobjectlist.cc
DEFINE_string(lol_workdir, NULL, "path for lol temp files")
DEFINE_bool(verify_lol, false, "perform debugging verification for lol")
#endif

// macro-assembler-ia32.cc
DEFINE_bool(native_code_counters, false,
            "generate extra code for manipulating stats counters")

// mark-compact.cc
DEFINE_bool(always_compact, false, "Perform compaction on every full GC")
DEFINE_bool(lazy_sweeping, true,
            "Use lazy sweeping for old pointer and data spaces")
DEFINE_bool(never_compact, false,
            "Never perform compaction on full GC - testing only")
DEFINE_bool(compact_code_space, true,
            "Compact code space on full non-incremental collections")
DEFINE_bool(cleanup_code_caches_at_gc, true,
            "Flush inline caches prior to mark compact collection and "
            "flush code caches in maps during mark compact cycle.")
DEFINE_int(random_seed, 0,
           "Default seed for initializing random generator "
           "(0, the default, means to use system random).")

// objects.cc
DEFINE_bool(use_verbose_printer, true, "allows verbose printing")

// parser.cc
DEFINE_bool(allow_natives_syntax, false, "allow natives syntax")

// simulator-arm.cc and simulator-mips.cc
DEFINE_bool(trace_sim, false, "Trace simulator execution")
DEFINE_bool(check_icache, false,
            "Check icache flushes in ARM and MIPS simulator")
DEFINE_int(stop_sim_at, 0, "Simulator stop after x number of instructions")
DEFINE_int(sim_stack_alignment, 8,
           "Stack alingment in bytes in simulator (4 or 8, 8 is default)")

// isolate.cc
DEFINE_bool(trace_exception, false,
            "print stack trace when throwing exceptions")
DEFINE_bool(preallocate_message_memory, false,
            "preallocate some memory to build stack traces.")
DEFINE_bool(randomize_hashes,
            true,
            "randomize hashes to avoid predictable hash collisions "
            "(with snapshots this option cannot override the baked-in seed)")
DEFINE_int(hash_seed,
           0,
           "Fixed seed to use to hash property keys (0 means random)"
           "(with snapshots this option cannot override the baked-in seed)")

// v8.cc
DEFINE_bool(preemption, false,
            "activate a 100ms timer that switches between V8 threads")

// Regexp
DEFINE_bool(regexp_optimization, true, "generate optimized regexp code")

// Testing flags test/cctest/test-{flags,api,serialization}.cc
DEFINE_bool(testing_bool_flag, true, "testing_bool_flag")
DEFINE_int(testing_int_flag, 13, "testing_int_flag")
DEFINE_float(testing_float_flag, 2.5, "float-flag")
DEFINE_string(testing_string_flag, "Hello, world!", "string-flag")
DEFINE_int(testing_prng_seed, 42, "Seed used for threading test randomness")
#ifdef WIN32
DEFINE_string(testing_serialization_file, "C:\\Windows\\Temp\\serdes",
              "file in which to testing_serialize heap")
#else
DEFINE_string(testing_serialization_file, "/tmp/serdes",
              "file in which to serialize heap")
#endif

//
// Dev shell flags
//

DEFINE_bool(help, false, "Print usage message, including flags, on console")
DEFINE_bool(dump_counters, false, "Dump counters on exit")

#ifdef ENABLE_DEBUGGER_SUPPORT
DEFINE_bool(debugger, false, "Enable JavaScript debugger")
DEFINE_bool(remote_debugger, false, "Connect JavaScript debugger to the "
                                    "debugger agent in another process")
DEFINE_bool(debugger_agent, false, "Enable debugger agent")
DEFINE_int(debugger_port, 5858, "Port to use for remote debugging")
#endif  // ENABLE_DEBUGGER_SUPPORT

DEFINE_string(map_counters, "", "Map counters to a file")
DEFINE_args(js_arguments, JSARGUMENTS_INIT,
            "Pass all remaining arguments to the script. Alias for \"--\".")

#if defined(WEBOS__)
DEFINE_bool(debug_compile_events, false, "Enable debugger compile events")
DEFINE_bool(debug_script_collected_events, false,
            "Enable debugger script collected events")
#else
DEFINE_bool(debug_compile_events, true, "Enable debugger compile events")
DEFINE_bool(debug_script_collected_events, true,
            "Enable debugger script collected events")
#endif


//
// GDB JIT integration flags.
//

DEFINE_bool(gdbjit, false, "enable GDBJIT interface (disables compacting GC)")
DEFINE_bool(gdbjit_full, false, "enable GDBJIT interface for all code objects")
DEFINE_bool(gdbjit_dump, false, "dump elf objects with debug info to disk")
DEFINE_string(gdbjit_dump_filter, "",
              "dump only objects containing this substring")

// mark-compact.cc
DEFINE_bool(force_marking_deque_overflows, false,
            "force overflows of marking deque by reducing it's size "
            "to 64 words")

DEFINE_bool(stress_compaction, false,
            "stress the GC compactor to flush out bugs (implies "
            "--force_marking_deque_overflows)")

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
DEFINE_bool(enable_slow_asserts, false,
            "enable asserts that are slow to execute")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(trace_codegen, false,
            "print name of functions for which code is generated")
DEFINE_bool(print_source, false, "pretty print source code")
DEFINE_bool(print_builtin_source, false,
            "pretty print source code for builtins")
DEFINE_bool(print_ast, false, "print source AST")
DEFINE_bool(print_builtin_ast, false, "print source AST for builtins")
DEFINE_string(stop_at, "", "function name where to insert a breakpoint")

// compiler.cc
DEFINE_bool(print_builtin_scopes, false, "print scopes for builtins")
DEFINE_bool(print_scopes, false, "print scopes")

// contexts.cc
DEFINE_bool(trace_contexts, false, "trace contexts operations")

// heap.cc
DEFINE_bool(gc_greedy, false, "perform GC prior to some allocations")
DEFINE_bool(gc_verbose, false, "print stuff during garbage collection")
DEFINE_bool(heap_stats, false, "report heap statistics before and after GC")
DEFINE_bool(code_stats, false, "report code statistics after GC")
DEFINE_bool(verify_heap, false, "verify heap pointers before and after GC")
DEFINE_bool(print_handles, false, "report handles after GC")
DEFINE_bool(print_global_handles, false, "report global handles after GC")

// ic.cc
DEFINE_bool(trace_ic, false, "trace inline cache state transitions")

// interface.cc
DEFINE_bool(print_interfaces, false, "print interfaces")
DEFINE_bool(print_interface_details, false, "print interface inference details")
DEFINE_int(print_interface_depth, 5, "depth for printing interfaces")

// objects.cc
DEFINE_bool(trace_normalization,
            false,
            "prints when objects are turned into dictionaries.")

// runtime.cc
DEFINE_bool(trace_lazy, false, "trace lazy compilation")

// spaces.cc
DEFINE_bool(collect_heap_spill_statistics, false,
            "report heap spill statistics along with heap_stats "
            "(requires heap_stats)")

DEFINE_bool(trace_isolates, false, "trace isolate state changes")

// VM state
DEFINE_bool(log_state_changes, false, "Log state changes.")

// Regexp
DEFINE_bool(regexp_possessive_quantifier,
            false,
            "enable possessive quantifier syntax for testing")
DEFINE_bool(trace_regexp_bytecodes, false, "trace regexp bytecode execution")
DEFINE_bool(trace_regexp_assembler,
            false,
            "trace regexp macro assembler calls.")

//
// Logging and profiling flags
//
#undef FLAG
#define FLAG FLAG_FULL

// log.cc
DEFINE_bool(log, false,
            "Minimal logging (no API, code, GC, suspect, or handles samples).")
DEFINE_bool(log_all, false, "Log all events to the log file.")
DEFINE_bool(log_runtime, false, "Activate runtime system %Log call.")
DEFINE_bool(log_api, false, "Log API events to the log file.")
DEFINE_bool(log_code, false,
            "Log code events to the log file without profiling.")
DEFINE_bool(log_gc, false,
            "Log heap samples on garbage collection for the hp2ps tool.")
DEFINE_bool(log_handles, false, "Log global handle events.")
DEFINE_bool(log_snapshot_positions, false,
            "log positions of (de)serialized objects in the snapshot.")
DEFINE_bool(log_suspect, false, "Log suspect operations.")
DEFINE_bool(prof, false,
            "Log statistical profiling information (implies --log-code).")
DEFINE_bool(prof_auto, true,
            "Used with --prof, starts profiling automatically")
DEFINE_bool(prof_lazy, false,
            "Used with --prof, only does sampling and logging"
            " when profiler is active (implies --noprof_auto).")
DEFINE_bool(prof_browser_mode, true,
            "Used with --prof, turns on browser-compatible mode for profiling.")
DEFINE_bool(log_regexp, false, "Log regular expression execution.")
DEFINE_bool(sliding_state_window, false,
            "Update sliding state window counters.")
DEFINE_string(logfile, "v8.log", "Specify the name of the log file.")
DEFINE_bool(ll_prof, false, "Enable low-level linux profiler.")

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
DEFINE_bool(trace_elements_transitions, false, "trace elements transitions")

// code-stubs.cc
DEFINE_bool(print_code_stubs, false, "print code stubs")
DEFINE_bool(test_secondary_stub_cache,
            false,
            "test secondary stub cache by disabling the primary one")

DEFINE_bool(test_primary_stub_cache,
            false,
            "test primary stub cache by disabling the secondary one")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(print_code, false, "print generated code")
DEFINE_bool(print_opt_code, false, "print optimized code")
DEFINE_bool(print_unopt_code, false, "print unoptimized code before "
            "printing optimized code based on it")
DEFINE_bool(print_code_verbose, false, "print more information for code")
DEFINE_bool(print_builtin_code, false, "print generated code for builtins")

#ifdef ENABLE_DISASSEMBLER
DEFINE_bool(print_all_code, false, "enable all flags related to printing code")
DEFINE_implication(print_all_code, print_code)
DEFINE_implication(print_all_code, print_opt_code)
DEFINE_implication(print_all_code, print_unopt_code)
DEFINE_implication(print_all_code, print_code_verbose)
DEFINE_implication(print_all_code, print_builtin_code)
DEFINE_implication(print_all_code, print_code_stubs)
DEFINE_implication(print_all_code, code_comments)
#ifdef DEBUG
DEFINE_implication(print_all_code, trace_codegen)
#endif
#endif

// Cleanup...
#undef FLAG_FULL
#undef FLAG_READONLY
#undef FLAG

#undef DEFINE_bool
#undef DEFINE_int
#undef DEFINE_string
#undef DEFINE_implication

#undef FLAG_MODE_DECLARE
#undef FLAG_MODE_DEFINE
#undef FLAG_MODE_DEFINE_DEFAULTS
#undef FLAG_MODE_META
#undef FLAG_MODE_DEFINE_IMPLICATIONS
