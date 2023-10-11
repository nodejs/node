import '../common/index.mjs';
import { strictEqual } from 'assert';

import secret from '../fixtures/experimental.json' with { type: 'json', unsupportedAttribute: 'should ignore' };

strictEqual(secret.ofLife, 42);
