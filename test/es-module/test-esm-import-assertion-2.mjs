// Flags: --experimental-json-modules
import '../common/index.mjs';
import { strictEqual } from 'assert';

// eslint-disable-next-line max-len
import secret from '../fixtures/experimental.json' assert { type: 'json', unsupportedAssertion: 'should ignore' };

strictEqual(secret.ofLife, 42);
