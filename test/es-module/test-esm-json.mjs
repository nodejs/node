// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */
import '../common/index.mjs';
import assert from 'assert';
import ok from '../fixtures/es-modules/test-esm-ok.mjs';
import json from '../fixtures/es-modules/json.json';

assert(ok);
assert.strictEqual(json.val, 42);
