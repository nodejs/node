'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };
const arg = { baz: 'buz' };

function check(found) {
  assert.strictEqual(found, input);
}

const handlers = {
  start: common.mustCall(check),
  end: common.mustCall((found) => {
    check(found);
    assert.strictEqual(found.result, expectedResult);
  }),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall()
};

assert.strictEqual(channel.start.hasSubscribers, false);
channel.subscribe(handlers);
assert.strictEqual(channel.start.hasSubscribers, true);
const result1 = channel.traceSync(function(arg1) {
  assert.strictEqual(arg1, arg);
  assert.strictEqual(this, thisArg);
  return expectedResult;
}, input, thisArg, arg);
assert.strictEqual(result1, expectedResult);

channel.unsubscribe(handlers);
assert.strictEqual(channel.start.hasSubscribers, false);
const result2 = channel.traceSync(function(arg1) {
  assert.strictEqual(arg1, arg);
  assert.strictEqual(this, thisArg);
  return expectedResult;
}, input, thisArg, arg);
assert.strictEqual(result2, expectedResult);
