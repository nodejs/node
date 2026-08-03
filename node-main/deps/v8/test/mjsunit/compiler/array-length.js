// Copyright 2010 the V8 project authors. All rights reserved.
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

function ArrayLength(a) { return a.length; }

function Test(a0, a2, a5) {
  assertEquals(0, ArrayLength(a0));
  assertEquals(2, ArrayLength(a2));
  assertEquals(5, ArrayLength(a5));
}

var a0 = [];
var a2 = [1,2];
var a5 = [1,2,3,4,5];

function MainTest() {
  %PrepareFunctionForOptimization(ArrayLength);
  for (var i = 0; i < 5; i++) Test(a0, a2, a5);
  %OptimizeFunctionOnNextCall(ArrayLength);
  Test(a0, a2, a5);
  %PrepareFunctionForOptimization(Test);
  %OptimizeFunctionOnNextCall(Test);
  Test(a0, a2, a5);
  assertEquals("undefined", typeof(ArrayLength(0)));
  %PrepareFunctionForOptimization(Test);
  for (var i = 0; i < 5; i++) Test(a0, a2, a5);
  %OptimizeFunctionOnNextCall(Test);
  Test(a0, a2, a5);
  assertEquals(4, ArrayLength("hest"));
}
MainTest();

// Packed
// Non-extensible, sealed, frozen
a0 = Object.preventExtensions([]);
a2 = Object.seal([1,'2']);
a5 = Object.freeze([1,2,'3',4,5]);
MainTest();

a0 = Object.seal([]);
a2 = Object.freeze([1,'2']);
a5 = Object.preventExtensions([1,2,'3',4,5]);
MainTest();

a0 = Object.freeze([]);
a2 = Object.preventExtensions([1,'2']);
a5 = Object.seal([1,2,'3',4,5]);
MainTest();

// Holey
// Non-extensible, sealed, frozen
a0 = Object.preventExtensions([]);
a2 = Object.seal([,'2']);
a5 = Object.freeze([,2,'3',4,5]);
MainTest();

a0 = Object.seal([]);
a2 = Object.freeze([,'2']);
a5 = Object.preventExtensions([,2,'3',4,5]);
MainTest();

a0 = Object.freeze([]);
a2 = Object.preventExtensions([,'2']);
a5 = Object.seal([,2,3,4,5]);
MainTest();
