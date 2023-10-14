import '../common/index.mjs';
import { strictEqual } from 'assert';

import secret0 from '../fixtures/experimental.json' with { type: 'json' };
const secret1 = await import('../fixtures/experimental.json',
  { with: { type: 'json' } });

strictEqual(secret0.ofLife, 42);
strictEqual(secret1.default.ofLife, 42);
strictEqual(secret1.default, secret0);
