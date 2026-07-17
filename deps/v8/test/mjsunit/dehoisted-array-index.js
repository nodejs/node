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

var a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];

// Key is HParameter
function aoo(i) {
  return a[i + 1];
}

%PrepareFunctionForOptimization(aoo);
aoo(1);
aoo(-1);
%OptimizeFunctionOnNextCall(aoo);
aoo(-1);


// Key is HChange, used by either dehoised or non-dehoisted
function boo(i) {
  var ret = 0;
  if (i < 0) {
    ret = a[i + 10];
  } else {
    ret = a[i];
  }
  return ret;
}

%PrepareFunctionForOptimization(boo);
boo(1);
boo(-1);
%OptimizeFunctionOnNextCall(boo);
boo(-1);


// Key is HMul(-i ==> i * (-1))
function coo() {
  var ret = 0;
  for (var i = 4; i > 0; i -= 1) {
    ret += a[-i + 4];  // dehoisted
  }

  return ret;
}

%PrepareFunctionForOptimization(coo);
coo();
coo();
%OptimizeFunctionOnNextCall(coo);
coo();


// Key is HPhi, used only by dehoisted
function doo() {
  var ret = 0;
  for (var i = 0; i < 5; i += 1) {
    ret += a[i + 1];  // dehoisted
  }
  return ret;
}
%PrepareFunctionForOptimization(doo);
doo();
doo();
%OptimizeFunctionOnNextCall(doo);
doo();

// Key is HPhi, but used by both dehoisted and non-dehoisted
// sign extend is useless
function eoo() {
  var ret = 0;
  for (var i = 0; i < 5; i += 1) {
    ret += a[i];      // non-dehoisted
    ret += a[i + 1];  // dehoisted
  }

  return ret;
}
%PrepareFunctionForOptimization(eoo);
eoo();
eoo();
%OptimizeFunctionOnNextCall(eoo);
eoo();



// Key is HPhi, but used by either dehoisted or non-dehoisted
function foo() {
  var ret = 0;
  for (var i = -3; i < 4; i += 1) {
    if (i < 0) {
      ret += a[i + 4];  // dehoisted
    } else {
      ret += a[i];      // non-dehoisted
    }
  }

  return ret;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();

// Key is HPhi, but not induction variable
function goo(i) {
  if (i > 0) {
    i += 1;
  } else {
    i += -1;
  }

  return a[i + 3];
}
%PrepareFunctionForOptimization(goo);
goo(-1);
goo(-1);
%OptimizeFunctionOnNextCall(goo);
goo(-1);

// Key is return value of function
function index() {
  return 1;
}
%NeverOptimizeFunction(index);
function hoo() {
   return a[index() + 3];
}

%PrepareFunctionForOptimization(hoo);
hoo();
hoo();
%OptimizeFunctionOnNextCall(hoo);
hoo();

// Sign extension of key makes AssertZeroExtended fail in DoBoundsCheck
function ioo(i) {
  return a[i] + a[i + 1];
}

%PrepareFunctionForOptimization(ioo);
ioo(1);
ioo(1);
%OptimizeFunctionOnNextCall(ioo);
ioo(-1);
