'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const nextChannel = dc.tracingChannel('test:next');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function check(found) {
  assert.deepStrictEqual(found, input);
}

const handlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

// next() and return() are each called once
const nextHandlers = {
  start: common.mustCall(check, 2),
  end: common.mustCall(check, 2),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

channel.subscribe(handlers);
nextChannel.subscribe(nextHandlers);

const iter = channel.traceIterator(common.mustCall(function*(value) {
  assert.deepStrictEqual(this, thisArg);
  yield value;
}), input, thisArg, expectedResult);

assert.deepStrictEqual(iter.next(), { value: expectedResult, done: false });
assert.deepStrictEqual(iter.return(), { value: undefined, done: true });
