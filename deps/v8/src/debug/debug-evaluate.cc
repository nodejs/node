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
#include "src/isolate.h"

namespace v8 {
namespace internal {

static inline bool IsDebugContext(Isolate* isolate, Context* context) {
  return context->native_context() == *isolate->debug()->debug_context();
}


MaybeHandle<Object> DebugEvaluate::Global(Isolate* isolate,
                                          Handle<String> source,
                                          bool disable_break,
                                          Handle<Object> context_extension) {
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
                                         Handle<Object> context_extension) {
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

  // Materialize stack locals and the arguments object.
  ContextBuilder context_builder(isolate, frame, inlined_jsframe_index);
  if (isolate->has_pending_exception()) return MaybeHandle<Object>();

  Handle<Object> receiver(frame->receiver(), isolate);
  MaybeHandle<Object> maybe_result = Evaluate(
      isolate, context_builder.outer_info(),
      context_builder.innermost_context(), context_extension, receiver, source);
  if (!maybe_result.is_null()) context_builder.UpdateValues();
  return maybe_result;
}


// Compile and evaluate source for the given context.
MaybeHandle<Object> DebugEvaluate::Evaluate(
    Isolate* isolate, Handle<SharedFunctionInfo> outer_info,
    Handle<Context> context, Handle<Object> context_extension,
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
    PrototypeIterator iter(isolate, result);
    // TODO(verwaest): This will crash when the global proxy is detached.
    result = Handle<JSObject>::cast(PrototypeIterator::GetCurrent(iter));
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
  Handle<JSFunction> function =
      handle(JSFunction::cast(frame_inspector.GetFunction()));
  Handle<Context> outer_context = handle(function->context(), isolate);
  outer_info_ = handle(function->shared());
  Handle<Context> inner_context;

  bool stop = false;
  for (ScopeIterator it(isolate, &frame_inspector);
       !it.Failed() && !it.Done() && !stop; it.Next()) {
    ScopeIterator::ScopeType scope_type = it.Type();

    if (scope_type == ScopeIterator::ScopeTypeLocal) {
      Handle<Context> parent_context =
          it.HasContext() ? it.CurrentContext() : outer_context;

      // The "this" binding, if any, can't be bound via "with".  If we need
      // to, add another node onto the outer context to bind "this".
      parent_context = MaterializeReceiver(parent_context, function);

      Handle<JSObject> materialized_function = NewJSObjectWithNullProto();

      frame_inspector.MaterializeStackLocals(materialized_function, function);

      MaterializeArgumentsObject(materialized_function, function);

      Handle<Context> with_context = isolate->factory()->NewWithContext(
          function, parent_context, materialized_function);

      ContextChainElement context_chain_element;
      context_chain_element.original_context = it.CurrentContext();
      context_chain_element.materialized_object = materialized_function;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_.Add(context_chain_element);

      stop = true;
      RecordContextsInChain(&inner_context, with_context, with_context);
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
            function, cloned_context, materialized_object);

        ContextChainElement context_chain_element;
        context_chain_element.original_context = it.CurrentContext();
        context_chain_element.cloned_context = cloned_context;
        context_chain_element.materialized_object = materialized_object;
        context_chain_element.scope_info = it.CurrentScopeInfo();
        context_chain_.Add(context_chain_element);

        RecordContextsInChain(&inner_context, cloned_context, with_context);
      } else {
        Handle<Context> with_context = isolate->factory()->NewWithContext(
            function, outer_context, materialized_object);

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
  Handle<JSObject> arguments =
      Handle<JSObject>::cast(Accessors::FunctionGetArguments(function));
  Handle<String> arguments_str = isolate_->factory()->arguments_string();
  JSObject::SetOwnPropertyIgnoreAttributes(target, arguments_str, arguments,
                                           NONE)
      .Check();
}


Handle<Context> DebugEvaluate::ContextBuilder::MaterializeReceiver(
    Handle<Context> target, Handle<JSFunction> function) {
  Handle<SharedFunctionInfo> shared(function->shared());
  Handle<ScopeInfo> scope_info(shared->scope_info());
  Handle<Object> receiver;
  switch (scope_info->scope_type()) {
    case FUNCTION_SCOPE: {
      VariableMode mode;
      VariableLocation location;
      InitializationFlag init_flag;
      MaybeAssignedFlag maybe_assigned_flag;

      // Don't bother creating a fake context node if "this" is in the context
      // already.
      if (ScopeInfo::ContextSlotIndex(
              scope_info, isolate_->factory()->this_string(), &mode, &location,
              &init_flag, &maybe_assigned_flag) >= 0) {
        return target;
      }
      receiver = handle(frame_->receiver(), isolate_);
      break;
    }
    case MODULE_SCOPE:
      receiver = isolate_->factory()->undefined_value();
      break;
    case SCRIPT_SCOPE:
      receiver = handle(function->global_proxy(), isolate_);
      break;
    default:
      // For eval code, arrow functions, and the like, there's no "this" binding
      // to materialize.
      return target;
  }

  return isolate_->factory()->NewCatchContext(
      function, target, isolate_->factory()->this_string(), receiver);
}

}  // namespace internal
}  // namespace v8
