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
