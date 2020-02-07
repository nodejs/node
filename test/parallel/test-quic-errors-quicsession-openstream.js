// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test errors thrown when openStream is called incorrectly
// or is not permitted

const { createHook } = require('async_hooks');
const assert = require('assert');
const quic = require('quic');

// Ensure that no QUICSTREAM instances are created during the test
createHook({
  init(id, type) {
    assert.notStrictEqual(type, 'QUICSTREAM');
  }
}).enable();

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
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  ['z', 1, {}, [], null, Infinity, 1n, new Date()].forEach((i) => {
    assert.throws(
      () => req.openStream({ halfOpen: i }),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  // Unidirectional streams are not allowed. openStream will succeeed
  // but the stream will be destroyed immediately. The underlying
  // QuicStream C++ handle will not be created.
  req.openStream({ halfOpen: true })
    .on('error', common.expectsError({
      code: 'ERR_QUICSTREAM_OPEN_FAILED'
    }))
    .on('error', common.mustCall(() => countdown.dec()));

}));

server.on('close', common.mustCall());
