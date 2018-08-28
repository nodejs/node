// Flags: --experimental-modules
/* eslint-disable node-core/required-modules */

import '../common/index.mjs';
import assert from 'assert';

import legacyLoader from
  '../fixtures/esm-package-scope/legacy-loader/index.mjs';
import newLoader from '../fixtures/esm-package-scope/new-loader/index.js';

assert.strictEqual(legacyLoader, 'legacy-loader');
assert.strictEqual(newLoader, 'new-loader');
