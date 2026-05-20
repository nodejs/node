import test from 'node:test';

test('should not wait for assertions and fail immediately', async (t) => {
  t.plan(1, { wait: false });

  // Set up an async operation that won't complete before the test finishes
  // Since wait:false, the test should fail immediately without waiting
  setTimeout(() => {
    t.assert.ok(true);
  }, 1000).unref();
});
