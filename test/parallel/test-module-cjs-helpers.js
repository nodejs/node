'use strict';

const common = require('../common');

if (!process.execArgv.includes('--experimental-worker'))
  common.relaunchWithFlags(['--experimental-worker', '--expose-internals']);

const assert = require('assert');
const { builtinLibs } = require('internal/modules/cjs/helpers');

const hasInspector = process.config.variables.v8_enable_inspector === 1;

const expectedLibs = hasInspector ? 34 : 33;
assert.strictEqual(builtinLibs.length, expectedLibs);
