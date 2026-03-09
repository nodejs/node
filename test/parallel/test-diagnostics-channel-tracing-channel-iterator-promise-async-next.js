'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const nextChannel = dc.tracingChannel('test:next');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

// An async generator that will be returned from the async fn
async function* asyncGen(value) {
  yield value;
}
const asyncIter = asyncGen(expectedResult);

function check(found) {
  assert.deepStrictEqual(found, input);
}

function checkAsync(found) {
  check(found);
  assert.strictEqual(found.error, undefined);
  assert.strictEqual(found.result, asyncIter);
}

function checkNextAsync(found) {
  check(found);
  assert.strictEqual(found.error, undefined);
  assert.deepStrictEqual(found.result, { value: expectedResult, done: false });
}

// Async fn returns a Promise, so main channel fires asyncStart/asyncEnd
const handlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustCall(checkAsync),
  asyncEnd: common.mustCall(checkAsync),
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

// Fn is async: returns a Promise resolving to an AsyncGenerator
// traceIterator returns a Promise resolving to the wrapped AsyncGenerator
channel.traceIterator(common.mustCall(async function() {
  assert.deepStrictEqual(this, thisArg);
  return asyncIter;
}), input, thisArg).then(common.mustCall((iter) => {
  // next() on AsyncGenerator returns a Promise
  iter.next().then(common.mustCall((result) => {
    assert.deepStrictEqual(result, { value: expectedResult, done: false });
  }));
}));
