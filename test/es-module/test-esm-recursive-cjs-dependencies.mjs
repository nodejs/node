import '../common/index.mjs';
import { strictEqual } from 'node:assert';

import '../fixtures/recursive-a.cjs';

strictEqual(global.counter, 1);
delete global.counter;
