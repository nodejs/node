// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_HYDROGEN_OSR_H_
#define V8_CRANKSHAFT_HYDROGEN_OSR_H_

#include "src/crankshaft/hydrogen.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class IterationStatement;

// Responsible for building graph parts related to OSR and otherwise
// setting up the graph to do an OSR compile.
class HOsrBuilder : public ZoneObject {
 public:
  explicit HOsrBuilder(HOptimizedGraphBuilder* builder)
    : unoptimized_frame_slots_(0),
      builder_(builder),
      osr_entry_(NULL),
      osr_loop_entry_(NULL),
      osr_values_(NULL) { }

  // Creates the loop entry block for the given statement, setting up OSR
  // entries as necessary, and sets the current block to the new block.
  HBasicBlock* BuildOsrLoopEntry(IterationStatement* statement);

  // Process the hydrogen graph after it has been completed, performing
  // any OSR-specific cleanups or changes.
  void FinishGraph();

  // Process the OSR values and phis after initial graph optimization.
  void FinishOsrValues();

  // Return the number of slots in the unoptimized frame at the entry to OSR.
  int UnoptimizedFrameSlots() const {
    return unoptimized_frame_slots_;
  }

  bool HasOsrEntryAt(IterationStatement* statement);

 private:
  int unoptimized_frame_slots_;
  HOptimizedGraphBuilder* builder_;
  HBasicBlock* osr_entry_;
  HBasicBlock* osr_loop_entry_;
  ZoneList<HUnknownOSRValue*>* osr_values_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_HYDROGEN_OSR_H_
