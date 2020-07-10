// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test errors thrown when openStream is called incorrectly
// or is not permitted

const { createHook } = require('async_hooks');
const assert = require('assert');
const { createQuicSocket } = require('net');

// Ensure that no QUICSTREAM instances are created during the test
createHook({
  init(id, type) {
    assert.notStrictEqual(type, 'QUICSTREAM');
  }
}).enable();

const Countdown = require('../common/countdown');
const { key, cert, ca } = require('../common/quic');

const options = { key, cert, ca, alpn: 'zzz', maxStreamsUni: 0 };
const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

const countdown = new Countdown(1, () => {
  server.close();
  client.close();
});

server.on('close', common.mustCall());
client.on('close', common.mustCall());

(async function() {
  server.on('session', common.mustCall((session) => {
    session.on('stream', common.mustNotCall());
  }));
  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port
  });

  ['z', 1, {}, [], null, Infinity, 1n].forEach((i) => {
    assert.throws(
      () => req.openStream({ halfOpen: i }),
      { code: 'ERR_INVALID_ARG_TYPE' }
    );
  });

  ['', 1n, {}, [], false, 'zebra'].forEach((defaultEncoding) => {
    assert.throws(() => req.openStream({ defaultEncoding }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  });

  [-1, Number.MAX_SAFE_INTEGER + 1].forEach((highWaterMark) => {
    assert.throws(() => req.openStream({ highWaterMark }), {
      code: 'ERR_OUT_OF_RANGE'
    });
  });

  ['a', 1n, [], {}, false].forEach((highWaterMark) => {
    assert.throws(() => req.openStream({ highWaterMark }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  });

  // Unidirectional streams are not allowed. openStream will succeeed
  // but the stream will be destroyed immediately. The underlying
  // QuicStream C++ handle will not be created.
  req.openStream({
    halfOpen: true,
    highWaterMark: 10,
    defaultEncoding: 'utf16le'
  }).on('error', common.expectsError({
    code: 'ERR_OPERATION_FAILED'
  })).on('error', common.mustCall(() => countdown.dec()));

})().then(common.mustCall());
