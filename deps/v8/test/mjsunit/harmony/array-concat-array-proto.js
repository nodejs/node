// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Check that @@isConcatSpreadable is checked when set on Object.prototype

"use strict"

var array = [1, 2, 3];
var object = {length: 1, '0': 'a'};

function testConcatDefaults() {
  assertEquals(array, [].concat(array));
  assertEquals(array, array.concat([]));
  assertEquals([1, 2, 3, 1, 2, 3], array.concat(array));
  assertEquals([object], [].concat(object));
  assertEquals([1, 2, 3, object], array.concat(object));
  assertEquals([object], Array.prototype.concat.call(object,[]));
  assertEquals([object, 1, 2, 3], Array.prototype.concat.call(object, array));
  assertEquals([object, object], Array.prototype.concat.call(object, object));
}

testConcatDefaults();

Array.prototype[Symbol.isConcatSpreadable] = false;

assertEquals([[], array], [].concat(array));
assertEquals([array, []], array.concat([]));
assertEquals([array, array], array.concat(array));
assertEquals([[], object], [].concat(object));
assertEquals([array, object], array.concat(object));
assertEquals([object, []], Array.prototype.concat.call(object,[]));
assertEquals([object, array], Array.prototype.concat.call(object, array));
assertEquals([object, object], Array.prototype.concat.call(object, object));

Array.prototype[Symbol.isConcatSpreadable] = true;

assertEquals(array, [].concat(array));
assertEquals(array, array.concat([]));
assertEquals([1, 2, 3, 1, 2, 3], array.concat(array));
assertEquals([object], [].concat(object));
assertEquals([1, 2, 3, object], array.concat(object));
assertEquals([object], Array.prototype.concat.call(object,[]));
assertEquals([object, 1, 2, 3], Array.prototype.concat.call(object, array));
assertEquals([object, object], Array.prototype.concat.call(object, object));

delete Array.prototype[Symbol.isConcatSpreadable];
testConcatDefaults();
