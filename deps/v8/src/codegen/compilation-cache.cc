// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/compilation-cache.h"

#include "src/common/globals.h"
#include "src/heap/factory.h"
#include "src/logging/counters.h"
#include "src/logging/log.h"
#include "src/objects/compilation-cache-table-inl.h"
#include "src/objects/objects-inl.h"
#include "src/objects/objects.h"
#include "src/objects/slots.h"
#include "src/objects/visitors.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

// Initial size of each compilation cache table allocated.
static const int kInitialCacheSize = 64;

CompilationCache::CompilationCache(Isolate* isolate)
    : isolate_(isolate),
      script_(isolate),
      eval_global_(isolate),
      eval_contextual_(isolate),
      reg_exp_(isolate),
      enabled_script_and_eval_(true) {}

Handle<CompilationCacheTable> CompilationCacheEvalOrScript::GetTable() {
  if (IsUndefined(table_, isolate())) {
    return CompilationCacheTable::New(isolate(), kInitialCacheSize);
  }
  return handle(Cast<CompilationCacheTable>(table_), isolate());
}

DirectHandle<CompilationCacheTable> CompilationCacheRegExp::GetTable(
    int generation) {
  DCHECK_LT(generation, kGenerations);
  DirectHandle<CompilationCacheTable> result;
  if (IsUndefined(tables_[generation], isolate())) {
    result = CompilationCacheTable::New(isolate(), kInitialCacheSize);
    tables_[generation] = *result;
  } else {
    Tagged<CompilationCacheTable> table =
        Cast<CompilationCacheTable>(tables_[generation]);
    result = DirectHandle<CompilationCacheTable>(table, isolate());
  }
  return result;
}

void CompilationCacheRegExp::Age() {
  static_assert(kGenerations > 1);

  // Age the generations implicitly killing off the oldest.
  for (int i = kGenerations - 1; i > 0; i--) {
    tables_[i] = tables_[i - 1];
  }

  // Set the first generation as unborn.
  tables_[0] = ReadOnlyRoots(isolate()).undefined_value();
}

void CompilationCacheScript::Age() {
  DisallowGarbageCollection no_gc;
  if (IsUndefined(table_, isolate())) return;
  Tagged<CompilationCacheTable> table = Cast<CompilationCacheTable>(table_);

  for (InternalIndex entry : table->IterateEntries()) {
    Tagged<Object> key;
    if (!table->ToKey(isolate(), entry, &key)) continue;
    DCHECK(IsWeakFixedArray(key));

    Tagged<Object> value = table->PrimaryValueAt(entry);
    if (!IsUndefined(value, isolate())) {
      Tagged<SharedFunctionInfo> info = Cast<SharedFunctionInfo>(value);
      // Clear entries after Bytecode was flushed from SFI.
      if (!info->HasBytecodeArray()) {
        table->SetPrimaryValueAt(entry,
                                 ReadOnlyRoots(isolate()).undefined_value(),
                                 SKIP_WRITE_BARRIER);
      }
    }
  }
}

void CompilationCacheEval::Age() {
  DisallowGarbageCollection no_gc;
  if (IsUndefined(table_, isolate())) return;
  Tagged<CompilationCacheTable> table = Cast<CompilationCacheTable>(table_);

  for (InternalIndex entry : table->IterateEntries()) {
    Tagged<Object> key;
    if (!table->ToKey(isolate(), entry, &key)) continue;

    DCHECK(IsFixedArray(key));
    // The ageing mechanism for eval caches.
    Tagged<SharedFunctionInfo> info =
        Cast<SharedFunctionInfo>(table->PrimaryValueAt(entry));
    // Clear entries after Bytecode was flushed from SFI.
    if (!info->HasBytecodeArray()) {
      table->RemoveEntry(entry);
    }
  }
}

void CompilationCacheEvalOrScript::Iterate(RootVisitor* v) {
  v->VisitRootPointer(Root::kCompilationCache, nullptr,
                      FullObjectSlot(&table_));
}

void CompilationCacheRegExp::Iterate(RootVisitor* v) {
  v->VisitRootPointers(Root::kCompilationCache, nullptr,
                       FullObjectSlot(&tables_[0]),
                       FullObjectSlot(&tables_[kGenerations]));
}

void CompilationCacheEvalOrScript::Clear() {
  table_ = ReadOnlyRoots(isolate()).undefined_value();
}

void CompilationCacheRegExp::Clear() {
  MemsetPointer(reinterpret_cast<Address*>(tables_),
                ReadOnlyRoots(isolate()).undefined_value().ptr(), kGenerations);
}

void CompilationCacheEvalOrScript::Remove(
    DirectHandle<SharedFunctionInfo> function_info) {
  if (IsUndefined(table_, isolate())) return;
  Cast<CompilationCacheTable>(table_)->Remove(*function_info);
}

CompilationCacheScript::LookupResult CompilationCacheScript::Lookup(
    Handle<String> source, const ScriptDetails& script_details) {
  DirectHandle<CompilationCacheTable> table = GetTable();
  LookupResult result = CompilationCacheTable::LookupScript(
      table, source, script_details, isolate());

  DirectHandle<Script> script;
  if (result.script().ToHandle(&script)) {
    DirectHandle<SharedFunctionInfo> sfi;
    if (result.toplevel_sfi().ToHandle(&sfi)) {
      isolate()->counters()->compilation_cache_hits()->Increment();
      LOG(isolate(), CompilationCacheEvent("hit", "script", *sfi));
    } else {
      isolate()->counters()->compilation_cache_partial_hits()->Increment();
    }
  } else {
    isolate()->counters()->compilation_cache_misses()->Increment();
  }
  return result;
}

void CompilationCacheScript::Put(
    Handle<String> source, DirectHandle<SharedFunctionInfo> function_info) {
  Handle<CompilationCacheTable> table = GetTable();
  table_ = *CompilationCacheTable::PutScript(table, source, kNullMaybeHandle,
                                             function_info, isolate());
}

void CompilationCacheEval::UpdateEval(
    DirectHandle<String> source, DirectHandle<SharedFunctionInfo> outer_info,
    DirectHandle<JSFunction> js_function, LanguageMode language_mode,
    int position) {
  DirectHandle<CompilationCacheTable> table = GetTable();
  CompilationCacheTable::UpdateEval(table, source, outer_info, js_function,
                                    language_mode, position);
}

InfoCellPair CompilationCacheEval::Lookup(
    DirectHandle<String> source, DirectHandle<SharedFunctionInfo> outer_info,
    DirectHandle<NativeContext> native_context, LanguageMode language_mode,
    int position) {
  InfoCellPair result;
  DirectHandle<CompilationCacheTable> table = GetTable();
  result = CompilationCacheTable::LookupEval(
      table, source, outer_info, native_context, language_mode, position);
  if (result.has_shared()) {
    isolate()->counters()->compilation_cache_hits()->Increment();
  } else {
    isolate()->counters()->compilation_cache_misses()->Increment();
  }
  return result;
}

void CompilationCacheEval::Put(DirectHandle<String> source,
                               DirectHandle<SharedFunctionInfo> outer_info,
                               DirectHandle<JSFunction> js_function,
                               int position) {
  DirectHandle<CompilationCacheTable> table = GetTable();
  table_ = *CompilationCacheTable::PutEval(table, source, outer_info,
                                           js_function, position);
}

MaybeDirectHandle<RegExpData> CompilationCacheRegExp::Lookup(
    DirectHandle<String> source, JSRegExp::Flags flags) {
  HandleScope scope(isolate());
  // Make sure not to leak the table into the surrounding handle
  // scope. Otherwise, we risk keeping old tables around even after
  // having cleared the cache.
  DirectHandle<Object> result = isolate()->factory()->undefined_value();
  int generation;
  for (generation = 0; generation < kGenerations; generation++) {
    DirectHandle<CompilationCacheTable> table = GetTable(generation);
    result = table->LookupRegExp(source, flags);
    if (IsRegExpDataWrapper(*result)) break;
  }
  if (IsRegExpDataWrapper(*result)) {
    Handle<RegExpData> data(Cast<RegExpDataWrapper>(result)->data(isolate()),
                            isolate());
    if (generation != 0) {
      Put(source, flags, data);
    }
    isolate()->counters()->compilation_cache_hits()->Increment();
    return scope.CloseAndEscape(data);
  } else {
    isolate()->counters()->compilation_cache_misses()->Increment();
    return MaybeDirectHandle<RegExpData>();
  }
}

void CompilationCacheRegExp::Put(DirectHandle<String> source,
                                 JSRegExp::Flags flags,
                                 DirectHandle<RegExpData> data) {
  HandleScope scope(isolate());
  DirectHandle<CompilationCacheTable> table = GetTable(0);
  tables_[0] =
      *CompilationCacheTable::PutRegExp(isolate(), table, source, flags, data);
}

void CompilationCache::Remove(DirectHandle<SharedFunctionInfo> function_info) {
  if (!IsEnabledScriptAndEval()) return;

  eval_global_.Remove(function_info);
  eval_contextual_.Remove(function_info);
  script_.Remove(function_info);
}

CompilationCacheScript::LookupResult CompilationCache::LookupScript(
    Handle<String> source, const ScriptDetails& script_details,
    LanguageMode language_mode) {
  if (!IsEnabledScript(language_mode)) return {};
  return script_.Lookup(source, script_details);
}

void CompilationCache::UpdateEval(DirectHandle<String> source,
                                  DirectHandle<SharedFunctionInfo> outer_info,
                                  DirectHandle<JSFunction> js_function,
                                  LanguageMode language_mode, int position) {
  if (IsNativeContext(js_function->context())) {
    eval_global_.UpdateEval(source, outer_info, js_function, language_mode,
                            position);
  } else {
    DCHECK_NE(position, kNoSourcePosition);
    eval_contextual_.UpdateEval(source, outer_info, js_function, language_mode,
                                position);
  }
}

InfoCellPair CompilationCache::LookupEval(
    DirectHandle<String> source, DirectHandle<SharedFunctionInfo> outer_info,
    DirectHandle<Context> context, LanguageMode language_mode, int position) {
  InfoCellPair result;
  if (!IsEnabledScriptAndEval()) return result;

  const char* cache_type;

  DirectHandle<NativeContext> maybe_native_context;
  if (TryCast<NativeContext>(context, &maybe_native_context)) {
    result = eval_global_.Lookup(source, outer_info, maybe_native_context,
                                 language_mode, position);
    cache_type = "eval-global";

  } else {
    DCHECK_NE(position, kNoSourcePosition);
    DirectHandle<NativeContext> native_context(context->native_context(),
                                               isolate());
    result = eval_contextual_.Lookup(source, outer_info, native_context,
                                     language_mode, position);
    cache_type = "eval-contextual";
  }

  if (result.has_shared()) {
    LOG(isolate(), CompilationCacheEvent("hit", cache_type, result.shared()));
  }

  return result;
}

MaybeDirectHandle<RegExpData> CompilationCache::LookupRegExp(
    DirectHandle<String> source, JSRegExp::Flags flags) {
  return reg_exp_.Lookup(source, flags);
}

void CompilationCache::PutScript(
    Handle<String> source, LanguageMode language_mode,
    DirectHandle<SharedFunctionInfo> function_info) {
  if (!IsEnabledScript(language_mode)) return;
  LOG(isolate(), CompilationCacheEvent("put", "script", *function_info));

  script_.Put(source, function_info);
}

void CompilationCache::PutEval(DirectHandle<String> source,
                               DirectHandle<SharedFunctionInfo> outer_info,
                               DirectHandle<JSFunction> js_function,
                               int position) {
  if (!IsEnabledScriptAndEval()) return;

  const char* cache_type;
  if (IsNativeContext(js_function->context())) {
    eval_global_.Put(source, outer_info, js_function, position);
    cache_type = "eval-global";
  } else {
    DCHECK_NE(position, kNoSourcePosition);
    eval_contextual_.Put(source, outer_info, js_function, position);
    cache_type = "eval-contextual";
  }
  LOG(isolate(),
      CompilationCacheEvent("put", cache_type, js_function->shared()));
}

void CompilationCache::PutRegExp(DirectHandle<String> source,
                                 JSRegExp::Flags flags,
                                 DirectHandle<RegExpData> data) {
  reg_exp_.Put(source, flags, data);
}

void CompilationCache::Clear() {
  script_.Clear();
  eval_global_.Clear();
  eval_contextual_.Clear();
  reg_exp_.Clear();
}

void CompilationCache::Iterate(RootVisitor* v) {
  script_.Iterate(v);
  eval_global_.Iterate(v);
  eval_contextual_.Iterate(v);
  reg_exp_.Iterate(v);
}

void CompilationCache::MarkCompactPrologue() {
  // Drop SFI entries with flushed bytecode.
  script_.Age();
  eval_global_.Age();
  eval_contextual_.Age();

  // Drop entries in oldest generation.
  reg_exp_.Age();
}

void CompilationCache::EnableScriptAndEval() {
  enabled_script_and_eval_ = true;
}

void CompilationCache::DisableScriptAndEval() {
  enabled_script_and_eval_ = false;
  Clear();
}

}  // namespace internal
}  // namespace v8
