'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const nextChannel = dc.tracingChannel('test:next');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

// A sync iterator that will be returned from the async fn
const syncIter = [expectedResult][Symbol.iterator]();

function check(found) {
  assert.deepStrictEqual(found, input);
}

function checkAsync(found) {
  check(found);
  assert.strictEqual(found.error, undefined);
  assert.strictEqual(found.result, syncIter);
}

// async fn returns a Promise, so main channel fires asyncStart/asyncEnd
const handlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustCall(checkAsync),
  asyncEnd: common.mustCall(checkAsync),
  error: common.mustNotCall(),
};

// next() on the wrapped sync iterator returns synchronously
const nextHandlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

channel.subscribe(handlers);
nextChannel.subscribe(nextHandlers);

// fn is async: returns a Promise resolving to a sync iterator
// traceIterator returns a Promise resolving to the wrapped iterator
channel.traceIterator(common.mustCall(async function() {
  assert.deepStrictEqual(this, thisArg);
  return syncIter;
}), input, thisArg).then(common.mustCall((iter) => {
  assert.deepStrictEqual(iter.next(), { value: expectedResult, done: false });
}));
