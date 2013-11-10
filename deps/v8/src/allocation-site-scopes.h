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

#ifndef V8_ALLOCATION_SITE_SCOPES_H_
#define V8_ALLOCATION_SITE_SCOPES_H_

#include "ast.h"
#include "handles.h"
#include "objects.h"
#include "zone.h"

namespace v8 {
namespace internal {


// AllocationSiteContext is the base class for walking and copying a nested
// boilerplate with AllocationSite and AllocationMemento support.
class AllocationSiteContext {
 public:
  AllocationSiteContext(Isolate* isolate, bool activated) {
    isolate_ = isolate;
    activated_ = activated;
  };
  virtual ~AllocationSiteContext() {}

  Handle<AllocationSite> top() { return top_; }
  Handle<AllocationSite> current() { return current_; }

  // If activated, then recursively create mementos
  bool activated() const { return activated_; }

  // Returns the AllocationSite that matches this scope.
  virtual Handle<AllocationSite> EnterNewScope() = 0;

  // scope_site should be the handle returned by the matching EnterNewScope()
  virtual void ExitScope(Handle<AllocationSite> scope_site,
                         Handle<JSObject> object) = 0;

 protected:
  void update_current_site(AllocationSite* site) {
    *(current_.location()) = site;
  }

  Isolate* isolate() { return isolate_; }
  void InitializeTraversal(Handle<AllocationSite> site) {
    top_ = site;
    current_ = Handle<AllocationSite>(*top_, isolate());
  }

 private:
  Isolate* isolate_;
  Handle<AllocationSite> top_;
  Handle<AllocationSite> current_;
  bool activated_;
};


// AllocationSiteCreationContext aids in the creation of AllocationSites to
// accompany object literals.
class AllocationSiteCreationContext : public AllocationSiteContext {
 public:
  explicit AllocationSiteCreationContext(Isolate* isolate)
      : AllocationSiteContext(isolate, true) { }

  virtual Handle<AllocationSite> EnterNewScope() V8_OVERRIDE;
  virtual void ExitScope(Handle<AllocationSite> site,
                         Handle<JSObject> object) V8_OVERRIDE;
};


// AllocationSiteUsageContext aids in the creation of AllocationMementos placed
// behind some/all components of a copied object literal.
class AllocationSiteUsageContext : public AllocationSiteContext {
 public:
  AllocationSiteUsageContext(Isolate* isolate, Handle<AllocationSite> site,
                             bool activated)
      : AllocationSiteContext(isolate, activated),
        top_site_(site) { }

  virtual Handle<AllocationSite> EnterNewScope() V8_OVERRIDE;
  virtual void ExitScope(Handle<AllocationSite> site,
                         Handle<JSObject> object) V8_OVERRIDE;

 private:
  Handle<AllocationSite> top_site_;
};


} }  // namespace v8::internal

#endif  // V8_ALLOCATION_SITE_SCOPES_H_
