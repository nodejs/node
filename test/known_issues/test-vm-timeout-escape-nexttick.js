'use strict';

// https://github.com/nodejs/node/issues/3020
// Promises, nextTick, and queueMicrotask allow code to escape the timeout
// set for runInContext, runInNewContext, and runInThisContext

const common = require('../common');
const assert = require('assert');
const vm = require('vm');

const NS_PER_MS = 1000000n;

const hrtime = process.hrtime.bigint;
const nextTick = process.nextTick;

const waitDuration = common.platformTimeout(100n);

function loop() {
  const start = hrtime();
  while (1) {
    const current = hrtime();
    const span = (current - start) / NS_PER_MS;
    if (span >= waitDuration) {
      throw new Error(
        `escaped timeout at ${span} milliseconds!`);
    }
  }
}

assert.throws(() => {
  vm.runInNewContext(
    'nextTick(loop); loop();',
    {
      hrtime,
      nextTick,
      loop
    },
    { timeout: common.platformTimeout(10) }
  );
}, {
  code: 'ERR_SCRIPT_EXECUTION_TIMEOUT'
});
