'use strict';

const common = require('../common');
const dc = require('diagnostics_channel');
const assert = require('assert');

const kSuccess = Symbol('kSuccess');
const kFailure = Symbol('kFailure');

class DerivedPromise extends Promise {}

const fulfilledThenable = {
  then(resolve) {
    resolve(kSuccess);
    return this;
  },
};
const rejectedThenable = {
  then(_, reject) {
    reject(kFailure);
    return this;
  },
};

function check(event) {
  if (event.expectError) {
    assert.strictEqual(event.error, kFailure);
    assert.strictEqual(event.result, undefined);
  } else {
    assert.strictEqual(event.error, undefined);
    assert.strictEqual(event.result, kSuccess);
  }
}

let i = 0;

function test(expected, { withSubscribers, expectAsync, expectError }) {
  const channel = dc.tracingChannel(`test${i++}`);

  if (withSubscribers) {
    channel.subscribe({
      start: common.mustCall(),
      end: common.mustCall(),
      asyncStart: expectAsync ? common.mustCall(check) : common.mustNotCall(),
      asyncEnd: expectAsync ? common.mustCall(check) : common.mustNotCall(),
      error: expectError ? common.mustCall(check) : common.mustNotCall(),
    });
  }

  const result = channel.tracePromise(() => expected, { expectError });
  assert.strictEqual(result, expected);
}

// Should trace Promises and thenables, and return them unaltered
const testValues = [
  [DerivedPromise.resolve(kSuccess), false],
  [DerivedPromise.reject(kFailure), true],
  [fulfilledThenable, false],
  [rejectedThenable, true],
];
for (const [value, expectError] of testValues) {
  test(value, { withSubscribers: false });
  test(value, { withSubscribers: true, expectAsync: true, expectError });
}

// Should return non-thenable values unaltered, and should not call async handlers
test(kSuccess, { withSubscribers: false });
test(kSuccess, { withSubscribers: true, expectAsync: false, expectError: false });

// Should throw synchronous exceptions unaltered
{
  const channel = dc.tracingChannel(`test${i++}`);

  const error = new Error('Synchronous exception');
  const throwError = () => { throw error; };

  assert.throws(() => channel.tracePromise(throwError), error);

  channel.subscribe({
    start: common.mustCall(),
    end: common.mustCall(),
    asyncStart: common.mustNotCall(),
    asyncEnd: common.mustNotCall(),
    error: common.mustCall(),
  });

  assert.throws(() => channel.tracePromise(throwError), error);
}
