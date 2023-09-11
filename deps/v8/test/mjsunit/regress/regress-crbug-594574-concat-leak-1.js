// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --expose-gc

array = new Array(10);
array[0] = 0.1;
// array[1] = THE_HOLE, reading through the prototype chain
array[2] = 2.1;
array[3] = 3.1;

var copy = array.slice(0, array.length);

// Change the array's prototype.
var proto = {};
array.__proto__ = proto;

// Define [1] on the prototype to alter the array during concatenation.
Object.defineProperty(
  proto, 1, {
    get() {
      // Alter the array.
      array.length = 1;
      // Force gc to move the array.
      gc();
      return "value from proto";
    },
    set(new_value) { }
});

var concatted_array = Array.prototype.concat.call(array);
assertEquals(concatted_array[0], 0.1);
assertEquals(concatted_array[1], "value from proto");
assertEquals(concatted_array[2], undefined);
assertEquals(concatted_array[3], undefined);
