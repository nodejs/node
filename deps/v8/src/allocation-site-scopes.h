// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ALLOCATION_SITE_SCOPES_H_
#define V8_ALLOCATION_SITE_SCOPES_H_

#include "src/ast/ast.h"
#include "src/handles.h"
#include "src/objects.h"
#include "src/zone.h"

namespace v8 {
namespace internal {


// AllocationSiteContext is the base class for walking and copying a nested
// boilerplate with AllocationSite and AllocationMemento support.
class AllocationSiteContext {
 public:
  explicit AllocationSiteContext(Isolate* isolate) {
    isolate_ = isolate;
  }

  Handle<AllocationSite> top() { return top_; }
  Handle<AllocationSite> current() { return current_; }

  bool ShouldCreateMemento(Handle<JSObject> object) { return false; }

  Isolate* isolate() { return isolate_; }

 protected:
  void update_current_site(AllocationSite* site) {
    *(current_.location()) = site;
  }

  void InitializeTraversal(Handle<AllocationSite> site) {
    top_ = site;
    current_ = Handle<AllocationSite>::New(*top_, isolate());
  }

 private:
  Isolate* isolate_;
  Handle<AllocationSite> top_;
  Handle<AllocationSite> current_;
};


// AllocationSiteCreationContext aids in the creation of AllocationSites to
// accompany object literals.
class AllocationSiteCreationContext : public AllocationSiteContext {
 public:
  explicit AllocationSiteCreationContext(Isolate* isolate)
      : AllocationSiteContext(isolate) { }

  Handle<AllocationSite> EnterNewScope();
  void ExitScope(Handle<AllocationSite> site, Handle<JSObject> object);
};


// AllocationSiteUsageContext aids in the creation of AllocationMementos placed
// behind some/all components of a copied object literal.
class AllocationSiteUsageContext : public AllocationSiteContext {
 public:
  AllocationSiteUsageContext(Isolate* isolate, Handle<AllocationSite> site,
                             bool activated)
      : AllocationSiteContext(isolate),
        top_site_(site),
        activated_(activated) { }

  inline Handle<AllocationSite> EnterNewScope() {
    if (top().is_null()) {
      InitializeTraversal(top_site_);
    } else {
      // Advance current site
      Object* nested_site = current()->nested_site();
      // Something is wrong if we advance to the end of the list here.
      update_current_site(AllocationSite::cast(nested_site));
    }
    return Handle<AllocationSite>(*current(), isolate());
  }

  inline void ExitScope(Handle<AllocationSite> scope_site,
                        Handle<JSObject> object) {
    // This assert ensures that we are pointing at the right sub-object in a
    // recursive walk of a nested literal.
    DCHECK(object.is_null() || *object == scope_site->transition_info());
  }

  bool ShouldCreateMemento(Handle<JSObject> object);

 private:
  Handle<AllocationSite> top_site_;
  bool activated_;
};


}  // namespace internal
}  // namespace v8

#endif  // V8_ALLOCATION_SITE_SCOPES_H_
