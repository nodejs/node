'use strict';

require('../common');

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

const child = fork(runjs, ['--set', 'dur=0',
                           '--set', 'n=1',
                           '--set', 'len=1',
                           '--set', 'params=1',
                           '--set', 'methodName=execSync',
                           'child_process'],
                   {env: {NODEJS_BENCHMARK_ZERO_ALLOWED: 1}});
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
