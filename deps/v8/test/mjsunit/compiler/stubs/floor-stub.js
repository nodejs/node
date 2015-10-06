// Copyright 2015 the V8 project authors. All rights reserved.
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

// Flags: --allow-natives-syntax --noalways-opt --turbo-filter=*

var stubs = %GetCodeStubExportsObject();

const kExtraTypeFeedbackMinusZeroSentinel = 1;
const kFirstJSFunctionTypeFeedbackIndex = 5;
const kFirstSlotExtraTypeFeedbackIndex = 5;

(function()  {
  var stub1 = stubs.MathFloorStub("MathFloorStub", 1);
  var tempForTypeVector = function(d) {
    return Math.round(d);
  }
  tempForTypeVector(5);
  var tv = %GetTypeFeedbackVector(tempForTypeVector);
  var floorFunc1 = function(v, first) {
    if (first) return;
    return stub1(stub1, kFirstSlotExtraTypeFeedbackIndex - 1, tv, undefined, v);
  };
  %OptimizeFunctionOnNextCall(stub1);
  floorFunc1(5, true);
  %FixedArraySet(tv, kFirstSlotExtraTypeFeedbackIndex - 1, stub1);
  assertTrue(kExtraTypeFeedbackMinusZeroSentinel !==
             %FixedArrayGet(tv, kFirstSlotExtraTypeFeedbackIndex));
  assertEquals(5.0, floorFunc1(5.5));
  assertTrue(kExtraTypeFeedbackMinusZeroSentinel !==
             %FixedArrayGet(tv, kFirstSlotExtraTypeFeedbackIndex));
  // Executing floor such that it returns -0 should set the proper sentinel in
  // the feedback vector.
  assertEquals(-Infinity, 1/floorFunc1(-0));
  assertEquals(kExtraTypeFeedbackMinusZeroSentinel,
               %FixedArrayGet(tv, kFirstSlotExtraTypeFeedbackIndex));
  %ClearFunctionTypeFeedback(floorFunc1);
})();
