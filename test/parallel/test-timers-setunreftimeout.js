// Flags: --expose-internals
'use strict';

const common = require('../common');

const { strictEqual } = require('assert');
const { setUnrefTimeout } = require('internal/timers');

{
  common.expectsError(
    () => setUnrefTimeout(),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError,
      message: 'Callback must be a function'
    }
  );
}

// Test this if correctly passes arguments to the callback
{
  const maxArgsNum = 4;
  for (let i = 0; i < maxArgsNum; i++) {
    const results = [];
    const inputArgs = [];
    // set the input argument params
    for (let j = 0; j <= i; j++) {
      inputArgs.push(j);
    }

    // For checking the arguments.length,
    // callback function should not be arrow fucntion.
    const timer = setUnrefTimeout(common.mustCall(
      function(...args) {
        // check the number of arguments passed to this callback.
        strictEqual(args.length, i + 1,
                    `arguments.length should be ${i + 1}.` +
                    `actual ${args.length}`
        );
        for (const arg of args) {
          results.push(arg);
        }
      }
    ), 1, ...inputArgs);

    const testTimer = setTimeout(common.mustCall(() => {
      for (let k = 0; k < maxArgsNum; k++) {
        // Checking the arguments passed to setUnrefTimeout
        const expected = (k <= i) ? inputArgs[k] : undefined;
        strictEqual(results[k], expected,
                    `result ${k} should be ${expected}.` +
                    `actual ${inputArgs[k]}`);
      }
      clearTimeout(testTimer);
      clearTimeout(timer);
    }), 100);
  }
}
