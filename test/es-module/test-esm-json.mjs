// Flags: --experimental-modules --experimental-json-modules
import '../common/index.mjs';
import { strictEqual } from 'assert';

import secret from '../fixtures/experimental.json';

strictEqual(secret.ofLife, 42);
