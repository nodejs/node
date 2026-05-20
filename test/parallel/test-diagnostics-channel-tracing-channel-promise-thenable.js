'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

class ResolvedThenable {
  #result;
  constructor(value) {
    this.#result = value;
  }
  then(resolve) {
    return new ResolvedThenable(resolve(this.#result));
  }
}

const channel = dc.tracingChannel('test');

const expectedResult = { foo: 'bar' };
const input = { foo: 'bar' };
const thisArg = { baz: 'buz' };

function check(found) {
  assert.strictEqual(found, input);
}

function checkAsync(found) {
  check(found);
  assert.strictEqual(found.error, undefined);
  assert.deepStrictEqual(found.result, expectedResult);
}

const handlers = {
  start: common.mustCall(check),
  end: common.mustCall(check),
  asyncStart: common.mustCall(checkAsync),
  asyncEnd: common.mustCall(checkAsync),
  error: common.mustNotCall()
};

channel.subscribe(handlers);

let innerThenable;

const result = channel.tracePromise(common.mustCall(function(value) {
  assert.deepStrictEqual(this, thisArg);
  innerThenable = new ResolvedThenable(value);
  return innerThenable;
}), input, thisArg, expectedResult);

assert(result instanceof ResolvedThenable);
assert.notStrictEqual(result, innerThenable);
result.then(common.mustCall((value) => {
  assert.deepStrictEqual(value, expectedResult);
}));
