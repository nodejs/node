'use strict';
const common = require('../../common');
const assert = require('assert');
const test_async = require(`./build/${common.buildType}/test_async`);
const iterations = 500;

let x = 0;
const workDone = common.mustCall((status) => {
  assert.strictEqual(status, 0);
  if (++x < iterations) {
    setImmediate(() => test_async.DoRepeatedWork(workDone));
  }
}, iterations);
test_async.DoRepeatedWork(workDone);
