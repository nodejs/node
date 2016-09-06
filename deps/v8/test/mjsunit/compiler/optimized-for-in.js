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

// Flags: --allow-natives-syntax

// Test for-in support in Crankshaft.  For simplicity this tests assumes certain
// fixed iteration order for properties and will have to be adjusted if V8
// stops following insertion order.


function a(t) {
  var result = [];
  for (var i in t) {
    result.push([i, t[i]]);
  }
  return result;
}

// Check that we correctly deoptimize on map check.
function b(t) {
  var result = [];
  for (var i in t) {
    result.push([i, t[i]]);
    delete t[i];
  }
  return result;
}

// Check that we correctly deoptimize during preparation step.
function c(t) {
  var result = [];
  for (var i in t) {
    result.push([i, t[i]]);
  }
  return result;
}

// Check that we deoptimize to the place after side effect in the right state.
function d(t) {
  var result = [];
  var o;
  for (var i in (o = t())) {
    result.push([i, o[i]]);
  }
  return result;
}

// Check that we correctly deoptimize on map check inserted for fused load.
function e(t) {
  var result = [];
  for (var i in t) {
    delete t[i];
    t[i] = i;
    result.push([i, t[i]]);
  }
  return result;
}

// Nested for-in loops.
function f(t) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push([i, j, t[i], t[j]]);
    }
  }
  return result;
}

// Deoptimization from the inner for-in loop.
function g(t) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push([i, j, t[i], t[j]]);
      var v = t[i];
      delete t[i];
      t[i] = v;
    }
  }
  return result;
}


// Break from the inner for-in loop.
function h(t, deopt) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push([i, j, t[i], t[j]]);
      break;
    }
  }
  deopt.deopt;
  return result;
}

// Continue in the inner loop.
function j(t, deopt) {
  var result = [];
  for (var i in t) {
    for (var j in t) {
      result.push([i, j, t[i], t[j]]);
      continue;
    }
  }
  deopt.deopt;
  return result;
}

// Continue of the outer loop.
function k(t, deopt) {
  var result = [];
  outer: for (var i in t) {
    for (var j in t) {
      result.push([i, j, t[i], t[j]]);
      continue outer;
    }
  }
  deopt.deopt;
  return result;
}

// Break of the outer loop.
function l(t, deopt) {
  var result = [];
  outer: for (var i in t) {
    for (var j in t) {
      result.push([i, j, t[i], t[j]]);
      break outer;
    }
  }
  deopt.deopt;
  return result;
}

// Test deoptimization from inlined frame (currently it is not inlined).
function m0(t, deopt) {
  for (var i in t) {
    for (var j in t) {
      deopt.deopt;
      return [i, j,  t[i], t[j]];
    }
  }
}

function m(t, deopt) {
  return m0(t, deopt);
}


function tryFunction(result, mkT, f) {
  var d = {deopt: false};
  assertEquals(result, f(mkT(), d));
  assertEquals(result, f(mkT(), d));
  assertEquals(result, f(mkT(), d));
  %OptimizeFunctionOnNextCall(f);
  assertEquals(result, f(mkT(), d));
  assertEquals(result, f(mkT(), {}));
}

var expectedResult = [["a","1"],["b","2"],["c","3"],["d","4"]];
function mkTable() { return { a: "1", b: "2", c: "3", d: "4" }; }


tryFunction(expectedResult, mkTable, a);
tryFunction(expectedResult, mkTable, b);

expectedResult = [["0","a"],["1","b"],["2","c"],["3","d"]];
tryFunction(expectedResult, function () { return "abcd"; }, c);
tryFunction(expectedResult, function () {
  var cnt = false;
  return function () {
    cnt = true;
    return "abcd";
  }
}, d);
tryFunction([["a","a"],["b","b"],["c","c"],["d","d"]], mkTable, e);

function mkSmallTable() { return { a: "1", b: "2" }; }

tryFunction([
    ["a","a","1","1"],["a","b","1","2"],
    ["b","a","2","1"],["b","b","2","2"]],
    mkSmallTable, f);
tryFunction([
    ["a","a","1","1"],["a","b","1","2"],
    ["b","b","2","2"],["b","a","2","1"]],
    mkSmallTable, g);
tryFunction([["a","a","1","1"],["b","a","2","1"]], mkSmallTable, h);
tryFunction([
    ["a","a","1","1"],["a","b","1","2"],
    ["b","a","2","1"],["b","b","2","2"]],
    mkSmallTable, j);
tryFunction([["a","a","1","1"],["b","a","2","1"]], mkSmallTable, h);
tryFunction([["a","a","1","1"],["b","a","2","1"]], mkSmallTable, k);
tryFunction([["a","a","1","1"]], mkSmallTable, l);
tryFunction(["a","a","1","1"], mkSmallTable, m);

// Test handling of null.
tryFunction("", function () {
  return function () { return null; }
}, function (t) {
  for (var i in t()) { return i; }
  return "";
});

// Test smis.
tryFunction("", function () {
  return function () { return 11; }
}, function (t) {
  for (var i in t()) { return i; }
  return "";
});

// Test LoadFieldByIndex for out of object properties.
function O() { this.a = 1; }
for (var i = 0; i < 10; i++) new O();
tryFunction([["a",1],["b",2],["c",3],["d",4],["e",5],["f",6]], function () {
  var o = new O();
  o.b = 2;
  o.c = 3;
  o.d = 4;
  o.e = 5;
  o.f = 6;
  return o;
}, function (t) {
  var r = [];
  for (var i in t) r.push([i, t[i]]);
  return r;
});

// Test OSR inside for-in.
function osr_inner(t, limit) {
  var r = 1;
  for (var x in t) {
    if (t.hasOwnProperty(x)) {
      for (var i = 0; i < t[x].length; i++) {
        r += t[x][i];
        if (i === limit) %OptimizeOsr();
      }
      r += x;
    }
  }
  return r;
}

function osr_outer(t, osr_after) {
  var r = 1;
  for (var x in t) {
    for (var i = 0; i < t[x].length; i++) {
      r += t[x][i];
    }
    if (x === osr_after) %OptimizeOsr();
    r += x;
  }
  return r;
}

function osr_outer_and_deopt(t, osr_after) {
  var r = 1;
  for (var x in t) {
    r += x;
    if (x == osr_after) %OptimizeOsr();
  }
  return r;
}

function test_osr() {
  with ({}) {}  // Disable optimizations of this function.
  var arr = new Array(20);
  for (var i = 0; i < arr.length; i++) {
    arr[i] = i + 1;
  }
  arr.push(":");  // Force deopt at the end of the loop.
  assertEquals("211:x1234567891011121314151617181920:y", osr_inner({x: arr, y: arr}, (arr.length / 2) | 0));
  assertEquals("7x456y", osr_outer({x: [1,2,3], y: [4,5,6]}, "x"));
  assertEquals("101234567", osr_outer_and_deopt([1,2,3,4,5,6,7,8], "5"));
}

test_osr();
