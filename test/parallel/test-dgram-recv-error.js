// Flags: --expose-internals
'use strict';
const common = require('../common');
const assert = require('assert');
const dgram = require('dgram');
const { kStateSymbol } = require('internal/dgram');
const s = dgram.createSocket('udp4');
const { handle } = s[kStateSymbol];

s.on('error', common.mustCall((err) => {
  s.close();

  // Don't check the full error message, as the errno is not important here.
  assert.match(String(err), /^Error: recvmsg/);
  assert.strictEqual(err.syscall, 'recvmsg');
}));

s.on('message', common.mustNotCall('no message should be received.'));
s.bind(common.mustCall(() => handle.onmessage(-1, handle, null, null)));
