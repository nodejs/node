// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f_1() {
  var v = new Array();
  v[0] = 10;
  return v;
}

function test() {
  var setter_called = false;
  // Turn array to NumberDictionary
  Array.prototype[123456789] = 42;
  assertEquals(f_1().length, 1);

  // Reset to empty_slow_dictionary
  Array.prototype.length = 0;

  // This should reset the prototype validity cell.
  Array.prototype.__defineSetter__("0", function() {setter_called = true});
  f_1();
  assertEquals(setter_called, true);
}
test();
