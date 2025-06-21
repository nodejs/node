// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function testNonConstructorStrict() {
  "use strict";
  assertThrows(function() {
    return new Math.cos(...[1,2,3]);
  }, TypeError);

  assertThrows(function() {
    var CallNull = null;
    return new CallNull(...[1,2,3]);
  }, TypeError);
})();


(function testNonConstructorSloppy() {
  assertThrows(function() {
    return new Math.cos(...[1,2,3]);
  }, TypeError);

  assertThrows(function() {
    var CallNull = null;
    return new CallNull(...[1,2,3]);
  }, TypeError);
})();


(function testConstructStrict() {
  "use strict";
  function TestClass(a, b, c) {
    this.wasCalled = true;
    this.args = [a, b, c];
  }
  TestClass.prototype.method = function() {
    return this.args;
  }

  assertInstanceof(new TestClass(...[1, 2, 3]), TestClass);
  assertEquals([1, 2, 3], (new TestClass(...[1, 2, 3])).method());
  assertEquals([1, 2, 3], (new TestClass(...[1, 2, 3])).args);
  assertTrue((new TestClass(...[1, 2, 3])).wasCalled);
})();


(function testConstructSloppy() {
  function TestClass(a, b, c) {
    this.wasCalled = true;
    this.args = [a, b, c];
  }
  TestClass.prototype.method = function() {
    return this.args;
  }

  assertInstanceof(new TestClass(...[1, 2, 3]), TestClass);
  assertEquals([1, 2, 3], (new TestClass(...[1, 2, 3])).method());
  assertEquals([1, 2, 3], (new TestClass(...[1, 2, 3])).args);
  assertTrue((new TestClass(...[1, 2, 3])).wasCalled);
})();
