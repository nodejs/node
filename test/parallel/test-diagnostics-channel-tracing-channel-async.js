'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function check(found) {
  assert.deepStrictEqual(found, input);
}

const handlers = {
  start: common.mustCall(check, 2),
  end: common.mustCall(check, 2),
  asyncStart: common.mustCall((found) => {
    check(found);
    assert.strictEqual(found.error, undefined);
    assert.deepStrictEqual(found.result, expectedResult);
  }, 2),
  asyncEnd: common.mustCall((found) => {
    check(found);
    assert.strictEqual(found.error, undefined);
    assert.deepStrictEqual(found.result, expectedResult);
  }, 2),
  error: common.mustNotCall()
};

channel.subscribe(handlers);

channel.traceCallback(function(cb, err, res) {
  assert.deepStrictEqual(this, thisArg);
  setImmediate(cb, err, res);
}, 0, input, thisArg, common.mustCall((err, res) => {
  assert.strictEqual(err, null);
  assert.deepStrictEqual(res, expectedResult);
}), null, expectedResult);

channel.tracePromise(function(value) {
  assert.deepStrictEqual(this, thisArg);
  return Promise.resolve(value);
}, input, thisArg, expectedResult).then(
  common.mustCall((value) => {
    assert.deepStrictEqual(value, expectedResult);
  }),
  common.mustNotCall()
);

let failed = false;
try {
  channel.traceCallback(common.mustNotCall(), 0, input, thisArg, 1, 2, 3);
} catch (err) {
  assert.ok(/"callback" argument must be of type function/.test(err.message));
  failed = true;
}
assert.strictEqual(failed, true);
