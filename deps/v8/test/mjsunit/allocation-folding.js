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

// Flags: --allow-natives-syntax --nouse-osr --expose-gc

// Test loop barrier when folding allocations.

function f() {
  var elem1 = [1,2,3];
  for (var i=0; i < 100000; i++) {
    var bar = [1];
  }
  var elem2 = [1,2,3];
  return elem2;
}

%PrepareFunctionForOptimization(f);
f(); f(); f();
%OptimizeFunctionOnNextCall(f);
var result = f();

gc();

assertEquals(result[2], 3);

// Test allocation folding of doubles.

function doubles() {
  var elem1 = [1.1, 1.2];
  var elem2 = [2.1, 2.2];
  return elem2;
}

%PrepareFunctionForOptimization(doubles);
doubles(); doubles(); doubles();
%OptimizeFunctionOnNextCall(doubles);
result = doubles();

gc();

assertEquals(result[1], 2.2);

// Test allocation folding of doubles into non-doubles.

function doubles_int() {
  var elem1 = [2, 3];
  var elem2 = [2.1, 3.1];
  return elem2;
}

%PrepareFunctionForOptimization(doubles_int);
doubles_int(); doubles_int(); doubles_int();
%OptimizeFunctionOnNextCall(doubles_int);
result = doubles_int();

gc();

assertEquals(result[1], 3.1);

// Test allocation folding over a branch.

function branch_int(left) {
  var elem1 = [1, 2];
  var elem2;
  if (left) {
    elem2 = [3, 4];
  } else {
    elem2 = [5, 6];
  }
  return elem2;
}

%PrepareFunctionForOptimization(branch_int);
branch_int(1); branch_int(1); branch_int(1);
%OptimizeFunctionOnNextCall(branch_int);
result = branch_int(1);
var result2 = branch_int(0);

gc();

assertEquals(result[1], 4);
assertEquals(result2[1], 6);

// Test to almost exceed the Page::MaxRegularHeapObjectSize limit.

function boom() {
  var a1 = new Array(84632);
  var a2 = new Array(84632);
  var a3 = new Array(84632);
  return [ a1, a2, a3 ];
}

%PrepareFunctionForOptimization(boom);
boom(); boom(); boom();
%OptimizeFunctionOnNextCall(boom);
boom();
