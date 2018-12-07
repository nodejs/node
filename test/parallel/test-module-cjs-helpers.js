'use strict';
// Flags: --expose-internals --experimental-worker

require('../common');
const assert = require('assert');
const { builtinLibs } = require('internal/modules/cjs/helpers');

const hasInspector = process.config.variables.v8_enable_inspector === 1;

const expectedLibs = hasInspector ? 36 : 31;
assert.strictEqual(builtinLibs.length, expectedLibs);
