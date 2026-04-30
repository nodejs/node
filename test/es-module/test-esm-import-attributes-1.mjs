import '../common/index.mjs';
import assert from 'assert';

import secret from '../fixtures/experimental.json' with { type: 'json' };

assert.strictEqual(secret.ofLife, 42);
