// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-scopes.h"

#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/globals.h"
#include "src/parser.h"
#include "src/scopes.h"

namespace v8 {
namespace internal {

ScopeIterator::ScopeIterator(Isolate* isolate, FrameInspector* frame_inspector,
                             bool ignore_nested_scopes)
    : isolate_(isolate),
      frame_inspector_(frame_inspector),
      nested_scope_chain_(4),
      seen_script_scope_(false),
      failed_(false) {
  if (!frame_inspector->GetContext()->IsContext() ||
      !frame_inspector->GetFunction()->IsJSFunction()) {
    // Optimized frame, context or function cannot be materialized. Give up.
    return;
  }

  context_ = Handle<Context>(Context::cast(frame_inspector->GetContext()));

  // Catch the case when the debugger stops in an internal function.
  Handle<JSFunction> function = GetFunction();
  Handle<SharedFunctionInfo> shared_info(function->shared());
  Handle<ScopeInfo> scope_info(shared_info->scope_info());
  if (shared_info->script() == isolate->heap()->undefined_value()) {
    while (context_->closure() == *function) {
      context_ = Handle<Context>(context_->previous(), isolate_);
    }
    return;
  }

  // Currently it takes too much time to find nested scopes due to script
  // parsing. Sometimes we want to run the ScopeIterator as fast as possible
  // (for example, while collecting async call stacks on every
  // addEventListener call), even if we drop some nested scopes.
  // Later we may optimize getting the nested scopes (cache the result?)
  // and include nested scopes into the "fast" iteration case as well.

  if (!ignore_nested_scopes && shared_info->HasDebugInfo()) {
    // The source position at return is always the end of the function,
    // which is not consistent with the current scope chain. Therefore all
    // nested with, catch and block contexts are skipped, and we can only
    // inspect the function scope.
    // This can only happen if we set a break point inside right before the
    // return, which requires a debug info to be available.
    Handle<DebugInfo> debug_info(shared_info->GetDebugInfo());

    // PC points to the instruction after the current one, possibly a break
    // location as well. So the "- 1" to exclude it from the search.
    Address call_pc = GetFrame()->pc() - 1;

    // Find the break point where execution has stopped.
    BreakLocation location =
        BreakLocation::FromAddress(debug_info, ALL_BREAK_LOCATIONS, call_pc);

    ignore_nested_scopes = location.IsReturn();
  }

  if (ignore_nested_scopes) {
    if (scope_info->HasContext()) {
      context_ = Handle<Context>(context_->declaration_context(), isolate_);
    } else {
      while (context_->closure() == *function) {
        context_ = Handle<Context>(context_->previous(), isolate_);
      }
    }
    if (scope_info->scope_type() == FUNCTION_SCOPE ||
        scope_info->scope_type() == ARROW_SCOPE) {
      nested_scope_chain_.Add(scope_info);
    }
  } else {
    // Reparse the code and analyze the scopes.
    Handle<Script> script(Script::cast(shared_info->script()));
    Scope* scope = NULL;

    // Check whether we are in global, eval or function code.
    Zone zone;
    if (scope_info->scope_type() != FUNCTION_SCOPE &&
        scope_info->scope_type() != ARROW_SCOPE) {
      // Global or eval code.
      ParseInfo info(&zone, script);
      if (scope_info->scope_type() == SCRIPT_SCOPE) {
        info.set_global();
      } else {
        DCHECK(scope_info->scope_type() == EVAL_SCOPE);
        info.set_eval();
        info.set_context(Handle<Context>(function->context()));
      }
      if (Parser::ParseStatic(&info) && Scope::Analyze(&info)) {
        scope = info.literal()->scope();
      }
      RetrieveScopeChain(scope, shared_info);
    } else {
      // Function code
      ParseInfo info(&zone, function);
      if (Parser::ParseStatic(&info) && Scope::Analyze(&info)) {
        scope = info.literal()->scope();
      }
      RetrieveScopeChain(scope, shared_info);
    }
  }
}


ScopeIterator::ScopeIterator(Isolate* isolate, Handle<JSFunction> function)
    : isolate_(isolate),
      frame_inspector_(NULL),
      context_(function->context()),
      seen_script_scope_(false),
      failed_(false) {
  if (function->IsBuiltin()) context_ = Handle<Context>();
}


MUST_USE_RESULT MaybeHandle<JSObject> ScopeIterator::MaterializeScopeDetails() {
  // Calculate the size of the result.
  Handle<FixedArray> details =
      isolate_->factory()->NewFixedArray(kScopeDetailsSize);
  // Fill in scope details.
  details->set(kScopeDetailsTypeIndex, Smi::FromInt(Type()));
  Handle<JSObject> scope_object;
  ASSIGN_RETURN_ON_EXCEPTION(isolate_, scope_object, ScopeObject(), JSObject);
  details->set(kScopeDetailsObjectIndex, *scope_object);
  return isolate_->factory()->NewJSArrayWithElements(details);
}


void ScopeIterator::Next() {
  DCHECK(!failed_);
  ScopeType scope_type = Type();
  if (scope_type == ScopeTypeGlobal) {
    // The global scope is always the last in the chain.
    DCHECK(context_->IsNativeContext());
    context_ = Handle<Context>();
    return;
  }
  if (scope_type == ScopeTypeScript) {
    seen_script_scope_ = true;
    if (context_->IsScriptContext()) {
      context_ = Handle<Context>(context_->previous(), isolate_);
    }
    if (!nested_scope_chain_.is_empty()) {
      DCHECK_EQ(nested_scope_chain_.last()->scope_type(), SCRIPT_SCOPE);
      nested_scope_chain_.RemoveLast();
      DCHECK(nested_scope_chain_.is_empty());
    }
    CHECK(context_->IsNativeContext());
    return;
  }
  if (nested_scope_chain_.is_empty()) {
    context_ = Handle<Context>(context_->previous(), isolate_);
  } else {
    if (nested_scope_chain_.last()->HasContext()) {
      DCHECK(context_->previous() != NULL);
      context_ = Handle<Context>(context_->previous(), isolate_);
    }
    nested_scope_chain_.RemoveLast();
  }
}


// Return the type of the current scope.
ScopeIterator::ScopeType ScopeIterator::Type() {
  DCHECK(!failed_);
  if (!nested_scope_chain_.is_empty()) {
    Handle<ScopeInfo> scope_info = nested_scope_chain_.last();
    switch (scope_info->scope_type()) {
      case FUNCTION_SCOPE:
      case ARROW_SCOPE:
        DCHECK(context_->IsFunctionContext() || !scope_info->HasContext());
        return ScopeTypeLocal;
      case MODULE_SCOPE:
        DCHECK(context_->IsModuleContext());
        return ScopeTypeModule;
      case SCRIPT_SCOPE:
        DCHECK(context_->IsScriptContext() || context_->IsNativeContext());
        return ScopeTypeScript;
      case WITH_SCOPE:
        DCHECK(context_->IsWithContext());
        return ScopeTypeWith;
      case CATCH_SCOPE:
        DCHECK(context_->IsCatchContext());
        return ScopeTypeCatch;
      case BLOCK_SCOPE:
        DCHECK(!scope_info->HasContext() || context_->IsBlockContext());
        return ScopeTypeBlock;
      case EVAL_SCOPE:
        UNREACHABLE();
    }
  }
  if (context_->IsNativeContext()) {
    DCHECK(context_->global_object()->IsGlobalObject());
    // If we are at the native context and have not yet seen script scope,
    // fake it.
    return seen_script_scope_ ? ScopeTypeGlobal : ScopeTypeScript;
  }
  if (context_->IsFunctionContext()) {
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


MaybeHandle<JSObject> ScopeIterator::ScopeObject() {
  DCHECK(!failed_);
  switch (Type()) {
    case ScopeIterator::ScopeTypeGlobal:
      return Handle<JSObject>(CurrentContext()->global_proxy());
    case ScopeIterator::ScopeTypeScript:
      return MaterializeScriptScope();
    case ScopeIterator::ScopeTypeLocal:
      // Materialize the content of the local scope into a JSObject.
      DCHECK(nested_scope_chain_.length() == 1);
      return MaterializeLocalScope();
    case ScopeIterator::ScopeTypeWith:
      // Return the with object.
      return Handle<JSObject>(JSObject::cast(CurrentContext()->extension()));
    case ScopeIterator::ScopeTypeCatch:
      return MaterializeCatchScope();
    case ScopeIterator::ScopeTypeClosure:
      // Materialize the content of the closure scope into a JSObject.
      return MaterializeClosure();
    case ScopeIterator::ScopeTypeBlock:
      return MaterializeBlockScope();
    case ScopeIterator::ScopeTypeModule:
      return MaterializeModuleScope();
  }
  UNREACHABLE();
  return Handle<JSObject>();
}


bool ScopeIterator::HasContext() {
  ScopeType type = Type();
  if (type == ScopeTypeBlock || type == ScopeTypeLocal) {
    if (!nested_scope_chain_.is_empty()) {
      return nested_scope_chain_.last()->HasContext();
    }
  }
  return true;
}


bool ScopeIterator::SetVariableValue(Handle<String> variable_name,
                                     Handle<Object> new_value) {
  DCHECK(!failed_);
  switch (Type()) {
    case ScopeIterator::ScopeTypeGlobal:
      break;
    case ScopeIterator::ScopeTypeLocal:
      return SetLocalVariableValue(variable_name, new_value);
    case ScopeIterator::ScopeTypeWith:
      break;
    case ScopeIterator::ScopeTypeCatch:
      return SetCatchVariableValue(variable_name, new_value);
    case ScopeIterator::ScopeTypeClosure:
      return SetClosureVariableValue(variable_name, new_value);
    case ScopeIterator::ScopeTypeScript:
      return SetScriptVariableValue(variable_name, new_value);
    case ScopeIterator::ScopeTypeBlock:
      return SetBlockVariableValue(variable_name, new_value);
    case ScopeIterator::ScopeTypeModule:
      // TODO(2399): should we implement it?
      break;
  }
  return false;
}


Handle<ScopeInfo> ScopeIterator::CurrentScopeInfo() {
  DCHECK(!failed_);
  if (!nested_scope_chain_.is_empty()) {
    return nested_scope_chain_.last();
  } else if (context_->IsBlockContext()) {
    return Handle<ScopeInfo>(ScopeInfo::cast(context_->extension()));
  } else if (context_->IsFunctionContext()) {
    return Handle<ScopeInfo>(context_->closure()->shared()->scope_info());
  }
  return Handle<ScopeInfo>::null();
}


Handle<Context> ScopeIterator::CurrentContext() {
  DCHECK(!failed_);
  if (Type() == ScopeTypeGlobal || Type() == ScopeTypeScript ||
      nested_scope_chain_.is_empty()) {
    return context_;
  } else if (nested_scope_chain_.last()->HasContext()) {
    return context_;
  } else {
    return Handle<Context>();
  }
}

#ifdef DEBUG
// Debug print of the content of the current scope.
void ScopeIterator::DebugPrint() {
  OFStream os(stdout);
  DCHECK(!failed_);
  switch (Type()) {
    case ScopeIterator::ScopeTypeGlobal:
      os << "Global:\n";
      CurrentContext()->Print(os);
      break;

    case ScopeIterator::ScopeTypeLocal: {
      os << "Local:\n";
      GetFunction()->shared()->scope_info()->Print();
      if (!CurrentContext().is_null()) {
        CurrentContext()->Print(os);
        if (CurrentContext()->has_extension()) {
          Handle<Object> extension(CurrentContext()->extension(), isolate_);
          if (extension->IsJSContextExtensionObject()) {
            extension->Print(os);
          }
        }
      }
      break;
    }

    case ScopeIterator::ScopeTypeWith:
      os << "With:\n";
      CurrentContext()->extension()->Print(os);
      break;

    case ScopeIterator::ScopeTypeCatch:
      os << "Catch:\n";
      CurrentContext()->extension()->Print(os);
      CurrentContext()->get(Context::THROWN_OBJECT_INDEX)->Print(os);
      break;

    case ScopeIterator::ScopeTypeClosure:
      os << "Closure:\n";
      CurrentContext()->Print(os);
      if (CurrentContext()->has_extension()) {
        Handle<Object> extension(CurrentContext()->extension(), isolate_);
        if (extension->IsJSContextExtensionObject()) {
          extension->Print(os);
        }
      }
      break;

    case ScopeIterator::ScopeTypeScript:
      os << "Script:\n";
      CurrentContext()
          ->global_object()
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


void ScopeIterator::RetrieveScopeChain(Scope* scope,
                                       Handle<SharedFunctionInfo> shared_info) {
  if (scope != NULL) {
    int source_position = frame_inspector_->GetSourcePosition();
    scope->GetNestedScopeChain(isolate_, &nested_scope_chain_, source_position);
  } else {
    // A failed reparse indicates that the preparser has diverged from the
    // parser or that the preparse data given to the initial parse has been
    // faulty. We fail in debug mode but in release mode we only provide the
    // information we get from the context chain but nothing about
    // completely stack allocated scopes or stack allocated locals.
    // Or it could be due to stack overflow.
    DCHECK(isolate_->has_pending_exception());
    failed_ = true;
  }
}


MaybeHandle<JSObject> ScopeIterator::MaterializeScriptScope() {
  Handle<GlobalObject> global(CurrentContext()->global_object());
  Handle<ScriptContextTable> script_contexts(
      global->native_context()->script_context_table());

  Handle<JSObject> script_scope =
      isolate_->factory()->NewJSObject(isolate_->object_function());

  for (int context_index = 0; context_index < script_contexts->used();
       context_index++) {
    Handle<Context> context =
        ScriptContextTable::GetContext(script_contexts, context_index);
    Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));
    CopyContextLocalsToScopeObject(scope_info, context, script_scope);
  }
  return script_scope;
}


MaybeHandle<JSObject> ScopeIterator::MaterializeLocalScope() {
  Handle<JSFunction> function = GetFunction();

  Handle<JSObject> local_scope =
      isolate_->factory()->NewJSObject(isolate_->object_function());
  frame_inspector_->MaterializeStackLocals(local_scope, function);

  Handle<Context> frame_context(Context::cast(frame_inspector_->GetContext()));

  HandleScope scope(isolate_);
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  if (!scope_info->HasContext()) return local_scope;

  // Third fill all context locals.
  Handle<Context> function_context(frame_context->declaration_context());
  CopyContextLocalsToScopeObject(scope_info, function_context, local_scope);

  // Finally copy any properties from the function context extension.
  // These will be variables introduced by eval.
  if (function_context->closure() == *function) {
    if (function_context->has_extension() &&
        !function_context->IsNativeContext()) {
      Handle<JSObject> ext(JSObject::cast(function_context->extension()));
      Handle<FixedArray> keys;
      ASSIGN_RETURN_ON_EXCEPTION(
          isolate_, keys, JSReceiver::GetKeys(ext, JSReceiver::INCLUDE_PROTOS),
          JSObject);

      for (int i = 0; i < keys->length(); i++) {
        // Names of variables introduced by eval are strings.
        DCHECK(keys->get(i)->IsString());
        Handle<String> key(String::cast(keys->get(i)));
        Handle<Object> value;
        ASSIGN_RETURN_ON_EXCEPTION(
            isolate_, value, Object::GetPropertyOrElement(ext, key), JSObject);
        RETURN_ON_EXCEPTION(isolate_,
                            Runtime::SetObjectProperty(isolate_, local_scope,
                                                       key, value, SLOPPY),
                            JSObject);
      }
    }
  }

  return local_scope;
}


// Create a plain JSObject which materializes the closure content for the
// context.
Handle<JSObject> ScopeIterator::MaterializeClosure() {
  Handle<Context> context = CurrentContext();
  DCHECK(context->IsFunctionContext());

  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  // Allocate and initialize a JSObject with all the content of this function
  // closure.
  Handle<JSObject> closure_scope =
      isolate_->factory()->NewJSObject(isolate_->object_function());

  // Fill all context locals to the context extension.
  CopyContextLocalsToScopeObject(scope_info, context, closure_scope);

  // Finally copy any properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    DCHECK(ext->IsJSContextExtensionObject());
    Handle<FixedArray> keys =
        JSReceiver::GetKeys(ext, JSReceiver::OWN_ONLY).ToHandleChecked();

    for (int i = 0; i < keys->length(); i++) {
      HandleScope scope(isolate_);
      // Names of variables introduced by eval are strings.
      DCHECK(keys->get(i)->IsString());
      Handle<String> key(String::cast(keys->get(i)));
      Handle<Object> value = Object::GetProperty(ext, key).ToHandleChecked();
      JSObject::SetOwnPropertyIgnoreAttributes(closure_scope, key, value, NONE)
          .Check();
    }
  }

  return closure_scope;
}


// Create a plain JSObject which materializes the scope for the specified
// catch context.
Handle<JSObject> ScopeIterator::MaterializeCatchScope() {
  Handle<Context> context = CurrentContext();
  DCHECK(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  Handle<Object> thrown_object(context->get(Context::THROWN_OBJECT_INDEX),
                               isolate_);
  Handle<JSObject> catch_scope =
      isolate_->factory()->NewJSObject(isolate_->object_function());
  JSObject::SetOwnPropertyIgnoreAttributes(catch_scope, name, thrown_object,
                                           NONE)
      .Check();
  return catch_scope;
}


// Create a plain JSObject which materializes the block scope for the specified
// block context.
Handle<JSObject> ScopeIterator::MaterializeBlockScope() {
  Handle<JSObject> block_scope =
      isolate_->factory()->NewJSObject(isolate_->object_function());

  Handle<Context> context = Handle<Context>::null();
  if (!nested_scope_chain_.is_empty()) {
    Handle<ScopeInfo> scope_info = nested_scope_chain_.last();
    frame_inspector_->MaterializeStackLocals(block_scope, scope_info);
    if (scope_info->HasContext()) context = CurrentContext();
  } else {
    context = CurrentContext();
  }

  if (!context.is_null()) {
    Handle<ScopeInfo> scope_info_from_context(
        ScopeInfo::cast(context->extension()));
    // Fill all context locals.
    CopyContextLocalsToScopeObject(scope_info_from_context, context,
                                   block_scope);
  }
  return block_scope;
}


// Create a plain JSObject which materializes the module scope for the specified
// module context.
MaybeHandle<JSObject> ScopeIterator::MaterializeModuleScope() {
  Handle<Context> context = CurrentContext();
  DCHECK(context->IsModuleContext());
  Handle<ScopeInfo> scope_info(ScopeInfo::cast(context->extension()));

  // Allocate and initialize a JSObject with all the members of the debugged
  // module.
  Handle<JSObject> module_scope =
      isolate_->factory()->NewJSObject(isolate_->object_function());

  // Fill all context locals.
  CopyContextLocalsToScopeObject(scope_info, context, module_scope);

  return module_scope;
}


// Set the context local variable value.
bool ScopeIterator::SetContextLocalValue(Handle<ScopeInfo> scope_info,
                                         Handle<Context> context,
                                         Handle<String> variable_name,
                                         Handle<Object> new_value) {
  for (int i = 0; i < scope_info->ContextLocalCount(); i++) {
    Handle<String> next_name(scope_info->ContextLocalName(i));
    if (String::Equals(variable_name, next_name)) {
      VariableMode mode;
      VariableLocation location;
      InitializationFlag init_flag;
      MaybeAssignedFlag maybe_assigned_flag;
      int context_index =
          ScopeInfo::ContextSlotIndex(scope_info, next_name, &mode, &location,
                                      &init_flag, &maybe_assigned_flag);
      context->set(context_index, *new_value);
      return true;
    }
  }

  return false;
}


bool ScopeIterator::SetLocalVariableValue(Handle<String> variable_name,
                                          Handle<Object> new_value) {
  JavaScriptFrame* frame = GetFrame();
  // Optimized frames are not supported.
  if (frame->is_optimized()) return false;

  Handle<JSFunction> function(frame->function());
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());

  bool default_result = false;

  // Parameters.
  for (int i = 0; i < scope_info->ParameterCount(); ++i) {
    HandleScope scope(isolate_);
    if (String::Equals(handle(scope_info->ParameterName(i)), variable_name)) {
      frame->SetParameterValue(i, *new_value);
      // Argument might be shadowed in heap context, don't stop here.
      default_result = true;
    }
  }

  // Stack locals.
  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    HandleScope scope(isolate_);
    if (String::Equals(handle(scope_info->StackLocalName(i)), variable_name)) {
      frame->SetExpression(scope_info->StackLocalIndex(i), *new_value);
      return true;
    }
  }

  if (scope_info->HasContext()) {
    // Context locals.
    Handle<Context> frame_context(Context::cast(frame->context()));
    Handle<Context> function_context(frame_context->declaration_context());
    if (SetContextLocalValue(scope_info, function_context, variable_name,
                             new_value)) {
      return true;
    }

    // Function context extension. These are variables introduced by eval.
    if (function_context->closure() == *function) {
      if (function_context->has_extension() &&
          !function_context->IsNativeContext()) {
        Handle<JSObject> ext(JSObject::cast(function_context->extension()));

        Maybe<bool> maybe = JSReceiver::HasProperty(ext, variable_name);
        DCHECK(maybe.IsJust());
        if (maybe.FromJust()) {
          // We don't expect this to do anything except replacing
          // property value.
          Runtime::SetObjectProperty(isolate_, ext, variable_name, new_value,
                                     SLOPPY)
              .Assert();
          return true;
        }
      }
    }
  }

  return default_result;
}


bool ScopeIterator::SetBlockVariableValue(Handle<String> variable_name,
                                          Handle<Object> new_value) {
  Handle<ScopeInfo> scope_info = CurrentScopeInfo();
  JavaScriptFrame* frame = GetFrame();

  for (int i = 0; i < scope_info->StackLocalCount(); ++i) {
    HandleScope scope(isolate_);
    if (String::Equals(handle(scope_info->StackLocalName(i)), variable_name)) {
      frame->SetExpression(scope_info->StackLocalIndex(i), *new_value);
      return true;
    }
  }

  if (HasContext()) {
    return SetContextLocalValue(scope_info, CurrentContext(), variable_name,
                                new_value);
  }
  return false;
}


// This method copies structure of MaterializeClosure method above.
bool ScopeIterator::SetClosureVariableValue(Handle<String> variable_name,
                                            Handle<Object> new_value) {
  Handle<Context> context = CurrentContext();
  DCHECK(context->IsFunctionContext());

  // Context locals to the context extension.
  Handle<SharedFunctionInfo> shared(context->closure()->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  if (SetContextLocalValue(scope_info, context, variable_name, new_value)) {
    return true;
  }

  // Properties from the function context extension. This will
  // be variables introduced by eval.
  if (context->has_extension()) {
    Handle<JSObject> ext(JSObject::cast(context->extension()));
    DCHECK(ext->IsJSContextExtensionObject());
    Maybe<bool> maybe = JSReceiver::HasOwnProperty(ext, variable_name);
    DCHECK(maybe.IsJust());
    if (maybe.FromJust()) {
      // We don't expect this to do anything except replacing property value.
      JSObject::SetOwnPropertyIgnoreAttributes(ext, variable_name, new_value,
                                               NONE)
          .Check();
      return true;
    }
  }

  return false;
}


bool ScopeIterator::SetScriptVariableValue(Handle<String> variable_name,
                                           Handle<Object> new_value) {
  Handle<Context> context = CurrentContext();
  Handle<ScriptContextTable> script_contexts(
      context->global_object()->native_context()->script_context_table());
  ScriptContextTable::LookupResult lookup_result;
  if (ScriptContextTable::Lookup(script_contexts, variable_name,
                                 &lookup_result)) {
    Handle<Context> script_context = ScriptContextTable::GetContext(
        script_contexts, lookup_result.context_index);
    script_context->set(lookup_result.slot_index, *new_value);
    return true;
  }

  return false;
}


bool ScopeIterator::SetCatchVariableValue(Handle<String> variable_name,
                                          Handle<Object> new_value) {
  Handle<Context> context = CurrentContext();
  DCHECK(context->IsCatchContext());
  Handle<String> name(String::cast(context->extension()));
  if (!String::Equals(name, variable_name)) {
    return false;
  }
  context->set(Context::THROWN_OBJECT_INDEX, *new_value);
  return true;
}


void ScopeIterator::CopyContextLocalsToScopeObject(
    Handle<ScopeInfo> scope_info, Handle<Context> context,
    Handle<JSObject> scope_object) {
  Isolate* isolate = scope_info->GetIsolate();
  int local_count = scope_info->ContextLocalCount();
  if (local_count == 0) return;
  // Fill all context locals to the context extension.
  int first_context_var = scope_info->StackLocalCount();
  int start = scope_info->ContextLocalNameEntriesIndex();
  for (int i = 0; i < local_count; ++i) {
    if (scope_info->LocalIsSynthetic(first_context_var + i)) continue;
    int context_index = Context::MIN_CONTEXT_SLOTS + i;
    Handle<Object> value = Handle<Object>(context->get(context_index), isolate);
    // Reflect variables under TDZ as undefined in scope object.
    if (value->IsTheHole()) continue;
    // This should always succeed.
    // TODO(verwaest): Use AddDataProperty instead.
    JSObject::SetOwnPropertyIgnoreAttributes(
        scope_object, handle(String::cast(scope_info->get(i + start))), value,
        ::NONE)
        .Check();
  }
}

}  // namespace internal
}  // namespace v8
