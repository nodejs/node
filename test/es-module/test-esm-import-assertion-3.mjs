// Flags: --experimental-json-modules
import '../common/index.mjs';
import { strictEqual } from 'assert';

import secret0 from '../fixtures/experimental.json' assert { type: 'json' };
const secret1 = await import('../fixtures/experimental.json',
  { assert: { type: 'json' } });

strictEqual(secret0.ofLife, 42);
strictEqual(secret1.default.ofLife, 42);
strictEqual(secret1.default, secret0);
