import '../common/index.mjs';
// eslint-disable-next-line node-core/must-call-assert
import assert, { strict } from 'assert';
// eslint-disable-next-line node-core/must-call-assert
import assertStrict from 'assert/strict';

assert.strictEqual(strict, assertStrict);
