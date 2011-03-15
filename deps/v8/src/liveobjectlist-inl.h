// Copyright 2011 the V8 project authors. All rights reserved.
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

#ifndef V8_LIVEOBJECTLIST_INL_H_
#define V8_LIVEOBJECTLIST_INL_H_

#include "v8.h"

#include "liveobjectlist.h"

namespace v8 {
namespace internal {

#ifdef LIVE_OBJECT_LIST

void LiveObjectList::GCEpilogue() {
  if (!NeedLOLProcessing()) return;
  GCEpiloguePrivate();
}


void LiveObjectList::GCPrologue() {
  if (!NeedLOLProcessing()) return;
#ifdef VERIFY_LOL
  if (FLAG_verify_lol) {
    Verify();
  }
#endif
}


void LiveObjectList::IterateElements(ObjectVisitor* v) {
  if (!NeedLOLProcessing()) return;
  IterateElementsPrivate(v);
}


void LiveObjectList::ProcessNonLive(HeapObject *obj) {
  // Only do work if we have at least one list to process.
  if (last()) DoProcessNonLive(obj);
}


void LiveObjectList::UpdateReferencesForScavengeGC() {
  if (LiveObjectList::NeedLOLProcessing()) {
    UpdateLiveObjectListVisitor update_visitor;
    LiveObjectList::IterateElements(&update_visitor);
  }
}


LiveObjectList* LiveObjectList::FindLolForId(int id,
                                             LiveObjectList* start_lol) {
  if (id != 0) {
    LiveObjectList* lol = start_lol;
    while (lol != NULL) {
      if (lol->id() == id) {
        return lol;
      }
      lol = lol->prev_;
    }
  }
  return NULL;
}


// Iterates the elements in every lol and returns the one that matches the
// specified key.  If no matching element is found, then it returns NULL.
template <typename T>
inline LiveObjectList::Element*
LiveObjectList::FindElementFor(T (*GetValue)(LiveObjectList::Element*), T key) {
  LiveObjectList *lol = last();
  while (lol != NULL) {
    Element* elements = lol->elements_;
    for (int i = 0; i < lol->obj_count_; i++) {
      Element* element = &elements[i];
      if (GetValue(element) == key) {
        return element;
      }
    }
    lol = lol->prev_;
  }
  return NULL;
}


inline int LiveObjectList::GetElementId(LiveObjectList::Element* element) {
  return element->id_;
}


inline HeapObject*
LiveObjectList::GetElementObj(LiveObjectList::Element* element) {
  return element->obj_;
}

#endif  // LIVE_OBJECT_LIST

} }  // namespace v8::internal

#endif  // V8_LIVEOBJECTLIST_INL_H_

