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

// Test that setUnrefTimeout correctly passes arguments to the callback
{
  const maxArgsNum = 4;
  for (let i = 0; i < maxArgsNum; i++) {
    const inputArgs = [];
    // Set the input argument params
    for (let j = 0; j <= i; j++) {
      inputArgs.push(j);
    }

    const timer = setUnrefTimeout(common.mustCall((...args) => {
      // Check the number of arguments passed to this callback.
      strictEqual(args.length, i + 1,
                  `arguments.length should be ${i + 1}.` +
                  `actual ${args.length}`
      );
      for (let k = 0; k < maxArgsNum; k++) {
        // Checking the arguments passed to setUnrefTimeout
        const expected = (k <= i) ? inputArgs[k] : undefined;
        strictEqual(args[k], expected,
                    `result ${k} should be ${expected}.` +
                    `actual ${args[k]}`);
      }
      clearTimeout(timer);
    }), 1, ...inputArgs);

    setTimeout(common.mustCall(), 1);
  }
}
