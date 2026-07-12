'use strict';

const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const { spawnSyncAndAssert } = require('../common/child_process');
const path = require('path');

spawnSyncAndAssert(process.execPath, [
  path.resolve(__dirname, 'test-http2-ping.js'),
], {
  env: {
    ...process.env,
    NODE_DEBUG: 'http2',
    NODE_DEBUG_NATIVE: 'http2',
  },
}, {
  trim: true,
  stderr(output) {
    assert.match(output,
                 /Setting the NODE_DEBUG environment variable to 'http2' can expose sensitive data/);
    assert.match(output, /\(such as passwords, tokens and authentication headers\) in the resulting log\.\r?\n/);
    assert.match(output, /Http2Session client \(\d+\) handling data frame for stream \d+\r?\n/);
    assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session client \(\d+\)\] reading starting\r?\n/);
    assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session client \(\d+\)\] closed with code 0\r?\n/);
    assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session server \(\d+\)\] closed with code 0\r?\n/);
    assert.match(output, /HttpStream \d+ \(\d+\) \[Http2Session server \(\d+\)\] tearing down stream\r?\n/);
  },
  stdout: ''
});
