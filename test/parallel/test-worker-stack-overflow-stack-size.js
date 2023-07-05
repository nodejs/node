'use strict';
const common = require('../common');
const assert = require('assert');
const { once } = require('events');
const v8 = require('v8');
const { Worker } = require('worker_threads');

// Verify that Workers don't care about --stack-size, as they have their own
// fixed and known stack sizes.

async function runWorker(options = {}) {
  const empiricalStackDepth = new Uint32Array(new SharedArrayBuffer(4));
  const worker = new Worker(`
  const { workerData: { empiricalStackDepth } } = require('worker_threads');
  function f() {
    empiricalStackDepth[0]++;
    f();
  }
  f();`, {
    eval: true,
    workerData: { empiricalStackDepth },
    ...options
  });

  const [ error ] = await once(worker, 'error');
  if (!options.skipErrorCheck) {
    common.expectsError({
      constructor: RangeError,
      message: 'Maximum call stack size exceeded'
    })(error);
  }

  return empiricalStackDepth[0];
}

(async function() {
  {
    v8.setFlagsFromString('--stack-size=500');
    const w1stack = await runWorker();
    v8.setFlagsFromString('--stack-size=1000');
    const w2stack = await runWorker();
    // Make sure the two stack sizes are within 10 % of each other, i.e. not
    // affected by the different `--stack-size` settings.
    assert(Math.max(w1stack, w2stack) / Math.min(w1stack, w2stack) < 1.1,
           `w1stack = ${w1stack}, w2stack = ${w2stack} are too far apart`);
  }

  {
    const w1stack = await runWorker({ resourceLimits: { stackSizeMb: 0.5 } });
    const w2stack = await runWorker({ resourceLimits: { stackSizeMb: 1.0 } });
    // Make sure the two stack sizes are at least 40 % apart from each other,
    // i.e. affected by the different `stackSizeMb` settings.
    assert(w2stack > w1stack * 1.4,
           `w1stack = ${w1stack}, w2stack = ${w2stack} are too close`);
  }

  // Test that various low stack sizes result in an 'error' event.
  // Currently the stack size needs to be at least 0.3MB for the worker to be
  // bootstrapped properly.
  for (const stackSizeMb of [ 0.3, 0.5, 1 ]) {
    await runWorker({ resourceLimits: { stackSizeMb }, skipErrorCheck: true });
  }
})().then(common.mustCall());
