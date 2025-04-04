// Flags: --test-timeout=20
'use strict';
const { describe, test, afterEach } = require('node:test');
const { setTimeout } = require('node:timers/promises');

let timeout;

describe('--test-timeout is set to 20ms', () => {
  test('should timeout after 20ms', async () => {
    timeout = await setTimeout(2000);
  });
  test('should timeout after 5ms', { timeout: 5 }, async () => {
    timeout = await setTimeout(2000);
  });
  test('should not timeout', { timeout: 5000 }, async () => {
    await setTimeout(200);
  });
  test('should pass', async () => {});

  afterEach(() => {
    if (timeout) {
      timeout.unref();
    }
  });
});
