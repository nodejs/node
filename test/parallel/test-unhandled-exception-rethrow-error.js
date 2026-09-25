'use strict';
require('../common');

if (process.argv[2] === 'child') {
  process.on('uncaughtException', (err) => {
    err.rethrow = true;
    throw err;
  });

  function throwException() {
    throw new Error('boom');
  }

  throwException();
} else {
  const assert = require('assert');
  const { spawnSync } = require('child_process');
  const result = spawnSync(process.execPath, [__filename, 'child']);

  assert.strictEqual(result.status, 7);
  assert.strictEqual(result.signal, null);
  assert.strictEqual(result.stdout.toString().trim(), '');
  // Verify that the error was thrown and that the stack was preserved.
  const stderr = result.stderr.toString();
  assert.match(stderr, /Error: boom/);
  assert.match(stderr, /at throwException/);
  assert.match(stderr, /rethrow: true/);
}
