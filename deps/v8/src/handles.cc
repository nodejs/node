// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles.h"

#include "src/address-map.h"
#include "src/api.h"
#include "src/base/logging.h"
#include "src/identity-map.h"
#include "src/maybe-handles.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// Handles should be trivially copyable so that they can be efficiently passed
// by value. If they are not trivially copyable, they cannot be passed in
// registers.
ASSERT_TRIVIALLY_COPYABLE(HandleBase);
ASSERT_TRIVIALLY_COPYABLE(Handle<Object>);
ASSERT_TRIVIALLY_COPYABLE(MaybeHandle<Object>);

#ifdef DEBUG
bool HandleBase::IsDereferenceAllowed(DereferenceCheckMode mode) const {
  DCHECK_NOT_NULL(location_);
  Object* object = *location_;
  if (object->IsSmi()) return true;
  HeapObject* heap_object = HeapObject::cast(object);
  Isolate* isolate;
  if (!Isolate::FromWritableHeapObject(heap_object, &isolate)) return true;
  Heap* heap = isolate->heap();
  RootIndex root_index;
  if (heap->IsRootHandleLocation(location_, &root_index) &&
      heap->RootCanBeTreatedAsConstant(root_index)) {
    return true;
  }
  if (!AllowHandleDereference::IsAllowed()) return false;
  if (mode == INCLUDE_DEFERRED_CHECK &&
      !AllowDeferredHandleDereference::IsAllowed()) {
    // Accessing cells, maps and internalized strings is safe.
    if (heap_object->IsCell()) return true;
    if (heap_object->IsMap()) return true;
    if (heap_object->IsInternalizedString()) return true;
    return !isolate->IsDeferredHandle(location_);
  }
  return true;
}
#endif


int HandleScope::NumberOfHandles(Isolate* isolate) {
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  int n = static_cast<int>(impl->blocks()->size());
  if (n == 0) return 0;
  return ((n - 1) * kHandleBlockSize) +
         static_cast<int>(
             (isolate->handle_scope_data()->next - impl->blocks()->back()));
}


Object** HandleScope::Extend(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();

  Object** result = current->next;

  DCHECK(result == current->limit);
  // Make sure there's at least one scope on the stack and that the
  // top of the scope stack isn't a barrier.
  if (!Utils::ApiCheck(current->level != current->sealed_level,
                       "v8::HandleScope::CreateHandle()",
                       "Cannot create a handle without a HandleScope")) {
    return nullptr;
  }
  HandleScopeImplementer* impl = isolate->handle_scope_implementer();
  // If there's more room in the last block, we use that. This is used
  // for fast creation of scopes after scope barriers.
  if (!impl->blocks()->empty()) {
    Object** limit = &impl->blocks()->back()[kHandleBlockSize];
    if (current->limit != limit) {
      current->limit = limit;
      DCHECK_LT(limit - current->next, kHandleBlockSize);
    }
  }

  // If we still haven't found a slot for the handle, we extend the
  // current handle scope by allocating a new handle block.
  if (result == current->limit) {
    // If there's a spare block, use it for growing the current scope.
    result = impl->GetSpareOrNewBlock();
    // Add the extension to the global list of blocks, but count the
    // extension as part of the current scope.
    impl->blocks()->push_back(result);
    current->limit = &result[kHandleBlockSize];
  }

  return result;
}


void HandleScope::DeleteExtensions(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();
  isolate->handle_scope_implementer()->DeleteExtensions(current->limit);
}


#ifdef ENABLE_HANDLE_ZAPPING
void HandleScope::ZapRange(Object** start, Object** end) {
  DCHECK_LE(end - start, kHandleBlockSize);
  for (Object** p = start; p != end; p++) {
    *reinterpret_cast<Address*>(p) = static_cast<Address>(kHandleZapValue);
  }
}
#endif


Address HandleScope::current_level_address(Isolate* isolate) {
  return reinterpret_cast<Address>(&isolate->handle_scope_data()->level);
}


Address HandleScope::current_next_address(Isolate* isolate) {
  return reinterpret_cast<Address>(&isolate->handle_scope_data()->next);
}


Address HandleScope::current_limit_address(Isolate* isolate) {
  return reinterpret_cast<Address>(&isolate->handle_scope_data()->limit);
}

CanonicalHandleScope::CanonicalHandleScope(Isolate* isolate)
    : isolate_(isolate), zone_(isolate->allocator(), ZONE_NAME) {
  HandleScopeData* handle_scope_data = isolate_->handle_scope_data();
  prev_canonical_scope_ = handle_scope_data->canonical_scope;
  handle_scope_data->canonical_scope = this;
  root_index_map_ = new RootIndexMap(isolate);
  identity_map_ = new IdentityMap<Object**, ZoneAllocationPolicy>(
      isolate->heap(), ZoneAllocationPolicy(&zone_));
  canonical_level_ = handle_scope_data->level;
}


CanonicalHandleScope::~CanonicalHandleScope() {
  delete root_index_map_;
  delete identity_map_;
  isolate_->handle_scope_data()->canonical_scope = prev_canonical_scope_;
}


Object** CanonicalHandleScope::Lookup(Object* object) {
  DCHECK_LE(canonical_level_, isolate_->handle_scope_data()->level);
  if (isolate_->handle_scope_data()->level != canonical_level_) {
    // We are in an inner handle scope. Do not canonicalize since we will leave
    // this handle scope while still being in the canonical scope.
    return HandleScope::CreateHandle(isolate_, object);
  }
  if (object->IsHeapObject()) {
    RootIndex root_index;
    if (root_index_map_->Lookup(HeapObject::cast(object), &root_index)) {
      return isolate_->heap()->root_handle(root_index).location();
    }
  }
  Object*** entry = identity_map_->Get(object);
  if (*entry == nullptr) {
    // Allocate new handle location.
    *entry = HandleScope::CreateHandle(isolate_, object);
  }
  return reinterpret_cast<Object**>(*entry);
}


DeferredHandleScope::DeferredHandleScope(Isolate* isolate)
    : impl_(isolate->handle_scope_implementer()) {
  impl_->BeginDeferredScope();
  HandleScopeData* data = impl_->isolate()->handle_scope_data();
  Object** new_next = impl_->GetSpareOrNewBlock();
  Object** new_limit = &new_next[kHandleBlockSize];
  // Check that at least one HandleScope with at least one Handle in it exists,
  // see the class description.
  DCHECK(!impl_->blocks()->empty());
  // Check that we are not in a SealedHandleScope.
  DCHECK(data->limit == &impl_->blocks()->back()[kHandleBlockSize]);
  impl_->blocks()->push_back(new_next);

#ifdef DEBUG
  prev_level_ = data->level;
#endif
  data->level++;
  prev_limit_ = data->limit;
  prev_next_ = data->next;
  data->next = new_next;
  data->limit = new_limit;
}


DeferredHandleScope::~DeferredHandleScope() {
  impl_->isolate()->handle_scope_data()->level--;
  DCHECK(handles_detached_);
  DCHECK(impl_->isolate()->handle_scope_data()->level == prev_level_);
}


DeferredHandles* DeferredHandleScope::Detach() {
  DeferredHandles* deferred = impl_->Detach(prev_limit_);
  HandleScopeData* data = impl_->isolate()->handle_scope_data();
  data->next = prev_next_;
  data->limit = prev_limit_;
#ifdef DEBUG
  handles_detached_ = true;
#endif
  return deferred;
}

}  // namespace internal
}  // namespace v8
