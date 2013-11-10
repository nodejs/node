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

#ifndef V8_HYDROGEN_OSR_H_
#define V8_HYDROGEN_OSR_H_

#include "hydrogen.h"
#include "ast.h"
#include "zone.h"

namespace v8 {
namespace internal {

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

} }  // namespace v8::internal

#endif  // V8_HYDROGEN_OSR_H_
