'use strict';

require('../common');

// Minimal test for zlib benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');
const argv = ['--set', 'method=deflate',
              '--set', 'n=1',
              '--set', 'options=true',
              '--set', 'type=Deflate',
              'zlib'];

const child = fork(runjs, argv);
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
