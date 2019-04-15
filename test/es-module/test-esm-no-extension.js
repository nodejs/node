'use strict';

const common = require('../common');
const fixtures = require('../common/fixtures');
const { spawn } = require('child_process');
const assert = require('assert');

const entry = fixtures.path('/es-modules/package-type-module/noext-esm');

// Run a module that does not have extension.
// This is to ensure that "type": "module" applies to extensionless files.

const child = spawn(process.execPath, [
  '--experimental-modules',
  entry
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
let stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  stdout += data;
});
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(stdout, 'executed\n');
  assert.strictEqual(stderr, `(node:${child.pid}) ` +
      'ExperimentalWarning: The ESM module loader is experimental.\n');
}));
