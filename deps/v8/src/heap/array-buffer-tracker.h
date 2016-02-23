// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_ARRAY_BUFFER_TRACKER_H_
#define V8_HEAP_ARRAY_BUFFER_TRACKER_H_

#include <map>

#include "src/globals.h"

namespace v8 {
namespace internal {

// Forward declarations.
class Heap;
class JSArrayBuffer;

class ArrayBufferTracker {
 public:
  explicit ArrayBufferTracker(Heap* heap) : heap_(heap) {}
  ~ArrayBufferTracker();

  inline Heap* heap() { return heap_; }

  // The following methods are used to track raw C++ pointers to externally
  // allocated memory used as backing store in live array buffers.

  // A new ArrayBuffer was created with |data| as backing store.
  void RegisterNew(JSArrayBuffer* buffer);

  // The backing store |data| is no longer owned by V8.
  void Unregister(JSArrayBuffer* buffer);

  // A live ArrayBuffer was discovered during marking/scavenge.
  void MarkLive(JSArrayBuffer* buffer);

  // Frees all backing store pointers that weren't discovered in the previous
  // marking or scavenge phase.
  void FreeDead(bool from_scavenge);

  // Prepare for a new scavenge phase. A new marking phase is implicitly
  // prepared by finishing the previous one.
  void PrepareDiscoveryInNewSpace();

  // An ArrayBuffer moved from new space to old space.
  void Promote(JSArrayBuffer* buffer);

 private:
  Heap* heap_;

  // |live_array_buffers_| maps externally allocated memory used as backing
  // store for ArrayBuffers to the length of the respective memory blocks.
  //
  // At the beginning of mark/compact, |not_yet_discovered_array_buffers_| is
  // a copy of |live_array_buffers_| and we remove pointers as we discover live
  // ArrayBuffer objects during marking. At the end of mark/compact, the
  // remaining memory blocks can be freed.
  std::map<void*, size_t> live_array_buffers_;
  std::map<void*, size_t> not_yet_discovered_array_buffers_;

  // To be able to free memory held by ArrayBuffers during scavenge as well, we
  // have a separate list of allocated memory held by ArrayBuffers in new space.
  //
  // Since mark/compact also evacuates the new space, all pointers in the
  // |live_array_buffers_for_scavenge_| list are also in the
  // |live_array_buffers_| list.
  std::map<void*, size_t> live_array_buffers_for_scavenge_;
  std::map<void*, size_t> not_yet_discovered_array_buffers_for_scavenge_;
};
}  // namespace internal
}  // namespace v8
#endif  // V8_HEAP_ARRAY_BUFFER_TRACKER_H_
