// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/module.h"

#include <array>
#include <istream>
#include <optional>
#include <sstream>
#include <strstream>
#include <unordered_map>
#include <unordered_set>

#include "include/v8-template.h"
#include "src/api/api-inl.h"
#include "src/ast/modules.h"
#include "src/base/strings.h"
#include "src/base/vector.h"
#include "src/builtins/accessors.h"
#include "src/common/assert-scope.h"
#include "src/debug/debug.h"
#include "src/heap/heap-inl.h"
#include "src/logging/counters.h"
#include "src/objects/cell-inl.h"
#include "src/objects/hash-table-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/module-inl.h"
#include "src/objects/object-predicates-inl.h"
#include "src/objects/property-descriptor.h"
#include "src/objects/source-text-module-inl.h"
#include "src/objects/synthetic-module-inl.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

namespace {
#ifdef DEBUG
void PrintModuleName(Tagged<Module> module, std::ostream& os) {
  if (IsSourceTextModule(module)) {
    Print(Cast<SourceTextModule>(module)->GetScript()->GetNameOrSourceURL(),
          os);
  } else {
    Print(Cast<SyntheticModule>(module)->name(), os);
  }
#ifndef OBJECT_PRINT
  os << "\n";
#endif  // OBJECT_PRINT
}

void PrintStatusTransition(Tagged<Module> module, Module::Status old_status) {
  if (!v8_flags.trace_module_status) return;
  StdoutStream os;
  os << "Changing module status from " << Module::StatusString(old_status)
     << " to "
     << Module::StatusString(static_cast<Module::Status>(module->status()))
     << " for ";
  PrintModuleName(module, os);
}

void PrintStatusMessage(Tagged<Module> module, const char* message) {
  if (!v8_flags.trace_module_status) return;
  StdoutStream os;
  os << "Instantiating module ";
  PrintModuleName(module, os);
}
#endif  // DEBUG

}  // end namespace

// static
const char* Module::StatusString(Module::Status status) {
  switch (status) {
    case Module::kUnlinked:
      return "Unlinked";
    case Module::kPreLinking:
      return "PreLinking";
    case Module::kLinking:
      return "Linking";
    case Module::kLinked:
      return "Linked";
    case Module::kEvaluating:
      return "Evaluating";
    case Module::kEvaluatingAsync:
      return "EvaluatingAsync";
    case Module::kEvaluated:
      return "Evaluated";
    case Module::kEvaluatedError:
      return "EvaluatedError";
  }
  // Reached only with a corrupted status; this function is used to report
  // exactly that, so it must not crash itself.
  return "<invalid>";
}

// static
void Module::FatalUnexpectedStatus(Status status, uint32_t status_mask) {
  std::stringstream expected;
  for (int s = kFirst; s <= kLast; ++s) {
    if ((status_mask & (uint32_t{1} << s)) == 0) continue;
    if (!expected.str().empty()) {
      expected << ", ";
    }
    expected << StatusString(static_cast<Status>(s));
  }
  // Multi-argument FATAL keeps its message in all build modes, so the crash
  // signature names the offending state.
  FATAL("Module in unexpected status %s (%d), expected one of {%s}",
        StatusString(status), static_cast<int>(status), expected.str().data());
}

void Module::SetStatusInternal(Status new_status) {
  DisallowGarbageCollection no_gc;
#ifdef DEBUG
  Status old_status = status();
  DCHECK(IsValidTransition(old_status, new_status));
  set_status(new_status);
  PrintStatusTransition(this, old_status);
#else
  set_status(new_status);
#endif  // DEBUG
}

void Module::RecordErrorInternal(Isolate* isolate, Tagged<Object> error) {
  DisallowGarbageCollection no_gc;
  DCHECK(IsTheHole(exception()));
  DCHECK(!IsTheHole(error));
  if (Tagged<SourceTextModule> self; TryCast(Tagged(this), &self)) {
    // Revert to minimal SFI in case we have already been instantiating or
    // evaluating.
    self->set_code(self->GetSharedFunctionInfo());
  }
  SetStatusInternal(Module::kEvaluatedError);
  if (isolate->is_catchable_by_javascript(error)) {
    set_exception(error);
  } else {
    // v8::TryCatch uses `null` for termination exceptions.
    set_exception(ReadOnlyRoots(isolate).null_value());
  }
}

void Module::ResetGraph(Isolate* isolate, Handle<Module> module) {
  MatchState(
      module,
      [](DirectHandle<InState<Module, kUnlinked, kLinked, kEvaluatingAsync,
                              kEvaluated, kEvaluatedError>>
             m) {
#ifdef DEBUG
        if (Tagged<SourceTextModule> stm; TryCast(*m, &stm)) {
          stm->VerifyRequestedModules();
        }
#endif  // DEBUG
      },
      [&](DirectHandle<InState<Module, kPreLinking, kLinking>> m) {
        DirectHandle<SourceTextModule> stm;
        if (!TryCast(m, &stm)) {
          Reset(isolate, m);
          return;
        }
        // Reset() clears requested_modules, so read it beforehand.
        DirectHandle<FixedArray> requested_modules(stm->requested_modules(),
                                                   isolate);
        Reset(isolate, m);

        const uint32_t requested_modules_len =
            requested_modules->ulength().value();
        for (uint32_t i = 0; i < requested_modules_len; ++i) {
          Handle<Object> descendant(requested_modules->get(i), isolate);
          if (Handle<Module> requested_module;
              TryCast(descendant, &requested_module)) {
            ResetGraph(isolate, requested_module);
          } else {
            // Source phase imports store a JSReceiver (e.g. WasmModuleObject)
            // in requested_modules. Pre-linking entries are Undefined.
            CHECK(IsUndefined(*descendant) || IsJSReceiver(*descendant));
          }
        }
      });
}

void Module::Reset(
    Isolate* isolate,
    DirectHandle<InState<Module, kPreLinking, kLinking>> module) {
  DCHECK(IsTheHole(module->exception()));
  // The namespace cells could have been created during ResolveExport for
  // 'export * as ns', reset them here.
  module->set_module_namespace(ReadOnlyRoots(isolate).undefined_value());
  module->set_deferred_module_namespace(
      ReadOnlyRoots(isolate).undefined_value());
  DirectHandle<InState<SourceTextModule, kPreLinking, kLinking>> stm;
  if (!TryCast(module, &stm)) {
    DirectHandle<ObjectHashTable> exports = ObjectHashTable::New(
        isolate,
        Cast<SyntheticModule>(*module)->export_names()->ulength().value());
    module->set_exports(*exports);
    TransitionStatus<kUnlinked>(module);
    return;
  }
  DirectHandle<ObjectHashTable> exports =
      ObjectHashTable::New(isolate, stm->regular_exports()->ulength().value());
  SourceTextModule::Reset(isolate, stm);
  module->set_exports(*exports);
  TransitionStatus<kUnlinked>(module);
}

Tagged<Object> Module::GetException() {
  DisallowGarbageCollection no_gc;
  DCHECK_EQ(status(), Module::kEvaluatedError);
  DCHECK(!IsTheHole(exception()));
  return exception();
}

MaybeHandle<Cell> Module::ResolveExport(Isolate* isolate, Handle<Module> module,
                                        DirectHandle<String> module_specifier,
                                        Handle<String> export_name,
                                        MessageLocation loc, bool must_resolve,
                                        Module::ResolveSet* resolve_set) {
  if (Handle<SourceTextModule> stm; TryCast(module, &stm)) {
    return SourceTextModule::ResolveExport(
        isolate,
        CheckedCast<InState<SourceTextModule, kPreLinking, kLinking, kLinked,
                            kEvaluatingAsync, kEvaluated, kEvaluatedError>>(
            stm),
        module_specifier, export_name, loc, must_resolve, resolve_set);
  } else {
    return SyntheticModule::ResolveExport(
        isolate,
        CheckedCast<InState<SyntheticModule, kPreLinking, kLinking, kLinked,
                            kEvaluatingAsync, kEvaluated, kEvaluatedError>>(
            module),
        module_specifier, export_name, loc, must_resolve);
  }
}

bool Module::Instantiate(Isolate* isolate, Handle<Module> module,
                         v8::Local<v8::Context> context,
                         const Module::UserResolveCallbacks& callbacks) {
#ifdef DEBUG
  PrintStatusMessage(*module, "Instantiating module ");
#endif  // DEBUG

  if (!PrepareInstantiate(isolate, module, context, callbacks)) {
    ResetGraph(isolate, module);
    DCHECK_EQ(module->status(), kUnlinked);
    return false;
  }
  Zone zone(isolate->allocator(), ZONE_NAME);
  ZoneForwardList<Handle<SourceTextModule::InState<kLinking>>> stack(&zone);
  unsigned dfs_index = 0;
  unsigned max_depth = 0;
  if (!FinishInstantiate(isolate, module, &stack, &dfs_index, &zone, 1,
                         &max_depth)) {
    ResetGraph(isolate, module);
    DCHECK_EQ(module->status(), kUnlinked);
    return false;
  }
  DCHECK_GE(module->status(), kLinked);
  DCHECK(stack.empty());
  isolate->counters()->esm_modules_per_page()->AddSample(dfs_index);
  isolate->counters()->esm_import_graph_depth()->AddSample(max_depth);
  return true;
}

bool Module::PrepareInstantiate(Isolate* isolate, Handle<Module> module,
                                v8::Local<v8::Context> context,
                                const UserResolveCallbacks& callbacks) {
  return MatchState(
      module,
      [](DirectHandle<InState<Module, kPreLinking, kLinked, kEvaluatingAsync,
                              kEvaluated, kEvaluatedError>>) { return true; },
      [&](Handle<InState<Module, kUnlinked>> unlinked) {
        Handle<InState<Module, kPreLinking>> pre_linking =
            TransitionStatus<kPreLinking>(unlinked);
        STACK_CHECK(isolate, false);
        if (Handle<InState<SourceTextModule, kPreLinking>> stm;
            TryCast(pre_linking, &stm)) {
          return SourceTextModule::PrepareInstantiate(isolate, stm, context,
                                                      callbacks);
        } else {
          return SyntheticModule::PrepareInstantiate(
              isolate, Cast<InState<SyntheticModule, kPreLinking>>(pre_linking),
              context);
        }
      });
}

bool Module::FinishInstantiate(
    Isolate* isolate, Handle<Module> module,
    ZoneForwardList<Handle<InState<SourceTextModule, kLinking>>>* stack,
    unsigned* dfs_index, Zone* zone, unsigned depth, unsigned* max_depth) {
  if (max_depth) *max_depth = std::max(*max_depth, depth);
  return MatchState(
      module,
      [](DirectHandle<InState<Module, kLinking, kLinked, kEvaluatingAsync,
                              kEvaluated, kEvaluatedError>>) { return true; },
      [&](Handle<InState<Module, kPreLinking>> pre_linking) {
        STACK_CHECK(isolate, false);
        if (Handle<InState<SourceTextModule, kPreLinking>> stm;
            TryCast(pre_linking, &stm)) {
          return SourceTextModule::FinishInstantiate(
              isolate, stm, stack, dfs_index, zone, depth, max_depth);
        } else {
          return SyntheticModule::FinishInstantiate(
              isolate,
              Cast<InState<SyntheticModule, kPreLinking>>(pre_linking));
        }
      });
}

// https://tc39.es/ecma262/#sec-moduleevaluation
MaybeDirectHandle<JSPromise> Module::Evaluate(Isolate* isolate,
                                              Handle<Module> module,
                                              bool debug_event,
                                              bool* body_threw) {
#ifdef DEBUG
  PrintStatusMessage(*module, "Evaluating module ");
#endif  // DEBUG

  // 2. Assert: module.[[Status]] is one of LINKED, EVALUATING-ASYNC, or
  //    EVALUATED.
  DCHECK(module->status() == kLinked || module->status() == kEvaluatingAsync ||
         module->status() == kEvaluated || module->status() == kEvaluatedError);

  // 3. If module.[[Status]] is either EVALUATING-ASYNC or EVALUATED, then
  // A synthetic module has no dependencies, so it is its own cycle root.
  if (Handle<SourceTextModule> stm; TryCast(module, &stm)) {
    module = MatchState(
        stm,
        [](Handle<InState<SourceTextModule, kLinked>> m) -> Handle<Module> {
          DCHECK(IsTheHole(m->cycle_root()));
          return m;
        },
        [&](DirectHandle<
            InState<SourceTextModule, kEvaluatingAsync, kEvaluated>>
                m) -> Handle<Module> {
          // a. If module.[[CycleRoot]] is not EMPTY, then
          //   i. Set module to module.[[CycleRoot]].
          return m->GetCycleRoot(isolate);
        },
        [&](Handle<InState<SourceTextModule, kEvaluatedError>> m)
            -> Handle<Module> {
          // a. If module.[[CycleRoot]] is not EMPTY, then
          //   i. Set module to module.[[CycleRoot]].
          if (Handle<SourceTextModule> cycle_root;
              m->TryGetCycleRoot(isolate).ToHandle(&cycle_root)) {
            return cycle_root;
          }
          // b. Else,
          //   i. Assert: module.[[Status]] is EVALUATED and
          //      module.[[EvaluationError]] is a throw completion.
          DCHECK_EQ(m->status(), kEvaluatedError);
          DCHECK(!IsTheHole(m->exception()));
          return m;
        });
  }

  // 4. If module.[[TopLevelCapability]] is not EMPTY, then
  //    a. Return module.[[TopLevelCapability]].[[Promise]].
  if (IsJSPromise(module->top_level_capability())) {
    DirectHandle<JSPromise> capability(
        Cast<JSPromise>(module->top_level_capability()), isolate);
    DCHECK_IMPLIES(module->status() == kEvaluatedError,
                   capability->status() == Promise::kRejected &&
                       capability->result() == module->exception());
    return capability;
  }
  DCHECK(IsUndefined(module->top_level_capability()));

  // If module.[[TopLevelCapability]] is EMPTY for an already-evaluating or
  // already-evaluated module, steps 6-11 of #sec-moduleevaluation allocate and
  // store a new capability on `module` and call InnerModuleEvaluation(module,
  // stack, 0), which immediately returns in step 2 (#sec-innermoduleevaluation)
  // without modifying `stack`:
  //   - EVALUATING-ASYNC: step 2.a returns NormalCompletion(0); step 10.c is
  //     skipped; step 11 returns the pending capability.
  //   - EVALUATED (no error): step 2.a returns NormalCompletion(0); step
  //     10.c.iii resolves the capability with undefined; step 11 returns it.
  //   - EVALUATED (errored): step 2.b returns module.[[EvaluationError]] with
  //     `stack` empty; step 9.d rejects the capability with the error; step 11
  //     returns it.
  return MatchState(
      module,
      [&](DirectHandle<InState<Module, kEvaluatingAsync>> m)
          -> MaybeDirectHandle<JSPromise> {
        // 6. Let capability be ! NewPromiseCapability(%Promise%).
        DirectHandle<JSPromise> capability = isolate->factory()->NewJSPromise();
        // 7. Set module.[[TopLevelCapability]] to capability.
        m->set_top_level_capability(*capability);
        // 8. InnerModuleEvaluation step 2.a returns 0.
        // 11. Return capability.[[Promise]].
        return capability;
      },
      [&](DirectHandle<InState<Module, kEvaluated>> m)
          -> MaybeDirectHandle<JSPromise> {
        // 6. Let capability be ! NewPromiseCapability(%Promise%).
        DirectHandle<JSPromise> capability = isolate->factory()->NewJSPromise();
        // 7. Set module.[[TopLevelCapability]] to capability.
        m->set_top_level_capability(*capability);
        // 8. InnerModuleEvaluation step 2.a returns 0.
        // 10.c.iii. Perform ! Call(capability.[[Resolve]], undefined,
        //                          « undefined »).
        JSPromise::Resolve(capability, isolate->factory()->undefined_value())
            .ToHandleChecked();
        // 11. Return capability.[[Promise]].
        return capability;
      },
      [&](DirectHandle<InState<Module, kEvaluatedError>> m)
          -> MaybeDirectHandle<JSPromise> {
        // 6. Let capability be ! NewPromiseCapability(%Promise%).
        DirectHandle<JSPromise> capability = isolate->factory()->NewJSPromise();
        // 7. Set module.[[TopLevelCapability]] to capability.
        m->set_top_level_capability(*capability);
        // 8. InnerModuleEvaluation step 2.b returns module.[[EvaluationError]].
        // 9.d. Perform ! Call(capability.[[Reject]], undefined,
        //                     « result.[[Value]] »).
        JSPromise::Reject(capability, direct_handle(m->GetException(), isolate),
                          debug_event);
        if (isolate->debug()->IsRestartFrameScheduled()) {
          m->set_top_level_capability(ReadOnlyRoots(isolate).undefined_value());
          isolate->TerminateExecution();
          return {};
        }
        // 11. Return capability.[[Promise]].
        return capability;
      },
      [&](Handle<InState<Module, kLinked>> linked)
          -> MaybeDirectHandle<JSPromise> {
        if (Handle<InState<SourceTextModule, kLinked>> stm;
            TryCast(linked, &stm)) {
          return SourceTextModule::Evaluate(isolate, stm, debug_event,
                                            body_threw);
        } else {
          return SyntheticModule::Evaluate(
              isolate, Cast<InState<SyntheticModule, kLinked>>(linked));
        }
      });
}

// https://tc39.es/proposal-defer-import-eval/#sec-EvaluateModuleSync
bool Module::EvaluateModuleSync(Isolate* isolate, Handle<Module> module) {
  // 2. If ReadyForSyncExecution(module) is false, throw a TypeError exception.
  {
    Zone zone(isolate->allocator(), ZONE_NAME);
    UnorderedModuleSet seen_modules(&zone);
    if (!SourceTextModule::ReadyForSyncExecution(isolate, module,
                                                 &seen_modules)) {
      isolate->Throw(*isolate->factory()->NewTypeError(
          MessageTemplate::kNotReadyForSyncExec));
      return false;
    }
  }

  // A side-effect-free debug-evaluate must not start the evaluation.
  if (isolate->debug_execution_mode() == DebugInfo::kSideEffects) {
    isolate->debug()->FailSideEffectCheckForDeferredModuleEvaluation();
    return false;
  }

  // 3. Let promise be module.Evaluate().
  // The capability is internal and its result is thrown again below, so
  // rejecting it is not a debug event.
  DirectHandle<JSPromise> promise;
  bool body_threw = false;
  if (!Module::Evaluate(isolate, module, /* debug_event */ false, &body_threw)
           .ToHandle(&promise)) {
    DCHECK(isolate->has_exception());
    return false;
  }

  // 4. Assert: promise.[[PromiseState]] is either FULFILLED or REJECTED.
  DCHECK(promise->status() == Promise::kFulfilled ||
         promise->status() == Promise::kRejected);
  // 5. If promise.[[PromiseState]] is REJECTED, then
  if (promise->status() == Promise::kRejected) {
    // a. If promise.[[PromiseIsHandled]] is false, perform
    //    HostPromiseRejectionTracker(promise, "handle").
    if (!promise->has_handler()) {
      isolate->ReportPromiseReject(promise, DirectHandle<Object>(),
                                   v8::kPromiseHandlerAddedAfterReject);
    }
    // b. Set promise.[[PromiseIsHandled]] to true.
    promise->set_has_handler(true);
    // c. Throw promise.[[PromiseResult]].
    // If a module body threw during this evaluation, the debugger already
    // reported the exception at its original throw site.
    std::optional<SuppressDebug> suppress_debug;
    if (body_threw) suppress_debug.emplace(isolate->debug());
    isolate->Throw(promise->result());
    return false;
  }
  DCHECK_EQ(promise->status(), Promise::kFulfilled);
  // 6. Return UNUSED.
  return true;
}

Handle<Cell> Module::GetModuleNamespaceCell(Isolate* isolate,
                                            Handle<Module> module,
                                            ModuleImportPhase phase) {
  ReadOnlyRoots roots(isolate);

  DCHECK(phase == ModuleImportPhase::kEvaluation ||
         phase == ModuleImportPhase::kDefer);

  Tagged<Object> maybe_cell = phase == ModuleImportPhase::kEvaluation
                                  ? module->module_namespace()
                                  : module->deferred_module_namespace();
  if (!IsUndefined(maybe_cell)) {
    return handle(Cast<Cell>(maybe_cell), isolate);
  }
  Handle<Cell> cell = isolate->factory()->NewCell();
  if (phase == ModuleImportPhase::kEvaluation) {
    module->set_module_namespace(*cell);
  } else {
    module->set_deferred_module_namespace(*cell);
  }
  return cell;
}

DirectHandle<JSModuleNamespace> Module::GetModuleNamespace(
    Isolate* isolate, Handle<Module> module, ModuleImportPhase phase) {
  ReadOnlyRoots roots(isolate);

  DCHECK(phase == ModuleImportPhase::kEvaluation ||
         phase == ModuleImportPhase::kDefer);
  DirectHandle<Cell> ns_cell = GetModuleNamespaceCell(isolate, module, phase);

  if (auto cur = ns_cell->value(); !IsUndefined(cur)) {
    return direct_handle(Cast<JSModuleNamespace>(cur), isolate);
  }

  // Collect the export names.
  Zone zone(isolate->allocator(), ZONE_NAME);
  UnorderedModuleSet visited(&zone);

  if (Handle<SourceTextModule> stm; TryCast(module, &stm)) {
    SourceTextModule::FetchStarExports(
        isolate,
        CheckedCast<InState<SourceTextModule, kLinking, kLinked, kEvaluating,
                            kEvaluatingAsync, kEvaluated, kEvaluatedError>>(
            stm),
        &zone, &visited);
  }

  DirectHandle<ObjectHashTable> exports(module->exports(), isolate);
  ZoneVector<IndirectHandle<String>> names(&zone);
  names.reserve(exports->NumberOfElements());
  [[maybe_unused]] bool skipped_then = false;
  for (InternalIndex i : exports->IterateEntries()) {
    Tagged<Object> key;
    if (!exports->ToKey(roots, i, &key)) continue;
    // https://tc39.es/proposal-defer-import-eval/#sec-getmodulenamespace
    // Deferred namespaces never expose a "then" export, so that awaiting
    // import.defer() resolves to the namespace itself.
    if (phase == ModuleImportPhase::kDefer && key == roots.then_string()) {
      skipped_then = true;
      continue;
    }
    names.push_back(handle(Cast<String>(key), isolate));
  }
  DCHECK_EQ(static_cast<int>(names.size()),
            exports->NumberOfElements() - (skipped_then ? 1 : 0));

  // Sort them alphabetically.
  std::sort(names.begin(), names.end(),
            [&isolate](IndirectHandle<String> a, IndirectHandle<String> b) {
              return String::Compare(isolate, a, b) ==
                     ComparisonResult::kLessThan;
            });

  // Create the namespace object (initially empty).
  DirectHandle<JSModuleNamespace> ns =
      phase == ModuleImportPhase::kEvaluation
          ? isolate->factory()->NewJSModuleNamespace()
          : isolate->factory()->NewJSDeferredModuleNamespace();
  ns->set_module(*module);
  ns_cell->set_value(*ns);

  // Create the properties in the namespace object. Transition the object
  // to dictionary mode so that property addition is faster.
  PropertyAttributes attr = DONT_DELETE;
  JSObject::NormalizeProperties(isolate, ns, CLEAR_INOBJECT_PROPERTIES,
                                static_cast<int>(names.size()),
                                "JSModuleNamespace");
  JSObject::NormalizeElements(isolate, ns);
  DirectHandle<AccessorInfo> module_namespace_property_accessor =
      isolate->factory()->module_namespace_property_accessor();
  for (const auto& name : names) {
    uint32_t index = 0;
    if (name->AsArrayIndex(&index)) {
      JSObject::SetNormalizedElement(
          ns, index, module_namespace_property_accessor,
          PropertyDetails(PropertyKind::kAccessor, attr,
                          PropertyCellType::kMutable));
    } else {
      JSObject::SetNormalizedProperty(
          ns, name, module_namespace_property_accessor,
          PropertyDetails(PropertyKind::kAccessor, attr,
                          PropertyCellType::kMutable))
          .Check();
    }
  }
  JSObject::PreventExtensions(isolate, ns, kThrowOnError).ToChecked();

  // Optimize the namespace object as a prototype, for two reasons:
  // - The object's map is guaranteed not to be shared. ICs rely on this.
  // - We can store a pointer from the map back to the namespace object.
  //   Turbofan can use this for inlining the access.
  JSObject::OptimizeAsPrototype(ns);

  DirectHandle<PrototypeInfo> proto_info =
      Map::GetOrCreatePrototypeInfo(ns, isolate);
  proto_info->set_module_namespace(*ns);
  return ns;
}

bool JSModuleNamespace::HasExport(Isolate* isolate, DirectHandle<String> name) {
  DirectHandle<Object> object(module()->exports()->Lookup(name), isolate);
  return !IsTheHole(*object);
}

MaybeDirectHandle<Object> JSModuleNamespace::GetExport(
    Isolate* isolate, DirectHandle<String> name) {
  DirectHandle<Object> object(module()->exports()->Lookup(name), isolate);
  if (IsTheHole(*object)) {
    return isolate->factory()->undefined_value();
  }

  DirectHandle<Object> value(Cast<Cell>(*object)->value(), isolate);
  if (IsTdzHole(*value)) {
    // According to https://tc39.es/ecma262/#sec-InnerModuleLinking
    // step 10 and
    // https://tc39.es/ecma262/#sec-source-text-module-record-initialize-environment
    // step 8-25, variables must be declared in Link. And according to
    // https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-get-p-receiver,
    // here accessing uninitialized variable error should be thrown.
    THROW_NEW_ERROR(isolate,
                    NewReferenceError(
                        MessageTemplate::kAccessedUninitializedVariable, name));
  }

  return value;
}

// static
void JSModuleNamespace::MaybeCountMissingDefaultWithStarExport(
    LookupIterator* it) {
  DCHECK_EQ(it->state(), LookupIterator::MODULE_NAMESPACE);

  if (it->IsElement()) return;

  Isolate* isolate = it->isolate();
  DirectHandle<String> default_string = isolate->factory()->default_string();
  if (*it->name() != *default_string) return;

  DirectHandle<JSModuleNamespace> ns = it->GetHolder<JSModuleNamespace>();
  if (ns->HasExport(isolate, default_string)) return;

  {
    DisallowGarbageCollection no_gc;
    Tagged<Module> module = ns->module();
    if (!IsSourceTextModule(module)) return;
    if (!Cast<SourceTextModule>(module)->info()->HasStarExports()) return;
  }

  isolate->CountUsage(
      v8::Isolate::kModuleNamespaceMissingDefaultWithStarExport);
}

Maybe<PropertyAttributes> JSModuleNamespace::GetPropertyAttributes(
    LookupIterator* it) {
  DirectHandle<JSModuleNamespace> object = it->GetHolder<JSModuleNamespace>();
  DirectHandle<String> name = Cast<String>(it->GetName());
  DCHECK(it->state() == LookupIterator::ACCESSOR ||
         it->state() == LookupIterator::MODULE_NAMESPACE);

  Isolate* isolate = it->isolate();

  DirectHandle<Object> lookup(object->module()->exports()->Lookup(name),
                              isolate);
  if (IsTheHole(*lookup)) return Just(ABSENT);

  DirectHandle<Object> value(Cast<Cell>(lookup)->value(), isolate);
  if (IsTdzHole(*value)) {
    isolate->Throw(*isolate->factory()->NewReferenceError(
        MessageTemplate::kNotDefined, name));
    return Nothing<PropertyAttributes>();
  }

  if (IsJSDeferredModuleNamespace(*object)) [[unlikely]] {
    return Just(DONT_DELETE);
  } else {
    return Just(it->property_attributes());
  }
}

// https://tc39.es/proposal-defer-import-eval/#sec-GetModuleExportsList
void JSDeferredModuleNamespace::EvaluateModuleSync(
    Isolate* isolate, DirectHandle<JSDeferredModuleNamespace> holder) {
  // Callers check isolate->has_exception().
  USE(Module::EvaluateModuleSync(isolate, handle(holder->module(), isolate)));
}

bool JSDeferredModuleNamespace::TriggersEvaluation(LookupIterator* it) {
  DirectHandle<JSModuleNamespace> ns = it->GetHolder<JSModuleNamespace>();
  if (IsJSDeferredModuleNamespace(*ns)) {
    Isolate* isolate = it->isolate();
    DirectHandle<Name> name = it->GetName();
    // https://tc39.es/proposal-defer-import-eval/#sec-IsSymbolLikeNamespaceKey
    if (*name == ReadOnlyRoots(isolate).then_string() || IsSymbol(*name)) {
      return false;
    }
    return ns->module()->status() != Module::kEvaluated;
  }

  return false;
}

// ES
// https://tc39.es/ecma262/#sec-module-namespace-exotic-objects-defineownproperty-p-desc
// static
Maybe<bool> JSModuleNamespace::DefineOwnProperty(
    Isolate* isolate, DirectHandle<JSModuleNamespace> object,
    DirectHandle<Object> key, PropertyDescriptor* desc,
    Maybe<ShouldThrow> should_throw) {
  // 1. If Type(P) is Symbol, return OrdinaryDefineOwnProperty(O, P, Desc).
  if (IsSymbol(*key)) {
    return JSReceiver::OrdinaryDefineOwnProperty(isolate, object, key, desc,
                                                 should_throw);
  }

  // 2. Let current be ? O.[[GetOwnProperty]](P).
  PropertyKey lookup_key(isolate, key);
  LookupIterator it(isolate, object, lookup_key, LookupIterator::OWN);
  PropertyDescriptor current;
  Maybe<bool> has_own = JSReceiver::GetOwnPropertyDescriptor(&it, &current);
  MAYBE_RETURN(has_own, Nothing<bool>());

  // 3. If current is undefined, return false.
  // 4. If Desc.[[Configurable]] is present and has value true, return false.
  // 5. If Desc.[[Enumerable]] is present and has value false, return false.
  // 6. If ! IsAccessorDescriptor(Desc) is true, return false.
  // 7. If Desc.[[Writable]] is present and has value false, return false.
  // 8. If Desc.[[Value]] is present, return
  //    SameValue(Desc.[[Value]], current.[[Value]]).
  if (!has_own.FromJust() ||
      (desc->has_configurable() && desc->configurable()) ||
      (desc->has_enumerable() && !desc->enumerable()) ||
      PropertyDescriptor::IsAccessorDescriptor(desc) ||
      (desc->has_writable() && !desc->writable()) ||
      (desc->has_value() &&
       !Object::SameValue(*desc->value(), *current.value()))) {
    RETURN_FAILURE(isolate, GetShouldThrow(isolate, should_throw),
                   NewTypeError(MessageTemplate::kRedefineDisallowed, key));
  }

  return Just(true);
}

bool Module::IsGraphAsync(Isolate* isolate) const {
  DisallowGarbageCollection no_gc;

  // Only SourceTextModules may be async.
  if (!Is<SourceTextModule>(this)) return false;
  Tagged<SourceTextModule> root = Cast<SourceTextModule>(this);
  DCHECK_GE(root->status(), kLinked);

  Zone zone(isolate->allocator(), ZONE_NAME);
  const size_t bucket_count = 2;
  ZoneUnorderedSet<Tagged<SourceTextModule>, Module::Hash> visited(
      &zone, bucket_count);
  ZoneVector<Tagged<SourceTextModule>> worklist(&zone);
  visited.insert(root);
  worklist.push_back(root);

  do {
    Tagged<SourceTextModule> current = worklist.back();
    worklist.pop_back();
    DCHECK_GE(current->status(), kLinked);

    if (current->has_toplevel_await()) return true;
    Tagged<FixedArray> requested_modules = current->requested_modules();
    const uint32_t requested_modules_len = requested_modules->ulength().value();
    for (uint32_t i = 0; i < requested_modules_len; ++i) {
      Tagged<Object> raw_descendant = requested_modules->get(i);
      // The current module must have been linked as the root has been linked.
      // If the request is a source phase import, the descendant can be a
      // JavaScript object, and it can not be async. Skip it.
      // If the request is an evaluation phase import, the descendant can be
      // either a SourceTextModule or a SyntheticModule. Visit it if it is a
      // SourceTextModule.
      if (IsSourceTextModule(raw_descendant)) {
        Tagged<SourceTextModule> descendant =
            Cast<SourceTextModule>(raw_descendant);
        const bool cycle = !visited.insert(descendant).second;
        if (!cycle) worklist.push_back(descendant);
      }
    }
  } while (!worklist.empty());

  return false;
}

}  // namespace internal
}  // namespace v8
