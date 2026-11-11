'use strict';

// Regression test: the Web Worker API is enabled by default and must not
// require the `--experimental-web-worker` flag.
// Refs: https://github.com/nodejs/node/pull/65323

require('../common');
const assert = require('assert');
const { Worker: WorkerThread } = require('worker_threads');

assert.strictEqual(typeof Worker, 'function');
assert.strictEqual(Worker.name, 'Worker');
assert.notStrictEqual(Worker, WorkerThread);
