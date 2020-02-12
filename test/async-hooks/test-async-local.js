'use strict';

require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

assert.strictEqual(new AsyncLocal().unwrap(), undefined);

const asyncLocal = new AsyncLocal();

assert.strictEqual(asyncLocal.unwrap(), undefined);

asyncLocal.store(42);
assert.strictEqual(asyncLocal.unwrap(), 42);
asyncLocal.store('foo');
assert.strictEqual(asyncLocal.unwrap(), 'foo');
const obj = {};
asyncLocal.store(obj);
assert.strictEqual(asyncLocal.unwrap(), obj);

asyncLocal.disable();
assert.strictEqual(asyncLocal.unwrap(), undefined);

// Does not throw when disabled
asyncLocal.store('bar');

// Subsequent .disable() does not throw
asyncLocal.disable();
