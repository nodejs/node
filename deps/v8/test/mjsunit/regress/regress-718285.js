// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo_reference(n) {
  var array = new Int32Array(n + 1);
  for (var i = 0; i < n; ++i) {
    array[i] = i;
  }
  var array2 = new Int32Array(array);
  array2.set(new Uint8Array(array.buffer, 0, n), 1);
  return array2;
}

function foo(n) {
  var array = new Int32Array(n + 1);
  for (var i = 0; i < n; ++i) {
    array[i] = i;
  }
  array.set(new Uint8Array(array.buffer, 0, n), 1);
  return array;
}

function bar_reference(n) {
  var array = new Int32Array(n + 1);
  for (var i = 0; i < n; ++i) {
    array[i] = i;
  }
  var array2 = new Int32Array(array);
  array2.set(new Uint8Array(array.buffer, 34), 0);
  return array2;
}

function bar(n) {
  var array = new Int32Array(n + 1);
  for (var i = 0; i < n; ++i) {
    array[i] = i;
  }
  array.set(new Uint8Array(array.buffer, 34), 0);
  return array;
}

foo(10);
foo_reference(10);
bar(10);
bar_reference(10);
