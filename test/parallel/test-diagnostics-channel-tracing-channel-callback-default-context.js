'use strict';

const common = require('../common');
const assert = require('assert');
const dc = require('diagnostics_channel');

for (const expectedError of [null, new Error('test')]) {
  const channel = dc.tracingChannel(`test-${expectedError ? 'error' : 'result'}`);
  const contexts = [];
  const results = [{ value: 1 }, { value: 2 }];
  let completed = 0;

  channel.subscribe({
    start: common.mustCall((context) => {
      assert.strictEqual(context.result, undefined);
      assert.strictEqual(context.error, undefined);
      for (const previous of contexts) {
        assert.notStrictEqual(context, previous);
      }
      contexts.push(context);
    }, 2),
    asyncStart: common.mustCall((context) => {
      assert.strictEqual(context, contexts[completed]);
      if (expectedError) {
        assert.strictEqual(context.error, expectedError);
        assert.strictEqual(context.result, undefined);
      } else {
        assert.strictEqual(context.result, results[completed]);
        assert.strictEqual(context.error, undefined);
      }
      completed++;
    }, 2),
    asyncEnd: common.mustCall(2),
    error: expectedError ? common.mustCall((context) => {
      assert.strictEqual(context, contexts[completed]);
      assert.strictEqual(context.error, expectedError);
    }, 2) : common.mustNotCall(),
  });

  for (const result of results) {
    channel.traceCallback(common.mustCall((callback) => {
      setImmediate(callback, expectedError, expectedError ? undefined : result);
    }), undefined, undefined, undefined, common.mustCall((err, value) => {
      assert.strictEqual(err, expectedError);
      assert.strictEqual(value, expectedError ? undefined : result);
    }));
  }

  setImmediate(common.mustCall(() => {
    assert.strictEqual(completed, 2);
    for (const [index, context] of contexts.entries()) {
      assert.strictEqual(context.error, expectedError || undefined);
      assert.strictEqual(context.result, expectedError ? undefined : results[index]);
    }
  }));
}
