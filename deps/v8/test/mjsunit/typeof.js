// Copyright 2008 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// The type of a regular expression should be 'object', including in
// the context of string equality comparisons.
{
  const r = new RegExp;

  assertEquals('object', typeof r);
  assertTrue(typeof r == 'object');
  assertFalse(typeof r == 'function');

  function equals(x, y) { return x == y; }
  assertTrue(equals('object', typeof r));
}

assertFalse(typeof null == "undefined");

assertEquals('undefined', typeof undefined);
assertEquals('object', typeof null);
assertEquals('boolean', typeof true);
assertEquals('boolean', typeof false);
assertEquals('number', typeof 42.42);
assertEquals('number', typeof 42);
assertEquals('bigint', typeof 42n);
assertEquals('string', typeof '42');
assertEquals('symbol', typeof Symbol(42));
assertEquals('object', typeof {});
assertEquals('object', typeof []);
assertEquals('object', typeof new Proxy({}, {}));
assertEquals('object', typeof new Proxy([], {}));
assertEquals('function', typeof (_ => 42));
assertEquals('function', typeof function() {});
assertEquals('function', typeof function*() {});
assertEquals('function', typeof async function() {});
assertEquals('function', typeof async function*() {});
assertEquals('function', typeof new Proxy(_ => 42, {}));
assertEquals('function', typeof class {});
assertEquals('function', typeof Object);
