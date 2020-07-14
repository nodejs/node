// Flags: --no-warnings
'use strict';
const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

// Test errors thrown when openStream is called incorrectly
// or is not permitted

const { once } = require('events');
const { createHook } = require('async_hooks');
const assert = require('assert');
const { createQuicSocket } = require('net');

// Ensure that no QUICSTREAM instances are created during the test
createHook({
  init(id, type) {
    assert.notStrictEqual(type, 'QUICSTREAM');
  }
}).enable();

const { key, cert, ca } = require('../common/quic');

const options = { key, cert, ca, alpn: 'zzz', maxStreamsUni: 0 };
const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

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

  for (const halfOpen of ['z', 1, {}, [], null, Infinity, 1n]) {
    await assert.rejects(req.openStream({ halfOpen }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }

  for (const defaultEncoding of ['', 1n, {}, [], false, 'zebra']) {
    await assert.rejects(req.openStream({ defaultEncoding }), {
      code: 'ERR_INVALID_ARG_VALUE'
    });
  }

  for (const highWaterMark of [-1, Number.MAX_SAFE_INTEGER + 1]) {
    await assert.rejects(req.openStream({ highWaterMark }), {
      code: 'ERR_OUT_OF_RANGE'
    });
  }

  for (const highWaterMark of ['a', 1n, [], {}, false]) {
    await assert.rejects(req.openStream({ highWaterMark }), {
      code: 'ERR_INVALID_ARG_TYPE'
    });
  }

  // Unidirectional streams are not allowed. openStream will succeeed
  // but the stream will be destroyed immediately. The underlying
  // QuicStream C++ handle will not be created.
  await assert.rejects(
    req.openStream({
      halfOpen: true,
      highWaterMark: 10,
      defaultEncoding: 'utf16le'
    }), {
      code: 'ERR_OPERATION_FAILED'
    });

  server.close();
  client.close();

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);

})().then(common.mustCall());
