// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CODEGEN_COMPILATION_CACHE_H_
#define V8_CODEGEN_COMPILATION_CACHE_H_

#include "src/base/hashmap.h"
#include "src/objects/compilation-cache-table.h"
#include "src/utils/allocation.h"

namespace v8 {
namespace internal {

template <typename T>
class Handle;

class RootVisitor;

// The compilation cache consists of several generational sub-caches which uses
// this class as a base class. A sub-cache contains a compilation cache tables
// for each generation of the sub-cache. Since the same source code string has
// different compiled code for scripts and evals, we use separate sub-caches
// for different compilation modes, to avoid retrieving the wrong result.
class CompilationSubCache {
 public:
  CompilationSubCache(Isolate* isolate, int generations)
      : isolate_(isolate), generations_(generations) {
    DCHECK_LE(generations, kMaxGenerations);
  }

  static constexpr int kFirstGeneration = 0;
  static constexpr int kMaxGenerations = 2;

  // Get the compilation cache tables for a specific generation.
  Handle<CompilationCacheTable> GetTable(int generation);

  // Accessors for first generation.
  Handle<CompilationCacheTable> GetFirstTable() {
    return GetTable(kFirstGeneration);
  }
  void SetFirstTable(Handle<CompilationCacheTable> value) {
    DCHECK_LT(kFirstGeneration, generations_);
    tables_[kFirstGeneration] = *value;
  }

  // Age the sub-cache by evicting the oldest generation and creating a new
  // young generation.
  virtual void Age() = 0;

  // GC support.
  void Iterate(RootVisitor* v);

  // Clear this sub-cache evicting all its content.
  void Clear();

  // Remove given shared function info from sub-cache.
  void Remove(Handle<SharedFunctionInfo> function_info);

  // Number of generations in this sub-cache.
  int generations() const { return generations_; }

 protected:
  Isolate* isolate() const { return isolate_; }

  // Ageing occurs either by removing the oldest generation, or with
  // custom logic implemented in CompilationCacheTable::Age.
  static void AgeByGeneration(CompilationSubCache* c);
  static void AgeCustom(CompilationSubCache* c);

 private:
  Isolate* const isolate_;
  const int generations_;
  Object tables_[kMaxGenerations];  // One for each generation.

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationSubCache);
};

// Sub-cache for scripts.
class CompilationCacheScript : public CompilationSubCache {
 public:
  explicit CompilationCacheScript(Isolate* isolate);

  MaybeHandle<SharedFunctionInfo> Lookup(Handle<String> source,
                                         MaybeHandle<Object> name,
                                         int line_offset, int column_offset,
                                         ScriptOriginOptions resource_options,
                                         LanguageMode language_mode);

  void Put(Handle<String> source, LanguageMode language_mode,
           Handle<SharedFunctionInfo> function_info);

  void Age() override;

 private:
  bool HasOrigin(Handle<SharedFunctionInfo> function_info,
                 MaybeHandle<Object> name, int line_offset, int column_offset,
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
class CompilationCacheEval : public CompilationSubCache {
 public:
  explicit CompilationCacheEval(Isolate* isolate)
      : CompilationSubCache(isolate, 1) {}

  InfoCellPair Lookup(Handle<String> source,
                      Handle<SharedFunctionInfo> outer_info,
                      Handle<Context> native_context,
                      LanguageMode language_mode, int position);

  void Put(Handle<String> source, Handle<SharedFunctionInfo> outer_info,
           Handle<SharedFunctionInfo> function_info,
           Handle<Context> native_context, Handle<FeedbackCell> feedback_cell,
           int position);

  void Age() override;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheEval);
};

// Sub-cache for regular expressions.
class CompilationCacheRegExp : public CompilationSubCache {
 public:
  CompilationCacheRegExp(Isolate* isolate, int generations)
      : CompilationSubCache(isolate, generations) {}

  MaybeHandle<FixedArray> Lookup(Handle<String> source, JSRegExp::Flags flags);

  void Put(Handle<String> source, JSRegExp::Flags flags,
           Handle<FixedArray> data);

  void Age() override;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheRegExp);
};

// The compilation cache keeps shared function infos for compiled
// scripts and evals. The shared function infos are looked up using
// the source string as the key. For regular expressions the
// compilation data is cached.
class V8_EXPORT_PRIVATE CompilationCache {
 public:
  CompilationCache(const CompilationCache&) = delete;
  CompilationCache& operator=(const CompilationCache&) = delete;

  // Finds the script shared function info for a source
  // string. Returns an empty handle if the cache doesn't contain a
  // script for the given source string with the right origin.
  MaybeHandle<SharedFunctionInfo> LookupScript(
      Handle<String> source, MaybeHandle<Object> name, int line_offset,
      int column_offset, ScriptOriginOptions resource_options,
      LanguageMode language_mode);

  // Finds the shared function info for a source string for eval in a
  // given context.  Returns an empty handle if the cache doesn't
  // contain a script for the given source string.
  InfoCellPair LookupEval(Handle<String> source,
                          Handle<SharedFunctionInfo> outer_info,
                          Handle<Context> context, LanguageMode language_mode,
                          int position);

  // Returns the regexp data associated with the given regexp if it
  // is in cache, otherwise an empty handle.
  MaybeHandle<FixedArray> LookupRegExp(Handle<String> source,
                                       JSRegExp::Flags flags);

  // Associate the (source, kind) pair to the shared function
  // info. This may overwrite an existing mapping.
  void PutScript(Handle<String> source, LanguageMode language_mode,
                 Handle<SharedFunctionInfo> function_info);

  // Associate the (source, context->closure()->shared(), kind) triple
  // with the shared function info. This may overwrite an existing mapping.
  void PutEval(Handle<String> source, Handle<SharedFunctionInfo> outer_info,
               Handle<Context> context,
               Handle<SharedFunctionInfo> function_info,
               Handle<FeedbackCell> feedback_cell, int position);

  // Associate the (source, flags) pair to the given regexp data.
  // This may overwrite an existing mapping.
  void PutRegExp(Handle<String> source, JSRegExp::Flags flags,
                 Handle<FixedArray> data);

  // Clear the cache - also used to initialize the cache at startup.
  void Clear();

  // Remove given shared function info from all caches.
  void Remove(Handle<SharedFunctionInfo> function_info);

  // GC support.
  void Iterate(RootVisitor* v);

  // Notify the cache that a mark-sweep garbage collection is about to
  // take place. This is used to retire entries from the cache to
  // avoid keeping them alive too long without using them.
  void MarkCompactPrologue();

  // Enable/disable compilation cache. Used by debugger to disable compilation
  // cache during debugging so that eval and new scripts are always compiled.
  // TODO(bmeurer, chromium:992277): The RegExp cache cannot be enabled and/or
  // disabled, since it doesn't affect debugging. However ideally the other
  // caches should also be always on, even in the presence of the debugger,
  // but at this point there are too many unclear invariants, and so I decided
  // to just fix the pressing performance problem for RegExp individually first.
  void EnableScriptAndEval();
  void DisableScriptAndEval();

 private:
  explicit CompilationCache(Isolate* isolate);
  ~CompilationCache() = default;

  base::HashMap* EagerOptimizingSet();

  bool IsEnabledScriptAndEval() const {
    return FLAG_compilation_cache && enabled_script_and_eval_;
  }

  Isolate* isolate() const { return isolate_; }

  Isolate* isolate_;

  CompilationCacheScript script_;
  CompilationCacheEval eval_global_;
  CompilationCacheEval eval_contextual_;
  CompilationCacheRegExp reg_exp_;

  static constexpr int kSubCacheCount = 4;
  CompilationSubCache* subcaches_[kSubCacheCount];

  // Current enable state of the compilation cache for scripts and eval.
  bool enabled_script_and_eval_;

  friend class Isolate;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CODEGEN_COMPILATION_CACHE_H_
