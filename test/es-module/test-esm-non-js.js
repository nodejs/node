'use strict';

const common = require('../common');
const { spawn } = require('child_process');
const assert = require('assert');

const entry = require.resolve('./test-esm-json.mjs');

// Verify non-js extensions fail for ESM
const child = spawn(process.execPath, [entry]);

let stderr = '';
child.stderr.setEncoding('utf8');
child.stderr.on('data', (data) => {
  stderr += data;
});
child.on('close', common.mustCall((code, signal) => {
  assert.strictEqual(code, 1);
  assert.strictEqual(signal, null);
  assert.ok(stderr.indexOf('ERR_UNKNOWN_FILE_EXTENSION') !== -1);
}));
