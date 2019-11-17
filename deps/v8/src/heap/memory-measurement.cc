// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-measurement.h"

#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory-inl.h"
#include "src/heap/factory.h"
#include "src/objects/js-promise.h"

namespace v8 {
namespace internal {

MemoryMeasurement::MemoryMeasurement(Isolate* isolate) : isolate_(isolate) {}

namespace {

class MemoryMeasurementResultBuilder {
 public:
  MemoryMeasurementResultBuilder(Isolate* isolate, Factory* factory)
      : isolate_(isolate), factory_(factory) {
    result_ = NewJSObject();
  }

  void AddTotals(size_t estimate, size_t lower_bound, size_t upper_bound) {
    Handle<JSObject> total = NewJSObject();
    Handle<Object> estimate_obj = NewNumber(estimate);
    AddProperty(total, factory_->jsMemoryEstimate_string(), estimate_obj);
    Handle<Object> range = NewRange(lower_bound, upper_bound);
    AddProperty(total, factory_->jsMemoryRange_string(), range);
    AddProperty(result_, factory_->total_string(), total);
  }

  Handle<JSObject> Build() { return result_; }

 private:
  Handle<Object> NewNumber(size_t value) {
    return factory_->NewNumberFromSize(value);
  }

  Handle<JSObject> NewJSObject() {
    return factory_->NewJSObject(isolate_->object_function());
  }

  Handle<JSArray> NewRange(size_t lower_bound, size_t upper_bound) {
    Handle<Object> lower = NewNumber(lower_bound);
    Handle<Object> upper = NewNumber(upper_bound);
    Handle<FixedArray> elements = factory_->NewFixedArray(2);
    elements->set(0, *lower);
    elements->set(1, *upper);
    return factory_->NewJSArrayWithElements(elements);
  }

  void AddProperty(Handle<JSObject> object, Handle<String> name,
                   Handle<Object> value) {
    JSObject::AddProperty(isolate_, object, name, value, NONE);
  }

  Isolate* isolate_;
  Factory* factory_;
  Handle<JSObject> result_;
};

}  // anonymous namespace

Handle<JSPromise> MemoryMeasurement::EnqueueRequest(
    Handle<NativeContext> context, v8::MeasureMemoryMode mode) {
  Handle<JSPromise> promise = isolate_->factory()->NewJSPromise();
  MemoryMeasurementResultBuilder result_builder(isolate_, isolate_->factory());
  result_builder.AddTotals(isolate_->heap()->SizeOfObjects(), 0,
                           isolate_->heap()->SizeOfObjects());
  Handle<JSObject> result = result_builder.Build();
  JSPromise::Resolve(promise, result).ToHandleChecked();
  return promise;
}

}  // namespace internal
}  // namespace v8
