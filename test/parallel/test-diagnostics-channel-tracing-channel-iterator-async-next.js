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

function checkNextAsync(found) {
  check(found);
  assert.strictEqual(found.error, undefined);
  assert.deepStrictEqual(found.result, { value: expectedResult, done: false });
}

// Async function* returns an AsyncGenerator synchronously, so no asyncStart/asyncEnd
// for the fn call itself
const handlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

// next() on an AsyncGenerator returns a Promise
const nextHandlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustCall(checkNextAsync),
  asyncEnd: common.mustCall(checkNextAsync),
  error: common.mustNotCall(),
};

channel.subscribe(handlers);
nextChannel.subscribe(nextHandlers);

const iter = channel.traceIterator(common.mustCall(async function*(value) {
  assert.deepStrictEqual(this, thisArg);
  yield value;
}), input, thisArg, expectedResult);

// next() returns a Promise since iter is an AsyncGenerator
iter.next().then(common.mustCall((result) => {
  assert.deepStrictEqual(result, { value: expectedResult, done: false });
}));
