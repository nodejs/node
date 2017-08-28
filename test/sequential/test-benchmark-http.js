'use strict';

const common = require('../common');

if (!common.enoughTestMem)
  common.skip('Insufficient memory for HTTP benchmark test');

// Minimal test for http benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

// Because the http benchmarks use hardcoded ports, this should be in sequential
// rather than parallel to make sure it does not conflict with tests that choose
// random available ports.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');

const env = Object.assign({}, process.env,
                          { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });

const child = fork(runjs, ['--set', 'benchmarker=test-double',
                           '--set', 'c=1',
                           '--set', 'chunkedEnc=true',
                           '--set', 'chunks=0',
                           '--set', 'dur=0.1',
                           '--set', 'key=""',
                           '--set', 'len=1',
                           '--set', 'method=write',
                           '--set', 'n=1',
                           '--set', 'res=normal',
                           'http'],
                   { env });
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
