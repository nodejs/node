// Flags: --experimental-modules
import { createRequire } from '../common';
import assert from 'assert';
//
const require = createRequire(import.meta.url);

require('../fixtures/es-module-require-cache/preload.js');
require('../fixtures/es-module-require-cache/counter.js');

assert.strictEqual(global.counter, 1);
delete global.counter;
