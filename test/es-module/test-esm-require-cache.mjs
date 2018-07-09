// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */
import '../common/index.mjs';
import '../fixtures/es-module-require-cache/preload.js';
import '../fixtures/es-module-require-cache/counter.js';
import assert from 'assert';
assert.strictEqual(global.counter, 1);
delete global.counter;
