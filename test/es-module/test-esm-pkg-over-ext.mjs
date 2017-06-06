// Flags: --experimental-modules
/* eslint-disable required-modules */

import resolved from '../fixtures/module-pkg-over-ext/inner';
import expected from '../fixtures/module-pkg-over-ext/inner/package.json';
import assert from 'assert';

assert.strictEqual(resolved, expected);
