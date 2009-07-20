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

#include "v8.h"

#include "compilation-cache.h"

namespace v8 {
namespace internal {


// The number of sub caches covering the different types to cache.
static const int kSubCacheCount = 4;

// The number of generations for each sub cache.
static const int kScriptGenerations = 5;
static const int kEvalGlobalGenerations = 2;
static const int kEvalContextualGenerations = 2;
static const int kRegExpGenerations = 2;

// Initial of each compilation cache table allocated.
static const int kInitialCacheSize = 64;

// The compilation cache consists of several generational sub-caches which uses
// this class as a base class. A sub-cache contains a compilation cache tables
// for each generation of the sub-cache. As the same source code string has
// different compiled code for scripts and evals. Internally, we use separate
// sub-caches to avoid getting the wrong kind of result when looking up.
class CompilationSubCache {
 public:
  explicit CompilationSubCache(int generations): generations_(generations) {
    tables_ = NewArray<Object*>(generations);
  }

  // Get the compilation cache tables for a specific generation.
  Handle<CompilationCacheTable> GetTable(int generation);

  // Age the sub-cache by evicting the oldest generation and creating a new
  // young generation.
  void Age();

  // GC support.
  void Iterate(ObjectVisitor* v);

  // Clear this sub-cache evicting all its content.
  void Clear();

  // Number of generations in this sub-cache.
  inline int generations() { return generations_; }

 private:
  int generations_;  // Number of generations.
  Object** tables_;  // Compilation cache tables - one for each generation.

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationSubCache);
};


// Sub-cache for scripts.
class CompilationCacheScript : public CompilationSubCache {
 public:
  explicit CompilationCacheScript(int generations)
      : CompilationSubCache(generations) { }

  Handle<JSFunction> Lookup(Handle<String> source,
                            Handle<Object> name,
                            int line_offset,
                            int column_offset);
  void Put(Handle<String> source, Handle<JSFunction> boilerplate);

 private:
  bool HasOrigin(Handle<JSFunction> boilerplate,
                 Handle<Object> name,
                 int line_offset,
                 int column_offset);

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheScript);
};


// Sub-cache for eval scripts.
class CompilationCacheEval: public CompilationSubCache {
 public:
  explicit CompilationCacheEval(int generations)
      : CompilationSubCache(generations) { }

  Handle<JSFunction> Lookup(Handle<String> source, Handle<Context> context);

  void Put(Handle<String> source,
           Handle<Context> context,
           Handle<JSFunction> boilerplate);

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheEval);
};


// Sub-cache for regular expressions.
class CompilationCacheRegExp: public CompilationSubCache {
 public:
  explicit CompilationCacheRegExp(int generations)
      : CompilationSubCache(generations) { }

  Handle<FixedArray> Lookup(Handle<String> source, JSRegExp::Flags flags);

  void Put(Handle<String> source,
           JSRegExp::Flags flags,
           Handle<FixedArray> data);

  DISALLOW_IMPLICIT_CONSTRUCTORS(CompilationCacheRegExp);
};


// Statically allocate all the sub-caches.
static CompilationCacheScript script(kScriptGenerations);
static CompilationCacheEval eval_global(kEvalGlobalGenerations);
static CompilationCacheEval eval_contextual(kEvalContextualGenerations);
static CompilationCacheRegExp reg_exp(kRegExpGenerations);
static CompilationSubCache* subcaches[kSubCacheCount] =
    {&script, &eval_global, &eval_contextual, &reg_exp};


// Current enable state of the compilation cache.
static bool enabled = true;
static inline bool IsEnabled() {
  return FLAG_compilation_cache && enabled;
}


static Handle<CompilationCacheTable> AllocateTable(int size) {
  CALL_HEAP_FUNCTION(CompilationCacheTable::Allocate(size),
                     CompilationCacheTable);
}


Handle<CompilationCacheTable> CompilationSubCache::GetTable(int generation) {
  ASSERT(generation < generations_);
  Handle<CompilationCacheTable> result;
  if (tables_[generation]->IsUndefined()) {
    result = AllocateTable(kInitialCacheSize);
    tables_[generation] = *result;
  } else {
    CompilationCacheTable* table =
        CompilationCacheTable::cast(tables_[generation]);
    result = Handle<CompilationCacheTable>(table);
  }
  return result;
}


void CompilationSubCache::Age() {
  // Age the generations implicitly killing off the oldest.
  for (int i = generations_ - 1; i > 0; i--) {
    tables_[i] = tables_[i - 1];
  }

  // Set the first generation as unborn.
  tables_[0] = Heap::undefined_value();
}


void CompilationSubCache::Iterate(ObjectVisitor* v) {
  v->VisitPointers(&tables_[0], &tables_[generations_]);
}


void CompilationSubCache::Clear() {
  for (int i = 0; i < generations_; i++) {
    tables_[i] = Heap::undefined_value();
  }
}


// We only re-use a cached function for some script source code if the
// script originates from the same place. This is to avoid issues
// when reporting errors, etc.
bool CompilationCacheScript::HasOrigin(Handle<JSFunction> boilerplate,
                                       Handle<Object> name,
                                       int line_offset,
                                       int column_offset) {
  Handle<Script> script =
      Handle<Script>(Script::cast(boilerplate->shared()->script()));
  // If the script name isn't set, the boilerplate script should have
  // an undefined name to have the same origin.
  if (name.is_null()) {
    return script->name()->IsUndefined();
  }
  // Do the fast bailout checks first.
  if (line_offset != script->line_offset()->value()) return false;
  if (column_offset != script->column_offset()->value()) return false;
  // Check that both names are strings. If not, no match.
  if (!name->IsString() || !script->name()->IsString()) return false;
  // Compare the two name strings for equality.
  return String::cast(*name)->Equals(String::cast(script->name()));
}


// TODO(245): Need to allow identical code from different contexts to
// be cached in the same script generation. Currently the first use
// will be cached, but subsequent code from different source / line
// won't.
Handle<JSFunction> CompilationCacheScript::Lookup(Handle<String> source,
                                                  Handle<Object> name,
                                                  int line_offset,
                                                  int column_offset) {
  Object* result = NULL;
  int generation;

  // Probe the script generation tables. Make sure not to leak handles
  // into the caller's handle scope.
  { HandleScope scope;
    for (generation = 0; generation < generations(); generation++) {
      Handle<CompilationCacheTable> table = GetTable(generation);
      Handle<Object> probe(table->Lookup(*source));
      if (probe->IsJSFunction()) {
        Handle<JSFunction> boilerplate = Handle<JSFunction>::cast(probe);
        // Break when we've found a suitable boilerplate function that
        // matches the origin.
        if (HasOrigin(boilerplate, name, line_offset, column_offset)) {
          result = *boilerplate;
          break;
        }
      }
    }
  }

  static void* script_histogram = StatsTable::CreateHistogram(
      "V8.ScriptCache",
      0,
      kScriptGenerations,
      kScriptGenerations + 1);

  if (script_histogram != NULL) {
    // The level NUMBER_OF_SCRIPT_GENERATIONS is equivalent to a cache miss.
    StatsTable::AddHistogramSample(script_histogram, generation);
  }

  // Once outside the manacles of the handle scope, we need to recheck
  // to see if we actually found a cached script. If so, we return a
  // handle created in the caller's handle scope.
  if (result != NULL) {
    Handle<JSFunction> boilerplate(JSFunction::cast(result));
    ASSERT(HasOrigin(boilerplate, name, line_offset, column_offset));
    // If the script was found in a later generation, we promote it to
    // the first generation to let it survive longer in the cache.
    if (generation != 0) Put(source, boilerplate);
    Counters::compilation_cache_hits.Increment();
    return boilerplate;
  } else {
    Counters::compilation_cache_misses.Increment();
    return Handle<JSFunction>::null();
  }
}


void CompilationCacheScript::Put(Handle<String> source,
                                 Handle<JSFunction> boilerplate) {
  HandleScope scope;
  ASSERT(boilerplate->IsBoilerplate());
  Handle<CompilationCacheTable> table = GetTable(0);
  // TODO(X64): -fstrict-aliasing causes a problem with table.  Fix it.
  CALL_HEAP_FUNCTION_VOID(table->Put(*source, *boilerplate));
}


Handle<JSFunction> CompilationCacheEval::Lookup(Handle<String> source,
                                                Handle<Context> context) {
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  Object* result = NULL;
  int generation;
  { HandleScope scope;
    for (generation = 0; generation < generations(); generation++) {
      Handle<CompilationCacheTable> table = GetTable(generation);
      result = table->LookupEval(*source, *context);
      if (result->IsJSFunction()) {
        break;
      }
    }
  }
  if (result->IsJSFunction()) {
    Handle<JSFunction> boilerplate(JSFunction::cast(result));
    if (generation != 0) {
      Put(source, context, boilerplate);
    }
    Counters::compilation_cache_hits.Increment();
    return boilerplate;
  } else {
    Counters::compilation_cache_misses.Increment();
    return Handle<JSFunction>::null();
  }
}


void CompilationCacheEval::Put(Handle<String> source,
                               Handle<Context> context,
                               Handle<JSFunction> boilerplate) {
  HandleScope scope;
  ASSERT(boilerplate->IsBoilerplate());
  Handle<CompilationCacheTable> table = GetTable(0);
  CALL_HEAP_FUNCTION_VOID(table->PutEval(*source, *context, *boilerplate));
}


Handle<FixedArray> CompilationCacheRegExp::Lookup(Handle<String> source,
                                                  JSRegExp::Flags flags) {
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  Object* result = NULL;
  int generation;
  { HandleScope scope;
    for (generation = 0; generation < generations(); generation++) {
      Handle<CompilationCacheTable> table = GetTable(generation);
      result = table->LookupRegExp(*source, flags);
      if (result->IsFixedArray()) {
        break;
      }
    }
  }
  if (result->IsFixedArray()) {
    Handle<FixedArray> data(FixedArray::cast(result));
    if (generation != 0) {
      Put(source, flags, data);
    }
    Counters::compilation_cache_hits.Increment();
    return data;
  } else {
    Counters::compilation_cache_misses.Increment();
    return Handle<FixedArray>::null();
  }
}


void CompilationCacheRegExp::Put(Handle<String> source,
                                 JSRegExp::Flags flags,
                                 Handle<FixedArray> data) {
  HandleScope scope;
  Handle<CompilationCacheTable> table = GetTable(0);
  CALL_HEAP_FUNCTION_VOID(table->PutRegExp(*source, flags, *data));
}


Handle<JSFunction> CompilationCache::LookupScript(Handle<String> source,
                                                  Handle<Object> name,
                                                  int line_offset,
                                                  int column_offset) {
  if (!IsEnabled()) {
    return Handle<JSFunction>::null();
  }

  return script.Lookup(source, name, line_offset, column_offset);
}


Handle<JSFunction> CompilationCache::LookupEval(Handle<String> source,
                                                Handle<Context> context,
                                                bool is_global) {
  if (!IsEnabled()) {
    return Handle<JSFunction>::null();
  }

  Handle<JSFunction> result;
  if (is_global) {
    result = eval_global.Lookup(source, context);
  } else {
    result = eval_contextual.Lookup(source, context);
  }
  return result;
}


Handle<FixedArray> CompilationCache::LookupRegExp(Handle<String> source,
                                                  JSRegExp::Flags flags) {
  if (!IsEnabled()) {
    return Handle<FixedArray>::null();
  }

  return reg_exp.Lookup(source, flags);
}


void CompilationCache::PutScript(Handle<String> source,
                                 Handle<JSFunction> boilerplate) {
  if (!IsEnabled()) {
    return;
  }

  ASSERT(boilerplate->IsBoilerplate());
  script.Put(source, boilerplate);
}


void CompilationCache::PutEval(Handle<String> source,
                               Handle<Context> context,
                               bool is_global,
                               Handle<JSFunction> boilerplate) {
  if (!IsEnabled()) {
    return;
  }

  HandleScope scope;
  ASSERT(boilerplate->IsBoilerplate());
  if (is_global) {
    eval_global.Put(source, context, boilerplate);
  } else {
    eval_contextual.Put(source, context, boilerplate);
  }
}



void CompilationCache::PutRegExp(Handle<String> source,
                                 JSRegExp::Flags flags,
                                 Handle<FixedArray> data) {
  if (!IsEnabled()) {
    return;
  }

  reg_exp.Put(source, flags, data);
}


void CompilationCache::Clear() {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches[i]->Clear();
  }
}


void CompilationCache::Iterate(ObjectVisitor* v) {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches[i]->Iterate(v);
  }
}


void CompilationCache::MarkCompactPrologue() {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches[i]->Age();
  }
}


void CompilationCache::Enable() {
  enabled = true;
}


void CompilationCache::Disable() {
  enabled = false;
  Clear();
}


} }  // namespace v8::internal
