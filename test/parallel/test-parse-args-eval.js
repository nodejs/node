'use strict';
const common = require('../common');
const assert = require('assert');
const { getMainArgs } = require('../../lib/internal/util/parse_args/parse_args');

const originalArgv = process.argv;
const originalExecArgv = process.execArgv;

common.mustCall(() => {
  process.argv = ['node', '--eval=code', 'arg1', 'arg2'];
  process.execArgv = ['--eval=code'];
  const evalArgs = getMainArgs();
  assert.deepStrictEqual(evalArgs, ['--eval=code', 'arg1', 'arg2']);

  process.argv = ['node', 'script.js', 'arg1', 'arg2'];
  process.execArgv = [];
  const normalArgs = getMainArgs();
  assert.deepStrictEqual(normalArgs, ['script.js', 'arg1', 'arg2']);
})();

process.argv = originalArgv;
process.execArgv = originalExecArgv;
