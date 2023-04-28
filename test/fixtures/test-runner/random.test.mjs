import test from 'node:test';

test('this should fail', () => {
  throw new Error('this is a failing test');
});
