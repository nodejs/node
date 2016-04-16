// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/context-measure.h"

#include "src/base/logging.h"
#include "src/contexts.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

ContextMeasure::ContextMeasure(Context* context)
    : context_(context),
      root_index_map_(context->GetIsolate()),
      recursion_depth_(0),
      count_(0),
      size_(0) {
  DCHECK(context_->IsNativeContext());
  Object* next_link = context_->get(Context::NEXT_CONTEXT_LINK);
  MeasureObject(context_);
  MeasureDeferredObjects();
  context_->set(Context::NEXT_CONTEXT_LINK, next_link);
}


bool ContextMeasure::IsShared(HeapObject* object) {
  if (object->IsScript()) return true;
  if (object->IsSharedFunctionInfo()) return true;
  if (object->IsScopeInfo()) return true;
  if (object->IsCode() && !Code::cast(object)->is_optimized_code()) return true;
  if (object->IsAccessorInfo()) return true;
  if (object->IsWeakCell()) return true;
  return false;
}


void ContextMeasure::MeasureObject(HeapObject* object) {
  if (back_reference_map_.Lookup(object).is_valid()) return;
  if (root_index_map_.Lookup(object) != RootIndexMap::kInvalidRootIndex) return;
  if (IsShared(object)) return;
  back_reference_map_.Add(object, BackReference::DummyReference());
  recursion_depth_++;
  if (recursion_depth_ > kMaxRecursion) {
    deferred_objects_.Add(object);
  } else {
    MeasureAndRecurse(object);
  }
  recursion_depth_--;
}


void ContextMeasure::MeasureDeferredObjects() {
  while (deferred_objects_.length() > 0) {
    MeasureAndRecurse(deferred_objects_.RemoveLast());
  }
}


void ContextMeasure::MeasureAndRecurse(HeapObject* object) {
  int size = object->Size();
  count_++;
  size_ += size;
  Map* map = object->map();
  MeasureObject(map);
  object->IterateBody(map->instance_type(), size, this);
}


void ContextMeasure::VisitPointers(Object** start, Object** end) {
  for (Object** current = start; current < end; current++) {
    if ((*current)->IsSmi()) continue;
    MeasureObject(HeapObject::cast(*current));
  }
}
}  // namespace internal
}  // namespace v8
