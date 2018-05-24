// Flags: --expose-internals

'use strict';

const common = require('../common');

const { strictEqual } = require('assert');
const { setUnrefTimeout } = require('internal/timers');

// Schedule the unrefed cases first so that the later case keeps the event loop
// active.

// Every case in this test relies on implicit sorting within either Node's or
// libuv's timers storage data structures.

// unref()'d timer
{
  let called = false;
  const timer = setTimeout(common.mustCall(() => {
    called = true;
  }), 1);
  timer.unref();

  // This relies on implicit timers handle sorting withing libuv.

  setTimeout(common.mustCall(() => {
    strictEqual(called, false, 'unref()\'d timer returned before check');
  }), 1);

  strictEqual(timer.refresh(), timer);
}

// unref pooled timer
{
  let called = false;
  const timer = setUnrefTimeout(common.mustCall(() => {
    called = true;
  }), 1);

  setUnrefTimeout(common.mustCall(() => {
    strictEqual(called, false, 'unref pooled timer returned before check');
  }), 1);

  strictEqual(timer.refresh(), timer);
}

// passing 3 arguments to setUnrefTimeout
{
  let called = false;
  let result1 = '';
  const inputArgs1 = 'a';
  const timer = setUnrefTimeout(common.mustCall((args1) => {
    called = true;
    result1 = args1;
  }), 1, inputArgs1);

  setTimeout(common.mustCall(() => {
    strictEqual(called, true,
                'unref pooled timer should be returned before check');
    strictEqual(result1, inputArgs1,
                `result1 should be ${inputArgs1}. actual ${result1}`);
  }), 100, inputArgs1);

  strictEqual(timer.refresh(), timer);
}

// passing 4 arguments to setUnrefTimeout
{
  let called = false;
  let result1 = '';
  let result2 = '';
  const inputArgs1 = 'a';
  const inputArgs2 = 'b';
  const timer = setUnrefTimeout(common.mustCall((args1, args2) => {
    called = true;
    result1 = args1;
    result2 = args2;
  }), 1, inputArgs1, inputArgs2);

  setTimeout(common.mustCall(() => {
    strictEqual(called, true,
                'unref pooled timer should be returned before check');
    strictEqual(result1, inputArgs1, `result1 should be ${inputArgs1}`);
    strictEqual(result2, inputArgs2, `result2 should be ${inputArgs2}`);
  }), 100);
  strictEqual(timer.refresh(), timer);
}

// passing 5 arguments to setUnrefTimeout
{
  let called = false;
  let result1 = '';
  let result2 = '';
  let result3 = '';
  const inputArgs1 = 'a';
  const inputArgs2 = 'b';
  const inputArgs3 = 'c';
  const timer = setUnrefTimeout(common.mustCall((args1, args2, args3) => {
    called = true;
    result1 = args1;
    result2 = args2;
    result3 = args3;
  }), 1, inputArgs1, inputArgs2, inputArgs3);

  setTimeout(common.mustCall(() => {
    strictEqual(called, true,
                'unref pooled timer should be returned before check');
    strictEqual(result1, inputArgs1, `result1 should be ${inputArgs1}`);
    strictEqual(result2, inputArgs2, `result2 should be ${inputArgs2}`);
    strictEqual(result3, inputArgs3, `result3 should be ${inputArgs3}`);
  }), 100);
  strictEqual(timer.refresh(), timer);
}

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

// regular timer
{
  let called = false;
  const timer = setTimeout(common.mustCall(() => {
    called = true;
  }), 1);

  setTimeout(common.mustCall(() => {
    strictEqual(called, false, 'pooled timer returned before check');
  }), 1);

  strictEqual(timer.refresh(), timer);
}

// interval
{
  let called = 0;
  const timer = setInterval(common.mustCall(() => {
    called += 1;
    if (called === 2) {
      clearInterval(timer);
    }
  }, 2), 1);

  setTimeout(common.mustCall(() => {
    strictEqual(called, 0, 'pooled timer returned before check');
  }), 1);

  strictEqual(timer.refresh(), timer);
}
