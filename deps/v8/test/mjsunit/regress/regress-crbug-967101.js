// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// For packed sealed object.
function packedStore() {
  let a = Object.seal([""]);
  a[0] = 0;
  assertEquals(a[0], 0);
}

packedStore();
packedStore();

// For holey sealed object.
function holeyStore() {
  let a = Object.seal([, ""]);
  a[0] = 0;
  assertEquals(a[0], undefined);
}

holeyStore();
holeyStore();

// Make sure IC store for holey is consistent.
let a = Object.seal([, ""]);
function foo() {
  a[1] = 0;
}

foo();
foo();
function bar() {
  a[0] = 1;
}
assertEquals(a, [, 0]);
bar();
assertEquals(a, [, 0]);
bar();
assertEquals(a, [, 0]);
function baz() {
  a[2] = 2;
}
assertEquals(a, [, 0]);
baz();
assertEquals(a, [, 0]);
baz();
assertEquals(a, [, 0]);
