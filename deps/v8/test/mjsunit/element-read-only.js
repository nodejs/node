// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f(a, b, c, d) { return arguments; }

// Ensure non-configurable argument elements stay non-configurable.
(function () {
  var args = f(1);
  Object.defineProperty(args, "0", {value: 10, configurable: false});
  %HeapObjectVerify(args);
  assertFalse(Object.getOwnPropertyDescriptor(args, "0").configurable);
  %HeapObjectVerify(args);
  for (var i = 0; i < 10; i++) {
    args[i] = 1;
  }
  %HeapObjectVerify(args);
  assertFalse(Object.getOwnPropertyDescriptor(args, "0").configurable);
  %HeapObjectVerify(args);
})();

// Ensure read-only properties on the prototype chain cause TypeError.

// Newly added.
(function () {
  var o = [];
  var proto = {};
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();

// Reconfigured.
(function () {
  var o = [];
  var proto = {3: 10000};
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();

// Newly added to arguments object.
(function () {
  var o = [];
  var proto = f(100);
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();

// Reconfigured on to arguments object.
(function () {
  var o = [];
  var proto = f(100, 200, 300, 400);
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();

// Extensions prevented object.
(function () {
  var o = [];
  var proto = [0, 1, 2, 3];
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.preventExtensions(proto);
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();
// Extensions prevented arguments object.
(function () {
  var o = [];
  var proto = f(100, 200, 300, 400);
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.preventExtensions(proto);
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();

// Array with large index.
(function () {
  var o = [];
  var proto = [];
  var index = 3;
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < index; i++) {
    store(o, i, 0);
  }
  proto[1 << 30] = 1;
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.defineProperty(proto, index, {value: 100, writable: false});
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, index, 0); });
  assertEquals(100, o[index]);
})();

// Frozen object.
(function () {
  var o = [];
  var proto = [0, 1, 2, 3];
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < 3; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.freeze(proto);
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, 3, 0); });
  assertEquals(3, o[3]);
})();

// Frozen arguments object.
(function () {
  var o = [];
  var proto = f(0, 1, 2, 3);
  function store(o, i, v) { "use strict"; o[i] = v; };
  o.__proto__ = proto;
  for (var i = 0; i < 3; i++) {
    store(o, i, 0);
  }
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  Object.freeze(proto);
  %HeapObjectVerify(proto);
  %HeapObjectVerify(o);
  assertThrows(function() { store(o, 3, 0); });
  assertEquals(3, o[3]);
})();
