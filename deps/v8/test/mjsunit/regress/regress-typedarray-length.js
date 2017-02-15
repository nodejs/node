// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var a = new Int32Array(100);
a.__proto__ = null;

function get(a) {
  return a.length;
}

assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
%OptimizeFunctionOnNextCall(get);
assertEquals(undefined, get(a));

get = function(a) {
  return a.byteLength;
}

assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
%OptimizeFunctionOnNextCall(get);
assertEquals(undefined, get(a));

get = function(a) {
  return a.byteOffset;
}

assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
%OptimizeFunctionOnNextCall(get);
assertEquals(undefined, get(a));

(function() {
  "use strict";

  class MyTypedArray extends Int32Array {
    get length() {
      return "length";
    }
  }

  a = new MyTypedArray();

  get = function(a) {
    return a.length;
  }

  assertEquals("length", get(a));
  assertEquals("length", get(a));
  assertEquals("length", get(a));
  %OptimizeFunctionOnNextCall(get);
  assertEquals("length", get(a));

  a.__proto__ = null;

  get = function(a) {
    return a.length;
  }

  assertEquals(undefined, get(a));
  assertEquals(undefined, get(a));
  assertEquals(undefined, get(a));
  %OptimizeFunctionOnNextCall(get);
  assertEquals(undefined, get(a));
})();

(function() {
  "use strict";

  class MyTypedArray extends Int32Array {
    constructor(length) {
      super(length);
    }
  }

  a = new MyTypedArray(1024);

  get = function(a) {
    return a.length;
  }

  assertEquals(1024, get(a));
  assertEquals(1024, get(a));
  assertEquals(1024, get(a));
  %OptimizeFunctionOnNextCall(get);
  assertEquals(1024, get(a));
})();

(function() {
  "use strict";
  var a = new Uint8Array(4);
  Object.defineProperty(a, "length", {get: function() { return "blah"; }});
  get = function(a) {
    return a.length;
  }

  assertEquals("blah", get(a));
  assertEquals("blah", get(a));
  assertEquals("blah", get(a));
  %OptimizeFunctionOnNextCall(get);
  assertEquals("blah", get(a));
})();

// Ensure we can delete length, byteOffset, byteLength.
assertTrue(Int32Array.prototype.__proto__.hasOwnProperty("length"));
assertTrue(Int32Array.prototype.__proto__.hasOwnProperty("byteOffset"));
assertTrue(Int32Array.prototype.__proto__.hasOwnProperty("byteLength"));
assertTrue(delete Int32Array.prototype.__proto__.length);
assertTrue(delete Int32Array.prototype.__proto__.byteOffset);
assertTrue(delete Int32Array.prototype.__proto__.byteLength);

a = new Int32Array(100);

get = function(a) {
  return a.length;
}

assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
%OptimizeFunctionOnNextCall(get);
assertEquals(undefined, get(a));

get = function(a) {
  return a.byteLength;
}

assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
%OptimizeFunctionOnNextCall(get);
assertEquals(undefined, get(a));

get = function(a) {
  return a.byteOffset;
}

assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
assertEquals(undefined, get(a));
%OptimizeFunctionOnNextCall(get);
assertEquals(undefined, get(a));
