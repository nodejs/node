// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Array.prototype[0] = 777;
var kElements = 10;

var input_array = [];
for (var i = 1; i < kElements; i++) {
  input_array[i] = 0.5;
}
var output_array = input_array.concat(0.5);

assertEquals(kElements + 1, output_array.length);
assertEquals(777, output_array[0]);
for (var j = 1; j < kElements; j++) {
  assertEquals(0.5, output_array[j]);
}
