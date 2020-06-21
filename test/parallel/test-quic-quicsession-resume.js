'use strict';

// Tests a simple QUIC client/server round-trip

const common = require('../common');
if (!common.hasQuic)
  common.skip('missing quic');

const { Buffer } = require('buffer');
const Countdown = require('../common/countdown');
const assert = require('assert');
const {
  key,
  cert,
  ca,
  debug,
} = require('../common/quic');

const { createQuicSocket } = require('net');

const options = { key, cert, ca, alpn: 'zzz' };

const server = createQuicSocket({ server: options });
const client = createQuicSocket({ client: options });

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

server.listen();
server.on('session', common.mustCall((session) => {
  session.on('secure', common.mustCall(() => {
    assert(session.usingEarlyData);
  }));

  session.on('stream', common.mustCall((stream) => {
    stream.resume();
  }));
}, 2));

server.on('ready', common.mustCall(() => {
  const req = client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });

  const stream = req.openStream({ halfOpen: true });
  stream.end('hello');
  stream.resume();
  stream.on('close', () => countdown.dec());

  req.on('sessionTicket', common.mustCall((ticket, params) => {
    assert(ticket instanceof Buffer);
    assert(params instanceof Buffer);
    debug('  Ticket: %s', ticket.toString('hex'));
    debug('  Params: %s', params.toString('hex'));

    // Destroy this initial client session...
    req.destroy();

    // Wait a tick then start a new one.
    setImmediate(newSession, ticket, params);
  }, 1));

  function newSession(sessionTicket, remoteTransportParams) {
    const req = client.connect({
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
      sessionTicket,
      remoteTransportParams,
      autoStart: false,
    });

    assert(req.allowEarlyData);

    const stream = req.openStream({ halfOpen: true });
    stream.end('hello');
    stream.on('error', common.mustNotCall());
    stream.on('close', common.mustCall(() => countdown.dec()));

    req.startHandshake();

    // TODO(@jasnell): There's a slight bug in here in that
    // calling end() will uncork the stream, causing data to
    // be flushed to the C++ layer, which will trigger a
    // SendPendingData that will start the handshake. That
    // has the effect of short circuiting the intent of
    // manual startHandshake(), which makes it not use 0RTT
    // for the stream data.

    req.on('secure', common.mustCall(() => {
      // TODO(@jasnell): This will be false for now because no
      // early data was sent. Once we actually start making
      // use of early data on the client side, this should be
      // true when the early data was accepted.
      assert(!req.usingEarlyData);
    }));
  }
}));
