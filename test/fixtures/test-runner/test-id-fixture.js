'use strict';
const { describe, it } = require('node:test');
const assert = require('node:assert');

// Factory that creates subtests at the SAME source location.
// Multiple concurrent `it` blocks calling this will have subtests
// sharing file:line:column — but each should get a distinct testId.
function makeSubtest(shouldFail) {
  return async function(t) {
    await t.test('e2e', async () => {
      if (shouldFail) assert.fail('intentional');
    });
  };
}

describe('suite', { concurrency: 10_000 }, () => {
  it('test-A (passes)', makeSubtest(false));
  it('test-B (passes)', makeSubtest(false));
  it('test-C (fails)', makeSubtest(true));
  it('test-D (passes)', makeSubtest(false));
});
