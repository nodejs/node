'use strict';

require('../common');

// Minimal test for dns benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

const env = Object.assign({}, process.env,
                          { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });

const child = fork(runjs,
                   ['--set', 'n=1',
                    '--set', 'all=false',
                    '--set', 'name=127.0.0.1',
                    'dns'],
                   { env });

child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
