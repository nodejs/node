'use strict';
const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const v8 = require('v8');
const { Worker } = require('worker_threads');

// Verify that Workers don't care about --stack-size, as they have their own
// fixed and known stack sizes.

async function runWorker() {
  const empiricalStackDepth = new Uint32Array(new SharedArrayBuffer(4));
  const worker = new Worker(`
  const { workerData: { empiricalStackDepth } } = require('worker_threads');
  function f() {
    empiricalStackDepth[0]++;
    f();
  }
  f();`, {
    eval: true,
    workerData: { empiricalStackDepth }
  });

  const [ error ] = await once(worker, 'error');

  common.expectsError({
    constructor: RangeError,
    message: 'Maximum call stack size exceeded'
  })(error);

  return empiricalStackDepth[0];
}

(async function() {
  v8.setFlagsFromString('--stack-size=500');
  const w1stack = await runWorker();
  v8.setFlagsFromString('--stack-size=1000');
  const w2stack = await runWorker();
  // Make sure the two stack sizes are within 10 % of each other, i.e. not
  // affected by the different `--stack-size` settings.
  assert(Math.max(w1stack, w2stack) / Math.min(w1stack, w2stack) < 1.1,
         `w1stack = ${w1stack}, w2stack ${w2stack} are too far apart`);
})().then(common.mustCall());
