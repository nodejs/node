import test from 'node:test';

test('extraneous async activity test', () => {
  setTimeout(() => { throw new Error(); }, 100);
});
