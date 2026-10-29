'use strict';
const { describe, it, after } = require('node:test');
const { setTimeout } = require('node:timers');

describe('--test-timeout is set to 100ms', () => {
  const timeoutRefs = [];

  it('should timeout after 100ms', async () => {
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


describe('should inherit timeout options to children', { timeout: 1 }, () => {
  const timeoutRefs = [];

  after(() => {
    for (const timeoutRef of timeoutRefs) {
      clearTimeout(timeoutRef);
    }
  });

  it('should timeout after 1ms', async () => {
    const { promise, resolve } = Promise.withResolvers();
    timeoutRefs.push(setTimeout(() => {
      resolve();
    }, 20000));
    await promise;
  });
});
