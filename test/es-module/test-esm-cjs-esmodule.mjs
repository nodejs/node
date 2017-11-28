// Flags: --experimental-modules
/* eslint-disable required-modules */

import assert from 'assert';
import eightyfour, { fourtytwo } from
  '../fixtures/es-module-loaders/babel-to-esm.js';

assert.strictEqual(eightyfour, 84);
assert.strictEqual(fourtytwo, 42);
