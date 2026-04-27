// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SOURCE_TEXT_MODULE_H_
#define V8_OBJECTS_SOURCE_TEXT_MODULE_H_

#include "src/objects/contexts.h"
#include "src/objects/module.h"
#include "src/objects/promise.h"
#include "src/objects/string.h"
#include "src/zone/zone-containers.h"
#include "torque-generated/bit-fields.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class UnorderedModuleSet;
class StructBodyDescriptor;

#include "torque-generated/src/objects/source-text-module-tq.inc"

// The runtime representation of an ECMAScript Source Text Module Record.
// https://tc39.es/ecma262/#sec-source-text-module-records
V8_OBJECT class SourceTextModule : public Module {
 public:
  DECL_VERIFIER(SourceTextModule)
  DECL_PRINTER(SourceTextModule)

  // The shared function info in case {status} is not kEvaluating, kEvaluated or
  // kErrored.
  Tagged<SharedFunctionInfo> GetSharedFunctionInfo() const;

  Tagged<Script> GetScript() const;

  // Whether or not this module contains a toplevel await. Set during module
  // creation and does not change afterwards.
  DECL_BOOLEAN_ACCESSORS(has_toplevel_await)

  // Get the SourceTextModuleInfo associated with the code.
  inline Tagged<SourceTextModuleInfo> info() const;

  Tagged<Cell> GetCell(int cell_index);
  static Handle<Object> LoadVariable(Isolate* isolate,
                                     DirectHandle<SourceTextModule> module,
                                     int cell_index);
  static void StoreVariable(DirectHandle<SourceTextModule> module,
                            int cell_index, DirectHandle<Object> value);

  static int ImportIndex(int cell_index);
  static int ExportIndex(int cell_index);

  // Used by builtins to fulfill or reject the promise associated
  // with async SourceTextModules. Return Nothing if the execution is
  // terminated.
  static Maybe<bool> AsyncModuleExecutionFulfilled(
      Isolate* isolate, Handle<SourceTextModule> module);
  static void AsyncModuleExecutionRejected(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      DirectHandle<Object> exception);

  // Get the namespace object for [module_request] of [module].  If it doesn't
  // exist yet, it is created.
  static DirectHandle<JSModuleNamespace> GetModuleNamespace(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      int module_request_index);

  // Get the import.meta object of [module].  If it doesn't exist yet, it is
  // created and passed to the embedder callback for initialization.
  V8_EXPORT_PRIVATE static MaybeHandle<JSObject> GetImportMeta(
      Isolate* isolate, DirectHandle<SourceTextModule> module);

  static constexpr unsigned kFirstAsyncEvaluationOrdinal = 2;

  enum ExecuteAsyncModuleContextSlots {
    kModule = Context::MIN_CONTEXT_SLOTS,
    kContextLength,
  };

  V8_EXPORT_PRIVATE
  std::pair<DirectHandleVector<SourceTextModule>,
            DirectHandleVector<JSMessageObject>>
  GetStalledTopLevelAwaitMessages(Isolate* isolate);

  static void GatherAsynchronousTransitiveDependencies(
      Isolate* isolate, Handle<Module> module,
      UnorderedModuleSet* evaluation_set,
      ZoneVector<Handle<SourceTextModule>>* evaluation_list,
      UnorderedModuleSet* seen);

  static bool ReadyForSyncExecution(Isolate* isolate, Handle<Module> module,
                                    UnorderedModuleSet* seen);

  inline Tagged<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>>
  code() const;
  inline void set_code(
      Tagged<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> regular_exports() const;
  inline void set_regular_exports(Tagged<FixedArray> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> regular_imports() const;
  inline void set_regular_imports(Tagged<FixedArray> value,
                                  WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> requested_modules() const;
  inline void set_requested_modules(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<TheHole, JSObject>> import_meta(AcquireLoadTag) const;
  inline void set_import_meta(Tagged<UnionOf<TheHole, JSObject>> value,
                              ReleaseStoreTag,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<SourceTextModule, TheHole>> cycle_root() const;
  inline void set_cycle_root(Tagged<UnionOf<SourceTextModule, TheHole>> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<ArrayList> async_parent_modules() const;
  inline void set_async_parent_modules(
      Tagged<ArrayList> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int dfs_index() const;
  inline void set_dfs_index(int value);

  inline int dfs_ancestor_index() const;
  inline void set_dfs_ancestor_index(int value);

  inline int pending_async_dependencies() const;
  inline void set_pending_async_dependencies(int value);

  inline uint32_t flags() const;
  inline void set_flags(uint32_t value);

 private:
  friend class Factory;
  friend class Module;

  struct AsyncEvaluationOrdinalCompare;
  using AvailableAncestorsSet =
      ZoneSet<Handle<SourceTextModule>, AsyncEvaluationOrdinalCompare>;

  // Appends a tuple of module and generator to the async parent modules
  // ArrayList.
  inline static void AddAsyncParentModule(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      DirectHandle<SourceTextModule> parent);

  // Get the non-hole cycle root. Only valid when status >= kEvaluated.
  inline Handle<SourceTextModule> GetCycleRoot(Isolate* isolate) const;

  // Returns a SourceTextModule, the
  // ith parent in depth first traversal order of a given async child.
  inline Handle<SourceTextModule> GetAsyncParentModule(Isolate* isolate,
                                                       uint32_t index);

  // Returns the number of async parent modules for a given async child.
  inline uint32_t AsyncParentModuleCount();

  inline bool HasAsyncEvaluationOrdinal() const;

  inline bool HasPendingAsyncDependencies();
  inline void IncrementPendingAsyncDependencies();
  inline void DecrementPendingAsyncDependencies();

  // Bits for flags.
  DEFINE_TORQUE_GENERATED_SOURCE_TEXT_MODULE_FLAGS()

  // async_evaluation_ordinal, top_level_capability, pending_async_dependencies,
  // and async_parent_modules are used exclusively during evaluation of async
  // modules and the modules which depend on them.
  //
  // If >1, this module is async and evaluating or currently evaluating an async
  // child. The integer is an ordinal for when this module first started async
  // evaluation and is used for sorting async parent modules when determining
  // which parent module can start executing after an async evaluation
  // completes.
  //
  // If 1, this module has finished async evaluating.
  //
  // If 0, this module is not async or has not been async evaluated.
  static constexpr unsigned kNotAsyncEvaluated = 0;
  static constexpr unsigned kAsyncEvaluateDidFinish = 1;
  static_assert(kNotAsyncEvaluated < kAsyncEvaluateDidFinish);
  static_assert(kAsyncEvaluateDidFinish < kFirstAsyncEvaluationOrdinal);
  DECL_PRIMITIVE_ACCESSORS(async_evaluation_ordinal, unsigned)

  // Helpers for Instantiate and Evaluate.
  static void CreateExport(Isolate* isolate,
                           DirectHandle<SourceTextModule> module,
                           int cell_index, DirectHandle<FixedArray> names);
  static void CreateIndirectExport(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      DirectHandle<String> name, DirectHandle<SourceTextModuleInfoEntry> entry);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExport(
      Isolate* isolate, Handle<SourceTextModule> module,
      DirectHandle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);
  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveImport(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      MaybeHandle<String> name, int module_request_index, MessageLocation loc,
      bool must_resolve, ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExportUsingStarExports(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      DirectHandle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      v8::Local<v8::Context> context,
      const Module::UserResolveCallbacks& callbacks);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<SourceTextModule> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index,
      Zone* zone);
  static V8_WARN_UNUSED_RESULT bool RunInitializationCode(
      Isolate* isolate, DirectHandle<SourceTextModule> module);

  static void FetchStarExports(Isolate* isolate,
                               Handle<SourceTextModule> module, Zone* zone,
                               UnorderedModuleSet* visited);

  static void GatherAvailableAncestors(Isolate* isolate, Zone* zone,
                                       Handle<SourceTextModule> start,
                                       AvailableAncestorsSet* exec_list);

  // Implementation of spec concrete method Evaluate.
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> Evaluate(
      Isolate* isolate, Handle<SourceTextModule> module);

  // Implementation of spec abstract operation InnerModuleEvaluation.
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> InnerModuleEvaluation(
      Isolate* isolate, Handle<SourceTextModule> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, unsigned* dfs_index);

  // Returns true if the evaluation exception was catchable by js, and false
  // for termination exceptions.
  bool MaybeHandleEvaluationException(
      Isolate* isolate, ZoneForwardList<Handle<SourceTextModule>>* stack);

  static V8_WARN_UNUSED_RESULT bool MaybeTransitionComponent(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      ZoneForwardList<Handle<SourceTextModule>>* stack, Status new_status);

  // Implementation of spec ExecuteModule is broken up into
  // InnerExecuteAsyncModule for asynchronous modules and ExecuteModule
  // for synchronous modules.
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object>
  InnerExecuteAsyncModule(Isolate* isolate,
                          DirectHandle<SourceTextModule> module,
                          DirectHandle<JSPromise> capability);

  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> ExecuteModule(
      Isolate* isolate, DirectHandle<SourceTextModule> module,
      MaybeDirectHandle<Object>* exception_out);

  // Implementation of spec ExecuteAsyncModule. Return Nothing if the execution
  // is been terminated.
  static V8_WARN_UNUSED_RESULT Maybe<bool> ExecuteAsyncModule(
      Isolate* isolate, DirectHandle<SourceTextModule> module);

  static void Reset(Isolate* isolate, DirectHandle<SourceTextModule> module);

  V8_EXPORT_PRIVATE void InnerGetStalledTopLevelAwaitModule(
      Isolate* isolate, UnorderedModuleSet* visited,
      DirectHandleVector<SourceTextModule>* result);

 public:
  TaggedMember<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>>
      code_;
  TaggedMember<FixedArray> regular_exports_;
  TaggedMember<FixedArray> regular_imports_;
  TaggedMember<FixedArray> requested_modules_;
  TaggedMember<UnionOf<TheHole, JSObject>> import_meta_;
  TaggedMember<UnionOf<SourceTextModule, TheHole>> cycle_root_;
  TaggedMember<ArrayList> async_parent_modules_;
  TaggedMember<Smi> dfs_index_;
  TaggedMember<Smi> dfs_ancestor_index_;
  TaggedMember<Smi> pending_async_dependencies_;
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

template <>
struct ObjectTraits<SourceTextModule> {
  using BodyDescriptor = SubclassBodyDescriptor<
      ObjectTraits<Module>::BodyDescriptor,
      FixedBodyDescriptor<offsetof(SourceTextModule, code_),
                          sizeof(SourceTextModule), sizeof(SourceTextModule)>>;
};

// SourceTextModuleInfo is to SourceTextModuleDescriptor what ScopeInfo is to
// Scope.
class SourceTextModuleInfo : public FixedArray {
 public:
  template <typename IsolateT>
  V8_EXPORT_PRIVATE static DirectHandle<SourceTextModuleInfo> New(
      IsolateT* isolate, Zone* zone, SourceTextModuleDescriptor* descr);

  inline Tagged<FixedArray> module_requests() const;
  inline Tagged<FixedArray> special_exports() const;
  inline Tagged<FixedArray> regular_exports() const;
  inline Tagged<FixedArray> regular_imports() const;
  inline Tagged<FixedArray> namespace_imports() const;

  // Accessors for [regular_exports].
  uint32_t RegularExportCount() const;
  Tagged<String> RegularExportLocalName(int i) const;
  int RegularExportCellIndex(int i) const;
  Tagged<FixedArray> RegularExportExportNames(int i) const;

  inline bool Equals(Tagged<SourceTextModuleInfo> other) const;

 private:
  template <typename Impl>
  friend class FactoryBase;
  friend class SourceTextModuleDescriptor;
  enum : uint32_t {
    kModuleRequestsIndex,
    kSpecialExportsIndex,
    kRegularExportsIndex,
    kNamespaceImportsIndex,
    kRegularImportsIndex,
    kLength
  };
  enum : uint32_t {
    kRegularExportLocalNameOffset,
    kRegularExportCellIndexOffset,
    kRegularExportExportNamesOffset,
    kRegularExportLength
  };
};

V8_OBJECT class ModuleRequest : public StructLayout {
 public:
  inline Tagged<String> specifier() const;
  inline void set_specifier(Tagged<String> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> import_attributes() const;
  inline void set_import_attributes(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline uint32_t flags() const;
  inline void set_flags(uint32_t value);

  DECL_VERIFIER(ModuleRequest)
  DECL_PRINTER(ModuleRequest)

  template <typename IsolateT>
  V8_EXPORT_PRIVATE static Handle<ModuleRequest> New(
      IsolateT* isolate, DirectHandle<String> specifier,
      ModuleImportPhase phase, DirectHandle<FixedArray> import_attributes,
      int position);

  // The number of entries in the import_attributes FixedArray that are used for
  // a single attribute.
  static const size_t kAttributeEntrySize = 3;

  // Bits for flags.
  DEFINE_TORQUE_GENERATED_MODULE_REQUEST_FLAGS()
  static_assert(PositionBits::kMax >= String::kMaxLength,
                "String::kMaxLength should fit in PositionBits::kMax");
  DECL_PRIMITIVE_ACCESSORS(position, unsigned)
  inline void set_phase(ModuleImportPhase phase);
  inline ModuleImportPhase phase() const;

  using BodyDescriptor = StructBodyDescriptor;

 public:
  TaggedMember<String> specifier_;
  TaggedMember<FixedArray> import_attributes_;
  TaggedMember<Smi> flags_;
} V8_OBJECT_END;

V8_OBJECT class SourceTextModuleInfoEntry : public StructLayout {
 public:
  inline Tagged<UnionOf<String, Undefined>> export_name() const;
  inline void set_export_name(Tagged<UnionOf<String, Undefined>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<String, Undefined>> local_name() const;
  inline void set_local_name(Tagged<UnionOf<String, Undefined>> value,
                             WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<String, Undefined>> import_name() const;
  inline void set_import_name(Tagged<UnionOf<String, Undefined>> value,
                              WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline int module_request() const;
  inline void set_module_request(int value);

  inline int cell_index() const;
  inline void set_cell_index(int value);

  inline int beg_pos() const;
  inline void set_beg_pos(int value);

  inline int end_pos() const;
  inline void set_end_pos(int value);

  DECL_VERIFIER(SourceTextModuleInfoEntry)
  DECL_PRINTER(SourceTextModuleInfoEntry)

  template <typename IsolateT>
  V8_EXPORT_PRIVATE static Handle<SourceTextModuleInfoEntry> New(
      IsolateT* isolate, DirectHandle<UnionOf<String, Undefined>> export_name,
      DirectHandle<UnionOf<String, Undefined>> local_name,
      DirectHandle<UnionOf<String, Undefined>> import_name, int module_request,
      int cell_index, int beg_pos, int end_pos);

  using BodyDescriptor = StructBodyDescriptor;

 public:
  TaggedMember<UnionOf<String, Undefined>> export_name_;
  TaggedMember<UnionOf<String, Undefined>> local_name_;
  TaggedMember<UnionOf<String, Undefined>> import_name_;
  TaggedMember<Smi> module_request_;
  TaggedMember<Smi> cell_index_;
  TaggedMember<Smi> beg_pos_;
  TaggedMember<Smi> end_pos_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SOURCE_TEXT_MODULE_H_
