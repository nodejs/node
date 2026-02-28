import '../common/index.mjs';
import assert from 'assert';

import secret0 from '../fixtures/experimental.json' with { type: 'json' };
const secret1 = await import('../fixtures/experimental.json',
  { with: { type: 'json' } });

assert.strictEqual(secret0.ofLife, 42);
assert.strictEqual(secret1.default.ofLife, 42);
assert.strictEqual(secret1.default, secret0);
