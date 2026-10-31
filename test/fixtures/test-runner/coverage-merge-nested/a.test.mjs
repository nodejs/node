import assert from 'node:assert';
import { test } from 'node:test';
import { f } from './mod.mjs';

test('a', () => {
  assert.strictEqual(f(true, false), 'a');
  assert.strictEqual(f(false, false), 'other');
});
