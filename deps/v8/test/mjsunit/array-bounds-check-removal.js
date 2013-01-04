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

// Flags: --allow-natives-syntax --expose-gc

var a = new Int32Array(1024);

// Test that we do not assert if the accessed index has not an int32 rep.
var v = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9];
function test_do_not_assert_on_non_int32(vector, base) {
  var r = 0;
  var a1 = base + 1;
  var a2 = base + 2;
  var a3 = base + 3;
  var a4 = base + 4;
  if (a1 == 2) {
    r += vector[a1];
    r += vector[a4];
    r += vector[a2];
    r += vector[a3];
  }
  return r;
}
test_do_not_assert_on_non_int32(v,1);
test_do_not_assert_on_non_int32(v,1);
test_do_not_assert_on_non_int32(v,"a");
test_do_not_assert_on_non_int32(v,"a");
%OptimizeFunctionOnNextCall(test_do_not_assert_on_non_int32);
test_do_not_assert_on_non_int32(v,0);

function test_base(base,cond) {
  a[base + 1] = 1;
  a[base + 4] = 2;
  a[base + 3] = 3;
  a[base + 2] = 4;
  a[base + 4] = base + 4;
  if (cond) {
    a[base + 1] = 1;
    a[base + 2] = 2;
    a[base + 2] = 3;
    a[base + 2] = 4;
    a[base + 4] = base + 4;
  } else {
    a[base + 6] = 1;
    a[base + 4] = 2;
    a[base + 3] = 3;
    a[base + 2] = 4;
    a[base + 4] = base - 4;
  }
}

function check_test_base(base,cond) {
  if (cond) {
    assertEquals(1, a[base + 1]);
    assertEquals(4, a[base + 2]);
    assertEquals(base + 4, a[base + 4]);
  } else {
    assertEquals(1, a[base + 6]);
    assertEquals(3, a[base + 3]);
    assertEquals(4, a[base + 2]);
    assertEquals(base - 4, a[base + 4]);
  }
}


function test_minus(base,cond) {
  a[base - 1] = 1;
  a[base - 2] = 2;
  a[base + 4] = 3;
  a[base] = 4;
  a[base + 4] = base + 4;
  if (cond) {
    a[base - 4] = 1;
    a[base + 5] = 2;
    a[base + 3] = 3;
    a[base + 2] = 4;
    a[base + 4] = base + 4;
  } else {
    a[base + 6] = 1;
    a[base + 4] = 2;
    a[base + 3] = 3;
    a[base + 2] = 4;
    a[base + 4] = base - 4;
  }
}

function check_test_minus(base,cond) {
  if (cond) {
    assertEquals(2, a[base + 5]);
    assertEquals(3, a[base + 3]);
    assertEquals(4, a[base + 2]);
    assertEquals(base + 4, a[base + 4]);
  } else {
    assertEquals(1, a[base + 6]);
    assertEquals(3, a[base + 3]);
    assertEquals(4, a[base + 2]);
    assertEquals(base - 4, a[base + 4]);
  }
}

test_base(1,true);
test_base(2,true);
test_base(1,false);
test_base(2,false);
%OptimizeFunctionOnNextCall(test_base);
test_base(3,true);
check_test_base(3,true);
test_base(3,false);
check_test_base(3,false);

test_minus(5,true);
test_minus(6,true);
%OptimizeFunctionOnNextCall(test_minus);
test_minus(7,true);
check_test_minus(7,true);
test_minus(7,false);
check_test_minus(7,false);

// Optimization status:
// YES: 1
// NO: 2
// ALWAYS: 3
// NEVER: 4

// Test that we still deopt on failed bound checks
test_base(5,true);
test_base(6,true);
test_base(5,false);
test_base(6,false);
%OptimizeFunctionOnNextCall(test_base);
test_base(-2,true);
assertTrue(%GetOptimizationStatus(test_base) != 1);

test_base(5,true);
test_base(6,true);
test_base(5,false);
test_base(6,false);
%OptimizeFunctionOnNextCall(test_base);
test_base(2048,true);
assertTrue(%GetOptimizationStatus(test_base) != 1);

// Specific test on negative offsets
var short_a = new Array(100);
for (var i = 0; i < short_a.length; i++) short_a[i] = 0;
function short_test(a, i) {
  a[i + 9] = 0;
  a[i - 10] = 0;
}
short_test(short_a, 50);
short_test(short_a, 50);
%OptimizeFunctionOnNextCall(short_test);
short_a.length = 10;
short_test(a, 0);
assertTrue(%GetOptimizationStatus(short_test) != 1);


// A test for when we would modify a phi index.
var data_phi = [0, 1, 2, 3, 4, 5, 6, 7, 8];
function test_phi(a, base, check) {
  var index;
  if (check) {
    index = base + 1;
  } else {
    index = base + 2;
  }
  var result = a[index];
  result += a[index + 1];
  result += a[index - 1];
  return result;
}
var result_phi = 0;
result_phi = test_phi(data_phi, 3,  true);
assertEquals(12, result_phi);
result_phi = test_phi(data_phi, 3,  true);
assertEquals(12, result_phi);
%OptimizeFunctionOnNextCall(test_phi);
result_phi = test_phi(data_phi, 3,  true);
assertEquals(12, result_phi);


gc();

