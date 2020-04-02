// Flags: --expose-internals --max-worker-threads=-1
'use strict';

// Check that when --max-worker-threads is negative,
// the option value is auto-calculated based on the
// number of CPUs

require('../common');
const { getOptionValue } = require('internal/options');
const { cpus } = require('os');
const assert = require('assert');

// Make sure the flag is actually set
assert(process.execArgv.indexOf('--max-worker-threads=-1') > -1);

const kWorkerThreadsMultiplier = 4;
const maxWorkerThreads = getOptionValue('--max-worker-threads');

assert.strictEqual(cpus().length * kWorkerThreadsMultiplier, maxWorkerThreads);
