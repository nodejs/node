// Flags: --no-warnings
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

const { createWriteStream } = require('fs');
const { createQuicSocket } = require('net');

const qlog = process.env.NODE_QLOG === '1';
const options = { key, cert, ca, alpn: 'zzz', qlog };

const server = createQuicSocket({ qlog, server: options });
const client = createQuicSocket({ qlog, client: options });

const countdown = new Countdown(2, () => {
  server.close();
  client.close();
});

(async function() {
  let counter = 0;
  server.on('session', common.mustCall((session) => {
    if (qlog) session.qlog.pipe(createWriteStream(`server-${counter++}.qlog`));
    session.on('stream', common.mustCall((stream) => {
      stream.on('close', common.mustCall());
      assert(stream.unidirectional);
      assert(!stream.writable);
      stream.resume();
    }));
  }, 2));

  await server.listen();

  let storedTicket;
  let storedParams;

  const req = await client.connect({
    address: common.localhostIPv4,
    port: server.endpoints[0].address.port,
  });
  if (qlog) req.qlog.pipe(createWriteStream(`client-${counter}.qlog`));

  req.on('sessionTicket', common.mustCall((ticket, params) => {
    assert(ticket instanceof Buffer);
    assert(params instanceof Buffer);
    debug('  Ticket: %s', ticket.toString('hex'));
    debug('  Params: %s', params.toString('hex'));
    storedTicket = ticket;
    storedParams = params;
  }, 1));

  const stream = await req.openStream({ halfOpen: true });
  stream.end('hello');
  stream.on('close', () => {
    countdown.dec();
    // Wait a turn then start a new session using the stored
    // ticket and transportParameters
    setImmediate(newSession, storedTicket, storedParams);
  });

  async function newSession(sessionTicket, remoteTransportParams) {
    const req = await client.connect({
      address: common.localhostIPv4,
      port: server.endpoints[0].address.port,
      sessionTicket,
      remoteTransportParams
    });
    if (qlog) req.qlog.pipe(createWriteStream('client2.qlog'));

    assert(req.allowEarlyData);

    const stream = await req.openStream({ halfOpen: true });
    stream.end('hello');
    stream.on('error', common.mustNotCall());
    stream.on('close', common.mustCall(() => countdown.dec()));

    // TODO(@jasnell): This will be false for now because no
    // early data was sent. Once we actually start making
    // use of early data on the client side, this should be
    // true when the early data was accepted.
    assert(!req.usingEarlyData);
  }
})().then(common.mustCall());
