import '../common/index.mjs';
import assert from 'assert';

import asdf from
  '../fixtures/es-modules/package-type-module/nested-default-type/module.js';

assert.strictEqual(asdf, 'asdf');
