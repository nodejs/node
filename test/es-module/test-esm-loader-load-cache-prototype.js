'use strict';
// Flags: --expose-internals

require('../common');

const assert = require('assert');
const { LoadCache } = require('internal/modules/esm/module_map');

const cache = new LoadCache();
const url = 'file:///module.mjs';
const job = () => {};

assert.strictEqual(cache.get(url, '__proto__'), undefined);
assert.strictEqual(cache.has(url, 'constructor'), false);
cache.set(url, '__proto__', job);
assert.strictEqual(cache.get(url, '__proto__'), job);
assert.strictEqual(cache.has(url, '__proto__'), true);
assert.strictEqual(cache.has(url, 'constructor'), false);
cache.set(url, 'constructor', job);
assert.strictEqual(cache.get(url, 'constructor'), job);
cache.delete(url, '__proto__');
assert.strictEqual(cache.get(url, '__proto__'), undefined);
