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

// This tests that captured objects materialized through the deoptimizer
// have field descriptors with a representation matching the values that
// have actually been stored in the object.

var values = [ function() { return {}; },
               function() { return 23; },
               function() { return 4.2; } ];

function constructor(value_track) {
  this.x = value_track();
}

function access(value_track, value_break, deopt) {
  var o = new constructor(value_track);
  o.x = value_break;
  deopt.deopt
  assertEquals(value_break, o.x);
}

function test(value_track, value_break) {
  var deopt = { deopt:false };

  // Warm-up field tracking to a certain representation.
  access(value_track, value_track(), deopt);
  access(value_track, value_track(), deopt);
  %OptimizeFunctionOnNextCall(access);
  access(value_track, value_track(), deopt);

  // Deoptimize on a run with a different representation.
  delete deopt.deopt;
  access(value_track, value_break(), deopt);

  // Clear type feedback of the access function for next run.
  %ClearFunctionFeedback(access);

  // Also make sure the initial map of the constructor is reset.
  constructor.prototype = {};
}

for (var i = 0; i < values.length; i++) {
  for (var j = 0; j < values.length; j++) {
    test(values[i], values[j])
  }
}
