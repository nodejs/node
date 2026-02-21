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

// Test shift operations that can be replaced by rotate operation.

function SideEffect() {
  with ({}) { }  // not inlinable
}

function Twenty() {
  SideEffect();
  return 20;
}

function Twelve() {
  SideEffect();
  return 12;
}


function ROR(x, sa) {
  return (x >>> sa) | (x << (32 - sa));
}

function ROR1(x, sa) {
  return (x >>> sa) | (x << (32 - sa));
}

function ROR2(x, sa) {
  return (x >>> (32 - sa)) | (x << (sa));
}

function ROR3(x, sa) {
  return (x << (32 - sa)) | (x >>> sa);
}

function ROR4(x, sa) {
  return (x << (sa)) | (x >>> (32 - sa));
}

%PrepareFunctionForOptimization(ROR);
assertEquals(1 << ((2 % 32)), ROR(1, 30));
assertEquals(1 << ((2 % 32)), ROR(1, 30));
%OptimizeFunctionOnNextCall(ROR);
assertEquals(1 << ((2 % 32)), ROR(1, 30));

%PrepareFunctionForOptimization(ROR1);
assertEquals(0xF0000FFF | 0, ROR1(0x0000FFFF, 4));
assertEquals(0xF0000FFF | 0, ROR1(0x0000FFFF, 4));
%OptimizeFunctionOnNextCall(ROR1);
assertEquals(0xF0000FFF | 0, ROR1(0x0000FFFF, 4));

%PrepareFunctionForOptimization(ROR1);
assertEquals(0x0FFFF000 | 0, ROR1(0x0000FFFF, 20));
assertEquals(0x0FFFF000 | 0, ROR1(0x0000FFFF, 20));
%OptimizeFunctionOnNextCall(ROR1);
assertEquals(0x0FFFF000 | 0, ROR1(0x0000FFFF, 20));

%PrepareFunctionForOptimization(ROR1);
assertEquals(0x0FFFF000 | 0, ROR1(0x0000FFFF, Twenty()));
assertEquals(0x0FFFF000 | 0, ROR1(0x0000FFFF, Twenty()));
%OptimizeFunctionOnNextCall(ROR1);
assertEquals(0x0FFFF000 | 0, ROR1(0x0000FFFF, Twenty()));

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR1);
  assertEquals(0xFFFFFFFF | 0, ROR1(0xFFFFFFFF, i));
  assertEquals(0xFFFFFFFF | 0, ROR1(0xFFFFFFFF, i));
  %OptimizeFunctionOnNextCall(ROR1);
  assertEquals(0xFFFFFFFF | 0, ROR1(0xFFFFFFFF, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR1);
  assertEquals(-1, ROR1(-1, i));
  assertEquals(-1, ROR1(-1, i));
  %OptimizeFunctionOnNextCall(ROR1);
  assertEquals(-1, ROR1(-1, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR1);
  assertEquals(1 << (32 - (i % 32)), ROR1(1, i));
  assertEquals(1 << (32 - (i % 32)), ROR1(1, i));
  %OptimizeFunctionOnNextCall(ROR1);
  assertEquals(1 << (32 - (i % 32)), ROR1(1, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR1);
  assertEquals(1 << (32 - (i % 32)), ROR1(1.4, i));
  assertEquals(1 << (32 - (i % 32)), ROR1(1.4, i));
  %OptimizeFunctionOnNextCall(ROR1);
  assertEquals(1 << (32 - (i % 32)), ROR1(1.4, i));
}



%PrepareFunctionForOptimization(ROR2);
assertEquals(0xF0000FFF | 0, ROR2(0x0000FFFF, 28));
assertEquals(0xF0000FFF | 0, ROR2(0x0000FFFF, 28));
%OptimizeFunctionOnNextCall(ROR2);
assertEquals(0xF0000FFF | 0, ROR2(0x0000FFFF, 28));

%PrepareFunctionForOptimization(ROR2);
assertEquals(0x0FFFF000 | 0, ROR2(0x0000FFFF, 12));
assertEquals(0x0FFFF000 | 0, ROR2(0x0000FFFF, 12));
%OptimizeFunctionOnNextCall(ROR2);
assertEquals(0x0FFFF000 | 0, ROR2(0x0000FFFF, 12));

%PrepareFunctionForOptimization(ROR2);
assertEquals(0x0FFFF000 | 0, ROR2(0x0000FFFF, Twelve()));
assertEquals(0x0FFFF000 | 0, ROR2(0x0000FFFF, Twelve()));
%OptimizeFunctionOnNextCall(ROR2);
assertEquals(0x0FFFF000 | 0, ROR2(0x0000FFFF, Twelve()));

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR2);
  assertEquals(0xFFFFFFFF | 0, ROR2(0xFFFFFFFF, i));
  assertEquals(0xFFFFFFFF | 0, ROR2(0xFFFFFFFF, i));
  %OptimizeFunctionOnNextCall(ROR2);
  assertEquals(0xFFFFFFFF | 0, ROR2(0xFFFFFFFF, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR2);
  assertEquals(-1, ROR2(-1, i));
  assertEquals(-1, ROR2(-1, i));
  %OptimizeFunctionOnNextCall(ROR2);
  assertEquals(-1, ROR2(-1, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR2);
  assertEquals(1 << ((i % 32)), ROR2(1, i));
  assertEquals(1 << ((i % 32)), ROR2(1, i));
  %OptimizeFunctionOnNextCall(ROR2);
  assertEquals(1 << ((i % 32)), ROR2(1, i));
}

%PrepareFunctionForOptimization(ROR3);
assertEquals(0xF0000FFF | 0, ROR3(0x0000FFFF, 4));
assertEquals(0xF0000FFF | 0, ROR3(0x0000FFFF, 4));
%OptimizeFunctionOnNextCall(ROR3);
assertEquals(0xF0000FFF | 0, ROR3(0x0000FFFF, 4));

%PrepareFunctionForOptimization(ROR3);
assertEquals(0x0FFFF000 | 0, ROR3(0x0000FFFF, 20));
assertEquals(0x0FFFF000 | 0, ROR3(0x0000FFFF, 20));
%OptimizeFunctionOnNextCall(ROR3);
assertEquals(0x0FFFF000 | 0, ROR3(0x0000FFFF, 20));

%PrepareFunctionForOptimization(ROR3);
assertEquals(0x0FFFF000 | 0, ROR3(0x0000FFFF, Twenty()));
assertEquals(0x0FFFF000 | 0, ROR3(0x0000FFFF, Twenty()));
%OptimizeFunctionOnNextCall(ROR3);
assertEquals(0x0FFFF000 | 0, ROR3(0x0000FFFF, Twenty()));

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR3);
  assertEquals(0xFFFFFFFF | 0, ROR3(0xFFFFFFFF, i));
  assertEquals(0xFFFFFFFF | 0, ROR3(0xFFFFFFFF, i));
  %OptimizeFunctionOnNextCall(ROR3);
  assertEquals(0xFFFFFFFF | 0, ROR3(0xFFFFFFFF, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR3);
  assertEquals(-1, ROR3(-1, i));
  assertEquals(-1, ROR3(-1, i));
  %OptimizeFunctionOnNextCall(ROR3);
  assertEquals(-1, ROR3(-1, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR3);
  assertEquals(1 << (32 - (i % 32)), ROR3(1, i));
  assertEquals(1 << (32 - (i % 32)), ROR3(1, i));
  %OptimizeFunctionOnNextCall(ROR3);
  assertEquals(1 << (32 - (i % 32)), ROR3(1, i));
}

%PrepareFunctionForOptimization(ROR4);
assertEquals(0xF0000FFF | 0, ROR4(0x0000FFFF, 28));
assertEquals(0xF0000FFF | 0, ROR4(0x0000FFFF, 28));
%OptimizeFunctionOnNextCall(ROR4);
assertEquals(0xF0000FFF | 0, ROR4(0x0000FFFF, 28));

%PrepareFunctionForOptimization(ROR4);
assertEquals(0x0FFFF000 | 0, ROR4(0x0000FFFF, 12));
assertEquals(0x0FFFF000 | 0, ROR4(0x0000FFFF, 12));
%OptimizeFunctionOnNextCall(ROR4);
assertEquals(0x0FFFF000 | 0, ROR4(0x0000FFFF, 12));

%PrepareFunctionForOptimization(ROR4);
assertEquals(0x0FFFF000 | 0, ROR4(0x0000FFFF, Twelve()));
assertEquals(0x0FFFF000 | 0, ROR4(0x0000FFFF, Twelve()));
%OptimizeFunctionOnNextCall(ROR4);
assertEquals(0x0FFFF000 | 0, ROR4(0x0000FFFF, Twelve()));

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR4);
  assertEquals(0xFFFFFFFF | 0, ROR4(0xFFFFFFFF, i));
  assertEquals(0xFFFFFFFF | 0, ROR4(0xFFFFFFFF, i));
  %OptimizeFunctionOnNextCall(ROR4);
  assertEquals(0xFFFFFFFF | 0, ROR4(0xFFFFFFFF, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR4);
  assertEquals(-1, ROR4(-1, i));
  assertEquals(-1, ROR4(-1, i));
  %OptimizeFunctionOnNextCall(ROR4);
  assertEquals(-1, ROR4(-1, i));
}

for (var i = 0; i <= 100; i++) {
  %PrepareFunctionForOptimization(ROR4);
  assertEquals(1 << ((i % 32)), ROR4(1, i));
  assertEquals(1 << ((i % 32)), ROR4(1, i));
  %OptimizeFunctionOnNextCall(ROR4);
  assertEquals(1 << ((i % 32)), ROR4(1, i));
}

//---------------------------------------------------------
// add test cases for constant operand
//---------------------------------------------------------
// constant operand: 20
function ROR1_sa20(x) {
  return (x >>> 20) | (x << 12);
}

function ROR2_sa20(x) {
  return (x >>> 12) | (x << 20);
}

function ROR3_sa20(x, sa) {
  return (x << 12) | (x >>> 20);
}

function ROR4_sa20(x) {
  return (x << 20) | (x >>> 12);
}

// constant operand: 40
function ROR1_sa40(x) {
  return (x >>> 40) | (x << -8);
}

function ROR2_sa40(x) {
  return (x >>> -8) | (x << 40);
}

function ROR3_sa40(x, sa) {
  return (x << -8) | (x >>> 40);
}

function ROR4_sa40(x) {
  return (x << 40) | (x >>> -8);
}

// ROR1_sa20
%PrepareFunctionForOptimization(ROR1_sa20);
assertEquals(ROR1(0x0000FFFF, 20), ROR1_sa20(0x0000FFFF));
assertEquals(ROR1(0x0000FFFF, 20), ROR1_sa20(0x0000FFFF));
%OptimizeFunctionOnNextCall(ROR1_sa20);
assertEquals(ROR1(0x0000FFFF, 20), ROR1_sa20(0x0000FFFF));

// ROR1_sa40
%PrepareFunctionForOptimization(ROR1_sa40);
assertEquals(ROR1(0x0000FFFF, 40), ROR1_sa40(0x0000FFFF));
assertEquals(ROR1(0x0000FFFF, 40), ROR1_sa40(0x0000FFFF));
%OptimizeFunctionOnNextCall(ROR1_sa40);
assertEquals(ROR1(0x0000FFFF, 40), ROR1_sa40(0x0000FFFF));

// ROR2_sa20
%PrepareFunctionForOptimization(ROR2_sa20);
assertEquals(ROR2(0xFFFFFFFF, 20), ROR2_sa20(0xFFFFFFFF));
assertEquals(ROR2(0xFFFFFFFF, 20), ROR2_sa20(0xFFFFFFFF));
%OptimizeFunctionOnNextCall(ROR2_sa20);
assertEquals(ROR2(0xFFFFFFFF, 20), ROR2_sa20(0xFFFFFFFF));

// ROR2_sa40
%PrepareFunctionForOptimization(ROR2_sa40);
assertEquals(ROR2(0x0000FFFF, 40), ROR2_sa40(0x0000FFFF));
assertEquals(ROR2(0x0000FFFF, 40), ROR2_sa40(0x0000FFFF));
%OptimizeFunctionOnNextCall(ROR2_sa40);
assertEquals(ROR2(0x0000FFFF, 40), ROR2_sa40(0x0000FFFF));

// ROR3_sa20
%PrepareFunctionForOptimization(ROR3_sa20);
assertEquals(ROR3(0x0000FFFF, 20), ROR3_sa20(0x0000FFFF));
assertEquals(ROR3(0x0000FFFF, 20), ROR3_sa20(0x0000FFFF));
%OptimizeFunctionOnNextCall(ROR3_sa20);
assertEquals(ROR3(0x0000FFFF, 20), ROR3_sa20(0x0000FFFF));

// ROR3_sa40
%PrepareFunctionForOptimization(ROR3_sa40);
assertEquals(ROR3(0x0000FFFF, 40), ROR3_sa40(0x0000FFFF));
assertEquals(ROR3(0x0000FFFF, 40), ROR3_sa40(0x0000FFFF));
%OptimizeFunctionOnNextCall(ROR3_sa40);
assertEquals(ROR3(0x0000FFFF, 40), ROR3_sa40(0x0000FFFF));

// ROR4_sa20
%PrepareFunctionForOptimization(ROR4_sa20);
assertEquals(ROR4(0x0000FFFF, 20), ROR4_sa20(0x0000FFFF));
assertEquals(ROR4(0x0000FFFF, 20), ROR4_sa20(0x0000FFFF));
%OptimizeFunctionOnNextCall(ROR4_sa20);
assertEquals(ROR4(0x0000FFFF, 20), ROR4_sa20(0x0000FFFF));

// ROR4_sa40
%PrepareFunctionForOptimization(ROR4_sa40);
assertEquals(ROR4(0xFFFFFFFF, 40), ROR4_sa40(0xFFFFFFFF));
assertEquals(ROR4(0xFFFFFFFF, 40), ROR4_sa40(0xFFFFFFFF));
%OptimizeFunctionOnNextCall(ROR4_sa40);
assertEquals(ROR4(0xFFFFFFFF, 40), ROR4_sa40(0xFFFFFFFF));
