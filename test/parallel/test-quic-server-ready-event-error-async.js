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

server.on('session', common.mustNotCall());

server.listen();

server.on('error', common.mustCall((error) => {
  assert.strictEqual(error.message, 'boom');
}));

server.on('ready', common.mustCall(async () => {
  throw new Error('boom');
}));
