'use strict';

require('../common');
const assert = require('assert');
const { describe, it } = require('node:test');

describe('AbortSignal.any() with timeout signals', () => {
  it('should abort when the first timeout signal fires', async () => {
    const signal = AbortSignal.any([AbortSignal.timeout(9000), AbortSignal.timeout(110000)]);

    const abortPromise = new Promise((resolve, reject) => {
      signal.addEventListener('abort', () => {
        reject(signal.reason);
      });

      setTimeout(resolve, 15000);
    });

    // The promise should be aborted by the 9000ms timeout
    const start = Date.now();
    await assert.rejects(
      () => abortPromise,
      {
        name: 'TimeoutError',
        message: 'The operation was aborted due to timeout'
      }
    );

    const duration = Date.now() - start;

    // Verify the timeout happened at approximately the right time (with some margin)
    assert.ok(duration >= 8900, `Timeout happened too early: ${duration}ms`);
    assert.ok(duration < 11000, `Timeout happened too late: ${duration}ms`);
  });
});
