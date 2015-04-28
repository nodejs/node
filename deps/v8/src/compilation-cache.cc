// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/assembler.h"
#include "src/compilation-cache.h"
#include "src/serialize.h"

namespace v8 {
namespace internal {


// The number of generations for each sub cache.
static const int kRegExpGenerations = 2;

// Initial size of each compilation cache table allocated.
static const int kInitialCacheSize = 64;


CompilationCache::CompilationCache(Isolate* isolate)
    : isolate_(isolate),
      script_(isolate, 1),
      eval_global_(isolate, 1),
      eval_contextual_(isolate, 1),
      reg_exp_(isolate, kRegExpGenerations),
      enabled_(true) {
  CompilationSubCache* subcaches[kSubCacheCount] =
    {&script_, &eval_global_, &eval_contextual_, &reg_exp_};
  for (int i = 0; i < kSubCacheCount; ++i) {
    subcaches_[i] = subcaches[i];
  }
}


CompilationCache::~CompilationCache() {}


Handle<CompilationCacheTable> CompilationSubCache::GetTable(int generation) {
  DCHECK(generation < generations_);
  Handle<CompilationCacheTable> result;
  if (tables_[generation]->IsUndefined()) {
    result = CompilationCacheTable::New(isolate(), kInitialCacheSize);
    tables_[generation] = *result;
  } else {
    CompilationCacheTable* table =
        CompilationCacheTable::cast(tables_[generation]);
    result = Handle<CompilationCacheTable>(table, isolate());
  }
  return result;
}


void CompilationSubCache::Age() {
  // Don't directly age single-generation caches.
  if (generations_ == 1) {
    if (tables_[0] != isolate()->heap()->undefined_value()) {
      CompilationCacheTable::cast(tables_[0])->Age();
    }
    return;
  }

  // Age the generations implicitly killing off the oldest.
  for (int i = generations_ - 1; i > 0; i--) {
    tables_[i] = tables_[i - 1];
  }

  // Set the first generation as unborn.
  tables_[0] = isolate()->heap()->undefined_value();
}


void CompilationSubCache::IterateFunctions(ObjectVisitor* v) {
  Object* undefined = isolate()->heap()->undefined_value();
  for (int i = 0; i < generations_; i++) {
    if (tables_[i] != undefined) {
      reinterpret_cast<CompilationCacheTable*>(tables_[i])->IterateElements(v);
    }
  }
}


void CompilationSubCache::Iterate(ObjectVisitor* v) {
  v->VisitPointers(&tables_[0], &tables_[generations_]);
}


void CompilationSubCache::Clear() {
  MemsetPointer(tables_, isolate()->heap()->undefined_value(), generations_);
}


void CompilationSubCache::Remove(Handle<SharedFunctionInfo> function_info) {
  // Probe the script generation tables. Make sure not to leak handles
  // into the caller's handle scope.
  { HandleScope scope(isolate());
    for (int generation = 0; generation < generations(); generation++) {
      Handle<CompilationCacheTable> table = GetTable(generation);
      table->Remove(*function_info);
    }
  }
}


CompilationCacheScript::CompilationCacheScript(Isolate* isolate,
                                               int generations)
    : CompilationSubCache(isolate, generations) {}


// We only re-use a cached function for some script source code if the
// script originates from the same place. This is to avoid issues
// when reporting errors, etc.
bool CompilationCacheScript::HasOrigin(Handle<SharedFunctionInfo> function_info,
                                       Handle<Object> name, int line_offset,
                                       int column_offset,
                                       bool is_embedder_debug_script,
                                       bool is_shared_cross_origin) {
  Handle<Script> script =
      Handle<Script>(Script::cast(function_info->script()), isolate());
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
  // Were both scripts tagged by the embedder as being internal script?
  if (is_embedder_debug_script != script->is_embedder_debug_script()) {
    return false;
  }
  // Were both scripts tagged by the embedder as being shared cross-origin?
  if (is_shared_cross_origin != script->is_shared_cross_origin()) return false;
  // Compare the two name strings for equality.
  return String::Equals(Handle<String>::cast(name),
                        Handle<String>(String::cast(script->name())));
}


// TODO(245): Need to allow identical code from different contexts to
// be cached in the same script generation. Currently the first use
// will be cached, but subsequent code from different source / line
// won't.
Handle<SharedFunctionInfo> CompilationCacheScript::Lookup(
    Handle<String> source, Handle<Object> name, int line_offset,
    int column_offset, bool is_embedder_debug_script,
    bool is_shared_cross_origin, Handle<Context> context,
    LanguageMode language_mode) {
  Object* result = NULL;
  int generation;

  // Probe the script generation tables. Make sure not to leak handles
  // into the caller's handle scope.
  { HandleScope scope(isolate());
    for (generation = 0; generation < generations(); generation++) {
      Handle<CompilationCacheTable> table = GetTable(generation);
      Handle<Object> probe = table->Lookup(source, context, language_mode);
      if (probe->IsSharedFunctionInfo()) {
        Handle<SharedFunctionInfo> function_info =
            Handle<SharedFunctionInfo>::cast(probe);
        // Break when we've found a suitable shared function info that
        // matches the origin.
        if (HasOrigin(function_info, name, line_offset, column_offset,
                      is_embedder_debug_script, is_shared_cross_origin)) {
          result = *function_info;
          break;
        }
      }
    }
  }

  // Once outside the manacles of the handle scope, we need to recheck
  // to see if we actually found a cached script. If so, we return a
  // handle created in the caller's handle scope.
  if (result != NULL) {
    Handle<SharedFunctionInfo> shared(SharedFunctionInfo::cast(result),
                                      isolate());
    DCHECK(HasOrigin(shared, name, line_offset, column_offset,
                     is_embedder_debug_script, is_shared_cross_origin));
    // If the script was found in a later generation, we promote it to
    // the first generation to let it survive longer in the cache.
    if (generation != 0) Put(source, context, language_mode, shared);
    isolate()->counters()->compilation_cache_hits()->Increment();
    return shared;
  } else {
    isolate()->counters()->compilation_cache_misses()->Increment();
    return Handle<SharedFunctionInfo>::null();
  }
}


void CompilationCacheScript::Put(Handle<String> source,
                                 Handle<Context> context,
                                 LanguageMode language_mode,
                                 Handle<SharedFunctionInfo> function_info) {
  HandleScope scope(isolate());
  Handle<CompilationCacheTable> table = GetFirstTable();
  SetFirstTable(CompilationCacheTable::Put(table, source, context,
                                           language_mode, function_info));
}


MaybeHandle<SharedFunctionInfo> CompilationCacheEval::Lookup(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    LanguageMode language_mode, int scope_position) {
  HandleScope scope(isolate());
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  Handle<Object> result = isolate()->factory()->undefined_value();
  int generation;
  for (generation = 0; generation < generations(); generation++) {
    Handle<CompilationCacheTable> table = GetTable(generation);
    result =
        table->LookupEval(source, outer_info, language_mode, scope_position);
    if (result->IsSharedFunctionInfo()) break;
  }
  if (result->IsSharedFunctionInfo()) {
    Handle<SharedFunctionInfo> function_info =
        Handle<SharedFunctionInfo>::cast(result);
    if (generation != 0) {
      Put(source, outer_info, function_info, scope_position);
    }
    isolate()->counters()->compilation_cache_hits()->Increment();
    return scope.CloseAndEscape(function_info);
  } else {
    isolate()->counters()->compilation_cache_misses()->Increment();
    return MaybeHandle<SharedFunctionInfo>();
  }
}


void CompilationCacheEval::Put(Handle<String> source,
                               Handle<SharedFunctionInfo> outer_info,
                               Handle<SharedFunctionInfo> function_info,
                               int scope_position) {
  HandleScope scope(isolate());
  Handle<CompilationCacheTable> table = GetFirstTable();
  table = CompilationCacheTable::PutEval(table, source, outer_info,
                                         function_info, scope_position);
  SetFirstTable(table);
}


MaybeHandle<FixedArray> CompilationCacheRegExp::Lookup(
    Handle<String> source,
    JSRegExp::Flags flags) {
  HandleScope scope(isolate());
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  Handle<Object> result = isolate()->factory()->undefined_value();
  int generation;
  for (generation = 0; generation < generations(); generation++) {
    Handle<CompilationCacheTable> table = GetTable(generation);
    result = table->LookupRegExp(source, flags);
    if (result->IsFixedArray()) break;
  }
  if (result->IsFixedArray()) {
    Handle<FixedArray> data = Handle<FixedArray>::cast(result);
    if (generation != 0) {
      Put(source, flags, data);
    }
    isolate()->counters()->compilation_cache_hits()->Increment();
    return scope.CloseAndEscape(data);
  } else {
    isolate()->counters()->compilation_cache_misses()->Increment();
    return MaybeHandle<FixedArray>();
  }
}


void CompilationCacheRegExp::Put(Handle<String> source,
                                 JSRegExp::Flags flags,
                                 Handle<FixedArray> data) {
  HandleScope scope(isolate());
  Handle<CompilationCacheTable> table = GetFirstTable();
  SetFirstTable(CompilationCacheTable::PutRegExp(table, source, flags, data));
}


void CompilationCache::Remove(Handle<SharedFunctionInfo> function_info) {
  if (!IsEnabled()) return;

  eval_global_.Remove(function_info);
  eval_contextual_.Remove(function_info);
  script_.Remove(function_info);
}


MaybeHandle<SharedFunctionInfo> CompilationCache::LookupScript(
    Handle<String> source, Handle<Object> name, int line_offset,
    int column_offset, bool is_embedder_debug_script,
    bool is_shared_cross_origin, Handle<Context> context,
    LanguageMode language_mode) {
  if (!IsEnabled()) return MaybeHandle<SharedFunctionInfo>();

  return script_.Lookup(source, name, line_offset, column_offset,
                        is_embedder_debug_script, is_shared_cross_origin,
                        context, language_mode);
}


MaybeHandle<SharedFunctionInfo> CompilationCache::LookupEval(
    Handle<String> source, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, LanguageMode language_mode, int scope_position) {
  if (!IsEnabled()) return MaybeHandle<SharedFunctionInfo>();

  MaybeHandle<SharedFunctionInfo> result;
  if (context->IsNativeContext()) {
    result =
        eval_global_.Lookup(source, outer_info, language_mode, scope_position);
  } else {
    DCHECK(scope_position != RelocInfo::kNoPosition);
    result = eval_contextual_.Lookup(source, outer_info, language_mode,
                                     scope_position);
  }
  return result;
}


MaybeHandle<FixedArray> CompilationCache::LookupRegExp(Handle<String> source,
                                                       JSRegExp::Flags flags) {
  if (!IsEnabled()) return MaybeHandle<FixedArray>();

  return reg_exp_.Lookup(source, flags);
}


void CompilationCache::PutScript(Handle<String> source,
                                 Handle<Context> context,
                                 LanguageMode language_mode,
                                 Handle<SharedFunctionInfo> function_info) {
  if (!IsEnabled()) return;

  script_.Put(source, context, language_mode, function_info);
}


void CompilationCache::PutEval(Handle<String> source,
                               Handle<SharedFunctionInfo> outer_info,
                               Handle<Context> context,
                               Handle<SharedFunctionInfo> function_info,
                               int scope_position) {
  if (!IsEnabled()) return;

  HandleScope scope(isolate());
  if (context->IsNativeContext()) {
    eval_global_.Put(source, outer_info, function_info, scope_position);
  } else {
    DCHECK(scope_position != RelocInfo::kNoPosition);
    eval_contextual_.Put(source, outer_info, function_info, scope_position);
  }
}



void CompilationCache::PutRegExp(Handle<String> source,
                                 JSRegExp::Flags flags,
                                 Handle<FixedArray> data) {
  if (!IsEnabled()) {
    return;
  }

  reg_exp_.Put(source, flags, data);
}


void CompilationCache::Clear() {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches_[i]->Clear();
  }
}


void CompilationCache::Iterate(ObjectVisitor* v) {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches_[i]->Iterate(v);
  }
}


void CompilationCache::IterateFunctions(ObjectVisitor* v) {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches_[i]->IterateFunctions(v);
  }
}


void CompilationCache::MarkCompactPrologue() {
  for (int i = 0; i < kSubCacheCount; i++) {
    subcaches_[i]->Age();
  }
}


void CompilationCache::Enable() {
  enabled_ = true;
}


void CompilationCache::Disable() {
  enabled_ = false;
  Clear();
}


} }  // namespace v8::internal
