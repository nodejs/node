'use strict';

const common = require('../common');

common.requireFlags(['--experimental-worker', '--expose-internals']);

const assert = require('assert');
const { builtinLibs } = require('internal/modules/cjs/helpers');
console.log(builtinLibs);
const hasInspector = process.config.variables.v8_enable_inspector === 1;

const expectedLibs = hasInspector ? 34 : 33;
assert.strictEqual(builtinLibs.length, expectedLibs);
