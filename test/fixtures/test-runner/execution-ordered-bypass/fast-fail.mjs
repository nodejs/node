import { test } from 'node:test';
import assert from 'node:assert';

test('fast-fail', (t) => {
  t.log('live');
  assert.fail('fast');
});
