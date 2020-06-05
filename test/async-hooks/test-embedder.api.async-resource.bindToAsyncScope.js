'use strict';
require('../common');
const assert = require('assert');
const { AsyncResource, executionAsyncId } = require('async_hooks');

const a = new AsyncResource('foobar');

function foo(bar) {
  assert.strictEqual(executionAsyncId(), a.asyncId());
  return bar;
}

const ret = a.bindToAsyncScope(foo)('baz');
assert.strictEqual(ret, 'baz');
