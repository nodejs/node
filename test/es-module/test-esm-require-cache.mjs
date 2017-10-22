// Flags: --experimental-modules
import '../common';
import '../fixtures/es-module-require-cache/preload.js';
import '../fixtures/es-module-require-cache/counter.js';
import assert from 'assert';
assert.strictEqual(global.counter, 1);
delete global.counter;
