// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_EVALUATE_H_
#define V8_DEBUG_DEBUG_EVALUATE_H_

#include "src/frames.h"
#include "src/objects.h"

namespace v8 {
namespace internal {

class DebugEvaluate : public AllStatic {
 public:
  static MaybeHandle<Object> Global(Isolate* isolate, Handle<String> source,
                                    bool disable_break,
                                    Handle<HeapObject> context_extension);

  // Evaluate a piece of JavaScript in the context of a stack frame for
  // debugging.  Things that need special attention are:
  // - Parameters and stack-allocated locals need to be materialized.  Altered
  //   values need to be written back to the stack afterwards.
  // - The arguments object needs to materialized.
  static MaybeHandle<Object> Local(Isolate* isolate, StackFrame::Id frame_id,
                                   int inlined_jsframe_index,
                                   Handle<String> source, bool disable_break,
                                   Handle<HeapObject> context_extension);

 private:
  // This class builds a context chain for evaluation of expressions
  // in debugger.
  // The scope chain leading up to a breakpoint where evaluation occurs
  // looks like:
  // - [a mix of with, catch and block scopes]
  //    - [function stack + context]
  //      - [outer context]
  // The builder materializes all stack variables into properties of objects;
  // the expression is then evaluated as if it is inside a series of 'with'
  // statements using those objects. To this end, the builder builds a new
  // context chain, based on a scope chain:
  //   - every With and Catch scope begets a cloned context
  //   - Block scope begets one or two contexts:
  //       - if a block has context-allocated varaibles, its context is cloned
  //       - stack locals are materizalized as a With context
  //   - Local scope begets a With context for materizalized locals, chained to
  //     original function context. Original function context is the end of
  //     the chain.
  class ContextBuilder {
   public:
    ContextBuilder(Isolate* isolate, JavaScriptFrame* frame,
                   int inlined_jsframe_index);

    void UpdateValues();

    Handle<Context> evaluation_context() const { return evaluation_context_; }
    Handle<SharedFunctionInfo> outer_info() const { return outer_info_; }

   private:
    struct ContextChainElement {
      Handle<ScopeInfo> scope_info;
      Handle<Context> wrapped_context;
      Handle<JSObject> materialized_object;
      Handle<StringSet> whitelist;
    };

    // Helper function to find or create the arguments object for
    // Runtime_DebugEvaluate.
    void MaterializeArgumentsObject(Handle<JSObject> target,
                                    Handle<JSFunction> function);

    void MaterializeReceiver(Handle<JSObject> target,
                             Handle<Context> local_context,
                             Handle<JSFunction> local_function,
                             Handle<StringSet> non_locals);

    Handle<SharedFunctionInfo> outer_info_;
    Handle<Context> evaluation_context_;
    List<ContextChainElement> context_chain_;
    Isolate* isolate_;
    JavaScriptFrame* frame_;
    int inlined_jsframe_index_;
  };

  static MaybeHandle<Object> Evaluate(Isolate* isolate,
                                      Handle<SharedFunctionInfo> outer_info,
                                      Handle<Context> context,
                                      Handle<HeapObject> context_extension,
                                      Handle<Object> receiver,
                                      Handle<String> source);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_EVALUATE_H_
