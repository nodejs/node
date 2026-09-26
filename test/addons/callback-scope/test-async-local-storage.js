'use strict';

const common = require('../../common');
const assert = require('assert');
const { AsyncLocalStorage } = require('async_hooks');
const { runInCallbackScope } = require(`./build/${common.buildType}/binding`);

// A CallbackScope must restore the async context frame that was active
// before it, when there was none and when there was one.

const als = new AsyncLocalStorage();

runInCallbackScope({}, 0, 0, common.mustCall(() => {
  als.enterWith('inner');
}));
assert.strictEqual(als.getStore(), undefined);

als.run('outer', common.mustCall(() => {
  runInCallbackScope({}, 0, 0, common.mustCall(() => {
    als.enterWith('inner');
  }));
  assert.strictEqual(als.getStore(), 'outer');
}));
