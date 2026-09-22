// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scopes.h"

#include <algorithm>
#include <optional>

#include "src/ast/modules.h"
#include "src/debug/debug-scope-info.h"
#include "src/debug/debug.h"
#include "src/execution/frames-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/source-text-module.h"
#include "src/objects/string-set.h"
#include "src/utils/ostreams.h"

namespace v8 {
namespace internal {

ScopeIterator::ScopeIterator(Isolate* isolate, FrameInspector* frame_inspector,
                             CalculateBlocklists calculate_blocklists)
    : isolate_(isolate),
      frame_inspector_(frame_inspector),
      function_(frame_inspector_->GetFunction()),
      script_(frame_inspector_->GetScript()) {
  if (!IsContext(*frame_inspector->GetContext())) {
    // Optimized frame, context or function cannot be materialized. Give up.
    return;
  }
  context_ = Cast<Context>(frame_inspector->GetContext());

#if V8_ENABLE_WEBASSEMBLY
  // We should not instantiate a ScopeIterator for wasm frames.
  DCHECK_NE(Script::Type::kWasm, frame_inspector->GetScript()->type());
#endif  // V8_ENABLE_WEBASSEMBLY

  TryParseAndRetrieveScopes(calculate_blocklists);
}

ScopeIterator::~ScopeIterator() = default;

DirectHandle<Object> ScopeIterator::GetFunctionDebugName() const {
  if (!function_.is_null()) {
    return JSFunction::GetDebugName(isolate_, function_);
  }

  if (!IsNativeContext(*context_)) {
    DisallowGarbageCollection no_gc;
    Tagged<ScopeInfo> closure_info = context_->closure_context()->scope_info();
    DirectHandle<String> debug_name(closure_info->FunctionDebugName(),
                                    isolate_);
    if (debug_name->length() > 0) return debug_name;
  }
  return isolate_->factory()->undefined_value();
}

ScopeIterator::ScopeIterator(Isolate* isolate,
                             DirectHandle<JSFunction> function)
    : isolate_(isolate), context_(function->context(), isolate) {
  if (!function->shared()->IsSubjectToDebugging()) {
    context_ = Handle<Context>();
    return;
  }
  script_ = handle(Cast<Script>(function->shared()->script()), isolate);
  UnwrapEvaluationContext();
}

ScopeIterator::ScopeIterator(Isolate* isolate,
                             Handle<JSGeneratorObject> generator)
    : isolate_(isolate),
      generator_(generator),
      function_(generator->function(), isolate),
      context_(generator->context(), isolate),
      script_(Cast<Script>(function_->shared()->script()), isolate) {
  CHECK(function_->shared()->IsSubjectToDebugging());
  TryParseAndRetrieveScopes(CalculateBlocklists::kNo);
}

void ScopeIterator::Restart() {
  DCHECK_NOT_NULL(frame_inspector_);
  function_ = frame_inspector_->GetFunction();
  context_ = Cast<Context>(frame_inspector_->GetContext());
  current_scope_index_ = start_scope_index_;
  DCHECK_NE(current_scope_index_, -1);
  UnwrapEvaluationContext();
  seen_script_scope_ = false;
  calculate_blocklists_ = false;
}

void ScopeIterator::TryParseAndRetrieveScopes(
    CalculateBlocklists calculate_blocklists) {
  // Catch the case when the debugger stops in an internal function.
  DirectHandle<SharedFunctionInfo> shared_info(function_->shared(), isolate_);
  DirectHandle<ScopeInfo> scope_info(shared_info->scope_info(), isolate_);
  if (IsUndefined(shared_info->script())) {
    current_scope_index_ = closure_scope_index_ = start_scope_index_ = -1;
    context_ = handle(function_->context(), isolate_);
    function_ = Handle<JSFunction>();
    return;
  }

  bool ignore_nested_scopes = false;
  if (shared_info->HasBreakInfo(isolate_) && frame_inspector_ != nullptr) {
    // The source position at return is always the end of the function,
    // which is not consistent with the current scope chain. Therefore all
    // nested with, catch and block contexts are skipped, and we can only
    // inspect the function scope.
    // This can only happen if we set a break point inside right before the
    // return, which requires a debug info to be available.
    Handle<DebugInfo> debug_info(shared_info->GetDebugInfo(isolate_), isolate_);

    // Find the break point where execution has stopped.
    BreakLocation location = BreakLocation::FromFrame(debug_info, GetFrame());

    ignore_nested_scopes = location.IsReturn();
  }

  if (calculate_blocklists == CalculateBlocklists::kIfNeeded) {
    Tagged<UnionOf<TheHole, StringSet>> maybe_block_list =
        isolate_->LocalsBlockListCacheGet(scope_info);
    calculate_blocklists_ = IsTheHole(maybe_block_list);
  }

  DirectHandle<Script> script(Cast<Script>(shared_info->script()), isolate_);
  debug_scope_info_ = EnsureDebugScriptScopeInfo(isolate_, script);
  if (debug_scope_info_.is_null()) {
    // A failed reparse indicates that the preparser has diverged from the
    // parser, that the preparse data given to the initial parse was faulty, or
    // a stack overflow.
    // Silently fail by presenting an empty context chain.
    context_ = Handle<Context>();
    return;
  }

  // For a FUNCTION_SCOPE we locate the paused function's scope in the
  // serialized tree. For top-level scopes (EVAL_SCOPE, SCRIPT_SCOPE,
  // MODULE_SCOPE) the closure scope is the root scope (index 0).
  std::optional<DebugScriptScope> debug_closure_scope =
      scope_info->scope_type() == FUNCTION_SCOPE
          ? FindClosureScope(debug_scope_info_, shared_info->StartPosition(),
                             shared_info->EndPosition(),
                             scope_info->scope_type())
          : DebugScriptScope::FromIndex(debug_scope_info_, 0);
  if (!debug_closure_scope.has_value()) {
    context_ = Handle<Context>();
    return;
  }
  closure_scope_index_ = debug_closure_scope->scope_index();
  start_scope_index_ =
      FindInnermostScope(*debug_closure_scope, GetSourcePosition())
          .scope_index();
  current_scope_index_ = start_scope_index_;

  if (ignore_nested_scopes) {
    current_scope_index_ = closure_scope_index_;
    start_scope_index_ = current_scope_index_;
    // ignore_nested_scopes is only used for the return-position breakpoint,
    // so we can safely assume that the closure context for the current
    // function exists if it needs one.
    if (closure_scope().needs_context()) {
      context_ = handle(context_->closure_context(), isolate_);
    }
  }

  MaybeCollectAndStoreLocalBlocklists();
  UnwrapEvaluationContext();
}

void ScopeIterator::UnwrapEvaluationContext() {
  if (!context_->IsDebugEvaluateContext()) return;
  Tagged<Context> current = *context_;
  do {
    Tagged<Object> wrapped = current->GetNoCell(Context::WRAPPED_CONTEXT_INDEX);
    if (IsContext(wrapped)) {
      current = Cast<Context>(wrapped);
    } else {
      DCHECK(!current->previous().is_null());
      current = current->previous();
    }
  } while (current->IsDebugEvaluateContext());
  context_ = handle(current, isolate_);
}

DirectHandle<JSObject> ScopeIterator::MaterializeScopeDetails() {
  // Calculate the size of the result.
  DirectHandle<FixedArray> details =
      isolate_->factory()->NewFixedArray(kScopeDetailsSize);
  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(Type()));
  DirectHandle<JSObject> scope_object = ScopeObject(Mode::ALL);
  details->set(kScopeDetailsObjectIndex, *scope_object);
  if (Type() == ScopeTypeGlobal || Type() == ScopeTypeScript) {
    return isolate_->factory()->NewJSArrayWithElements(details);
  } else if (HasContext()) {
    DirectHandle<Object> closure_name = GetFunctionDebugName();
    details->set(kScopeDetailsNameIndex, *closure_name);
    details->set(kScopeDetailsStartPositionIndex,
                 Smi::FromInt(start_position()));
    details->set(kScopeDetailsEndPositionIndex, Smi::FromInt(end_position()));
    if (InInnerScope()) {
      details->set(kScopeDetailsFunctionIndex, *function_);
    }
  }
  return isolate_->factory()->NewJSArrayWithElements(details);
}

bool ScopeIterator::HasPositionInfo() {
  return InInnerScope() || !IsNativeContext(*context_);
}

int ScopeIterator::start_position() {
  if (InInnerScope()) return current_scope().start_position();
  if (IsNativeContext(*context_)) return 0;
  return context_->closure_context()->scope_info()->StartPosition();
}

int ScopeIterator::end_position() {
  if (InInnerScope()) return current_scope().end_position();
  if (IsNativeContext(*context_)) return 0;
  return context_->closure_context()->scope_info()->EndPosition();
}

bool ScopeIterator::DeclaresLocals(Mode mode) const {
  ScopeType type = Type();

  if (type == ScopeTypeWith) return mode == Mode::ALL;
  if (type == ScopeTypeGlobal) return mode == Mode::ALL;

  bool declares_local = false;
  auto visitor = [&](DirectHandle<String> name, DirectHandle<Object> value,
                     ScopeType scope_type) {
    declares_local = true;
    return true;
  };
  VisitScope(visitor, mode);
  return declares_local;
}

bool ScopeIterator::ShouldIgnore() const {
  if (Type() == ScopeTypeLocal ||
      (Type() == ScopeTypeModule && InInnerScope())) {
    return false;
  }
  return !DeclaresLocals(Mode::ALL);
}

bool ScopeIterator::AdvanceToScopeNumber(int scope_number) {
  while (!Done() && ShouldIgnore()) Next();
  while (!Done() && scope_number > 0) {
    --scope_number;
    Next();
    while (!Done() && ShouldIgnore()) Next();
  }
  return scope_number == 0 && !Done();
}

bool ScopeIterator::HasContext() const {
  // In rare cases we pause in a scope that doesn't have its context pushed yet.
  // E.g. when pausing in for-of loop headers (see https://crbug.com/399002824).
  //
  // We can detect this by comparing the scope ID of the parsed scope and the
  // runtime scope.
  if (current_scope_index_ != -1 && NeedsContext() &&
      current_scope().unique_id_in_script() !=
          context_->scope_info()->UniqueIdInScript()) {
    return false;
  }

  return !InInnerScope() || NeedsContext();
}

bool ScopeIterator::NeedsContext() const {
  const bool needs_context = current_scope().needs_context();

  // We try very hard to ensure that a function's context is already
  // available when we pause right at the beginning of that function.
  // This can be tricky when we pause via stack check or via
  // `BreakOnNextFunctionCall`, which happens normally in the middle of frame
  // construction and we have to "step into" the function first.
  //
  // We check this by ensuring that the current context is not the closure
  // context should the function need one. In that case the function has already
  // pushed the context and we are good.
  CHECK_IMPLIES(needs_context && current_scope_index_ == closure_scope_index_ &&
                    current_scope().is_function_scope() && !function_.is_null(),
                function_->context() != *context_);

  return needs_context;
}

bool ScopeIterator::AdvanceOneScope() {
  if (current_scope_index_ == -1) return false;
  std::optional<DebugScriptScope> parent = current_scope().parent();
  if (!parent.has_value()) return false;
  current_scope_index_ = parent->scope_index();
  return true;
}

void ScopeIterator::AdvanceOneContext() {
  DCHECK(!IsNativeContext(*context_));
  DCHECK(!context_->previous().is_null());
  context_ = handle(context_->previous(), isolate_);
}

void ScopeIterator::AdvanceScope() {
  DCHECK(InInnerScope());

  do {
    if (NeedsAndHasContext()) {
      // current_scope() needs a context so moving one scope up requires us to
      // also move up one context.
      AdvanceOneContext();
    }

    CHECK(AdvanceOneScope());
  } while (current_scope().is_hidden());
}

void ScopeIterator::AdvanceContext() { AdvanceOneContext(); }

void ScopeIterator::Next() {
  DCHECK(!Done());

  ScopeType scope_type = Type();

  if (scope_type == ScopeTypeGlobal) {
    // The global scope is always the last in the chain.
    DCHECK(IsNativeContext(*context_));
    context_ = Handle<Context>();
    DCHECK(Done());
    return;
  }

  bool leaving_closure = current_scope_index_ == closure_scope_index_;

  if (scope_type == ScopeTypeScript) {
    DCHECK_IMPLIES(InInnerScope() && !leaving_closure,
                   current_scope().is_script_scope());
    seen_script_scope_ = true;
    if (context_->IsScriptContext()) {
      context_ = handle(context_->previous(), isolate_);
    }
    if (leaving_closure) {
      current_scope_index_ = -1;
    }
  } else if (!InInnerScope()) {
    AdvanceContext();
  } else {
    DCHECK_NE(current_scope_index_, -1);
    if (leaving_closure) {
      // DebugScriptScope represents the entire script's scope tree, so calling
      // AdvanceScope() here would step into the enclosing outer scope. Outside
      // the paused closure, ScopeIterator iterates purely via runtime context_
      // (!InInnerScope()). Consume the closure's context if it had one and
      // reset the scope cursor.
      if (NeedsAndHasContext()) {
        AdvanceOneContext();
      }
      current_scope_index_ = -1;
    } else {
      AdvanceScope();
    }
  }

  MaybeCollectAndStoreLocalBlocklists();
  UnwrapEvaluationContext();

  DCHECK_IMPLIES(current_scope_index_ != -1 && NeedsAndHasContext(),
                 current_scope().unique_id_in_script() ==
                     context_->scope_info()->UniqueIdInScript());

  if (leaving_closure) function_ = Handle<JSFunction>();
}

// Return the type of the current scope.
ScopeIterator::ScopeType ScopeIterator::Type() const {
  DCHECK(!Done());
  if (InInnerScope()) {
    switch (current_scope().scope_type()) {
      case FUNCTION_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(),
                       context_->IsFunctionContext() ||
                           context_->IsDebugEvaluateContext());
        return ScopeTypeLocal;
      case MODULE_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(), context_->IsModuleContext());
        return ScopeTypeModule;
      case SCRIPT_SCOPE:
      case REPL_MODE_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(), context_->IsScriptContext() ||
                                                 IsNativeContext(*context_));
        return ScopeTypeScript;
      case WITH_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(), context_->IsWithContext());
        return ScopeTypeWith;
      case CATCH_SCOPE:
        DCHECK(context_->IsCatchContext());
        return ScopeTypeCatch;
      case BLOCK_SCOPE:
      case CLASS_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(), context_->IsBlockContext());
        return ScopeTypeBlock;
      case EVAL_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(), context_->IsEvalContext());
        return ScopeTypeEval;
      case SHADOW_REALM_SCOPE:
        DCHECK_IMPLIES(NeedsAndHasContext(), IsNativeContext(*context_));
        // TODO(v8:11989): New ScopeType for ShadowRealms?
        return ScopeTypeScript;
    }
    UNREACHABLE();
  }
  if (IsNativeContext(*context_)) {
    DCHECK(IsJSGlobalObject(context_->global_object()));
    // If we are at the native context and have not yet seen script scope,
    // fake it.
    return seen_script_scope_ ? ScopeTypeGlobal : ScopeTypeScript;
  }
  if (context_->IsFunctionContext() || context_->IsEvalContext() ||
      context_->IsDebugEvaluateContext()) {
    return ScopeTypeClosure;
  }
  if (context_->IsCatchContext()) {
    return ScopeTypeCatch;
  }
  if (context_->IsBlockContext()) {
    return ScopeTypeBlock;
  }
  if (context_->IsModuleContext()) {
    return ScopeTypeModule;
  }
  if (context_->IsScriptContext()) {
    return ScopeTypeScript;
  }
  DCHECK(context_->IsWithContext());
  return ScopeTypeWith;
}

Handle<JSObject> ScopeIterator::ScopeObject(Mode mode) {
  DCHECK(!Done());

  ScopeType type = Type();
  if (type == ScopeTypeGlobal) {
    DCHECK_EQ(Mode::ALL, mode);
    return handle(context_->global_proxy(), isolate_);
  }
  if (type == ScopeTypeWith) {
    DCHECK_EQ(Mode::ALL, mode);
    return WithContextExtension();
  }

  Handle<JSObject> scope = isolate_->factory()->NewSlowJSObjectWithNullProto();
  auto visitor = [=, this](DirectHandle<String> name,
                           DirectHandle<Object> value, ScopeType scope_type) {
    if (IsOptimizedOut(*value)) {
      JSObject::SetAccessor(
          scope, name, isolate_->factory()->value_unavailable_accessor(), NONE)
          .Check();
    } else if (IsTheHole(*value)) {
      const bool is_overriden_repl_let =
          scope_type == ScopeTypeScript &&
          JSReceiver::HasOwnProperty(isolate_, scope, name).FromMaybe(true);
      if (!is_overriden_repl_let) {
        // We also use the hole to represent overridden let-declarations via
        // REPL mode in a script context. Don't install the unavailable accessor
        // in that case.
        JSObject::SetAccessor(scope, name,
                              isolate_->factory()->value_unavailable_accessor(),
                              NONE)
            .Check();
      }
    } else {
      // Overwrite properties. Sometimes names in the same scope can collide,
      // e.g. with extension objects introduced via local eval.
      Object::SetPropertyOrElement(isolate_, scope, name, value,
                                   Just(ShouldThrow::kDontThrow))
          .Check();
    }
    return false;
  };

  VisitScope(visitor, mode);
  return scope;
}

void ScopeIterator::VisitScope(const Visitor& visitor, Mode mode) const {
  switch (Type()) {
    case ScopeTypeLocal:
    case ScopeTypeClosure:
    case ScopeTypeCatch:
    case ScopeTypeBlock:
    case ScopeTypeEval:
      return VisitLocalScope(visitor, mode, Type());
    case ScopeTypeModule:
      if (InInnerScope()) {
        return VisitLocalScope(visitor, mode, Type());
      }
      DCHECK_EQ(Mode::ALL, mode);
      return VisitModuleScope(visitor);
    case ScopeTypeScript:
      DCHECK_EQ(Mode::ALL, mode);
      return VisitScriptScope(visitor);
    case ScopeTypeWith:
    case ScopeTypeGlobal:
      UNREACHABLE();
  }
}

bool ScopeIterator::SetVariableValue(Handle<String> name,
                                     DirectHandle<Object> value) {
  DCHECK(!Done());
  name = isolate_->factory()->InternalizeString(name);
  // Synthetic variables are compiler-introduced and not exposed to the user, so
  // they may carry values outside the JSAny type and must not be overwritten.
  if (ScopeInfo::VariableIsSynthetic(*name)) return false;
  switch (Type()) {
    case ScopeTypeGlobal:
    case ScopeTypeWith:
      break;

    case ScopeTypeEval:
    case ScopeTypeBlock:
    case ScopeTypeCatch:
    case ScopeTypeModule:
      if (InInnerScope()) return SetLocalVariableValue(name, value);
      if (Type() == ScopeTypeModule && SetModuleVariableValue(name, value)) {
        return true;
      }
      return SetContextVariableValue(name, value);

    case ScopeTypeLocal:
    case ScopeTypeClosure:
      if (InInnerScope()) {
        DCHECK_EQ(ScopeTypeLocal, Type());
        if (SetLocalVariableValue(name, value)) return true;
        // There may not be an associated context since we're InInnerScope().
        if (!NeedsContext()) return false;
      } else {
        DCHECK_EQ(ScopeTypeClosure, Type());
        if (SetContextVariableValue(name, value)) return true;
      }
      // The above functions only set variables statically declared in the
      // function. There may be eval-introduced variables. Check them in
      // SetContextExtensionValue.
      return SetContextExtensionValue(name, value);

    case ScopeTypeScript:
      return SetScriptVariableValue(name, value);
  }
  return false;
}

bool ScopeIterator::ClosureScopeHasThisReference() const {
  // closure_scope_index_ can be -1 if parsing failed.
  return closure_scope_index_ != -1 &&
         !closure_scope().has_this_declaration() &&
         closure_scope().has_this_reference();
}

#ifdef DEBUG
// Debug print of the content of the current scope.
void ScopeIterator::DebugPrint() {
  StdoutStream os;
  DCHECK(!Done());
  switch (Type()) {
    case ScopeIterator::ScopeTypeGlobal:
      os << "Global:\n";
      Print(*context_, os);
      break;

    case ScopeIterator::ScopeTypeLocal: {
      os << "Local:\n";
      if (NeedsContext()) {
        Print(*context_, os);
        if (context_->has_extension()) {
          DirectHandle<HeapObject> extension(context_->extension(), isolate_);
          DCHECK(IsJSContextExtensionObject(*extension));
          Print(*extension, os);
        }
      }
      break;
    }

    case ScopeIterator::ScopeTypeWith:
      os << "With:\n";
      Print(context_->extension(), os);
      break;

    case ScopeIterator::ScopeTypeCatch:
      os << "Catch:\n";
      Print(context_->extension(), os);
      Print(context_->GetNoCell(Context::THROWN_OBJECT_INDEX), os);
      break;

    case ScopeIterator::ScopeTypeClosure:
      os << "Closure:\n";
      Print(*context_, os);
      if (context_->has_extension()) {
        DirectHandle<HeapObject> extension(context_->extension(), isolate_);
        DCHECK(IsJSContextExtensionObject(*extension));
        Print(*extension, os);
      }
      break;

    case ScopeIterator::ScopeTypeScript:
      os << "Script:\n";
      Print(context_->native_context()->script_context_table(), os);
      break;

    default:
      UNREACHABLE();
  }
  PrintF("\n");
}
#endif

int ScopeIterator::GetSourcePosition() const {
  if (frame_inspector_) {
    return frame_inspector_->GetSourcePosition();
  } else {
    DCHECK(!generator_.is_null());
    SharedFunctionInfo::EnsureSourcePositionsAvailable(
        isolate_, direct_handle(generator_->function()->shared(), isolate_));
    return generator_->source_position();
  }
}

void ScopeIterator::VisitScriptScope(const Visitor& visitor) const {
  DirectHandle<ScriptContextTable> script_contexts(
      context_->native_context()->script_context_table(), isolate_);

  // Skip the first script since that just declares 'this'.
  const uint32_t len = script_contexts->length(kAcquireLoad).value();
  for (uint32_t i = 1; i < len; i++) {
    DirectHandle<Context> context(script_contexts->get(i), isolate_);
    DirectHandle<ScopeInfo> scope_info(context->scope_info(), isolate_);
    if (VisitContextLocals(visitor, scope_info, context, ScopeTypeScript)) {
      return;
    }
  }
}

void ScopeIterator::VisitModuleScope(const Visitor& visitor) const {
  DCHECK(context_->IsModuleContext());

  DirectHandle<ScopeInfo> scope_info(context_->scope_info(), isolate_);
  if (VisitContextLocals(visitor, scope_info, context_, ScopeTypeModule)) {
    return;
  }

  int module_variable_count = scope_info->ModuleVariableCount();

  DirectHandle<SourceTextModule> module(context_->module(), isolate_);

  for (int i = 0; i < module_variable_count; ++i) {
    int index;
    Handle<String> name;
    {
      Tagged<String> raw_name;
      scope_info->ModuleVariable(i, &raw_name, &index);
      if (ScopeInfo::VariableIsSynthetic(raw_name)) continue;
      name = handle(raw_name, isolate_);
    }
    Handle<Object> value =
        SourceTextModule::LoadVariable(isolate_, module, index);

    if (visitor(name, value, ScopeTypeModule)) return;
  }
}

bool ScopeIterator::VisitContextLocals(const Visitor& visitor,
                                       DirectHandle<ScopeInfo> scope_info,
                                       DirectHandle<Context> context,
                                       ScopeType scope_type) const {
  // Fill all context locals to the context extension.
  for (auto it : ScopeInfo::IterateLocalNames(scope_info)) {
    Handle<String> name(it->name(), isolate_);
    if (ScopeInfo::VariableIsSynthetic(*name)) continue;
    int context_index = scope_info->ContextHeaderLength() + it->index();
    Handle<Object> value = indirect_handle(
        Context::Get(context, context_index, isolate_), isolate_);
    if (visitor(name, value, scope_type)) return true;
  }
  return false;
}

bool ScopeIterator::VisitLocals(const Visitor& visitor, Mode mode,
                                ScopeType scope_type) const {
  if (mode == Mode::STACK && current_scope().has_this_declaration()) {
    // TODO(bmeurer): We should refactor the general variable lookup
    // around "this", since the current way is rather hacky when the
    // receiver is context-allocated.
    auto [this_alloc, this_index] = current_scope().receiver_info();
    Handle<Object> receiver =
        this_alloc == VariableAllocationInfo::CONTEXT
            ? handle(context_->GetNoCell(this_index), isolate_)
        : frame_inspector_ == nullptr ? handle(generator_->receiver(), isolate_)
                                      : frame_inspector_->GetReceiver();
    if (visitor(isolate_->factory()->this_string(), receiver, scope_type)) {
      return true;
    }
  }

  if (current_scope().has_function_variable()) {
    DirectHandle<JSFunction> function = frame_inspector_ == nullptr
                                            ? function_
                                            : frame_inspector_->GetFunction();
    DirectHandle<String> name(current_scope().function_variable_name(),
                              isolate_);
    if (visitor(name, function, scope_type)) return true;
  }

  DebugScriptScope scope = current_scope();
  auto [args_alloc, args_index] = scope.arguments_info();
  for (int i = 0; i < scope.variable_count(); ++i) {
    DebugVariableInfo var = scope.variable(i);
    if (var.is_synthetic) {
      // We want to materialize "new.target" for debug-evaluate.
      if (mode != Mode::STACK ||
          !var.name->Equals(*isolate_->factory()->dot_new_target_string())) {
        continue;
      }
    }

    int index = var.index;
    Handle<Object> value;
    switch (var.location) {
      case VariableLocation::LOOKUP:
        UNREACHABLE();

      case VariableLocation::REPL_GLOBAL:
        // REPL declared variables are ignored for now.
      case VariableLocation::UNALLOCATED:
        continue;

      case VariableLocation::PARAMETER: {
        if (frame_inspector_ == nullptr) {
          // Get the variable from the suspended generator.
          DCHECK(!generator_.is_null());
          Tagged<FixedArray> parameters_and_registers =
              generator_->parameters_and_registers();
          CHECK_GE(index, 0);
          CHECK_LT(static_cast<uint32_t>(index),
                   parameters_and_registers->ulength().value());
          value = handle(parameters_and_registers->get(index), isolate_);
        } else if (var.is_receiver) {
          value = frame_inspector_->GetReceiver();
        } else {
          JavaScriptFrame* frame = GetFrame();
          if (frame->is_unoptimized()) {
            CHECK_GE(index, 0);
            CHECK_LT(static_cast<uint32_t>(index),
                     std::max(frame->GetActualArgumentCount(),
                              frame->ComputeParametersCount()));
          }
          value = frame_inspector_->GetParameter(index);
        }
        break;
      }

      case VariableLocation::LOCAL:
        if (frame_inspector_ == nullptr) {
          // Get the variable from the suspended generator.
          DCHECK(!generator_.is_null());
          Tagged<FixedArray> parameters_and_registers =
              generator_->parameters_and_registers();
          int parameter_count =
              function_->shared()->scope_info()->ParameterCount();
          index += parameter_count;
          CHECK_GE(index, 0);
          CHECK_LT(static_cast<uint32_t>(index),
                   parameters_and_registers->ulength().value());
          value = handle(parameters_and_registers->get(index), isolate_);
        } else {
          JavaScriptFrame* frame = GetFrame();
          if (frame->is_unoptimized()) {
            CHECK_GE(index, 0);
            CHECK_LT(index, frame->ComputeExpressionsCount());
          }
          value = frame_inspector_->GetExpression(index);
          if (IsOptimizedOut(*value)) {
            // We'll rematerialize this later.
            if (args_alloc == VariableAllocationInfo::STACK &&
                args_index == var.index) {
              continue;
            }
          } else if (IsLexicalVariableMode(var.mode) && IsUndefined(*value) &&
                     GetSourcePosition() != kNoSourcePosition &&
                     GetSourcePosition() <= var.initializer_position) {
            // Variables that are `undefined` could also mean an elided hole
            // write. We explicitly check the static scope information if we
            // are currently stopped before the variable is actually initialized
            // which means we are in the middle of that var's TDZ.
            value = isolate_->factory()->the_hole_value();
          }
        }
        break;

      case VariableLocation::CONTEXT:
        if (mode == Mode::STACK) continue;
        if (!HasContext()) {
          // If the context was not yet pushed we report the variable as
          // unavailable.
          value = isolate_->factory()->the_hole_value();
          break;
        }
        DCHECK_EQ(context_->scope_info()->ContextSlotIndex(var.name), index);
        value =
            indirect_handle(Context::Get(context_, index, isolate_), isolate_);
        break;

      case VariableLocation::MODULE: {
        if (mode == Mode::STACK) continue;
        // if (var->IsExport()) continue;
        DirectHandle<SourceTextModule> module(context_->module(), isolate_);
        value = SourceTextModule::LoadVariable(isolate_, module, var.index);
        break;
      }
    }

    if (visitor(direct_handle(var.name, isolate_), value, scope_type)) {
      return true;
    }
  }
  return false;
}

// Retrieve the with-context extension object. If the extension object is
// a proxy, return an empty object.
Handle<JSObject> ScopeIterator::WithContextExtension() {
  DCHECK(context_->IsWithContext());
  if (!IsJSObject(context_->extension_receiver())) {
    DCHECK(IsJSProxy(context_->extension_receiver()) ||
           IsWasmObject(context_->extension_receiver()));
    return isolate_->factory()->NewSlowJSObjectWithNullProto();
  }
  return handle(Cast<JSObject>(context_->extension_receiver()), isolate_);
}

// Create a plain JSObject which materializes the block scope for the specified
// block context.
void ScopeIterator::VisitLocalScope(const Visitor& visitor, Mode mode,
                                    ScopeType scope_type) const {
  if (InInnerScope()) {
    if (VisitLocals(visitor, mode, scope_type)) return;
    if (mode == Mode::STACK && Type() == ScopeTypeLocal) {
      // Hide |this| in arrow functions that may be embedded in other functions
      // but don't force |this| to be context-allocated. Otherwise we'd find the
      // wrong |this| value.
      if (!closure_scope().has_this_declaration() &&
          !closure_scope().has_this_reference()) {
        if (visitor(isolate_->factory()->this_string(),
                    isolate_->factory()->undefined_value(), scope_type)) {
          return;
        }
      }
      // Add |arguments| to the function scope even if it wasn't used.
      // Currently we don't yet support materializing the arguments object of
      // suspended generators. We'd need to read the arguments out from the
      // suspended generator rather than from an activation as
      // FunctionGetArguments does.
      if (frame_inspector_ != nullptr && !closure_scope().is_arrow_scope()) {
        auto [args_alloc, args_index] = closure_scope().arguments_info();
        bool arguments_optimized_out =
            args_alloc == VariableAllocationInfo::NONE;
        JavaScriptFrame* frame = GetFrame();
        if (!arguments_optimized_out &&
            args_alloc == VariableAllocationInfo::STACK) {
          if (frame->is_unoptimized()) {
            CHECK_GE(args_index, 0);
            CHECK_LT(args_index, frame->ComputeExpressionsCount());
          }
          arguments_optimized_out =
              IsOptimizedOut(*frame_inspector_->GetExpression(args_index));
        }

        if (arguments_optimized_out) {
          Handle<JSObject> arguments = Accessors::FunctionGetArguments(
              frame, frame_inspector_->inlined_frame_index());
          if (visitor(isolate_->factory()->arguments_string(), arguments,
                      scope_type)) {
            return;
          }
        }
      }
    }
  } else {
    DCHECK_EQ(Mode::ALL, mode);
    DirectHandle<ScopeInfo> scope_info(context_->scope_info(), isolate_);
    if (VisitContextLocals(visitor, scope_info, context_, scope_type)) return;
  }

  if (mode == Mode::ALL && HasContext()) {
    DCHECK(!context_->IsScriptContext());
    DCHECK(!IsNativeContext(*context_));
    DCHECK(!context_->IsWithContext());
    if (!context_->scope_info()->SloppyEvalCanExtendVars()) return;
    if (context_->extension_object().is_null()) return;
    DirectHandle<JSObject> extension(context_->extension_object(), isolate_);
    DirectHandle<FixedArray> keys =
        KeyAccumulator::GetKeys(isolate_, extension,
                                KeyCollectionMode::kOwnOnly, ENUMERABLE_STRINGS,
                                GetKeysConversion::kConvertToString, false,
                                true)
            .ToHandleChecked();

    uint32_t keys_len = keys->ulength().value();
    for (uint32_t i = 0; i < keys_len; i++) {
      // Names of variables introduced by eval are strings.
      DCHECK(IsString(keys->get(i)));
      Handle<String> key(Cast<String>(keys->get(i)), isolate_);
      Handle<Object> value =
          JSReceiver::GetDataProperty(isolate_, extension, key);
      if (visitor(key, value, scope_type)) return;
    }
  }
}

bool ScopeIterator::SetLocalVariableValue(DirectHandle<String> variable_name,
                                          DirectHandle<Object> new_value) {
  Tagged<InternalizedString> internalized_name =
      CheckedCast<InternalizedString>(*variable_name);
  DebugScriptScope scope = current_scope();
  for (int i = 0; i < scope.variable_count(); ++i) {
    DebugVariableInfo var = scope.variable(i);
    if (var.name == internalized_name) {
      int index = var.index;
      switch (var.location) {
        case VariableLocation::LOOKUP:
        case VariableLocation::UNALLOCATED:
          // Drop assignments to unallocated locals.
          DCHECK(*variable_name == ReadOnlyRoots(isolate_).this_string() ||
                 *variable_name == ReadOnlyRoots(isolate_).arguments_string());
          return false;

        case VariableLocation::REPL_GLOBAL:
          // Assignments to REPL declared variables are ignored for now.
          return false;

        case VariableLocation::PARAMETER: {
          if (var.is_receiver) return false;
          if (frame_inspector_ == nullptr) {
            // Set the variable in the suspended generator.
            DCHECK(!generator_.is_null());
            DirectHandle<FixedArray> parameters_and_registers(
                generator_->parameters_and_registers(), isolate_);
            CHECK_GE(index, 0);
            CHECK_LT(static_cast<uint32_t>(index),
                     parameters_and_registers->ulength().value());
            parameters_and_registers->set(index, *new_value);
          } else {
            JavaScriptFrame* frame = GetFrame();
            if (!frame->is_unoptimized()) return false;

            frame->SetParameterValue(index, *new_value);
          }
          return true;
        }

        case VariableLocation::LOCAL:
          if (frame_inspector_ == nullptr) {
            // Set the variable in the suspended generator.
            DCHECK(!generator_.is_null());
            int parameter_count =
                function_->shared()->scope_info()->ParameterCount();
            index += parameter_count;
            DirectHandle<FixedArray> parameters_and_registers(
                generator_->parameters_and_registers(), isolate_);
            CHECK_GE(index, 0);
            CHECK_LT(static_cast<uint32_t>(index),
                     parameters_and_registers->ulength().value());
            parameters_and_registers->set(index, *new_value);
          } else {
            // Set the variable on the stack.
            JavaScriptFrame* frame = GetFrame();
            if (!frame->is_unoptimized()) return false;

            CHECK_GE(index, 0);
            CHECK_LT(index, frame->ComputeExpressionsCount());
            frame->SetExpression(index, *new_value);
          }
          return true;

        case VariableLocation::CONTEXT:
          // We know of at least one open bug where the context and scope chain
          // don't match (https://crbug.com/753338).
          // Skip the write if the context's ScopeInfo doesn't know anything
          // about this variable.
          if (context_->scope_info()->ContextSlotIndex(*variable_name) !=
              index) {
            return false;
          }
          Context::Set(context_, index, new_value, isolate_);
          return true;

        case VariableLocation::MODULE:
          if (!var.is_export()) return false;
          DirectHandle<SourceTextModule> module(context_->module(), isolate_);
          SourceTextModule::StoreVariable(module, var.index, new_value);
          return true;
      }
      UNREACHABLE();
    }
  }

  return false;
}

bool ScopeIterator::SetContextExtensionValue(DirectHandle<String> variable_name,
                                             DirectHandle<Object> new_value) {
  if (!context_->has_extension()) return false;

  DCHECK(IsJSContextExtensionObject(context_->extension_object()));
  DirectHandle<JSObject> ext(context_->extension_object(), isolate_);
  LookupIterator it(isolate_, ext, variable_name, LookupIterator::OWN);
  Maybe<bool> maybe = JSReceiver::HasProperty(&it);
  DCHECK(maybe.IsJust());
  if (!maybe.FromJust()) return false;

  CHECK(Object::SetDataProperty(&it, new_value).ToChecked());
  return true;
}

bool ScopeIterator::SetContextVariableValue(DirectHandle<String> variable_name,
                                            DirectHandle<Object> new_value) {
  int slot_index = context_->scope_info()->ContextSlotIndex(*variable_name);
  if (slot_index < 0) return false;
  Context::Set(context_, slot_index, new_value, isolate_);
  return true;
}

bool ScopeIterator::SetModuleVariableValue(DirectHandle<String> variable_name,
                                           DirectHandle<Object> new_value) {
  DisallowGarbageCollection no_gc;
  int cell_index;
  VariableMode mode;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  cell_index = context_->scope_info()->ModuleIndex(
      *variable_name, &mode, &init_flag, &maybe_assigned_flag);

  // Setting imports is currently not supported.
  if (SourceTextModuleDescriptor::GetCellIndexKind(cell_index) !=
      SourceTextModuleDescriptor::kExport) {
    return false;
  }

  DirectHandle<SourceTextModule> module(context_->module(), isolate_);
  SourceTextModule::StoreVariable(module, cell_index, new_value);
  return true;
}

bool ScopeIterator::SetScriptVariableValue(DirectHandle<String> variable_name,
                                           DirectHandle<Object> new_value) {
  DirectHandle<ScriptContextTable> script_contexts(
      context_->native_context()->script_context_table(), isolate_);
  VariableLookupResult lookup_result;
  if (script_contexts->Lookup(variable_name, &lookup_result)) {
    DirectHandle<Context> script_context(
        script_contexts->get(lookup_result.context_index), isolate_);
    Context::Set(script_context, lookup_result.slot_index, new_value, isolate_);
    return true;
  }

  return false;
}

namespace {

// Given the scope and context of a paused function, this class calculates
// all the necessary block lists on the scope chain and stores them in the
// global LocalsBlockListCache ephemeron table.
//
// Doc: bit.ly/chrome-devtools-debug-evaluate-design.
//
// The algorithm works in a single walk of the scope chain from the
// paused function scope outwards to the script scope.
//
// When we step from scope "a" to its outer scope "b", we do:
//
//   1. Add all stack-allocated variables from "b" to the blocklists.
//   2. Does "b" need a context? If yes:
//        - Store all current blocklists in the global table
//        - Start a new blocklist for scope "b"
//   3. Is "b" a function scope without a context? If yes:
//        - Start a new blocklist for scope "b"
//
// The scope chain is read from the script's serialized scope tree. Scopes are
// identified by their index in that tree, so that the collector doesn't have
// to hold on to stack-allocated DebugScriptScope cursors.
class LocalBlocklistsCollector {
 public:
  LocalBlocklistsCollector(Isolate* isolate, Handle<Script> script,
                           Handle<Context> context,
                           Handle<DebugScriptScopeInfo> debug_scope_info,
                           int closure_scope_index);
  void CollectAndStore();

 private:
  DebugScriptScope scope() const {
    return DebugScriptScope::FromIndex(debug_scope_info_, scope_index_);
  }

  void InitializeWithClosureScope();
  void AdvanceToNextNonHiddenScope();
  void CollectCurrentLocalsIntoBlocklists();
  DirectHandle<ScopeInfo> FindScopeInfoForScope(int scope_index) const;
  void StoreFunctionBlocklists(DirectHandle<ScopeInfo> outer_scope_info);

  Isolate* isolate_;
  Handle<Script> script_;
  Handle<Context> context_;
  IndirectHandle<DebugScriptScopeInfo> debug_scope_info_;
  int scope_index_;
  const int closure_scope_index_;

  Handle<StringSet> context_blocklist_;
  std::map<int, IndirectHandle<StringSet>> function_blocklists_;
  bool has_matched_first_context_ = false;
};

LocalBlocklistsCollector::LocalBlocklistsCollector(
    Isolate* isolate, Handle<Script> script, Handle<Context> context,
    Handle<DebugScriptScopeInfo> debug_scope_info, int closure_scope_index)
    : isolate_(isolate),
      script_(script),
      context_(context),
      debug_scope_info_(debug_scope_info),
      scope_index_(closure_scope_index),
      closure_scope_index_(closure_scope_index) {}

void LocalBlocklistsCollector::InitializeWithClosureScope() {
  CHECK(scope().is_declaration_scope());
  function_blocklists_.emplace(scope_index_, StringSet::New(isolate_));
  context_blocklist_ = StringSet::New(isolate_);
  has_matched_first_context_ = scope().needs_context();
}

void LocalBlocklistsCollector::AdvanceToNextNonHiddenScope() {
  std::optional<DebugScriptScope> outer_scope = scope().parent();
  DCHECK(outer_scope.has_value());
  while (outer_scope.has_value() && outer_scope->is_hidden()) {
    outer_scope = outer_scope->parent();
  }
  CHECK(outer_scope.has_value());
  scope_index_ = outer_scope->scope_index();
}

void LocalBlocklistsCollector::CollectCurrentLocalsIntoBlocklists() {
  for (int i = 0; i < scope().variable_count(); ++i) {
    DebugVariableInfo var = scope().variable(i);
    if (var.location != VariableLocation::PARAMETER &&
        var.location != VariableLocation::LOCAL) {
      continue;
    }
    // StringSet::Add() can allocate, so the name has to be handlified before
    // the first of those calls.
    DirectHandle<String> name(var.name, isolate_);
    if (!context_blocklist_.is_null()) {
      context_blocklist_ = StringSet::Add(isolate_, context_blocklist_, name);
    }
    for (auto& pair : function_blocklists_) {
      pair.second = StringSet::Add(isolate_, pair.second, name);
    }
  }
}

DirectHandle<ScopeInfo> LocalBlocklistsCollector::FindScopeInfoForScope(
    int scope_index) const {
  DisallowGarbageCollection no_gc;
  DebugScriptScope scope =
      DebugScriptScope::FromIndex(debug_scope_info_, scope_index);
  const int start_position = scope.start_position();
  const int end_position = scope.end_position();
  const ScopeType scope_type = scope.scope_type();
  SharedFunctionInfo::ScriptIterator iterator(isolate_, *script_);
  for (Tagged<SharedFunctionInfo> info = iterator.Next(); !info.is_null();
       info = iterator.Next()) {
    Tagged<ScopeInfo> scope_info = info->scope_info();
    if (info->is_compiled() && !scope_info.is_null() &&
        start_position == info->StartPosition() &&
        end_position == info->EndPosition() &&
        scope_type == scope_info->scope_type()) {
      return direct_handle(scope_info, isolate_);
    }
  }
  return DirectHandle<ScopeInfo>();
}

void LocalBlocklistsCollector::StoreFunctionBlocklists(
    DirectHandle<ScopeInfo> outer_scope_info) {
  for (const auto& pair : function_blocklists_) {
    DirectHandle<ScopeInfo> scope_info = FindScopeInfoForScope(pair.first);
    // If we don't find a ScopeInfo it's not tragic. It means we'll do
    // a full-reparse in case we pause in that function in the future.
    // The only ScopeInfo that MUST be found is for the closure scope.
    CHECK_IMPLIES(pair.first == closure_scope_index_, !scope_info.is_null());
    if (scope_info.is_null()) continue;
    isolate_->LocalsBlockListCacheSet(scope_info, outer_scope_info,
                                      pair.second);
  }
}

void LocalBlocklistsCollector::CollectAndStore() {
  InitializeWithClosureScope();

  while (scope().parent().has_value() && !IsNativeContext(*context_)) {
    AdvanceToNextNonHiddenScope();

    if (!has_matched_first_context_ && scope().needs_context()) {
      context_blocklist_ = StringSet::New(isolate_);
    }

    // 1. Add all stack-allocated variables of the current scope to the various
    //    lists.
    CollectCurrentLocalsIntoBlocklists();

    // 2. If the current scope requires a context then all the blocklists "stop"
    //    here and we store them.  Next, advance the current context so
    //    `context_` and the current scope match again.
    if (scope().needs_context()) {
      if (has_matched_first_context_) {
        // Only store the block list and advance the context if we have already
        // matched our first context. This handles the case when we start on
        // a closure scope that doesn't require a context. In that case
        // `context_` is already the right context for the current scope so we
        // don't need to advance `context_`.
        isolate_->LocalsBlockListCacheSet(
            direct_handle(context_->scope_info(), isolate_),
            direct_handle(context_->previous()->scope_info(), isolate_),
            context_blocklist_);
        context_ = handle(context_->previous(), isolate_);
        context_blocklist_ = StringSet::New(isolate_);
      }
      has_matched_first_context_ = true;

      StoreFunctionBlocklists(direct_handle(context_->scope_info(), isolate_));
      function_blocklists_.clear();
    } else if (scope().is_function_scope()) {
      // 3. If `scope` is a function scope with an SFI, start recording
      //    locals for its ScopeInfo.
      CHECK(!scope().needs_context());
      function_blocklists_.emplace(scope_index_, StringSet::New(isolate_));
    }
  }

  if (has_matched_first_context_ && !IsNativeContext(*context_)) {
    isolate_->LocalsBlockListCacheSet(
        direct_handle(context_->scope_info(), isolate_),
        direct_handle(context_->previous()->scope_info(), isolate_),
        context_blocklist_);
  }

  // In case we don't have any outer scopes we still need to record the empty
  // block list for the paused function to prevent future re-parses.
  StoreFunctionBlocklists(direct_handle(context_->scope_info(), isolate_));
}

}  // namespace

void ScopeIterator::MaybeCollectAndStoreLocalBlocklists() const {
  if (!calculate_blocklists_ || current_scope_index_ != closure_scope_index_ ||
      Type() == ScopeTypeScript) {
    return;
  }

  DCHECK(IsTheHole(isolate_->LocalsBlockListCacheGet(
      direct_handle(function_->shared()->scope_info(), isolate_))));

  LocalBlocklistsCollector collector(isolate_, script_, context_,
                                     debug_scope_info_, closure_scope_index_);
  collector.CollectAndStore();
}

}  // namespace internal
}  // namespace v8
