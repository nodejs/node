'use strict';
const common = require('../common');
const ArrayStream = require('../common/arraystream');
const repl = require('repl');

common.allowGlobals('require', '_', '_error', ...require('module').builtinModules);

// Regression test for https://github.com/nodejs/node/issues/6802
const input = new ArrayStream();
repl.start({ input, output: process.stdout, useGlobal: true });
input.run(['let process']);
