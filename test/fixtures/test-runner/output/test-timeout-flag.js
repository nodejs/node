// Flags: --test-timeout=20
'use strict';
const { describe, test } = require('node:test');
const { setTimeout } = require('node:timers/promises');

describe('--test-timeout is set to 20ms', () => {
  test('should timeout after 20ms', async () => {
    await setTimeout(200000, undefined, { ref: false });
  });
  test('should timeout after 5ms', { timeout: 5 }, async () => {
    await setTimeout(200000, undefined, { ref: false });
  });

  test('should not timeout', { timeout: 50000 }, async () => {
    await setTimeout(1);
  });

  test('should pass', async () => {});
});
