// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const { createQuicSocket } = require('net');

{
  const socket = createQuicSocket();
  socket.close(common.mustCall());
  socket.on('close', common.mustCall());
  assert.throws(() => socket.close(), {
    code: 'ERR_INVALID_STATE'
  });
}
