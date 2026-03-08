'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const channel = dc.tracingChannel('test');
const nextChannel = dc.tracingChannel('test:next');

const expectedError = new Error('test');
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function check(found) {
  assert.deepStrictEqual(found, input);
}

function checkError(found) {
  check(found);
  assert.deepStrictEqual(found.error, expectedError);
}

// Two traceIterator calls: one for next() error, one for throw() error
const handlers = {
  start: common.mustCall(check, 2),
  end: common.mustCall(check, 2),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustNotCall(),
};

// next() and throw() each produce one error
const nextHandlers = {
  start: common.mustCall(check, 2),
  end: common.mustCall(check, 2),
  asyncStart: common.mustNotCall(),
  asyncEnd: common.mustNotCall(),
  error: common.mustCall(checkError, 2),
};

channel.subscribe(handlers);
nextChannel.subscribe(nextHandlers);

// Test next(): generator throws on first next() call
const iter1 = channel.traceIterator(common.mustCall(function*() {
  assert.deepStrictEqual(this, thisArg);
  throw expectedError;
}), input, thisArg);

assert.throws(() => iter1.next(), expectedError);

// Test throw(): propagates error through the iterator
const iter2 = channel.traceIterator(common.mustCall(function*() {
  yield 1;
}), input, thisArg);

assert.throws(() => iter2.throw(expectedError), expectedError);
