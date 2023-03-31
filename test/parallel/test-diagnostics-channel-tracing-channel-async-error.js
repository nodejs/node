'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');

const expectedError = new Error('test');
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function check(found) {
  assert.deepStrictEqual(found, input);
}

const handlers = {
  start: common.mustCall(check, 2),
  end: common.mustCall(check, 2),
  asyncStart: common.mustCall(check, 2),
  asyncEnd: common.mustCall(check, 2),
  error: common.mustCall((found) => {
    check(found);
    assert.deepStrictEqual(found.error, expectedError);
  }, 2)
};

channel.subscribe(handlers);

channel.traceCallback(function(cb, err) {
  assert.deepStrictEqual(this, thisArg);
  setImmediate(cb, err);
}, 0, input, thisArg, common.mustCall((err, res) => {
  assert.strictEqual(err, expectedError);
  assert.strictEqual(res, undefined);
}), expectedError);

channel.tracePromise(function(value) {
  assert.deepStrictEqual(this, thisArg);
  return Promise.reject(value);
}, input, thisArg, expectedError).then(
  common.mustNotCall(),
  common.mustCall((value) => {
    assert.deepStrictEqual(value, expectedError);
  })
);
