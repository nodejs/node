// Flags: --experimental-json-modules
import '../common/index.mjs';
import { path } from '../common/fixtures.mjs';
import { strictEqual, ok } from 'assert';
import { spawn } from 'child_process';

import secret from '../fixtures/experimental.json';

strictEqual(secret.ofLife, 42);

// Test warning message
const child = spawn(process.execPath, [
  '--experimental-json-modules',
  path('/es-modules/json-modules.mjs')
]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', (code, signal) => {
  strictEqual(code, 0);
  strictEqual(signal, null);
  ok(stderr.toString().includes(
    'ExperimentalWarning: Importing JSON modules is an experimental feature. ' +
    'This feature could change at any time'
  ));
});
