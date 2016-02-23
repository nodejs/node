// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/allocation-site-scopes.h"

namespace v8 {
namespace internal {


Handle<AllocationSite> AllocationSiteCreationContext::EnterNewScope() {
  Handle<AllocationSite> scope_site;
  if (top().is_null()) {
    // We are creating the top level AllocationSite as opposed to a nested
    // AllocationSite.
    InitializeTraversal(isolate()->factory()->NewAllocationSite());
    scope_site = Handle<AllocationSite>(*top(), isolate());
    if (FLAG_trace_creation_allocation_sites) {
      PrintF("*** Creating top level AllocationSite %p\n",
             static_cast<void*>(*scope_site));
    }
  } else {
    DCHECK(!current().is_null());
    scope_site = isolate()->factory()->NewAllocationSite();
    if (FLAG_trace_creation_allocation_sites) {
      PrintF("Creating nested site (top, current, new) (%p, %p, %p)\n",
             static_cast<void*>(*top()),
             static_cast<void*>(*current()),
             static_cast<void*>(*scope_site));
    }
    current()->set_nested_site(*scope_site);
    update_current_site(*scope_site);
  }
  DCHECK(!scope_site.is_null());
  return scope_site;
}


void AllocationSiteCreationContext::ExitScope(
    Handle<AllocationSite> scope_site,
    Handle<JSObject> object) {
  if (!object.is_null()) {
    bool top_level = !scope_site.is_null() &&
        top().is_identical_to(scope_site);

    scope_site->set_transition_info(*object);
    if (FLAG_trace_creation_allocation_sites) {
      if (top_level) {
        PrintF("*** Setting AllocationSite %p transition_info %p\n",
               static_cast<void*>(*scope_site),
               static_cast<void*>(*object));
      } else {
        PrintF("Setting AllocationSite (%p, %p) transition_info %p\n",
               static_cast<void*>(*top()),
               static_cast<void*>(*scope_site),
               static_cast<void*>(*object));
      }
    }
  }
}


bool AllocationSiteUsageContext::ShouldCreateMemento(Handle<JSObject> object) {
  if (activated_ && AllocationSite::CanTrack(object->map()->instance_type())) {
    if (FLAG_allocation_site_pretenuring ||
        AllocationSite::GetMode(object->GetElementsKind()) ==
        TRACK_ALLOCATION_SITE) {
      if (FLAG_trace_creation_allocation_sites) {
        PrintF("*** Creating Memento for %s %p\n",
               object->IsJSArray() ? "JSArray" : "JSObject",
               static_cast<void*>(*object));
      }
      return true;
    }
  }
  return false;
}

}  // namespace internal
}  // namespace v8
