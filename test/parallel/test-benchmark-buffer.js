'use strict';

require('../common');

// Minimal test for buffer benchmarks. This makes sure the benchmarks aren't
// completely broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');
const argv = ['--set', 'aligned=true',
              '--set', 'args=1',
              '--set', 'buffer=fast',
              '--set', 'encoding=utf8',
              '--set', 'len=2',
              '--set', 'method=',
              '--set', 'n=1',
              '--set', 'noAssert=true',
              '--set', 'pieces=1',
              '--set', 'pieceSize=1',
              '--set', 'search=@',
              '--set', 'size=1',
              '--set', 'source=array',
              '--set', 'type=',
              '--set', 'withTotalLength=0',
              'buffers'];

const child = fork(runjs, argv);
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
