'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function checkStart(found) {
  assert.strictEqual(found, input);
}

function checkEnd(found) {
  checkStart(found);
  assert.strictEqual(found.error, undefined);
  assert.deepStrictEqual(found.result, expectedResult);
}

const handlers = {
  start: common.mustCall(checkStart),
  end: common.mustCall(checkEnd),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall()
};

channel.subscribe(handlers);

process.on('warning', common.mustCall((warning) => {
  assert.strictEqual(
    warning.message,
    "tracePromise was called with the function '<anonymous>', which returned a non-thenable."
  );
}));

assert.strictEqual(
  channel.tracePromise(common.mustCall(function(value) {
    assert.deepStrictEqual(this, thisArg);
    return value;
  }), input, thisArg, expectedResult),
  expectedResult,
);
