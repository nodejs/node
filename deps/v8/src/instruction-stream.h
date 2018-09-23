// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INSTRUCTION_STREAM_H_
#define V8_INSTRUCTION_STREAM_H_

#include "src/base/macros.h"
#include "src/globals.h"

namespace v8 {
namespace internal {

class Code;
class Isolate;

// Wraps an off-heap instruction stream.
// TODO(jgruber,v8:6666): Remove this class.
class InstructionStream final : public AllStatic {
 public:
  // Returns true, iff the given pc points into an off-heap instruction stream.
  static bool PcIsOffHeap(Isolate* isolate, Address pc);

  // Returns the corresponding Code object if it exists, and nullptr otherwise.
  static Code* TryLookupCode(Isolate* isolate, Address address);

  // During snapshot creation, we first create an executable off-heap area
  // containing all off-heap code. The area is guaranteed to be contiguous.
  // Note that this only applies when building the snapshot, e.g. for
  // mksnapshot. Otherwise, off-heap code is embedded directly into the binary.
  static void CreateOffHeapInstructionStream(Isolate* isolate, uint8_t** data,
                                             uint32_t* size);
  static void FreeOffHeapInstructionStream(uint8_t* data, uint32_t size);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_INSTRUCTION_STREAM_H_
