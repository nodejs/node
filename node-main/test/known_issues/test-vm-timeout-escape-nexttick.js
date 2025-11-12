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

const waitDuration = common.platformTimeout(200n);

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

// The bug won't happen 100% reliably so run the test a small number of times to
// make sure we catch it if the bug exists.
for (let i = 0; i < 4; i++) {
  assert.throws(() => {
    vm.runInNewContext(
      'nextTick(loop); loop();',
      {
        hrtime,
        nextTick,
        loop,
      },
      { timeout: common.platformTimeout(100) },
    );
  }, {
    code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
  });
}
