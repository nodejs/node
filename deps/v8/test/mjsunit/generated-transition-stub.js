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

%NeverOptimizeFunction(test);
function test() {

  var iteration_count = 1;

  function transition1(a, i, v) {
    a[i] = v;
  }

  //
  // Test PACKED SMI -> PACKED DOUBLE
  //

  var a1 = [0, 1, 2, 3, 4];
  transition1(a1, 0, 2.5);
  var a2 = [0, 1, 2, 3, 4];
  transition1(a2, 0, 2.5);
  assertFalse(%HasFastHoleyElements(a2));
  %OptimizeFunctionOnNextCall(transition1);

  var a3 = [0, 1, 2, 3, 4];
  assertTrue(%HasFastSmiElements(a3));
  transition1(a3, 0, 2.5);
  assertFalse(%HasFastHoleyElements(a3));
  assertEquals(4, a3[4]);
  assertEquals(2.5, a3[0]);

  // Test handling of hole.
  var a4 = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
  a4.length = 7;
  assertTrue(%HasFastSmiElements(a4));
  transition1(a4, 0, 2.5);
  assertFalse(%HasFastHoleyElements(a4));
  assertEquals(2.5, a4[0]);
  assertEquals(undefined, a4[8]);

  // Large array should deopt to runtimea
  for (j = 0; j < iteration_count; ++j) {
    a5 = new Array();
    for (i = 0; i < 0x40000; ++i) {
      a5[i] = 0;
    }
    assertTrue(%HasFastSmiElements(a5) || %HasFastDoubleElements(a5));
    transition1(a5, 0, 2.5);
    assertEquals(2.5, a5[0]);
  }

  //
  // Test HOLEY SMI -> HOLEY DOUBLE
  //

  function transition2(a, i, v) {
    a[i] = v;
  }

  var b1 = [0, 1, 2, , 4];
  transition2(b1, 0, 2.5);
  var b2 = [0, 1, 2, , 4];
  transition2(b2, 0, 2.5);
  assertTrue(%HasFastHoleyElements(b2));
  %OptimizeFunctionOnNextCall(transition2);

  var b3 = [0, 1, 2, , 4];
  assertTrue(%HasFastSmiElements(b3));
  assertTrue(%HasFastHoleyElements(b3));
  transition2(b3, 0, 2.5);
  assertTrue(%HasFastHoleyElements(b3));
  assertEquals(4, b3[4]);
  assertEquals(2.5, b3[0]);

  // Large array should deopt to runtime
  for (j = 0; j < iteration_count; ++j) {
    b4 = [0, ,0];
    for (i = 3; i < 0x40000; ++i) {
      b4[i] = 0;
    }
    assertTrue(%HasFastSmiElements(b4));
    transition2(b4, 0, 2.5);
    assertEquals(2.5, b4[0]);
  }

  //
  // Test PACKED DOUBLE -> PACKED OBJECT
  //

  function transition3(a, i, v) {
    a[i] = v;
  }

  var c1 = [0, 1, 2, 3.5, 4];
  transition3(c1, 0, new Object());
  var c2 = [0, 1, 2, 3.5, 4];
  transition3(c2, 0, new Object());
  assertTrue(%HasFastObjectElements(c2));
  assertTrue(!%HasFastHoleyElements(c2));
  %OptimizeFunctionOnNextCall(transition3);

  var c3 = [0, 1, 2, 3.5, 4];
  assertTrue(%HasFastDoubleElements(c3));
  assertTrue(!%HasFastHoleyElements(c3));
  transition3(c3, 0, new Array());
  assertTrue(!%HasFastHoleyElements(c3));
  assertTrue(%HasFastObjectElements(c3));
  assertEquals(4, c3[4]);
  assertEquals(0, c3[0].length);

  // Large array under the deopt threshold should be able to trigger GC without
  // causing crashes.
  for (j = 0; j < iteration_count; ++j) {
    c4 = [0, 2.5, 0];
    for (i = 3; i < 0xa000; ++i) {
      c4[i] = 0;
    }
    assertTrue(%HasFastDoubleElements(c4));
    assertTrue(!%HasFastHoleyElements(c4));
    transition3(c4, 0, new Array(5));
    assertTrue(!%HasFastHoleyElements(c4));
    assertTrue(%HasFastObjectElements(c4));
    assertEquals(5, c4[0].length);
  }

  // Large array should deopt to runtime
  for (j = 0; j < iteration_count; ++j) {
    c5 = [0, 2.5, 0];
    for (i = 3; i < 0x40000; ++i) {
      c5[i] = 0;
    }
    assertTrue(%HasFastDoubleElements(c5));
    assertTrue(!%HasFastHoleyElements(c5));
    transition3(c5, 0, new Array(5));
    assertTrue(!%HasFastHoleyElements(c5));
    assertTrue(%HasFastObjectElements(c5));
    assertEquals(5, c5[0].length);
  }

  //
  // Test HOLEY DOUBLE -> HOLEY OBJECT
  //

  function transition4(a, i, v) {
      a[i] = v;
  }

  var d1 = [0, 1, , 3.5, 4];
  transition4(d1, 0, new Object());
  var d2 = [0, 1, , 3.5, 4];
  transition4(d2, 0, new Object());
  assertTrue(%HasFastObjectElements(d2));
  assertTrue(%HasFastHoleyElements(d2));
  %OptimizeFunctionOnNextCall(transition4);

  var d3 = [0, 1, , 3.5, 4];
  assertTrue(%HasFastDoubleElements(d3));
  assertTrue(%HasFastHoleyElements(d3));
  transition4(d3, 0, new Array());
  assertTrue(%HasFastHoleyElements(d3));
  assertTrue(%HasFastObjectElements(d3));
  assertEquals(4, d3[4]);
  assertEquals(0, d3[0].length);

  // Large array under the deopt threshold should be able to trigger GC without
  // causing crashes.
  for (j = 0; j < iteration_count; ++j) {
    d4 = [, 2.5, ,];
    for (i = 3; i < 0xa000; ++i) {
      d4[i] = 0;
    }
    assertTrue(%HasFastDoubleElements(d4));
    assertTrue(%HasFastHoleyElements(d4));
    transition4(d4, 0, new Array(5));
    assertTrue(%HasFastHoleyElements(d4));
    assertTrue(%HasFastObjectElements(d4));
    assertEquals(5, d4[0].length);
    assertEquals(undefined, d4[2]);
  }

  // Large array should deopt to runtime
  for (j = 0; j < iteration_count; ++j) {
    d5 = [, 2.5, ,];
    for (i = 3; i < 0x40000; ++i) {
      d5[i] = 0;
    }
    assertTrue(%HasFastDoubleElements(d5));
    assertTrue(%HasFastHoleyElements(d5));
    transition4(d5, 0, new Array(5));
    assertTrue(%HasFastHoleyElements(d5));
    assertTrue(%HasFastObjectElements(d5));
    assertEquals(5, d5[0].length);
    assertEquals(undefined, d5[2]);
  }

}
test();
