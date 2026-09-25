'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

class SpoofedPromise extends Promise {
  customMethod() {
    return 'works';
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

let innerPromise;

const result = channel.tracePromise(common.mustCall(function() {
  innerPromise = SpoofedPromise.resolve(expectedResult);
  // Spoof the constructor to try to trick the brand check
  innerPromise.constructor = Promise;
  return innerPromise;
}), input, thisArg);

// Despite the spoofed constructor, the original subclass instance should be
// returned directly so that custom methods remain accessible.
assert(result instanceof SpoofedPromise);
assert.strictEqual(result, innerPromise);
assert.strictEqual(result.customMethod(), 'works');
