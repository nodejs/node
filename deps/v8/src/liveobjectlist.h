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

#ifndef V8_LIVEOBJECTLIST_H_
#define V8_LIVEOBJECTLIST_H_

#include "v8.h"

#include "checks.h"
#include "heap.h"
#include "objects.h"
#include "globals.h"

namespace v8 {
namespace internal {

#ifdef LIVE_OBJECT_LIST


// Temporary stubbed out LiveObjectList implementation.
class LiveObjectList {
 public:
  inline static void GCEpilogue() {}
  inline static void GCPrologue() {}
  inline static void IterateElements(ObjectVisitor* v) {}
  inline static void ProcessNonLive(HeapObject *obj) {}
  inline static void UpdateReferencesForScavengeGC() {}

  static MaybeObject* Capture() { return Heap::undefined_value(); }
  static bool Delete(int id) { return false; }
  static MaybeObject* Dump(int id1,
                           int id2,
                           int start_idx,
                           int dump_limit,
                           Handle<JSObject> filter_obj) {
    return Heap::undefined_value();
  }
  static MaybeObject* Info(int start_idx, int dump_limit) {
    return Heap::undefined_value();
  }
  static MaybeObject* Summarize(int id1,
                                int id2,
                                Handle<JSObject> filter_obj) {
    return Heap::undefined_value();
  }

  static void Reset() {}
  static Object* GetObj(int obj_id) { return Heap::undefined_value(); }
  static Object* GetObjId(Handle<String> address) {
    return Heap::undefined_value();
  }
  static MaybeObject* GetObjRetainers(int obj_id,
                                      Handle<JSObject> instance_filter,
                                      bool verbose,
                                      int start,
                                      int count,
                                      Handle<JSObject> filter_obj) {
    return Heap::undefined_value();
  }

  static Object* GetPath(int obj_id1,
                         int obj_id2,
                         Handle<JSObject> instance_filter) {
    return Heap::undefined_value();
  }
  static Object* PrintObj(int obj_id) { return Heap::undefined_value(); }
};


#else  // !LIVE_OBJECT_LIST


class LiveObjectList {
 public:
  static void GCEpilogue() {}
  static void GCPrologue() {}
  static void IterateElements(ObjectVisitor* v) {}
  static void ProcessNonLive(HeapObject *obj) {}
  static void UpdateReferencesForScavengeGC() {}
};


#endif  // LIVE_OBJECT_LIST

} }  // namespace v8::internal

#endif  // V8_LIVEOBJECTLIST_H_

