'use strict';
const common = require('../common');
const repl = require('repl');

common.globalCheck = false;

// Regression test for https://github.com/nodejs/node/issues/6802
const input = new common.ArrayStream();
repl.start({ input, output: process.stdout, useGlobal: true });
input.run(['let process']);
