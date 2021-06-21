// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/d8/d8.h"

#include "include/v8-fast-api-calls.h"

// This file exposes a d8.test.fast_c_api object, which adds testing facility
// for writing mjsunit tests that exercise fast API calls. The fast_c_api object
// contains an `add_all` method with the following signature:
// double add_all(bool /*should_fallback*/, int32_t, uint32_t,
//   int64_t, uint64_t, float, double), that is wired as a "fast API" method.
// The fast_c_api object also supports querying the number of fast/slow calls
// and resetting these counters.

// Make sure to sync the following with src/compiler/globals.h.
#if defined(V8_TARGET_ARCH_X64)
#define V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
#endif

namespace v8 {
namespace {
class FastCApiObject {
 public:
  static double AddAllFastCallback(ApiObject receiver, bool should_fallback,
                                   int32_t arg_i32, uint32_t arg_u32,
                                   int64_t arg_i64, uint64_t arg_u64,
                                   float arg_f32, double arg_f64,
                                   FastApiCallbackOptions& options) {
    Value* receiver_value = reinterpret_cast<Value*>(&receiver);
    CHECK(receiver_value->IsObject());
    FastCApiObject* self = UnwrapObject(Object::Cast(receiver_value));
    self->fast_call_count_++;

    if (should_fallback) {
      options.fallback = 1;
      return 0;
    }

    return static_cast<double>(arg_i32) + static_cast<double>(arg_u32) +
           static_cast<double>(arg_i64) + static_cast<double>(arg_u64) +
           static_cast<double>(arg_f32) + arg_f64;
  }
  static void AddAllSlowCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    FastCApiObject* self = UnwrapObject(*args.This());
    self->slow_call_count_++;

    HandleScope handle_scope(isolate);

    double sum = 0;
    if (args.Length() > 1) {
      sum += args[1]->Int32Value(isolate->GetCurrentContext()).FromJust();
    }
    if (args.Length() > 2) {
      sum += args[2]->Uint32Value(isolate->GetCurrentContext()).FromJust();
    }
    if (args.Length() > 3) {
      sum += args[3]->IntegerValue(isolate->GetCurrentContext()).FromJust();
    }
    if (args.Length() > 4) {
      sum += args[4]->IntegerValue(isolate->GetCurrentContext()).FromJust();
    }
    if (args.Length() > 5) {
      sum += args[5]->NumberValue(isolate->GetCurrentContext()).FromJust();
    } else {
      sum += std::numeric_limits<double>::quiet_NaN();
    }
    if (args.Length() > 6) {
      sum += args[6]->NumberValue(isolate->GetCurrentContext()).FromJust();
    } else {
      sum += std::numeric_limits<double>::quiet_NaN();
    }

    args.GetReturnValue().Set(Number::New(isolate, sum));
  }

  static int Add32BitIntFastCallback(ApiObject receiver, bool should_fallback,
                                     int32_t arg_i32, uint32_t arg_u32,
                                     FastApiCallbackOptions& options) {
    Value* receiver_value = reinterpret_cast<Value*>(&receiver);
    CHECK(receiver_value->IsObject());
    FastCApiObject* self = UnwrapObject(Object::Cast(receiver_value));
    self->fast_call_count_++;

    if (should_fallback) {
      options.fallback = 1;
      return 0;
    }

    return arg_i32 + arg_u32;
  }
  static void Add32BitIntSlowCallback(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = args.GetIsolate();

    FastCApiObject* self = UnwrapObject(*args.This());
    self->slow_call_count_++;

    HandleScope handle_scope(isolate);

    double sum = 0;
    if (args.Length() > 1) {
      sum += args[1]->Int32Value(isolate->GetCurrentContext()).FromJust();
    }
    if (args.Length() > 2) {
      sum += args[2]->Uint32Value(isolate->GetCurrentContext()).FromJust();
    }

    args.GetReturnValue().Set(Number::New(isolate, sum));
  }

  static void FastCallCount(const FunctionCallbackInfo<Value>& args) {
    FastCApiObject* self = UnwrapObject(*args.This());
    args.GetReturnValue().Set(
        Number::New(args.GetIsolate(), self->fast_call_count()));
  }
  static void SlowCallCount(const FunctionCallbackInfo<Value>& args) {
    FastCApiObject* self = UnwrapObject(*args.This());
    args.GetReturnValue().Set(
        Number::New(args.GetIsolate(), self->slow_call_count()));
  }
  static void ResetCounts(const FunctionCallbackInfo<Value>& args) {
    FastCApiObject* self = UnwrapObject(*args.This());
    self->reset_counts();
    args.GetReturnValue().Set(Undefined(args.GetIsolate()));
  }
  static void SupportsFPParams(const FunctionCallbackInfo<Value>& info) {
    FastCApiObject* self = UnwrapObject(*info.This());
    info.GetReturnValue().Set(self->supports_fp_params_);
  }

  int fast_call_count() const { return fast_call_count_; }
  int slow_call_count() const { return slow_call_count_; }
  void reset_counts() {
    fast_call_count_ = 0;
    slow_call_count_ = 0;
  }

  static const int kV8WrapperObjectIndex = 1;

 private:
  static FastCApiObject* UnwrapObject(Object* object) {
    i::Address addr = *reinterpret_cast<i::Address*>(object);
    auto instance_type = i::Internals::GetInstanceType(addr);
    if (instance_type != i::Internals::kJSObjectType &&
        instance_type != i::Internals::kJSApiObjectType &&
        instance_type != i::Internals::kJSSpecialApiObjectType) {
      return nullptr;
    }
    FastCApiObject* wrapped = reinterpret_cast<FastCApiObject*>(
        object->GetAlignedPointerFromInternalField(kV8WrapperObjectIndex));
    CHECK_NOT_NULL(wrapped);
    return wrapped;
  }
  int fast_call_count_ = 0, slow_call_count_ = 0;
#ifdef V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
  bool supports_fp_params_ = true;
#else   // V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
  bool supports_fp_params_ = false;
#endif  // V8_ENABLE_FP_PARAMS_IN_C_LINKAGE
};

// The object is statically initialized for simplicity, typically the embedder
// will take care of managing their C++ objects lifetime.
thread_local FastCApiObject kFastCApiObject;
}  // namespace

// TODO(mslekova): Rename the fast_c_api helper to FastCAPI.
void CreateObject(const FunctionCallbackInfo<Value>& info) {
  if (!info.IsConstructCall()) {
    info.GetIsolate()->ThrowException(
        v8::Exception::Error(String::NewFromUtf8Literal(
            info.GetIsolate(),
            "FastCAPI helper must be constructed with new.")));
    return;
  }
  Local<Object> api_object = info.Holder();
  api_object->SetAlignedPointerInInternalField(
      FastCApiObject::kV8WrapperObjectIndex,
      reinterpret_cast<void*>(&kFastCApiObject));
  api_object->SetAccessorProperty(
      String::NewFromUtf8Literal(info.GetIsolate(), "supports_fp_params"),
      FunctionTemplate::New(info.GetIsolate(), FastCApiObject::SupportsFPParams)
          ->GetFunction(api_object->GetCreationContext().ToLocalChecked())
          .ToLocalChecked());
}

Local<FunctionTemplate> Shell::CreateTestFastCApiTemplate(Isolate* isolate) {
  Local<FunctionTemplate> api_obj_ctor =
      FunctionTemplate::New(isolate, CreateObject);
  Local<Signature> signature = Signature::New(isolate, api_obj_ctor);
  {
    CFunction add_all_c_func =
        CFunction::Make(FastCApiObject::AddAllFastCallback);
    api_obj_ctor->PrototypeTemplate()->Set(
        isolate, "add_all",
        FunctionTemplate::New(isolate, FastCApiObject::AddAllSlowCallback,
                              Local<Value>(), signature, 1,
                              ConstructorBehavior::kThrow,
                              SideEffectType::kHasSideEffect, &add_all_c_func));
    CFunction add_32bit_int_c_func =
        CFunction::Make(FastCApiObject::Add32BitIntFastCallback);
    api_obj_ctor->PrototypeTemplate()->Set(
        isolate, "add_32bit_int",
        FunctionTemplate::New(
            isolate, FastCApiObject::Add32BitIntSlowCallback, Local<Value>(),
            signature, 1, ConstructorBehavior::kThrow,
            SideEffectType::kHasSideEffect, &add_32bit_int_c_func));
    api_obj_ctor->PrototypeTemplate()->Set(
        isolate, "fast_call_count",
        FunctionTemplate::New(isolate, FastCApiObject::FastCallCount,
                              Local<Value>(), signature));
    api_obj_ctor->PrototypeTemplate()->Set(
        isolate, "slow_call_count",
        FunctionTemplate::New(isolate, FastCApiObject::SlowCallCount,
                              Local<Value>(), signature));
    api_obj_ctor->PrototypeTemplate()->Set(
        isolate, "reset_counts",
        FunctionTemplate::New(isolate, FastCApiObject::ResetCounts,
                              Local<Value>(), signature));
  }
  api_obj_ctor->InstanceTemplate()->SetInternalFieldCount(
      FastCApiObject::kV8WrapperObjectIndex + 1);

  return api_obj_ctor;
}

}  // namespace v8
