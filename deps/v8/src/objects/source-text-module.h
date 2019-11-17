// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SOURCE_TEXT_MODULE_H_
#define V8_OBJECTS_SOURCE_TEXT_MODULE_H_

#include "src/objects/module.h"
#include "src/objects/promise.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class UnorderedModuleSet;

// The runtime representation of an ECMAScript Source Text Module Record.
// https://tc39.github.io/ecma262/#sec-source-text-module-records
class SourceTextModule
    : public TorqueGeneratedSourceTextModule<SourceTextModule, Module> {
 public:
  NEVER_READ_ONLY_SPACE
  DECL_VERIFIER(SourceTextModule)
  DECL_PRINTER(SourceTextModule)

  // The shared function info in case {status} is not kEvaluating, kEvaluated or
  // kErrored.
  SharedFunctionInfo GetSharedFunctionInfo() const;

  // Whether or not this module is an async module. Set during module creation
  // and does not change afterwards.
  DECL_BOOLEAN_ACCESSORS(async)

  // Get the SourceTextModuleInfo associated with the code.
  inline SourceTextModuleInfo info() const;

  Cell GetCell(int cell_index);
  static Handle<Object> LoadVariable(Isolate* isolate,
                                     Handle<SourceTextModule> module,
                                     int cell_index);
  static void StoreVariable(Handle<SourceTextModule> module, int cell_index,
                            Handle<Object> value);

  static int ImportIndex(int cell_index);
  static int ExportIndex(int cell_index);

  // Used by builtins to fulfill or reject the promise associated
  // with async SourceTextModules.
  static void AsyncModuleExecutionFulfilled(Isolate* isolate,
                                            Handle<SourceTextModule> module);
  static void AsyncModuleExecutionRejected(Isolate* isolate,
                                           Handle<SourceTextModule> module,
                                           Handle<Object> exception);

  // Get the namespace object for [module_request] of [module].  If it doesn't
  // exist yet, it is created.
  static Handle<JSModuleNamespace> GetModuleNamespace(
      Isolate* isolate, Handle<SourceTextModule> module, int module_request);

  using BodyDescriptor =
      SubclassBodyDescriptor<Module::BodyDescriptor,
                             FixedBodyDescriptor<kCodeOffset, kSize, kSize>>;

 private:
  friend class Factory;
  friend class Module;

  // Appends a tuple of module and generator to the async parent modules
  // ArrayList.
  inline void AddAsyncParentModule(Isolate* isolate,
                                   Handle<SourceTextModule> module);

  // Returns a SourceTextModule, the
  // ith parent in depth first traversal order of a given async child.
  inline Handle<SourceTextModule> GetAsyncParentModule(Isolate* isolate,
                                                       int index);

  // Returns the number of async parent modules for a given async child.
  inline int AsyncParentModuleCount();

  inline bool HasPendingAsyncDependencies();
  inline void IncrementPendingAsyncDependencies();
  inline void DecrementPendingAsyncDependencies();

  // TODO(neis): Don't store those in the module object?
  DECL_INT_ACCESSORS(dfs_index)
  DECL_INT_ACCESSORS(dfs_ancestor_index)

  // Storage for boolean flags.
  DECL_INT_ACCESSORS(flags)

  // Bits for flags.
  static const int kAsyncBit = 0;
  static const int kAsyncEvaluatingBit = 1;

  // async_evaluating, top_level_capability, pending_async_dependencies, and
  // async_parent_modules are used exclusively during evaluation of async
  // modules and the modules which depend on them.
  //
  // Whether or not this module is async and evaluating or currently evaluating
  // an async child.
  DECL_BOOLEAN_ACCESSORS(async_evaluating)

  // The top level promise capability of this module. Will only be defined
  // for cycle roots.
  DECL_ACCESSORS(top_level_capability, HeapObject)

  // The number of currently evaluating async dependencies of this module.
  DECL_INT_ACCESSORS(pending_async_dependencies)

  // The parent modules of a given async dependency, use async_parent_modules()
  // to retrieve the ArrayList representation.
  DECL_ACCESSORS(async_parent_modules, ArrayList)

  // Helpers for Instantiate and Evaluate.
  static void CreateExport(Isolate* isolate, Handle<SourceTextModule> module,
                           int cell_index, Handle<FixedArray> names);
  static void CreateIndirectExport(Isolate* isolate,
                                   Handle<SourceTextModule> module,
                                   Handle<String> name,
                                   Handle<SourceTextModuleInfoEntry> entry);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExport(
      Isolate* isolate, Handle<SourceTextModule> module,
      Handle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);
  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveImport(
      Isolate* isolate, Handle<SourceTextModule> module, Handle<String> name,
      int module_request, MessageLocation loc, bool must_resolve,
      ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExportUsingStarExports(
      Isolate* isolate, Handle<SourceTextModule> module,
      Handle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, Handle<SourceTextModule> module,
      v8::Local<v8::Context> context, v8::Module::ResolveCallback callback);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<SourceTextModule> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index,
      Zone* zone);
  static V8_WARN_UNUSED_RESULT bool RunInitializationCode(
      Isolate* isolate, Handle<SourceTextModule> module);

  static void FetchStarExports(Isolate* isolate,
                               Handle<SourceTextModule> module, Zone* zone,
                               UnorderedModuleSet* visited);

  // Implementation of spec concrete method Evaluate.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> EvaluateMaybeAsync(
      Isolate* isolate, Handle<SourceTextModule> module);

  // Continued implementation of spec concrete method Evaluate.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> Evaluate(
      Isolate* isolate, Handle<SourceTextModule> module);

  // Implementation of spec abstract operation InnerModuleEvaluation.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> InnerModuleEvaluation(
      Isolate* isolate, Handle<SourceTextModule> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index);

  static V8_WARN_UNUSED_RESULT bool MaybeTransitionComponent(
      Isolate* isolate, Handle<SourceTextModule> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, Status new_status);

  // Implementation of spec GetAsyncCycleRoot.
  static V8_WARN_UNUSED_RESULT Handle<SourceTextModule> GetAsyncCycleRoot(
      Isolate* isolate, Handle<SourceTextModule> module);

  // Implementation of spec ExecuteModule is broken up into
  // InnerExecuteAsyncModule for asynchronous modules and ExecuteModule
  // for synchronous modules.
  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> InnerExecuteAsyncModule(
      Isolate* isolate, Handle<SourceTextModule> module,
      Handle<JSPromise> capability);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Object> ExecuteModule(
      Isolate* isolate, Handle<SourceTextModule> module);

  // Implementation of spec ExecuteAsyncModule.
  static void ExecuteAsyncModule(Isolate* isolate,
                                 Handle<SourceTextModule> module);

  static void Reset(Isolate* isolate, Handle<SourceTextModule> module);

  TQ_OBJECT_CONSTRUCTORS(SourceTextModule)
};

// SourceTextModuleInfo is to SourceTextModuleDescriptor what ScopeInfo is to
// Scope.
class SourceTextModuleInfo : public FixedArray {
 public:
  DECL_CAST(SourceTextModuleInfo)

  static Handle<SourceTextModuleInfo> New(Isolate* isolate, Zone* zone,
                                          SourceTextModuleDescriptor* descr);

  inline FixedArray module_requests() const;
  inline FixedArray special_exports() const;
  inline FixedArray regular_exports() const;
  inline FixedArray regular_imports() const;
  inline FixedArray namespace_imports() const;
  inline FixedArray module_request_positions() const;

  // Accessors for [regular_exports].
  int RegularExportCount() const;
  String RegularExportLocalName(int i) const;
  int RegularExportCellIndex(int i) const;
  FixedArray RegularExportExportNames(int i) const;

#ifdef DEBUG
  inline bool Equals(SourceTextModuleInfo other) const;
#endif

 private:
  friend class Factory;
  friend class SourceTextModuleDescriptor;
  enum {
    kModuleRequestsIndex,
    kSpecialExportsIndex,
    kRegularExportsIndex,
    kNamespaceImportsIndex,
    kRegularImportsIndex,
    kModuleRequestPositionsIndex,
    kLength
  };
  enum {
    kRegularExportLocalNameOffset,
    kRegularExportCellIndexOffset,
    kRegularExportExportNamesOffset,
    kRegularExportLength
  };

  OBJECT_CONSTRUCTORS(SourceTextModuleInfo, FixedArray);
};

class SourceTextModuleInfoEntry
    : public TorqueGeneratedSourceTextModuleInfoEntry<SourceTextModuleInfoEntry,
                                                      Struct> {
 public:
  DECL_PRINTER(SourceTextModuleInfoEntry)
  DECL_VERIFIER(SourceTextModuleInfoEntry)

  DECL_INT_ACCESSORS(module_request)
  DECL_INT_ACCESSORS(cell_index)
  DECL_INT_ACCESSORS(beg_pos)
  DECL_INT_ACCESSORS(end_pos)

  static Handle<SourceTextModuleInfoEntry> New(
      Isolate* isolate, Handle<PrimitiveHeapObject> export_name,
      Handle<PrimitiveHeapObject> local_name,
      Handle<PrimitiveHeapObject> import_name, int module_request,
      int cell_index, int beg_pos, int end_pos);

  TQ_OBJECT_CONSTRUCTORS(SourceTextModuleInfoEntry)
};

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SOURCE_TEXT_MODULE_H_
