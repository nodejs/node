// Copyright 2012 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax --inline-construct

// Test that inlined object allocation works for different layouts of
// objects (e.g. in object properties, slack tracking in progress or
// changing of function prototypes)

function test_helper(construct, a, b) {
  return new construct(a, b);
}

function test(construct) {
  %DeoptimizeFunction(test);
  test_helper(construct, 0, 0);
  test_helper(construct, 0, 0);
  %OptimizeFunctionOnNextCall(test_helper);
  // Test adding a new property after allocation was inlined.
  var o = test_helper(construct, 1, 2);
  o.z = 3;
  assertEquals(1, o.x);
  assertEquals(2, o.y);
  assertEquals(3, o.z);
  // Test changing the prototype after allocation was inlined.
  construct.prototype = { z:6 };
  var o = test_helper(construct, 4, 5);
  assertEquals(4, o.x);
  assertEquals(5, o.y);
  assertEquals(6, o.z);
  %DeoptimizeFunction(test_helper);
  %ClearFunctionTypeFeedback(test_helper);
}

function finalize_slack_tracking(construct) {
  // Value chosen based on kGenerousAllocationCount = 8.
  for (var i = 0; i < 8; i++) {
    new construct(0, 0);
  }
}


// Both properties are pre-allocated in object properties.
function ConstructInObjectPreAllocated(a, b) {
  this.x = a;
  this.y = b;
}
finalize_slack_tracking(ConstructInObjectPreAllocated);
test(ConstructInObjectPreAllocated);


// Both properties are unused in object properties.
function ConstructInObjectUnused(a, b) {
  this.x = a < 0 ? 0 : a;
  this.y = b > 0 ? b : 0;
}
finalize_slack_tracking(ConstructInObjectUnused);
test(ConstructInObjectUnused);


// Test inlined allocation while slack tracking is still in progress.
function ConstructWhileSlackTracking(a, b) {
  this.x = a;
  this.y = b;
}
test(ConstructWhileSlackTracking);
