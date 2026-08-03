import '../common/index.mjs';
import assert from 'node:assert';

import '../fixtures/recursive-a.cjs';

assert.strictEqual(global.counter, 1);
delete global.counter;
