'use strict';

require('../common');
const assert = require('assert');
const { once } = require('node:events');
const { describe, it } = require('node:test');

describe('AbortSignal.any() with timeout signals', () => {
  it('should abort when the first timeout signal fires', async () => {
    const signal = AbortSignal.any([AbortSignal.timeout(9000), AbortSignal.timeout(110000)]);

    const abortPromise = Promise.race([
      once(signal, 'abort').then(() => {
        throw signal.reason;
      }),
      new Promise((resolve) => setTimeout(resolve, 10000)),
    ]);

    // The promise should be aborted by the 9000ms timeout
    await assert.rejects(
      () => abortPromise,
      {
        name: 'TimeoutError',
        message: 'The operation was aborted due to timeout'
      }
    );
  });
});
