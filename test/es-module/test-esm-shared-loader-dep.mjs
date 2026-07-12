// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-shared-dep.mjs
import { createRequire } from '../common/index.mjs';

import assert from 'assert';
import '../fixtures/es-modules/test-esm-ok.mjs';

const require = createRequire(import.meta.url);
const dep = require('../fixtures/es-module-loaders/loader-dep.js');

assert.strictEqual(dep.format, 'module');
