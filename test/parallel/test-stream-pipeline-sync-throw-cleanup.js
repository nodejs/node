'use strict';

const common = require('../common');
const assert = require('assert');
const { pipeline, PassThrough, Readable } = require('stream');
const { pipeline: pipelinePromise } = require('stream/promises');
const { getEventListeners } = require('events');

// pipeline() can throw synchronously while it is still wiring the streams
// together, e.g. ERR_STREAM_UNABLE_TO_PIPE when the destination has already
// been destroyed. On that path finishImpl() never runs, so the listener added
// to the caller's AbortSignal has to be disposed of here instead, otherwise it
// stays attached for the lifetime of the signal.
//
// The streams themselves are intentionally not destroyed, see the
// ERR_INVALID_RETURN_VALUE cases in test-stream-pipeline.js.

// Callback form. `pipeline()` does not accept options, so this only checks
// that the throw still propagates and no listener is left behind on the
// streams' behalf.
{
  const source = new Readable({ read() {} });
  const dest = new PassThrough();
  dest.destroy();

  assert.throws(() => {
    pipeline(source, new PassThrough(), dest, common.mustNotCall());
  }, { code: 'ERR_STREAM_UNABLE_TO_PIPE' });
}

// Promise form with a caller-owned signal: the listener must be removed.
{
  const ac = new AbortController();
  assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);

  const source = new Readable({ read() {} });
  const dest = new PassThrough();
  dest.destroy();

  assert.rejects(
    pipelinePromise(source, new PassThrough(), dest, { signal: ac.signal }),
    { code: 'ERR_STREAM_UNABLE_TO_PIPE' },
  ).then(common.mustCall(() => {
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
  }));
}

// The same signal reused across several failed calls must not accumulate
// listeners.
{
  const ac = new AbortController();
  const pending = [];

  for (let i = 0; i < 10; i++) {
    const dest = new PassThrough();
    dest.destroy();
    pending.push(assert.rejects(
      pipelinePromise(new Readable({ read() {} }), new PassThrough(), dest,
                      { signal: ac.signal }),
      { code: 'ERR_STREAM_UNABLE_TO_PIPE' },
    ));
  }

  Promise.all(pending).then(common.mustCall(() => {
    assert.strictEqual(getEventListeners(ac.signal, 'abort').length, 0);
  }));
}
