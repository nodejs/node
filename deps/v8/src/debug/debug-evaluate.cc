// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/debug/debug-evaluate.h"

#include "src/accessors.h"
#include "src/compiler.h"
#include "src/contexts.h"
#include "src/debug/debug-frames.h"
#include "src/debug/debug-scopes.h"
#include "src/debug/debug.h"
#include "src/frames-inl.h"
#include "src/globals.h"
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
  StackTraceFrameIterator it(isolate, frame_id);
  if (!it.is_javascript()) return isolate->factory()->undefined_value();
  JavaScriptFrame* frame = it.javascript_frame();

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

  Handle<Context> context = context_builder.evaluation_context();
  Handle<JSObject> receiver(context->global_proxy());
  MaybeHandle<Object> maybe_result =
      Evaluate(isolate, context_builder.outer_info(), context,
               context_extension, receiver, source);
  if (!maybe_result.is_null()) context_builder.UpdateValues();
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
    context = isolate->factory()->NewWithContext(
        closure, context,
        ScopeInfo::CreateForWithScope(
            isolate, context->IsNativeContext()
                         ? Handle<ScopeInfo>::null()
                         : Handle<ScopeInfo>(context->scope_info())),
        extension);
  }

  Handle<JSFunction> eval_fun;
  ASSIGN_RETURN_ON_EXCEPTION(
      isolate, eval_fun,
      Compiler::GetFunctionFromEval(source, outer_info, context, SLOPPY,
                                    NO_PARSE_RESTRICTION, kNoSourcePosition,
                                    kNoSourcePosition),
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
  Handle<JSFunction> local_function = frame_inspector.GetFunction();
  Handle<Context> outer_context(local_function->context());
  evaluation_context_ = outer_context;
  outer_info_ = handle(local_function->shared());
  Factory* factory = isolate->factory();

  // To evaluate as if we were running eval at the point of the debug break,
  // we reconstruct the context chain as follows:
  //  - To make stack-allocated variables visible, we materialize them and
  //    use a debug-evaluate context to wrap both the materialized object and
  //    the original context.
  //  - We use the original context chain from the function context to the
  //    native context.
  //  - Between the function scope and the native context, we only resolve
  //    variable names that the current function already uses. Only for these
  //    names we can be sure that they will be correctly resolved. For the
  //    rest, we only resolve to with, script, and native contexts. We use a
  //    whitelist to implement that.
  // Context::Lookup has special handling for debug-evaluate contexts:
  //  - Look up in the materialized stack variables.
  //  - Look up in the original context.
  //  - Check the whitelist to find out whether to skip contexts during lookup.
  const ScopeIterator::Option option = ScopeIterator::COLLECT_NON_LOCALS;
  for (ScopeIterator it(isolate, &frame_inspector, option);
       !it.Failed() && !it.Done(); it.Next()) {
    ScopeIterator::ScopeType scope_type = it.Type();
    if (scope_type == ScopeIterator::ScopeTypeLocal) {
      DCHECK_EQ(FUNCTION_SCOPE, it.CurrentScopeInfo()->scope_type());
      Handle<JSObject> materialized = factory->NewJSObjectWithNullProto();
      Handle<Context> local_context =
          it.HasContext() ? it.CurrentContext() : outer_context;
      Handle<StringSet> non_locals = it.GetNonLocals();
      MaterializeReceiver(materialized, local_context, local_function,
                          non_locals);
      frame_inspector.MaterializeStackLocals(materialized, local_function);
      MaterializeArgumentsObject(materialized, local_function);
      ContextChainElement context_chain_element;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_element.materialized_object = materialized;
      // Non-locals that are already being referenced by the current function
      // are guaranteed to be correctly resolved.
      context_chain_element.whitelist = non_locals;
      if (it.HasContext()) {
        context_chain_element.wrapped_context = it.CurrentContext();
      }
      context_chain_.Add(context_chain_element);
      evaluation_context_ = outer_context;
      break;
    } else if (scope_type == ScopeIterator::ScopeTypeCatch ||
               scope_type == ScopeIterator::ScopeTypeWith) {
      ContextChainElement context_chain_element;
      Handle<Context> current_context = it.CurrentContext();
      if (!current_context->IsDebugEvaluateContext()) {
        context_chain_element.wrapped_context = current_context;
      }
      context_chain_.Add(context_chain_element);
    } else if (scope_type == ScopeIterator::ScopeTypeBlock ||
               scope_type == ScopeIterator::ScopeTypeEval) {
      Handle<JSObject> materialized = factory->NewJSObjectWithNullProto();
      frame_inspector.MaterializeStackLocals(materialized,
                                             it.CurrentScopeInfo());
      ContextChainElement context_chain_element;
      context_chain_element.scope_info = it.CurrentScopeInfo();
      context_chain_element.materialized_object = materialized;
      if (it.HasContext()) {
        context_chain_element.wrapped_context = it.CurrentContext();
      }
      context_chain_.Add(context_chain_element);
    } else {
      break;
    }
  }

  for (int i = context_chain_.length() - 1; i >= 0; i--) {
    Handle<ScopeInfo> scope_info(ScopeInfo::CreateForWithScope(
        isolate, evaluation_context_->IsNativeContext()
                     ? Handle<ScopeInfo>::null()
                     : Handle<ScopeInfo>(evaluation_context_->scope_info())));
    scope_info->SetIsDebugEvaluateScope();
    evaluation_context_ = factory->NewDebugEvaluateContext(
        evaluation_context_, scope_info, context_chain_[i].materialized_object,
        context_chain_[i].wrapped_context, context_chain_[i].whitelist);
  }
}


void DebugEvaluate::ContextBuilder::UpdateValues() {
  // TODO(yangguo): remove updating values.
  for (int i = 0; i < context_chain_.length(); i++) {
    ContextChainElement element = context_chain_[i];
    if (!element.materialized_object.is_null()) {
      // Write back potential changes to materialized stack locals to the stack.
      FrameInspector(frame_, inlined_jsframe_index_, isolate_)
          .UpdateStackLocalsFromMaterializedObject(element.materialized_object,
                                                   element.scope_info);
    }
  }
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

void DebugEvaluate::ContextBuilder::MaterializeReceiver(
    Handle<JSObject> target, Handle<Context> local_context,
    Handle<JSFunction> local_function, Handle<StringSet> non_locals) {
  Handle<Object> recv = isolate_->factory()->undefined_value();
  Handle<String> name = isolate_->factory()->this_string();
  if (non_locals->Has(name)) {
    // 'this' is allocated in an outer context and is is already being
    // referenced by the current function, so it can be correctly resolved.
    return;
  } else if (local_function->shared()->scope_info()->HasReceiver() &&
             !frame_->receiver()->IsTheHole(isolate_)) {
    recv = handle(frame_->receiver(), isolate_);
  }
  JSObject::SetOwnPropertyIgnoreAttributes(target, name, recv, NONE).Check();
}

}  // namespace internal
}  // namespace v8
