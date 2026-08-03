import '../common/index.mjs';
import assert from 'node:assert';
import { b, c } from '../fixtures/es-modules/require-module-instantiated/a.mjs';
assert.strictEqual(b, c);
