// Flags: --no-warnings --expose-gc --expose-internals
'use strict';
require('../common');

const assert = require('assert');

const {
  test,
} = require('node:test');

const { setTimeout: sleep } = require('timers/promises');

// Regression test for https://github.com/nodejs/node/issues/55428
// When a signal "follows" another signal (a listener on the source signal
// aborts a dependent signal), the dependent signal must remain alive while the
// relationship exists, per the DOM spec's "AbortSignal garbage collection"
// section. This mirrors what `fetch`/`Request` does: the request's signal
// follows the user-provided signal, and it must keep firing its own listeners
// even after the `Request` object has been garbage collected.

test('a following AbortSignal must survive gc', async () => {
  const parent = new AbortController();
  let fired = false;

  {
    const dependent = new AbortController();
    // The dependent signal "follows" parent: when parent aborts, dependent
    // aborts too. This is the same pattern used by `Request`.
    parent.signal.addEventListener('abort', () => dependent.abort(), { once: true });
    dependent.signal.addEventListener('abort', () => { fired = true; });
    // `dependent` goes out of scope here; only `parent` is reachable.
  }

  await sleep(10);
  globalThis.gc();

  // Aborting the parent must still propagate to, and fire listeners on, the
  // dependent signal even though the dependent controller is no longer
  // referenced from JS.
  parent.abort();

  await sleep(10);
  assert.strictEqual(fired, true,
    'listener on the following signal should fire after gc of its controller');
});
