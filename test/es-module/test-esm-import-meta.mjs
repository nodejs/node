// Flags: --experimental-modules

import '../common';
import assert from 'assert';

const fixtures = import.meta.require('../common/fixtures');

assert.strictEqual(Object.getPrototypeOf(import.meta), null);

const keys = ['url', 'require'];
assert.deepStrictEqual(Reflect.ownKeys(import.meta), keys);

const descriptors = Object.getOwnPropertyDescriptors(import.meta);

for (const descriptor of Object.values(descriptors)) {
  delete descriptor.value; // Values are verified below.
}

assert.deepStrictEqual(descriptors.url, {
  enumerable: true,
  writable: true,
  configurable: true
});

assert.deepStrictEqual(descriptors.require, {
  get: descriptors.require.get,
  set: undefined,
  enumerable: true,
  configurable: true
});

assert.strictEqual(import.meta.require.cache, undefined);
assert.strictEqual(import.meta.require.extensions, undefined);
assert.strictEqual(import.meta.require.main, undefined);
assert.strictEqual(import.meta.require.paths, undefined);

const urlReg = /^file:\/\/\/.*\/test\/es-module\/test-esm-import-meta\.mjs$/;
assert(import.meta.url.match(urlReg));

const a = import.meta.require(
  fixtures.path('module-require', 'relative', 'dot.js')
);
const b = import.meta.require(
  fixtures.path('module-require', 'relative', 'dot-slash.js')
);

assert.strictEqual(a.value, 42);
// require(".") should resolve like require("./")
assert.strictEqual(a, b);
