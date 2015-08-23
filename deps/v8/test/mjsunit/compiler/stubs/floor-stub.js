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

// Flags: --allow-natives-syntax --expose-natives-as=builtins --noalways-opt

const kExtraTypeFeedbackMinusZeroSentinel = 1;
const kFirstSlotExtraTypeFeedbackIndex = 5;

(function(){
  var floorFunc = function() {
    Math.floor(NaN);
  }
  // Execute the function once to make sure it has a type feedback vector.
  floorFunc(5);
  var stub = builtins.MathFloorStub("MathFloorStub", 0);
  assertTrue(kExtraTypeFeedbackMinusZeroSentinel !==
             %FixedArrayGet(%GetTypeFeedbackVector(floorFunc),
                            kFirstSlotExtraTypeFeedbackIndex));
  assertEquals(5.0, stub(floorFunc, 4, 5.5));
  assertTrue(kExtraTypeFeedbackMinusZeroSentinel !==
             %FixedArrayGet(%GetTypeFeedbackVector(floorFunc),
                            kFirstSlotExtraTypeFeedbackIndex));
  // Executing floor such that it returns -0 should set the proper sentinel in
  // the feedback vector.
  assertEquals(-Infinity, 1/stub(floorFunc, 4, -0));
  assertEquals(kExtraTypeFeedbackMinusZeroSentinel,
               %FixedArrayGet(%GetTypeFeedbackVector(floorFunc),
                              kFirstSlotExtraTypeFeedbackIndex));
  %ClearFunctionTypeFeedback(floorFunc);
})();
