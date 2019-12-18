'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

assert.strictEqual(new AsyncLocal().get(), undefined);

const asyncLocal = new AsyncLocal();

assert.strictEqual(asyncLocal.get(), undefined);

asyncLocal.set(42);
assert.strictEqual(asyncLocal.get(), 42);
asyncLocal.set('foo');
assert.strictEqual(asyncLocal.get(), 'foo');
const obj = {};
asyncLocal.set(obj);
assert.strictEqual(asyncLocal.get(), obj);

asyncLocal.remove();
assert.strictEqual(asyncLocal.get(), undefined);

// Throws on modification after removal
const error = common.expectsError({
  code: 'ERR_ASYNC_LOCAL_CANNOT_SET_VALUE',
  name: 'Error',
});
assert.throws(() => asyncLocal.set('bar'), error);

// Subsequent .remove() does not throw
asyncLocal.remove();
