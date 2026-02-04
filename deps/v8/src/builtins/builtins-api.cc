// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/api/api-arguments-inl.h"
#include "src/api/api-natives.h"
#include "src/base/small-vector.h"
#include "src/builtins/builtins-utils-inl.h"
#include "src/builtins/builtins.h"
#include "src/common/assert-scope.h"
#include "src/logging/log.h"
#include "src/logging/runtime-call-stats-scope.h"
#include "src/objects/objects-inl.h"
#include "src/objects/prototype.h"
#include "src/objects/templates.h"
#include "src/objects/visitors.h"

namespace v8 {
namespace internal {

namespace {

// Returns true if the function can legally be called with this receiver,
// otherwise false.
// TODO(ishell): CallOptimization duplicates this logic, merge.
bool IsCompatibleReceiver(Isolate* isolate, Tagged<FunctionTemplateInfo> info,
                          Tagged<JSReceiver> receiver) {
  RCS_SCOPE(isolate, RuntimeCallCounterId::kGetCompatibleReceiver);
  Tagged<Object> recv_type = info->signature();
  // No signature, so function can be called with any receiver.
  if (!IsFunctionTemplateInfo(recv_type)) return true;
  // A Proxy or Wasm object cannot have been created from the signature
  // template.
  if (!IsJSObject(receiver)) return false;

  Tagged<JSObject> js_obj_receiver = Cast<JSObject>(receiver);
  Tagged<FunctionTemplateInfo> signature =
      Cast<FunctionTemplateInfo>(recv_type);

  // Check the receiver.
  if (signature->IsTemplateFor(js_obj_receiver)) return true;

  // The JSGlobalProxy might have a hidden prototype.
  if (V8_UNLIKELY(IsJSGlobalProxy(js_obj_receiver))) {
    Tagged<HeapObject> prototype = js_obj_receiver->map()->prototype();
    if (!IsNull(prototype, isolate)) {
      Tagged<JSObject> js_obj_prototype = Cast<JSObject>(prototype);
      if (signature->IsTemplateFor(js_obj_prototype)) return true;
    }
  }
  return false;
}

// argv and argc are the same as those passed to FunctionCallbackInfo:
// - argc is the number of arguments excluding the receiver
// - argv is the array arguments. The receiver is stored at argv[-1].
template <bool is_construct, typename ArgT>
V8_WARN_UNUSED_RESULT MaybeHandle<Object> HandleApiCallHelper(
    Isolate* isolate, DirectHandle<HeapObject> new_target,
    DirectHandle<FunctionTemplateInfo> fun_data, DirectHandle<Object> receiver,
    const base::Vector<const ArgT>& args,
    Address* receiver_location_on_stack = nullptr) {
  static_assert(std::is_same_v<ArgT, DirectHandle<Object>> ||
                std::is_same_v<ArgT, Address>);

  Handle<JSReceiver> js_receiver;
  if (is_construct) {
    DCHECK(IsTheHole(*receiver, isolate));
    if (IsUndefined(fun_data->GetInstanceTemplate(), isolate)) {
      v8::Local<ObjectTemplate> templ =
          ObjectTemplate::New(reinterpret_cast<v8::Isolate*>(isolate),
                              ToApiHandle<v8::FunctionTemplate>(fun_data));
      FunctionTemplateInfo::SetInstanceTemplate(
          isolate, fun_data, Utils::OpenDirectHandle(*templ));
    }
    DirectHandle<ObjectTemplateInfo> instance_template(
        Cast<ObjectTemplateInfo>(fun_data->GetInstanceTemplate()), isolate);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, js_receiver,
        ApiNatives::InstantiateObject(isolate, instance_template,
                                      Cast<JSReceiver>(new_target)));

    if constexpr (std::is_same_v<ArgT, Address>) {
      // In case we get here via HandleApiConstruct, update receiver value
      // on the stack in order to make sure that the stack trace Api could
      // observe the actual value of the receiver.
      *receiver_location_on_stack = js_receiver->ptr();
    }

  } else {
    DCHECK(IsJSReceiver(*receiver));
    js_receiver = indirect_handle(Cast<JSReceiver>(receiver), isolate);

    if (!fun_data->accept_any_receiver() && IsAccessCheckNeeded(*js_receiver)) {
      // Proxies and Wasm objects never need access checks.
      DCHECK(IsJSObject(*js_receiver));
      DirectHandle<JSObject> js_object = Cast<JSObject>(js_receiver);
      if (!isolate->MayAccess(isolate->native_context(), js_object)) {
        RETURN_ON_EXCEPTION(isolate,
                            isolate->ReportFailedAccessCheck(js_object));
        UNREACHABLE();
      }
    }

    if (!IsCompatibleReceiver(isolate, *fun_data, *js_receiver)) {
      // This function cannot be called with the given receiver.  Abort!
      THROW_NEW_ERROR(isolate,
                      NewTypeError(MessageTemplate::kIllegalInvocation));
    }
  }

  if (fun_data->has_callback(isolate)) {
    Tagged<JSAny> raw_result;
    if (is_construct) {
      FunctionCallbackArguments custom(isolate, *fun_data, *new_target,
                                       *js_receiver, args);
      raw_result = custom.CallOrConstruct(isolate, *fun_data, is_construct);
    } else {
      FunctionCallbackArguments custom(isolate, *fun_data, *js_receiver, args);
      raw_result = custom.CallOrConstruct(isolate, *fun_data, is_construct);
    }
    RETURN_EXCEPTION_IF_EXCEPTION(isolate);
    if (!is_construct || IsJSReceiver(raw_result)) {
      return handle(raw_result, isolate);
    }
  }

  return js_receiver;
}

}  // anonymous namespace

BUILTIN(HandleApiConstruct) {
  HandleScope scope(isolate);
  DirectHandle<Object> receiver = args.receiver();
  DirectHandle<HeapObject> new_target = args.new_target();
  DCHECK(!IsUndefined(*new_target, isolate));
  DirectHandle<FunctionTemplateInfo> fun_data(
      args.target()->shared()->api_func_data(), isolate);

  // TODO(ishell, http://crbug.com/326505377): avoid double-copying of the
  // arguments on this path by porting this builtin to assembly and letting
  // it create the required frame structure.

  RETURN_RESULT_OR_FAILURE(isolate, HandleApiCallHelper<true>(
                                        isolate, new_target, fun_data, receiver,
                                        base::VectorOf<const Address>(
                                            args.address_of_first_argument(),
                                            args.argc_without_receiver()),
                                        args.address_of_receiver()));
}

MaybeHandle<Object> Builtins::InvokeApiFunction(
    Isolate* isolate, bool is_construct,
    DirectHandle<FunctionTemplateInfo> function, DirectHandle<Object> receiver,
    base::Vector<const DirectHandle<Object>> args,
    DirectHandle<HeapObject> new_target) {
  RCS_SCOPE(isolate, RuntimeCallCounterId::kInvokeApiFunction);

  // Do proper receiver conversion for non-strict mode api functions.
  if (!is_construct && !IsJSReceiver(*receiver)) {
    ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                               Object::ConvertReceiver(isolate, receiver));
  }

  // We assume that all lazy accessor pairs have been instantiated when setting
  // a break point on any API function.
  DCHECK(!Cast<FunctionTemplateInfo>(function)->BreakAtEntry(isolate));

  if (is_construct) {
    return HandleApiCallHelper<true>(isolate, new_target, function, receiver,
                                     args);
  }
  return HandleApiCallHelper<false>(isolate, new_target, function, receiver,
                                    args);
}

// Helper function to handle calls to non-function objects created through the
// API. The object can be called as either a constructor (using new) or just as
// a function (without new).
V8_WARN_UNUSED_RESULT static Tagged<Object>
HandleApiCallAsFunctionOrConstructorDelegate(Isolate* isolate,
                                             bool is_construct_call,
                                             DirectHandle<JSAny> receiver,
                                             base::Vector<const Address> args) {
  // Get the object called.
  Tagged<JSObject> obj = Cast<JSObject>(*receiver);

  // Set the new target.
  Tagged<HeapObject> new_target;
  if (is_construct_call) {
    // TODO(adamk): This should be passed through in args instead of
    // being patched in here. We need to set a non-undefined value
    // for v8::FunctionCallbackInfo::IsConstructCall() to get the
    // right answer.
    new_target = obj;
  } else {
    new_target = ReadOnlyRoots(isolate).undefined_value();
  }

  // Get the invocation callback from the function descriptor that was
  // used to create the called object.
  DCHECK(obj->map()->is_callable());
  Tagged<JSFunction> constructor =
      Cast<JSFunction>(obj->map()->GetConstructor());
  DCHECK(constructor->shared()->IsApiFunction());
  Tagged<Object> handler =
      constructor->shared()->api_func_data()->GetInstanceCallHandler();
  DCHECK(!IsUndefined(handler, isolate));
  Tagged<FunctionTemplateInfo> templ = Cast<FunctionTemplateInfo>(handler);
  DCHECK(templ->is_object_template_call_handler());
  DCHECK(templ->has_callback(isolate));

  // Get the data for the call and perform the callback.
  Tagged<JSAny> result;
  {
    HandleScope scope(isolate);
    if (is_construct_call) {
      FunctionCallbackArguments custom(isolate, templ, new_target, *receiver,
                                       args);
      result = custom.CallOrConstruct(isolate, templ, is_construct_call);
    } else {
      FunctionCallbackArguments custom(isolate, templ, *receiver, args);
      result = custom.CallOrConstruct(isolate, templ, is_construct_call);
    }
    // Check for exceptions and return result.
    RETURN_FAILURE_IF_EXCEPTION(isolate);
  }
  return result;
}

// Handle calls to non-function objects created through the API. This delegate
// function is used when the call is a normal function call.
BUILTIN(HandleApiCallAsFunctionDelegate) {
  isolate->CountUsage(v8::Isolate::UseCounterFeature::kDocumentAllLegacyCall);
  return HandleApiCallAsFunctionOrConstructorDelegate(
      isolate, false, args.receiver(),
      base::VectorOf(args.address_of_first_argument(),
                     args.argc_without_receiver()));
}

// Handle calls to non-function objects created through the API. This delegate
// function is used when the call is a construct call.
BUILTIN(HandleApiCallAsConstructorDelegate) {
  isolate->CountUsage(
      v8::Isolate::UseCounterFeature::kDocumentAllLegacyConstruct);
  return HandleApiCallAsFunctionOrConstructorDelegate(
      isolate, true, args.receiver(),
      base::VectorOf(args.address_of_first_argument(),
                     args.argc_without_receiver()));
}

}  // namespace internal
}  // namespace v8
