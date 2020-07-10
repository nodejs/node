// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');

const socket = createQuicSocket();
socket.on('close', common.mustCall());

(async function() {
  await socket.close();
  assert.rejects(() => socket.close(), {
    code: 'ERR_INVALID_STATE'
  });
})().then(common.mustCall());
