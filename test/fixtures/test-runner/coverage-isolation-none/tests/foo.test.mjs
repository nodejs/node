import test from 'node:test';
import assert from 'node:assert';
import { add, sub } from '../src/foo.mjs';

test('add', () => {
  assert.strictEqual(add(2, 3), 5);
});

test('sub', () => {
  assert.strictEqual(sub(5, 3), 2);
});
