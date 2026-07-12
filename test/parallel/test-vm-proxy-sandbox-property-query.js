'use strict';

require('../common');
const assert = require('node:assert');
const vm = require('node:vm');

// This test ensures that Proxy-backed vm sandboxes report own properties
// consistently across descriptor and membership queries.

const sandbox = new Proxy({}, {});
const ctx = vm.createContext(sandbox);

vm.runInContext(`
Object.defineProperty(this, 'foo', {
  value: 42,
  writable: true,
  enumerable: true,
  configurable: true,
});
Object.defineProperty(this, 'hidden', {
  value: 1,
  enumerable: false,
  configurable: true,
});
Object.defineProperty(this, 'readonly', {
  value: 2,
  writable: false,
  enumerable: true,
  configurable: true,
});
`, ctx);

const result = vm.runInContext(`({
  value: foo,
  descriptor: Object.getOwnPropertyDescriptor(globalThis, 'foo'),
  ownNamesIncludes: Object.getOwnPropertyNames(globalThis).includes('foo'),
  hasOwnProperty:
    Object.prototype.hasOwnProperty.call(globalThis, 'foo'),
  objectHasOwn: Object.hasOwn(globalThis, 'foo'),
  inGlobalThis: 'foo' in globalThis,
  reflectHas: Reflect.has(globalThis, 'foo'),
  keysIncludesFoo: Object.keys(globalThis).includes('foo'),
  keysIncludesHidden: Object.keys(globalThis).includes('hidden'),
  hiddenIsEnumerable:
    Object.prototype.propertyIsEnumerable.call(globalThis, 'hidden'),
  readonlyDescriptor:
    Object.getOwnPropertyDescriptor(globalThis, 'readonly'),
  hasOwnToString: Object.hasOwn(globalThis, 'toString'),
  toStringInGlobalThis: 'toString' in globalThis,
})`, ctx);

assert.strictEqual(result.value, 42);
assert.deepStrictEqual({ ...result.descriptor }, {
  value: 42,
  writable: true,
  enumerable: true,
  configurable: true,
});
assert.strictEqual(result.ownNamesIncludes, true);
assert.strictEqual(result.hasOwnProperty, true);
assert.strictEqual(result.objectHasOwn, true);
assert.strictEqual(result.inGlobalThis, true);
assert.strictEqual(result.reflectHas, true);
assert.strictEqual(result.keysIncludesFoo, true);
assert.strictEqual(result.keysIncludesHidden, false);
assert.strictEqual(result.hiddenIsEnumerable, false);
assert.deepStrictEqual({ ...result.readonlyDescriptor }, {
  value: 2,
  writable: false,
  enumerable: true,
  configurable: true,
});
assert.strictEqual(result.hasOwnToString, false);
assert.strictEqual(result.toStringInGlobalThis, true);
