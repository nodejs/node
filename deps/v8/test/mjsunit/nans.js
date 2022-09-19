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

// Flags: --allow-natives-syntax

// Helper to determine endian - returns true on little endian platforms
function isLittleEndian() {
  return ((new Uint32Array((new Uint8Array([4,3,2,1])).buffer))[0])
           == 0x01020304;
}

// Test that both kinds of NaNs (signaling or quiet) do not signal

function TestAllModes(f) {
  %PrepareFunctionForOptimization(f);
  f(); // Runtime
  f(); // IC
  f(); // IC second time
  %OptimizeFunctionOnNextCall(f);
  f(); // hydrogen
}

function TestDoubleSignalingNan() {
  // NaN with signal bit set
  function f() {
    if(isLittleEndian()) {
      var bytes = new Uint32Array([1, 0x7FF00000]);
    } else {
      var bytes = new Uint32Array([0x7FF00000, 1]);
    }
    var doubles = new Float64Array(bytes.buffer);
    assertTrue(isNaN(doubles[0]));
    assertTrue(isNaN(doubles[0]*2.0));
    assertTrue(isNaN(doubles[0] + 0.5));
  }

  TestAllModes(f);
}

TestDoubleSignalingNan();

function TestDoubleQuietNan() {
  // NaN with signal bit cleared
  function f() {
    if(isLittleEndian()) {
      var bytes = new Uint32Array([0, 0x7FF80000]);
    } else {
      var bytes = new Uint32Array([0x7FF80000, 0]);
    }
    var doubles = new Float64Array(bytes.buffer);
    assertTrue(isNaN(doubles[0]));
    assertTrue(isNaN(doubles[0]*2.0));
    assertTrue(isNaN(doubles[0] + 0.5));
  }

  TestAllModes(f);
}

TestDoubleQuietNan();

function TestFloatSignalingNan() {
  // NaN with signal bit set
  function f() {
    var bytes = new Uint32Array([0x7F800001]);
    var floats = new Float32Array(bytes.buffer);
    assertTrue(isNaN(floats[0]));
    assertTrue(isNaN(floats[0]*2.0));
    assertTrue(isNaN(floats[0] + 0.5));
  }

  TestAllModes(f);
}

TestFloatSignalingNan();

function TestFloatQuietNan() {
  // NaN with signal bit cleared
  function f() {
    var bytes = new Uint32Array([0x7FC00000]);
    var floats = new Float32Array(bytes.buffer);
    assertTrue(isNaN(floats[0]));
    assertTrue(isNaN(floats[0]*2.0));
    assertTrue(isNaN(floats[0] + 0.5));
  }

  TestAllModes(f);
}

TestFloatQuietNan();
