// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_SOURCE_TEXT_MODULE_H_
#define V8_OBJECTS_SOURCE_TEXT_MODULE_H_

#include "src/base/bit-field.h"
#include "src/objects/bit-field-group.h"
#include "src/objects/contexts.h"
#include "src/objects/module.h"
#include "src/objects/promise.h"
#include "src/objects/string.h"
#include "src/zone/zone-containers.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class UnorderedModuleSet;
class StructBodyDescriptor;

// The runtime representation of an ECMAScript Source Text Module Record.
// https://tc39.es/ecma262/#sec-source-text-module-records
V8_OBJECT class SourceTextModule : public Module {
 public:
  DECL_VERIFIER(SourceTextModule)
  DECL_PRINTER(SourceTextModule)

#if defined(DEBUG) || defined(VERIFY_HEAP)
  void VerifyRequestedModules() const;
#endif

  // The shared function info of the module body, retrieved through {code}
  // according to {status}. Valid in every status.
  V8_EXPORT_PRIVATE Tagged<SharedFunctionInfo> GetSharedFunctionInfo() const;

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

  // https://tc39.es/proposal-defer-import-eval/#sec-IsModuleSCCEvaluated
  // This function checks if the Strongly Connected Component (SCC) that the
  // module participates is evaluated.
  static bool IsModuleSCCEvaluated(DirectHandle<SourceTextModule> module);

  static void GatherAsynchronousTransitiveDependencies(
      Isolate* isolate, Handle<Module> module,
      UnorderedModuleSet* evaluation_set,
      ZoneVector<Handle<SourceTextModule>>* evaluation_list,
      UnorderedModuleSet* seen);

  static bool ReadyForSyncExecution(Isolate* isolate, Handle<Module> module,
                                    UnorderedModuleSet* seen);

  template <Status... kStates>
  using InState = Module::InState<SourceTextModule, kStates...>;

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

  struct Flags : BitFieldGroup<Flags, uint32_t> {
    using BitFieldGroup::BitFieldGroup;

    using HasToplevelAwaitBit = First<bool, 1>;
    using AsyncEvaluationOrdinalBits = HasToplevelAwaitBit::Next<uint32_t, 30>;

    BIT_FIELD_GROUP_ACCESSORS(has_toplevel_await, HasToplevelAwaitBit)
    BIT_FIELD_GROUP_ACCESSORS(async_evaluation_ordinal,
                              AsyncEvaluationOrdinalBits)
  };

 private:
  inline Flags flags() const;
  inline void set_flags(Flags value);

  friend class Factory;
  friend class Module;
  template <typename ModuleT, Status... kStates>
  friend class Module::InState;

  inline Tagged<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>>
  code() const;
  inline void set_code(
      Tagged<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // [[CycleRoot]]: the root of this module's strongly connected component, set
  // when the component transitions out of kEvaluating. It stays TheHole if the
  // component fails synchronously; a module that fails asynchronously keeps
  // it, and its cycle root fails as well.
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

  struct AsyncEvaluationOrdinalCompare;
  using AvailableAncestorsSet =
      ZoneSet<Handle<SourceTextModule>, AsyncEvaluationOrdinalCompare>;

  // Appends a parent module to the async parent modules ArrayList of an
  // async-evaluating module.
  inline static void AddAsyncParentModule(
      Isolate* isolate,
      DirectHandle<InState<kEvaluating, kEvaluatingAsync>> module,
      DirectHandle<InState<kEvaluating>> parent);

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

  // Atomic state transitions:
  static Handle<InState<kLinking>> TransitionToLinking(
      Isolate* isolate, Handle<InState<kPreLinking>> module,
      ZoneForwardList<Handle<InState<kLinking>>>* stack, unsigned* dfs_index);
  template <Status kFromState, typename TransitionFn>
  static void TransitionComponent(
      DirectHandle<InState<kFromState>> module,
      ZoneForwardList<Handle<InState<kFromState>>>* stack, Status trace_status,
      TransitionFn&& transition_ancestor);
  static V8_WARN_UNUSED_RESULT bool TransitionComponentToLinked(
      Isolate* isolate, DirectHandle<InState<kLinking>> module,
      ZoneForwardList<Handle<InState<kLinking>>>* stack);
  static void TransitionComponentAfterEvaluation(
      DirectHandle<InState<kEvaluating>> module,
      ZoneForwardList<Handle<InState<kEvaluating>>>* stack);
  static Handle<InState<kEvaluated>> TransitionAsyncToEvaluated(
      Isolate* isolate, Handle<InState<kEvaluatingAsync>> module);

  // Helpers for Instantiate and Evaluate.
  static void CreateExport(Isolate* isolate,
                           DirectHandle<InState<kPreLinking>> module,
                           int cell_index, DirectHandle<FixedArray> names);
  static void CreateIndirectExport(
      Isolate* isolate, DirectHandle<InState<kPreLinking>> module,
      DirectHandle<String> name, DirectHandle<SourceTextModuleInfoEntry> entry);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExport(
      Isolate* isolate,
      Handle<InState<kPreLinking, kLinking, kLinked, kEvaluatingAsync,
                     kEvaluated, kEvaluatedError>>
          module,
      DirectHandle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);
  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveImport(
      Isolate* isolate,
      DirectHandle<InState<kPreLinking, kLinking, kLinked, kEvaluatingAsync,
                           kEvaluated, kEvaluatedError>>
          module,
      MaybeHandle<String> name, int module_request_index, MessageLocation loc,
      bool must_resolve, ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExportUsingStarExports(
      Isolate* isolate,
      DirectHandle<InState<kPreLinking, kLinking, kLinked, kEvaluatingAsync,
                           kEvaluated, kEvaluatedError>>
          module,
      DirectHandle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, DirectHandle<InState<kPreLinking>> module,
      v8::Local<v8::Context> context,
      const Module::UserResolveCallbacks& callbacks);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<InState<kPreLinking>> module,
      ZoneForwardList<Handle<InState<kLinking>>>* stack, unsigned* dfs_index,
      Zone* zone, unsigned depth, unsigned* max_depth);
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSGeneratorObject>
  RunInitializationCode(Isolate* isolate,
                        DirectHandle<InState<kLinking>> module);

  static void FetchStarExports(
      Isolate* isolate,
      Handle<InState<kLinking, kLinked, kEvaluating, kEvaluatingAsync,
                     kEvaluated, kEvaluatedError>>
          module,
      Zone* zone, UnorderedModuleSet* visited);

  static void GatherAvailableAncestors(Isolate* isolate, Zone* zone,
                                       Handle<InState<kEvaluated>> start,
                                       AvailableAncestorsSet* exec_list);

  // Implementation of spec concrete method Evaluate.
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSPromise> Evaluate(
      Isolate* isolate, Handle<InState<kLinked>> module, bool debug_event,
      bool* body_threw);

  // Implementation of spec abstract operation InnerModuleEvaluation. Sets
  // |body_threw| to true if a module body threw.
  static V8_WARN_UNUSED_RESULT bool InnerModuleEvaluation(
      Isolate* isolate, Handle<SourceTextModule> module,
      ZoneForwardList<Handle<InState<kEvaluating>>>* stack, unsigned* dfs_index,
      bool* body_threw);

  // Implementation of spec ExecuteModule is broken up into
  // InnerExecuteAsyncModule for asynchronous modules and ExecuteModule
  // for synchronous modules.
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object>
  InnerExecuteAsyncModule(
      Isolate* isolate,
      DirectHandle<InState<kEvaluating, kEvaluatingAsync>> module,
      DirectHandle<JSPromise> capability);

  static V8_WARN_UNUSED_RESULT bool ExecuteModule(
      Isolate* isolate,
      DirectHandle<InState<kEvaluating, kEvaluatingAsync>> module,
      MaybeDirectHandle<Object>* exception_out);

  // Implementation of spec ExecuteAsyncModule. Return Nothing if the execution
  // is been terminated.
  static V8_WARN_UNUSED_RESULT Maybe<bool> ExecuteAsyncModule(
      Isolate* isolate,
      DirectHandle<InState<kEvaluating, kEvaluatingAsync>> module);

  static void Reset(Isolate* isolate,
                    DirectHandle<InState<kPreLinking, kLinking>> module);

  V8_EXPORT_PRIVATE void InnerGetStalledTopLevelAwaitModule(
      Isolate* isolate, UnorderedModuleSet* visited,
      DirectHandleVector<SourceTextModule>* result);

  friend class TorqueGeneratedSourceTextModuleAsserts;
  friend struct ObjectTraits<SourceTextModule>;
  friend class CodeStubAssembler;

  TaggedMember<UnionOf<SharedFunctionInfo, JSFunction, JSGeneratorObject>>
      code_;

 public:
  // Public for offsetof() in interpreter/baseline/compiler assemblers (some in
  // anonymous namespaces that cannot be friended).
  TaggedMember<FixedArray> regular_exports_;
  TaggedMember<FixedArray> regular_imports_;

 private:
  TaggedMember<FixedArray> requested_modules_;
  V8_TQ_ACQ_REL TaggedMember<UnionOf<TheHole, JSObject>> import_meta_;
  TaggedMember<UnionOf<SourceTextModule, TheHole>> cycle_root_;
  TaggedMember<ArrayList> async_parent_modules_;
  TaggedMember<Smi> dfs_index_;
  TaggedMember<Smi> dfs_ancestor_index_;
  TaggedMember<Smi> pending_async_dependencies_;
  SmiBitFieldMember<Flags> flags_;
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
  V8_IT_NO_AUTO_CHECKER;

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

  // Whether the module has at least one `export * from '...'` statement.
  bool HasStarExports() const;

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

V8_OBJECT class ModuleRequest : public Struct {
 public:
  inline ModuleRequest(const AllocationWitness& witness,
                       Tagged<String> specifier, ModuleImportPhase phase,
                       Tagged<FixedArray> import_attributes, int position);

  inline Tagged<String> specifier() const;
  inline Tagged<FixedArray> import_attributes() const;

  struct Flags : BitFieldGroup<Flags, uint32_t> {
    using BitFieldGroup::BitFieldGroup;

    using PhaseBits = First<ModuleImportPhase, 2>;
    using PositionBits = PhaseBits::Next<uint32_t, 29>;

    BIT_FIELD_GROUP_ACCESSORS(phase, PhaseBits)
    BIT_FIELD_GROUP_ACCESSORS(position, PositionBits)
  };

  inline Flags flags() const;

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

  static_assert(Flags::PositionBits::kMax >= String::kMaxLength,
                "String::kMaxLength should fit in PositionBits::kMax");
  inline unsigned position() const;
  inline ModuleImportPhase phase() const;

  using BodyDescriptor = StructBodyDescriptor;

 public:
  const TaggedMember<String> specifier_;
  const TaggedMember<FixedArray> import_attributes_;
  const SmiBitFieldMember<Flags> flags_;
} V8_OBJECT_END;

V8_OBJECT class SourceTextModuleInfoEntry : public Struct {
 public:
  inline SourceTextModuleInfoEntry(
      const AllocationWitness& witness,
      Tagged<UnionOf<String, Undefined>> export_name,
      Tagged<UnionOf<String, Undefined>> local_name,
      Tagged<UnionOf<String, Undefined>> import_name, int module_request,
      int cell_index, int beg_pos, int end_pos);

  inline Tagged<UnionOf<String, Undefined>> export_name() const;
  inline Tagged<UnionOf<String, Undefined>> local_name() const;
  inline Tagged<UnionOf<String, Undefined>> import_name() const;
  inline int module_request() const;
  inline int cell_index() const;
  inline int beg_pos() const;
  inline int end_pos() const;

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
  const TaggedMember<UnionOf<String, Undefined>> export_name_;
  const TaggedMember<UnionOf<String, Undefined>> local_name_;
  const TaggedMember<UnionOf<String, Undefined>> import_name_;
  const TaggedMember<Smi> module_request_;
  const TaggedMember<Smi> cell_index_;
  const TaggedMember<Smi> beg_pos_;
  const TaggedMember<Smi> end_pos_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_SOURCE_TEXT_MODULE_H_
