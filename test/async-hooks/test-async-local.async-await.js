'use strict';

const common = require('../common');
const assert = require('assert');
const async_hooks = require('async_hooks');
const { AsyncLocal } = async_hooks;

const asyncLocal = new AsyncLocal();

async function asyncFunc() {
  return new Promise((resolve) => {
    setTimeout(resolve, 0);
  });
}

async function testAwait() {
  asyncLocal.set('foo');
  await asyncFunc();
  assert.strictEqual(asyncLocal.get(), 'foo');
}

testAwait().then(common.mustCall(() =>
  assert.strictEqual(asyncLocal.get(), 'foo')
));
