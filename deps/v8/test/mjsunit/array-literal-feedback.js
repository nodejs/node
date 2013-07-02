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

// Flags: --allow-natives-syntax --smi-only-arrays --expose-gc
// Flags: --track-allocation-sites --noalways-opt

// Test element kind of objects.
// Since --smi-only-arrays affects builtins, its default setting at compile
// time sticks if built with snapshot.  If --smi-only-arrays is deactivated
// by default, only a no-snapshot build actually has smi-only arrays enabled
// in this test case.  Depending on whether smi-only arrays are actually
// enabled, this test takes the appropriate code path to check smi-only arrays.

// support_smi_only_arrays = %HasFastSmiElements(new Array(1,2,3,4,5,6,7,8));
support_smi_only_arrays = true;

if (support_smi_only_arrays) {
  print("Tests include smi-only arrays.");
} else {
  print("Tests do NOT include smi-only arrays.");
}

if (support_smi_only_arrays) {

  function get_literal(x) {
    var literal = [1, 2, x];
    return literal;
  }

  get_literal(3);
  get_literal(3);
  %OptimizeFunctionOnNextCall(get_literal);
  a = get_literal(3);
  assertTrue(2 != %GetOptimizationStatus(get_literal));
  assertTrue(%HasFastSmiElements(a));
  a[0] = 3.5;

  // We should have transitioned the boilerplate array to double, and
  // crankshafted code should de-opt on the unexpected elements kind
  b = get_literal(3);
  assertTrue(%HasFastDoubleElements(b));
  assertEquals([1, 2, 3], b);
  assertTrue(1 != %GetOptimizationStatus(get_literal));

  // Optimize again
  get_literal(3);
  %OptimizeFunctionOnNextCall(get_literal);
  b = get_literal(3);
  assertTrue(%HasFastDoubleElements(b));
  assertTrue(2 != %GetOptimizationStatus(get_literal));
}
