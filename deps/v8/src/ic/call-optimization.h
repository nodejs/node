// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_IC_CALL_OPTIMIZATION_H_
#define V8_IC_CALL_OPTIMIZATION_H_

#include "src/api-arguments.h"
#include "src/objects.h"

namespace v8 {
namespace internal {
// Holds information about possible function call optimizations.
class CallOptimization {
 public:
  CallOptimization(Isolate* isolate, Handle<Object> function);

  Context GetAccessorContext(Map holder_map) const;
  bool IsCrossContextLazyAccessorPair(Context native_context,
                                      Map holder_map) const;

  bool is_constant_call() const { return !constant_function_.is_null(); }

  Handle<JSFunction> constant_function() const {
    DCHECK(is_constant_call());
    return constant_function_;
  }

  bool is_simple_api_call() const { return is_simple_api_call_; }

  Handle<FunctionTemplateInfo> expected_receiver_type() const {
    DCHECK(is_simple_api_call());
    return expected_receiver_type_;
  }

  Handle<CallHandlerInfo> api_call_info() const {
    DCHECK(is_simple_api_call());
    return api_call_info_;
  }

  enum HolderLookup { kHolderNotFound, kHolderIsReceiver, kHolderFound };
  Handle<JSObject> LookupHolderOfExpectedType(
      Handle<Map> receiver_map, HolderLookup* holder_lookup) const;

  // Check if the api holder is between the receiver and the holder.
  bool IsCompatibleReceiver(Handle<Object> receiver,
                            Handle<JSObject> holder) const;

  // Check if the api holder is between the receiver and the holder.
  bool IsCompatibleReceiverMap(Handle<Map> receiver_map,
                               Handle<JSObject> holder) const;

 private:
  void Initialize(Isolate* isolate, Handle<JSFunction> function);
  void Initialize(Isolate* isolate,
                  Handle<FunctionTemplateInfo> function_template_info);

  // Determines whether the given function can be called using the
  // fast api call builtin.
  void AnalyzePossibleApiFunction(Isolate* isolate,
                                  Handle<JSFunction> function);

  Handle<JSFunction> constant_function_;
  bool is_simple_api_call_;
  Handle<FunctionTemplateInfo> expected_receiver_type_;
  Handle<CallHandlerInfo> api_call_info_;
};
}  // namespace internal
}  // namespace v8

#endif  // V8_IC_CALL_OPTIMIZATION_H_
