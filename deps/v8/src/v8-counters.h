// Copyright 2007-2008 the V8 project authors. All rights reserved.
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

#ifndef V8_V8_COUNTERS_H_
#define V8_V8_COUNTERS_H_

#include "counters.h"

namespace v8 {
namespace internal {

#define HISTOGRAM_TIMER_LIST(HT)                                      \
  /* Garbage collection timers. */                                    \
  HT(gc_compactor, V8.GCCompactor)                                    \
  HT(gc_scavenger, V8.GCScavenger)                                    \
  HT(gc_context, V8.GCContext) /* GC context cleanup time */          \
  /* Parsing timers. */                                               \
  HT(parse, V8.Parse)                                                 \
  HT(parse_lazy, V8.ParseLazy)                                        \
  HT(pre_parse, V8.PreParse)                                          \
  /* Total compilation times. */                                      \
  HT(compile, V8.Compile)                                             \
  HT(compile_eval, V8.CompileEval)                                    \
  HT(compile_lazy, V8.CompileLazy)                                    \
  /* Individual compiler passes. */                                   \
  HT(rewriting, V8.Rewriting)                                         \
  HT(usage_analysis, V8.UsageAnalysis)                                \
  HT(variable_allocation, V8.VariableAllocation)                      \
  HT(ast_optimization, V8.ASTOptimization)                            \
  HT(code_generation, V8.CodeGeneration)                              \
  HT(deferred_code_generation, V8.DeferredCodeGeneration)


// WARNING: STATS_COUNTER_LIST_* is a very large macro that is causing MSVC
// Intellisense to crash.  It was broken into two macros (each of length 40
// lines) rather than one macro (of length about 80 lines) to work around
// this problem.  Please avoid using recursive macros of this length when
// possible.
#define STATS_COUNTER_LIST_1(SC)                                 \
  /* Global Handle Count*/                                       \
  SC(global_handles, V8.GlobalHandles)                           \
  /* Mallocs from PCRE */                                        \
  SC(pcre_mallocs, V8.PcreMallocCount)                           \
  /* OS Memory allocated */                                      \
  SC(memory_allocated, V8.OsMemoryAllocated)                     \
  SC(props_to_dictionary, V8.ObjectPropertiesToDictionary)       \
  SC(elements_to_dictionary, V8.ObjectElementsToDictionary)      \
  SC(alive_after_last_gc, V8.AliveAfterLastGC)                   \
  SC(objs_since_last_young, V8.ObjsSinceLastYoung)               \
  SC(objs_since_last_full, V8.ObjsSinceLastFull)                 \
  SC(symbol_table_capacity, V8.SymbolTableCapacity)              \
  SC(number_of_symbols, V8.NumberOfSymbols)                      \
  SC(script_wrappers, V8.ScriptWrappers)                         \
  SC(call_initialize_stubs, V8.CallInitializeStubs)              \
  SC(call_premonomorphic_stubs, V8.CallPreMonomorphicStubs)      \
  SC(call_normal_stubs, V8.CallNormalStubs)                      \
  SC(call_megamorphic_stubs, V8.CallMegamorphicStubs)            \
  SC(arguments_adaptors, V8.ArgumentsAdaptors)                   \
  SC(compilation_cache_hits, V8.CompilationCacheHits)            \
  SC(compilation_cache_misses, V8.CompilationCacheMisses)        \
  SC(regexp_cache_hits, V8.RegExpCacheHits)                      \
  SC(regexp_cache_misses, V8.RegExpCacheMisses)                  \
  /* Amount of evaled source code. */                            \
  SC(total_eval_size, V8.TotalEvalSize)                          \
  /* Amount of loaded source code. */                            \
  SC(total_load_size, V8.TotalLoadSize)                          \
  /* Amount of parsed source code. */                            \
  SC(total_parse_size, V8.TotalParseSize)                        \
  /* Amount of source code skipped over using preparsing. */     \
  SC(total_preparse_skipped, V8.TotalPreparseSkipped)            \
  /* Amount of compiled source code. */                          \
  SC(total_compile_size, V8.TotalCompileSize)


#define STATS_COUNTER_LIST_2(SC)                                    \
  /* Number of code stubs. */                                       \
  SC(code_stubs, V8.CodeStubs)                                      \
  /* Amount of stub code. */                                        \
  SC(total_stubs_code_size, V8.TotalStubsCodeSize)                  \
  /* Amount of (JS) compiled code. */                               \
  SC(total_compiled_code_size, V8.TotalCompiledCodeSize)            \
  SC(gc_compactor_caused_by_request, V8.GCCompactorCausedByRequest) \
  SC(gc_compactor_caused_by_promoted_data,                          \
     V8.GCCompactorCausedByPromotedData)                            \
  SC(gc_compactor_caused_by_oldspace_exhaustion,                    \
     V8.GCCompactorCausedByOldspaceExhaustion)                      \
  SC(gc_compactor_caused_by_weak_handles,                           \
     V8.GCCompactorCausedByWeakHandles)                             \
  SC(gc_last_resort_from_js, V8.GCLastResortFromJS)                 \
  SC(gc_last_resort_from_handles, V8.GCLastResortFromHandles)       \
  /* How is the generic keyed-load stub used? */                    \
  SC(keyed_load_generic_smi, V8.KeyedLoadGenericSmi)                \
  SC(keyed_load_generic_symbol, V8.KeyedLoadGenericSymbol)          \
  SC(keyed_load_generic_slow, V8.KeyedLoadGenericSlow)              \
  SC(keyed_load_external_array_slow, V8.KeyedLoadExternalArraySlow) \
  /* Count how much the monomorphic keyed-load stubs are hit. */    \
  SC(keyed_load_function_prototype, V8.KeyedLoadFunctionPrototype)  \
  SC(keyed_load_string_length, V8.KeyedLoadStringLength)            \
  SC(keyed_load_array_length, V8.KeyedLoadArrayLength)              \
  SC(keyed_load_constant_function, V8.KeyedLoadConstantFunction)    \
  SC(keyed_load_field, V8.KeyedLoadField)                           \
  SC(keyed_load_callback, V8.KeyedLoadCallback)                     \
  SC(keyed_load_interceptor, V8.KeyedLoadInterceptor)               \
  SC(keyed_load_inline, V8.KeyedLoadInline)                         \
  SC(keyed_load_inline_miss, V8.KeyedLoadInlineMiss)                \
  SC(named_load_inline, V8.NamedLoadInline)                         \
  SC(named_load_inline_miss, V8.NamedLoadInlineMiss)                \
  SC(named_load_global_inline, V8.NamedLoadGlobalInline)            \
  SC(named_load_global_inline_miss, V8.NamedLoadGlobalInlineMiss)   \
  SC(keyed_store_field, V8.KeyedStoreField)                         \
  SC(keyed_store_inline, V8.KeyedStoreInline)                       \
  SC(keyed_store_inline_miss, V8.KeyedStoreInlineMiss)              \
  SC(named_store_global_inline, V8.NamedStoreGlobalInline)          \
  SC(named_store_global_inline_miss, V8.NamedStoreGlobalInlineMiss) \
  SC(call_global_inline, V8.CallGlobalInline)                       \
  SC(call_global_inline_miss, V8.CallGlobalInlineMiss)              \
  SC(constructed_objects, V8.ConstructedObjects)                    \
  SC(constructed_objects_runtime, V8.ConstructedObjectsRuntime)     \
  SC(constructed_objects_stub, V8.ConstructedObjectsStub)           \
  SC(array_function_runtime, V8.ArrayFunctionRuntime)               \
  SC(array_function_native, V8.ArrayFunctionNative)                 \
  SC(for_in, V8.ForIn)                                              \
  SC(enum_cache_hits, V8.EnumCacheHits)                             \
  SC(enum_cache_misses, V8.EnumCacheMisses)                         \
  SC(reloc_info_count, V8.RelocInfoCount)                           \
  SC(reloc_info_size, V8.RelocInfoSize)                             \
  SC(zone_segment_bytes, V8.ZoneSegmentBytes)                       \
  SC(compute_entry_frame, V8.ComputeEntryFrame)                     \
  SC(generic_binary_stub_calls, V8.GenericBinaryStubCalls)          \
  SC(generic_binary_stub_calls_regs, V8.GenericBinaryStubCallsRegs) \
  SC(string_add_runtime, V8.StringAddRuntime)                       \
  SC(string_add_native, V8.StringAddNative)                         \
  SC(sub_string_runtime, V8.SubStringRuntime)                       \
  SC(sub_string_native, V8.SubStringNative)                         \
  SC(string_compare_native, V8.StringCompareNative)                 \
  SC(string_compare_runtime, V8.StringCompareRuntime)               \
  SC(regexp_entry_runtime, V8.RegExpEntryRuntime)                   \
  SC(regexp_entry_native, V8.RegExpEntryNative)

// This file contains all the v8 counters that are in use.
class Counters : AllStatic {
 public:
#define HT(name, caption) \
  static HistogramTimer name;
  HISTOGRAM_TIMER_LIST(HT)
#undef HT

#define SC(name, caption) \
  static StatsCounter name;
  STATS_COUNTER_LIST_1(SC)
  STATS_COUNTER_LIST_2(SC)
#undef SC

  enum Id {
#define RATE_ID(name, caption) k_##name,
    HISTOGRAM_TIMER_LIST(RATE_ID)
#undef RATE_ID
#define COUNTER_ID(name, caption) k_##name,
  STATS_COUNTER_LIST_1(COUNTER_ID)
  STATS_COUNTER_LIST_2(COUNTER_ID)
#undef COUNTER_ID
#define COUNTER_ID(name) k_##name,
  STATE_TAG_LIST(COUNTER_ID)
#undef COUNTER_ID
    stats_counter_count
  };

  // Sliding state window counters.
  static StatsCounter state_counters[];
};

} }  // namespace v8::internal

#endif  // V8_V8_COUNTERS_H_
