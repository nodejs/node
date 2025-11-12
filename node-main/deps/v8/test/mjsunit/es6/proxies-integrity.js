// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.



function toKey(x) {
  if (typeof x === "symbol") return x;
  return String(x);
}


const noconf = {configurable: false};
const noconf_nowrite = {configurable: false, writable: false};


var symbol = Symbol();


var log = [];
var logger = {};
var handler = new Proxy({}, logger);

logger.get = function(t, trap, r) {
  return function() {
    log.push([trap, ...arguments]);
    return Reflect[trap](...arguments);
  }
};


(function Seal() {
  var target = [];
  var proxy = new Proxy(target, handler);
  log.length = 0;

  target.wurst = 42;
  target[0] = true;
  Object.defineProperty(target, symbol, {get: undefined});

  Object.seal(proxy);
  assertEquals(6, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["preventExtensions", target], log[0]);
  assertArrayEquals(
      ["ownKeys", target], log[1]);
  assertArrayEquals(
      ["defineProperty", target, toKey(0), noconf], log[2]);
  assertArrayEquals(
      ["defineProperty", target, toKey("length"), noconf], log[3]);
  assertArrayEquals(
      ["defineProperty", target, toKey("wurst"), noconf], log[4]);
  assertArrayEquals(
      ["defineProperty", target, toKey(symbol), noconf], log[5]);
})();


(function Freeze() {
  var target = [];
  var proxy = new Proxy(target, handler);
  log.length = 0;

  target.wurst = 42;
  target[0] = true;
  Object.defineProperty(target, symbol, {get: undefined});

  Object.freeze(proxy);
  assertEquals(10, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["preventExtensions", target], log[0]);
  assertArrayEquals(
      ["ownKeys", target], log[1]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(0)], log[2]);
  assertArrayEquals(
      ["defineProperty", target, toKey(0), noconf_nowrite], log[3]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey("length")], log[4]);
  assertArrayEquals(
      ["defineProperty", target, toKey("length"), noconf_nowrite], log[5]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey("wurst")], log[6]);
  assertArrayEquals(
      ["defineProperty", target, toKey("wurst"), noconf_nowrite], log[7]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(symbol)], log[8]);
  assertArrayEquals(
      ["defineProperty", target, toKey(symbol), noconf], log[9]);
})();


(function IsSealed() {
  var target = [];
  var proxy = new Proxy(target, handler);

  target.wurst = 42;
  target[0] = true;
  Object.defineProperty(target, symbol, {get: undefined});

  // Extensible.

  log.length = 0;

  Object.isSealed(proxy);
  assertEquals(1, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["isExtensible", target], log[0]);

  // Not extensible but not sealed.

  log.length = 0;
  Object.preventExtensions(target);

  Object.isSealed(proxy);
  assertEquals(3, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["isExtensible", target], log[0]);
  assertArrayEquals(
      ["ownKeys", target], log[1]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(0)], log[2]);

  // Sealed.

  log.length = 0;
  Object.seal(target);

  Object.isSealed(proxy);
  assertEquals(6, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["isExtensible", target], log[0]);
  assertArrayEquals(
      ["ownKeys", target], log[1]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(0)], log[2]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey("length")], log[3]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey("wurst")], log[4]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(symbol)], log[5]);
})();


(function IsFrozen() {
  var target = [];
  var proxy = new Proxy(target, handler);

  target.wurst = 42;
  target[0] = true;
  Object.defineProperty(target, symbol, {get: undefined});

  // Extensible.

  log.length = 0;

  Object.isFrozen(proxy);
  assertEquals(1, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["isExtensible", target], log[0]);

  // Not extensible but not frozen.

  log.length = 0;
  Object.preventExtensions(target);

  Object.isFrozen(proxy);
  assertEquals(3, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["isExtensible", target], log[0]);
  assertArrayEquals(
      ["ownKeys", target], log[1]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(0)], log[2]);

  // Frozen.

  log.length = 0;
  Object.freeze(target);

  Object.isFrozen(proxy);
  assertEquals(6, log.length)
  for (var i in log) assertSame(target, log[i][1]);

  assertArrayEquals(
      ["isExtensible", target], log[0]);
  assertArrayEquals(
      ["ownKeys", target], log[1]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(0)], log[2]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey("length")], log[3]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey("wurst")], log[4]);
  assertArrayEquals(
      ["getOwnPropertyDescriptor", target, toKey(symbol)], log[5]);
})();
