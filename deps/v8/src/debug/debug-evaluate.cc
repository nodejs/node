// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-evaluate.h"

#include "src/accessors.h"
#include "src/contexts.h"
#include "src/debug/debug.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug-scopes.h"
#include "src/frames-inl.h"
#include "src/isolate-inl.h"

namespace v8 {
namespace internal {

static inline bool IsDebugContext(Isolate* isolate, Context* context) {
  return context->native_context() == *isolate->debug()->debug_context();
}


MaybeHandle<Object> DebugEvaluate::Global(
    Isolate* isolate, Handle<String> source, bool disable_break,
    Handle<HeapObject> context_extension) {
  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug(), disable_break);

  // Enter the top context from before the debugger was invoked.
  SaveContext save(isolate);
  SaveContext* top = &save;
  while (top != NULL && IsDebugContext(isolate, *top->context())) {
    top = top->prev();
  }
  if (top != NULL) isolate->set_context(*top->context());

  // Get the native context now set to the top context from before the
  // debugger was invoked.
  Handle<Context> context = isolate->native_context();
  Handle<JSObject> receiver(context->global_proxy());
  Handle<SharedFunctionInfo> outer_info(context->closure()->shared(), isolate);
  return Evaluate(isolate, outer_info, context, context_extension, receiver,
                  source);
}


MaybeHandle<Object> DebugEvaluate::Local(Isolate* isolate,
                                         StackFrame::Id frame_id,
                                         int inlined_jsframe_index,
                                         Handle<String> source,
                                         bool disable_break,
                                         Handle<HeapObject> context_extension) {
  // Handle the processing of break.
  DisableBreak disable_break_scope(isolate->debug(), disable_break);

  // Get the frame where the debugging is performed.
  JavaScriptFrameIterator it(isolate, frame_id);
  JavaScriptFrame* frame = it.frame();

  // Traverse the saved contexts chain to find the active context for the
  // selected frame.
  SaveContext* save =
      DebugFrameHelper::FindSavedContextForFrame(isolate, frame);
  SaveContext savex(isolate);
  isolate->set_context(*(save->context()));

  // This is not a lot different than DebugEvaluate::Global, except that
  // variables accessible by the function we are evaluating from are
  // materialized and included on top of the native context. Changes to
  // the materialized object are written back afterwards.
  // Note that the native context is taken from the original context chain,
  // which may not be the current native context of the isolate.
  ContextBuilder context_builder(isolate, frame, inlined_jsframe_index);
  if (isolate->has_pending_exception()) return MaybeHandle<Object>();

  Handle<Context> context = context_builder.native_context();
  Handle<JSObject> receiver(context->global_proxy());
  MaybeHandle<Object> maybe_result = Evaluate(
      isolate, context_builder.outer_info(),
      context_builder.innermost_context(), context_extension, receiver, source);
  if (!maybe_result.is_null() && !FLAG_debug_eval_readonly_locals) {
    context_builder.UpdateValues();
  }
  return maybe_result;
}


// Compile and evaluate source for the given context.
MaybeHandle<Object> DebugEvaluate::Evaluate(
    Isolate* isolate, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, Handle<HeapObject> context_extension,
    Handle<Object> receiver, Handle<String> source) {
  if (context_extension->IsJSObject()) {
    Handle<JSObject> extension = Handle<JSObject>::cast(context_extension);
    Handle<JSFunction> closure(context->closure(), isolate);
    context = isolate->factory()->NewWithContext(closure, context, extension);
  }

  Handle<JSFunction> eval_fun;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, eval_fun,
                             Compiler::GetFunctionFromEval(
                                 source, outer_info, context, SLOPPY,
                                 NO_PARSE_RESTRICTION, RelocInfo::kNoPosition),
                             Object);

  Handle<Object> result;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, result, Execution::Call(isolate, eval_fun, receiver, 0, NULL),
      Object);

  // Skip the global proxy as it has no properties and always delegates to the
  // real global object.
  if (result->IsJSGlobalProxy()) {
    PrototypeIterator iter(isolate, Handle<JSGlobalProxy>::cast(result));
    // TODO(verwaest): This will crash when the global proxy is detached.
    result = PrototypeIterator::GetCurrent<JSObject>(iter);
  }

  return result;
}


DebugEvaluate::ContextBuilder::ContextBuilder(Isolate* isolate,
                                              JavaScriptFrame* frame,
                                              int inlined_jsframe_index)
    : isolate_(isolate),
      frame_(frame),
      inlined_jsframe_index_(inlined_jsframe_index) {
  FrameInspector frame_inspector(frame, inlined_jsframe_index, isolate);
  Handle<JSFunction> local_function =
      Handle<JSFunction>::cast(frame_inspector.GetFunction());
  Handle<Context> outer_context(local_function->context());
  native_context_ = Handle<Context>(outer_context->native_context());
  Handle<JSFunction> global_function(native_context_->closure());
  outer_info_ = handle(global_function->shared());
  Handle<Context> inner_context;

  bool stop = false;

  // Iterate the original context chain to create a context chain that reflects
  // our needs. The original context chain may look like this:
  // <native context> <outer contexts> <function context> <inner contexts>
  // In the resulting context chain, we want to materialize the receiver,
  // the parameters of the current function, the stack locals. We only
  // materialize context variables that the function already references,
  // because only for those variables we can be sure that they will be resolved
  // correctly. Variables that are not referenced by the function may be
  // context-allocated and thus accessible, but may be shadowed by stack-
  // allocated variables and the resolution would be incorrect.
  // The result will look like this:
  // <native context> <receiver context>
  //     <materialized stack and accessible context vars> <inner contexts>
  // All contexts use the closure of the native context, since there is no
  // function context in the chain. Variables that cannot be resolved are
  // bound to toplevel (script contexts or global object).
  // Once debug-evaluate has been executed, the changes to the materialized
  // objects are written back to the original context chain. Any changes to
  // the original context chain will therefore be overwritten.
  const ScopeIterator::Option option = ScopeIterator::COLLECT_NON_LOCALS;
  for (ScopeIterator it(isolate, &frame_inspector, option);
       !it.Failed() && !it.Done() && !stop; it.Next()) {
    ScopeIterator::ScopeType scope_type = it.Type();
    if (scope_type == ScopeIterator::ScopeTypeLocal) {
      DCHECK_EQ(FUNCTION_SCOPE, it.CurrentScopeInfo()->scope_type());
      it.GetNonLocals(&non_locals_);
      Handle<Context> local_context =
          it.HasContext() ? it.CurrentContext() : outer_context;

      // The "this" binding, if any, can't be bound via "with".  If we need
      // to, add another node onto the outer context to bind "this".
      Handle<Context> receiver_context =
          MaterializeReceiver(native_context_, local_context, local_function,
                              global_function, it.ThisIsNonLocal());

      Handle<JSObject> materialized_function = NewJSObjectWithNullProto();
      frame_inspector.MaterializeStackLocals(materialized_function,
                                             local_function);
      MaterializeArgumentsObject(materialized_function, local_function);
      MaterializeContextChain(materialized_function, local_context);

      Handle<Context> with_context = isolate->factory()->NewWithContext(
          global_function, receiver_context, materialized_function);

      ContextChainElement context_chain_element;
      context_chain_element.original_context = local_context;
      context_chain_element.materialized_object = materialized_function;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_.Add(context_chain_element);

      stop = true;
      RecordContextsInChain(&inner_context, receiver_context, with_context);
    } else if (scope_type == ScopeIterator::ScopeTypeCatch ||
               scope_type == ScopeIterator::ScopeTypeWith) {
      Handle<Context> cloned_context = Handle<Context>::cast(
          isolate->factory()->CopyFixedArray(it.CurrentContext()));

      ContextChainElement context_chain_element;
      context_chain_element.original_context = it.CurrentContext();
      context_chain_element.cloned_context = cloned_context;
      context_chain_.Add(context_chain_element);

      RecordContextsInChain(&inner_context, cloned_context, cloned_context);
    } else if (scope_type == ScopeIterator::ScopeTypeBlock) {
      Handle<JSObject> materialized_object = NewJSObjectWithNullProto();
      frame_inspector.MaterializeStackLocals(materialized_object,
                                             it.CurrentScopeInfo());
      if (it.HasContext()) {
        Handle<Context> cloned_context = Handle<Context>::cast(
            isolate->factory()->CopyFixedArray(it.CurrentContext()));
        Handle<Context> with_context = isolate->factory()->NewWithContext(
            global_function, cloned_context, materialized_object);

        ContextChainElement context_chain_element;
        context_chain_element.original_context = it.CurrentContext();
        context_chain_element.cloned_context = cloned_context;
        context_chain_element.materialized_object = materialized_object;
        context_chain_element.scope_info = it.CurrentScopeInfo();
        context_chain_.Add(context_chain_element);

        RecordContextsInChain(&inner_context, cloned_context, with_context);
      } else {
        Handle<Context> with_context = isolate->factory()->NewWithContext(
            global_function, outer_context, materialized_object);

        ContextChainElement context_chain_element;
        context_chain_element.materialized_object = materialized_object;
        context_chain_element.scope_info = it.CurrentScopeInfo();
        context_chain_.Add(context_chain_element);

        RecordContextsInChain(&inner_context, with_context, with_context);
      }
    } else {
      stop = true;
    }
  }
  if (innermost_context_.is_null()) {
    innermost_context_ = outer_context;
  }
  DCHECK(!innermost_context_.is_null());
}


void DebugEvaluate::ContextBuilder::UpdateValues() {
  // TODO(yangguo): remove updating values.
  for (int i = 0; i < context_chain_.length(); i++) {
    ContextChainElement element = context_chain_[i];
    if (!element.original_context.is_null() &&
        !element.cloned_context.is_null()) {
      Handle<Context> cloned_context = element.cloned_context;
      cloned_context->CopyTo(
          Context::MIN_CONTEXT_SLOTS, *element.original_context,
          Context::MIN_CONTEXT_SLOTS,
          cloned_context->length() - Context::MIN_CONTEXT_SLOTS);
    }
    if (!element.materialized_object.is_null()) {
      // Write back potential changes to materialized stack locals to the
      // stack.
      FrameInspector(frame_, inlined_jsframe_index_, isolate_)
          .UpdateStackLocalsFromMaterializedObject(element.materialized_object,
                                                   element.scope_info);
      if (element.scope_info->scope_type() == FUNCTION_SCOPE) {
        DCHECK_EQ(context_chain_.length() - 1, i);
        UpdateContextChainFromMaterializedObject(element.materialized_object,
                                                 element.original_context);
      }
    }
  }
}


Handle<JSObject> DebugEvaluate::ContextBuilder::NewJSObjectWithNullProto() {
  Handle<JSObject> result =
      isolate_->factory()->NewJSObject(isolate_->object_function());
  Handle<Map> new_map =
      Map::Copy(Handle<Map>(result->map()), "ObjectWithNullProto");
  Map::SetPrototype(new_map, isolate_->factory()->null_value());
  JSObject::MigrateToMap(result, new_map);
  return result;
}


void DebugEvaluate::ContextBuilder::RecordContextsInChain(
    Handle<Context>* inner_context, Handle<Context> first,
    Handle<Context> last) {
  if (!inner_context->is_null()) {
    (*inner_context)->set_previous(*last);
  } else {
    innermost_context_ = last;
  }
  *inner_context = first;
}


void DebugEvaluate::ContextBuilder::MaterializeArgumentsObject(
    Handle<JSObject> target, Handle<JSFunction> function) {
  // Do not materialize the arguments object for eval or top-level code.
  // Skip if "arguments" is already taken.
  if (!function->shared()->is_function()) return;
  Maybe<bool> maybe = JSReceiver::HasOwnProperty(
      target, isolate_->factory()->arguments_string());
  DCHECK(maybe.IsJust());
  if (maybe.FromJust()) return;

  // FunctionGetArguments can't throw an exception.
  Handle<JSObject> arguments = Accessors::FunctionGetArguments(function);
  Handle<String> arguments_str = isolate_->factory()->arguments_string();
  JSObject::SetOwnPropertyIgnoreAttributes(target, arguments_str, arguments,
                                           NONE)
      .Check();
}


MaybeHandle<Object> DebugEvaluate::ContextBuilder::LoadFromContext(
    Handle<Context> context, Handle<String> name, bool* global) {
  static const ContextLookupFlags flags = FOLLOW_CONTEXT_CHAIN;
  int index;
  PropertyAttributes attributes;
  BindingFlags binding;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding);
  if (holder.is_null()) return MaybeHandle<Object>();
  Handle<Object> value;
  if (index != Context::kNotFound) {  // Found on context.
    Handle<Context> context = Handle<Context>::cast(holder);
    // Do not shadow variables on the script context.
    *global = context->IsScriptContext();
    return Handle<Object>(context->get(index), isolate_);
  } else {  // Found on object.
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(holder);
    // Do not shadow properties on the global object.
    *global = object->IsJSGlobalObject();
    return JSReceiver::GetDataProperty(object, name);
  }
}


void DebugEvaluate::ContextBuilder::MaterializeContextChain(
    Handle<JSObject> target, Handle<Context> context) {
  for (const Handle<String>& name : non_locals_) {
    HandleScope scope(isolate_);
    Handle<Object> value;
    bool global;
    if (!LoadFromContext(context, name, &global).ToHandle(&value) || global) {
      // If resolving the variable fails, skip it. If it resolves to a global
      // variable, skip it as well since it's not read-only and can be resolved
      // within debug-evaluate.
      continue;
    }
    if (value->IsTheHole()) continue;  // Value is not initialized yet (in TDZ).
    JSObject::SetOwnPropertyIgnoreAttributes(target, name, value, NONE).Check();
  }
}


void DebugEvaluate::ContextBuilder::StoreToContext(Handle<Context> context,
                                                   Handle<String> name,
                                                   Handle<Object> value) {
  static const ContextLookupFlags flags = FOLLOW_CONTEXT_CHAIN;
  int index;
  PropertyAttributes attributes;
  BindingFlags binding;
  Handle<Object> holder =
      context->Lookup(name, flags, &index, &attributes, &binding);
  if (holder.is_null()) return;
  if (attributes & READ_ONLY) return;
  if (index != Context::kNotFound) {  // Found on context.
    Handle<Context> context = Handle<Context>::cast(holder);
    context->set(index, *value);
  } else {  // Found on object.
    Handle<JSReceiver> object = Handle<JSReceiver>::cast(holder);
    LookupIterator lookup(object, name);
    if (lookup.state() != LookupIterator::DATA) return;
    CHECK(JSReceiver::SetDataProperty(&lookup, value).FromJust());
  }
}


void DebugEvaluate::ContextBuilder::UpdateContextChainFromMaterializedObject(
    Handle<JSObject> source, Handle<Context> context) {
  // TODO(yangguo): check whether overwriting context fields is actually safe
  //                wrt fields we consider constant.
  for (const Handle<String>& name : non_locals_) {
    HandleScope scope(isolate_);
    Handle<Object> value = JSReceiver::GetDataProperty(source, name);
    StoreToContext(context, name, value);
  }
}


Handle<Context> DebugEvaluate::ContextBuilder::MaterializeReceiver(
    Handle<Context> parent_context, Handle<Context> lookup_context,
    Handle<JSFunction> local_function, Handle<JSFunction> global_function,
    bool this_is_non_local) {
  Handle<Object> receiver = isolate_->factory()->undefined_value();
  Handle<String> this_string = isolate_->factory()->this_string();
  if (this_is_non_local) {
    bool global;
    LoadFromContext(lookup_context, this_string, &global).ToHandle(&receiver);
  } else if (local_function->shared()->scope_info()->HasReceiver()) {
    receiver = handle(frame_->receiver(), isolate_);
  }
  return isolate_->factory()->NewCatchContext(global_function, parent_context,
                                              this_string, receiver);
}

}  // namespace internal
}  // namespace v8
