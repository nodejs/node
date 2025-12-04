import test from 'node:test';

test('more assertions than planned', (t) => {
  t.plan(1);
  t.assert.ok(true, 'first assertion');
  t.assert.ok(true, 'extra assertion'); // This should cause failure
});
