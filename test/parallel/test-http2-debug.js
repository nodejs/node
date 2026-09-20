'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const fs = require('fs');
const path = require('path');
const tmpdir = require('../common/tmpdir');

tmpdir.refresh();
const stderrFile = tmpdir.resolve('stderr.log');
// Native debug writes can be lost when a non-blocking stderr pipe fills.
const stderrFd = fs.openSync(stderrFile, 'w');
try {
  spawnSyncAndAssert(process.execPath, [
    path.resolve(__dirname, 'test-http2-ping.js'),
  ], {
    env: {
      ...process.env,
      NODE_DEBUG: 'http2',
      NODE_DEBUG_NATIVE: 'http2',
    },
    stdio: ['pipe', 'pipe', stderrFd],
  }, {
    stdout: ''
  });
} finally {
  fs.closeSync(stderrFd);
}

const output = fs.readFileSync(stderrFile, 'utf8');
assert.match(output,
             /Setting the NODE_DEBUG environment variable to 'http2' can expose sensitive data/);
assert.match(output, /\(such as passwords, tokens and authentication headers\) in the resulting log\.\r?\n/);
assert.match(output, /Http2Session client \(\d+\) handling data frame for stream \d+\r?\n/);
assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session client \(\d+\)\] reading starting\r?\n/);
assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session client \(\d+\)\] closed with code 0\r?\n/);
assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session server \(\d+\)\] closed with code 0\r?\n/);
assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session server \(\d+\)\] tearing down stream\r?\n/);
