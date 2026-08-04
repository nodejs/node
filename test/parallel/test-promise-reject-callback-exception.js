'use strict';
require('../common');
const tmpdir = require('../common/tmpdir');
const assert = require('assert');
const path = require('path');
const child_process = require('child_process');

tmpdir.refresh();

// Tests that exceptions from the PromiseRejectCallback are printed to stderr
// when they occur as a best-effort way of handling them, and that calling
// `console.log()` works after that. Earlier, the latter did not work because
// of the exception left lying around by the PromiseRejectCallback when its JS
// part exceeded the call stack limit, and when the inspector/built-in coverage
// was enabled, it resulted in a hard crash.

for (const NODE_V8_COVERAGE of ['', tmpdir.path]) {
  // NODE_V8_COVERAGE does not work without the inspector.
  // Refs: https://github.com/nodejs/node/issues/29542
  if (!process.features.inspector && NODE_V8_COVERAGE !== '') continue;

  const { status, signal, stdout, stderr } =
    child_process.spawnSync(process.execPath,
                            [path.join(__dirname, 'test-ttywrap-stack.js')],
                            { env: { ...process.env, NODE_V8_COVERAGE } });

  assert(stdout.toString('utf8')
         .startsWith('RangeError: Maximum call stack size exceeded'),
         `stdout: <${stdout}>`);
  assert(stderr.toString('utf8')
         .startsWith('Exception in PromiseRejectCallback'),
         `stderr: <${stderr}>`);
  assert.strictEqual(status, 0);
  assert.strictEqual(signal, null);
}
