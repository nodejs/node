import test from 'node:test';

test('failing planning by options', { plan: 1 }, () => {
  // Should fail - no assertions
});

test('passing planning by options', { plan: 1 }, (t) => {
  t.assert.ok(true);
});
