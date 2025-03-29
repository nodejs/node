// Flags: --test-timeout=20
'use strict';
const { describe, test } = require('node:test');
const { setTimeout } = require('node:timers/promises');

describe('--test-timeout is set to 20ms', () => {
  test('test 1 must fail due to testTimeoutFailure with error test timed out after 20ms', async () => {
    await setTimeout(1000);
  })
  test('test 2  must fail due to testTimeoutFailure with error test timed out after 600ms', { timeout: 600 }, async () => {
    await setTimeout(1000);
  })
  test('test 3 must pass', async () => {
    await setTimeout(10);
  })
});
