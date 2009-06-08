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

enum {
  // The number of script generations tell how many GCs a script can
  // survive in the compilation cache, before it will be flushed if it
  // hasn't been used.
  NUMBER_OF_SCRIPT_GENERATIONS = 5,

  // The compilation cache consists of tables - one for each entry
  // kind plus extras for the script generations.
  NUMBER_OF_TABLE_ENTRIES =
      CompilationCache::LAST_ENTRY + NUMBER_OF_SCRIPT_GENERATIONS
};


// Current enable state of the compilation cache.
static bool enabled = true;
static inline bool IsEnabled() {
  return FLAG_compilation_cache && enabled;
}

// Keep separate tables for the different entry kinds.
static Object* tables[NUMBER_OF_TABLE_ENTRIES] = { 0, };


static Handle<CompilationCacheTable> AllocateTable(int size) {
  CALL_HEAP_FUNCTION(CompilationCacheTable::Allocate(size),
                     CompilationCacheTable);
}


static Handle<CompilationCacheTable> GetTable(int index) {
  ASSERT(index >= 0 && index < NUMBER_OF_TABLE_ENTRIES);
  Handle<CompilationCacheTable> result;
  if (tables[index]->IsUndefined()) {
    static const int kInitialCacheSize = 64;
    result = AllocateTable(kInitialCacheSize);
    tables[index] = *result;
  } else {
    CompilationCacheTable* table = CompilationCacheTable::cast(tables[index]);
    result = Handle<CompilationCacheTable>(table);
  }
  return result;
}


static Handle<JSFunction> Lookup(Handle<String> source,
                                 Handle<Context> context,
                                 CompilationCache::Entry entry) {
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  Object* result;
  { HandleScope scope;
    Handle<CompilationCacheTable> table = GetTable(entry);
    result = table->LookupEval(*source, *context);
  }
  if (result->IsJSFunction()) {
    return Handle<JSFunction>(JSFunction::cast(result));
  } else {
    return Handle<JSFunction>::null();
  }
}


static Handle<FixedArray> Lookup(Handle<String> source,
                                 JSRegExp::Flags flags) {
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  Object* result;
  { HandleScope scope;
    Handle<CompilationCacheTable> table = GetTable(CompilationCache::REGEXP);
    result = table->LookupRegExp(*source, flags);
  }
  if (result->IsFixedArray()) {
    return Handle<FixedArray>(FixedArray::cast(result));
  } else {
    return Handle<FixedArray>::null();
  }
}


// We only re-use a cached function for some script source code if the
// script originates from the same place. This is to avoid issues
// when reporting errors, etc.
static bool HasOrigin(Handle<JSFunction> boilerplate,
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
Handle<JSFunction> CompilationCache::LookupScript(Handle<String> source,
                                                  Handle<Object> name,
                                                  int line_offset,
                                                  int column_offset) {
  if (!IsEnabled()) {
    return Handle<JSFunction>::null();
  }

  // Use an int for the generation index, so value range propagation
  // in gcc 4.3+ won't assume it can only go up to LAST_ENTRY when in
  // fact it can go up to SCRIPT + NUMBER_OF_SCRIPT_GENERATIONS.
  int generation = SCRIPT;
  Object* result = NULL;

  // Probe the script generation tables. Make sure not to leak handles
  // into the caller's handle scope.
  { HandleScope scope;
    while (generation < SCRIPT + NUMBER_OF_SCRIPT_GENERATIONS) {
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
      // Go to the next generation.
      generation++;
    }
  }

  static void* script_histogram = StatsTable::CreateHistogram(
      "V8.ScriptCache",
      0,
      NUMBER_OF_SCRIPT_GENERATIONS,
      NUMBER_OF_SCRIPT_GENERATIONS + 1);

  if (script_histogram != NULL) {
    // The level NUMBER_OF_SCRIPT_GENERATIONS is equivalent to a cache miss.
    StatsTable::AddHistogramSample(script_histogram, generation - SCRIPT);
  }

  // Once outside the manacles of the handle scope, we need to recheck
  // to see if we actually found a cached script. If so, we return a
  // handle created in the caller's handle scope.
  if (result != NULL) {
    Handle<JSFunction> boilerplate(JSFunction::cast(result));
    ASSERT(HasOrigin(boilerplate, name, line_offset, column_offset));
    // If the script was found in a later generation, we promote it to
    // the first generation to let it survive longer in the cache.
    if (generation != SCRIPT) PutScript(source, boilerplate);
    Counters::compilation_cache_hits.Increment();
    return boilerplate;
  } else {
    Counters::compilation_cache_misses.Increment();
    return Handle<JSFunction>::null();
  }
}


Handle<JSFunction> CompilationCache::LookupEval(Handle<String> source,
                                                Handle<Context> context,
                                                Entry entry) {
  if (!IsEnabled()) {
    return Handle<JSFunction>::null();
  }

  ASSERT(entry == EVAL_GLOBAL || entry == EVAL_CONTEXTUAL);
  Handle<JSFunction> result = Lookup(source, context, entry);
  if (result.is_null()) {
    Counters::compilation_cache_misses.Increment();
  } else {
    Counters::compilation_cache_hits.Increment();
  }
  return result;
}


Handle<FixedArray> CompilationCache::LookupRegExp(Handle<String> source,
                                                  JSRegExp::Flags flags) {
  if (!IsEnabled()) {
    return Handle<FixedArray>::null();
  }

  Handle<FixedArray> result = Lookup(source, flags);
  if (result.is_null()) {
    Counters::compilation_cache_misses.Increment();
  } else {
    Counters::compilation_cache_hits.Increment();
  }
  return result;
}


void CompilationCache::PutScript(Handle<String> source,
                                 Handle<JSFunction> boilerplate) {
  if (!IsEnabled()) {
    return;
  }

  HandleScope scope;
  ASSERT(boilerplate->IsBoilerplate());
  Handle<CompilationCacheTable> table = GetTable(SCRIPT);
  CALL_HEAP_FUNCTION_VOID(table->Put(*source, *boilerplate));
}


void CompilationCache::PutEval(Handle<String> source,
                               Handle<Context> context,
                               Entry entry,
                               Handle<JSFunction> boilerplate) {
  if (!IsEnabled()) {
    return;
  }

  HandleScope scope;
  ASSERT(boilerplate->IsBoilerplate());
  Handle<CompilationCacheTable> table = GetTable(entry);
  CALL_HEAP_FUNCTION_VOID(table->PutEval(*source, *context, *boilerplate));
}



void CompilationCache::PutRegExp(Handle<String> source,
                                 JSRegExp::Flags flags,
                                 Handle<FixedArray> data) {
  if (!IsEnabled()) {
    return;
  }

  HandleScope scope;
  Handle<CompilationCacheTable> table = GetTable(REGEXP);
  CALL_HEAP_FUNCTION_VOID(table->PutRegExp(*source, flags, *data));
}


void CompilationCache::Clear() {
  for (int i = 0; i < NUMBER_OF_TABLE_ENTRIES; i++) {
    tables[i] = Heap::undefined_value();
  }
}


void CompilationCache::Iterate(ObjectVisitor* v) {
  v->VisitPointers(&tables[0], &tables[NUMBER_OF_TABLE_ENTRIES]);
}


void CompilationCache::MarkCompactPrologue() {
  ASSERT(LAST_ENTRY == SCRIPT);
  for (int i = NUMBER_OF_TABLE_ENTRIES - 1; i > SCRIPT; i--) {
    tables[i] = tables[i - 1];
  }
  for (int j = 0; j <= LAST_ENTRY; j++) {
    tables[j] = Heap::undefined_value();
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
