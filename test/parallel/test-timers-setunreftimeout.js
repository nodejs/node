// Flags: --expose-internals

'use strict';

const common = require('../common');

const { strictEqual } = require('assert');
const { setUnrefTimeout } = require('internal/timers');

// ERR_INVALID_CALLBACK
{
  // It throws ERR_INVALID_CALLBACK when first argument is not a function.
  common.expectsError(
    () => setUnrefTimeout(),
    {
      code: 'ERR_INVALID_CALLBACK',
      type: TypeError,
      message: 'Callback must be a function'
    }
  );
}

{
  const timer = setUnrefTimeout(common.mustCall(() => {
    clearTimeout(testTimer);
  }), 1);

  const testTimer = setTimeout(common.mustNotCall(() => {
  }), 1);
}

// test case for passing the arguments
{
  for (let i = 0; i < 4; i++) {
    let results = [];
    let inputArgs = [];
    // set the input argument params
    for (let j = 0; j <= i ; j++) {
      inputArgs.push(j);
    }

    // For checking the arguments.length, callback function should not be arrow fucntion.
    const timer = setUnrefTimeout(common.mustCall(function(args1, args2, args3, args4) {
      // check the number of arguments passed to this callback.
      strictEqual(arguments.length, i + 1,
                  `arguments.length should be ${i + 1}. actual ${arguments.length}`
      );
      results.push(args1);
      results.push(args2);
      results.push(args3);
      results.push(args4);
    }), 1, ...inputArgs);

    const testTimer = setTimeout(common.mustCall(() => {
      const maxArgsNum = 4;
      for (let k = 0; k < maxArgsNum; k++) {
        // Checking the arguments passed to setUnrefTimeout
        let expect = (k <= i) ? inputArgs[k] : undefined;
        strictEqual(expect, results[k],
                    `result ${k} should be ${inputArgs[k]}. actual ${expect}`);
      }
      clearTimeout(testTimer);
    }), 100);
  }
}
