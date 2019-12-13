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
const fixtures = require('../common/fixtures');
const key = fixtures.readKey('agent8-key.pem', 'binary');
const cert = fixtures.readKey('agent8-cert.pem', 'binary');
const { debuglog } = require('util');
const debug = debuglog('test');

const { createSocket } = require('quic');

let client;
const server = createSocket({ type: 'udp4', port: 0 });

const countdown = new Countdown(4, () => {
  debug('Countdown expired. Closing sockets');
  server.close();
  client.close();
});

const closeHandler = common.mustCall(() => countdown.dec(), 4);

server.listen({ key, cert, alpn: 'zzz' });
server.on('session', common.mustCall((session) => {
  debug('QuicServerSession created');
  session.on('secure', common.mustCall(() => {
    debug('QuicServerSession TLS Handshake Completed.');

    ([3, 1n, [], {}, null, 'meow']).forEach((halfOpen) => {
      const message = 'The "options.halfOpen" property must' +
        ` be of type boolean. Received type ${typeof halfOpen}`;

      assert.throws(() => session.openStream({ halfOpen }), {
        code: 'ERR_INVALID_ARG_TYPE',
        name: 'TypeError',
        message
      });
    });

    const uni = session.openStream({ halfOpen: true });
    uni.end('test');
    debug('Unidirectional, Server-initiated stream %d opened', uni.id);

    const bidi = session.openStream();
    bidi.end('test');
    bidi.resume();
    bidi.on('end', common.mustCall());
    debug('Bidirectional, Server-initiated stream %d opened', bidi.id);

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
  }));

  session.on('stream', common.mustCall((stream) => {
    assert(stream.clientInitiated);
    assert(!stream.serverInitiated);
    switch (stream.id) {
      case 0:
        debug('Bidirectional, Client-initiated stream %d received', stream.id);
        assert(stream.bidirectional);
        assert(!stream.unidirectional);
        stream.end('test');
        break;
      case 2:
        debug('Unidirectional, Client-initiated stream %d receieved',
              stream.id);
        assert(stream.unidirectional);
        assert(!stream.bidirectional);
        break;
    }
    stream.resume();
    stream.on('end', common.mustCall());
  }, 2));
}));

server.on('ready', common.mustCall(() => {
  debug('Server listening on port %d', server.address.port);
  client = createSocket({ type: 'udp4', port: 0 });
  const req = client.connect({
    type: 'udp4',
    address: 'localhost',
    port: server.address.port,
    rejectUnauthorized: false,
    maxStreamsUni: 10,
    alpn: 'zzz',
  });

  req.on('secure', common.mustCall(() => {
    debug('QuicClientSession TLS Handshake Completed');
    const bidi = req.openStream();
    bidi.end('test');
    bidi.resume();
    bidi.on('close', closeHandler);
    assert.strictEqual(bidi.id, 0);
    debug('Bidirectional, Client-initiated stream %d opened', bidi.id);

    assert(bidi.clientInitiated);
    assert(bidi.bidirectional);
    assert(!bidi.serverInitiated);
    assert(!bidi.unidirectional);

    const uni = req.openStream({ halfOpen: true });
    uni.end('test');
    uni.on('close', closeHandler);
    assert.strictEqual(uni.id, 2);
    debug('Unidirectional, Client-initiated stream %d opened', uni.id);

    assert(uni.clientInitiated);
    assert(!uni.bidirectional);
    assert(!uni.serverInitiated);
    assert(uni.unidirectional);
  }));

  req.on('stream', common.mustCall((stream) => {
    assert(stream.serverInitiated);
    assert(!stream.clientInitiated);
    switch (stream.id) {
      case 1:
        debug('Bidirectional, Server-initiated stream %d received', stream.id);
        assert(!stream.unidirectional);
        assert(stream.bidirectional);
        stream.end();
        break;
      case 3:
        debug('Unidirectional, Server-initiated stream %d received', stream.id);
        assert(stream.unidirectional);
        assert(!stream.bidirectional);
    }
    stream.resume();
    stream.on('end', common.mustCall());
    stream.on('close', closeHandler);
  }, 2));

}));

server.on('listening', common.mustCall());
