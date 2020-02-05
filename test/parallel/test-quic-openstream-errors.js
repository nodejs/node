// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test errors thrown when openStream is called incorrectly
// or is not permitted

const assert = require('assert');
const quic = require('quic');

const Countdown = require('../common/countdown');
const { key, cert, ca } = require('../common/quic');

const options = { key, cert, ca, alpn: 'zzz', maxStreamsUni: 0 };
const server = quic.createSocket({ server: options });
const client = quic.createSocket({ client: options });

const countdown = new Countdown(1, () => {
  server.close();
  client.close();
});

server.listen();
server.on('session', common.mustCall((session) => {
  session.on('stream', common.mustNotCall());
}));

server.on('ready', common.mustCall(() => {
  const req = client.connect({
    address: 'localhost',
    port: server.endpoints[0].address.port
  });

  ['z', 1, {}, [], null, Infinity, 1n, new Date()].forEach((i) => {
    assert.throws(
      () => req.openStream({ halfOpen: i }),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  // Unidirectional streams are not allowed. openStream will succeeed
  // but the stream will be destroyed immediately.
  req.openStream({ halfOpen: true })
    .on('error', common.expectsError({
      code: 'ERR_QUICSTREAM_OPEN_FAILED'
    }))
    .on('error', common.mustCall(() => countdown.dec()));

}));

server.on('close', common.mustCall());
