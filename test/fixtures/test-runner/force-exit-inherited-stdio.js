'use strict';

const { spawn } = require('node:child_process');
const { test } = require('node:test');

test('leaks a child with inherited stdio', () => {
  spawn(
    process.execPath,
    [
      '-e',
      `setTimeout(() => {}, ${process.env.TEST_RUNNER_STALL_MS})`,
    ],
    { stdio: 'inherit' },
  );
});
