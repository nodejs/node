'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };

function check(found) {
  assert.deepStrictEqual(found, input);
}

const handlers = {
  start: common.mustCall(check),
  end: common.mustCall((found) => {
    check(found);
    assert.deepStrictEqual(found.result, expectedResult);
  }),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall()
};

assert.strictEqual(channel.hasSubscribers, false);
channel.subscribe(handlers);
assert.strictEqual(channel.hasSubscribers, true);
channel.traceSync(() => {
  return expectedResult;
}, input);

channel.unsubscribe(handlers);
assert.strictEqual(channel.hasSubscribers, false);
channel.traceSync(() => {
  return expectedResult;
}, input);
