'use strict';
const common = require('../common');
if (!common.hasCrypto)
  common.skip('missing crypto');
const assert = require('assert');
const child_process = require('child_process');
const path = require('path');

process.env.NODE_DEBUG_NATIVE = 'http2';
process.env.NODE_DEBUG = 'http2';
const { stdout, stderr } = child_process.spawnSync(process.execPath, [
  path.resolve(__dirname, 'test-http2-ping.js')
], { encoding: 'utf8' });

assert(stderr.match(/Setting the NODE_DEBUG environment variable to 'http2' can expose sensitive data \(such as passwords, tokens and authentication headers\) in the resulting log\.\r?\n/),
       stderr);
assert(stderr.match(/Http2Session client \(\d+\) handling data frame for stream \d+\r?\n/),
       stderr);
assert(stderr.match(/HttpStream \d+ \(\d+\) \[Http2Session client \(\d+\)\] reading starting\r?\n/),
       stderr);
assert(stderr.match(/HttpStream \d+ \(\d+\) \[Http2Session client \(\d+\)\] closed with code 0\r?\n/),
       stderr);
assert(stderr.match(/HttpStream \d+ \(\d+\) \[Http2Session server \(\d+\)\] closed with code 0\r?\n/),
       stderr);
assert(stderr.match(/HttpStream \d+ \(\d+\) \[Http2Session server \(\d+\)\] tearing down stream\r?\n/),
       stderr);
assert.strictEqual(stdout.trim(), '');
