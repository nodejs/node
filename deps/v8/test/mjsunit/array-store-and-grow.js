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

// Verifies that the KeyedStoreIC correctly handles out-of-bounds stores
// to an array that grow it by a single element. Test functions are
// called twice to make sure that the IC is used, first call is handled
// by the runtime in the miss stub.

function array_store_1(a,b,c) {
  return (a[b] = c);
}

// Check handling of the empty array.
var a = [];
array_store_1(a, 0, 1);
a = [];
array_store_1(a, 0, 1);
assertEquals(1, a[0]);
assertEquals(1, array_store_1([], 0, 1));

a = [];
for (x=0;x<100000;++x) {
  assertEquals(x, array_store_1(a, x, x));
}

for (x=0;x<100000;++x) {
  assertEquals(x, array_store_1([], 0, x));
}

function array_store_2(a,b,c) {
  return (a[b] = c);
}

a = [];
array_store_2(a, 0, 0.5);
a = [];
array_store_2(a, 0, 0.5);
assertEquals(0.5, a[0]);
assertEquals(0.5, array_store_2([], 0, 0.5));

function array_store_3(a,b,c) {
  return (a[b] = c);
}

x = new Object();
a = [];
array_store_3(a, 0, x);
a = [];
array_store_3(a, 0, x);
assertEquals(x, a[0]);
assertEquals(x, array_store_3([], 0, x));

// Check the handling of COW arrays
function makeCOW() {
  return [1];
}

function array_store_4(a,b,c) {
  return (a[b] = c);
}

a = makeCOW();
array_store_4(a, 1, 1);
a = makeCOW();
array_store_4(a, 1, 1);
assertEquals(1, a[1]);
assertEquals(1, array_store_4([], 1, 1));

function array_store_5(a,b,c) {
  return (a[b] = c);
}

a = makeCOW();
array_store_5(a, 1, 0.5);
a = makeCOW();
array_store_5(a, 1, 0.5);
assertEquals(0.5, a[1]);
assertEquals(0.5, array_store_5([], 1, 0.5));

function array_store_6(a,b,c) {
  return (a[b] = c);
}

a = makeCOW();
array_store_6(a, 1, x);
a = makeCOW();
array_store_6(a, 1, x);
assertEquals(x, a[1]);
assertEquals(x, array_store_6([], 1, x));

// Check the handling of mutable arrays.
a = new Array(1,2,3);
array_store_4(a, 3, 1);
a = new Array(1,2,3);
array_store_4(a, 3, 1);
assertEquals(1, a[3]);
assertEquals(1, array_store_4([], 3, 1));

function array_store_5(a,b,c) {
  return (a[b] = c);
}

a = new Array(1,2,3);
array_store_5(a, 3, 0.5);
a = new Array(1,2,3);
array_store_5(a, 3, 0.5);
assertEquals(0.5, a[3]);
assertEquals(0.5, array_store_5([], 3, 0.5));

function array_store_6(a,b,c) {
  return (a[b] = c);
}

a = new Array(1,2,3);
array_store_6(a, 3, x);
a = new Array(1,2,3);
array_store_6(a, 3, x);
assertEquals(x, a[3]);
assertEquals(x, array_store_6([], 3, x));

function array_store_7(a,b,c) {
  return (a[b] = c);
}

// Check the handling of mutable arrays of doubles
var a = new Array(0.5, 1.5);
array_store_7(a, 2, .5);
a = new Array(0.5, 1.5);
array_store_7(a, 2, .5);
assertEquals(0.5, a[2]);
a = new Array(0.5, 1.5);
assertEquals(0.5, array_store_7(a, 2, 0.5));

for (x=0;x<100000;++x) {
  a = new Array(0.5, 1.5);
  assertEquals(x, array_store_7(a, 2, x));
}

function array_store_8(a,b,c) {
  return (a[b] = c);
}

var a = new Array(0.5, 1.5);
array_store_8(a, 2, .5);
a = new Array(0.5, 1.5);
array_store_8(a, 10, .5);
assertEquals(0.5, a[10]);

// Grow the empty array with a double store.
function array_store_9(a,b,c) {
  return (a[b] = c);
}

var a = [];
array_store_9(a, 0, 0.5);
a = [];
array_store_1(a, 0, 0.5);
assertEquals(0.5, a[0]);
assertEquals(0.5, array_store_1([], 0, 0.5));
