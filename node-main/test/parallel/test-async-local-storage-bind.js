'use strict';

const common = require('../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');

[1, false, '', {}, []].forEach((i) => {
  assert.throws(() => AsyncLocalStorage.bind(i), {
    code: 'ERR_INVALID_ARG_TYPE'
  });
});

const fn = common.mustCall(AsyncLocalStorage.bind(() => 123));
assert.strictEqual(fn(), 123);

const fn2 = AsyncLocalStorage.bind(common.mustCall((arg) => assert.strictEqual(arg, 'test')));
fn2('test');
