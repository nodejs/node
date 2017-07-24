'use strict';

require('../common');

// Minimal test for timers benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');
const argv = ['--set', 'type=depth',
              '--set', 'millions=0.000001',
              '--set', 'thousands=0.001',
              'timers'];

const child = fork(runjs, argv, {env: {NODEJS_BENCHMARK_ZERO_ALLOWED: 1}});
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
