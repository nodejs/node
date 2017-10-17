// Flags: --experimental-modules
/* eslint-disable required-modules */
import '../common/index';
import assert from 'assert';
import ok from './test-esm-ok.mjs';
import json from './json.json';

assert(ok);
assert.strictEqual(json.val, 42);
