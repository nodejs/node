// Flags: --no-warnings
'use strict';

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const assert = require('assert');
const {
  key,
  cert,
  ca,
} = require('../common/quic');

const { createQuicSocket } = require('net');

const options = { key, cert, ca, alpn: 'zzz' };

const server = createQuicSocket({ server: options });

server.on('busy', common.mustCall(async () => {
  throw new Error('boom');
}));

server.on('close', common.mustCall());

server.on('error', common.mustCall((err) => {
  assert.strictEqual(err.message, 'boom');
}));

assert.strictEqual(server.serverBusy, false);
server.serverBusy = true;
