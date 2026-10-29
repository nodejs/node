// Flags: --expose-internals

'use strict';

const common = require('../common');

const assert = require('assert');
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

  // This relies on implicit timers handle sorting within libuv.

  setTimeout(common.mustCall(() => {
    assert.strictEqual(called, false);
  }), 1);

  assert.strictEqual(timer.refresh(), timer);
}

// Should throw with non-functions
{
  [null, true, false, 0, 1, NaN, '', 'foo', {}, Symbol()].forEach((cb) => {
    assert.throws(
      () => setUnrefTimeout(cb),
      {
        code: 'ERR_INVALID_ARG_TYPE',
      }
    );
  });
}

// unref pooled timer
{
  let called = false;
  const timer = setUnrefTimeout(common.mustCall(() => {
    called = true;
  }), 1);

  setUnrefTimeout(common.mustCall(() => {
    assert.strictEqual(called, false);
  }), 1);

  assert.strictEqual(timer.refresh(), timer);
}

// regular timer
{
  let called = false;
  const timer = setTimeout(common.mustCall(() => {
    called = true;
  }), 1);

  setTimeout(common.mustCall(() => {
    assert.strictEqual(called, false);
  }), 1);

  assert.strictEqual(timer.refresh(), timer);
}

// regular timer
{
  let called = false;
  const timer = setTimeout(common.mustCall(() => {
    if (!called) {
      called = true;
      process.nextTick(common.mustCall(() => {
        timer.refresh();
        assert.strictEqual(timer.hasRef(), true);
      }));
    }
  }, 2), 1);
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
    assert.strictEqual(called, 0);
  }), 1);

  assert.strictEqual(timer.refresh(), timer);
}
