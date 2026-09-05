'use strict';
// Each flaky retry must start from a clean MockTracker: a mock applied on a
// failing attempt must be restored before the next attempt runs. The body reads
// the method at entry (must be the real implementation) before re-mocking, and
// fails the first two attempts to force retries.
const { test } = require('node:test');
const assert = require('node:assert');

let attempt = 0;
const obj = { fn() { return 'real'; } };

test('mock is reset between retries', { flaky: 3 }, (t) => {
  attempt++;
  const atEntry = obj.fn();
  t.mock.method(obj, 'fn', () => 'mocked');
  if (attempt < 3) {
    assert.fail('force retry');
  }
  assert.strictEqual(atEntry, 'real',
                     `attempt ${attempt} started with obj.fn()='${atEntry}', expected 'real'`);
});
