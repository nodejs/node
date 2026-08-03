// Flags: --no-warnings --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const timers = require('timers');
const { promisify } = require('util');

const { getEventListeners } = require('events');
const { NodeEventTarget } = require('internal/event_target');

const timerPromises = require('timers/promises');

const setPromiseImmediate = promisify(timers.setImmediate);

assert.strictEqual(setPromiseImmediate, timerPromises.setImmediate);

{
  const promise = setPromiseImmediate();
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, undefined);
  }));
}

{
  const promise = setPromiseImmediate('foobar');
  promise.then(common.mustCall((value) => {
    assert.strictEqual(value, 'foobar');
  }));
}

{
  const ac = new AbortController();
  const signal = ac.signal;
  assert.rejects(setPromiseImmediate(10, { signal }), /AbortError/)
    .then(common.mustCall());
  ac.abort();
}

{
  const signal = AbortSignal.abort(); // Abort in advance
  assert.rejects(setPromiseImmediate(10, { signal }), /AbortError/)
    .then(common.mustCall());
}

{
  // Check that aborting after resolve will not reject.
  const ac = new AbortController();
  const signal = ac.signal;
  setPromiseImmediate(10, { signal })
    .then(common.mustCall(() => { ac.abort(); }))
    .then(common.mustCall());
}

{
  // Check that timer adding signals does not leak handlers
  const signal = new NodeEventTarget();
  signal.aborted = false;
  setPromiseImmediate(0, { signal }).finally(common.mustCall(() => {
    assert.strictEqual(getEventListeners(signal, 'abort').length, 0);
  }));
}

{
  Promise.all(
    [1, '', false, Infinity].map(
      (i) => assert.rejects(setPromiseImmediate(10, i), {
        code: 'ERR_INVALID_ARG_TYPE'
      })
    )
  ).then(common.mustCall());

  Promise.all(
    [1, '', false, Infinity, null, {}].map(
      (signal) => assert.rejects(setPromiseImmediate(10, { signal }), {
        code: 'ERR_INVALID_ARG_TYPE'
      })
    )
  ).then(common.mustCall());

  Promise.all(
    [1, '', Infinity, null, {}].map(
      (ref) => assert.rejects(setPromiseImmediate(10, { ref }), {
        code: 'ERR_INVALID_ARG_TYPE'
      })
    )
  ).then(common.mustCall());
}

{
  common.spawnPromisified(process.execPath, ['-pe', "const assert = require('assert');" +
    'require(\'timers/promises\').setImmediate(null, { ref: false }).' +
    'then(assert.fail)']).then(common.mustCall(({ stderr }) => {
    assert.strictEqual(stderr, '');
  }));
}

(async () => {
  const signal = AbortSignal.abort('boom');
  await assert.rejects(timerPromises.setImmediate(undefined, { signal }), {
    cause: 'boom',
  });
})().then(common.mustCall());
