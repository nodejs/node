import assert from 'node:assert';
import { test } from 'node:test';
import { f } from './mod.mjs';

test('b', () => {
  assert.strictEqual(f(false, false), 'other');
});
