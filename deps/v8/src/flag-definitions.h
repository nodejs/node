// Copyright 2008 the V8 project authors. All rights reserved.
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

// We want to supply the actual storage and value for the flag variable in the
// .cc file.  We only do this for writable flags.
#elif defined(FLAG_MODE_DEFINE)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  ctype FLAG_##nam = def;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)

// We need to define all of our default values so that the Flag structure can
// access them by pointer.  These are just used internally inside of one .cc,
// for MODE_META, so there is no impact on the flags interface.
#elif defined(FLAG_MODE_DEFINE_DEFAULTS)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  static ctype const FLAGDEFAULT_##nam = def;
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)


// We want to write entries into our meta data table, for internal parsing and
// printing / etc in the flag parser code.  We only do this for writable flags.
#elif defined(FLAG_MODE_META)
#define FLAG_FULL(ftype, ctype, nam, def, cmt) \
  { Flag::TYPE_##ftype, #nam, &FLAG_##nam, &FLAGDEFAULT_##nam, cmt, false },
#define FLAG_READONLY(ftype, ctype, nam, def, cmt)

#else
#error No mode supplied when including flags.defs
#endif

#ifdef FLAG_MODE_DECLARE
// Structure used to hold a collection of arguments to the JavaScript code.
struct JSArguments {
public:
  JSArguments();
  JSArguments(int argc, const char** argv);
  int argc() const;
  const char** argv();
  const char*& operator[](int idx);
  JSArguments& operator=(JSArguments args);
private:
  int argc_;
  const char** argv_;
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

// assembler-ia32.cc / assembler-arm.cc / assembler-x64.cc
DEFINE_bool(debug_code, false,
            "generate extra code (comments, assertions) for debugging")
DEFINE_bool(emit_branch_hints, false, "emit branch hints")
DEFINE_bool(peephole_optimization, true,
            "perform peephole optimizations in assembly code")
DEFINE_bool(print_peephole_optimization, false,
            "print peephole optimizations in assembly code")
DEFINE_bool(enable_sse2, true,
            "enable use of SSE2 instructions if available")
DEFINE_bool(enable_sse3, true,
            "enable use of SSE3 instructions if available")
DEFINE_bool(enable_cmov, true,
            "enable use of CMOV instruction if available")
DEFINE_bool(enable_rdtsc, true,
            "enable use of RDTSC instruction if available")
DEFINE_bool(enable_sahf, true,
            "enable use of SAHF instruction if available (X64 only)")
DEFINE_bool(enable_vfp3, true,
            "enable use of VFP3 instructions if available (ARM only)")
DEFINE_bool(enable_armv7, true,
            "enable use of ARMv7 instructions if available (ARM only)")

// bootstrapper.cc
DEFINE_string(expose_natives_as, NULL, "expose natives in global object")
DEFINE_string(expose_debug_as, NULL, "expose debug in global object")
DEFINE_bool(expose_gc, false, "expose gc extension")
DEFINE_int(stack_trace_limit, 10, "number of stack frames to capture")
DEFINE_bool(disable_native_files, false, "disable builtin natives files")

// builtins-ia32.cc
DEFINE_bool(inline_new, true, "use fast inline allocation")

// checks.cc
DEFINE_bool(stack_trace_on_abort, true,
            "print a stack trace if an assertion failure occurs")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(trace, false, "trace function calls")
DEFINE_bool(defer_negation, true, "defer negation operation")

// codegen.cc
DEFINE_bool(lazy, true, "use lazy compilation")
DEFINE_bool(debug_info, true, "add debug information to compiled functions")

// compiler.cc
DEFINE_bool(strict, false, "strict error checking")
DEFINE_int(min_preparse_length, 1024,
           "minimum length for automatic enable preparsing")
DEFINE_bool(full_compiler, true, "enable dedicated backend for run-once code")
DEFINE_bool(fast_compiler, false, "enable speculative optimizing backend")
DEFINE_bool(always_full_compiler, false,
            "try to use the dedicated run-once backend for all code")
DEFINE_bool(always_fast_compiler, false,
            "try to use the speculative optimizing backend for all code")
DEFINE_bool(trace_bailout, false,
            "print reasons for falling back to using the classic V8 backend")
DEFINE_bool(safe_int32_compiler, true,
            "enable optimized side-effect-free int32 expressions.")
DEFINE_bool(use_flow_graph, false, "perform flow-graph based optimizations")

// compilation-cache.cc
DEFINE_bool(compilation_cache, true, "enable compilation cache")

// data-flow.cc
DEFINE_bool(loop_peeling, false, "Peel off the first iteration of loops.")

// debug.cc
DEFINE_bool(remote_debugging, false, "enable remote debugging")
DEFINE_bool(trace_debug_json, false, "trace debugging JSON request/response")
DEFINE_bool(debugger_auto_break, true,
            "automatically set the debug break flag when debugger commands are "
            "in the queue")
DEFINE_bool(enable_liveedit, true, "enable liveedit experimental feature")

// frames.cc
DEFINE_int(max_stack_trace_source_length, 300,
           "maximum length of function source code printed in a stack trace.")

// heap.cc
DEFINE_int(max_new_space_size, 0, "max size of the new generation")
DEFINE_int(max_old_space_size, 0, "max size of the old generation")
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
DEFINE_bool(collect_maps, true,
            "garbage collect maps from which no objects can be reached")
DEFINE_bool(flush_code, false,
            "flush code that we expect not to use again before full gc")

// v8.cc
DEFINE_bool(use_idle_notification, true,
            "Use idle notification to reduce memory footprint.")
// ic.cc
DEFINE_bool(use_ic, true, "use inline caching")

// macro-assembler-ia32.cc
DEFINE_bool(native_code_counters, false,
            "generate extra code for manipulating stats counters")

// mark-compact.cc
DEFINE_bool(always_compact, false, "Perform compaction on every full GC")
DEFINE_bool(never_compact, false,
            "Never perform compaction on full GC - testing only")
DEFINE_bool(cleanup_ics_at_gc, true,
            "Flush inline caches prior to mark compact collection.")
DEFINE_bool(cleanup_caches_in_maps_at_gc, true,
            "Flush code caches in maps during mark compact cycle.")
DEFINE_int(random_seed, 0,
           "Default seed for initializing random generator "
           "(0, the default, means to use system random).")

DEFINE_bool(canonicalize_object_literal_maps, true,
            "Canonicalize maps for object literals.")

DEFINE_bool(use_big_map_space, true,
            "Use big map space, but don't compact if it grew too big.")

DEFINE_int(max_map_space_pages, MapSpace::kMaxMapPageIndex - 1,
           "Maximum number of pages in map space which still allows to encode "
           "forwarding pointers.  That's actually a constant, but it's useful "
           "to control it with a flag for better testing.")

// mksnapshot.cc
DEFINE_bool(h, false, "print this message")
DEFINE_bool(new_snapshot, true, "use new snapshot implementation")

// parser.cc
DEFINE_bool(allow_natives_syntax, false, "allow natives syntax")

// rewriter.cc
DEFINE_bool(optimize_ast, true, "optimize the ast")

// simulator-arm.cc and simulator-mips.cc
DEFINE_bool(trace_sim, false, "Trace simulator execution")
DEFINE_bool(check_icache, false, "Check icache flushes in ARM simulator")
DEFINE_int(stop_sim_at, 0, "Simulator stop after x number of instructions")
DEFINE_int(sim_stack_alignment, 8,
           "Stack alingment in bytes in simulator (4 or 8, 8 is default)")

// top.cc
DEFINE_bool(trace_exception, false,
            "print stack trace when throwing exceptions")
DEFINE_bool(preallocate_message_memory, false,
            "preallocate some memory to build stack traces.")

// v8.cc
DEFINE_bool(preemption, false,
            "activate a 100ms timer that switches between V8 threads")

// Regexp
DEFINE_bool(trace_regexps, false, "trace regexp execution")
DEFINE_bool(regexp_optimization, true, "generate optimized regexp code")
DEFINE_bool(regexp_entry_native, true, "use native code to enter regexp")

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
DEFINE_bool(debugger, false, "Enable JavaScript debugger")
DEFINE_bool(remote_debugger, false, "Connect JavaScript debugger to the "
                                    "debugger agent in another process")
DEFINE_bool(debugger_agent, false, "Enable debugger agent")
DEFINE_int(debugger_port, 5858, "Port to use for remote debugging")
DEFINE_string(map_counters, false, "Map counters to a file")
DEFINE_args(js_arguments, JSArguments(),
            "Pass all remaining arguments to the script. Alias for \"--\".")

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
DEFINE_bool(print_json_ast, false, "print source AST as JSON")
DEFINE_bool(print_builtin_json_ast, false,
            "print source AST for builtins as JSON")
DEFINE_bool(trace_calls, false, "trace calls")
DEFINE_bool(trace_builtin_calls, false, "trace builtins calls")
DEFINE_string(stop_at, "", "function name where to insert a breakpoint")

// compiler.cc
DEFINE_bool(print_builtin_scopes, false, "print scopes for builtins")
DEFINE_bool(print_scopes, false, "print scopes")
DEFINE_bool(print_ir, false, "print the AST as seen by the backend")
DEFINE_bool(print_graph_text, false,
            "print a text representation of the flow graph")

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

// objects.cc
DEFINE_bool(trace_normalization,
            false,
            "prints when objects are turned into dictionaries.")

// runtime.cc
DEFINE_bool(trace_lazy, false, "trace lazy compilation")

// serialize.cc
DEFINE_bool(debug_serialization, false,
            "write debug information into the snapshot.")

// spaces.cc
DEFINE_bool(collect_heap_spill_statistics, false,
            "report heap spill statistics along with heap_stats "
            "(requires heap_stats)")

// Regexp
DEFINE_bool(regexp_possessive_quantifier,
            false,
            "enable possessive quantifier syntax for testing")
DEFINE_bool(trace_regexp_bytecodes, false, "trace regexp bytecode execution")
DEFINE_bool(trace_regexp_assembler,
            false,
            "trace regexp macro assembler calls.")

//
// Logging and profiling only flags
//
#undef FLAG
#ifdef ENABLE_LOGGING_AND_PROFILING
#define FLAG FLAG_FULL
#else
#define FLAG FLAG_READONLY
#endif

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
DEFINE_bool(log_state_changes, false, "Log state changes.")
DEFINE_bool(log_suspect, false, "Log suspect operations.")
DEFINE_bool(log_producers, false, "Log stack traces of JS objects allocations.")
DEFINE_bool(compress_log, false,
            "Compress log to save space (makes log less human-readable).")
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
DEFINE_bool(oprofile, false, "Enable JIT agent for OProfile.")

//
// Heap protection flags
// Using heap protection requires ENABLE_LOGGING_AND_PROFILING as well.
//
#ifdef ENABLE_HEAP_PROTECTION
#undef FLAG
#define FLAG FLAG_FULL

DEFINE_bool(protect_heap, false,
            "Protect/unprotect V8's heap when leaving/entring the VM.")

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

// code-stubs.cc
DEFINE_bool(print_code_stubs, false, "print code stubs")

// codegen-ia32.cc / codegen-arm.cc
DEFINE_bool(print_code, false, "print generated code")
DEFINE_bool(print_builtin_code, false, "print generated code for builtins")

// Cleanup...
#undef FLAG_FULL
#undef FLAG_READONLY
#undef FLAG

#undef DEFINE_bool
#undef DEFINE_int
#undef DEFINE_string

#undef FLAG_MODE_DECLARE
#undef FLAG_MODE_DEFINE
#undef FLAG_MODE_DEFINE_DEFAULTS
#undef FLAG_MODE_META
