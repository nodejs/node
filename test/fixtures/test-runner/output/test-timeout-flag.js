'use strict';
const { describe, it, after } = require('node:test');
const { setTimeout } = require('node:timers');

const timeoutRefs = [];

describe('--test-timeout is set to 20ms', () => {
  it('should timeout after 20ms', async () => {
    const { promise, resolve } = Promise.withResolvers();
    timeoutRefs.push(setTimeout(() => {
      resolve();
    }, 20000));
    await promise;
  });

  it('should timeout after 5ms', { timeout: 5 }, async () => {
    const { promise, resolve } = Promise.withResolvers();
    timeoutRefs.push(setTimeout(() => {
      resolve();
    }, 20000));
    await promise;
  });

  it('should not timeout', { timeout: 50000 }, async () => {
    const { promise, resolve } = Promise.withResolvers();
    timeoutRefs.push(setTimeout(() => {
      resolve();
    }, 1));
    await promise;
  });

  it('should pass', async () => {});

  after(() => {
    for (const timeoutRef of timeoutRefs) {
      clearTimeout(timeoutRef);
    }
  });
});
