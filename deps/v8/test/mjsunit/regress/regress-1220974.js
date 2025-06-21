// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --stress-concurrent-inlining --expose-gc

(function () {
  assertArrayEquals = function assertArrayEquals() {};
  assertContains = function () {};
  assertPromiseResult = function () {};
  isNeverOptimizeLiteMode = function isNeverOptimizeLiteMode() {};
  isUnoptimized = function isUnoptimized() {};
  isOptimized = function isOptimized() {};
  isTurboFanned = function isTurboFanned() {};
})();

(function (global) {
  assertEq = function assertEq() {}
  function reportFailure() {}
  global.reportFailure = reportFailure;
  function printStatus() {}
  global.printStatus = printStatus;
})(this);

(function (global) {
  global.completesNormally = function completesNormally() {};
  global.raisesException = function raisesException() {};
  global.deepEqual = function deepEqual() {}
  global.assertThrowsInstanceOf = function assertThrowsInstanceOf() {};
  global.assertDeepEq = function () {
  var call = Function.prototype.call,
    Map_ = Map,
    Map_set = call.bind(Map.prototype.set),
    Object_getPrototypeOf = Object.getPrototypeOf,
    Object_isExtensible = Object.isExtensible = Object.getOwnPropertyNames;
  function isPrimitive() {}
  return function assertDeepEq(a, b, options) {
    function assertSameProto() {
      check(a, Object_getPrototypeOf(b), );
    }
    function assertSameProps() {};
    var bpath = new Map_();
    function check(a, b) {
      if (typeof a === "symbol") {
      } else {
        Map_set(bpath, b);
        assertSameProto();
      }
    }
    check();
  };
}();
})(this);
function __isPropertyOfType(obj, name) {
  desc = Object.getOwnPropertyDescriptor( name);
  return typeof type === 'undefined' || typeof desc.value === type;
}
function __getProperties(obj, type) {
  let properties = [];
  for (let name of Object.getOwnPropertyNames(obj)) {
    if (__isPropertyOfType(obj, name, type)) properties.push(name);
  }
}
function* __getObjects(root = this, level = 0) {
  let obj_names = __getProperties(root);
  for (let obj_name of obj_names) {
  }
}
function __getRandomObject() {
  for (let obj of __getObjects()) {
  }
}
function runNearStackLimit() {}

try {
  __getRandomObject()(), {};
} catch (e) {}

try {
  delete __getRandomObject()[__getRandomProperty()]();
} catch (e) {}

try {
  __getRandomObject()[703044] =  __callGC();
} catch (e) {}

try {
  __getRandomObject(), {
    get: function () {}
  };
} catch (e) {}

try {
  __getRandomObject(), {};
} catch (e) {}

try {
  (function __f_30() {
    try {
      __getRandomObject()[__getRandomProperty()]();
    } catch (e) {}
    try {
      __getRandomObject()(), {};
    } catch (e) {}

    __getRandomObject()[1004383]();
  })();
} catch (e) {}

try {
  assertDeepEq(Array.from("anything"), []);
} catch (e) {}
