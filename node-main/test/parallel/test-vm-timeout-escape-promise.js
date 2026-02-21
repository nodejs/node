'use strict';

// https://github.com/nodejs/node/issues/3020
// Promises used to allow code to escape the timeout
// set for runInContext, runInNewContext, and runInThisContext.

require('../common');
const assert = require('assert');
const vm = require('vm');

const NS_PER_MS = 1000000n;

const hrtime = process.hrtime.bigint;

function loop() {
  const start = hrtime();
  while (1) {
    const current = hrtime();
    const span = (current - start) / NS_PER_MS;
    if (span >= 2000n) {
      throw new Error(
        `escaped timeout at ${span} milliseconds!`);
    }
  }
}

assert.throws(() => {
  vm.runInNewContext(
    'Promise.resolve().then(() => loop()); loop();',
    {
      hrtime,
      loop
    },
    { timeout: 5, microtaskMode: 'afterEvaluate' }
  );
}, {
  code: 'ERR_SCRIPT_EXECUTION_TIMEOUT',
  message: 'Script execution timed out after 5ms'
});
