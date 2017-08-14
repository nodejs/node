'use strict';

const common = require('../common');

if (!common.hasCrypto)
  common.skip('missing crypto');

if (common.hasFipsCrypto)
  common.skip('some benchmarks are FIPS-incompatible');

// Minimal test for crypto benchmarks. This makes sure the benchmarks aren't
// horribly broken but nothing more than that.

const assert = require('assert');
const fork = require('child_process').fork;
const path = require('path');

const runjs = path.join(__dirname, '..', '..', 'benchmark', 'run.js');
const argv = ['--set', 'algo=sha256',
              '--set', 'api=stream',
              '--set', 'keylen=1024',
              '--set', 'len=1',
              '--set', 'n=1',
              '--set', 'out=buffer',
              '--set', 'type=buf',
              '--set', 'v=crypto',
              '--set', 'writes=1',
              'crypto'];
const env = Object.assign({}, process.env,
                          { NODEJS_BENCHMARK_ZERO_ALLOWED: 1 });
const child = fork(runjs, argv, { env });
child.on('exit', (code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
});
