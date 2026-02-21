// Flags: --expose-internals
'use strict';

require('../common');

// Tests basic functionality of util.deprecate().

const assert = require('assert');
const util = require('util');
const internalUtil = require('internal/util');

const expectedWarnings = new Map();

// Deprecated function length is preserved
for (const fn of [
  function() {},
  function(a) {},
  function(a, b, c) {},
  function(...args) {},
  function(a, b, c, ...args) {},
  () => {},
  (a) => {},
  (a, b, c) => {},
  (...args) => {},
  (a, b, c, ...args) => {},
]) {
  assert.strictEqual(util.deprecate(fn).length, fn.length);
  assert.strictEqual(internalUtil.pendingDeprecate(fn).length, fn.length);
}

// Emits deprecation only once if same function is called.
{
  const msg = 'fhqwhgads';
  const fn = util.deprecate(() => {}, msg);
  expectedWarnings.set(msg, { code: undefined, count: 1 });
  fn();
  fn();
}

// Emits deprecation twice for different functions.
{
  const msg = 'sterrance';
  const fn1 = util.deprecate(() => {}, msg);
  const fn2 = util.deprecate(() => {}, msg);
  expectedWarnings.set(msg, { code: undefined, count: 2 });
  fn1();
  fn2();
}

// Emits deprecation only once if optional code is the same, even for different
// functions.
{
  const msg = 'cannonmouth';
  const code = 'deprecatesque';
  const fn1 = util.deprecate(() => {}, msg, code);
  const fn2 = util.deprecate(() => {}, msg, code);
  expectedWarnings.set(msg, { code, count: 1 });
  fn1();
  fn2();
  fn1();
  fn2();
}


// Test modifyPrototype option
{
  const msg = 'prototype-test';
  const code = 'proto-code';

  function OriginalFn() {}
  OriginalFn.prototype.testMethod = function() { return 'test'; };

  const deprecatedWithoutProto = util.deprecate(OriginalFn, msg, code, { modifyPrototype: false });

  assert.notStrictEqual(deprecatedWithoutProto.prototype, OriginalFn.prototype);
  assert.notStrictEqual(Object.getPrototypeOf(deprecatedWithoutProto), OriginalFn);
  assert.strictEqual(deprecatedWithoutProto.prototype.testMethod, undefined);

  const deprecatedWithProto = util.deprecate(OriginalFn, msg, code);

  assert.strictEqual(deprecatedWithProto.prototype, OriginalFn.prototype);
  assert.strictEqual(Object.getPrototypeOf(deprecatedWithProto), OriginalFn);
  assert.strictEqual(typeof deprecatedWithProto.prototype.testMethod, 'function');
}

process.on('warning', (warning) => {
  assert.strictEqual(warning.name, 'DeprecationWarning');
  assert.ok(expectedWarnings.has(warning.message));
  const expected = expectedWarnings.get(warning.message);
  assert.strictEqual(warning.code, expected.code);
  expected.count = expected.count - 1;
  if (expected.count === 0)
    expectedWarnings.delete(warning.message);
});

process.on('exit', () => {
  assert.deepStrictEqual(expectedWarnings, new Map());
});
