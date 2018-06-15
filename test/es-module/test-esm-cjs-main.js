'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');

const entry = fixtures.path('/es-modules/cjs.js');

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
child.on('close', common.mustCall((code, stdout) => {
  assert.strictEqual(validatedExecution, true);
  assert.strictEqual(code, 0);
}));
