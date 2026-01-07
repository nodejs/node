'use strict';
// Addons: test_async, test_async_vtable

const common = require('../../common');
const { addonPath } = require('../../common/addon-test');
const assert = require('assert');
const test_async = require(addonPath);
const iterations = 500;

let x = 0;
const workDone = common.mustCall((status) => {
  assert.strictEqual(status, 0);
  if (++x < iterations) {
    setImmediate(() => test_async.DoRepeatedWork(workDone));
  }
}, iterations);
test_async.DoRepeatedWork(workDone);
