'use strict';

const assert = require('assert');
const path = require('path');
const entry = path.resolve(__dirname, '../fixtures/es-modules/symlink.js');
const { spawn } = require('child_process');

spawn(process.execPath, ['--experimental-modules', '--preserve-symlinks', entry],
      { stdio: 'inherit' }).on('exit', (code) => {
  assert.strictEqual(code, 0);
});
