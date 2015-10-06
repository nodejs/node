// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f(a, b, c, d) { return arguments; }

// Ensure non-configurable argument elements stay non-configurable.
(function () {
  var args = f(1);
  Object.defineProperty(args, "0", {value: 10, configurable: false});
  assertFalse(Object.getOwnPropertyDescriptor(args, "0").configurable);
  for (var i = 0; i < 10; i++) {
    args[i] = 1;
  }
  assertFalse(Object.getOwnPropertyDescriptor(args, "0").configurable);
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
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.preventExtensions(proto);
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.preventExtensions(proto);
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.defineProperty(proto, index, {value: 100, writable: false});
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
  Object.freeze(proto);
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
  Object.freeze(proto);
  assertThrows(function() { store(o, 3, 0); });
  assertEquals(3, o[3]);
})();
