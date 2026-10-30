// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_MODULE_H_
#define V8_OBJECTS_MODULE_H_

#include "include/v8-script.h"
#include "include/v8-template.h"
#include "src/objects/js-objects.h"
#include "src/objects/objects.h"
#include "src/objects/struct.h"

// Has to be the last include (doesn't have include guards):
#include "src/objects/object-macros.h"

namespace v8 {
namespace internal {

class JSAsyncFunctionObject;
class JSModuleNamespace;
class SourceTextModule;
class SourceTextModuleDescriptor;
class SourceTextModuleInfo;
class SourceTextModuleInfoEntry;
class SyntheticModule;
class Zone;
template <typename T>
class ZoneForwardList;

// Module is the base class for ECMAScript module types, roughly corresponding
// to Abstract Module Record.
// https://tc39.es/ecma262/#sec-abstract-module-records
V8_OBJECT class Module : public HeapObject {
  V8_IT_ABSTRACT;

 public:
  DECL_VERIFIER(Module)
  DECL_PRINTER(Module)

  // A module only ever moves forward through these states (see
  // IsValidTransition, TransitionStatus, and RecordError):
  //
  //   kUnlinked -> kPreLinking -> kLinking -> kLinked -> kEvaluating
  //
  //   kPreLinking -> kLinked            (synthetic modules skip kLinking)
  //   kPreLinking -> kUnlinked          (failed link resets the graph)
  //   kLinking    -> kUnlinked          (failed link resets the graph)
  //
  //   kEvaluating -> kEvaluatingAsync -> kEvaluated
  //   kEvaluating -> kEvaluated
  //   kEvaluating -> kEvaluatedError
  //   kEvaluatingAsync -> kEvaluatedError
  //
  // kEvaluating means the module is on the DFS stack of an in-progress
  // Evaluate() call. kEvaluatedError is the spec's EVALUATED with a non-empty
  // [[EvaluationError]]: {exception} holds the error, or null if a termination
  // exception aborted evaluation. When Evaluate() fails, every module on its
  // DFS stack becomes kEvaluatedError.
  //
  // {top_level_capability} is the promise returned by Evaluate(). Evaluate()
  // redirects to the cycle root of an already evaluated strongly connected
  // component, so the capability lives on cycle roots (and on synthetic
  // modules) and mirrors their status: pending while kEvaluating or
  // kEvaluatingAsync, fulfilled when kEvaluated, and rejected with {exception}
  // when kEvaluatedError (dropped to undefined if termination prevented
  // rejecting it, in which case a later Evaluate() materializes a fresh
  // capability rejected with null). These are steady-state invariants:
  // JSPromise resolution runs embedder hooks before the promise changes state,
  // so they are asserted where the capability is used, not in the heap
  // verifier.
  enum Status : int {
    kFirst,

    // Order matters!
    kUnlinked = kFirst,
    kPreLinking,
    kLinking,
    kLinked,
    kEvaluating,
    kEvaluatingAsync,
    kEvaluated,
    kEvaluatedError,

    kLast = kEvaluatedError
  };

  template <Status... kStatuses>
  struct StatusList {
    static constexpr size_t kSize = sizeof...(kStatuses);
  };
  // The single source of truth for "every status" used by the compile-time
  // checks and the dispatch in MatchState.
  using AllStatuses =
      StatusList<kUnlinked, kPreLinking, kLinking, kLinked, kEvaluating,
                 kEvaluatingAsync, kEvaluated, kEvaluatedError>;
  static_assert(AllStatuses::kSize == kEvaluatedError + 1);

  static constexpr bool IsValidTransition(Status from, Status to) {
    switch (from) {
      case kUnlinked:
        return to == kPreLinking;
      case kPreLinking:
        return to == kLinking || to == kLinked || to == kUnlinked;
      case kLinking:
        return to == kLinked || to == kUnlinked;
      case kLinked:
        return to == kEvaluating;
      case kEvaluating:
        return to == kEvaluatingAsync || to == kEvaluated ||
               to == kEvaluatedError;
      case kEvaluatingAsync:
        return to == kEvaluated || to == kEvaluatedError;
      case kEvaluated:
      case kEvaluatedError:
        return false;
    }
  }

  template <Status... kAllowed>
  static constexpr bool IsStateIn(Status s) {
    return ((s == kAllowed) || ...);
  }

  // Typestate handle proving at compile time that [handle] is in one
  // of [kStates...] and restricting state-dependent operations, transitions,
  // and fields to the states where they are valid. Obtained only by branching
  // via MatchState or as the result of a checked transition.
  template <typename ModuleT, Status... kStates>
  class InState;

  // Human-readable name of [status]; "<invalid>" for out-of-range values.
  static const char* StatusString(Module::Status status);

  // The exception in the case {status} is kEvaluatedError.
  Tagged<Object> GetException();
  inline Tagged<Object> exception() const;

  // Returns if this module or any transitively requested module is [[Async]],
  // i.e. has a top-level await.
  V8_WARN_UNUSED_RESULT bool IsGraphAsync(Isolate* isolate) const;

  struct UserResolveCallbacks {
    v8::Module::ResolveModuleCallback module_callback = nullptr;
    v8::Module::ResolveSourceCallback source_callback = nullptr;
    v8::Module::ResolveModuleByIndexCallback module_callback_by_index = nullptr;
    v8::Module::ResolveSourceByIndexCallback source_callback_by_index = nullptr;
  };

  // Implementation of spec operation ModuleDeclarationInstantiation.
  // Returns false if an exception occurred during instantiation, true
  // otherwise. (In the case where the callback throws an exception, that
  // exception is propagated.)
  static V8_WARN_UNUSED_RESULT bool Instantiate(
      Isolate* isolate, Handle<Module> module, v8::Local<v8::Context> context,
      const UserResolveCallbacks& callbacks);

  // Implementation of spec operation ModuleEvaluation. Returns the module's
  // top-level capability promise, or an empty handle with the exception
  // pending if evaluation was aborted by a termination exception or a
  // synthetic module's evaluation steps threw. If |debug_event| is false,
  // rejecting the capability is not reported to the debugger. If |body_threw|
  // is given, it is set to true when a module body threw during this
  // evaluation.
  static V8_WARN_UNUSED_RESULT MaybeDirectHandle<JSPromise> Evaluate(
      Isolate* isolate, Handle<Module> module, bool debug_event = true,
      bool* body_threw = nullptr);

  // Implementation of spec abstract operation EvaluateModuleSync. Throws and
  // returns false if the module is not ready for synchronous execution or if
  // evaluation rejects.
  static V8_WARN_UNUSED_RESULT bool EvaluateModuleSync(Isolate* isolate,
                                                       Handle<Module> module);

  // Get the namespace object for [module].  If it doesn't exist yet, it is
  // created.
  static Handle<Cell> GetModuleNamespaceCell(
      Isolate* isolate, Handle<Module> module,
      ModuleImportPhase phase = ModuleImportPhase::kEvaluation);
  static DirectHandle<JSModuleNamespace> GetModuleNamespace(
      Isolate* isolate, Handle<Module> module,
      ModuleImportPhase phase = ModuleImportPhase::kEvaluation);

  inline Tagged<ObjectHashTable> exports() const;

  inline int hash() const;
  inline void set_hash(int value);

  inline Status status() const;

  inline Tagged<UnionOf<Cell, Undefined>> module_namespace() const;
  inline void set_module_namespace(
      Tagged<UnionOf<Cell, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<Cell, Undefined>> deferred_module_namespace() const;
  inline void set_deferred_module_namespace(
      Tagged<UnionOf<Cell, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<UnionOf<JSPromise, Undefined>> top_level_capability() const;

  struct Hash;

 protected:
  friend class Factory;
  template <typename ModuleT, Status... kStates>
  friend class InState;

  inline void set_top_level_capability(
      Tagged<UnionOf<JSPromise, Undefined>> value,
      WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  // Checked state transitions; the allowed edges are statically verified
  // against IsValidTransition.
  template <Status kTo, template <typename> class Holder, typename ModuleT,
            Status... kStates>
    requires((IsValidTransition(kStates, kTo) && ...) &&
             (std::is_same_v<Holder<ModuleT>, Handle<ModuleT>> ||
              std::is_same_v<Holder<ModuleT>, DirectHandle<ModuleT>>))
  static inline Holder<InState<ModuleT, kTo>> TransitionStatus(
      Holder<InState<ModuleT, kStates...>> module) {
    CHECK(((module->status() == kStates) || ...));
    module->SetStatusInternal(kTo);
    return UncheckedCast<InState<ModuleT, kTo>>(module);
  }

  // Transitions from kEvaluating or kEvaluatingAsync to kEvaluatedError.
  // Records [error] as {exception} (null for a termination exception) and, for
  // source text modules, drops the generator and ends async evaluation.
  template <template <typename> class Holder, typename ModuleT,
            Status... kStates>
    requires((IsValidTransition(kStates, kEvaluatedError) && ...) &&
             (std::is_same_v<Holder<ModuleT>, Handle<ModuleT>> ||
              std::is_same_v<Holder<ModuleT>, DirectHandle<ModuleT>>))
  static inline Holder<InState<ModuleT, kEvaluatedError>> RecordError(
      Isolate* isolate, Holder<InState<ModuleT, kStates...>> module,
      DirectHandle<Object> error) {
    CHECK(((module->status() == kStates) || ...));
    module->RecordErrorInternal(isolate, *error);
    return UncheckedCast<InState<ModuleT, kEvaluatedError>>(module);
  }

  // Dispatches [module] to the handler in [handlers...] whose
  // Handle/DirectHandle<InState<ModuleT, kStates...>> parameter matches
  // module->status(). Any status not handled by [handlers...] (including
  // out-of-range values from heap corruption) crashes reliably, naming the
  // actual and expected statuses in all build modes.
  template <template <typename> class Holder, typename ModuleT,
            typename... Handlers>
    requires(std::is_base_of_v<Module, ModuleT> &&
             (std::is_same_v<Holder<ModuleT>, Handle<ModuleT>> ||
              std::is_same_v<Holder<ModuleT>, DirectHandle<ModuleT>>))
  static inline decltype(auto) MatchState(Holder<ModuleT> module,
                                          Handlers&&... handlers);

  // The [must_resolve] argument indicates whether or not an exception should be
  // thrown in case the module does not provide an export named [name]
  // (including when a cycle is detected).  An exception is always thrown in the
  // case of conflicting star exports.
  //
  // If [must_resolve] is true, a null result indicates an exception. If
  // [must_resolve] is false, a null result may or may not indicate an
  // exception (so check manually!).
  class ResolveSet;
  static V8_WARN_UNUSED_RESULT MaybeHandle<Cell> ResolveExport(
      Isolate* isolate, Handle<Module> module,
      DirectHandle<String> module_specifier, Handle<String> export_name,
      MessageLocation loc, bool must_resolve, ResolveSet* resolve_set);

  static V8_WARN_UNUSED_RESULT bool PrepareInstantiate(
      Isolate* isolate, Handle<Module> module, v8::Local<v8::Context> context,
      const UserResolveCallbacks& callbacks);
  static V8_WARN_UNUSED_RESULT bool FinishInstantiate(
      Isolate* isolate, Handle<Module> module,
      ZoneForwardList<Handle<InState<SourceTextModule, kLinking>>>* stack,
      unsigned* dfs_index, Zone* zone, unsigned depth, unsigned* max_depth);

  // Set module's status back to kUnlinked and reset other internal state.
  // This is used when instantiation fails.
  static void Reset(
      Isolate* isolate,
      DirectHandle<InState<Module, kPreLinking, kLinking>> module);
  static void ResetGraph(Isolate* isolate, Handle<Module> module);

 private:
  inline void set_exports(Tagged<ObjectHashTable> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER);
  // Once a module namespace (normal or deferred) has been created in
  // GetModuleNamespace, all star exports have been fetched and {exports} is
  // finalized (must not be mutated or rehashed as ICs index into it).
  inline bool ExportsFinalized() const;
  inline void set_status(int value);
  inline void set_exception(Tagged<Object> value,
                            WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  template <typename... Ts>
  struct Overloaded : Ts... {
    using Ts::operator()...;
  };

  // Compile-time helpers for MatchState, all enumerating AllStatuses so that
  // adding a status cannot silently leave a check incomplete.
  template <template <typename> class Holder, typename ModuleT, Status kState,
            typename... Handlers>
  static consteval size_t MatchingHandlerCount() {
    return (
        static_cast<size_t>(
            std::is_invocable_v<Handlers&, Holder<InState<ModuleT, kState>>>) +
        ...);
  }

  template <template <typename> class Holder, typename ModuleT,
            typename Handler, Status... kAll>
  static consteval bool HandlesAnyState(StatusList<kAll...>) {
    return !std::is_invocable_v<Handler&, Holder<ModuleT>> &&
           (std::is_invocable_v<Handler&, Holder<InState<ModuleT, kAll>>> ||
            ...);
  }

  template <template <typename> class Holder, typename ModuleT,
            typename... Handlers, Status... kAll>
  static consteval bool NoDuplicateHandlers(StatusList<kAll...>) {
    return ((MatchingHandlerCount<Holder, ModuleT, kAll, Handlers...>() <= 1) &&
            ...);
  }

  // Bit mask over Status of the states some handler accepts.
  template <template <typename> class Holder, typename ModuleT,
            typename... Handlers, Status... kAll>
  static consteval uint32_t HandledStatusMask(StatusList<kAll...>) {
    return ((MatchingHandlerCount<Holder, ModuleT, kAll, Handlers...>() > 0
                 ? uint32_t{1} << kAll
                 : uint32_t{0}) |
            ...);
  }

  // Crashes with a message naming [status] and the statuses in [handled_mask],
  // in release builds too, so that a module reaching a MatchState in an
  // unexpected state produces a diagnosable crash signature.
  [[noreturn]] static void FatalUnexpectedStatus(Status status,
                                                 uint32_t handled_mask);

  void SetStatusInternal(Status new_status);
  void RecordErrorInternal(Isolate* isolate, Tagged<Object> error);

  friend class TorqueGeneratedModuleAsserts;
  friend struct ObjectTraits<Module>;
  friend class AccessorAssembler;

  TaggedMember<ObjectHashTable> exports_;
  TaggedMember<Smi> hash_;
  TaggedMember<Smi> status_;
  TaggedMember<UnionOf<Cell, Undefined>> module_namespace_;
  TaggedMember<UnionOf<Cell, Undefined>> deferred_module_namespace_;
  TaggedMember<Object> exception_;
  TaggedMember<UnionOf<JSPromise, Undefined>> top_level_capability_;
} V8_OBJECT_END;

V8_OBJECT
template <typename ModuleT, Module::Status... kStates>
class Module::InState : public ModuleT {
 public:
  static_assert(sizeof...(kStates) > 0);
  static_assert(std::is_base_of_v<Module, ModuleT>);

  template <Status... kAllowed>
  static consteval bool IsModuleAllowed() {
    return (IsStateIn<kAllowed...>(kStates) && ...);
  }

  template <Status... kAllowed>
  static consteval bool IsAllowed() {
    return std::is_same_v<ModuleT, SourceTextModule> &&
           IsModuleAllowed<kAllowed...>();
  }

  inline void set_exports(Tagged<ObjectHashTable> value,
                          WriteBarrierMode mode = UPDATE_WRITE_BARRIER)
    requires(IsModuleAllowed<kPreLinking, kLinking>());
  inline void set_star_exports(Tagged<ObjectHashTable> value,
                               WriteBarrierMode mode = UPDATE_WRITE_BARRIER)
    requires(IsAllowed<kLinking, kLinked, kEvaluating, kEvaluatingAsync,
                       kEvaluated, kEvaluatedError>());
  inline bool ExportsFinalized() const
    requires(IsModuleAllowed<kLinking, kLinked, kEvaluating, kEvaluatingAsync,
                             kEvaluated, kEvaluatedError>());

  inline Tagged<SharedFunctionInfo> sfi() const
    requires(IsAllowed<kUnlinked, kPreLinking, kEvaluatedError>());
  inline Tagged<JSFunction> function() const
    requires(IsAllowed<kLinking>());
  inline Tagged<JSGeneratorObject> generator() const
    requires(IsAllowed<kLinked, kEvaluating, kEvaluatingAsync, kEvaluated>());
  inline Tagged<JSAsyncFunctionObject> async_function_object() const
    requires(IsAllowed<kEvaluating, kEvaluatingAsync>());

  inline Tagged<SourceTextModule> GetCycleRoot() const
    requires(IsAllowed<kEvaluatingAsync, kEvaluated>());
  inline Handle<SourceTextModule> GetCycleRoot(Isolate* isolate) const
    requires(IsAllowed<kEvaluatingAsync, kEvaluated>());
  inline MaybeHandle<SourceTextModule> TryGetCycleRoot(Isolate* isolate) const
    requires(IsAllowed<kEvaluatedError>());

  inline int dfs_index() const
    requires(IsAllowed<kLinking, kEvaluating>());
  inline int dfs_ancestor_index() const
    requires(IsAllowed<kLinking, kEvaluating>());
  inline void UpdateDfsAncestorIndex(
      DirectHandle<InState<SourceTextModule, kLinking>> child)
    requires(IsAllowed<kLinking>());
  inline void UpdateDfsAncestorIndex(
      DirectHandle<InState<SourceTextModule, kEvaluating>> child)
    requires(IsAllowed<kEvaluating>());

  inline bool HasPendingAsyncDependencies()
    requires(IsAllowed<kEvaluating, kEvaluatingAsync>());
  inline void IncrementPendingAsyncDependencies()
    requires(IsAllowed<kEvaluating>());
  inline void DecrementPendingAsyncDependencies()
    requires(IsAllowed<kEvaluatingAsync>());

 private:
  InState() = delete;
} V8_OBJECT_END;

template <typename ModuleT, Module::Status... kStates>
struct CastTraits<Module::InState<ModuleT, kStates...>> {
  template <typename From>
    requires(std::is_base_of_v<Module, From>)
  static inline bool AllowFrom(Tagged<From> value) {
    if (!Module::IsStateIn<kStates...>(value->status())) return false;
    if constexpr (std::is_base_of_v<ModuleT, From>) {
      return true;
    } else {
      return Is<ModuleT>(value);
    }
  }
};

namespace detail {
template <typename ModuleT, Module::Status kS1, Module::Status kS2,
          Module::Status... kRest>
struct normalize_type<Module::InState<ModuleT, kS1, kS2, kRest...>> {
  using type =
      Union<Module::InState<ModuleT, kS1>, Module::InState<ModuleT, kS2>,
            Module::InState<ModuleT, kRest>...>;
};
}  // namespace detail

template <>
struct ObjectTraits<Module> {
  using BodyDescriptor = FixedBodyDescriptor<offsetof(Module, exports_),
                                             sizeof(Module), sizeof(Module)>;
};

// When importing a module namespace (import * as foo from "bar"), a
// JSModuleNamespace object (representing module "bar") is created and bound to
// the declared variable (foo).  A module can have at most one namespace object.
V8_OBJECT class JSModuleNamespace : public JSSpecialObject {
  V8_IT_OWN_TYPE;

 public:
  inline Tagged<Module> module() const;
  inline void set_module(Tagged<Module> value,
                         WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(JSModuleNamespace)
  DECL_VERIFIER(JSModuleNamespace)

  // Retrieve the value exported by [module] under the given [name]. If there is
  // no such export, return Just(undefined). If the export is uninitialized,
  // schedule an exception and return Nothing.
  V8_WARN_UNUSED_RESULT MaybeDirectHandle<Object> GetExport(
      Isolate* isolate, DirectHandle<String> name);

  bool HasExport(Isolate* isolate, DirectHandle<String> name);

  // Counts an access to a missing `default` export on the namespace [it] starts
  // at, when that module has an `export * from '...'` statement. Such a
  // `default` is absent today because `export *` skips it, but would resolve to
  // the star-exported default (if it exists) under the export-star-default
  // proposal (https://github.com/tc39/proposal-export-star-default).
  // Called from [[Get]] and [[HasProperty]]. Only valid while [it] is in the
  // LookupIterator::MODULE_NAMESPACE state.
  static void MaybeCountMissingDefaultWithStarExport(LookupIterator* it);

  // Return the (constant) property attributes for the referenced property,
  // which is assumed to correspond to an export. If the export is
  // uninitialized, schedule an exception and return Nothing.
  static V8_WARN_UNUSED_RESULT Maybe<PropertyAttributes> GetPropertyAttributes(
      LookupIterator* it);

  static V8_WARN_UNUSED_RESULT Maybe<bool> DefineOwnProperty(
      Isolate* isolate, DirectHandle<JSModuleNamespace> o,
      DirectHandle<Object> key, PropertyDescriptor* desc,
      Maybe<ShouldThrow> should_throw);

  // In-object fields. These indices represent both the in-object field index,
  // as well as the descriptor index.
  enum {
    kToStringTagIndex,
    kInObjectFieldCount,
  };

  static const int kHeaderSize;
  // We need to include in-object fields
  // TODO(v8:8944): improve handling of in-object fields
  static const int kSize;

 public:
  TaggedMember<Module> module_;
} V8_OBJECT_END;

inline constexpr int JSModuleNamespace::kHeaderSize = sizeof(JSModuleNamespace);
inline constexpr int JSModuleNamespace::kSize =
    JSModuleNamespace::kHeaderSize + (kTaggedSize * kInObjectFieldCount);

V8_OBJECT class JSDeferredModuleNamespace : public JSModuleNamespace {
 public:
  DECL_PRINTER(JSDeferredModuleNamespace)
  DECL_VERIFIER(JSDeferredModuleNamespace)

  static void EvaluateModuleSync(
      Isolate* isolate, DirectHandle<JSDeferredModuleNamespace> holder);
  static bool TriggersEvaluation(LookupIterator* it);

  // We need to include in-object fields
  // TODO(v8:8944): improve handling of in-object fields
  static const int kSize;
} V8_OBJECT_END;

inline constexpr int JSDeferredModuleNamespace::kSize =
    sizeof(JSDeferredModuleNamespace) + (kTaggedSize * kInObjectFieldCount);

V8_OBJECT class ScriptOrModule : public Struct {
 public:
  inline Tagged<Object> resource_name() const;
  inline void set_resource_name(Tagged<Object> value,
                                WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  inline Tagged<FixedArray> host_defined_options() const;
  inline void set_host_defined_options(
      Tagged<FixedArray> value, WriteBarrierMode mode = UPDATE_WRITE_BARRIER);

  DECL_PRINTER(ScriptOrModule)
  DECL_VERIFIER(ScriptOrModule)

  using BodyDescriptor = StructBodyDescriptor;

 private:
  friend class TorqueGeneratedScriptOrModuleAsserts;

  TaggedMember<Object> resource_name_;
  TaggedMember<FixedArray> host_defined_options_;
} V8_OBJECT_END;

}  // namespace internal
}  // namespace v8

#include "src/objects/object-macros-undef.h"

#endif  // V8_OBJECTS_MODULE_H_
