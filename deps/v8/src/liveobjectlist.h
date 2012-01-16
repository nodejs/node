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

#ifdef DEBUG
// The following symbol when defined enables thorough verification of lol data.
// FLAG_verify_lol will also need to set to true to enable the verification.
#define VERIFY_LOL
#endif


typedef int LiveObjectType;
class LolFilter;
class LiveObjectSummary;
class DumpWriter;
class SummaryWriter;


// The LiveObjectList is both a mechanism for tracking a live capture of
// objects in the JS heap, as well as is the data structure which represents
// each of those captures.  Unlike a snapshot, the lol is live.  For example,
// if an object in a captured lol dies and is collected by the GC, the lol
// will reflect that the object is no longer available.  The term
// LiveObjectList (and lol) is used to describe both the mechanism and the
// data structure depending on context of use.
//
// In captured lols, objects are tracked using their address and an object id.
// The object id is unique.  Once assigned to an object, the object id can never
// be assigned to another object.  That is unless all captured lols are deleted
// which allows the user to start over with a fresh set of lols and object ids.
// The uniqueness of the object ids allows the user to track specific objects
// and inspect its longevity while debugging JS code in execution.
//
// The lol comes with utility functions to capture, dump, summarize, and diff
// captured lols amongst other functionality.  These functionality are
// accessible via the v8 debugger interface.
class LiveObjectList {
 public:
  inline static void GCEpilogue();
  inline static void GCPrologue();
  inline static void IterateElements(ObjectVisitor* v);
  inline static void ProcessNonLive(HeapObject* obj);
  inline static void UpdateReferencesForScavengeGC();

  // Note: LOLs can be listed by calling Dump(0, <lol id>), and 2 LOLs can be
  // compared/diff'ed using Dump(<lol id1>, <lol id2>, ...).  This will yield
  // a verbose dump of all the objects in the resultant lists.
  //   Similarly, a summarized result of a LOL listing or a diff can be
  // attained using the Summarize(0, <lol id>) and Summarize(<lol id1,
  // <lol id2>, ...) respectively.

  static MaybeObject* Capture();
  static bool Delete(int id);
  static MaybeObject* Dump(int id1,
                           int id2,
                           int start_idx,
                           int dump_limit,
                           Handle<JSObject> filter_obj);
  static MaybeObject* Info(int start_idx, int dump_limit);
  static MaybeObject* Summarize(int id1, int id2, Handle<JSObject> filter_obj);

  static void Reset();
  static Object* GetObj(int obj_id);
  static int GetObjId(Object* obj);
  static Object* GetObjId(Handle<String> address);
  static MaybeObject* GetObjRetainers(int obj_id,
                                      Handle<JSObject> instance_filter,
                                      bool verbose,
                                      int start,
                                      int count,
                                      Handle<JSObject> filter_obj);

  static Object* GetPath(int obj_id1,
                         int obj_id2,
                         Handle<JSObject> instance_filter);
  static Object* PrintObj(int obj_id);

 private:
  struct Element {
    int id_;
    HeapObject* obj_;
  };

  explicit LiveObjectList(LiveObjectList* prev, int capacity);
  ~LiveObjectList();

  static void GCEpiloguePrivate();
  static void IterateElementsPrivate(ObjectVisitor* v);

  static void DoProcessNonLive(HeapObject* obj);

  static int CompareElement(const Element* a, const Element* b);

  static Object* GetPathPrivate(HeapObject* obj1, HeapObject* obj2);

  static int GetRetainers(Handle<HeapObject> target,
                          Handle<JSObject> instance_filter,
                          Handle<FixedArray> retainers_arr,
                          int start,
                          int dump_limit,
                          int* total_count,
                          LolFilter* filter,
                          LiveObjectSummary* summary,
                          JSFunction* arguments_function,
                          Handle<Object> error);

  static MaybeObject* DumpPrivate(DumpWriter* writer,
                                  int start,
                                  int dump_limit,
                                  LolFilter* filter);
  static MaybeObject* SummarizePrivate(SummaryWriter* writer,
                                       LolFilter* filter,
                                       bool is_tracking_roots);

  static bool NeedLOLProcessing() { return (last() != NULL); }
  static void NullifyNonLivePointer(HeapObject** p) {
    // Mask out the low bit that marks this as a heap object.  We'll use this
    // cleared bit as an indicator that this pointer needs to be collected.
    //
    // Meanwhile, we still preserve its approximate value so that we don't
    // have to resort the elements list all the time.
    //
    // Note: Doing so also makes this HeapObject* look like an SMI.  Hence,
    // GC pointer updater will ignore it when it gets scanned.
    *p = reinterpret_cast<HeapObject*>((*p)->address());
  }

  LiveObjectList* prev() { return prev_; }
  LiveObjectList* next() { return next_; }
  int id() { return id_; }

  static int list_count() { return list_count_; }
  static LiveObjectList* last() { return last_; }

  inline static LiveObjectList* FindLolForId(int id, LiveObjectList* start_lol);
  int TotalObjCount() { return GetTotalObjCountAndSize(NULL); }
  int GetTotalObjCountAndSize(int* size_p);

  bool Add(HeapObject* obj);
  Element* Find(HeapObject* obj);
  static void NullifyMostRecent(HeapObject* obj);
  void Sort();
  static void SortAll();

  static void PurgeDuplicates();  // Only to be called by GCEpilogue.

#ifdef VERIFY_LOL
  static void Verify(bool match_heap_exactly = false);
  static void VerifyNotInFromSpace();
#endif

  // Iterates the elements in every lol and returns the one that matches the
  // specified key.  If no matching element is found, then it returns NULL.
  template <typename T>
  inline static LiveObjectList::Element*
      FindElementFor(T (*GetValue)(LiveObjectList::Element*), T key);

  inline static int GetElementId(Element* element);
  inline static HeapObject* GetElementObj(Element* element);

  // Instance fields.
  LiveObjectList* prev_;
  LiveObjectList* next_;
  int id_;
  int capacity_;
  int obj_count_;
  Element* elements_;

  // Statics for managing all the lists.
  static uint32_t next_element_id_;
  static int list_count_;
  static int last_id_;
  static LiveObjectList* first_;
  static LiveObjectList* last_;

  friend class LolIterator;
  friend class LolForwardIterator;
  friend class LolDumpWriter;
  friend class RetainersDumpWriter;
  friend class RetainersSummaryWriter;
  friend class UpdateLiveObjectListVisitor;
};


// Helper class for updating the LiveObjectList HeapObject pointers.
class UpdateLiveObjectListVisitor: public ObjectVisitor {
 public:
  void VisitPointer(Object** p) { UpdatePointer(p); }

  void VisitPointers(Object** start, Object** end) {
    // Copy all HeapObject pointers in [start, end).
    for (Object** p = start; p < end; p++) UpdatePointer(p);
  }

 private:
  // Based on Heap::ScavengeObject() but only does forwarding of pointers
  // to live new space objects, and not actually keep them alive.
  void UpdatePointer(Object** p) {
    Object* object = *p;
    if (!HEAP->InNewSpace(object)) return;

    HeapObject* heap_obj = HeapObject::cast(object);
    ASSERT(HEAP->InFromSpace(heap_obj));

    // We use the first word (where the map pointer usually is) of a heap
    // object to record the forwarding pointer.  A forwarding pointer can
    // point to an old space, the code space, or the to space of the new
    // generation.
    MapWord first_word = heap_obj->map_word();

    // If the first word is a forwarding address, the object has already been
    // copied.
    if (first_word.IsForwardingAddress()) {
      *p = first_word.ToForwardingAddress();
      return;

    // Else, it's a dead object.
    } else {
      LiveObjectList::NullifyNonLivePointer(reinterpret_cast<HeapObject**>(p));
    }
  }
};


#else  // !LIVE_OBJECT_LIST


class LiveObjectList {
 public:
  inline static void GCEpilogue() {}
  inline static void GCPrologue() {}
  inline static void IterateElements(ObjectVisitor* v) {}
  inline static void ProcessNonLive(HeapObject* obj) {}
  inline static void UpdateReferencesForScavengeGC() {}

  inline static MaybeObject* Capture() { return HEAP->undefined_value(); }
  inline static bool Delete(int id) { return false; }
  inline static MaybeObject* Dump(int id1,
                                  int id2,
                                  int start_idx,
                                  int dump_limit,
                                  Handle<JSObject> filter_obj) {
    return HEAP->undefined_value();
  }
  inline static MaybeObject* Info(int start_idx, int dump_limit) {
    return HEAP->undefined_value();
  }
  inline static MaybeObject* Summarize(int id1,
                                       int id2,
                                       Handle<JSObject> filter_obj) {
    return HEAP->undefined_value();
  }

  inline static void Reset() {}
  inline static Object* GetObj(int obj_id) { return HEAP->undefined_value(); }
  inline static Object* GetObjId(Handle<String> address) {
    return HEAP->undefined_value();
  }
  inline static MaybeObject* GetObjRetainers(int obj_id,
                                             Handle<JSObject> instance_filter,
                                             bool verbose,
                                             int start,
                                             int count,
                                             Handle<JSObject> filter_obj) {
    return HEAP->undefined_value();
  }

  inline static Object* GetPath(int obj_id1,
                                int obj_id2,
                                Handle<JSObject> instance_filter) {
    return HEAP->undefined_value();
  }
  inline static Object* PrintObj(int obj_id) { return HEAP->undefined_value(); }
};


#endif  // LIVE_OBJECT_LIST

} }  // namespace v8::internal

#endif  // V8_LIVEOBJECTLIST_H_
