// Flags: --expose-gc
'use strict';

const common = require('../common');
const assert = require('assert');
const { once } = require('node:events');
const { describe, it } = require('node:test');

describe('AbortSignal.any() with timeout signals', () => {
  it('should abort when the first timeout signal fires', async () => {
    const signal = AbortSignal.any([
      AbortSignal.timeout(common.platformTimeout(1000)),
      AbortSignal.timeout(110000),
    ]);
    let timeout;

    const abortPromise = Promise.race([
      once(signal, 'abort').then(() => {
        throw signal.reason;
      }),
      new Promise((resolve) => {
        timeout = setTimeout(resolve, common.platformTimeout(10000));
      }),
    ]);

    // Collect after this turn so the WeakRefs no longer keep the timeout
    // signals alive by themselves.
    setImmediate(common.mustCall(() => globalThis.gc()));

    try {
      await assert.rejects(
        () => abortPromise,
        {
          name: 'TimeoutError',
          message: 'The operation was aborted due to timeout'
        }
      );
    } finally {
      clearTimeout(timeout);
    }
  });
});
