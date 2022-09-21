// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/gc-info.h"

#include "include/cppgc/internal/name-trait.h"
#include "include/v8config.h"
#include "src/heap/cppgc/gc-info-table.h"

namespace cppgc {
namespace internal {

namespace {

HeapObjectName GetHiddenName(const void*, HeapObjectNameForUnnamedObject) {
  return {NameProvider::kHiddenName, true};
}

}  // namespace

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback, NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, name_callback, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, GetHiddenName, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback,
    NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, name_callback, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, GetHiddenName, true});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback, NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, name_callback, false});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback,
    FinalizationCallback finalization_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index,
      {finalization_callback, trace_callback, GetHiddenName, false});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback,
    NameCallback name_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, name_callback, false});
}

// static
GCInfoIndex EnsureGCInfoIndexTrait::EnsureGCInfoIndexNonPolymorphic(
    std::atomic<GCInfoIndex>& registered_index, TraceCallback trace_callback) {
  return GlobalGCInfoTable::GetMutable().RegisterNewGCInfo(
      registered_index, {nullptr, trace_callback, GetHiddenName, false});
}

}  // namespace internal
}  // namespace cppgc
