'use strict';

require('../common');
const vm = require('vm');
const assert = require('assert');

// Object.defineProperty should only modify specified attributes and preserve
// unspecified ones. Regression test for https://github.com/nodejs/node/issues/64008
{
  const context = {};
  vm.createContext(context);

  // Changing enumerable should preserve value.
  vm.runInContext(`
    globalThis.a1 = 1;
    Object.defineProperty(globalThis, 'a1', { enumerable: false });
    Object.defineProperty(globalThis, 'a2', {
      value: 2, enumerable: false, configurable: true,
    });
    Object.defineProperty(globalThis, 'a2', { enumerable: true });
  `, context);
  assert.strictEqual(context.a1, 1);
  assert.strictEqual(
    Object.getOwnPropertyDescriptor(context, 'a1').enumerable, false);
  assert.strictEqual(context.a2, 2);
  assert.strictEqual(
    Object.getOwnPropertyDescriptor(context, 'a2').enumerable, true);

  // Changing writable should preserve value.
  vm.runInContext(`
    globalThis.b1 = 1;
    Object.defineProperty(globalThis, 'b1', { writable: false });
    Object.defineProperty(globalThis, 'b2', {
      value: 2, writable: false, configurable: true,
    });
    Object.defineProperty(globalThis, 'b2', { writable: true });
  `, context);
  assert.strictEqual(context.b1, 1);
  assert.strictEqual(context.b2, 2);

  // Changing value should preserve enumerable.
  vm.runInContext(`
    globalThis.c1 = 1;
    Object.defineProperty(globalThis, 'c1', { value: 2 });
    Object.defineProperty(globalThis, 'c2', {
      value: 1, enumerable: false, configurable: true,
    });
    Object.defineProperty(globalThis, 'c2', { value: 2 });
  `, context);
  assert.strictEqual(context.c1, 2);
  assert.strictEqual(
    Object.getOwnPropertyDescriptor(context, 'c1').enumerable, true);
  assert.strictEqual(context.c2, 2);
  assert.strictEqual(
    Object.getOwnPropertyDescriptor(context, 'c2').enumerable, false);
}
