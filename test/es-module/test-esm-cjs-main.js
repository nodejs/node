'use strict';

require('../common');
const { spawn } = require('child_process');
const assert = require('assert');
const path = require('path');

const entry = path.resolve(__dirname, '../fixtures/es-modules/cjs.js');

const child = spawn(process.execPath, ['--experimental-modules', entry]);
let experimentalWarning = false;
let validatedExecution = false;
child.stderr.on('data', (data) => {
  if (!experimentalWarning) {
    experimentalWarning = true;
    return;
  }
  throw new Error(data.toString());
});
child.stdout.on('data', (data) => {
  assert.strictEqual(data.toString(), 'executed\n');
  validatedExecution = true;
});
child.on('close', (code, stdout) => {
  assert.strictEqual(validatedExecution, true);
  assert.strictEqual(code, 0);
});
