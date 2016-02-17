// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --strong-mode

"use strict";

function getClass() {
  class Foo {
    static get bar() { return 0 }
    get bar() { return 0 }
  }
  return Foo;
}

function getClassExpr() {
  return (class { static get bar() { return 0 } get bar() { return 0 } });
}

function getClassStrong() {
  "use strong";
  class Foo {
    static get bar() { return 0 }
    get bar() { return 0 }
  }
  return Foo;
}

function getClassExprStrong() {
  "use strong";
  return (class { static get bar() { return 0 } get bar() { return 0 } });
}

function addProperty(o) {
  o.baz = 1;
}

function convertPropertyToData(o) {
  assertTrue(o.hasOwnProperty("bar"));
  Object.defineProperty(o, "bar", { value: 1 });
}

function testWeakClass(classFunc) {
  assertDoesNotThrow(function(){addProperty(classFunc())});
  assertDoesNotThrow(function(){addProperty(classFunc().prototype)});
  assertDoesNotThrow(function(){convertPropertyToData(classFunc())});
  assertDoesNotThrow(function(){convertPropertyToData(classFunc().prototype)});
}

function testStrongClass(classFunc) {
  assertThrows(function(){addProperty(classFunc())}, TypeError);
  assertThrows(function(){addProperty(classFunc().prototype)}, TypeError);
  assertThrows(function(){convertPropertyToData(classFunc())}, TypeError);
  assertThrows(function(){convertPropertyToData(classFunc().prototype)},
               TypeError);
}

testWeakClass(getClass);
testWeakClass(getClassExpr);

testStrongClass(getClassStrong);
testStrongClass(getClassExprStrong);

// Check strong classes don't freeze their parents.
(function() {
  let parent = getClass();

  let classFunc = function() {
    "use strong";
    class Foo extends parent {
      static get bar() { return 0 }
      get bar() { return 0 }
    }
    return Foo;
  }

  testStrongClass(classFunc);
  assertDoesNotThrow(function(){addProperty(parent)});
  assertDoesNotThrow(function(){convertPropertyToData(parent)});
})();

// Check strong classes don't freeze their children.
(function() {
  let parent = getClassStrong();

  let classFunc = function() {
    class Foo extends parent {
      static get bar() { return 0 }
      get bar() { return 0 }
    }
    return Foo;
  }

  assertThrows(function(){addProperty(parent)}, TypeError);
  assertThrows(function(){convertPropertyToData(parent)}, TypeError);
  testWeakClass(classFunc);
})();
