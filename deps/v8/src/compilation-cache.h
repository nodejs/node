// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILATION_CACHE_H_
#define V8_COMPILATION_CACHE_H_

#include "src/allocation.h"
#include "src/handles.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

// The compilation cache consists of several generational sub-caches which uses
// this class as a base class. A sub-cache contains a compilation cache tables
// for each generation of the sub-cache. Since the same source code string has
// different compiled code for scripts and evals, we use separate sub-caches
// for different compilation modes, to avoid retrieving the wrong result.
class CompilationSubCache {
 public:
  CompilationSubCache(Isolate* isolate, int generations)
      : isolate_(isolate),
        generations_(generations) {
    tables_ = NewArray<Object*>(generations);
  }

  ~CompilationSubCache() { DeleteArray(tables_); }

  // Index for the first generation in the cache.
  static const int kFirstGeneration = 0;

  // Get the compilation cache tables for a specific generation.
  Handle<CompilationCacheTable> GetTable(int generation);

  // Accessors for first generation.
  Handle<CompilationCacheTable> GetFirstTable() {
    return GetTable(kFirstGeneration);
  }
  void SetFirstTable(Handle<CompilationCacheTable> value) {
    DCHECK(kFirstGeneration < generations_);
    tables_[kFirstGeneration] = *value;
  }

  // Age the sub-cache by evicting the oldest generation and creating a new
  // young generation.
  void Age();

  // GC support.
  void Iterate(ObjectVisitor* v);
  void IterateFunctions(ObjectVisitor* v);

  // Clear this sub-cache evicting all its content.
  void Clear();

  // Remove given shared function info from sub-cache.
  void Remove(Handle<SharedFunctionInfo> function_info);

  // Number of generations in this sub-cache.
  inline int generations() { return generations_; }

 protected:
  Isolate* isolate() { return isolate_; }

 private:
  Isolate* isolate_;
  int generations_;  // Number of generations.
  Object** tables_;  // Compilation cache tables - one for each generation.

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationSubCache);
};


// Sub-cache for scripts.
class CompilationCacheScript : public CompilationSubCache {
 public:
  CompilationCacheScript(Isolate* isolate, int generations);

  Handle<SharedFunctionInfo> Lookup(Handle<String> source, Handle<Object> name,
                                    int line_offset, int column_offset,
                                    ScriptOriginOptions resource_options,
                                    Handle<Context> context,
                                    LanguageMode language_mode);
  void Put(Handle<String> source,
           Handle<Context> context,
           LanguageMode language_mode,
           Handle<SharedFunctionInfo> function_info);

 private:
  bool HasOrigin(Handle<SharedFunctionInfo> function_info, Handle<Object> name,
                 int line_offset, int column_offset,
                 ScriptOriginOptions resource_options);

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheScript);
};


// Sub-cache for eval scripts. Two caches for eval are used. One for eval calls
// in native contexts and one for eval calls in other contexts. The cache
// considers the following pieces of information when checking for matching
// entries:
// 1. The source string.
// 2. The shared function info of the calling function.
// 3. Whether the source should be compiled as strict code or as sloppy code.
//    Note: Currently there are clients of CompileEval that always compile
//    sloppy code even if the calling function is a strict mode function.
//    More specifically these are the CompileString, DebugEvaluate and
//    DebugEvaluateGlobal runtime functions.
// 4. The start position of the calling scope.
class CompilationCacheEval: public CompilationSubCache {
 public:
  CompilationCacheEval(Isolate* isolate, int generations)
      : CompilationSubCache(isolate, generations) { }

  MaybeHandle<SharedFunctionInfo> Lookup(Handle<String> source,
                                         Handle<SharedFunctionInfo> outer_info,
                                         LanguageMode language_mode,
                                         int scope_position);

  void Put(Handle<String> source, Handle<SharedFunctionInfo> outer_info,
           Handle<SharedFunctionInfo> function_info, int scope_position);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheEval);
};


// Sub-cache for regular expressions.
class CompilationCacheRegExp: public CompilationSubCache {
 public:
  CompilationCacheRegExp(Isolate* isolate, int generations)
      : CompilationSubCache(isolate, generations) { }

  MaybeHandle<FixedArray> Lookup(Handle<String> source, JSRegExp::Flags flags);

  void Put(Handle<String> source,
           JSRegExp::Flags flags,
           Handle<FixedArray> data);
 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheRegExp);
};


// The compilation cache keeps shared function infos for compiled
// scripts and evals. The shared function infos are looked up using
// the source string as the key. For regular expressions the
// compilation data is cached.
class CompilationCache {
 public:
  // Finds the script shared function info for a source
  // string. Returns an empty handle if the cache doesn't contain a
  // script for the given source string with the right origin.
  MaybeHandle<SharedFunctionInfo> LookupScript(
      Handle<String> source, Handle<Object> name, int line_offset,
      int column_offset, ScriptOriginOptions resource_options,
      Handle<Context> context, LanguageMode language_mode);

  // Finds the shared function info for a source string for eval in a
  // given context.  Returns an empty handle if the cache doesn't
  // contain a script for the given source string.
  MaybeHandle<SharedFunctionInfo> LookupEval(
      Handle<String> source, Handle<SharedFunctionInfo> outer_info,
      Handle<Context> context, LanguageMode language_mode, int scope_position);

  // Returns the regexp data associated with the given regexp if it
  // is in cache, otherwise an empty handle.
  MaybeHandle<FixedArray> LookupRegExp(
      Handle<String> source, JSRegExp::Flags flags);

  // Associate the (source, kind) pair to the shared function
  // info. This may overwrite an existing mapping.
  void PutScript(Handle<String> source,
                 Handle<Context> context,
                 LanguageMode language_mode,
                 Handle<SharedFunctionInfo> function_info);

  // Associate the (source, context->closure()->shared(), kind) triple
  // with the shared function info. This may overwrite an existing mapping.
  void PutEval(Handle<String> source, Handle<SharedFunctionInfo> outer_info,
               Handle<Context> context,
               Handle<SharedFunctionInfo> function_info, int scope_position);

  // Associate the (source, flags) pair to the given regexp data.
  // This may overwrite an existing mapping.
  void PutRegExp(Handle<String> source,
                 JSRegExp::Flags flags,
                 Handle<FixedArray> data);

  // Clear the cache - also used to initialize the cache at startup.
  void Clear();

  // Remove given shared function info from all caches.
  void Remove(Handle<SharedFunctionInfo> function_info);

  // GC support.
  void Iterate(ObjectVisitor* v);
  void IterateFunctions(ObjectVisitor* v);

  // Notify the cache that a mark-sweep garbage collection is about to
  // take place. This is used to retire entries from the cache to
  // avoid keeping them alive too long without using them.
  void MarkCompactPrologue();

  // Enable/disable compilation cache. Used by debugger to disable compilation
  // cache during debugging to make sure new scripts are always compiled.
  void Enable();
  void Disable();

 private:
  explicit CompilationCache(Isolate* isolate);
  ~CompilationCache();

  HashMap* EagerOptimizingSet();

  // The number of sub caches covering the different types to cache.
  static const int kSubCacheCount = 4;

  bool IsEnabled() { return FLAG_compilation_cache && enabled_; }

  Isolate* isolate() { return isolate_; }

  Isolate* isolate_;

  CompilationCacheScript script_;
  CompilationCacheEval eval_global_;
  CompilationCacheEval eval_contextual_;
  CompilationCacheRegExp reg_exp_;
  CompilationSubCache* subcaches_[kSubCacheCount];

  // Current enable state of the compilation cache.
  bool enabled_;

  friend class Isolate;

  DISALLOW_COPY_AND_ASSIGN(CompilationCache);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILATION_CACHE_H_
