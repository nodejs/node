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

#ifndef V8_COMPILATION_CACHE_H_
#define V8_COMPILATION_CACHE_H_

namespace v8 {
namespace internal {

class HashMap;

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
    ASSERT(kFirstGeneration < generations_);
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

  Handle<SharedFunctionInfo> Lookup(Handle<String> source,
                                    Handle<Object> name,
                                    int line_offset,
                                    int column_offset);
  void Put(Handle<String> source, Handle<SharedFunctionInfo> function_info);

 private:
  MUST_USE_RESULT MaybeObject* TryTablePut(
      Handle<String> source, Handle<SharedFunctionInfo> function_info);

  // Note: Returns a new hash table if operation results in expansion.
  Handle<CompilationCacheTable> TablePut(
      Handle<String> source, Handle<SharedFunctionInfo> function_info);

  bool HasOrigin(Handle<SharedFunctionInfo> function_info,
                 Handle<Object> name,
                 int line_offset,
                 int column_offset);

  void* script_histogram_;
  bool script_histogram_initialized_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheScript);
};


// Sub-cache for eval scripts.
class CompilationCacheEval: public CompilationSubCache {
 public:
  CompilationCacheEval(Isolate* isolate, int generations)
      : CompilationSubCache(isolate, generations) { }

  Handle<SharedFunctionInfo> Lookup(Handle<String> source,
                                    Handle<Context> context,
                                    StrictModeFlag strict_mode);

  void Put(Handle<String> source,
           Handle<Context> context,
           Handle<SharedFunctionInfo> function_info);

 private:
  MUST_USE_RESULT MaybeObject* TryTablePut(
      Handle<String> source,
      Handle<Context> context,
      Handle<SharedFunctionInfo> function_info);

  // Note: Returns a new hash table if operation results in expansion.
  Handle<CompilationCacheTable> TablePut(
      Handle<String> source,
      Handle<Context> context,
      Handle<SharedFunctionInfo> function_info);

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheEval);
};


// Sub-cache for regular expressions.
class CompilationCacheRegExp: public CompilationSubCache {
 public:
  CompilationCacheRegExp(Isolate* isolate, int generations)
      : CompilationSubCache(isolate, generations) { }

  Handle<FixedArray> Lookup(Handle<String> source, JSRegExp::Flags flags);

  void Put(Handle<String> source,
           JSRegExp::Flags flags,
           Handle<FixedArray> data);
 private:
  MUST_USE_RESULT MaybeObject* TryTablePut(Handle<String> source,
                                      JSRegExp::Flags flags,
                                      Handle<FixedArray> data);

  // Note: Returns a new hash table if operation results in expansion.
  Handle<CompilationCacheTable> TablePut(Handle<String> source,
                                         JSRegExp::Flags flags,
                                         Handle<FixedArray> data);

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
  Handle<SharedFunctionInfo> LookupScript(Handle<String> source,
                                          Handle<Object> name,
                                          int line_offset,
                                          int column_offset);

  // Finds the shared function info for a source string for eval in a
  // given context.  Returns an empty handle if the cache doesn't
  // contain a script for the given source string.
  Handle<SharedFunctionInfo> LookupEval(Handle<String> source,
                                        Handle<Context> context,
                                        bool is_global,
                                        StrictModeFlag strict_mode);

  // Returns the regexp data associated with the given regexp if it
  // is in cache, otherwise an empty handle.
  Handle<FixedArray> LookupRegExp(Handle<String> source,
                                  JSRegExp::Flags flags);

  // Associate the (source, kind) pair to the shared function
  // info. This may overwrite an existing mapping.
  void PutScript(Handle<String> source,
                 Handle<SharedFunctionInfo> function_info);

  // Associate the (source, context->closure()->shared(), kind) triple
  // with the shared function info. This may overwrite an existing mapping.
  void PutEval(Handle<String> source,
               Handle<Context> context,
               bool is_global,
               Handle<SharedFunctionInfo> function_info);

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


} }  // namespace v8::internal

#endif  // V8_COMPILATION_CACHE_H_
