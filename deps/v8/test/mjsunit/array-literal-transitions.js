// Copyright 2011 the V8 project authors. All rights reserved.
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
// Test element kind of objects.
// Since --smi-only-arrays affects builtins, its default setting at compile
// time sticks if built with snapshot.  If --smi-only-arrays is deactivated
// by default, only a no-snapshot build actually has smi-only arrays enabled
// in this test case.  Depending on whether smi-only arrays are actually
// enabled, this test takes the appropriate code path to check smi-only arrays.

support_smi_only_arrays = %HasFastSmiOnlyElements(new Array());

// IC and Crankshaft support for smi-only elements in dynamic array literals.
function get(foo) { return foo; }  // Used to generate dynamic values.

function array_literal_test() {
  var a0 = [1, 2, 3];
  assertTrue(%HasFastSmiOnlyElements(a0));
  var a1 = [get(1), get(2), get(3)];
  assertTrue(%HasFastSmiOnlyElements(a1));

  var b0 = [1, 2, get("three")];
  assertTrue(%HasFastElements(b0));
  var b1 = [get(1), get(2), get("three")];
  assertTrue(%HasFastElements(b1));

  var c0 = [1, 2, get(3.5)];
  assertTrue(%HasFastDoubleElements(c0));
  assertEquals(3.5, c0[2]);
  assertEquals(2, c0[1]);
  assertEquals(1, c0[0]);

  var c1 = [1, 2, 3.5];
  assertTrue(%HasFastDoubleElements(c1));
  assertEquals(3.5, c1[2]);
  assertEquals(2, c1[1]);
  assertEquals(1, c1[0]);

  var c2 = [get(1), get(2), get(3.5)];
  assertTrue(%HasFastDoubleElements(c2));
  assertEquals(3.5, c2[2]);
  assertEquals(2, c2[1]);
  assertEquals(1, c2[0]);

  var object = new Object();
  var d0 = [1, 2, object];
  assertTrue(%HasFastElements(d0));
  assertEquals(object, d0[2]);
  assertEquals(2, d0[1]);
  assertEquals(1, d0[0]);

  var e0 = [1, 2, 3.5];
  assertTrue(%HasFastDoubleElements(e0));
  assertEquals(3.5, e0[2]);
  assertEquals(2, e0[1]);
  assertEquals(1, e0[0]);

  var f0 = [1, 2, [1, 2]];
  assertTrue(%HasFastElements(f0));
  assertEquals([1,2], f0[2]);
  assertEquals(2, f0[1]);
  assertEquals(1, f0[0]);
}

if (support_smi_only_arrays) {
  for (var i = 0; i < 3; i++) {
    array_literal_test();
  }
  %OptimizeFunctionOnNextCall(array_literal_test);
  array_literal_test();

  function test_large_literal() {

    function d() {
      gc();
      return 2.5;
    }

    function o() {
      gc();
      return new Object();
    }

    large =
        [ 0, 1, 2, 3, 4, 5, d(), d(), d(), d(), d(), d(), o(), o(), o(), o() ];
    assertFalse(%HasDictionaryElements(large));
    assertFalse(%HasFastSmiOnlyElements(large));
    assertFalse(%HasFastDoubleElements(large));
    assertTrue(%HasFastElements(large));
    assertEquals(large,
                 [0, 1, 2, 3, 4, 5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5,
                  new Object(), new Object(), new Object(), new Object()]);
  }

  for (var i = 0; i < 3; i++) {
    test_large_literal();
  }
  %OptimizeFunctionOnNextCall(test_large_literal);
  test_large_literal();
}
