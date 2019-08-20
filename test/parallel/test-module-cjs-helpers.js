'use strict';
// Flags: --expose-internals

require('../common');
const assert = require('assert');
const { builtinLibs } = require('internal/modules/cjs/helpers');

const expectedLibs = process.features.inspector ? 34 : 33;
assert.strictEqual(builtinLibs.length, expectedLibs);
