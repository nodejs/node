// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

var heap_number_producer = {y:1.5};
heap_number_producer.y = 0;
var heap_number_zero = heap_number_producer.y;
var non_constant_eight = {};
non_constant_eight = 8;

function BreakIt() {
  return heap_number_zero | (1 | non_constant_eight);
}

function expose(a, b, c) {
  return b;
}

assertEquals(9, expose(8, 9, 10));
assertEquals(9, expose(8, BreakIt(), 10));
assertEquals(9, BreakIt());
