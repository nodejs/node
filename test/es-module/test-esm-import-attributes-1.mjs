import '../common/index.mjs';
import { strictEqual } from 'assert';

import secret from '../fixtures/experimental.json' with { type: 'json' };

strictEqual(secret.ofLife, 42);
