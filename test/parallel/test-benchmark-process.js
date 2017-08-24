'use strict';

require('../common');

// Minimal test for process benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');
const argv = ['--set', 'millions=0.000001',
              '--set', 'n=1',
              '--set', 'type=raw',
              'process'];

const child = fork(runjs, argv);
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
