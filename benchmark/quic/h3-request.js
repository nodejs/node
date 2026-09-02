'use strict';

// Measures a complete HTTP/3 exchange: establish a session, send one request
// and read the whole response. Run in two modes, so the cost of a resumed
// 0-RTT session can be compared against a full handshake.
//
// The 0-RTT mode needs a session ticket, which can only come from an earlier
// connection. That first connection is made during warmup, outside the
// measured region, so what is timed is only the resumed exchange.

const common = require('../common.js');
const fixtures = require('../../test/common/fixtures');
const { createPrivateKey } = require('crypto');

const bench = common.createBenchmark(main, {
  // '0rtt' resumes from a ticket and sends the request in the very first
  // flight; '1rtt' is a fresh session each time. 0-RTT is listed first so
  // that it is the mode the benchmark CI test exercises.
  mode: ['0rtt', '1rtt'],
  n: [500],
}, { flags: ['--experimental-quic', '--experimental-stream-iter',
             '--no-warnings'] });

async function main({ mode, n }) {
  const { listen, connect } = require('node:quic');
  const { bytes } = require('stream/iter');

  const key = createPrivateKey(fixtures.readKey('agent1-key.pem'));
  const cert = fixtures.readKey('agent1-cert.pem');
  const body = new TextEncoder().encode('x'.repeat(256));
  const decoder = new TextDecoder();

  const request = {
    ':method': 'GET',
    ':path': '/',
    ':scheme': 'https',
    ':authority': 'localhost',
  };

  const endpoint = await listen((session) => {
    session.opened.catch(() => {});
    session.closed.catch(() => {});
    session.onstream = (stream) => { stream.closed.catch(() => {}); };
  }, {
    sni: { '*': { keys: [key], certs: [cert] } },
    onheaders() {
      this.sendHeaders({ ':status': '200' });
      this.writer.writeSync(body);
      this.writer.endSync();
    },
    endpoint: {
      maxConnectionsPerHost: 0xFFFF,
      maxConnectionsTotal: 0xFFFF,
      sessionCreationRate: 1_000_000,
      sessionCreationBurst: 1_000_000,
    },
  });

  const address = endpoint.address;
  let received = 0;
  const onheaders = () => { received++; };

  // A full handshake, one request, one response. When resume is supplied the
  // request goes out in the first flight, before the handshake completes.
  async function exchange(resume) {
    const session = await connect(address, {
      servername: 'localhost',
      verifyPeer: 'manual',
      alpn: 'h3',
      ...resume,
    });
    const stream = await session.createBidirectionalStream({
      headers: request,
      onheaders,
    });
    if (resume === undefined) await session.opened;
    const response = decoder.decode(await bytes(stream));
    if (response.length !== body.length) {
      throw new Error(`short response: ${response.length}`);
    }
    session.close();
    await session.closed.catch(() => {});
    return session;
  }

  // Collect a ticket for the 0-RTT mode from a connection that is not timed.
  let resume;
  if (mode === '0rtt') {
    const { promise, resolve } = Promise.withResolvers();
    let ticket;
    let token;
    const session = await connect(address, {
      servername: 'localhost',
      verifyPeer: 'manual',
      alpn: 'h3',
      onsessionticket(value) {
        ticket ??= value;
        if (token !== undefined) resolve();
      },
      onnewtoken(value) {
        token ??= value;
        if (ticket !== undefined) resolve();
      },
    });
    await session.opened;
    await promise;
    session.close();
    await session.closed.catch(() => {});
    resume = { sessionTicket: ticket, token };
  }

  // The timed 0-RTT exchanges deliberately never await session.opened, since
  // waiting for the handshake is exactly what 0-RTT avoids. That leaves no
  // opportunity to notice early data being refused, so check separately -
  // otherwise a ticket the server stopped accepting would quietly turn this
  // into a measurement of the 1-RTT path.
  async function checkEarlyDataAccepted() {
    const session = await connect(address, {
      servername: 'localhost',
      verifyPeer: 'manual',
      alpn: 'h3',
      ...resume,
    });
    const stream = await session.createBidirectionalStream({
      headers: request,
      onheaders,
    });
    const info = await session.opened;
    await bytes(stream);
    session.close();
    await session.closed.catch(() => {});
    if (!info.earlyDataAccepted) {
      throw new Error('0-RTT was not accepted, benchmark would be invalid');
    }
  }

  for (let i = 0; i < 20; i++) await exchange(resume);
  if (mode === '0rtt') await checkEarlyDataAccepted();

  received = 0;
  bench.start();
  for (let i = 0; i < n; i++) await exchange(resume);
  bench.end(n);

  if (received !== n) throw new Error(`missing responses: ${received}/${n}`);
  // The ticket is reused for every iteration, so confirm it was still being
  // accepted at the end of the run and not just at the start.
  if (mode === '0rtt') await checkEarlyDataAccepted();
  await endpoint.close();
}
