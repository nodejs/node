// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins.h"

#include "src/api-arguments.h"
#include "src/api-natives.h"
#include "src/builtins/builtins-utils.h"
#include "src/counters.h"
#include "src/log.h"
#include "src/objects-inl.h"
#include "src/prototype.h"
#include "src/visitors.h"

namespace v8 {
namespace internal {

namespace {

// Returns the holder JSObject if the function can legally be called with this
// receiver.  Returns nullptr if the call is illegal.
// TODO(dcarney): CallOptimization duplicates this logic, merge.
JSObject* GetCompatibleReceiver(Isolate* isolate, FunctionTemplateInfo* info,
                                JSObject* receiver) {
  Object* recv_type = info->signature();
  // No signature, return holder.
  if (!recv_type->IsFunctionTemplateInfo()) return receiver;
  FunctionTemplateInfo* signature = FunctionTemplateInfo::cast(recv_type);

  // Check the receiver. Fast path for receivers with no hidden prototypes.
  if (signature->IsTemplateFor(receiver)) return receiver;
  if (!receiver->map()->has_hidden_prototype()) return nullptr;
  for (PrototypeIterator iter(isolate, receiver, kStartAtPrototype,
                              PrototypeIterator::END_AT_NON_HIDDEN);
       !iter.IsAtEnd(); iter.Advance()) {
    JSObject* current = iter.GetCurrent<JSObject>();
    if (signature->IsTemplateFor(current)) return current;
  }
  return nullptr;
}

template <bool is_construct>
MUST_USE_RESULT MaybeHandle<Object> HandleApiCallHelper(
    Isolate* isolate, Handle<HeapObject> function,
    Handle<HeapObject> new_target, Handle<FunctionTemplateInfo> fun_data,
    Handle<Object> receiver, BuiltinArguments args) {
  Handle<JSObject> js_receiver;
  JSObject* raw_holder;
  if (is_construct) {
    DCHECK(args.receiver()->IsTheHole(isolate));
    if (fun_data->instance_template()->IsUndefined(isolate)) {
      v8::Local<ObjectTemplate> templ =
          ObjectTemplate::New(reinterpret_cast<v8::Isolate*>(isolate),
                              ToApiHandle<v8::FunctionTemplate>(fun_data));
      fun_data->set_instance_template(*Utils::OpenHandle(*templ));
    }
    Handle<ObjectTemplateInfo> instance_template(
        ObjectTemplateInfo::cast(fun_data->instance_template()), isolate);
    ASSIGN_RETURN_ON_EXCEPTION(
        isolate, js_receiver,
        ApiNatives::InstantiateObject(instance_template,
                                      Handle<JSReceiver>::cast(new_target)),
        Object);
    args[0] = *js_receiver;
    DCHECK_EQ(*js_receiver, *args.receiver());

    raw_holder = *js_receiver;
  } else {
    DCHECK(receiver->IsJSReceiver());

    if (!receiver->IsJSObject()) {
      // This function cannot be called with the given receiver.  Abort!
      THROW_NEW_ERROR(
          isolate, NewTypeError(MessageTemplate::kIllegalInvocation), Object);
    }

    js_receiver = Handle<JSObject>::cast(receiver);

    if (!fun_data->accept_any_receiver() &&
        js_receiver->IsAccessCheckNeeded() &&
        !isolate->MayAccess(handle(isolate->context()), js_receiver)) {
      isolate->ReportFailedAccessCheck(js_receiver);
      RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
      return isolate->factory()->undefined_value();
    }

    raw_holder = GetCompatibleReceiver(isolate, *fun_data, *js_receiver);

    if (raw_holder == nullptr) {
      // This function cannot be called with the given receiver.  Abort!
      THROW_NEW_ERROR(
          isolate, NewTypeError(MessageTemplate::kIllegalInvocation), Object);
    }
  }

  Object* raw_call_data = fun_data->call_code();
  if (!raw_call_data->IsUndefined(isolate)) {
    DCHECK(raw_call_data->IsCallHandlerInfo());
    CallHandlerInfo* call_data = CallHandlerInfo::cast(raw_call_data);
    Object* callback_obj = call_data->callback();
    v8::FunctionCallback callback =
        v8::ToCData<v8::FunctionCallback>(callback_obj);
    Object* data_obj = call_data->data();

    LOG(isolate, ApiObjectAccess("call", JSObject::cast(*js_receiver)));

    FunctionCallbackArguments custom(isolate, data_obj, *function, raw_holder,
                                     *new_target, &args[0] - 1,
                                     args.length() - 1);

    Handle<Object> result = custom.Call(callback);

    RETURN_EXCEPTION_IF_SCHEDULED_EXCEPTION(isolate, Object);
    if (result.is_null()) {
      if (is_construct) return js_receiver;
      return isolate->factory()->undefined_value();
    }
    // Rebox the result.
    result->VerifyApiCallResultType();
    if (!is_construct || result->IsJSReceiver())
      return handle(*result, isolate);
  }

  return js_receiver;
}

}  // anonymous namespace

BUILTIN(HandleApiCall) {
  HandleScope scope(isolate);
  Handle<JSFunction> function = args.target();
  Handle<Object> receiver = args.receiver();
  Handle<HeapObject> new_target = args.new_target();
  Handle<FunctionTemplateInfo> fun_data(function->shared()->get_api_func_data(),
                                        isolate);
  if (new_target->IsJSReceiver()) {
    RETURN_RESULT_OR_FAILURE(
        isolate, HandleApiCallHelper<true>(isolate, function, new_target,
                                           fun_data, receiver, args));
  } else {
    RETURN_RESULT_OR_FAILURE(
        isolate, HandleApiCallHelper<false>(isolate, function, new_target,
                                            fun_data, receiver, args));
  }
}

namespace {

class RelocatableArguments : public BuiltinArguments, public Relocatable {
 public:
  RelocatableArguments(Isolate* isolate, int length, Object** arguments)
      : BuiltinArguments(length, arguments), Relocatable(isolate) {}

  virtual inline void IterateInstance(RootVisitor* v) {
    if (length() == 0) return;
    v->VisitRootPointers(Root::kRelocatable, lowest_address(),
                         highest_address() + 1);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(RelocatableArguments);
};

}  // namespace

MaybeHandle<Object> Builtins::InvokeApiFunction(Isolate* isolate,
                                                bool is_construct,
                                                Handle<HeapObject> function,
                                                Handle<Object> receiver,
                                                int argc, Handle<Object> args[],
                                                Handle<HeapObject> new_target) {
  DCHECK(function->IsFunctionTemplateInfo() ||
         (function->IsJSFunction() &&
          JSFunction::cast(*function)->shared()->IsApiFunction()));

  // Do proper receiver conversion for non-strict mode api functions.
  if (!is_construct && !receiver->IsJSReceiver()) {
    if (function->IsFunctionTemplateInfo() ||
        is_sloppy(JSFunction::cast(*function)->shared()->language_mode())) {
      ASSIGN_RETURN_ON_EXCEPTION(isolate, receiver,
                                 Object::ConvertReceiver(isolate, receiver),
                                 Object);
    }
  }

  Handle<FunctionTemplateInfo> fun_data =
      function->IsFunctionTemplateInfo()
          ? Handle<FunctionTemplateInfo>::cast(function)
          : handle(JSFunction::cast(*function)->shared()->get_api_func_data(),
                   isolate);
  // Construct BuiltinArguments object:
  // new target, function, arguments reversed, receiver.
  const int kBufferSize = 32;
  Object* small_argv[kBufferSize];
  Object** argv;
  const int frame_argc = argc + BuiltinArguments::kNumExtraArgsWithReceiver;
  if (frame_argc <= kBufferSize) {
    argv = small_argv;
  } else {
    argv = new Object*[frame_argc];
  }
  int cursor = frame_argc - 1;
  argv[cursor--] = *receiver;
  for (int i = 0; i < argc; ++i) {
    argv[cursor--] = *args[i];
  }
  DCHECK_EQ(cursor, BuiltinArguments::kPaddingOffset);
  argv[BuiltinArguments::kPaddingOffset] = isolate->heap()->the_hole_value();
  argv[BuiltinArguments::kArgcOffset] = Smi::FromInt(frame_argc);
  argv[BuiltinArguments::kTargetOffset] = *function;
  argv[BuiltinArguments::kNewTargetOffset] = *new_target;
  MaybeHandle<Object> result;
  {
    RelocatableArguments arguments(isolate, frame_argc, &argv[frame_argc - 1]);
    if (is_construct) {
      result = HandleApiCallHelper<true>(isolate, function, new_target,
                                         fun_data, receiver, arguments);
    } else {
      result = HandleApiCallHelper<false>(isolate, function, new_target,
                                          fun_data, receiver, arguments);
    }
  }
  if (argv != small_argv) delete[] argv;
  return result;
}

// Helper function to handle calls to non-function objects created through the
// API. The object can be called as either a constructor (using new) or just as
// a function (without new).
MUST_USE_RESULT static Object* HandleApiCallAsFunctionOrConstructor(
    Isolate* isolate, bool is_construct_call, BuiltinArguments args) {
  Handle<Object> receiver = args.receiver();

  // Get the object called.
  JSObject* obj = JSObject::cast(*receiver);

  // Set the new target.
  HeapObject* new_target;
  if (is_construct_call) {
    // TODO(adamk): This should be passed through in args instead of
    // being patched in here. We need to set a non-undefined value
    // for v8::FunctionCallbackInfo::IsConstructCall() to get the
    // right answer.
    new_target = obj;
  } else {
    new_target = isolate->heap()->undefined_value();
  }

  // Get the invocation callback from the function descriptor that was
  // used to create the called object.
  DCHECK(obj->map()->is_callable());
  JSFunction* constructor = JSFunction::cast(obj->map()->GetConstructor());
  // TODO(ishell): turn this back to a DCHECK.
  CHECK(constructor->shared()->IsApiFunction());
  Object* handler =
      constructor->shared()->get_api_func_data()->instance_call_handler();
  DCHECK(!handler->IsUndefined(isolate));
  // TODO(ishell): remove this debugging code.
  CHECK(handler->IsCallHandlerInfo());
  CallHandlerInfo* call_data = CallHandlerInfo::cast(handler);
  Object* callback_obj = call_data->callback();
  v8::FunctionCallback callback =
      v8::ToCData<v8::FunctionCallback>(callback_obj);

  // Get the data for the call and perform the callback.
  Object* result;
  {
    HandleScope scope(isolate);
    LOG(isolate, ApiObjectAccess("call non-function", obj));

    FunctionCallbackArguments custom(isolate, call_data->data(), constructor,
                                     obj, new_target, &args[0] - 1,
                                     args.length() - 1);
    Handle<Object> result_handle = custom.Call(callback);
    if (result_handle.is_null()) {
      result = isolate->heap()->undefined_value();
    } else {
      result = *result_handle;
    }
  }
  // Check for exceptions and return result.
  RETURN_FAILURE_IF_SCHEDULED_EXCEPTION(isolate);
  return result;
}

// Handle calls to non-function objects created through the API. This delegate
// function is used when the call is a normal function call.
BUILTIN(HandleApiCallAsFunction) {
  return HandleApiCallAsFunctionOrConstructor(isolate, false, args);
}

// Handle calls to non-function objects created through the API. This delegate
// function is used when the call is a construct call.
BUILTIN(HandleApiCallAsConstructor) {
  return HandleApiCallAsFunctionOrConstructor(isolate, true, args);
}

}  // namespace internal
}  // namespace v8
