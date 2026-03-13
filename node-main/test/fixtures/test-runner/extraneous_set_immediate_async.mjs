import test from 'node:test';

test('extraneous async activity test', () => {
  setImmediate(() => { throw new Error(); });
});
