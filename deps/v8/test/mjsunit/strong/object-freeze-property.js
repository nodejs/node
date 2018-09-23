// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode --allow-natives-syntax

// TODO(conradw): Track implementation of strong bit for other objects, add
// tests.

function getSloppyObjects() {
  return [(function(){}), ({})];
}

function getStrictObjects() {
  "use strict";
  return [(function(){}), ({})];
}

function getStrongObjects() {
  "use strong";
  // Strong functions can't have properties added to them.
  return [{}];
}

(function testStrongObjectFreezePropValid() {
  "use strict";
  let strongObjects = getStrongObjects();

  for (let o of strongObjects) {
    Object.defineProperty(o, "foo", { configurable: true, writable: true });
    assertDoesNotThrow(
      function() {
        "use strong";
        Object.defineProperty(o, "foo", {configurable: true, writable: false });
      });
  }
})();

(function testStrongObjectFreezePropInvalid() {
  "use strict";
  let sloppyObjects = getSloppyObjects();
  let strictObjects = getStrictObjects();
  let strongObjects = getStrongObjects();
  let weakObjects = sloppyObjects.concat(strictObjects);

  for (let o of weakObjects) {
    Object.defineProperty(o, "foo", { writable: true });
    assertDoesNotThrow(
      function() {
        "use strong";
        Object.defineProperty(o, "foo", { writable: false });
      });
  }
  for (let o of strongObjects) {
    function defProp(o) {
      Object.defineProperty(o, "foo", { writable: false });
    }
    function defProps(o) {
      Object.defineProperties(o, { "foo": { writable: false } });
    }
    function freezeProp(o) {
      Object.freeze(o);
    }
    Object.defineProperty(o, "foo", { writable: true });
    for (let func of [defProp, defProps, freezeProp]) {
      assertThrows(function(){func(o)}, TypeError);
      assertThrows(function(){func(o)}, TypeError);
      assertThrows(function(){func(o)}, TypeError);
      %OptimizeFunctionOnNextCall(func);
      assertThrows(function(){func(o)}, TypeError);
      %DeoptimizeFunction(func);
      assertThrows(function(){func(o)}, TypeError);
    }
  }
})();
