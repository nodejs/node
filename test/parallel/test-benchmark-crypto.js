'use strict';

const common = require('../common');

if (!common.hasCrypto) {
  common.skip('missing crypto');
  return;
}

if (common.hasFipsCrypto) {
  common.skip('some benchmarks are FIPS-incompatible');
  return;
}

// Minimal test for crypto benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');
const argv = ['--set', 'n=1',
              '--set', 'writes=1',
              '--set', 'len=1',
              '--set', 'api=stream',
              '--set', 'out=buffer',
              '--set', 'keylen=1024',
              '--set', 'type=buf',
              'crypto'];

const child = fork(runjs, argv, {env: {NODEJS_BENCHMARK_ZERO_ALLOWED: 1}});
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
