// Copyright 2013 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "allocation-site-scopes.h"

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
    ASSERT(!current().is_null());
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
  ASSERT(!scope_site.is_null());
  return scope_site;
}


void AllocationSiteCreationContext::ExitScope(
    Handle<AllocationSite> scope_site,
    Handle<JSObject> object) {
  if (!object.is_null() && !object->IsFailure()) {
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

} }  // namespace v8::internal
