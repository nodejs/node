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

// Flags: --allow-natives-syntax --expose-gc

// IC and Crankshaft support for smi-only elements in dynamic array literals.
function get(foo) { return foo; }  // Used to generate dynamic values.

function array_literal_test() {
  var a0 = [1, 2, 3];
  assertTrue(%HasFastSmiElements(a0));
  var a1 = [get(1), get(2), get(3)];
  assertTrue(%HasFastSmiElements(a1));

  var b0 = [1, 2, get("three")];
  assertTrue(%HasFastObjectElements(b0));
  var b1 = [get(1), get(2), get("three")];
  assertTrue(%HasFastObjectElements(b1));

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
  assertTrue(%HasFastObjectElements(d0));
  assertEquals(object, d0[2]);
  assertEquals(2, d0[1]);
  assertEquals(1, d0[0]);

  var e0 = [1, 2, 3.5];
  assertTrue(%HasFastDoubleElements(e0));
  assertEquals(3.5, e0[2]);
  assertEquals(2, e0[1]);
  assertEquals(1, e0[0]);

  var f0 = [1, 2, [1, 2]];
  assertTrue(%HasFastObjectElements(f0));
  assertEquals([1,2], f0[2]);
  assertEquals(2, f0[1]);
  assertEquals(1, f0[0]);
}

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
  assertFalse(%HasFastSmiElements(large));
  assertFalse(%HasFastDoubleElements(large));
  assertTrue(%HasFastObjectElements(large));
  assertEquals(large,
               [0, 1, 2, 3, 4, 5, 2.5, 2.5, 2.5, 2.5, 2.5, 2.5,
                new Object(), new Object(), new Object(), new Object()]);
}

for (var i = 0; i < 3; i++) {
  test_large_literal();
}
  %OptimizeFunctionOnNextCall(test_large_literal);
test_large_literal();

function deopt_array(use_literal) {
  if (use_literal) {
    return [.5, 3, 4];
  }  else {
    return new Array();
  }
}

deopt_array(false);
deopt_array(false);
deopt_array(false);
  %OptimizeFunctionOnNextCall(deopt_array);
var array = deopt_array(false);
assertOptimized(deopt_array);
deopt_array(true);
assertOptimized(deopt_array);
array = deopt_array(false);
assertOptimized(deopt_array);

// Check that unexpected changes in the objects stored into the boilerplate
// also force a deopt.
function deopt_array_literal_all_smis(a) {
  return [0, 1, a];
}

deopt_array_literal_all_smis(2);
deopt_array_literal_all_smis(3);
deopt_array_literal_all_smis(4);
array = deopt_array_literal_all_smis(4);
assertEquals(0, array[0]);
assertEquals(1, array[1]);
assertEquals(4, array[2]);
  %OptimizeFunctionOnNextCall(deopt_array_literal_all_smis);
array = deopt_array_literal_all_smis(5);
array = deopt_array_literal_all_smis(6);
assertOptimized(deopt_array_literal_all_smis);
assertEquals(0, array[0]);
assertEquals(1, array[1]);
assertEquals(6, array[2]);

array = deopt_array_literal_all_smis(.5);
assertUnoptimized(deopt_array_literal_all_smis);
assertEquals(0, array[0]);
assertEquals(1, array[1]);
assertEquals(.5, array[2]);

function deopt_array_literal_all_doubles(a) {
  return [0.5, 1, a];
}

deopt_array_literal_all_doubles(.5);
deopt_array_literal_all_doubles(.5);
deopt_array_literal_all_doubles(.5);
array = deopt_array_literal_all_doubles(0.5);
assertEquals(0.5, array[0]);
assertEquals(1, array[1]);
assertEquals(0.5, array[2]);
  %OptimizeFunctionOnNextCall(deopt_array_literal_all_doubles);
array = deopt_array_literal_all_doubles(5);
array = deopt_array_literal_all_doubles(6);
assertOptimized(deopt_array_literal_all_doubles);
assertEquals(0.5, array[0]);
assertEquals(1, array[1]);
assertEquals(6, array[2]);

var foo = new Object();
array = deopt_array_literal_all_doubles(foo);
assertUnoptimized(deopt_array_literal_all_doubles);
assertEquals(0.5, array[0]);
assertEquals(1, array[1]);
assertEquals(foo, array[2]);

(function literals_after_osr() {
  var color = [0];
  // Trigger OSR, if optimization is not disabled.
  if (%GetOptimizationStatus(literals_after_osr) != 4) {
    while (%GetOptimizationCount(literals_after_osr) == 0) {}
  }
  return [color[0]];
})();
