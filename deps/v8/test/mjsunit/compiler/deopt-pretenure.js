// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --opt --no-always-opt
// Flags: --allocation-site-pretenuring --stress-gc-during-compilation
// Flags: --stress-scavenge=0 --gc-interval=-1
// Flags: --max-optimized-bytecode-size=132000

function CheckOptimizationStatus(func, expectedOptimizationStatus) {
  let opt_status = %GetOptimizationStatus(func);
  assertTrue ((opt_status & expectedOptimizationStatus) !== 0,
      "Expected flag " + expectedOptimizationStatus +
      " to be set in optimization status");
}

// Trigger pretenuring decision change at entry, deopting at bytecode offset -1.
let arr = [];
var empty;
function DeoptEntry(expectedStatus) {
  CheckOptimizationStatus(DeoptEntry, expectedStatus);
  empty = [];
  arr.push(empty);
}

%PrepareFunctionForOptimization(DeoptEntry);
DeoptEntry(V8OptimizationStatus.kTopmostFrameIsInterpreted
            | V8OptimizationStatus.kTopmostFrameIsBaseline);

%OptimizeFunctionOnNextCall(DeoptEntry);
// Force the allocation site to be pretenured.
assertTrue(%PretenureAllocationSite(empty));
// This call should deopt at entry because of the pretenuring decision change.
DeoptEntry(V8OptimizationStatus.kTopmostFrameIsInterpreted
            | V8OptimizationStatus.kTopmostFrameIsBaseline);

%PrepareFunctionForOptimization(DeoptEntry);
%OptimizeFunctionOnNextCall(DeoptEntry);
// Function should be compiled now.
DeoptEntry(V8OptimizationStatus.kTopmostFrameIsTurboFanned);

// Trigger pretenuring decision change during OSR.
function createSource(name, fillCnt) {
  var src =
  `function ${name}() {
     let arr = [];
     for (var i = 0; i < 10; i++) {
       let local_arr = [];
       arr[i] = local_arr;`
  // Useless bytecodes to force a wider jump.
  for (var i = 0; i < fillCnt; i++) {
    src += '    try {} catch (e) {}\n';
  }
  src +=
  `    if (i == 5) {
         %OptimizeOsr();
         %PretenureAllocationSite(local_arr);
       }
     }
   }
   %PrepareFunctionForOptimization(${name});
   ${name}();`
  return src;
}

// Deopt at JumpLoop.
eval(createSource('Loop',0));
// Deopt at JumpLoop.Wide.
eval(createSource('LoopWide',0xFF));
// Deopt at JumpLoop.ExtraWide.
// --max-optimized-bytecode-size has to be large enough to compile this.
eval(createSource('LoopExtraWide',0xFFF));
