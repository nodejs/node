// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/memory-measurement.h"

#include "include/v8.h"
#include "src/api/api-inl.h"
#include "src/api/api.h"
#include "src/execution/isolate-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory-inl.h"
#include "src/heap/factory.h"
#include "src/heap/incremental-marking.h"
#include "src/heap/marking-worklist.h"
#include "src/logging/counters.h"
#include "src/objects/js-promise-inl.h"
#include "src/objects/js-promise.h"
#include "src/tasks/task-utils.h"

namespace v8 {
namespace internal {

namespace {
class MemoryMeasurementResultBuilder {
 public:
  MemoryMeasurementResultBuilder(Isolate* isolate, Factory* factory)
      : isolate_(isolate), factory_(factory) {
    result_ = NewJSObject();
  }
  void AddTotal(size_t estimate, size_t lower_bound, size_t upper_bound) {
    AddProperty(result_, factory_->total_string(),
                NewResult(estimate, lower_bound, upper_bound));
  }
  void AddCurrent(size_t estimate, size_t lower_bound, size_t upper_bound) {
    detailed_ = true;
    AddProperty(result_, factory_->current_string(),
                NewResult(estimate, lower_bound, upper_bound));
  }
  void AddOther(size_t estimate, size_t lower_bound, size_t upper_bound) {
    detailed_ = true;
    other_.push_back(NewResult(estimate, lower_bound, upper_bound));
  }
  Handle<JSObject> Build() {
    if (detailed_) {
      int length = static_cast<int>(other_.size());
      Handle<FixedArray> other = factory_->NewFixedArray(length);
      for (int i = 0; i < length; i++) {
        other->set(i, *other_[i]);
      }
      AddProperty(result_, factory_->other_string(),
                  factory_->NewJSArrayWithElements(other));
    }
    return result_;
  }

 private:
  Handle<JSObject> NewResult(size_t estimate, size_t lower_bound,
                             size_t upper_bound) {
    Handle<JSObject> result = NewJSObject();
    Handle<Object> estimate_obj = NewNumber(estimate);
    AddProperty(result, factory_->jsMemoryEstimate_string(), estimate_obj);
    Handle<Object> range = NewRange(lower_bound, upper_bound);
    AddProperty(result, factory_->jsMemoryRange_string(), range);
    return result;
  }
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
  std::vector<Handle<JSObject>> other_;
  bool detailed_ = false;
};
}  // anonymous namespace

class V8_EXPORT_PRIVATE MeasureMemoryDelegate
    : public v8::MeasureMemoryDelegate {
 public:
  MeasureMemoryDelegate(Isolate* isolate, Handle<NativeContext> context,
                        Handle<JSPromise> promise, v8::MeasureMemoryMode mode);
  ~MeasureMemoryDelegate();

  // v8::MeasureMemoryDelegate overrides:
  bool ShouldMeasure(v8::Local<v8::Context> context) override;
  void MeasurementComplete(
      const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
          context_sizes_in_bytes,
      size_t unattributed_size_in_bytes) override;

 private:
  Isolate* isolate_;
  Handle<JSPromise> promise_;
  Handle<NativeContext> context_;
  v8::MeasureMemoryMode mode_;
};

MeasureMemoryDelegate::MeasureMemoryDelegate(Isolate* isolate,
                                             Handle<NativeContext> context,
                                             Handle<JSPromise> promise,
                                             v8::MeasureMemoryMode mode)
    : isolate_(isolate), mode_(mode) {
  context_ = isolate->global_handles()->Create(*context);
  promise_ = isolate->global_handles()->Create(*promise);
}

MeasureMemoryDelegate::~MeasureMemoryDelegate() {
  isolate_->global_handles()->Destroy(promise_.location());
  isolate_->global_handles()->Destroy(context_.location());
}

bool MeasureMemoryDelegate::ShouldMeasure(v8::Local<v8::Context> context) {
  Handle<NativeContext> native_context =
      Handle<NativeContext>::cast(Utils::OpenHandle(*context));
  return context_->security_token() == native_context->security_token();
}

void MeasureMemoryDelegate::MeasurementComplete(
    const std::vector<std::pair<v8::Local<v8::Context>, size_t>>&
        context_sizes_in_bytes,
    size_t shared_size) {
  v8::Local<v8::Context> v8_context =
      Utils::Convert<HeapObject, v8::Context>(context_);
  v8::Context::Scope scope(v8_context);
  size_t total_size = 0;
  size_t current_size = 0;
  for (const auto& context_and_size : context_sizes_in_bytes) {
    total_size += context_and_size.second;
    if (*Utils::OpenHandle(*context_and_size.first) == *context_) {
      current_size = context_and_size.second;
    }
  }
  MemoryMeasurementResultBuilder result_builder(isolate_, isolate_->factory());
  result_builder.AddTotal(total_size, total_size, total_size + shared_size);

  if (mode_ == v8::MeasureMemoryMode::kDetailed) {
    result_builder.AddCurrent(current_size, current_size,
                              current_size + shared_size);
    for (const auto& context_and_size : context_sizes_in_bytes) {
      if (*Utils::OpenHandle(*context_and_size.first) != *context_) {
        size_t other_size = context_and_size.second;
        result_builder.AddOther(other_size, other_size,
                                other_size + shared_size);
      }
    }
  }

  Handle<JSObject> result = result_builder.Build();
  JSPromise::Resolve(promise_, result).ToHandleChecked();
}

MemoryMeasurement::MemoryMeasurement(Isolate* isolate) : isolate_(isolate) {}

bool MemoryMeasurement::EnqueueRequest(
    std::unique_ptr<v8::MeasureMemoryDelegate> delegate,
    v8::MeasureMemoryExecution execution,
    const std::vector<Handle<NativeContext>> contexts) {
  int length = static_cast<int>(contexts.size());
  Handle<WeakFixedArray> weak_contexts =
      isolate_->factory()->NewWeakFixedArray(length);
  for (int i = 0; i < length; ++i) {
    weak_contexts->Set(i, HeapObjectReference::Weak(*contexts[i]));
  }
  Handle<WeakFixedArray> global_weak_contexts =
      isolate_->global_handles()->Create(*weak_contexts);
  Request request = {std::move(delegate),
                     global_weak_contexts,
                     std::vector<size_t>(length),
                     0u,
                     {}};
  request.timer.Start();
  received_.push_back(std::move(request));
  ScheduleGCTask(execution);
  return true;
}

std::vector<Address> MemoryMeasurement::StartProcessing() {
  if (received_.empty()) return {};
  std::unordered_set<Address> unique_contexts;
  DCHECK(processing_.empty());
  processing_ = std::move(received_);
  for (const auto& request : processing_) {
    Handle<WeakFixedArray> contexts = request.contexts;
    for (int i = 0; i < contexts->length(); i++) {
      HeapObject context;
      if (contexts->Get(i).GetHeapObject(&context)) {
        unique_contexts.insert(context.ptr());
      }
    }
  }
  return std::vector<Address>(unique_contexts.begin(), unique_contexts.end());
}

void MemoryMeasurement::FinishProcessing(const NativeContextStats& stats) {
  if (processing_.empty()) return;

  while (!processing_.empty()) {
    Request request = std::move(processing_.front());
    processing_.pop_front();
    for (int i = 0; i < static_cast<int>(request.sizes.size()); i++) {
      HeapObject context;
      if (!request.contexts->Get(i).GetHeapObject(&context)) {
        continue;
      }
      request.sizes[i] = stats.Get(context.ptr());
    }
    request.shared = stats.Get(MarkingWorklists::kSharedContext);
    done_.push_back(std::move(request));
  }
  ScheduleReportingTask();
}

void MemoryMeasurement::ScheduleReportingTask() {
  if (reporting_task_pending_) return;
  reporting_task_pending_ = true;
  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
      reinterpret_cast<v8::Isolate*>(isolate_));
  taskrunner->PostTask(MakeCancelableTask(isolate_, [this] {
    reporting_task_pending_ = false;
    ReportResults();
  }));
}

bool MemoryMeasurement::IsGCTaskPending(v8::MeasureMemoryExecution execution) {
  return execution == v8::MeasureMemoryExecution::kEager
             ? eager_gc_task_pending_
             : delayed_gc_task_pending_;
}

void MemoryMeasurement::SetGCTaskPending(v8::MeasureMemoryExecution execution) {
  if (execution == v8::MeasureMemoryExecution::kEager) {
    eager_gc_task_pending_ = true;
  } else {
    delayed_gc_task_pending_ = true;
  }
}

void MemoryMeasurement::SetGCTaskDone(v8::MeasureMemoryExecution execution) {
  if (execution == v8::MeasureMemoryExecution::kEager) {
    eager_gc_task_pending_ = false;
  } else {
    delayed_gc_task_pending_ = false;
  }
}

void MemoryMeasurement::ScheduleGCTask(v8::MeasureMemoryExecution execution) {
  if (IsGCTaskPending(execution)) return;
  SetGCTaskPending(execution);
  auto taskrunner = V8::GetCurrentPlatform()->GetForegroundTaskRunner(
      reinterpret_cast<v8::Isolate*>(isolate_));
  auto task = MakeCancelableTask(isolate_, [this, execution] {
    SetGCTaskDone(execution);
    if (received_.empty()) return;
    Heap* heap = isolate_->heap();
    if (FLAG_incremental_marking) {
      if (heap->incremental_marking()->IsStopped()) {
        heap->StartIncrementalMarking(Heap::kNoGCFlags,
                                      GarbageCollectionReason::kMeasureMemory);
      } else {
        if (execution == v8::MeasureMemoryExecution::kEager) {
          heap->FinalizeIncrementalMarkingAtomically(
              GarbageCollectionReason::kMeasureMemory);
        }
        ScheduleGCTask(execution);
      }
    } else {
      heap->CollectGarbage(OLD_SPACE, GarbageCollectionReason::kMeasureMemory);
    }
  });
  if (execution == v8::MeasureMemoryExecution::kEager) {
    taskrunner->PostTask(std::move(task));
  } else {
    taskrunner->PostDelayedTask(std::move(task), kGCTaskDelayInSeconds);
  }
}

void MemoryMeasurement::ReportResults() {
  while (!done_.empty()) {
    Request request = std::move(done_.front());
    done_.pop_front();
    HandleScope handle_scope(isolate_);
    std::vector<std::pair<v8::Local<v8::Context>, size_t>> sizes;
    DCHECK_EQ(request.sizes.size(),
              static_cast<size_t>(request.contexts->length()));
    for (int i = 0; i < request.contexts->length(); i++) {
      HeapObject raw_context;
      if (!request.contexts->Get(i).GetHeapObject(&raw_context)) {
        continue;
      }
      v8::Local<v8::Context> context = Utils::Convert<HeapObject, v8::Context>(
          handle(raw_context, isolate_));
      sizes.push_back(std::make_pair(context, request.sizes[i]));
    }
    request.delegate->MeasurementComplete(sizes, request.shared);
    isolate_->counters()->measure_memory_delay_ms()->AddSample(
        static_cast<int>(request.timer.Elapsed().InMilliseconds()));
  }
}

std::unique_ptr<v8::MeasureMemoryDelegate> MemoryMeasurement::DefaultDelegate(
    Isolate* isolate, Handle<NativeContext> context, Handle<JSPromise> promise,
    v8::MeasureMemoryMode mode) {
  return std::make_unique<MeasureMemoryDelegate>(isolate, context, promise,
                                                 mode);
}

bool NativeContextInferrer::InferForJSFunction(JSFunction function,
                                               Address* native_context) {
  if (function.has_context()) {
    *native_context = function.context().native_context().ptr();
    return true;
  }
  return false;
}

bool NativeContextInferrer::InferForJSObject(Isolate* isolate, Map map,
                                             JSObject object,
                                             Address* native_context) {
  if (map.instance_type() == JS_GLOBAL_OBJECT_TYPE) {
    Object maybe_context =
        JSGlobalObject::cast(object).native_context_unchecked(isolate);
    if (maybe_context.IsNativeContext()) {
      *native_context = maybe_context.ptr();
      return true;
    }
  }
  // The maximum number of steps to perform when looking for the context.
  const int kMaxSteps = 3;
  Object maybe_constructor = map.TryGetConstructor(isolate, kMaxSteps);
  if (maybe_constructor.IsJSFunction()) {
    return InferForJSFunction(JSFunction::cast(maybe_constructor),
                              native_context);
  }
  return false;
}

void NativeContextStats::Clear() { size_by_context_.clear(); }

void NativeContextStats::Merge(const NativeContextStats& other) {
  for (const auto& it : other.size_by_context_) {
    size_by_context_[it.first] += it.second;
  }
}

void NativeContextStats::IncrementExternalSize(Address context, Map map,
                                               HeapObject object) {
  InstanceType instance_type = map.instance_type();
  size_t external_size = 0;
  if (instance_type == JS_ARRAY_BUFFER_TYPE) {
    external_size = JSArrayBuffer::cast(object).allocation_length();
  } else {
    DCHECK(InstanceTypeChecker::IsExternalString(instance_type));
    external_size = ExternalString::cast(object).ExternalPayloadSize();
  }
  size_by_context_[context] += external_size;
}

}  // namespace internal
}  // namespace v8
