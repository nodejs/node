import { test } from 'node:test';
import assert from 'node:assert';

test('fast-fail', () => {
  assert.fail('fast');
});
