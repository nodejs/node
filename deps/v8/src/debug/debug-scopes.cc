// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scopes.h"

#include <memory>

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/isolate-inl.h"
#include "src/objects/js-generator-inl.h"
#include "src/objects/module.h"
#include "src/ostreams.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parsing.h"
#include "src/parsing/rewriter.h"

namespace v8 {
namespace internal {

ScopeIterator::ScopeIterator(Isolate* isolate, FrameInspector* frame_inspector,
                             ScopeIterator::Option option)
    : isolate_(isolate),
      frame_inspector_(frame_inspector),
      function_(frame_inspector_->GetFunction()),
      script_(frame_inspector_->GetScript()) {
  if (!frame_inspector->GetContext()->IsContext()) {
    // Optimized frame, context or function cannot be materialized. Give up.
    return;
  }
  context_ = Handle<Context>::cast(frame_inspector->GetContext());

  // We should not instantiate a ScopeIterator for wasm frames.
  DCHECK_NE(Script::TYPE_WASM, frame_inspector->GetScript()->type());

  TryParseAndRetrieveScopes(option);
}

ScopeIterator::~ScopeIterator() { delete info_; }

Handle<Object> ScopeIterator::GetFunctionDebugName() const {
  if (!function_.is_null()) return JSFunction::GetDebugName(function_);

  if (!context_->IsNativeContext()) {
    DisallowHeapAllocation no_gc;
    ScopeInfo closure_info = context_->closure_context()->scope_info();
    Handle<String> debug_name(closure_info->FunctionDebugName(), isolate_);
    if (debug_name->length() > 0) return debug_name;
  }
  return isolate_->factory()->undefined_value();
}

ScopeIterator::ScopeIterator(Isolate* isolate, Handle<JSFunction> function)
    : isolate_(isolate), context_(function->context(), isolate) {
  if (!function->shared()->IsSubjectToDebugging()) {
    context_ = Handle<Context>();
    return;
  }
  script_ = handle(Script::cast(function->shared()->script()), isolate);
  UnwrapEvaluationContext();
}

ScopeIterator::ScopeIterator(Isolate* isolate,
                             Handle<JSGeneratorObject> generator)
    : isolate_(isolate),
      generator_(generator),
      function_(generator->function(), isolate),
      context_(generator->context(), isolate),
      script_(Script::cast(function_->shared()->script()), isolate) {
  CHECK(function_->shared()->IsSubjectToDebugging());
  TryParseAndRetrieveScopes(DEFAULT);
}

void ScopeIterator::Restart() {
  DCHECK_NOT_NULL(frame_inspector_);
  function_ = frame_inspector_->GetFunction();
  context_ = Handle<Context>::cast(frame_inspector_->GetContext());
  current_scope_ = start_scope_;
  DCHECK_NOT_NULL(current_scope_);
  UnwrapEvaluationContext();
}

void ScopeIterator::TryParseAndRetrieveScopes(ScopeIterator::Option option) {
  // Catch the case when the debugger stops in an internal function.
  Handle<SharedFunctionInfo> shared_info(function_->shared(), isolate_);
  Handle<ScopeInfo> scope_info(shared_info->scope_info(), isolate_);
  if (shared_info->script()->IsUndefined(isolate_)) {
    current_scope_ = closure_scope_ = nullptr;
    context_ = handle(function_->context(), isolate_);
    function_ = Handle<JSFunction>();
    return;
  }

  // Class fields initializer functions don't have any scope
  // information. We short circuit the parsing of the class literal
  // and return an empty context here.
  if (IsClassMembersInitializerFunction(shared_info->kind())) {
    current_scope_ = closure_scope_ = nullptr;
    context_ = Handle<Context>();
    function_ = Handle<JSFunction>();
    return;
  }

  DCHECK_NE(IGNORE_NESTED_SCOPES, option);
  bool ignore_nested_scopes = false;
  if (shared_info->HasBreakInfo() && frame_inspector_ != nullptr) {
    // The source position at return is always the end of the function,
    // which is not consistent with the current scope chain. Therefore all
    // nested with, catch and block contexts are skipped, and we can only
    // inspect the function scope.
    // This can only happen if we set a break point inside right before the
    // return, which requires a debug info to be available.
    Handle<DebugInfo> debug_info(shared_info->GetDebugInfo(), isolate_);

    // Find the break point where execution has stopped.
    BreakLocation location = BreakLocation::FromFrame(debug_info, GetFrame());

    ignore_nested_scopes = location.IsReturn();
  }

  // Reparse the code and analyze the scopes.
  // Check whether we are in global, eval or function code.
  if (scope_info->scope_type() == FUNCTION_SCOPE) {
    // Inner function.
    info_ = new ParseInfo(isolate_, shared_info);
  } else {
    // Global or eval code.
    Handle<Script> script(Script::cast(shared_info->script()), isolate_);
    info_ = new ParseInfo(isolate_, script);
    if (scope_info->scope_type() == EVAL_SCOPE) {
      info_->set_eval();
      if (!context_->IsNativeContext()) {
        info_->set_outer_scope_info(handle(context_->scope_info(), isolate_));
      }
      // Language mode may be inherited from the eval caller.
      // Retrieve it from shared function info.
      info_->set_language_mode(shared_info->language_mode());
    } else if (scope_info->scope_type() == MODULE_SCOPE) {
      DCHECK(info_->is_module());
    } else {
      DCHECK_EQ(SCRIPT_SCOPE, scope_info->scope_type());
    }
  }

  if (parsing::ParseAny(info_, shared_info, isolate_) &&
      Rewriter::Rewrite(info_)) {
    info_->ast_value_factory()->Internalize(isolate_);
    closure_scope_ = info_->literal()->scope();

    if (option == COLLECT_NON_LOCALS) {
      DCHECK(non_locals_.is_null());
      non_locals_ = info_->literal()->scope()->CollectNonLocals(
          isolate_, info_, StringSet::New(isolate_));
      if (!closure_scope_->has_this_declaration() &&
          closure_scope_->HasThisReference()) {
        non_locals_ = StringSet::Add(isolate_, non_locals_,
                                     isolate_->factory()->this_string());
      }
    }

    CHECK(DeclarationScope::Analyze(info_));
    if (ignore_nested_scopes) {
      current_scope_ = closure_scope_;
      start_scope_ = current_scope_;
      if (closure_scope_->NeedsContext()) {
        context_ = handle(context_->closure_context(), isolate_);
      }
    } else {
      RetrieveScopeChain(closure_scope_);
    }
    UnwrapEvaluationContext();
  } else {
    // A failed reparse indicates that the preparser has diverged from the
    // parser or that the preparse data given to the initial parse has been
    // faulty. We fail in debug mode but in release mode we only provide the
    // information we get from the context chain but nothing about
    // completely stack allocated scopes or stack allocated locals.
    // Or it could be due to stack overflow.
    // Silently fail by presenting an empty context chain.
    CHECK(isolate_->has_pending_exception());
    isolate_->clear_pending_exception();
    context_ = Handle<Context>();
  }
}

void ScopeIterator::UnwrapEvaluationContext() {
  if (!context_->IsDebugEvaluateContext()) return;
  Context current = *context_;
  do {
    Object wrapped = current->get(Context::WRAPPED_CONTEXT_INDEX);
    if (wrapped->IsContext()) {
      current = Context::cast(wrapped);
    } else {
      DCHECK(!current->previous().is_null());
      current = current->previous();
    }
  } while (current->IsDebugEvaluateContext());
  context_ = handle(current, isolate_);
}

Handle<JSObject> ScopeIterator::MaterializeScopeDetails() {
  // Calculate the size of the result.
  Handle<FixedArray> details =
      isolate_->factory()->NewFixedArray(kScopeDetailsSize);
  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(Type()));
  Handle<JSObject> scope_object = ScopeObject(Mode::ALL);
  details->set(kScopeDetailsObjectIndex, *scope_object);
  if (Type() == ScopeTypeGlobal || Type() == ScopeTypeScript) {
    return isolate_->factory()->NewJSArrayWithElements(details);
  } else if (HasContext()) {
    Handle<Object> closure_name = GetFunctionDebugName();
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
  return InInnerScope() || !context_->IsNativeContext();
}

int ScopeIterator::start_position() {
  if (InInnerScope()) return current_scope_->start_position();
  if (context_->IsNativeContext()) return 0;
  return context_->closure_context()->scope_info()->StartPosition();
}

int ScopeIterator::end_position() {
  if (InInnerScope()) return current_scope_->end_position();
  if (context_->IsNativeContext()) return 0;
  return context_->closure_context()->scope_info()->EndPosition();
}

bool ScopeIterator::DeclaresLocals(Mode mode) const {
  ScopeType type = Type();

  if (type == ScopeTypeWith) return mode == Mode::ALL;
  if (type == ScopeTypeGlobal) return mode == Mode::ALL;

  bool declares_local = false;
  auto visitor = [&](Handle<String> name, Handle<Object> value) {
    declares_local = true;
    return true;
  };
  VisitScope(visitor, mode);
  return declares_local;
}

bool ScopeIterator::HasContext() const {
  return !InInnerScope() || current_scope_->NeedsContext();
}

void ScopeIterator::Next() {
  DCHECK(!Done());

  ScopeType scope_type = Type();

  if (scope_type == ScopeTypeGlobal) {
    // The global scope is always the last in the chain.
    DCHECK(context_->IsNativeContext());
    context_ = Handle<Context>();
    DCHECK(Done());
    return;
  }

  bool inner = InInnerScope();
  if (current_scope_ == closure_scope_) function_ = Handle<JSFunction>();

  if (scope_type == ScopeTypeScript) {
    DCHECK_IMPLIES(InInnerScope(), current_scope_->is_script_scope());
    seen_script_scope_ = true;
    if (context_->IsScriptContext()) {
      context_ = handle(context_->previous(), isolate_);
    }
  } else if (!inner) {
    DCHECK(!context_->IsNativeContext());
    context_ = handle(context_->previous(), isolate_);
  } else {
    DCHECK_NOT_NULL(current_scope_);
    do {
      if (current_scope_->NeedsContext()) {
        DCHECK(!context_->previous().is_null());
        context_ = handle(context_->previous(), isolate_);
      }
      DCHECK_IMPLIES(InInnerScope(), current_scope_->outer_scope() != nullptr);
      current_scope_ = current_scope_->outer_scope();
      // Repeat to skip hidden scopes.
    } while (current_scope_->is_hidden());
  }

  UnwrapEvaluationContext();
}


// Return the type of the current scope.
ScopeIterator::ScopeType ScopeIterator::Type() const {
  DCHECK(!Done());
  if (InInnerScope()) {
    switch (current_scope_->scope_type()) {
      case FUNCTION_SCOPE:
        DCHECK_IMPLIES(current_scope_->NeedsContext(),
                       context_->IsFunctionContext() ||
                           context_->IsDebugEvaluateContext());
        return ScopeTypeLocal;
      case MODULE_SCOPE:
        DCHECK_IMPLIES(current_scope_->NeedsContext(),
                       context_->IsModuleContext());
        return ScopeTypeModule;
      case SCRIPT_SCOPE:
        DCHECK_IMPLIES(
            current_scope_->NeedsContext(),
            context_->IsScriptContext() || context_->IsNativeContext());
        return ScopeTypeScript;
      case WITH_SCOPE:
        DCHECK_IMPLIES(current_scope_->NeedsContext(),
                       context_->IsWithContext());
        return ScopeTypeWith;
      case CATCH_SCOPE:
        DCHECK(context_->IsCatchContext());
        return ScopeTypeCatch;
      case BLOCK_SCOPE:
        DCHECK_IMPLIES(current_scope_->NeedsContext(),
                       context_->IsBlockContext());
        return ScopeTypeBlock;
      case EVAL_SCOPE:
        DCHECK_IMPLIES(current_scope_->NeedsContext(),
                       context_->IsEvalContext());
        return ScopeTypeEval;
    }
    UNREACHABLE();
  }
  if (context_->IsNativeContext()) {
    DCHECK(context_->global_object()->IsJSGlobalObject());
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

  Handle<JSObject> scope = isolate_->factory()->NewJSObjectWithNullProto();
  auto visitor = [=](Handle<String> name, Handle<Object> value) {
    JSObject::AddProperty(isolate_, scope, name, value, NONE);
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
      return VisitLocalScope(visitor, mode);
    case ScopeTypeModule:
      if (InInnerScope()) {
        return VisitLocalScope(visitor, mode);
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
                                     Handle<Object> value) {
  DCHECK(!Done());
  name = isolate_->factory()->InternalizeString(name);
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
        if (!current_scope_->NeedsContext()) return false;
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

Handle<StringSet> ScopeIterator::GetNonLocals() { return non_locals_; }

#ifdef DEBUG
// Debug print of the content of the current scope.
void ScopeIterator::DebugPrint() {
  StdoutStream os;
  DCHECK(!Done());
  switch (Type()) {
    case ScopeIterator::ScopeTypeGlobal:
      os << "Global:\n";
      context_->Print(os);
      break;

    case ScopeIterator::ScopeTypeLocal: {
      os << "Local:\n";
      if (current_scope_->NeedsContext()) {
        context_->Print(os);
        if (context_->has_extension()) {
          Handle<HeapObject> extension(context_->extension(), isolate_);
          DCHECK(extension->IsJSContextExtensionObject());
          extension->Print(os);
        }
      }
      break;
    }

    case ScopeIterator::ScopeTypeWith:
      os << "With:\n";
      context_->extension()->Print(os);
      break;

    case ScopeIterator::ScopeTypeCatch:
      os << "Catch:\n";
      context_->extension()->Print(os);
      context_->get(Context::THROWN_OBJECT_INDEX)->Print(os);
      break;

    case ScopeIterator::ScopeTypeClosure:
      os << "Closure:\n";
      context_->Print(os);
      if (context_->has_extension()) {
        Handle<HeapObject> extension(context_->extension(), isolate_);
        DCHECK(extension->IsJSContextExtensionObject());
        extension->Print(os);
      }
      break;

    case ScopeIterator::ScopeTypeScript:
      os << "Script:\n";
      context_->global_object()
          ->native_context()
          ->script_context_table()
          ->Print(os);
      break;

    default:
      UNREACHABLE();
  }
  PrintF("\n");
}
#endif

int ScopeIterator::GetSourcePosition() {
  if (frame_inspector_) {
    return frame_inspector_->GetSourcePosition();
  } else {
    DCHECK(!generator_.is_null());
    SharedFunctionInfo::EnsureSourcePositionsAvailable(
        isolate_, handle(generator_->function()->shared(), isolate_));
    return generator_->source_position();
  }
}

void ScopeIterator::RetrieveScopeChain(DeclarationScope* scope) {
  DCHECK_NOT_NULL(scope);

  const int position = GetSourcePosition();

  Scope* parent = nullptr;
  Scope* current = scope;
  while (parent != current) {
    parent = current;
    for (Scope* inner_scope = current->inner_scope(); inner_scope != nullptr;
         inner_scope = inner_scope->sibling()) {
      int beg_pos = inner_scope->start_position();
      int end_pos = inner_scope->end_position();
      DCHECK((beg_pos >= 0 && end_pos >= 0) || inner_scope->is_hidden());
      if (beg_pos <= position && position < end_pos) {
        // Don't walk into inner functions.
        if (!inner_scope->is_function_scope()) {
          current = inner_scope;
        }
        break;
      }
    }
  }

  start_scope_ = current;
  current_scope_ = current;
}

void ScopeIterator::VisitScriptScope(const Visitor& visitor) const {
  Handle<JSGlobalObject> global(context_->global_object(), isolate_);
  Handle<ScriptContextTable> script_contexts(
      global->native_context()->script_context_table(), isolate_);

  // Skip the first script since that just declares 'this'.
  for (int context_index = 1; context_index < script_contexts->used();
       context_index++) {
    Handle<Context> context = ScriptContextTable::GetContext(
        isolate_, script_contexts, context_index);
    Handle<ScopeInfo> scope_info(context->scope_info(), isolate_);
    if (VisitContextLocals(visitor, scope_info, context)) return;
  }
}

void ScopeIterator::VisitModuleScope(const Visitor& visitor) const {
  DCHECK(context_->IsModuleContext());

  Handle<ScopeInfo> scope_info(context_->scope_info(), isolate_);
  if (VisitContextLocals(visitor, scope_info, context_)) return;

  int count_index = scope_info->ModuleVariableCountIndex();
  int module_variable_count = Smi::cast(scope_info->get(count_index))->value();

  Handle<Module> module(context_->module(), isolate_);

  for (int i = 0; i < module_variable_count; ++i) {
    int index;
    Handle<String> name;
    {
      String raw_name;
      scope_info->ModuleVariable(i, &raw_name, &index);
      if (ScopeInfo::VariableIsSynthetic(raw_name)) continue;
      name = handle(raw_name, isolate_);
    }
    Handle<Object> value = Module::LoadVariable(isolate_, module, index);

    // Reflect variables under TDZ as undeclared in scope object.
    if (value->IsTheHole(isolate_)) continue;
    if (visitor(name, value)) return;
  }
}

bool ScopeIterator::VisitContextLocals(const Visitor& visitor,
                                       Handle<ScopeInfo> scope_info,
                                       Handle<Context> context) const {
  // Fill all context locals to the context extension.
  for (int i = 0; i < scope_info->ContextLocalCount(); ++i) {
    Handle<String> name(scope_info->ContextLocalName(i), isolate_);
    if (ScopeInfo::VariableIsSynthetic(*name)) continue;
    int context_index = Context::MIN_CONTEXT_SLOTS + i;
    Handle<Object> value(context->get(context_index), isolate_);
    // Reflect variables under TDZ as undefined in scope object.
    if (value->IsTheHole(isolate_)) continue;
    if (visitor(name, value)) return true;
  }
  return false;
}

bool ScopeIterator::VisitLocals(const Visitor& visitor, Mode mode) const {
  if (mode == Mode::STACK && current_scope_->is_declaration_scope() &&
      current_scope_->AsDeclarationScope()->has_this_declaration()) {
    Handle<Object> receiver = frame_inspector_ == nullptr
                                  ? handle(generator_->receiver(), isolate_)
                                  : frame_inspector_->GetReceiver();
    if (receiver->IsOptimizedOut(isolate_) || receiver->IsTheHole(isolate_)) {
      receiver = isolate_->factory()->undefined_value();
    }
    if (visitor(isolate_->factory()->this_string(), receiver)) return true;
  }

  for (Variable* var : *current_scope_->locals()) {
    DCHECK(!var->is_this());
    if (ScopeInfo::VariableIsSynthetic(*var->name())) continue;

    int index = var->index();
    Handle<Object> value;
    switch (var->location()) {
      case VariableLocation::LOOKUP:
        UNREACHABLE();
        break;

      case VariableLocation::UNALLOCATED:
        continue;

      case VariableLocation::PARAMETER: {
        if (frame_inspector_ == nullptr) {
          // Get the variable from the suspended generator.
          DCHECK(!generator_.is_null());
          FixedArray parameters_and_registers =
              generator_->parameters_and_registers();
          DCHECK_LT(index, parameters_and_registers->length());
          value = handle(parameters_and_registers->get(index), isolate_);
        } else {
          value = frame_inspector_->GetParameter(index);

          if (value->IsOptimizedOut(isolate_)) {
            value = isolate_->factory()->undefined_value();
          }
        }
        break;
      }

      case VariableLocation::LOCAL:
        if (frame_inspector_ == nullptr) {
          // Get the variable from the suspended generator.
          DCHECK(!generator_.is_null());
          FixedArray parameters_and_registers =
              generator_->parameters_and_registers();
          int parameter_count =
              function_->shared()->scope_info()->ParameterCount();
          index += parameter_count;
          DCHECK_LT(index, parameters_and_registers->length());
          value = handle(parameters_and_registers->get(index), isolate_);
          if (value->IsTheHole(isolate_)) {
            value = isolate_->factory()->undefined_value();
          }
        } else {
          value = frame_inspector_->GetExpression(index);
          if (value->IsOptimizedOut(isolate_)) {
            // We'll rematerialize this later.
            if (current_scope_->is_declaration_scope() &&
                current_scope_->AsDeclarationScope()->arguments() == var) {
              continue;
            }
            value = isolate_->factory()->undefined_value();
          } else if (value->IsTheHole(isolate_)) {
            // Reflect variables under TDZ as undeclared in scope object.
            continue;
          }
        }
        break;

      case VariableLocation::CONTEXT:
        if (mode == Mode::STACK) continue;
        DCHECK(var->IsContextSlot());
        value = handle(context_->get(index), isolate_);
        // Reflect variables under TDZ as undeclared in scope object.
        if (value->IsTheHole(isolate_)) continue;
        break;

      case VariableLocation::MODULE: {
        if (mode == Mode::STACK) continue;
        // if (var->IsExport()) continue;
        Handle<Module> module(context_->module(), isolate_);
        value = Module::LoadVariable(isolate_, module, var->index());
        // Reflect variables under TDZ as undeclared in scope object.
        if (value->IsTheHole(isolate_)) continue;
        break;
      }
    }

    if (visitor(var->name(), value)) return true;
  }
  return false;
}

// Retrieve the with-context extension object. If the extension object is
// a proxy, return an empty object.
Handle<JSObject> ScopeIterator::WithContextExtension() {
  DCHECK(context_->IsWithContext());
  if (context_->extension_receiver()->IsJSProxy()) {
    return isolate_->factory()->NewJSObjectWithNullProto();
  }
  return handle(JSObject::cast(context_->extension_receiver()), isolate_);
}

// Create a plain JSObject which materializes the block scope for the specified
// block context.
void ScopeIterator::VisitLocalScope(const Visitor& visitor, Mode mode) const {
  if (InInnerScope()) {
    if (VisitLocals(visitor, mode)) return;
    if (mode == Mode::STACK && Type() == ScopeTypeLocal) {
      // Hide |this| in arrow functions that may be embedded in other functions
      // but don't force |this| to be context-allocated. Otherwise we'd find the
      // wrong |this| value.
      if (!closure_scope_->has_this_declaration() &&
          !closure_scope_->HasThisReference()) {
        if (visitor(isolate_->factory()->this_string(),
                    isolate_->factory()->undefined_value()))
          return;
      }
      // Add |arguments| to the function scope even if it wasn't used.
      // Currently we don't yet support materializing the arguments object of
      // suspended generators. We'd need to read the arguments out from the
      // suspended generator rather than from an activation as
      // FunctionGetArguments does.
      if (frame_inspector_ != nullptr && !closure_scope_->is_arrow_scope() &&
          (closure_scope_->arguments() == nullptr ||
           frame_inspector_->GetExpression(closure_scope_->arguments()->index())
               ->IsOptimizedOut(isolate_))) {
        JavaScriptFrame* frame = GetFrame();
        Handle<JSObject> arguments = Accessors::FunctionGetArguments(
            frame, frame_inspector_->inlined_frame_index());
        if (visitor(isolate_->factory()->arguments_string(), arguments)) return;
      }
    }
  } else {
    DCHECK_EQ(Mode::ALL, mode);
    Handle<ScopeInfo> scope_info(context_->scope_info(), isolate_);
    if (VisitContextLocals(visitor, scope_info, context_)) return;
  }

  if (mode == Mode::ALL && HasContext()) {
    DCHECK(!context_->IsScriptContext());
    DCHECK(!context_->IsNativeContext());
    DCHECK(!context_->IsWithContext());
    if (!context_->scope_info()->CallsSloppyEval()) return;
    if (context_->extension_object().is_null()) return;
    Handle<JSObject> extension(context_->extension_object(), isolate_);
    Handle<FixedArray> keys =
        KeyAccumulator::GetKeys(extension, KeyCollectionMode::kOwnOnly,
                                ENUMERABLE_STRINGS)
            .ToHandleChecked();

    for (int i = 0; i < keys->length(); i++) {
      // Names of variables introduced by eval are strings.
      DCHECK(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)), isolate_);
      Handle<Object> value = JSReceiver::GetDataProperty(extension, key);
      if (visitor(key, value)) return;
    }
  }
}

bool ScopeIterator::SetLocalVariableValue(Handle<String> variable_name,
                                          Handle<Object> new_value) {
  // TODO(verwaest): Walk parameters backwards, not forwards.
  // TODO(verwaest): Use VariableMap rather than locals() list for lookup.
  for (Variable* var : *current_scope_->locals()) {
    if (String::Equals(isolate_, var->name(), variable_name)) {
      int index = var->index();
      switch (var->location()) {
        case VariableLocation::LOOKUP:
        case VariableLocation::UNALLOCATED:
          // Drop assignments to unallocated locals.
          DCHECK(var->is_this() ||
                 *variable_name == ReadOnlyRoots(isolate_).arguments_string());
          return false;

        case VariableLocation::PARAMETER: {
          if (var->is_this()) return false;
          if (frame_inspector_ == nullptr) {
            // Set the variable in the suspended generator.
            DCHECK(!generator_.is_null());
            Handle<FixedArray> parameters_and_registers(
                generator_->parameters_and_registers(), isolate_);
            DCHECK_LT(index, parameters_and_registers->length());
            parameters_and_registers->set(index, *new_value);
          } else {
            JavaScriptFrame* frame = GetFrame();
            if (frame->is_optimized()) return false;

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
            Handle<FixedArray> parameters_and_registers(
                generator_->parameters_and_registers(), isolate_);
            DCHECK_LT(index, parameters_and_registers->length());
            parameters_and_registers->set(index, *new_value);
          } else {
            // Set the variable on the stack.
            JavaScriptFrame* frame = GetFrame();
            if (frame->is_optimized()) return false;

            frame->SetExpression(index, *new_value);
          }
          return true;

        case VariableLocation::CONTEXT:
          DCHECK(var->IsContextSlot());
          context_->set(index, *new_value);
          return true;

        case VariableLocation::MODULE:
          if (!var->IsExport()) return false;
          Handle<Module> module(context_->module(), isolate_);
          Module::StoreVariable(module, var->index(), new_value);
          return true;
      }
      UNREACHABLE();
    }
  }

  return false;
}

bool ScopeIterator::SetContextExtensionValue(Handle<String> variable_name,
                                             Handle<Object> new_value) {
  if (!context_->has_extension()) return false;

  DCHECK(context_->extension_object()->IsJSContextExtensionObject());
  Handle<JSObject> ext(context_->extension_object(), isolate_);
  LookupIterator it(isolate_, ext, variable_name, LookupIterator::OWN);
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(ext, variable_name);
  DCHECK(maybe.IsJust());
  if (!maybe.FromJust()) return false;

  CHECK(Object::SetDataProperty(&it, new_value).ToChecked());
  return true;
}

bool ScopeIterator::SetContextVariableValue(Handle<String> variable_name,
                                            Handle<Object> new_value) {
  DisallowHeapAllocation no_gc;
  VariableMode mode;
  InitializationFlag flag;
  MaybeAssignedFlag maybe_assigned_flag;
  int slot_index =
      ScopeInfo::ContextSlotIndex(context_->scope_info(), *variable_name, &mode,
                                  &flag, &maybe_assigned_flag);
  if (slot_index < 0) return false;

  context_->set(slot_index, *new_value);
  return true;
}

bool ScopeIterator::SetModuleVariableValue(Handle<String> variable_name,
                                           Handle<Object> new_value) {
  DisallowHeapAllocation no_gc;
  int cell_index;
  VariableMode mode;
  InitializationFlag init_flag;
  MaybeAssignedFlag maybe_assigned_flag;
  cell_index = context_->scope_info()->ModuleIndex(
      *variable_name, &mode, &init_flag, &maybe_assigned_flag);

  // Setting imports is currently not supported.
  if (ModuleDescriptor::GetCellIndexKind(cell_index) !=
      ModuleDescriptor::kExport) {
    return false;
  }

  Handle<Module> module(context_->module(), isolate_);
  Module::StoreVariable(module, cell_index, new_value);
  return true;
}

bool ScopeIterator::SetScriptVariableValue(Handle<String> variable_name,
                                           Handle<Object> new_value) {
  Handle<ScriptContextTable> script_contexts(
      context_->global_object()->native_context()->script_context_table(),
      isolate_);
  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(isolate_, *script_contexts, *variable_name,
                                 &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        isolate_, script_contexts, lookup_result.context_index);
    script_context->set(lookup_result.slot_index, *new_value);
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace v8
