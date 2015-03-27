// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


(function TestGetter() {
  var o = {
    get x() {}
  };
  var desc = Object.getOwnPropertyDescriptor(o, 'x');
  assertEquals('function', typeof desc.get);
  assertFalse('prototype' in desc.get);

  assertThrows(function() {
    new desc.get();
  }, TypeError);
})();


(function TestSetter() {
  var o = {
    set x(_) {}
  };
  var desc = Object.getOwnPropertyDescriptor(o, 'x');
  assertEquals('function', typeof desc.set);
  assertFalse('prototype' in desc.set);

  assertThrows(function() {
    new desc.set();
  }, TypeError);
})();


(function TestBoth() {
  var o = {
    get x() {},
    set x(_) {}
  };
  var desc = Object.getOwnPropertyDescriptor(o, 'x');
  assertEquals('function', typeof desc.get);
  assertEquals('function', typeof desc.set);
  assertFalse('prototype' in desc.get);
  assertFalse('prototype' in desc.set);

  assertThrows(function() {
    new desc.get();
  }, TypeError);
  assertThrows(function() {
    new desc.set();
  }, TypeError);
})();
