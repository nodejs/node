// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/handles/handles.h"

#include "src/api/api.h"
#include "src/base/logging.h"
#include "src/codegen/optimized-compilation-info.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/objects-inl.h"
#include "src/roots/roots-inl.h"
#include "src/utils/address-map.h"
#include "src/utils/identity-map.h"

#ifdef DEBUG
// For GetIsolateFromWritableHeapObject.
#include "src/heap/heap-write-barrier-inl.h"
#endif

namespace v8 {
namespace internal {

// Handles should be trivially copyable so that they can be efficiently passed
// by value. If they are not trivially copyable, they cannot be passed in
// registers.
ASSERT_TRIVIALLY_COPYABLE(HandleBase);
ASSERT_TRIVIALLY_COPYABLE(Handle<Object>);
ASSERT_TRIVIALLY_COPYABLE(MaybeHandle<Object>);

#ifdef DEBUG
bool HandleBase::IsDereferenceAllowed() const {
  DCHECK_NOT_NULL(location_);
  Object object(*location_);
  if (object.IsSmi()) return true;
  HeapObject heap_object = HeapObject::cast(object);
  if (IsReadOnlyHeapObject(heap_object)) return true;
  Isolate* isolate = GetIsolateFromWritableObject(heap_object);
  RootIndex root_index;
  if (isolate->roots_table().IsRootHandleLocation(location_, &root_index) &&
      RootsTable::IsImmortalImmovable(root_index)) {
    return true;
  }
  if (isolate->IsBuiltinsTableHandleLocation(location_)) return true;

  LocalHeap* local_heap = LocalHeap::Current();
  if (FLAG_local_heaps && local_heap) {
    // Local heap can't access handles when parked
    if (!local_heap->IsHandleDereferenceAllowed()) return false;

    if (local_heap->ContainsPersistentHandle(location_) ||
        local_heap->ContainsLocalHandle(location_)) {
      // The current thread owns the handle and thus can dereference it.
      return true;
    }
  }

  return AllowHandleDereference::IsAllowed();
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

Address* HandleScope::Extend(Isolate* isolate) {
  HandleScopeData* current = isolate->handle_scope_data();

  Address* result = current->next;

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
    Address* limit = &impl->blocks()->back()[kHandleBlockSize];
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
void HandleScope::ZapRange(Address* start, Address* end) {
  DCHECK_LE(end - start, kHandleBlockSize);
  for (Address* p = start; p != end; p++) {
    *p = static_cast<Address>(kHandleZapValue);
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

CanonicalHandleScope::CanonicalHandleScope(Isolate* isolate,
                                           OptimizedCompilationInfo* info)
    : isolate_(isolate),
      info_(info),
      zone_(info ? info->zone() : new Zone(isolate->allocator(), ZONE_NAME)) {
  HandleScopeData* handle_scope_data = isolate_->handle_scope_data();
  prev_canonical_scope_ = handle_scope_data->canonical_scope;
  handle_scope_data->canonical_scope = this;
  root_index_map_ = new RootIndexMap(isolate);
  identity_map_ = std::make_unique<CanonicalHandlesMap>(
      isolate->heap(), ZoneAllocationPolicy(zone_));
  canonical_level_ = handle_scope_data->level;
}

CanonicalHandleScope::~CanonicalHandleScope() {
  delete root_index_map_;
  if (info_) {
    // If we passed a compilation info as parameter, we created the identity map
    // on its zone(). Then, we pass it to the compilation info which is
    // responsible for the disposal.
    info_->set_canonical_handles(DetachCanonicalHandles());
  } else {
    // If we don't have a compilation info, we created the zone manually. To
    // properly dispose of said zone, we need to first free the identity_map_.
    // Then we do so manually even though identity_map_ is a unique_ptr.
    identity_map_.reset();
    delete zone_;
  }
  isolate_->handle_scope_data()->canonical_scope = prev_canonical_scope_;
}

Address* CanonicalHandleScope::Lookup(Address object) {
  DCHECK_LE(canonical_level_, isolate_->handle_scope_data()->level);
  if (isolate_->handle_scope_data()->level != canonical_level_) {
    // We are in an inner handle scope. Do not canonicalize since we will leave
    // this handle scope while still being in the canonical scope.
    return HandleScope::CreateHandle(isolate_, object);
  }
  if (Internals::HasHeapObjectTag(object)) {
    RootIndex root_index;
    if (root_index_map_->Lookup(object, &root_index)) {
      return isolate_->root_handle(root_index).location();
    }
  }
  auto find_result = identity_map_->FindOrInsert(Object(object));
  if (!find_result.already_exists) {
    // Allocate new handle location.
    *find_result.entry = HandleScope::CreateHandle(isolate_, object);
  }
  return *find_result.entry;
}

std::unique_ptr<CanonicalHandlesMap>
CanonicalHandleScope::DetachCanonicalHandles() {
  return std::move(identity_map_);
}

}  // namespace internal
}  // namespace v8
