// Flags: --experimental-loader ./test/fixtures/es-module-loaders/loader-with-custom-condition.mjs
import '../common/index.mjs';
import assert from 'assert';

import * as ns from '../fixtures/es-modules/conditional-exports.mjs';

assert.deepStrictEqual({ ...ns }, { default: 'from custom condition' });
