// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test that Array builtins can be called on primitive values.
var values = [ 23, 4.2, true, false, 0/0 ];
for (var i = 0; i < values.length; ++i) {
  var v = values[i];
  Array.prototype.join.call(v);
  Array.prototype.pop.call(v);
  Array.prototype.push.call(v);
  Array.prototype.reverse.call(v);
  Array.prototype.shift.call(v);
  Array.prototype.slice.call(v);
  Array.prototype.splice.call(v);
  Array.prototype.unshift.call(v);
}

// Test that ToObject on primitive values is only called once.
var length_receiver, element_receiver;
function length() { length_receiver = this; return 2; }
function element() { element_receiver = this; return "x"; }
Object.defineProperty(Number.prototype, "length", { get:length, set:length });
Object.defineProperty(Number.prototype, "0", { get:element, set:element });
Object.defineProperty(Number.prototype, "1", { get:element, set:element });
Object.defineProperty(Number.prototype, "2", { get:element, set:element });
function test_receiver(expected, call_string) {
  assertDoesNotThrow(call_string);
  assertEquals(new Number(expected), length_receiver);
  assertSame(length_receiver, element_receiver);
}

test_receiver(11, "Array.prototype.join.call(11)")
test_receiver(23, "Array.prototype.pop.call(23)");
test_receiver(42, "Array.prototype.push.call(42, 'y')");
test_receiver(49, "Array.prototype.reverse.call(49)");
test_receiver(65, "Array.prototype.shift.call(65)");
test_receiver(77, "Array.prototype.slice.call(77, 1)");
test_receiver(88, "Array.prototype.splice.call(88, 1, 1)");
test_receiver(99, "Array.prototype.unshift.call(99, 'z')");
