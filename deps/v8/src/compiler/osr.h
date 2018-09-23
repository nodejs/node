// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_OSR_H_
#define V8_COMPILER_OSR_H_

#include "src/zone.h"

// TurboFan structures OSR graphs in a way that separates almost all phases of
// compilation from OSR implementation details. This is accomplished with
// special control nodes that are added at graph building time. In particular,
// the graph is built in such a way that typing still computes the best types
// and optimizations and lowering work unchanged. All that remains is to
// deconstruct the OSR artifacts before scheduling and code generation.

// Graphs built for OSR from the AstGraphBuilder are structured as follows:
//                             Start
//          +-------------------^^-----+
//          |                          |
//   OsrNormalEntry               OsrLoopEntry <-------------+
//          |                          |                     |
//   control flow before loop          |           A     OsrValue
//          |                          |           |         |
//          | +------------------------+           | +-------+
//          | | +-------------+                    | | +--------+
//          | | |             |                    | | |        |
//        ( Loop )<-----------|------------------ ( phi )       |
//          |                 |                                 |
//      loop body             | backedge(s)                     |
//          |  |              |                                 |
//          |  +--------------+                        B  <-----+
//          |
//         end

// The control structure expresses the relationship that the loop has a separate
// entrypoint which corresponds to entering the loop directly from the middle
// of unoptimized code.
// Similarly, the values that come in from unoptimized code are represented with
// {OsrValue} nodes that merge into any phis associated with the OSR loop.
// In the above diagram, nodes {A} and {B} represent values in the "normal"
// graph that correspond to the values of those phis before the loop and on any
// backedges, respectively.

// To deconstruct OSR, we simply replace the uses of the {OsrNormalEntry}
// control node with {Dead} and {OsrLoopEntry} with start and run the
// {ControlReducer}. Control reduction propagates the dead control forward,
// essentially "killing" all the code before the OSR loop. The entrypoint to the
// loop corresponding to the "normal" entry path will also be removed, as well
// as the inputs to the loop phis, resulting in the reduced graph:

//                             Start
//         Dead                  |^-------------------------+
//          |                    |                          |
//          |                    |                          |
//          |                    |                          |
//    disconnected, dead         |           A=dead      OsrValue
//                               |                          |
//            +------------------+                   +------+
//            | +-------------+                      | +--------+
//            | |             |                      | |        |
//        ( Loop )<-----------|------------------ ( phi )       |
//          |                 |                                 |
//      loop body             | backedge(s)                     |
//          |  |              |                                 |
//          |  +--------------+                        B  <-----+
//          |
//         end

// Other than the presences of the OsrValue nodes, this is a normal, schedulable
// graph. OsrValue nodes are handled specially in the instruction selector to
// simply load from the unoptimized frame.

// For nested OSR loops, loop peeling must first be applied as many times as
// necessary in order to bring the OSR loop up to the top level (i.e. to be
// an outer loop).

namespace v8 {
namespace internal {

class CompilationInfo;

namespace compiler {

class JSGraph;
class CommonOperatorBuilder;
class Frame;
class Linkage;

// Encapsulates logic relating to OSR compilations as well has handles some
// details of the frame layout.
class OsrHelper {
 public:
  explicit OsrHelper(CompilationInfo* info);
  // Only for testing.
  OsrHelper(size_t parameter_count, size_t stack_slot_count)
      : parameter_count_(parameter_count),
        stack_slot_count_(stack_slot_count) {}

  // Deconstructs the artificial {OsrNormalEntry} and rewrites the graph so
  // that only the path corresponding to {OsrLoopEntry} remains.
  void Deconstruct(JSGraph* jsgraph, CommonOperatorBuilder* common,
                   Zone* tmp_zone);

  // Prepares the frame w.r.t. OSR.
  void SetupFrame(Frame* frame);

  // Returns the number of unoptimized frame slots for this OSR.
  size_t UnoptimizedFrameSlots() { return stack_slot_count_; }

  // Returns the environment index of the first stack slot.
  static int FirstStackSlotIndex(int parameter_count) {
    // n.b. unlike Crankshaft, TurboFan environments do not contain the context.
    return 1 + parameter_count;  // receiver + params
  }

 private:
  size_t parameter_count_;
  size_t stack_slot_count_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_OSR_H_
