// Flags: --experimental-extensionless-modules
import * as common from '../common/index.mjs';
import * as fixtures from '../common/fixtures.mjs';
import { spawn } from 'node:child_process';
import assert from 'node:assert';

const entry = fixtures.path('/es-modules/package-type-module/noext-esm');

// Run a module that does not have extension.
// This is to ensure that "type": "module" applies to extensionless files.

const child = spawn(process.execPath, [
  '--experimental-extensionless-modules',
  entry,
]);

let stdout = '';
child.stdout.setEncoding('utf8');
child.stdout.on('data', (data) => {
  stdout += data;
});
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 0);
  assert.strictEqual(signal, null);
  assert.strictEqual(stdout, 'executed\n');
}));
