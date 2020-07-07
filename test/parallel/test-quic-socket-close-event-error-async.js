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

server.on('error', common.mustNotCall());

server.on('close', common.mustCall(async () => {
  throw new Error('boom');
}));

process.on('uncaughtException', (error) => {
  assert.strictEqual(error.message, 'boom');
});

server.destroy();
