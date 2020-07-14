// Flags: --no-warnings
'use strict';

// Tests that both client and server can open
// bidirectional and unidirectional streams,
// and that the properties for each are set
// accordingly.
//
// +------+----------------------------------+
// |  ID  | Stream Type                      |
// +------+----------------------------------+
// |   0  | Client-Initiated, Bidirectional  |
// |      |                                  |
// |   1  | Server-Initiated, Bidirectional  |
// |      |                                  |
// |   2  | Client-Initiated, Unidirectional |
// |      |                                  |
// |   3  | Server-Initiated, Unidirectional |
// +------+----------------------------------+

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const Countdown = require('../common/countdown');
const assert = require('assert');
const { once } = require('events');
const { key, cert, ca } = require('../common/quic');

const { createQuicSocket } = require('net');
const options = { key, cert, ca, alpn: 'zzz' };

const client = createQuicSocket({ client: options });
const server = createQuicSocket({ server: options });

const countdown = new Countdown(4, () => {
  server.close();
  client.close();
});

const closeHandler = common.mustCall(() => countdown.dec(), 4);

(async function() {
  server.on('session', common.mustCall(async (session) => {
    ([3, 1n, [], {}, null, 'meow']).forEach((halfOpen) => {
      assert.rejects(
        session.openStream({ halfOpen }), {
          code: 'ERR_INVALID_ARG_TYPE'
        });
    });

    const uni = await session.openStream({ halfOpen: true });
    uni.end('test');

    const bidi = await session.openStream();
    bidi.end('test');
    bidi.resume();
    bidi.on('end', common.mustCall());

    assert.strictEqual(uni.id, 3);
    assert(uni.unidirectional);
    assert(uni.serverInitiated);
    assert(!uni.bidirectional);
    assert(!uni.clientInitiated);

    assert.strictEqual(bidi.id, 1);
    assert(bidi.bidirectional);
    assert(bidi.serverInitiated);
    assert(!bidi.unidirectional);
    assert(!bidi.clientInitiated);

    session.on('stream', common.mustCall((stream) => {
      assert(stream.clientInitiated);
      assert(!stream.serverInitiated);
      switch (stream.id) {
        case 0:
          assert(stream.bidirectional);
          assert(!stream.unidirectional);
          stream.end('test');
          break;
        case 2:
          assert(stream.unidirectional);
          assert(!stream.bidirectional);
          break;
      }
      stream.resume();
      stream.on('end', common.mustCall());
    }, 2));
  }));

  await server.listen();

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  const bidi = await req.openStream();
  bidi.end('test');
  bidi.resume();
  bidi.on('close', closeHandler);
  assert.strictEqual(bidi.id, 0);

  assert(bidi.clientInitiated);
  assert(bidi.bidirectional);
  assert(!bidi.serverInitiated);
  assert(!bidi.unidirectional);

  const uni = await req.openStream({ halfOpen: true });
  uni.end('test');
  uni.on('close', closeHandler);
  assert.strictEqual(uni.id, 2);

  assert(uni.clientInitiated);
  assert(!uni.bidirectional);
  assert(!uni.serverInitiated);
  assert(uni.unidirectional);

  req.on('stream', common.mustCall((stream) => {
    assert(stream.serverInitiated);
    assert(!stream.clientInitiated);
    switch (stream.id) {
      case 1:
        assert(!stream.unidirectional);
        assert(stream.bidirectional);
        stream.end();
        break;
      case 3:
        assert(stream.unidirectional);
        assert(!stream.bidirectional);
    }
    stream.resume();
    stream.on('end', common.mustCall());
    stream.on('close', closeHandler);
  }, 2));

  await Promise.all([
    once(server, 'close'),
    once(client, 'close')
  ]);
})().then(common.mustCall());
