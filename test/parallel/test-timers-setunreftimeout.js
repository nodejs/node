// Flags: --expose-internals

'use strict';

const common = require('../common');

const { strictEqual, notStrictEqual } = require('assert');
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
  let called = false;
  const timer = setUnrefTimeout(common.mustCall(() => {
    called = true;
  }), 1);

  const testTimer = setUnrefTimeout(common.mustCall(() => {
    strictEqual(called, false, 'unref pooled timer returned before check');
    clearTimeout(testTimer);
  }), 1);

  strictEqual(timer.refresh(), timer);
}

// test case for passing the arguments
{
  for (let i = 3; i <= 6; i++) {
    let called = false;
    let results = [];
    let inputArgs = [];
    // set the input argument params
    for (let j = 0; j <= i - 3 ; j++) {
      inputArgs.push(j);
    }

    const timer = setUnrefTimeout(common.mustCall((args1, args2, args3, args4) => {
      called = true;
      results.push(args1);
      results.push(args2);
      results.push(args3);
      results.push(args4);
    }), 1, inputArgs[0], inputArgs[1], inputArgs[2], inputArgs[3]);

    const testTimer = setTimeout(common.mustCall(() => {
      strictEqual(called, true,
                  'unref pooled timer should be returned before check');
      const maxArgsNum = 4;
      for (let k = 0; k < maxArgsNum; k++) {
        // checking the arguments passed to setUnrefTimeout
        let expect = (k + 3 <= i) ? inputArgs[k] : undefined;
        strictEqual(expect, results[k],
                    `result ${k} should be ${inputArgs[k]}. actual ${expect}`);
      }
      clearTimeout(testTimer);
    }), 100);

    strictEqual(timer.refresh(), timer);
  }
}
